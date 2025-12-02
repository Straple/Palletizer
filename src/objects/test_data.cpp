#include <objects/test_data.hpp>

#include <utils/assert.hpp>
#include <utils/tools.hpp>

#include <algorithm>

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

std::istream &operator>>(std::istream &input, TestData &test_data) {
    std::string line;
    ASSERT(std::getline(input, line), "unable to read header");
    ASSERT(line == "SKU,Quantity,Length,Width,Height,Weight,Strength,Aisle,Caustic", "invalid header");
    test_data = {};
    while (std::getline(input, line)) {
        auto data = split(line, ',');
        ASSERT(data.size() == 9, "invalid row");

        Box box = {
                parse<uint32_t>(data[0]),
                parse<uint32_t>(data[1]),
                parse<uint32_t>(data[2]),
                parse<uint32_t>(data[3]),
                parse<uint32_t>(data[4]),
                parse<uint32_t>(data[5]),
                parse<uint32_t>(data[6]),
                parse<uint32_t>(data[7]),
                parse<uint32_t>(data[8])
        };
        test_data.boxes.emplace_back(box);
    }
    auto get_area = [&](const Box &box) {
        auto available_boxes = get_available_boxes(test_data.header, box);
        uint64_t area = 0;
        for (auto box: available_boxes) {
            area = std::max(area, static_cast<uint64_t>(box.length) * box.width);
        }
        return area;
    };
    std::stable_sort(test_data.boxes.begin(), test_data.boxes.end(), [&](const Box &lhs, const Box &rhs) {
        return get_area(lhs) > get_area(rhs);
    });
    return input;
}
