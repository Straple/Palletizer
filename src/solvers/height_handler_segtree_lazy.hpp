#pragma once

#include <solvers/height_handler.hpp>
#include <solvers/height_handler_rects.hpp>  // Для HeightRect
#include <vector>
#include <cstdint>
#include <algorithm>

// 2D Segment Tree с Lazy Propagation
// Поддерживает:
// - Range max query: O(log² n)
// - Range max update: O(log² n)
template<uint32_t CELL_SIZE_X = 10, uint32_t CELL_SIZE_Y = 10>
class HeightHandlerSegTreeLazyT : public HeightHandler {
private:
    // Узел 1D Segment Tree (для Y-размерности)
    struct YNode {
        uint32_t value = 0;      // Максимум в диапазоне
        uint32_t lazy = 0;       // Отложенное обновление
        bool has_lazy = false;   // Есть ли отложенное обновление
        
        void apply_lazy(uint32_t val) {
            value = std::max(value, val);
            lazy = std::max(lazy, val);
            has_lazy = true;
        }
        
        void push_lazy(YNode& left, YNode& right) {
            if (has_lazy) {
                left.apply_lazy(lazy);
                right.apply_lazy(lazy);
                has_lazy = false;
                lazy = 0;
            }
        }
    };
    
    // Узел 2D Segment Tree (для X-размерности)
    struct XNode {
        std::vector<YNode> y_tree;  // 1D Segment Tree по Y
        uint32_t lazy = 0;          // Отложенное обновление для всего Y-дерева
        bool has_lazy = false;      // Есть ли отложенное обновление
        
        XNode(uint32_t y_size) : y_tree(2 * y_size) {}
        
        void apply_lazy(uint32_t val, uint32_t y_size) {
            // Применяем lazy ко всему Y-дереву
            for (uint32_t i = 1; i < 2 * y_size; ++i) {
                y_tree[i].apply_lazy(val);
            }
            lazy = std::max(lazy, val);
            has_lazy = true;
        }
        
        void push_lazy(XNode& left, XNode& right, uint32_t y_size) {
            if (has_lazy) {
                left.apply_lazy(lazy, y_size);
                right.apply_lazy(lazy, y_size);
                has_lazy = false;
                lazy = 0;
            }
        }
    };
    
    std::vector<XNode> x_tree;  // 2D Segment Tree
    uint32_t tree_size_x;       // Размер дерева по X (степень двойки)
    uint32_t tree_size_y;       // Размер дерева по Y (степень двойки)
    uint32_t grid_width;        // Количество ячеек по X
    uint32_t grid_height;       // Количество ячеек по Y
    uint32_t pallet_length;
    uint32_t pallet_width;
    
    // Храним прямоугольники для get_dots
    std::vector<HeightRect> height_rects;

    // Преобразование координат
    [[nodiscard]] uint32_t to_cell_x(uint32_t x) const { return x / CELL_SIZE_X; }
    [[nodiscard]] uint32_t to_cell_y(uint32_t y) const { return y / CELL_SIZE_Y; }
    
    // Внутренние методы Y-дерева
    void y_update(uint32_t x_idx, uint32_t vy, uint32_t ly, uint32_t ry, uint32_t y1, uint32_t y2, uint32_t val);
    void y_push(uint32_t x_idx, uint32_t vy, uint32_t ly, uint32_t ry);
    [[nodiscard]] uint32_t y_query(uint32_t x_idx, uint32_t vy, uint32_t ly, uint32_t ry, uint32_t y1, uint32_t y2);
    
    // Внутренние методы X-дерева
    void x_update(uint32_t vx, uint32_t lx, uint32_t rx, uint32_t x1, uint32_t x2, uint32_t y1, uint32_t y2, uint32_t val);
    void x_push(uint32_t vx, uint32_t lx, uint32_t rx);
    [[nodiscard]] uint32_t x_query(uint32_t vx, uint32_t lx, uint32_t rx, uint32_t x1, uint32_t x2, uint32_t y1, uint32_t y2);

public:
    // Конструктор
    HeightHandlerSegTreeLazyT(uint32_t length, uint32_t width);
    
    // Получить максимальную высоту в прямоугольнике
    [[nodiscard]] uint32_t get(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override;
    
    // Добавить прямоугольник с заданной высотой
    void add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) override;
    
    // Получить площадь на максимальной высоте
    [[nodiscard]] uint64_t get_area_at_max_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override;
    
