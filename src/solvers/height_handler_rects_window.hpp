#pragma once

#include <solvers/height_handler.hpp>
#include <solvers/height_handler_rects.hpp>
#include <vector>
#include <algorithm>

// HeightHandlerRectsWindow — версия HeightHandlerRects с "скользящим окном"
// Хранит только прямоугольники выше минимальной высоты (min_z)
// При add_rect обновляет min_z и удаляет устаревшие прямоугольники
class HeightHandlerRectsWindow : public HeightHandler {
    std::vector<HeightRect> height_rects;
    uint32_t min_z = 0;  // Минимальная высота — всё ниже игнорируется

public:
    // Конструктор: инициализирует с начальным прямоугольником (пол)
    HeightHandlerRectsWindow(uint32_t length, uint32_t width) {
        height_rects.push_back(HeightRect{0, 0, length - 1, width - 1, 0});
    }
    
    [[nodiscard]] uint32_t get_h(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override {
        for (const auto& rect : height_rects) {
            if (rect.x <= X && x <= rect.X && rect.y <= Y && y <= rect.Y) {
                return rect.h;
            }
        }
        // Если не найдено — возвращаем min_z (всё ниже "забыто")
        return min_z;
    }
    
    [[nodiscard]] uint64_t get_area(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override {
        uint64_t area = 0;
        uint32_t max_height = min_z;
        bool found_max = false;
        
        for (const auto& rect : height_rects) {
            if (rect.x <= X && x <= rect.X && rect.y <= Y && y <= rect.Y) {
                if (!found_max) {
                    max_height = rect.h;
                    found_max = true;
                }
                
                if (rect.h == max_height) {
                    uint32_t ix = std::max(x, rect.x);
                    uint32_t iX = std::min(X, rect.X);
                    uint32_t iy = std::max(y, rect.y);
                    uint32_t iY = std::min(Y, rect.Y);
                    
                    area += static_cast<uint64_t>(iX - ix + 1) * (iY - iy + 1);
                } else {
                    break;
                }
            }
        }
        
        return area;
    }

    void add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) override {
        HeightRect new_rect{x, y, X, Y, h};
        
        if (!new_rect.is_valid()) {
            return;
        }

        // Обновляем минимальную высоту — нижняя граница нового прямоугольника
        // Это z-координата основания коробки
        uint32_t base_z = get(x, y, X, Y);
        if (base_z > min_z) {
            min_z = base_z;
            
            // Удаляем прямоугольники ниже min_z
            height_rects.erase(
                std::remove_if(height_rects.begin(), height_rects.end(),
                    [this](const HeightRect& r) { return r.h < min_z; }),
                height_rects.end());
        }

        std::vector<HeightRect> new_height_rects;

        for (const auto& existing : height_rects) {
            if (!existing.intersects(new_rect)) {
                new_height_rects.push_back(existing);
            } else if (existing.h > new_rect.h) {
                new_height_rects.push_back(existing);
            } else {
                auto parts = subtract(existing, new_rect);
                for (const auto& part : parts) {
                    new_height_rects.push_back(part);
                }
            }
        }

        std::vector<HeightRect> new_parts = {new_rect};

        for (const auto& existing : height_rects) {
            if (existing.h > new_rect.h && existing.intersects(new_rect)) {
                std::vector<HeightRect> updated_parts;
                for (const auto& part : new_parts) {
                    auto subtracted = subtract(part, existing);
                    for (const auto& s : subtracted) {
                        updated_parts.push_back(s);
                    }
                }
                new_parts = std::move(updated_parts);
            }
        }

        for (const auto& part : new_parts) {
            new_height_rects.push_back(part);
        }

        height_rects = std::move(new_height_rects);

        // Сортируем по убыванию высоты
        std::sort(height_rects.begin(), height_rects.end(),
                  [](const HeightRect& a, const HeightRect& b) {
                      return a.h > b.h;
                  });
    }

    [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> get_dots(
        const TestDataHeader& header, const BoxSize& box) const override {
        
        std::vector<std::pair<uint32_t, uint32_t>> result;
        for (const auto& rect : height_rects) {
            std::vector<std::pair<uint32_t, uint32_t>> dots = {
                {rect.x, rect.y},
                {rect.x, rect.Y + 1},
                {rect.X + 1, rect.y},
                {rect.X + 1, rect.Y + 1},

                {rect.X - box.length + 1, rect.y},
                {rect.x, rect.Y - box.width + 1},
                {rect.X - box.length + 1, rect.Y - box.width + 1},

                {rect.x - box.length, rect.y},
                {rect.x, rect.y - box.width},
                {rect.x - box.length, rect.y - box.width},

                {rect.X + 1 - box.length, rect.Y + 1},
                {rect.X + 1 - box.length, rect.y - box.width},
            };

            for (auto& [px, py] : dots) {
                if (std::max(px, px + box.length) <= header.length && 
                    std::max(py, py + box.width) <= header.width) {
                    result.emplace_back(px, py);
                }
            }
        }
        return result;
    }

    [[nodiscard]] uint32_t size() const override { return height_rects.size(); }
    
    [[nodiscard]] uint32_t get_min_z() const { return min_z; }

private:
    // Разбивает existing прямоугольник, вырезая из него область cutter
    [[nodiscard]] static std::vector<HeightRect> subtract(const HeightRect& existing,
                                                          const HeightRect& cutter) {
        std::vector<HeightRect> result;

        if (!existing.intersects(cutter)) {
            result.push_back(existing);
            return result;
        }

        if (cutter.x <= existing.x && cutter.X >= existing.X &&
            cutter.y <= existing.y && cutter.Y >= existing.Y) {
            return result;
        }

        uint32_t ix = std::max(existing.x, cutter.x);
        uint32_t iX = std::min(existing.X, cutter.X);
        uint32_t iy = std::max(existing.y, cutter.y);
        uint32_t iY = std::min(existing.Y, cutter.Y);

        // BOTTOM
        if (existing.y < iy) {
            HeightRect bottom;
            bottom.x = existing.x;
            bottom.X = existing.X;
            bottom.y = existing.y;
            bottom.Y = iy - 1;
            bottom.h = existing.h;
            if (bottom.is_valid()) {
                result.push_back(bottom);
            }
        }

        // TOP
        if (existing.Y > iY) {
            HeightRect top;
            top.x = existing.x;
            top.X = existing.X;
            top.y = iY + 1;
            top.Y = existing.Y;
            top.h = existing.h;
            if (top.is_valid()) {
                result.push_back(top);
            }
        }

        // LEFT
        if (existing.x < ix) {
            HeightRect left;
            left.x = existing.x;
            left.X = ix - 1;
            left.y = iy;
            left.Y = iY;
            left.h = existing.h;
            if (left.is_valid()) {
                result.push_back(left);
            }
        }

        // RIGHT
        if (existing.X > iX) {
            HeightRect right;
            right.x = iX + 1;
            right.X = existing.X;
            right.y = iy;
            right.Y = iY;
            right.h = existing.h;
            if (right.is_valid()) {
                result.push_back(right);
            }
        }

        return result;
    }
};
