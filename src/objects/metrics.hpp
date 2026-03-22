#pragma once

#include <objects/answer.hpp>
#include <objects/test_data.hpp>

#include <cstdint>
#include <vector>

// Центр масс
struct CenterOfMass {
    double x = 0;
    double y = 0;
    double z = 0;
};

// Нормализованный центр масс
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

// Метрики одной паллеты
struct PalletMetrics {
    // Число размещённых коробок на этой паллете.
    uint32_t boxes = 0;
    // Суммарный объём коробок
    uint64_t boxes_volume = 0;
    // Высота паллеты
    uint32_t height = 0;
    // Объем паллеты
    uint64_t pallet_volume = 0;

    // Сумма площадей опирающихся ячеек оснований
    double supported_area = 0;
    // Сумма площадей нижних граней коробок
    double total_area = 0;
    // Минимум по относительным площадям коробок
    double min_support_ratio = 1.0;

    // Центр масс
    CenterOfMass center_of_mass;
    // Нормализованный центр масс
    RelativeCenterOfMass relative_center_of_mass;
    // Суммарный вес коробок
    uint64_t total_weight = 0;

    std::vector<double> footprint_support_ratios;
};

struct Metrics {
    // Показатели по каждой паллете
    std::vector<PalletMetrics> pallet_metrics;
    // Склейка footprint_support_ratios по паллетам подряд; индекс как в мутациях LNS.
    std::vector<double> box_footprint_support_ratios;

    // Сводка по всем паллетам (согласована с суммами и минимумами по pallet_metrics).

    // Всего размещённых коробок.
    uint32_t boxes = 0;

    // Габариты поддона из теста, мм (общие для всех паллет).
    uint32_t length = 0;
    uint32_t width = 0;

    // Максимум из полей height по паллетам.
    uint32_t height = 0;

    // Суммарный объём коробок, мм в кубе.
    uint64_t boxes_volume = 0;
    // Сумма объёмов слотов паллет (сумма pallet_metrics.pallet_volume), знаменатель перколяции.
    uint64_t pallet_volume = 0;

    // Отношение boxes_volume к pallet_volume: насколько объём коробок заполняет суммарный объём под паллеты.
    double percolation = 0;

    // Близость высот штабелей: 1 если все одинаковые; меньше при разбросе (среднее и СКО по высотам).
    double height_balance = 1.0;

    // Сумма supported_area по паллетам.
    double supported_area = 0;
    // Сумма total_area по паллетам.
    double total_area = 0;
    // Минимум min_support_ratio по паллетам для коробок не на полу.
    double min_support_ratio = 1.0;

    // Центр масс всех размещённых коробок, мм.
    CenterOfMass center_of_mass;
    // Относительный центр масс: x,y к половине поддона; z к глобальному height
    RelativeCenterOfMass relative_center_of_mass;
    // z центра масс / score_normalization_height из заголовка; вклад в целевой скор отжига и ГА
    double com_z_normalized = 0;
    // Суммарный вес размещённых коробок
    uint64_t total_weight = 0;

    // Ожидаемое число коробок минус размещённые
    uint32_t unable_to_put_boxes = 0;
    // Счётчик прогонов паллет у солвера; calc_metrics не трогает, задаётся снаружи
    uint64_t pallets_computed = 1;
};

Metrics calc_metrics(const TestData &test_data, const Answer &answer);

// Центр масс всех коробок из ответа
CenterOfMass calc_center_of_mass(const TestData &test_data, const Answer &answer, uint64_t &total_weight);

// Длина совпадения рёбер двух коробок в проекции XY (разные уровни по Z)
uint32_t calc_overlapping_edges(const Position &box1, const Position &box2);

// Длина пересечения двух отрезков на одной прямой
uint32_t segment_overlap_length(uint32_t a1, uint32_t a2, uint32_t b1, uint32_t b2);
