#pragma once

#include <solvers/solver.hpp>

class GreedySolver : public Solver {
protected:

    // [x][y]
    std::vector<std::vector<uint32_t>> heights;

    uint32_t get_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const;

    void set_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h);

public:

    explicit GreedySolver(TestData test_data);

    Answer solve(TimePoint end_time);
};
