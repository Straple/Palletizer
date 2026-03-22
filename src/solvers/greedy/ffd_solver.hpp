#pragma once

#include <solvers/solver.hpp>

// First-Fit Decreasing по объёму + тот же жадный размещатель, что GreedySolver
class FFDSolver : public Solver {
public:
    explicit FFDSolver(TestData test_data);

    Answer solve(TimePoint end_time);
};
