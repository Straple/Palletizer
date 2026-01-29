#pragma once

#include <objects/metrics.hpp>
#include <solvers/solver.hpp>

struct BoxMeta {
    uint32_t box_id = 0;
    uint32_t rotation = -1;
    uint32_t position = 3;
};

struct MutableParams {
    // Weights for mutation selection (11 mutations)
    // 0: position_rotation, 1: reverse, 2: shuffle, 3: swap, 4: swap_adjacent,
    // 5: threshold, 6: group_by_sku, 7: move_bad_box, 8: sort_by_area,
    // 9: sort_by_height, 10: sort_by_weight
    std::vector<uint32_t> weights = {104, 159, 87, 26, 190, 113, 59, 162, 100, 100, 100};

    // Segment selection parameters (used by reverse, shuffle, sort mutations)
    double segment_small_probability = 0.5;
    double segment_small_relative_len = 0.1;

    // mutate_position_rotation parameters
    double position_vs_rotation_probability = 0.5;
};

std::tuple<Answer, Metrics, double> simulate(const TestData &test_data, const std::vector<BoxMeta> &order);

class LNSSolver : public Solver {
public:
    MutableParams mutable_params;
    
    explicit LNSSolver(TestData test_data);
    explicit LNSSolver(TestData test_data, MutableParams params);

    Answer solve(TimePoint end_time);
};
