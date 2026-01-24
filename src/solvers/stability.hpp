#pragma once

#include <objects/answer.hpp>
#include <objects/test_data.hpp>

#include <cstdint>
#include <vector>

// Метрики устойчивости паллеты
struct StabilityMetrics {
    // Перевязка слоёв (Interlocking)
    // G = K_int × (L_SumPer − L_Sum)
    double l_sum_per = 0;     // Суммарный периметр оснований всех коробок
    double l_sum = 0;         // Суммарная длина совпадающих кусков периметров
    double interlocking = 0;  // Коэффициент перевязки (чем больше, тем лучше)
    
    // Центр тяжести
    double center_of_mass_x = 0;
    double center_of_mass_y = 0;
    double center_of_mass_z = 0;  // Высота центра масс
    double total_weight = 0;
    
    // Нормализованные метрики (0-1)
    double interlocking_ratio = 0;  // l_sum / l_sum_per (чем меньше, тем лучше перевязка)
    double stability_score = 0;     // Общий коэффициент устойчивости
};

// Структура для хранения ребра (для расчёта совпадающих рёбер)
struct Edge {
    uint32_t x1, y1, x2, y2;  // Координаты начала и конца
    uint32_t z;               // Высота (нижняя грань коробки)
    bool is_horizontal;       // true = горизонтальное, false = вертикальное
    
    // Длина ребра
    [[nodiscard]] uint32_t length() const {
        if (is_horizontal) {
            return x2 - x1;
        } else {
            return y2 - y1;
        }
    }
};

// Рассчитать метрики устойчивости для заданной укладки
StabilityMetrics calc_stability(const TestData& test_data, const Answer& answer);

// Рассчитать длину совпадающих рёбер между двумя коробками
// (коробки должны быть на разных уровнях по Z)
uint32_t calc_overlapping_edges(const Position& box1, const Position& box2);

// Проверка, совпадают ли два горизонтальных отрезка (частично или полностью)
// Возвращает длину совпадения
uint32_t segment_overlap_length(uint32_t a1, uint32_t a2, uint32_t b1, uint32_t b2);
