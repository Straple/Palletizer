#pragma once

#include <objects/metrics.hpp>
#include <objects/test_data.hpp>
#include <utils/randomizer.hpp>

#include <cstdint>
#include <vector>

struct BoxMeta {
    uint32_t box_id = 0;
    uint32_t rotation = -1;
    uint32_t position = 3;
};

struct MutableParams {
    std::vector<uint32_t> weights = {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100};
};

struct SimulationResult {
    Answer answer;
    Metrics metrics;
    std::vector<double> per_box_support;

    double get_score(const TestDataHeader &header) const;

    uint32_t get_worst_flat_idx() const;

    std::pair<uint32_t, uint32_t> flat_to_pallet_local(uint32_t flat) const;
};

struct SimulationParams {
    std::vector<std::vector<BoxMeta>> order;
    double support_threshold = 0.0;

    void mutate(Randomizer &rnd, const TestData &test_data, const SimulationResult &last_result,
                const MutableParams &mp);
};

SimulationResult run_simulation(const TestData &test_data, const SimulationParams &params);

SimulationParams make_initial_simulation_params(const TestData &test_data);
