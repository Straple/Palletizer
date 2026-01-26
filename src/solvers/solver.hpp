#pragma once

#include <objects/answer.hpp>
#include <objects/test_data.hpp>
#include <utils/time.hpp>

class Solver {
protected:
    TestData test_data;

public:
    explicit Solver(TestData test_data);

    virtual Answer solve(TimePoint end_time);
};
