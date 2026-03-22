#include <objects/metrics.hpp>

#include <solvers/height_handler_rects.hpp>
#include <utils/assert.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>

// Проверка, совпадают ли два отрезка на одной линии
// Возвращает длину совпадения
uint32_t segment_overlap_length(uint32_t a1, uint32_t a2, uint32_t b1, uint32_t b2) {
    // Убедимся что a1 < a2 и b1 < b2
    if (a1 > a2) std::swap(a1, a2);
    if (b1 > b2) std::swap(b1, b2);

    // Найдём пересечение отрезков [a1, a2] и [b1, b2]
    uint32_t overlap_start = std::max(a1, b1);
    uint32_t overlap_end = std::min(a2, b2);

    if (overlap_start < overlap_end) {
        return overlap_end - overlap_start;
    }
    return 0;
}

// Рассчитать длину совпадающих рёбер между двумя коробками
// Коробка 1 должна быть выше коробки 2 (box1.z == box2.Z)
uint32_t calc_overlapping_edges(const Position &box1, const Position &box2) {
    // Проверяем, что коробки соприкасаются по Z
    // box1 стоит на box2: нижняя грань box1 совпадает с верхней гранью box2
    if (box1.z != box2.Z) {
        return 0;
    }

    // Проверяем, пересекаются ли проекции на плоскость XY
    bool x_overlap = box1.x < box2.X && box2.x < box1.X;
    bool y_overlap = box1.y < box2.Y && box2.y < box1.Y;

    if (!x_overlap || !y_overlap) {
        return 0;// Коробки не пересекаются в плоскости XY
    }

    uint32_t total_overlap = 0;

    // Проверяем совпадение вертикальных рёбер (параллельных оси Y)
    // Рёбра box1: x = box1.x и x = box1.X
    // Рёбра box2: x = box2.x и x = box2.X

    // Если x-координаты рёбер совпадают, проверяем перекрытие по Y
    if (box1.x == box2.x) {
        total_overlap += segment_overlap_length(box1.y, box1.Y, box2.y, box2.Y);
    }
    if (box1.x == box2.X) {
        total_overlap += segment_overlap_length(box1.y, box1.Y, box2.y, box2.Y);
    }
    if (box1.X == box2.x) {
        total_overlap += segment_overlap_length(box1.y, box1.Y, box2.y, box2.Y);
    }
    if (box1.X == box2.X) {
        total_overlap += segment_overlap_length(box1.y, box1.Y, box2.y, box2.Y);
    }

    // Проверяем совпадение горизонтальных рёбер (параллельных оси X)
    // Рёбра box1: y = box1.y и y = box1.Y
    // Рёбра box2: y = box2.y и y = box2.Y

    if (box1.y == box2.y) {
        total_overlap += segment_overlap_length(box1.x, box1.X, box2.x, box2.X);
    }
    if (box1.y == box2.Y) {
        total_overlap += segment_overlap_length(box1.x, box1.X, box2.x, box2.X);
    }
    if (box1.Y == box2.y) {
        total_overlap += segment_overlap_length(box1.x, box1.X, box2.x, box2.X);
    }
    if (box1.Y == box2.Y) {
        total_overlap += segment_overlap_length(box1.x, box1.X, box2.x, box2.X);
    }

    return total_overlap;
}

