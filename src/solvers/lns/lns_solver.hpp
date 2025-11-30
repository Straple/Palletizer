#pragma once

#include <solvers/solver.hpp>
#include <objects/metrics.hpp>

struct BoxMeta {
    uint32_t box_id = 0;
    double h_weight = 1000;
    double x_weight = 1;
    double y_weight = 1;
    uint32_t k = 0;
};

std::tuple<Answer, Metrics, double> simulate(const TestData &test_data, const std::vector<BoxMeta> &order);

class LNSSolver : public Solver {
protected:

public:

    explicit LNSSolver(TestData test_data);

    Answer solve(TimePoint end_time);
};
