#pragma once

#include <objects/metrics.hpp>
#include <solvers/solver.hpp>

class GeneticSolver : public Solver {
protected:
public:
    explicit GeneticSolver(TestData test_data);

    Answer solve(TimePoint end_time);
};
