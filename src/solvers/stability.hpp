#pragma once

#include <objects/answer.hpp>
#include <objects/test_data.hpp>

#include <cstdint>
#include <vector>

// Центр масс паллеты
struct CenterOfMass {
    double x = 0;  // Координата X центра масс (мм)
    double y = 0;  // Координата Y центра масс (мм)
    double z = 0;  // Координата Z центра масс (мм) - высота
    double total_weight = 0;  // Суммарный вес всех коробок
};

// Метрики устойчивости паллеты
struct StabilityMetrics {
    // Перевязка слоёв (Interlocking) — версия 1: совпадение рёбер
    // G = K_int × (L_SumPer − L_Sum)
    double l_sum_per = 0;     // Суммарный периметр оснований всех коробок
    double l_sum = 0;         // Суммарная длина совпадающих кусков периметров
    double interlocking = 0;  // Коэффициент перевязки (чем больше, тем лучше)
    
    // Опора площади — версия 2 (с HeightHandler)
    double supported_area = 0;  // Площадь, которая опирается на что-то
    double hanging_area = 0;    // Площадь, которая "висит" в воздухе
    double total_area = 0;      // Суммарная площадь оснований всех коробок
    
    // Метрики для отдельных коробок
    double min_support_ratio = 1.0;  // Минимальный % опоры среди всех коробок (0-1)
    uint32_t unstable_boxes_count = 0;  // Количество коробок с опорой < 70%
    uint32_t total_boxes_above_floor = 0;  // Всего коробок не на полу
    
    // Центр тяжести
    CenterOfMass center_of_mass;
    
    // Нормализованные метрики (0-1)
    double interlocking_ratio = 0;    // l_sum / l_sum_per (чем меньше, тем лучше)
    double stability = 0;             // 1 - interlocking_ratio
    double stability_area = 0;        // supported_area / total_area
    double stability_area_sq = 0;     // sum(supported²) / sum(area²) — более строгая метрика
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

// Рассчитать центр масс паллеты
CenterOfMass calc_center_of_mass(const TestData& test_data, const Answer& answer);

// Рассчитать длину совпадающих рёбер между двумя коробками
// (коробки должны быть на разных уровнях по Z)
uint32_t calc_overlapping_edges(const Position& box1, const Position& box2);

// Проверка, совпадают ли два горизонтальных отрезка (частично или полностью)
// Возвращает длину совпадения
uint32_t segment_overlap_length(uint32_t a1, uint32_t a2, uint32_t b1, uint32_t b2);
