#pragma once

#include <objects/answer.hpp>
#include <objects/test_data.hpp>

#include <cstdint>
#include <vector>

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

// Метрики одной физической паллеты (индекс совпадает с answer.pallets).
struct PalletMetrics {
    // Декодер LNS: для каждой коробки в порядке укладки — доля площади нижней грани, опирающейся на опору снизу [0, 1].
    std::vector<double> footprint_support_ratios;

    // Число размещённых коробок на этой паллете.
    uint32_t boxes = 0;
    // Суммарный объём коробок на паллете (по каталогу размеров), мм³.
    uint64_t boxes_volume = 0;
    // Высота штабеля: максимум координаты верхней грани Z среди коробок на паллете.
    uint32_t height = 0;
    // Объём «слота» под паллету: length×width×height (length/width из заголовка теста), мм³.
    uint64_t pallet_volume = 0;

    // Сумма площадей опирающихся ячеек основания по всем коробкам (модель HeightHandler), усл. ед.².
    double supported_area = 0;
    // Сумма площадей нижних граней всех коробок на паллете (проекция XY), усл. ед.².
    double total_area = 0;
    // Минимум среди коробок не на полу (z>0): доля опорной площади основания [0, 1]; если только пол — остаётся 1.
    double min_support_ratio = 1.0;

    // Центр масс только коробок этой паллеты (взвешенный по весу SKU), мм.
    CenterOfMass center_of_mass;
    // Нормализация CoM: x,y относительно половины поддона; z относительно height этой паллеты.
    RelativeCenterOfMass relative_center_of_mass;
    // Суммарный вес коробок на паллете.
    uint64_t total_weight = 0;
};

// Итоговые метрики решения: по-паллетно + агрегаты для скоринга и отчётов.
struct Metrics {
    // Детализация по каждой физической паллете.
    std::vector<PalletMetrics> pallet_metrics;
    // Конкатенация footprint_support_ratios по паллетам 0,1,… — глобальный индекс коробки как в мутациях LNS.
    std::vector<double> box_footprint_support_ratios;

    // --- Агрегаты по всем паллетам (согласованы с суммой/минимумом по pallet_metrics, где применимо) ---

    // Всего размещённых коробок на всех паллетах.
    uint32_t boxes = 0;

    // Габариты поддона из теста (одинаковы для всех паллет), мм.
    uint32_t length = 0;
    uint32_t width = 0;

    // Максимум высот штабелей по паллетам (max по pallet_metrics.height).
    uint32_t height = 0;

    // Суммарный объём коробок по всем паллетам, мм³.
    uint64_t boxes_volume = 0;
    // Сумма объёмов слотов (Σ pallet_metrics.pallet_volume) — знаменатель для перколяции.
    uint64_t pallet_volume = 0;

    // Заполненность: boxes_volume / pallet_volume (насколько объём коробок заполняет суммарный «короб» под паллеты).
    double percolation = 0;

    // Равномерность высот: 1 при одинаковых height по паллетам; падает при разбросе (см. std/mean по высотам).
    double height_balance = 1.0;

    // Сумма supported_area по паллетам.
    double supported_area = 0;
    // Сумма total_area по паллетам.
    double total_area = 0;
    // Глобальный минимум опоры основания по коробкам не на полу; min по pallet_metrics.min_support_ratio.
    double min_support_ratio = 1.0;

    // Центр масс всех размещённых коробок (веса из каталога), мм.
    CenterOfMass center_of_mass;
    // Относительный CoM: x,y к половине поддона; z к глобальному height; для скоринга смещения.
    RelativeCenterOfMass relative_center_of_mass;
    // center_of_mass.z / score_normalization_height из заголовка — вклад в целевой скор LNS/ГА.
    double com_z_normalized = 0;
    // Суммарный вес размещённых коробок.
    uint64_t total_weight = 0;

    // Сколько коробок из заказа не поместились (expected − размещённые).
    uint32_t unable_to_put_boxes = 0;
    // Счётчик работы солвера (прогоны паллет); не заполняется calc_metrics, выставляется снаружи.
    uint64_t pallets_computed = 1;
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
