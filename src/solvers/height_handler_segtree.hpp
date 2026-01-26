#pragma once

#include <solvers/height_handler.hpp>
#include <vector>
#include <cstdint>
#include <algorithm>

// 2D Segment Tree с Lazy Propagation
// В каждом узле хранится (max_h, count) — максимальная высота и количество ячеек с этой высотой
// Это позволяет эффективно вычислять get_area() за O(log² n)
//
// Операции:
// - get_h():    O(log² n) — запрос максимума
// - get_area(): O(log² n) — запрос площади на максимальной высоте
// - add_rect(): O(ширина × log n) — обновление (X без lazy, Y с lazy)
template<uint32_t CELL_SIZE_X = 10, uint32_t CELL_SIZE_Y = 10>
class HeightHandlerSegTreeT {
private:
    // Узел Y-дерева: хранит (max_h, count) с lazy propagation
    struct YNode {
        uint32_t max_h = 0;      // Максимальная высота в диапазоне
        uint64_t count = 0;      // Количество ячеек с максимальной высотой
        uint32_t lazy = 0;       // Отложенное max-обновление
        uint32_t size = 0;       // Количество ячеек в поддиапазоне
        
        void apply_lazy(uint32_t val) {
            if (val > max_h) {
                max_h = val;
                count = size;  // Вся область теперь на новой высоте
            }
            lazy = std::max(lazy, val);
        }
        
        // Мерж двух узлов
        static YNode merge(const YNode& left, const YNode& right) {
            YNode result;
            result.size = left.size + right.size;
            
            if (left.max_h > right.max_h) {
                result.max_h = left.max_h;
                result.count = left.count;
            } else if (left.max_h < right.max_h) {
                result.max_h = right.max_h;
                result.count = right.count;
            } else {
                result.max_h = left.max_h;
                result.count = left.count + right.count;
            }
            return result;
        }
    };
    
    // X-узел содержит Y-дерево
    struct XNode {
        std::vector<YNode> y_tree;
        XNode(uint32_t y_size) : y_tree(4 * y_size) {}
    };
    
    std::vector<XNode> x_tree;
    uint32_t grid_width;
    uint32_t grid_height;
    uint32_t tree_size_y;
    uint32_t pallet_length;
    uint32_t pallet_width;
    
    std::vector<HeightRect> height_rects;

    [[nodiscard]] uint32_t to_cell_x(uint32_t x) const { return x / CELL_SIZE_X; }
    [[nodiscard]] uint32_t to_cell_y(uint32_t y) const { return y / CELL_SIZE_Y; }
    
    // Y-дерево: push lazy вниз
    void y_push(uint32_t x_idx, uint32_t v, uint32_t tl, uint32_t tr) {
        if (tl >= tr) return;
        auto& node = x_tree[x_idx].y_tree[v];
        if (node.lazy > 0) {
            uint32_t tm = (tl + tr) / 2;
            x_tree[x_idx].y_tree[2*v].apply_lazy(node.lazy);
            x_tree[x_idx].y_tree[2*v+1].apply_lazy(node.lazy);
            node.lazy = 0;
        }
    }
    
    // Y-дерево: построение (инициализация размеров)
    void y_build(uint32_t x_idx, uint32_t v, uint32_t tl, uint32_t tr) {
        auto& node = x_tree[x_idx].y_tree[v];
        node.size = tr - tl + 1;
        node.count = tr - tl + 1;  // Изначально все на высоте 0
        node.max_h = 0;
        node.lazy = 0;
        
        if (tl < tr) {
            uint32_t tm = (tl + tr) / 2;
            y_build(x_idx, 2*v, tl, tm);
            y_build(x_idx, 2*v+1, tm+1, tr);
        }
    }
    
    // Y-дерево: range max update с lazy
    void y_update(uint32_t x_idx, uint32_t v, uint32_t tl, uint32_t tr, 
                  uint32_t l, uint32_t r, uint32_t val) {
        if (l > r || l > tr || r < tl) return;
        
        if (l <= tl && tr <= r) {
            x_tree[x_idx].y_tree[v].apply_lazy(val);
            return;
        }
        
        y_push(x_idx, v, tl, tr);
        
        uint32_t tm = (tl + tr) / 2;
        y_update(x_idx, 2*v, tl, tm, l, r, val);
        y_update(x_idx, 2*v+1, tm+1, tr, l, r, val);
        
        x_tree[x_idx].y_tree[v] = YNode::merge(
            x_tree[x_idx].y_tree[2*v],
            x_tree[x_idx].y_tree[2*v+1]
        );
        x_tree[x_idx].y_tree[v].lazy = 0;
    }
    