    // Получить интересные точки для размещения коробки
    [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> get_dots(
        const TestDataHeader& header, const BoxSize& box) const override;
    
    // Количество прямоугольников
    [[nodiscard]] uint32_t size() const override { return height_rects.size(); }
};

// Алиас для стандартного использования
using HeightHandlerSegTreeLazy = HeightHandlerSegTreeLazyT<10, 10>;

// ============== РЕАЛИЗАЦИЯ ШАБЛОНА ==============

// Найти следующую степень двойки
inline uint32_t lazy_next_power_of_two(uint32_t n) {
    uint32_t p = 1;
    while (p < n) p *= 2;
    return p;
}

template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
HeightHandlerSegTreeLazyT<CELL_SIZE_X, CELL_SIZE_Y>::HeightHandlerSegTreeLazyT(uint32_t length, uint32_t width)
    : pallet_length(length), pallet_width(width) {
    
    // Вычисляем размеры сетки
    grid_width = (length + CELL_SIZE_X - 1) / CELL_SIZE_X;
    grid_height = (width + CELL_SIZE_Y - 1) / CELL_SIZE_Y;
    
    // Размеры деревьев (степень двойки)
    tree_size_x = lazy_next_power_of_two(grid_width);
    tree_size_y = lazy_next_power_of_two(grid_height);
    
    // Инициализируем 2D segment tree с lazy propagation
    x_tree.reserve(2 * tree_size_x);
    for (uint32_t i = 0; i < 2 * tree_size_x; ++i) {
        x_tree.emplace_back(tree_size_y);
    }
    
    // Добавляем начальный прямоугольник (пол)
    height_rects.push_back(HeightRect{0, 0, length - 1, width - 1, 0});
}

// Проталкивание lazy в Y-дереве
template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
void HeightHandlerSegTreeLazyT<CELL_SIZE_X, CELL_SIZE_Y>::y_push(uint32_t x_idx, uint32_t vy, uint32_t ly, uint32_t ry) {
    if (ly == ry) return;  // Лист
    
    auto& node = x_tree[x_idx].y_tree[vy];
    if (node.has_lazy) {
        uint32_t mid = (ly + ry) / 2;
        if (2 * vy < x_tree[x_idx].y_tree.size()) {
            x_tree[x_idx].y_tree[2 * vy].apply_lazy(node.lazy);
        }
        if (2 * vy + 1 < x_tree[x_idx].y_tree.size()) {
            x_tree[x_idx].y_tree[2 * vy + 1].apply_lazy(node.lazy);
        }
        node.has_lazy = false;
        node.lazy = 0;
    }
}

// Обновление Y-дерева
template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
void HeightHandlerSegTreeLazyT<CELL_SIZE_X, CELL_SIZE_Y>::y_update(uint32_t x_idx, uint32_t vy, uint32_t ly, uint32_t ry,
                                                                     uint32_t y1, uint32_t y2, uint32_t val) {
    if (y1 > ry || y2 < ly) return;
    
    if (y1 <= ly && ry <= y2) {
        // Полное покрытие — применяем lazy
        x_tree[x_idx].y_tree[vy].apply_lazy(val);
        return;
    }
    
    // Частичное покрытие — проталкиваем lazy и рекурсивно обновляем
    y_push(x_idx, vy, ly, ry);
    
    uint32_t mid = (ly + ry) / 2;
    y_update(x_idx, 2 * vy, ly, mid, y1, y2, val);
    y_update(x_idx, 2 * vy + 1, mid + 1, ry, y1, y2, val);
    
    // Пересчитываем значение узла
    uint32_t left_val = (2 * vy < x_tree[x_idx].y_tree.size()) ? x_tree[x_idx].y_tree[2 * vy].value : 0;
    uint32_t right_val = (2 * vy + 1 < x_tree[x_idx].y_tree.size()) ? x_tree[x_idx].y_tree[2 * vy + 1].value : 0;
    x_tree[x_idx].y_tree[vy].value = std::max(left_val, right_val);
}

// Запрос Y-дерева
template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
uint32_t HeightHandlerSegTreeLazyT<CELL_SIZE_X, CELL_SIZE_Y>::y_query(uint32_t x_idx, uint32_t vy, uint32_t ly, uint32_t ry,
                                                                        uint32_t y1, uint32_t y2) {
    if (y1 > ry || y2 < ly) return 0;
    
    if (y1 <= ly && ry <= y2) {
        return x_tree[x_idx].y_tree[vy].value;
    }
    
    y_push(x_idx, vy, ly, ry);
    
    uint32_t mid = (ly + ry) / 2;
    return std::max(
        y_query(x_idx, 2 * vy, ly, mid, y1, y2),
        y_query(x_idx, 2 * vy + 1, mid + 1, ry, y1, y2)
    );
}

// Проталкивание lazy в X-дереве
template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
void HeightHandlerSegTreeLazyT<CELL_SIZE_X, CELL_SIZE_Y>::x_push(uint32_t vx, uint32_t lx, uint32_t rx) {
    if (lx == rx) return;  // Лист
    
    auto& node = x_tree[vx];
    if (node.has_lazy) {
        if (2 * vx < x_tree.size()) {
            x_tree[2 * vx].apply_lazy(node.lazy, tree_size_y);
        }
        if (2 * vx + 1 < x_tree.size()) {
            x_tree[2 * vx + 1].apply_lazy(node.lazy, tree_size_y);
        }
        node.has_lazy = false;
        node.lazy = 0;
    }
}

// Обновление X-дерева
template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
void HeightHandlerSegTreeLazyT<CELL_SIZE_X, CELL_SIZE_Y>::x_update(uint32_t vx, uint32_t lx, uint32_t rx,
                                                                     uint32_t x1, uint32_t x2, uint32_t y1, uint32_t y2, uint32_t val) {
    if (x1 > rx || x2 < lx) return;
    
    if (x1 <= lx && rx <= x2) {
        // Полное покрытие по X — обновляем Y-дерево
        y_update(vx, 1, 0, tree_size_y - 1, y1, y2, val);
        return;
    }
    
    // Частичное покрытие — проталкиваем lazy и рекурсивно обновляем
    x_push(vx, lx, rx);
    
    uint32_t mid = (lx + rx) / 2;
    x_update(2 * vx, lx, mid, x1, x2, y1, y2, val);
    x_update(2 * vx + 1, mid + 1, rx, x1, x2, y1, y2, val);
    
    // Пересчитываем Y-дерево текущего узла как max детей
    for (uint32_t i = 1; i < 2 * tree_size_y; ++i) {
        uint32_t left_val = (2 * vx < x_tree.size()) ? x_tree[2 * vx].y_tree[i].value : 0;
        uint32_t right_val = (2 * vx + 1 < x_tree.size()) ? x_tree[2 * vx + 1].y_tree[i].value : 0;
        x_tree[vx].y_tree[i].value = std::max(left_val, right_val);
    }
}

// Запрос X-дерева
template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
uint32_t HeightHandlerSegTreeLazyT<CELL_SIZE_X, CELL_SIZE_Y>::x_query(uint32_t vx, uint32_t lx, uint32_t rx,
                                                                        uint32_t x1, uint32_t x2, uint32_t y1, uint32_t y2) {
    if (x1 > rx || x2 < lx) return 0;
    
    if (x1 <= lx && rx <= x2) {
        return y_query(vx, 1, 0, tree_size_y - 1, y1, y2);
    }
    
    x_push(vx, lx, rx);
    
    uint32_t mid = (lx + rx) / 2;
    return std::max(
        x_query(2 * vx, lx, mid, x1, x2, y1, y2),
        x_query(2 * vx + 1, mid + 1, rx, x1, x2, y1, y2)
    );
}

template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
uint32_t HeightHandlerSegTreeLazyT<CELL_SIZE_X, CELL_SIZE_Y>::get(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    uint32_t cx1 = to_cell_x(x);
    uint32_t cy1 = to_cell_y(y);
    uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
    uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);
    
