#pragma once

#include <objects/test_data.hpp>
#include <vector>
#include <algorithm>
#include <cstdint>

// Простой прямоугольник с высотой
struct HeightRect {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t X = 0;
    uint32_t Y = 0;
    uint32_t h = 0;

    [[nodiscard]] bool intersects(const HeightRect &other) const {
        return x <= other.X && other.x <= X && y <= other.Y && other.y <= Y;
    }

    [[nodiscard]] bool is_valid() const {
        return x <= X && y <= Y;
    }
};

// HeightHandler — простейшая baseline реализация
// Просто хранит все добавленные прямоугольники, честно перебирает их
// Используется как эталон для тестирования других реализаций
class HeightHandler {
    std::vector<HeightRect> height_rects;
    uint32_t pallet_length;
    uint32_t pallet_width;

public:
    HeightHandler(uint32_t length, uint32_t width) 
        : pallet_length(length), pallet_width(width) {
        height_rects.push_back(HeightRect{0, 0, length - 1, width - 1, 0});
    }
    
    // Получить максимальную высоту в прямоугольнике [x, X] × [y, Y]
    // Просто перебираем все прямоугольники
    [[nodiscard]] uint32_t get_h(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
        uint32_t max_h = 0;
        for (const auto& rect : height_rects) {
            // Проверяем пересечение
            if (rect.x <= X && x <= rect.X && rect.y <= Y && y <= rect.Y) {
                max_h = std::max(max_h, rect.h);
            }
        }
        return max_h;
    }
    
    // Получить площадь на максимальной высоте в прямоугольнике
    [[nodiscard]] uint64_t get_area(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
        uint32_t max_h = get_h(x, y, X, Y);
        
        // Считаем площадь пересечений с прямоугольниками максимальной высоты
        uint64_t area = 0;
        for (const auto& rect : height_rects) {
            if (rect.h == max_h && rect.x <= X && x <= rect.X && rect.y <= Y && y <= rect.Y) {
                uint32_t ix = std::max(x, rect.x);
                uint32_t iX = std::min(X, rect.X);
                uint32_t iy = std::max(y, rect.y);
                uint32_t iY = std::min(Y, rect.Y);
                area += static_cast<uint64_t>(iX - ix + 1) * (iY - iy + 1);
            }
        }
        return area;
    }

    void add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) {
        HeightRect rect{x, y, X, Y, h};
        if (rect.is_valid()) {
            height_rects.push_back(rect);
        }
    }
    
    // Получить интересные точки для размещения коробки
    [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> get_dots(
        const TestDataHeader& header, const BoxSize& box) const {
        
        std::vector<std::pair<uint32_t, uint32_t>> result;
        
        // Добавляем угол (0, 0)
        if (box.length <= header.length && box.width <= header.width) {
            result.emplace_back(0, 0);
        }
        
        // Добавляем точки от углов всех прямоугольников
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
    
    [[nodiscard]] uint32_t size() const { return height_rects.size(); }
    
    [[nodiscard]] const std::vector<HeightRect>& get_rects() const { return height_rects; }
};
