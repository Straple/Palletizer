#pragma once

#include <objects/test_data.hpp>
#include <objects/answer.hpp>

#include <cstdint>

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

    // boxes_volume / pallete_volume
    double relative_volume = 0;
};

Metrics calc_metrics(TestData test_data, Answer answer);
