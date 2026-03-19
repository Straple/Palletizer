#pragma once

#include <solvers/lns/pallet_simulation.hpp>
#include <solvers/solver.hpp>

class LNSSolver : public Solver {
public:
    MutableParams mutable_params;

    explicit LNSSolver(TestData test_data);
    explicit LNSSolver(TestData test_data, MutableParams params);

    Answer solve(TimePoint end_time);
};
