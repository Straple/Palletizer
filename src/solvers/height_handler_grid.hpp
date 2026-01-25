#pragma once

#include <solvers/height_handler.hpp>
#include <solvers/height_handler_rects.hpp>  // Для HeightRect
#include <vector>
#include <cstdint>

// Dense 2D Grid реализация HeightHandler
// Разбивает паллету на ячейки размером CELL_SIZE_X × CELL_SIZE_Y мм
// В каждой ячейке хранится максимальная высота
// Также хранит список прямоугольников для get_dots (как в HeightHandlerRects)
class HeightHandlerGrid : public HeightHandler {
public:
    // Размеры ячеек (можно настроить)
    static constexpr uint32_t CELL_SIZE_X = 10;  // мм
    static constexpr uint32_t CELL_SIZE_Y = 10;  // мм

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
    HeightHandlerGrid(uint32_t length, uint32_t width);
    
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
