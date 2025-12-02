#pragma once

#include <objects/test_data.hpp>

struct BoxSize {
    uint32_t length = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t rotation = 0;
};

// возвращает возможные коробки, полученные вращением
std::vector<BoxSize> get_available_boxes(const TestDataHeader &header, const Box &box);
