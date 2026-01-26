#include <objects/metrics.hpp>

#include <utils/assert.hpp>

#include <unordered_map>

Metrics calc_metrics(TestData test_data, Answer answer) {
    Metrics metrics;

    metrics.boxes = answer.positions.size();
    metrics.length = test_data.header.length;
    metrics.width = test_data.header.width;

    uint32_t expected_boxes = 0;
    std::unordered_map<uint32_t, uint32_t> sku_to_index;
    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        sku_to_index[test_data.boxes[i].sku] = i;
        expected_boxes += test_data.boxes[i].quantity;
    }
    ASSERT(expected_boxes == metrics.boxes, "invalid boxes num: got " + std::to_string(metrics.boxes) + " != expected " + std::to_string(expected_boxes));
    for (auto pos: answer.positions) {
        ASSERT(sku_to_index.contains(pos.sku), "sku does not contains");
        auto box = test_data.boxes[sku_to_index.at(pos.sku)];
        metrics.height = std::max(metrics.height, pos.Z);
        metrics.boxes_volume += box.length * static_cast<uint64_t>(box.width) * box.height;
        // TODO: validate box coordinates
    }
    metrics.pallet_volume = metrics.length * static_cast<uint64_t>(metrics.width) * metrics.height;
    metrics.percolation = static_cast<double>(metrics.boxes_volume) / metrics.pallet_volume;

    // validate collision boxes
    for (uint32_t i = 0; i < answer.positions.size(); i++) {
        for (uint32_t j = i + 1; j < answer.positions.size(); j++) {
            auto pos1 = answer.positions[i];
            auto pos2 = answer.positions[j];

            auto is_intersect = [&](uint32_t x, uint32_t X, uint32_t y, uint32_t Y) {
                return !(Y <= x || X <= y);
            };

            ASSERT(!(is_intersect(pos1.x, pos1.X, pos2.x, pos2.X) &&
                     is_intersect(pos1.y, pos1.Y, pos2.y, pos2.Y) &&
                     is_intersect(pos1.z, pos1.Z, pos2.z, pos2.Z)), "boxes intersects");
        }
    }
    return metrics;
}
