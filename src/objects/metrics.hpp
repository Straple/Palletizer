#pragma once

#include <objects/answer.hpp>
#include <objects/test_data.hpp>

#include <cstdint>

// Центр масс паллеты
struct CenterOfMass {
    double x = 0;           // Координата X центра масс (мм)
    double y = 0;           // Координата Y центра масс (мм)
    double z = 0;           // Координата Z центра масс (мм) - высота
};

// Относительный центр масс (нормализованный)
struct RelativeCenterOfMass {
    double x = 0;  // |center_of_mass.x / length - 0.5|
    double y = 0;  // |center_of_mass.y / width - 0.5|
    double z = 0;  // center_of_mass.z / height
};

// Структура для хранения ребра (для расчёта совпадающих рёбер)
struct Edge {
    uint32_t x1, y1, x2, y2;// Координаты начала и конца
    uint32_t z;             // Высота (нижняя грань коробки)
    bool is_horizontal;     // true = горизонтальное, false = вертикальное

    // Длина ребра
    [[nodiscard]] uint32_t length() const {
        if (is_horizontal) {
            return x2 - x1;
        } else {
            return y2 - y1;
        }
    }
};

struct Metrics {
    uint32_t boxes = 0;

    uint32_t length = 0;

    uint32_t width = 0;

    // суммарная высота паллета
    uint32_t height = 0;

    // суммарный объем коробок
    uint64_t boxes_volume = 0;

    // объем паллеты
    uint64_t pallet_volume = 0;

    // относительный объем = boxes_volume / pallet_volume
    double percolation = 0;

    // === Метрики устойчивости ===

    // Перевязка слоёв (Interlocking) — версия 1: совпадение рёбер
    // G = K_int × (L_SumPer − L_Sum)
    // double l_sum_per = 0;   // Суммарный периметр оснований всех коробок
    // double l_sum = 0;       // Суммарная длина совпадающих кусков периметров
    // double interlocking = 0;// Коэффициент перевязки (чем больше, тем лучше)

    // Опора площади — версия 2 (с HeightHandler)
    double supported_area = 0;// Площадь, которая опирается на что-то
    // double hanging_area = 0;  // Площадь, которая "висит" в воздухе
    double total_area = 0;    // Суммарная площадь оснований всех коробок

    // Метрики для отдельных коробок
    double min_support_ratio = 1.0;      // Минимальный % опоры среди всех коробок (0-1)
    // uint32_t unstable_boxes_count = 0;   // Количество коробок с опорой < 70%
    // uint32_t total_boxes_above_floor = 0;// Всего коробок не на полу

    // Центр тяжести
    CenterOfMass center_of_mass;
    RelativeCenterOfMass relative_center_of_mass;
    double com_z_normalized = 0;  // center_of_mass.z / score_normalization_height (для скора отжига)
    uint64_t total_weight = 0;    // Суммарный вес всех коробок

    // Нормализованные метрики (0-1)
    // double interlocking_ratio = 0;// l_sum / l_sum_per (чем меньше, тем лучше)
    // double stability = 0;         // 1 - interlocking_ratio
    // double stability_area = 0;    // supported_area / total_area
    // double stability_area_sq = 0; // sum(supported²) / sum(area²) — более строгая метрика

    // Метрики вычислений
    uint32_t unable_to_put_boxes = 0;  // Количество коробок, которые не удалось разместить
    uint64_t pallets_computed = 1;     // Количество вычисленных паллет
};

Metrics calc_metrics(const TestData &test_data, const Answer &answer);

// Рассчитать центр масс паллеты
CenterOfMass calc_center_of_mass(const TestData &test_data, const Answer &answer, uint64_t &total_weight);

// Рассчитать длину совпадающих рёбер между двумя коробками
// (коробки должны быть на разных уровнях по Z)
uint32_t calc_overlapping_edges(const Position &box1, const Position &box2);

// Проверка, совпадают ли два горизонтальных отрезка (частично или полностью)
// Возвращает длину совпадения
uint32_t segment_overlap_length(uint32_t a1, uint32_t a2, uint32_t b1, uint32_t b2);
