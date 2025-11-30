#pragma once

#include <solvers/solver.hpp>
#include <objects/metrics.hpp>

class GeneticSolver : public Solver {
protected:

public:
    explicit GeneticSolver(TestData test_data);

    Answer solve(TimePoint end_time);
};
