#pragma once

#include <vector>
#include <cstdint>
#include <istream>

struct Box {
    uint32_t sku = 0;
    uint32_t quantity = 0;
    uint32_t length = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t weight = 0;
    uint32_t strength = 0;
    uint32_t aisle = 0;
    uint32_t caustic = 0;
};

struct TestData {
    // TODO: or 1200x1000
    uint32_t length = 1200;
    uint32_t width = 800;
    std::vector<Box> boxes;
};

std::istream &operator>>(std::istream &input, TestData &test_data);
