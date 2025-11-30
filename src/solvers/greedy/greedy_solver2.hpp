#pragma once

#include <solvers/solver.hpp>

struct HeightRect {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t X = 0;
    uint32_t Y = 0;
    uint32_t h = 0;
};

class HeightHandler {
protected:
    std::vector<HeightRect> height_rects;

public:

    [[nodiscard]] uint32_t get_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const;

    void add_rect(HeightRect rect);

    template<typename foo_t>
    void iterate(foo_t&& foo){
        for(auto rect : height_rects){
            foo(rect);
        }
    }
};

class GreedySolver2 : public Solver {
protected:

public:

    explicit GreedySolver2(TestData test_data);

    Answer solve(TimePoint end_time);
};
