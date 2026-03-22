#pragma once

#include <cstdint>
#include <istream>
#include <vector>

struct Position {
    uint32_t sku = 0;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t z = 0;
    uint32_t X = 0;
    uint32_t Y = 0;
    uint32_t Z = 0;
};

struct Answer {
    // Для каждой физической паллеты - список размещённых коробок
    std::vector<std::vector<Position>> pallets;
};

std::ostream &operator<<(std::ostream &output, const Answer &answer);
