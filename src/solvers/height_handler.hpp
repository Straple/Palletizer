#pragma once

#include <cstdint>
#include <vector>

struct HeightRect {
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t X = 0;
    uint32_t Y = 0;
    uint32_t h = 0;
};

// Хранит информацию о текущих высотах палеты
// позволяет быстро получать высоту и итерироваться по интересным нам точкам
class HeightHandler {
    std::vector<HeightRect> height_rects;

public:

    [[nodiscard]] uint32_t get(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const;

    void add_rect(HeightRect rect);

    template<typename foo_t>
    void iterate(foo_t&& foo){
        for(auto rect : height_rects){
            foo(rect);
        }
    }
};
