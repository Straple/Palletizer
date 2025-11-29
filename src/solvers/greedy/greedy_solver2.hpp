#pragma once

#include <solvers/solver.hpp>

class GreedySolver2 : public Solver {
protected:

    struct HeightRect {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t X = 0;
        uint32_t Y = 0;
        uint32_t h = 0;
    };

    std::vector<HeightRect> height_rects;


    [[nodiscard]] uint32_t get_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const;

public:

    explicit GreedySolver2(TestData test_data);

    Answer solve(TimePoint end_time);
};
