#include <solvers/lns/pallet_simulation.hpp>

#include <solvers/height_handler_rects.hpp>
#include <utils/assert.hpp>

#include <algorithm>
#include <cmath>
#include <tuple>

double SimulationResult::get_score(const TestDataHeader &header) const {
    if (metrics.unable_to_put_boxes) {
        return -1e9;
    }
    double score = 0;
    score += header.score_percolation_mult * metrics.percolation;
    score += header.score_min_support_ratio_mult * metrics.min_support_ratio;
    score += header.score_center_of_mass_z_mult * (1 - metrics.com_z_normalized);
    score += header.score_height_balance_mult * metrics.height_balance;
    return score;
}

uint32_t SimulationResult::get_worst_flat_idx() const {
    if (per_box_support.empty()) {
        return 0;
    }
    uint32_t worst_idx = 0;
    double worst_support = per_box_support[0];
    for (uint32_t i = 1; i < per_box_support.size(); i++) {
        if (per_box_support[i] < worst_support) {
            worst_support = per_box_support[i];
            worst_idx = i;
        }
    }
    return worst_idx;
}

std::pair<uint32_t, uint32_t> SimulationResult::flat_to_pallet_local(uint32_t flat) const {
    uint32_t off = 0;
    for (uint32_t p = 0; p < answer.pallets.size(); ++p) {
        uint32_t sz = static_cast<uint32_t>(answer.pallets[p].size());
        if (flat < off + sz) {
            return {p, flat - off};
        }
        off += sz;
    }
    return {0, 0};
}

namespace {

    std::tuple<uint32_t, uint32_t, uint32_t> get_random_segment(Randomizer &rnd, std::vector<std::vector<BoxMeta>> &order) {
        if (order.empty()) {
            return {0, 0, 0};
        }
        uint32_t n_p = static_cast<uint32_t>(order.size());
        for (uint32_t attempt = 0; attempt < 64; ++attempt) {
            uint32_t p = rnd.get(0, n_p - 1);
            if (order[p].size() < 2) {
                continue;
            }
            uint32_t l = rnd.get(0, static_cast<uint32_t>(order[p].size() - 1));
            uint32_t r = rnd.get(0, static_cast<uint32_t>(order[p].size() - 1));
            if (l > r) {
                std::swap(l, r);
            }
            r++;
            return {p, l, r};
        }
        for (uint32_t p = 0; p < n_p; ++p) {
            if (order[p].size() >= 2) {
                return {p, 0, static_cast<uint32_t>(order[p].size())};
            }
        }
        return {0, 0, 0};
    }

}// namespace

