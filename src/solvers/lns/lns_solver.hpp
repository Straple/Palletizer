#pragma once

#include <objects/metrics.hpp>
#include <solvers/solver.hpp>

struct BoxMeta {
    uint32_t box_id = 0;
    uint32_t rotation = -1;
    uint32_t position = 3;
};

struct MutableParams {
    // Weights for mutation selection (13 mutations, grouped by similarity)
    // Position/Rotation: 0: position_rotation_fixed, 1: position_rotation_random
    // Order: 2: reverse, 3: shuffle, 4: swap, 5: swap_adjacent
    // Sort: 6: sort_by_area, 7: sort_by_volume, 8: sort_by_height, 9: sort_by_weight
    // Special: 10: threshold, 11: group_by_sku, 12: move_bad_box
    std::vector<uint32_t> weights = {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100};//{80, 100, 70, 130, 90, 60, 128, 110, 90, 70, 100, 150, 122};//{100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100};//{100, 100, 140, 0, 190, 60, 80, 70, 80, 130, 133, 97, 120};
};

std::tuple<Answer, Metrics, double> simulate(const TestData &test_data, const std::vector<BoxMeta> &order);

class LNSSolver : public Solver {
public:
    MutableParams mutable_params;

    explicit LNSSolver(TestData test_data);
    explicit LNSSolver(TestData test_data, MutableParams params);

    Answer solve(TimePoint end_time);
};
