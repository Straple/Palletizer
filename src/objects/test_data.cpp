#include <objects/test_data.hpp>

#include <utils/assert.hpp>
#include <utils/tools.hpp>

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
    return input;
}
