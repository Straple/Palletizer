#pragma once

#include <solvers/solver.hpp>

// Deepest Bottom Left Fill (DBLF)
class DBLFSolver : public Solver {
public:
    explicit DBLFSolver(TestData test_data);

    Answer solve(TimePoint end_time);
};