void SimulationParams::mutate(Randomizer &rnd, const TestData &test_data, const SimulationResult &last_result,
                              const MutableParams &mp) {
    uint32_t type = rnd.get(mp.weights);

    switch (type) {
        case 0: {
            auto [p, l, r] = get_random_segment(rnd, order);
            if (l >= r) {
                break;
            }
            if (rnd.get_d() < 0.5) {
                uint32_t new_position = rnd.get(0, 3);
                for (uint32_t i = l; i < r; i++) {
                    order[p][i].position = new_position;
                }
            } else {
                int32_t new_rotation = rnd.get(-1, static_cast<int32_t>(test_data.header.available_rotations) - 1);
                for (uint32_t i = l; i < r; i++) {
                    order[p][i].rotation = static_cast<uint32_t>(new_rotation);
                }
            }
            break;
        }
        case 1: {
            auto [p, l, r] = get_random_segment(rnd, order);
            if (l >= r) {
                break;
            }
            for (uint32_t i = l; i < r; i++) {
                if (rnd.get_d() < 0.5) {
                    order[p][i].position = rnd.get(0, 3);
                } else {
                    order[p][i].rotation = static_cast<uint32_t>(
                            rnd.get(-1, static_cast<int32_t>(test_data.header.available_rotations) - 1));
                }
            }
            break;
        }
        case 2: {
            auto [p, l, r] = get_random_segment(rnd, order);
            if (l >= r) {
                break;
            }
            std::reverse(order[p].begin() + static_cast<std::ptrdiff_t>(l),
                         order[p].begin() + static_cast<std::ptrdiff_t>(r));
            break;
        }
        case 3: {
            auto [p, l, r] = get_random_segment(rnd, order);
            if (l >= r) {
                break;
            }
            std::shuffle(order[p].begin() + static_cast<std::ptrdiff_t>(l),
                         order[p].begin() + static_cast<std::ptrdiff_t>(r), rnd.generator);
            break;
        }
        case 4: {
            if (order.empty()) {
                break;
            }
            uint32_t p1 = rnd.get(0, static_cast<uint32_t>(order.size() - 1));
            if (order[p1].empty()) {
                break;
            }
            uint32_t p2 = rnd.get(0, static_cast<uint32_t>(order.size() - 1));
            if (order[p2].empty()) {
                break;
            }
            uint32_t a = rnd.get(0, static_cast<uint32_t>(order[p1].size() - 1));
            uint32_t b = rnd.get(0, static_cast<uint32_t>(order[p2].size() - 1));
            std::swap(order[p1][a], order[p2][b]);
            break;
        }
        case 5: {
            for (uint32_t attempt = 0; attempt < 32; ++attempt) {
                if (order.empty()) {
                    break;
                }
                uint32_t p = rnd.get(0, static_cast<uint32_t>(order.size() - 1));
                if (order[p].size() < 2) {
                    continue;
                }
                uint32_t idx = rnd.get(0, static_cast<uint32_t>(order[p].size() - 2));
                std::swap(order[p][idx], order[p][idx + 1]);
                break;
            }
            break;
        }
        case 6: {
            auto [p, l, r] = get_random_segment(rnd, order);
            if (l >= r) {
                break;
            }
            std::stable_sort(order[p].begin() + static_cast<std::ptrdiff_t>(l),
                             order[p].begin() + static_cast<std::ptrdiff_t>(r),
                             [&](const BoxMeta &a, const BoxMeta &b) {
                                 auto &ba = test_data.boxes[a.box_id];
                                 auto &bb = test_data.boxes[b.box_id];
                                 return ba.length * ba.width > bb.length * bb.width;
                             });
            break;
        }
        case 7: {
            auto [p, l, r] = get_random_segment(rnd, order);
            if (l >= r) {
                break;
            }
            std::stable_sort(order[p].begin() + static_cast<std::ptrdiff_t>(l),
                             order[p].begin() + static_cast<std::ptrdiff_t>(r),
                             [&](const BoxMeta &a, const BoxMeta &b) {
                                 auto &ba = test_data.boxes[a.box_id];
                                 auto &bb = test_data.boxes[b.box_id];
                                 return (uint64_t) ba.length * ba.width * ba.height >
                                        (uint64_t) bb.length * bb.width * bb.height;
                             });
            break;
        }
        case 8: {
            auto [p, l, r] = get_random_segment(rnd, order);
            if (l >= r) {
                break;
            }
            std::stable_sort(order[p].begin() + static_cast<std::ptrdiff_t>(l),
                             order[p].begin() + static_cast<std::ptrdiff_t>(r),
                             [&](const BoxMeta &a, const BoxMeta &b) {
                                 return test_data.boxes[a.box_id].height > test_data.boxes[b.box_id].height;
                             });
            break;
        }
        case 9: {
            auto [p, l, r] = get_random_segment(rnd, order);
            if (l >= r) {
                break;
            }
            std::stable_sort(order[p].begin() + static_cast<std::ptrdiff_t>(l),
                             order[p].begin() + static_cast<std::ptrdiff_t>(r),
                             [&](const BoxMeta &a, const BoxMeta &b) {
                                 return test_data.boxes[a.box_id].weight > test_data.boxes[b.box_id].weight;
                             });
            break;
        }
        case 10:
            support_threshold = rnd.get_d(0, 1);
            break;
        case 11: {
            std::vector<BoxMeta> flat;
            std::vector<uint32_t> sizes;
            for (auto &row: order) {
                sizes.push_back(static_cast<uint32_t>(row.size()));
                for (auto &box: row) {
                    flat.push_back(box);
                }
            }
            if (flat.size() < 2) {
                break;
            }
            uint32_t random_idx = rnd.get(0, static_cast<uint32_t>(flat.size() - 1));
            uint32_t target_sku = test_data.boxes[flat[random_idx].box_id].sku;

            std::vector<BoxMeta> same_sku;
            std::vector<BoxMeta> others;
            for (auto &box: flat) {
                if (test_data.boxes[box.box_id].sku == target_sku) {
                    same_sku.push_back(box);
                } else {
                    others.push_back(box);
                }
            }
            if (others.empty()) {
                break;
            }
            uint32_t insert_pos = rnd.get(0, static_cast<uint32_t>(others.size()));
            std::vector<BoxMeta> new_flat;
            for (uint32_t i = 0; i < insert_pos; i++) {
                new_flat.push_back(others[i]);
            }
            for (auto &box: same_sku) {
                new_flat.push_back(box);
            }
            for (uint32_t i = insert_pos; i < others.size(); i++) {
                new_flat.push_back(others[i]);
            }
            uint32_t idx = 0;
            for (uint32_t p = 0; p < order.size(); ++p) {
                for (uint32_t k = 0; k < sizes[p]; ++k) {
                    order[p][k] = new_flat[idx++];
                }
            }
            break;
        }
        case 12: {
            uint32_t from_flat = last_result.get_worst_flat_idx();
            auto [from_p, from_i] = last_result.flat_to_pallet_local(from_flat);
            if (from_i == 0) {
                break;
            }
            uint32_t to_i = rnd.get(0, from_i - 1);
            BoxMeta box = order[from_p][from_i];
            order[from_p].erase(order[from_p].begin() + static_cast<std::ptrdiff_t>(from_i));
            order[from_p].insert(order[from_p].begin() + static_cast<std::ptrdiff_t>(to_i), box);
            break;
        }
    }
}

