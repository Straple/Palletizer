#pragma once

#include <solvers/solver.hpp>

class GreedySolver : public Solver {
public:

    explicit GreedySolver(TestData test_data);

    Answer solve(TimePoint end_time);
};
