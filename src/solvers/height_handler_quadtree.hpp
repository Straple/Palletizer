#pragma once

#include <solvers/height_handler.hpp>
#include <solvers/height_handler_rects.hpp>  // Для HeightRect
#include <vector>
#include <memory>
#include <cstdint>

// Quadtree реализация HeightHandler
// Рекурсивное разбиение пространства на 4 квадранта
// Поддерживает:
// - Point/range max query: O(log n) в среднем
// - Range update with max: O(log n) в среднем
class HeightHandlerQuadtree : public HeightHandler {
public:
    // Минимальный размер узла (не делим дальше)
    static constexpr uint32_t MIN_SIZE = 10;  // мм

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
    HeightHandlerQuadtree(uint32_t length, uint32_t width);
    
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