    // Y-дерево: range query для (max_h, count)
    YNode y_query(uint32_t x_idx, uint32_t v, uint32_t tl, uint32_t tr,
                  uint32_t l, uint32_t r) {
        if (l > r || l > tr || r < tl) {
            YNode empty;
            empty.max_h = 0;
            empty.count = 0;
            empty.size = 0;
            return empty;
        }
        
        if (l <= tl && tr <= r) {
            return x_tree[x_idx].y_tree[v];
        }
        
        y_push(x_idx, v, tl, tr);
        
        uint32_t tm = (tl + tr) / 2;
        YNode left_result = y_query(x_idx, 2*v, tl, tm, l, r);
        YNode right_result = y_query(x_idx, 2*v+1, tm+1, tr, l, r);
        
        return YNode::merge(left_result, right_result);
    }

public:
    HeightHandlerSegTreeT(uint32_t length, uint32_t width)
        : pallet_length(length), pallet_width(width) {
        
        grid_width = (length + CELL_SIZE_X - 1) / CELL_SIZE_X;
        grid_height = (width + CELL_SIZE_Y - 1) / CELL_SIZE_Y;
        tree_size_y = grid_height;
        
        // Инициализируем X-узлы с Y-деревьями
        x_tree.reserve(grid_width);
        for (uint32_t i = 0; i < grid_width; ++i) {
            x_tree.emplace_back(tree_size_y);
            y_build(i, 1, 0, tree_size_y - 1);
        }
        
        height_rects.push_back(HeightRect{0, 0, length - 1, width - 1, 0});
    }
    
    [[nodiscard]] uint32_t get_h(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
        uint32_t cx1 = to_cell_x(x);
        uint32_t cy1 = to_cell_y(y);
        uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
        uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);
        
        uint32_t max_h = 0;
        for (uint32_t cx = cx1; cx <= cx2; ++cx) {
            YNode result = const_cast<HeightHandlerSegTreeT*>(this)->y_query(
                cx, 1, 0, tree_size_y - 1, cy1, cy2);
            max_h = std::max(max_h, result.max_h);
        }
        return max_h;
    }
    
    [[nodiscard]] uint64_t get_area(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
        uint32_t cx1 = to_cell_x(x);
        uint32_t cy1 = to_cell_y(y);
        uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
        uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);
        
        // Сначала находим максимальную высоту
        uint32_t max_h = 0;
        for (uint32_t cx = cx1; cx <= cx2; ++cx) {
            YNode result = const_cast<HeightHandlerSegTreeT*>(this)->y_query(
                cx, 1, 0, tree_size_y - 1, cy1, cy2);
            max_h = std::max(max_h, result.max_h);
        }
        
        // Теперь считаем количество ячеек с max_h
        uint64_t total_count = 0;
        for (uint32_t cx = cx1; cx <= cx2; ++cx) {
            YNode result = const_cast<HeightHandlerSegTreeT*>(this)->y_query(
                cx, 1, 0, tree_size_y - 1, cy1, cy2);
            if (result.max_h == max_h) {
                total_count += result.count;
            }
        }
        
        // Конвертируем ячейки в пиксели (с учётом границ запроса)
        // Упрощение: каждая ячейка = CELL_SIZE_X * CELL_SIZE_Y
        return total_count * CELL_SIZE_X * CELL_SIZE_Y;
    }

    void add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) {
        uint32_t cx1 = to_cell_x(x);
        uint32_t cy1 = to_cell_y(y);
        uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
        uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);
        
        for (uint32_t cx = cx1; cx <= cx2; ++cx) {
            y_update(cx, 1, 0, tree_size_y - 1, cy1, cy2, h);
        }
        
        height_rects.push_back(HeightRect{x, y, X, Y, h});
    }
    
    [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> get_dots(
        const TestDataHeader& header, const BoxSize& box) const {
        
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
    
    [[nodiscard]] uint32_t size() const { return height_rects.size(); }
};

using HeightHandlerSegTree = HeightHandlerSegTreeT<10, 10>;
