#pragma once

#include <solvers/height_handler.hpp>
#include <utils/assert.hpp>

#include <algorithm>
#include <vector>

// Реализация HeightHandler на основе списка непересекающихся прямоугольников
// Оптимизация: прямоугольники отсортированы по убыванию высоты
class HeightHandlerRects {
    std::vector<HeightRect> height_rects;

public:
    HeightHandlerRects(uint32_t length, uint32_t width) {
        height_rects.push_back(HeightRect{0, 0, length - 1, width - 1, 0});
    }

    [[nodiscard]] uint32_t get_h(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
        // Прямоугольники отсортированы по h↓, первый пересекающийся = максимум
        for (const auto &rect: height_rects) {
            if (rect.x <= X && x <= rect.X && rect.y <= Y && y <= rect.Y) {
                return rect.h;
            }
        }
        return 0;
    }

    [[nodiscard]] uint64_t get_area(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
        uint64_t area = 0;
        uint32_t max_height = 0;
        bool found_max = false;

        for (const auto &rect: height_rects) {
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
                    break;// Прямоугольники отсортированы по h↓
                }
            }
        }
        return area;
    }

    void add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) {
        HeightRect new_rect{x, y, X, Y, h};
        if (!new_rect.is_valid()) return;

        /*std::vector<HeightRect> new_height_rects;

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

        height_rects = std::move(new_height_rects);*/

        height_rects.push_back(new_rect);
        for (size_t i = height_rects.size() - 1; i > 0; --i) {
            if (height_rects[i].h > height_rects[i - 1].h) {
                std::swap(height_rects[i], height_rects[i - 1]);
            } else {
                break;
            }
        }
    }

    void add_rect(HeightRect rect) { add_rect(rect.x, rect.y, rect.X, rect.Y, rect.h); }

    [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> get_dots(
            const TestDataHeader &header, const BoxSize &box) const {

        std::vector<std::pair<uint32_t, uint32_t>> result;
        for (const auto &rect: height_rects) {
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

            for (auto &[px, py]: dots) {
                if (std::max(px, px + box.length) <= header.length &&
                    std::max(py, py + box.width) <= header.width) {
                    result.emplace_back(px, py);
                }
            }
        }
        return result;
    }

    [[nodiscard]] uint32_t size() const { return height_rects.size(); }

    [[nodiscard]] const std::vector<HeightRect> &get_rects() const { return height_rects; }

private:
    [[nodiscard]] static std::vector<HeightRect> subtract(const HeightRect &existing,
                                                          const HeightRect &cutter) {
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

        if (existing.y < iy) {
            HeightRect bottom{existing.x, existing.y, existing.X, iy - 1, existing.h};
            if (bottom.is_valid()) result.push_back(bottom);
        }
        if (existing.Y > iY) {
            HeightRect top{existing.x, iY + 1, existing.X, existing.Y, existing.h};
            if (top.is_valid()) result.push_back(top);
        }
        if (existing.x < ix) {
            HeightRect left{existing.x, iy, ix - 1, iY, existing.h};
            if (left.is_valid()) result.push_back(left);
        }
        if (existing.X > iX) {
            HeightRect right{iX + 1, iy, existing.X, iY, existing.h};
            if (right.is_valid()) result.push_back(right);
        }

        return result;
    }
};
