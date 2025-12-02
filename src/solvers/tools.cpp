#include <solvers/tools.hpp>

std::vector<BoxSize> get_available_boxes(const TestDataHeader &header, const Box &box) {
    std::vector<BoxSize> result;
    result.push_back({box.length, box.width, box.height, 0});
    if (header.can_swap_length_width) {
        result.push_back({box.width, box.length, box.height, 1});
    }
    if (header.can_swap_width_height) {
        result.push_back({box.length, box.height, box.width, 2});
    }
    if (header.can_swap_length_width && header.can_swap_width_height) {
        result.push_back({box.width, box.height, box.length, 3});
        result.push_back({box.height, box.length, box.width, 4});
        result.push_back({box.height, box.width, box.length, 5});
    }
    return result;
}
