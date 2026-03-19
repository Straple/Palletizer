#pragma once

#include <cstdint>
#include <istream>
#include <string>
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
    bool can_swap_width_height = false; // true
    uint32_t available_rotations = 2; // 6

    double score_percolation_mult = 1;
    double score_min_support_ratio_mult = 2;
    double score_center_of_mass_z_mult = 0;
    double score_height_balance_mult = 0;
    uint32_t score_normalization_height = 2200;
};

struct TestData {
    TestDataHeader header;
    std::vector<Box> boxes;
    // How many pallets this instance should be packed into (>= 1).
    uint32_t pallet_count = 1;
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

// Concatenate boxes and pallet budgets; headers must match on packing parameters.
TestData operator+(const TestData &a, const TestData &b);

// Склеивает все *.csv из каталога (например multitests/3/) в один TestData через operator+.
TestData load_multitest_combined(const std::string &directory_path);
