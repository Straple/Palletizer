#pragma once

#include <solvers/height_handler.hpp>
#include <solvers/height_handler_rects.hpp>  // Для HeightRect
#include <vector>
#include <cstdint>

// Dense 2D Grid реализация HeightHandler
// Разбивает паллету на ячейки размером CELL_SIZE_X × CELL_SIZE_Y мм
// В каждой ячейке хранится максимальная высота
// Также хранит список прямоугольников для get_dots (как в HeightHandlerRects)
template<uint32_t CELL_SIZE_X = 10, uint32_t CELL_SIZE_Y = 10>
class HeightHandlerGridT : public HeightHandler {
private:
    std::vector<uint32_t> grid;  // heights[cy * grid_width + cx]
    uint32_t grid_width;         // Количество ячеек по X
    uint32_t grid_height;        // Количество ячеек по Y
    uint32_t pallet_length;      // Размер паллеты (мм)
    uint32_t pallet_width;       // Размер паллеты (мм)
    
    // Храним прямоугольники для get_dots (как в HeightHandlerRects)
    std::vector<HeightRect> height_rects;

    // Преобразование координат из мм в индексы ячеек
    [[nodiscard]] uint32_t to_cell_x(uint32_t x) const {
        return x / CELL_SIZE_X;
    }
    
    [[nodiscard]] uint32_t to_cell_y(uint32_t y) const {
        return y / CELL_SIZE_Y;
    }
    
    [[nodiscard]] uint32_t& cell_at(uint32_t cx, uint32_t cy) {
        return grid[cy * grid_width + cx];
    }
    
    [[nodiscard]] uint32_t cell_at(uint32_t cx, uint32_t cy) const {
        return grid[cy * grid_width + cx];
    }

public:
    // Конструктор: инициализация для паллеты заданного размера
    HeightHandlerGridT(uint32_t length, uint32_t width);
    
    // Получить максимальную высоту в прямоугольнике [x, X] × [y, Y] (включительно)
    [[nodiscard]] uint32_t get(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override;
    
    // Добавить прямоугольник с заданной высотой
    void add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) override;
    
    // Получить площадь (в мм²), которая находится на максимальной высоте в прямоугольнике
    [[nodiscard]] uint64_t get_area_at_max_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override;
    
    // Получить интересные точки для размещения коробки
    [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> get_dots(
        const TestDataHeader& header, const BoxSize& box) const override;
    
    // Количество прямоугольников
    [[nodiscard]] uint32_t size() const override { return height_rects.size(); }
};

// Алиас для стандартного использования
using HeightHandlerGrid = HeightHandlerGridT<10, 10>;

// ============== РЕАЛИЗАЦИЯ ШАБЛОНА ==============

template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
HeightHandlerGridT<CELL_SIZE_X, CELL_SIZE_Y>::HeightHandlerGridT(uint32_t length, uint32_t width)
    : pallet_length(length), pallet_width(width) {
    
    // Рассчитываем размер сетки
    grid_width = (length + CELL_SIZE_X - 1) / CELL_SIZE_X;
    grid_height = (width + CELL_SIZE_Y - 1) / CELL_SIZE_Y;
    
    // Инициализируем сетку нулями
    grid.resize(grid_width * grid_height, 0);
    
    // Добавляем начальный прямоугольник (пол)
    height_rects.push_back(HeightRect{0, 0, length - 1, width - 1, 0});
}

template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
uint32_t HeightHandlerGridT<CELL_SIZE_X, CELL_SIZE_Y>::get(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    // Преобразуем в индексы ячеек
    uint32_t cx1 = to_cell_x(x);
    uint32_t cy1 = to_cell_y(y);
    uint32_t cx2 = to_cell_x(X);
    uint32_t cy2 = to_cell_y(Y);
    
    // Ограничиваем индексы
    cx2 = std::min(cx2, grid_width - 1);
    cy2 = std::min(cy2, grid_height - 1);
    
    // Ищем максимум
    uint32_t max_h = 0;
    for (uint32_t cy = cy1; cy <= cy2; ++cy) {
        for (uint32_t cx = cx1; cx <= cx2; ++cx) {
            max_h = std::max(max_h, cell_at(cx, cy));
        }
    }
    return max_h;
}

template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
void HeightHandlerGridT<CELL_SIZE_X, CELL_SIZE_Y>::add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) {
    // Преобразуем в индексы ячеек
    uint32_t cx1 = to_cell_x(x);
    uint32_t cy1 = to_cell_y(y);
    uint32_t cx2 = to_cell_x(X);
    uint32_t cy2 = to_cell_y(Y);
    
    // Ограничиваем индексы
    cx2 = std::min(cx2, grid_width - 1);
    cy2 = std::min(cy2, grid_height - 1);
    
    // Обновляем ячейки
    for (uint32_t cy = cy1; cy <= cy2; ++cy) {
        for (uint32_t cx = cx1; cx <= cx2; ++cx) {
            cell_at(cx, cy) = std::max(cell_at(cx, cy), h);
        }
    }
    
    // Добавляем прямоугольник для get_dots
    height_rects.push_back(HeightRect{x, y, X, Y, h});
}

template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
uint64_t HeightHandlerGridT<CELL_SIZE_X, CELL_SIZE_Y>::get_area_at_max_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    // Преобразуем в индексы ячеек
    uint32_t cx1 = to_cell_x(x);
    uint32_t cy1 = to_cell_y(y);
    uint32_t cx2 = to_cell_x(X);
    uint32_t cy2 = to_cell_y(Y);
    
    // Ограничиваем индексы
    cx2 = std::min(cx2, grid_width - 1);
    cy2 = std::min(cy2, grid_height - 1);
    
    // Сначала находим максимальную высоту
    uint32_t max_h = get(x, y, X, Y);
    
    // Считаем площадь на этой высоте
    uint64_t area = 0;
    for (uint32_t cy = cy1; cy <= cy2; ++cy) {
        for (uint32_t cx = cx1; cx <= cx2; ++cx) {
            if (cell_at(cx, cy) == max_h) {
                // Вычисляем реальную площадь ячейки с учётом границ запроса
                uint32_t cell_x1 = cx * CELL_SIZE_X;
                uint32_t cell_y1 = cy * CELL_SIZE_Y;
                uint32_t cell_x2 = cell_x1 + CELL_SIZE_X - 1;
                uint32_t cell_y2 = cell_y1 + CELL_SIZE_Y - 1;
                
                // Пересечение с запросом
                uint32_t ix1 = std::max(x, cell_x1);
                uint32_t iy1 = std::max(y, cell_y1);
                uint32_t ix2 = std::min(X, cell_x2);
                uint32_t iy2 = std::min(Y, cell_y2);
                
                if (ix1 <= ix2 && iy1 <= iy2) {
                    area += static_cast<uint64_t>(ix2 - ix1 + 1) * (iy2 - iy1 + 1);
                }
            }
        }
    }
    return area;
}

template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
std::vector<std::pair<uint32_t, uint32_t>> HeightHandlerGridT<CELL_SIZE_X, CELL_SIZE_Y>::get_dots(
    const TestDataHeader& header, const BoxSize& box) const {
    
    // Используем тот же алгоритм что и в HeightHandlerRects
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
            if (std::max(px, px + box.length) <= header.length && std::max(py, py + box.width) <= header.width) {
                result.emplace_back(px, py);
            }
        }
    }
    
    return result;
}
