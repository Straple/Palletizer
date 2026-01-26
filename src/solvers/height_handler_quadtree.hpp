#pragma once

#include <solvers/height_handler.hpp>
#include <solvers/height_handler_rects.hpp>  // Для HeightRect
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>

// Quadtree реализация HeightHandler (упрощённая корректная версия)
template<uint32_t MIN_SIZE = 10>
class HeightHandlerQuadtreeT : public HeightHandler {
private:
    // Узел quadtree (упрощённый)
    struct Node {
        uint32_t x, y, X, Y;  // Границы узла (включительно)
        uint32_t max_height = 0;  // Максимальная высота в узле
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

public:
    // Конструктор
    HeightHandlerQuadtreeT(uint32_t length, uint32_t width);
    
    // Получить максимальную высоту в прямоугольнике
    [[nodiscard]] uint32_t get_h(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override;
    
    // Добавить прямоугольник с заданной высотой
    void add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) override;
    
    // Получить площадь на максимальной высоте (используем Rects подход)
    [[nodiscard]] uint64_t get_area(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override;
    
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
        }
    }
}

template<uint32_t MIN_SIZE>
void HeightHandlerQuadtreeT<MIN_SIZE>::update_node(Node* node, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t h) {
    if (!node) return;
    
    // Нет пересечения
    if (!qt_intersects(x1, y1, x2, y2, node->x, node->y, node->X, node->Y)) {
        return;
    }
    
    // Узел полностью внутри области обновления — можно обновить max_height
    if (qt_contains(node->x, node->y, node->X, node->Y, x1, y1, x2, y2)) {
        node->max_height = std::max(node->max_height, h);
        // Если есть дети, обновляем их тоже для согласованности
        if (!node->is_leaf()) {
            for (int i = 0; i < 4; ++i) {
                update_node(node->children[i].get(), x1, y1, x2, y2, h);
            }
        }
        return;
    }
    
    // Частичное пересечение - НЕ обновляем max_height родителя напрямую!
    // Нужно разбить и спуститься к детям
    if (node->is_leaf()) {
        // Если слишком маленький для разбиения, обновляем весь узел
        // (это приводит к переоценке, но гарантирует корректность)
        if (node->width() <= MIN_SIZE || node->height() <= MIN_SIZE) {
            node->max_height = std::max(node->max_height, h);
            return;
        }
        subdivide(node);
    }
    
    // Рекурсивно обновляем детей
    for (int i = 0; i < 4; ++i) {
        update_node(node->children[i].get(), x1, y1, x2, y2, h);
    }
    
    // Пересчитываем max_height родителя ИЗ детей
    node->max_height = 0;
    for (int i = 0; i < 4; ++i) {
        if (node->children[i]) {
            node->max_height = std::max(node->max_height, node->children[i]->max_height);
        }
    }
}

template<uint32_t MIN_SIZE>
uint32_t HeightHandlerQuadtreeT<MIN_SIZE>::query_node(const Node* node, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) const {
    if (!node) return 0;
    
    // Нет пересечения
    if (!qt_intersects(x1, y1, x2, y2, node->x, node->y, node->X, node->Y)) {
        return 0;
    }
    
    // Если лист или узел полностью внутри запроса
    if (node->is_leaf() || qt_contains(node->x, node->y, node->X, node->Y, x1, y1, x2, y2)) {
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
uint32_t HeightHandlerQuadtreeT<MIN_SIZE>::get_h(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    return query_node(root.get(), x, y, X, Y);
}

template<uint32_t MIN_SIZE>
void HeightHandlerQuadtreeT<MIN_SIZE>::add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) {
    update_node(root.get(), x, y, X, Y, h);
    
    // Добавляем прямоугольник для get_dots
    height_rects.push_back(HeightRect{x, y, X, Y, h});
}

template<uint32_t MIN_SIZE>
uint64_t HeightHandlerQuadtreeT<MIN_SIZE>::get_area(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    // Упрощённый подход: используем алгоритм из HeightHandlerRects
    uint32_t max_h = get(x, y, X, Y);
    
    uint64_t area = 0;
    
    // Проходим по всем прямоугольникам и считаем площадь пересечения с нужной высотой
    for (const auto& rect : height_rects) {
        if (rect.h == max_h) {
            // Пересечение прямоугольников
            uint32_t ix1 = std::max(x, rect.x);
            uint32_t iy1 = std::max(y, rect.y);
            uint32_t ix2 = std::min(X, rect.X);
            uint32_t iy2 = std::min(Y, rect.Y);
            
            if (ix1 <= ix2 && iy1 <= iy2) {
                area += static_cast<uint64_t>(ix2 - ix1 + 1) * (iy2 - iy1 + 1);
            }
        }
    }
    
    return area;
}

template<uint32_t MIN_SIZE>
std::vector<std::pair<uint32_t, uint32_t>> HeightHandlerQuadtreeT<MIN_SIZE>::get_dots(
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