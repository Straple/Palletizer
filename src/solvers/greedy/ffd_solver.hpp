#pragma once

#include <solvers/solver.hpp>

// First-Fit Decreasing по объёму + тот же жадный размещатель, что GreedySolver
class FfdGreedySolver : public Solver {
public:
    explicit FfdGreedySolver(TestData test_data);

    Answer solve(TimePoint end_time);
};
