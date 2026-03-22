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

    uint32_t length = 0;
    uint32_t width = 0;

    // Высота паллеты
    uint32_t height = 0;
    // Объем паллеты
    uint64_t pallet_volume = 0;
    // boxes_volume / pallet_volume
    double percolation = 0;

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

    // Доля опоры нижней грани
    std::vector<double> box_support_ratio;

    // Ожидаемое число коробок минус размещённые
    uint32_t unable_to_put_boxes = 0;
};

struct Metrics {
    // Показатели по каждой паллете
    std::vector<PalletMetrics> pallet_metrics;

    PalletMetrics total;

    // Близость высот штабелей: 1 если все одинаковые; меньше при разбросе (среднее и СКО по высотам).
    double height_balance = 1.0;

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
