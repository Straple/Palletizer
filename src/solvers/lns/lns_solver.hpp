#pragma once

#include <objects/metrics.hpp>
#include <solvers/solver.hpp>

struct BoxMeta {
    uint32_t box_id = 0;
    uint32_t rotation = -1;
    uint32_t position = 3;
};

std::tuple<Answer, Metrics, double> simulate(const TestData &test_data, const std::vector<BoxMeta> &order);

class LNSSolver : public Solver {
public:
    std::vector<uint32_t> mutation_weights;
    
    explicit LNSSolver(TestData test_data);
    explicit LNSSolver(TestData test_data, std::vector<uint32_t> weights);

    Answer solve(TimePoint end_time);
};
