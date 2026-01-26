#pragma once

#include <objects/test_data.hpp>
#include <vector>
#include <cstdint>

// Абстрактный базовый класс для HeightHandler
// Все реализации наследуются от него
class HeightHandler {
public:
    virtual ~HeightHandler() = default;
    
    // Получить максимальную высоту в прямоугольнике [x, X] × [y, Y] (включительно)
    [[nodiscard]] virtual uint32_t get_h(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const = 0;
    
    // Добавить прямоугольник с заданной высотой
    virtual void add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) = 0;
    
    // Получить площадь (в мм²), которая находится на максимальной высоте в прямоугольнике
    [[nodiscard]] virtual uint64_t get_area(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const = 0;
    
    // Получить интересные точки для размещения коробки
    [[nodiscard]] virtual std::vector<std::pair<uint32_t, uint32_t>> get_dots(
        const TestDataHeader& header, const BoxSize& box) const = 0;
    
    // Количество внутренних элементов (для статистики)
    [[nodiscard]] virtual uint32_t size() const = 0;
};