CenterOfMass calc_center_of_mass(const TestData &test_data, const Answer &answer, uint64_t &total_weight) {
    CenterOfMass result;
    total_weight = 0;

    std::map<uint32_t, const Box *> sku_to_box;
    for (const auto &box: test_data.boxes) {
        sku_to_box[box.sku] = &box;
    }

    double weighted_x = 0, weighted_y = 0, weighted_z = 0;

    for (const auto &pallet: answer.pallets) {
        for (const auto &pos: pallet) {
            auto it = sku_to_box.find(pos.sku);
            ASSERT(it != sku_to_box.end(), "invalid SKU");
            double weight = static_cast<double>(it->second->weight);

            double center_x = (pos.x + pos.X) / 2.0;
            double center_y = (pos.y + pos.Y) / 2.0;
            double center_z = (pos.z + pos.Z) / 2.0;

            weighted_x += center_x * weight;
            weighted_y += center_y * weight;
            weighted_z += center_z * weight;
            total_weight += it->second->weight;
        }
    }

    if (total_weight > 0) {
        result.x = weighted_x / total_weight;
        result.y = weighted_y / total_weight;
        result.z = weighted_z / total_weight;
    }

    return result;
}

Metrics calc_metrics(const TestData &test_data, const Answer &answer) {
    Metrics metrics;

    const uint32_t n_pallets = static_cast<uint32_t>(answer.pallets.size());
    metrics.pallet_metrics.assign(n_pallets, PalletMetrics{});
    metrics.box_footprint_support_ratios.clear();

    metrics.length = test_data.header.length;
    metrics.width = test_data.header.width;

    uint32_t expected_boxes = 0;
    std::unordered_map<uint32_t, uint32_t> sku_to_index;
    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        sku_to_index[test_data.boxes[i].sku] = i;
        expected_boxes += test_data.boxes[i].quantity;
    }

    std::vector<uint32_t> pallet_max_z;
    pallet_max_z.reserve(n_pallets);

    for (uint32_t pi = 0; pi < n_pallets; pi++) {
        PalletMetrics &pm = metrics.pallet_metrics[pi];
        const auto &pallet = answer.pallets[pi];
        pm.boxes = static_cast<uint32_t>(pallet.size());
        metrics.boxes += pm.boxes;

        uint32_t mz = 0;
        for (const auto &pos: pallet) {
            ASSERT(sku_to_index.contains(pos.sku), "sku does not contains");
            const auto &box = test_data.boxes[sku_to_index.at(pos.sku)];
            uint64_t vol = box.length * static_cast<uint64_t>(box.width) * box.height;
            pm.boxes_volume += vol;
            metrics.boxes_volume += vol;
            mz = std::max(mz, pos.Z);
        }
        pm.height = mz;
        pallet_max_z.push_back(mz);
        pm.pallet_volume = metrics.length * static_cast<uint64_t>(metrics.width) * mz;
        metrics.pallet_volume += pm.pallet_volume;
    }

    for (uint32_t h: pallet_max_z) {
        metrics.height = std::max(metrics.height, h);
    }

    if (pallet_max_z.size() >= 2) {
        double sum = 0;
        for (uint32_t h: pallet_max_z) {
            sum += static_cast<double>(h);
        }
        double mean = sum / static_cast<double>(pallet_max_z.size());
        double var = 0;
        for (uint32_t h: pallet_max_z) {
            double d = static_cast<double>(h) - mean;
            var += d * d;
        }
        var /= static_cast<double>(pallet_max_z.size());
        double std_dev = std::sqrt(var);
        metrics.height_balance = (mean > 1e-9) ? std::max(0.0, 1.0 - std_dev / mean) : 0.0;
    } else {
        metrics.height_balance = 1.0;
    }

    if (metrics.pallet_volume > 0) {
        metrics.percolation = static_cast<double>(metrics.boxes_volume) / static_cast<double>(metrics.pallet_volume);
    } else {
        metrics.percolation = 0;
    }

    metrics.unable_to_put_boxes = expected_boxes - metrics.boxes;

    if (metrics.boxes == 0) {
        return metrics;
    }

    metrics.center_of_mass = calc_center_of_mass(test_data, answer, metrics.total_weight);

    metrics.relative_center_of_mass.x = std::abs(metrics.center_of_mass.x / metrics.length - 0.5);
    metrics.relative_center_of_mass.y = std::abs(metrics.center_of_mass.y / metrics.width - 0.5);
    if (metrics.height > 0) {
        metrics.relative_center_of_mass.z = metrics.center_of_mass.z / static_cast<double>(metrics.height);
    }
    metrics.com_z_normalized = metrics.center_of_mass.z / test_data.header.score_normalization_height;

    metrics.min_support_ratio = 1.0;
    metrics.supported_area = 0;
    metrics.total_area = 0;

    std::map<uint32_t, const Box *> sku_to_box;
    for (const auto &box: test_data.boxes) {
        sku_to_box[box.sku] = &box;
    }

    for (uint32_t pi = 0; pi < n_pallets; pi++) {
        PalletMetrics &pm = metrics.pallet_metrics[pi];
        const auto &pallet = answer.pallets[pi];
        HeightHandlerRects height_handler(test_data.header.length, test_data.header.width);

        double weighted_x = 0;
        double weighted_y = 0;
        double weighted_z = 0;
        uint64_t pallet_weight = 0;

        for (const auto &pos: pallet) {
            uint32_t w = pos.X - pos.x;
            uint32_t len = pos.Y - pos.y;
            uint64_t area = static_cast<uint64_t>(w) * len;

            pm.total_area += static_cast<double>(area);

            uint64_t supported_cells = height_handler.get_area(pos.x, pos.y, pos.X - 1, pos.Y - 1);
            pm.supported_area += static_cast<double>(supported_cells);

            if (pos.z > 0) {
                double box_support_ratio = (area > 0) ? static_cast<double>(supported_cells) / area : 0.0;
                pm.min_support_ratio = std::min(pm.min_support_ratio, box_support_ratio);
            }

            height_handler.add_rect(pos.x, pos.y, pos.X - 1, pos.Y - 1, pos.Z);

            auto it = sku_to_box.find(pos.sku);
            ASSERT(it != sku_to_box.end(), "invalid SKU");
            double wt = static_cast<double>(it->second->weight);
            double cx = (pos.x + pos.X) / 2.0;
            double cy = (pos.y + pos.Y) / 2.0;
            double cz = (pos.z + pos.Z) / 2.0;
            weighted_x += cx * wt;
            weighted_y += cy * wt;
            weighted_z += cz * wt;
            pallet_weight += it->second->weight;
        }

        metrics.total_area += pm.total_area;
        metrics.supported_area += pm.supported_area;
        metrics.min_support_ratio = std::min(metrics.min_support_ratio, pm.min_support_ratio);

        if (pallet_weight > 0) {
            double pw = static_cast<double>(pallet_weight);
            pm.center_of_mass.x = weighted_x / pw;
            pm.center_of_mass.y = weighted_y / pw;
            pm.center_of_mass.z = weighted_z / pw;
        }
        pm.total_weight = pallet_weight;

        pm.relative_center_of_mass.x = std::abs(pm.center_of_mass.x / metrics.length - 0.5);
        pm.relative_center_of_mass.y = std::abs(pm.center_of_mass.y / metrics.width - 0.5);
        if (pm.height > 0) {
            pm.relative_center_of_mass.z = pm.center_of_mass.z / static_cast<double>(pm.height);
        }
    }

    for (const auto &pallet: answer.pallets) {
        for (uint32_t i = 0; i < pallet.size(); i++) {
            for (uint32_t j = i + 1; j < pallet.size(); j++) {
                auto pos1 = pallet[i];
                auto pos2 = pallet[j];

                auto is_intersect = [&](uint32_t x, uint32_t X, uint32_t y, uint32_t Y) {
                    return !(Y <= x || X <= y);
                };

                ASSERT(!(is_intersect(pos1.x, pos1.X, pos2.x, pos2.X) &&
                         is_intersect(pos1.y, pos1.Y, pos2.y, pos2.Y) &&
                         is_intersect(pos1.z, pos1.Z, pos2.z, pos2.Z)),
                       "boxes intersects");
            }
        }
    }

    return metrics;
}
