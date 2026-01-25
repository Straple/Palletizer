#pragma once

#include <solvers/height_handler.hpp>
#include <solvers/height_handler_rects.hpp>  // Для HeightRect
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>

// Quadtree реализация HeightHandler
// Рекурсивное разбиение пространства на 4 квадранта
// Поддерживает:
// - Point/range max query: O(log n) в среднем
// - Range update with max: O(log n) в среднем
template<uint32_t MIN_SIZE = 10>
class HeightHandlerQuadtreeT : public HeightHandler {
private:
    // Узел quadtree
    struct Node {
        uint32_t x, y, X, Y;  // Границы узла (включительно)
        uint32_t max_height = 0;  // Максимальная высота в узле
        bool is_uniform = true;   // Все ячейки имеют одинаковую высоту
        std::unique_ptr<Node> children[4];  // NW, NE, SW, SE
        
        Node(uint32_t x_, uint32_t y_, uint32_t X_, uint32_t Y_)
            : x(x_), y(y_), X(X_), Y(Y_) {}
        
        bool is_leaf() const {
            return !children[0];
        }
        
        uint32_t width() const { return X - x + 1; }
        uint32_t height() const { return Y - y + 1; }
    };
    
    std::unique_ptr<Node> root;
    uint32_t pallet_length;
    uint32_t pallet_width;
    
    // Храним прямоугольники для get_dots
    std::vector<HeightRect> height_rects;
    
    // Внутренние методы
    void update_node(Node* node, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t h);
    [[nodiscard]] uint32_t query_node(const Node* node, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) const;
    void subdivide(Node* node);
    uint64_t calc_area_at_max(const Node* node, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t target_h) const;

public:
    // Конструктор
    HeightHandlerQuadtreeT(uint32_t length, uint32_t width);
    
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
using HeightHandlerQuadtree = HeightHandlerQuadtreeT<10>;

// ============== РЕАЛИЗАЦИЯ ШАБЛОНА ==============

// Проверка пересечения прямоугольников
inline bool qt_intersects(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2,
                          uint32_t X1, uint32_t Y1, uint32_t X2, uint32_t Y2) {
    return x1 <= X2 && x2 >= X1 && y1 <= Y2 && y2 >= Y1;
}

// Проверка что rect1 полностью внутри rect2
inline bool qt_contains(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2,
                        uint32_t X1, uint32_t Y1, uint32_t X2, uint32_t Y2) {
    return X1 <= x1 && x2 <= X2 && Y1 <= y1 && y2 <= Y2;
}

template<uint32_t MIN_SIZE>
HeightHandlerQuadtreeT<MIN_SIZE>::HeightHandlerQuadtreeT(uint32_t length, uint32_t width)
    : pallet_length(length), pallet_width(width) {
    
    // Создаём корневой узел
    root = std::make_unique<Node>(0, 0, length - 1, width - 1);
    
    // Добавляем начальный прямоугольник (пол)
    height_rects.push_back(HeightRect{0, 0, length - 1, width - 1, 0});
}

// Разбивает узел на 4 дочерних
template<uint32_t MIN_SIZE>
void HeightHandlerQuadtreeT<MIN_SIZE>::subdivide(Node* node) {
    if (!node->is_leaf()) return;
    
    uint32_t mid_x = (node->x + node->X) / 2;
    uint32_t mid_y = (node->y + node->Y) / 2;
    
    // NW (top-left)
    node->children[0] = std::make_unique<Node>(node->x, node->y, mid_x, mid_y);
    // NE (top-right)
    node->children[1] = std::make_unique<Node>(mid_x + 1, node->y, node->X, mid_y);
    // SW (bottom-left)
    node->children[2] = std::make_unique<Node>(node->x, mid_y + 1, mid_x, node->Y);
    // SE (bottom-right)
    node->children[3] = std::make_unique<Node>(mid_x + 1, mid_y + 1, node->X, node->Y);
    
    // Передаём высоту детям
    for (int i = 0; i < 4; ++i) {
        if (node->children[i]) {
            node->children[i]->max_height = node->max_height;
            node->children[i]->is_uniform = true;
        }
    }
    
    node->is_uniform = false;
}

template<uint32_t MIN_SIZE>
void HeightHandlerQuadtreeT<MIN_SIZE>::update_node(Node* node, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t h) {
    if (!node) return;
    
    // Нет пересечения
    if (!qt_intersects(x1, y1, x2, y2, node->x, node->y, node->X, node->Y)) {
        return;
    }
    
    // Узел полностью внутри области обновления
    if (qt_contains(node->x, node->y, node->X, node->Y, x1, y1, x2, y2)) {
        uint32_t new_height = std::max(node->max_height, h);
        
        // Если высота не изменилась и узел uniform, ничего не делаем
        if (new_height == node->max_height && node->is_uniform) {
            return;
        }
        
        node->max_height = new_height;
        
        // При полном покрытии узел становится uniform
        node->is_uniform = true;
        
        // Если есть дети, обновляем их тоже для согласованности
        if (!node->is_leaf()) {
            for (int i = 0; i < 4; ++i) {
                if (node->children[i]) {
                    node->children[i]->max_height = new_height;
                    node->children[i]->is_uniform = true;
                }
            }
        }
        return;
    }
    
    // Частичное пересечение — разбиваем если нужно
    if (node->is_leaf()) {
        // Разбиваем только если размер достаточный
        if (node->width() > MIN_SIZE && node->height() > MIN_SIZE) {
            subdivide(node);
        } else {
            // Слишком маленький — просто обновляем
            node->max_height = std::max(node->max_height, h);
            return;
        }
    }
    
    // Обновляем детей
    for (int i = 0; i < 4; ++i) {
        update_node(node->children[i].get(), x1, y1, x2, y2, h);
    }
    
    // Пересчитываем max_height и is_uniform узла
    node->max_height = 0;
    bool all_same_height = true;
    uint32_t first_child_height = 0;
    bool found_first = false;
    
    for (int i = 0; i < 4; ++i) {
        if (node->children[i]) {
            node->max_height = std::max(node->max_height, node->children[i]->max_height);
            
            // Проверяем uniform: все дети должны иметь одинаковую высоту и быть uniform
            if (!found_first) {
                first_child_height = node->children[i]->max_height;
                found_first = true;
            }
            
            if (node->children[i]->max_height != first_child_height || !node->children[i]->is_uniform) {
                all_same_height = false;
            }
        }
    }
    
    node->is_uniform = all_same_height;
}

template<uint32_t MIN_SIZE>
uint32_t HeightHandlerQuadtreeT<MIN_SIZE>::query_node(const Node* node, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) const {
    if (!node) return 0;
    
    // Нет пересечения
    if (!qt_intersects(x1, y1, x2, y2, node->x, node->y, node->X, node->Y)) {
        return 0;
    }
    
    // Узел полностью внутри области запроса
    if (qt_contains(node->x, node->y, node->X, node->Y, x1, y1, x2, y2)) {
        return node->max_height;
    }
    
    // Лист или маленький узел — возвращаем его значение
    if (node->is_leaf()) {
        return node->max_height;
    }
    
    // Запрашиваем у детей
    uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
        result = std::max(result, query_node(node->children[i].get(), x1, y1, x2, y2));
    }
    return result;
}

