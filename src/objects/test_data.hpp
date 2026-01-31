#pragma once

#include <cstdint>
#include <istream>
#include <vector>

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

struct TestDataHeader {
    // TODO: or 1200x1000
    uint32_t length = 1200;
    uint32_t width = 800;
    bool can_swap_length_width = true;
    bool can_swap_width_height = true;
    uint32_t available_rotations = 6;
    double min_support_threshold = 0;

    double score_percolation_mult = 1.0;
    double score_min_support_ratio_mult = 1.0;
    double score_center_of_mass_z_mult = 10.0;
};

struct TestData {
    TestDataHeader header;
    std::vector<Box> boxes;
};

struct BoxSize {
    uint32_t length = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t rotation = 0;
};

// возвращает возможные коробки, полученные вращением
std::vector<BoxSize> get_available_boxes(const TestDataHeader &header, const Box &box);

std::istream &operator>>(std::istream &input, TestData &test_data);
