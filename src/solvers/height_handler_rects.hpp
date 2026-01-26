#pragma once

#include <solvers/height_handler.hpp>

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

// Реализация HeightHandler на основе списка непересекающихся прямоугольников
// Хранит информацию о текущих высотах палеты
class HeightHandlerRects : public HeightHandler {
    std::vector<HeightRect> height_rects;

public:
    // Конструктор: инициализирует с начальным прямоугольником (пол)
    HeightHandlerRects(uint32_t length, uint32_t width);
    
    [[nodiscard]] uint32_t get_h(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override;
    
    [[nodiscard]] uint64_t get_area(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override;

    void add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) override;
    
    // Старый интерфейс для совместимости
    void add_rect(HeightRect rect) { add_rect(rect.x, rect.y, rect.X, rect.Y, rect.h); }

    template<typename foo_t>
    void iterate(foo_t &&foo) {
        for (auto rect: height_rects) {
            foo(rect);
        }
    }

    // возвращает интересные точки для такой коробки
    [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> get_dots(const TestDataHeader &header, const BoxSize &box) const override;

    uint32_t size() const override {
        return height_rects.size();
    }
    
    // Получить список прямоугольников (для других реализаций)
    const std::vector<HeightRect>& get_rects() const { return height_rects; }

    void print();
};
