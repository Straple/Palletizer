#include <objects/metrics.hpp>

#include <utils/assert.hpp>

#include <unordered_map>

Metrics calc_metrics(TestData test_data, Answer answer) {
    Metrics metrics;

    std::unordered_map<uint32_t, uint32_t> sku_to_index;
    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        sku_to_index[test_data.boxes[i].sku] = i;
    }
    for (auto pos: answer.positions) {
        ASSERT(sku_to_index.contains(pos.sku), "sku does not contains");
        auto box = test_data.boxes[sku_to_index.at(pos.sku)];
        metrics.height = std::max(metrics.height, pos.Z);
        metrics.boxes_volume += box.length * static_cast<uint64_t>(box.width) * box.height;
        // TODO: validate box coordinates
    }
    metrics.pallet_volume = test_data.length * static_cast<uint64_t>(test_data.width) * metrics.height;
    metrics.relative_volume = static_cast<double>(metrics.boxes_volume) / metrics.pallet_volume;
    // TODO: validate collision boxes
    return metrics;
}
