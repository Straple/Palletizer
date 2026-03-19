#pragma once

#include <solvers/solver.hpp>

// Deepest Bottom Left Fill (DBLF) — см. papers_general/ALGORITHMS.md и @karabulut.
class DblfSolver : public Solver {
public:
    explicit DblfSolver(TestData test_data);

    Answer solve(TimePoint end_time);
};
