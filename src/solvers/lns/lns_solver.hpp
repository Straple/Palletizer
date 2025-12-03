#pragma once

#include <solvers/solver.hpp>
#include <objects/metrics.hpp>

struct BoxMeta {
    uint32_t box_id = 0;
    uint32_t rotation = -1;
    uint32_t position = 3;
};

std::tuple<Answer, Metrics, double> simulate(const TestData &test_data, const std::vector<BoxMeta> &order);

class LNSSolver : public Solver {
protected:

public:

    explicit LNSSolver(TestData test_data);

    Answer solve(TimePoint end_time);
};