SimulationParams make_initial_simulation_params(const TestData &test_data) {
    SimulationParams params;
    const uint32_t pc = test_data.pallet_count;
    ASSERT(pc >= 1, "pallet_count");
    params.order.assign(pc, {});

    std::vector<BoxMeta> flat;
    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        for (uint32_t q = 0; q < test_data.boxes[i].quantity; q++) {
            flat.push_back(BoxMeta{i, static_cast<uint32_t>(-1), 0});
        }
    }
    const uint32_t n = static_cast<uint32_t>(flat.size());
    const uint32_t base = n / pc;
    const uint32_t rem = n % pc;
    uint32_t off = 0;
    for (uint32_t p = 0; p < pc; ++p) {
        const uint32_t sz = base + (p < rem ? 1u : 0u);
        for (uint32_t k = 0; k < sz; ++k) {
            params.order[p].push_back(flat[off++]);
        }
    }
    return params;
}

SimulationResult run_simulation(const TestData &test_data, const SimulationParams &params) {
    SimulationResult result;
    const uint32_t pallet_count = test_data.pallet_count;
    ASSERT(pallet_count >= 1, "pallet_count");
    result.answer.pallets.assign(pallet_count, {});

    auto run_pallet = [&](uint32_t pallet_idx, const std::vector<BoxMeta> &sequence) {
        HeightHandlerRects height_handler(test_data.header.length, test_data.header.width);

        auto calc_support_ratio = [&](uint32_t x, uint32_t y, uint32_t length, uint32_t width) -> double {
            uint32_t hh = height_handler.get_h(x, y, x + length - 1, y + width - 1);
            if (hh == 0) {
                return 1.0;
            }
            uint64_t supported = height_handler.get_area(x, y, x + length - 1, y + width - 1);
            uint64_t tot = static_cast<uint64_t>(length) * width;
            return tot > 0 ? static_cast<double>(supported) / tot : 0.0;
        };

        auto is_center_of_mass_supported = [&](uint32_t x, uint32_t y, uint32_t length, uint32_t width) -> bool {
            uint32_t hh = height_handler.get_h(x, y, x + length - 1, y + width - 1);
            if (hh == 0) {
                return true;
            }
            uint32_t com_x = x + length / 2;
            uint32_t com_y = y + width / 2;
            uint32_t h_at_com = height_handler.get_h(com_x, com_y, com_x, com_y);
            return h_at_com == hh;
        };

        auto get_score = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t box_height) {
            return height_handler.get_h(x, y, X, Y) + box_height;
        };

        auto get_position_dist = [&](uint32_t position, uint32_t x, uint32_t y, uint32_t length, uint32_t width,
                                     uint32_t /*height*/) {
            auto dist_func = [&](uint64_t px, uint64_t py) -> uint64_t {
                if (position == 0) {
                    return px * px + py * py;
                } else if (position == 1) {
                    return px * px + (test_data.header.width - py) * (test_data.header.width - py);
                } else if (position == 2) {
                    return (test_data.header.length - px) * (test_data.header.length - px) + py * py;
                } else {
                    return (test_data.header.length - px) * (test_data.header.length - px) +
                           (test_data.header.width - py) * (test_data.header.width - py);
                }
            };

            return std::min({dist_func(x, y),
                             dist_func(x + length - 1, y),
                             dist_func(x, y + width - 1),
                             dist_func(x + length - 1, y + width - 1)});
        };

        auto set_box = [&](BoxMeta box_meta) -> bool {
            auto box = test_data.boxes[box_meta.box_id];

            double best_score = 1e300;
            uint32_t best_x = static_cast<uint32_t>(-1);
            uint32_t best_y = static_cast<uint32_t>(-1);
            uint32_t best_length = static_cast<uint32_t>(-1);
            uint32_t best_width = static_cast<uint32_t>(-1);
            uint32_t best_height = static_cast<uint32_t>(-1);

            auto available_boxes = get_available_boxes(test_data.header, box);

            for (const auto &rotated_box: available_boxes) {
                if (box_meta.rotation != static_cast<uint32_t>(-1) && box_meta.rotation != rotated_box.rotation) {
                    continue;
                }

                auto dots = height_handler.get_dots(test_data.header, rotated_box);
                for (auto [x, y]: dots) {
                    if (!is_center_of_mass_supported(x, y, rotated_box.length, rotated_box.width)) {
                        continue;
                    }

                    double support = calc_support_ratio(x, y, rotated_box.length, rotated_box.width);
                    if (support < params.support_threshold) {
                        continue;
                    }

                    auto score =
                            get_score(x, y, x + rotated_box.length - 1, y + rotated_box.width - 1, rotated_box.height);

                    if (score < best_score ||
                        (score == best_score &&
                         get_position_dist(box_meta.position, x, y, rotated_box.length, rotated_box.width, rotated_box.height) <
                                 get_position_dist(box_meta.position, best_x, best_y, best_length, best_width, best_height))) {
                        best_score = score;
                        best_x = x;
                        best_y = y;
                        best_length = rotated_box.length;
                        best_width = rotated_box.width;
                        best_height = rotated_box.height;
                    }
                }
            }

            if (best_score > 1e100) {
                result.per_box_support.push_back(0);
                return false;
            }

            double best_support = calc_support_ratio(best_x, best_y, best_length, best_width);
            result.per_box_support.push_back(best_support);

            uint32_t h = height_handler.get_h(best_x, best_y, best_x + best_length - 1, best_y + best_width - 1);

            Position pos = {
                    box.sku,
                    best_x,
                    best_y,
                    h,
                    best_x + best_length,
                    best_y + best_width,
                    h + best_height,
            };
            result.answer.pallets[pallet_idx].push_back(pos);

            height_handler.add_rect(best_x, best_y, best_x + best_length - 1, best_y + best_width - 1, h + best_height);
            return true;
        };

        for (auto box_meta: sequence) {
            set_box(box_meta);
        }
    };

    ASSERT(params.order.size() == pallet_count, "order/pallet_count mismatch");
    for (uint32_t p = 0; p < pallet_count; ++p) {
        run_pallet(p, params.order[p]);
    }

    result.metrics = calc_metrics(test_data, result.answer);
    return result;
}
