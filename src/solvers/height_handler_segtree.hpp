#pragma once

#include <solvers/height_handler.hpp>
#include <solvers/height_handler_rects.hpp>  // Для HeightRect
#include <vector>
#include <cstdint>

// 2D Segment Tree реализация HeightHandler
// Поддерживает:
// - Range max query: O(log² n)
// - Point/range update with max: O(log² n)
class HeightHandlerSegTree : public HeightHandler {
public:
    // Размеры ячеек для дискретизации
    static constexpr uint32_t CELL_SIZE_X = 10;  // мм
    static constexpr uint32_t CELL_SIZE_Y = 10;  // мм

private:
    // 2D Segment Tree: tree[x_node] содержит 1D segment tree по Y
    std::vector<std::vector<uint32_t>> tree;
    uint32_t tree_size_x;  // Размер дерева по X (степень двойки)
    uint32_t tree_size_y;  // Размер дерева по Y (степень двойки)
    uint32_t grid_width;   // Количество ячеек по X
    uint32_t grid_height;  // Количество ячеек по Y
    uint32_t pallet_length;
    uint32_t pallet_width;
    
    // Храним прямоугольники для get_dots
    std::vector<HeightRect> height_rects;

    // Преобразование координат
    [[nodiscard]] uint32_t to_cell_x(uint32_t x) const { return x / CELL_SIZE_X; }
    [[nodiscard]] uint32_t to_cell_y(uint32_t y) const { return y / CELL_SIZE_Y; }
    
    // Внутренние методы segment tree
    void update_y(uint32_t vx, uint32_t lx, uint32_t rx, uint32_t vy, uint32_t ly, uint32_t ry, uint32_t y1, uint32_t y2, uint32_t val);
    void update_x(uint32_t vx, uint32_t lx, uint32_t rx, uint32_t x1, uint32_t x2, uint32_t y1, uint32_t y2, uint32_t val);
    [[nodiscard]] uint32_t query_y(uint32_t vx, uint32_t vy, uint32_t ly, uint32_t ry, uint32_t y1, uint32_t y2) const;
    [[nodiscard]] uint32_t query_x(uint32_t vx, uint32_t lx, uint32_t rx, uint32_t x1, uint32_t x2, uint32_t y1, uint32_t y2) const;

public:
    // Конструктор
    HeightHandlerSegTree(uint32_t length, uint32_t width);
    
    // Получить максимальную высоту в прямоугольнике
    [[nodiscard]] uint32_t get(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override;
    
    // Добавить прямоугольник с заданной высотой
    void add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) override;
    
    // Получить площадь на максимальной высоте (используем grid для точности)
    [[nodiscard]] uint64_t get_area_at_max_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override;
    
    // Получить интересные точки для размещения коробки
    [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> get_dots(
        const TestDataHeader& header, const BoxSize& box) const override;
    
    // Количество прямоугольников
    [[nodiscard]] uint32_t size() const override { return height_rects.size(); }
};
