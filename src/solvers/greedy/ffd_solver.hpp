#pragma once

#include <solvers/solver.hpp>

// First-Fit Decreasing по объёму + тот же жадный размещатель, что GreedySolver.
// См. papers_general/ALGORITHMS.md — @Martello2000, @Lodi2002.
class FfdGreedySolver : public Solver {
public:
    explicit FfdGreedySolver(TestData test_data);

    Answer solve(TimePoint end_time);
};
