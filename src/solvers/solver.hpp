#pragma once

#include <objects/test_data.hpp>
#include <objects/answer.hpp>
#include <utils/time.hpp>

class Solver{
    TestData test_data;
public:

    explicit Solver(TestData test_data);

    // поставим все коробки друг на друга
    Answer solve(TimePoint end_time);
};
