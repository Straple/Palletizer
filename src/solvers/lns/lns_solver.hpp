#pragma once

#include <solvers/solver.hpp>
#include <objects/metrics.hpp>

class LNSSolver : public Solver {
protected:

    struct HeightRect {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t X = 0;
        uint32_t Y = 0;
        uint32_t h = 0;
    };

    std::vector<uint32_t> order;

    std::tuple<Answer, Metrics, double> simulate() const;

public:

    explicit LNSSolver(TestData test_data);

    Answer solve(TimePoint end_time);
};