template<uint32_t MIN_SIZE>
uint64_t HeightHandlerQuadtreeT<MIN_SIZE>::calc_area_at_max(const Node* node, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t target_h) const {
    if (!node) return 0;
    
    // Нет пересечения
    if (!qt_intersects(x1, y1, x2, y2, node->x, node->y, node->X, node->Y)) {
        return 0;
    }
    
    // Пересечение с запросом
    uint32_t ix1 = std::max(x1, node->x);
    uint32_t iy1 = std::max(y1, node->y);
    uint32_t ix2 = std::min(x2, node->X);
    uint32_t iy2 = std::min(y2, node->Y);
    
    // Если узел однородный или лист
    if (node->is_leaf() || node->is_uniform) {
        if (node->max_height == target_h) {
            return static_cast<uint64_t>(ix2 - ix1 + 1) * (iy2 - iy1 + 1);
        }
        return 0;
    }
    
    // Запрашиваем у детей
    uint64_t area = 0;
    for (int i = 0; i < 4; ++i) {
        area += calc_area_at_max(node->children[i].get(), x1, y1, x2, y2, target_h);
    }
    return area;
}

template<uint32_t MIN_SIZE>
uint32_t HeightHandlerQuadtreeT<MIN_SIZE>::get(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    return query_node(root.get(), x, y, X, Y);
}

template<uint32_t MIN_SIZE>
void HeightHandlerQuadtreeT<MIN_SIZE>::add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) {
    update_node(root.get(), x, y, X, Y, h);
    
    // Добавляем прямоугольник для get_dots
    height_rects.push_back(HeightRect{x, y, X, Y, h});
}

template<uint32_t MIN_SIZE>
uint64_t HeightHandlerQuadtreeT<MIN_SIZE>::get_area_at_max_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    uint32_t max_h = query_node(root.get(), x, y, X, Y);
    return calc_area_at_max(root.get(), x, y, X, Y, max_h);
}

template<uint32_t MIN_SIZE>
std::vector<std::pair<uint32_t, uint32_t>> HeightHandlerQuadtreeT<MIN_SIZE>::get_dots(
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
