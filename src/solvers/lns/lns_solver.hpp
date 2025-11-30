#pragma once

#include <solvers/solver.hpp>
#include <objects/metrics.hpp>

class LNSSolver : public Solver {
protected:

    struct BoxMeta {
        uint32_t box_id = 0;
        double h_weight = 1000;
        double x_weight = 1;
        double y_weight = 1;
        uint32_t k = 0;
    };

    std::vector<BoxMeta> order;

    std::tuple<Answer, Metrics, double> simulate() const;

public:

    explicit LNSSolver(TestData test_data);

    Answer solve(TimePoint end_time);
};
