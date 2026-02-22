#pragma once

#include <objects/metrics.hpp>
#include <solvers/lns/lns_solver.hpp>
#include <solvers/solver.hpp>

class LNSSolver2 : public Solver {
public:
    MutableParams mutable_params;

    explicit LNSSolver2(TestData test_data);
    explicit LNSSolver2(TestData test_data, MutableParams params);

    Answer solve(TimePoint end_time);
};