    return const_cast<HeightHandlerSegTreeLazyT*>(this)->x_query(1, 0, tree_size_x - 1, cx1, cx2, cy1, cy2);
}

template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
void HeightHandlerSegTreeLazyT<CELL_SIZE_X, CELL_SIZE_Y>::add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) {
    uint32_t cx1 = to_cell_x(x);
    uint32_t cy1 = to_cell_y(y);
    uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
    uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);
    
    x_update(1, 0, tree_size_x - 1, cx1, cx2, cy1, cy2, h);
    
    // Добавляем прямоугольник для get_dots
    height_rects.push_back(HeightRect{x, y, X, Y, h});
}

template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
uint64_t HeightHandlerSegTreeLazyT<CELL_SIZE_X, CELL_SIZE_Y>::get_area_at_max_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    uint32_t cx1 = to_cell_x(x);
    uint32_t cy1 = to_cell_y(y);
    uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
    uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);
    
    // Находим максимальную высоту
    uint32_t max_h = get(x, y, X, Y);
    
    // Считаем площадь через точечные запросы (можно оптимизировать)
    uint64_t area = 0;
    for (uint32_t cx = cx1; cx <= cx2; ++cx) {
        for (uint32_t cy = cy1; cy <= cy2; ++cy) {
            uint32_t cell_h = const_cast<HeightHandlerSegTreeLazyT*>(this)->x_query(1, 0, tree_size_x - 1, cx, cx, cy, cy);
            if (cell_h == max_h) {
                // Размер ячейки с учётом границ запроса
                uint32_t cell_x_start = cx * CELL_SIZE_X;
                uint32_t cell_y_start = cy * CELL_SIZE_Y;
                uint32_t cell_x_end = cell_x_start + CELL_SIZE_X - 1;
                uint32_t cell_y_end = cell_y_start + CELL_SIZE_Y - 1;
                
                uint32_t ix1 = std::max(cell_x_start, x);
                uint32_t iy1 = std::max(cell_y_start, y);
                uint32_t ix2 = std::min(cell_x_end, X);
                uint32_t iy2 = std::min(cell_y_end, Y);
                
                if (ix1 <= ix2 && iy1 <= iy2) {
                    area += static_cast<uint64_t>(ix2 - ix1 + 1) * (iy2 - iy1 + 1);
                }
            }
        }
    }
    
    return area;
}

template<uint32_t CELL_SIZE_X, uint32_t CELL_SIZE_Y>
std::vector<std::pair<uint32_t, uint32_t>> HeightHandlerSegTreeLazyT<CELL_SIZE_X, CELL_SIZE_Y>::get_dots(
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
            if (std::max(px, px + box.length) <= header.length && std::max(py, py + box.width) <= header.width) {
                result.emplace_back(px, py);
            }
        }
    }
    
    return result;
}