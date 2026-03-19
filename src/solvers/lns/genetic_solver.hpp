#pragma once

#include <solvers/lns/pallet_simulation.hpp>
#include <solvers/solver.hpp>

class GeneticSolver : public Solver {
public:
    MutableParams mutable_params;

    explicit GeneticSolver(TestData test_data);
    explicit GeneticSolver(TestData test_data, MutableParams params);

    Answer solve(TimePoint end_time);
};
