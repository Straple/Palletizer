#include <solvers/lns/pallet_simulation.hpp>

#include <solvers/height_handler_rects.hpp>
#include <utils/assert.hpp>

#include <algorithm>
#include <cmath>
#include <tuple>

double Pallet::get_score(const TestDataHeader &header) const {
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

uint32_t Pallet::get_worst_flat_idx() const {
    if (metrics.box_footprint_support_ratios.empty()) {
        return 0;
    }
    uint32_t worst_idx = 0;
    double worst_support = metrics.box_footprint_support_ratios[0];
    for (uint32_t i = 1; i < metrics.box_footprint_support_ratios.size(); i++) {
        if (metrics.box_footprint_support_ratios[i] < worst_support) {
            worst_support = metrics.box_footprint_support_ratios[i];
            worst_idx = i;
        }
    }
    return worst_idx;
}

std::pair<uint32_t, uint32_t> Pallet::flat_to_pallet_local(uint32_t flat) const {
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

    void simulate_one_pallet(const TestData &test_data, double support_threshold, const std::vector<BoxMeta> &sequence,
                             std::vector<Position> &out_positions, std::vector<double> &out_support) {
        out_positions.clear();
        out_support.clear();
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
                    if (support < support_threshold) {
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
                out_support.push_back(0);
                return false;
            }

            double best_support = calc_support_ratio(best_x, best_y, best_length, best_width);
            out_support.push_back(best_support);

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
            out_positions.push_back(pos);

            height_handler.add_rect(best_x, best_y, best_x + best_length - 1, best_y + best_width - 1, h + best_height);
            return true;
        };

        for (auto box_meta: sequence) {
            set_box(box_meta);
        }
    }

}// namespace

GenomHandler::GenomHandler(const TestData &test_data) : test_data_(&test_data) {
    const uint32_t pc = test_data.pallet_count;
    ASSERT(pc >= 1, "pallet_count");
    order.assign(pc, {});
    pallet_.answer.pallets.assign(pc, {});
    pallet_.metrics.pallet_metrics.assign(pc, PalletMetrics{});
    pallet_.metrics.box_footprint_support_ratios.clear();

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
            order[p].push_back(flat[off++]);
        }
    }
}

void GenomHandler::mark_pallet_dirty(std::vector<bool> *pallet_dirty, uint32_t p) const {
    if (pallet_dirty && p < pallet_dirty->size()) {
        (*pallet_dirty)[p] = true;
    }
}

void GenomHandler::mark_all_pallets_dirty(std::vector<bool> *pallet_dirty) const {
    if (pallet_dirty) {
        pallet_dirty->assign(order.size(), true);
    }
}

void GenomHandler::mutate_segment_uniform_meta(Randomizer &rnd, std::vector<bool> *pallet_dirty) {
    auto [p, l, r] = get_random_segment(rnd, order);
    if (l >= r) {
        return;
    }
    if (rnd.get_d() < 0.5) {
        uint32_t new_position = rnd.get(0, 3);
        for (uint32_t i = l; i < r; i++) {
            order[p][i].position = new_position;
        }
    } else {
        int32_t new_rotation = rnd.get(-1, static_cast<int32_t>(test_data_->header.available_rotations) - 1);
        for (uint32_t i = l; i < r; i++) {
            order[p][i].rotation = static_cast<uint32_t>(new_rotation);
        }
    }
    mark_pallet_dirty(pallet_dirty, p);
}

void GenomHandler::mutate_segment_random_meta_per_box(Randomizer &rnd, std::vector<bool> *pallet_dirty) {
    auto [p, l, r] = get_random_segment(rnd, order);
    if (l >= r) {
        return;
    }
    for (uint32_t i = l; i < r; i++) {
        if (rnd.get_d() < 0.5) {
            order[p][i].position = rnd.get(0, 3);
        } else {
            order[p][i].rotation =
                    static_cast<uint32_t>(rnd.get(-1, static_cast<int32_t>(test_data_->header.available_rotations) - 1));
        }
    }
    mark_pallet_dirty(pallet_dirty, p);
}

void GenomHandler::mutate_segment_reverse(Randomizer &rnd, std::vector<bool> *pallet_dirty) {
    auto [p, l, r] = get_random_segment(rnd, order);
    if (l >= r) {
        return;
    }
    std::reverse(order[p].begin() + static_cast<std::ptrdiff_t>(l), order[p].begin() + static_cast<std::ptrdiff_t>(r));
    mark_pallet_dirty(pallet_dirty, p);
}

void GenomHandler::mutate_segment_shuffle(Randomizer &rnd, std::vector<bool> *pallet_dirty) {
    auto [p, l, r] = get_random_segment(rnd, order);
    if (l >= r) {
        return;
    }
    std::shuffle(order[p].begin() + static_cast<std::ptrdiff_t>(l), order[p].begin() + static_cast<std::ptrdiff_t>(r),
                 rnd.generator);
    mark_pallet_dirty(pallet_dirty, p);
}

void GenomHandler::mutate_swap_boxes_across_pallets(Randomizer &rnd, std::vector<bool> *pallet_dirty) {
    if (order.empty()) {
        return;
    }
    uint32_t p1 = rnd.get(0, static_cast<uint32_t>(order.size() - 1));
    if (order[p1].empty()) {
        return;
    }
    uint32_t p2 = rnd.get(0, static_cast<uint32_t>(order.size() - 1));
    if (order[p2].empty()) {
        return;
    }
    uint32_t a = rnd.get(0, static_cast<uint32_t>(order[p1].size() - 1));
    uint32_t b = rnd.get(0, static_cast<uint32_t>(order[p2].size() - 1));
    std::swap(order[p1][a], order[p2][b]);
    mark_pallet_dirty(pallet_dirty, p1);
    mark_pallet_dirty(pallet_dirty, p2);
}

void GenomHandler::mutate_swap_adjacent_pair(Randomizer &rnd, std::vector<bool> *pallet_dirty) {
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
        mark_pallet_dirty(pallet_dirty, p);
        break;
    }
}

void GenomHandler::mutate_segment_sort_by_footprint(Randomizer &rnd, std::vector<bool> *pallet_dirty) {
    auto [p, l, r] = get_random_segment(rnd, order);
    if (l >= r) {
        return;
    }
    std::stable_sort(order[p].begin() + static_cast<std::ptrdiff_t>(l), order[p].begin() + static_cast<std::ptrdiff_t>(r),
                     [&](const BoxMeta &a, const BoxMeta &b) {
                         auto &ba = test_data_->boxes[a.box_id];
                         auto &bb = test_data_->boxes[b.box_id];
                         return ba.length * ba.width > bb.length * bb.width;
                     });
    mark_pallet_dirty(pallet_dirty, p);
}

void GenomHandler::mutate_segment_sort_by_volume(Randomizer &rnd, std::vector<bool> *pallet_dirty) {
    auto [p, l, r] = get_random_segment(rnd, order);
    if (l >= r) {
        return;
    }
    std::stable_sort(order[p].begin() + static_cast<std::ptrdiff_t>(l), order[p].begin() + static_cast<std::ptrdiff_t>(r),
                     [&](const BoxMeta &a, const BoxMeta &b) {
                         auto &ba = test_data_->boxes[a.box_id];
                         auto &bb = test_data_->boxes[b.box_id];
                         return (uint64_t) ba.length * ba.width * ba.height > (uint64_t) bb.length * bb.width * bb.height;
                     });
    mark_pallet_dirty(pallet_dirty, p);
}

void GenomHandler::mutate_segment_sort_by_height(Randomizer &rnd, std::vector<bool> *pallet_dirty) {
    auto [p, l, r] = get_random_segment(rnd, order);
    if (l >= r) {
        return;
    }
    std::stable_sort(order[p].begin() + static_cast<std::ptrdiff_t>(l), order[p].begin() + static_cast<std::ptrdiff_t>(r),
                     [&](const BoxMeta &a, const BoxMeta &b) {
                         return test_data_->boxes[a.box_id].height > test_data_->boxes[b.box_id].height;
                     });
    mark_pallet_dirty(pallet_dirty, p);
}

void GenomHandler::mutate_segment_sort_by_weight(Randomizer &rnd, std::vector<bool> *pallet_dirty) {
    auto [p, l, r] = get_random_segment(rnd, order);
    if (l >= r) {
        return;
    }
    std::stable_sort(order[p].begin() + static_cast<std::ptrdiff_t>(l), order[p].begin() + static_cast<std::ptrdiff_t>(r),
                     [&](const BoxMeta &a, const BoxMeta &b) {
                         return test_data_->boxes[a.box_id].weight > test_data_->boxes[b.box_id].weight;
                     });
    mark_pallet_dirty(pallet_dirty, p);
}

void GenomHandler::mutate_resample_support_threshold(Randomizer &rnd, std::vector<bool> *pallet_dirty) {
    support_threshold = rnd.get_d(0, 1);
    mark_all_pallets_dirty(pallet_dirty);
}

void GenomHandler::mutate_cluster_one_sku(Randomizer &rnd, std::vector<bool> *pallet_dirty) {
    std::vector<BoxMeta> flat;
    std::vector<uint32_t> sizes;
    for (auto &row: order) {
        sizes.push_back(static_cast<uint32_t>(row.size()));
        for (auto &box: row) {
            flat.push_back(box);
        }
    }
    if (flat.size() < 2) {
        return;
    }
    uint32_t random_idx = rnd.get(0, static_cast<uint32_t>(flat.size() - 1));
    uint32_t target_sku = test_data_->boxes[flat[random_idx].box_id].sku;

    std::vector<BoxMeta> same_sku;
    std::vector<BoxMeta> others;
    for (auto &box: flat) {
        if (test_data_->boxes[box.box_id].sku == target_sku) {
            same_sku.push_back(box);
        } else {
            others.push_back(box);
        }
    }
    if (others.empty()) {
        return;
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
    mark_all_pallets_dirty(pallet_dirty);
}

void GenomHandler::mutate_move_low_support_box_earlier(Randomizer &rnd, const Pallet &last_pallet,
                                                       std::vector<bool> *pallet_dirty) {
    uint32_t from_flat = last_pallet.get_worst_flat_idx();
    auto [from_p, from_i] = last_pallet.flat_to_pallet_local(from_flat);
    if (from_i == 0) {
        return;
    }
    uint32_t to_i = rnd.get(0, from_i - 1);
    BoxMeta box = order[from_p][from_i];
    order[from_p].erase(order[from_p].begin() + static_cast<std::ptrdiff_t>(from_i));
    order[from_p].insert(order[from_p].begin() + static_cast<std::ptrdiff_t>(to_i), box);
    mark_pallet_dirty(pallet_dirty, from_p);
}

void GenomHandler::mutate(Randomizer &rnd, const Pallet &last_pallet, const MutableParams &mp,
                          std::vector<bool> *pallet_dirty) {
    if (pallet_dirty) {
        pallet_dirty->assign(order.size(), false);
    }
    uint32_t type = rnd.get(mp.weights);
    switch (type) {
        case 0:
            mutate_segment_uniform_meta(rnd, pallet_dirty);
            break;
        case 1:
            mutate_segment_random_meta_per_box(rnd, pallet_dirty);
            break;
        case 2:
            mutate_segment_reverse(rnd, pallet_dirty);
            break;
        case 3:
            mutate_segment_shuffle(rnd, pallet_dirty);
            break;
        case 4:
            mutate_swap_boxes_across_pallets(rnd, pallet_dirty);
            break;
        case 5:
            mutate_swap_adjacent_pair(rnd, pallet_dirty);
            break;
        case 6:
            mutate_segment_sort_by_footprint(rnd, pallet_dirty);
            break;
        case 7:
            mutate_segment_sort_by_volume(rnd, pallet_dirty);
            break;
        case 8:
            mutate_segment_sort_by_height(rnd, pallet_dirty);
            break;
        case 9:
            mutate_segment_sort_by_weight(rnd, pallet_dirty);
            break;
        case 10:
            mutate_resample_support_threshold(rnd, pallet_dirty);
            break;
        case 11:
            mutate_cluster_one_sku(rnd, pallet_dirty);
            break;
        case 12:
            mutate_move_low_support_box_earlier(rnd, last_pallet, pallet_dirty);
            break;
    }
}

void GenomHandler::run_single_pallet(uint32_t pallet_idx) {
    ASSERT(pallet_idx < test_data_->pallet_count, "pallet_idx");
    ASSERT(pallet_idx < order.size(), "order");
    simulate_one_pallet(*test_data_, support_threshold, order[pallet_idx], pallet_.answer.pallets[pallet_idx],
                        pallet_.metrics.pallet_metrics[pallet_idx].footprint_support_ratios);
}

void GenomHandler::flatten_support_and_metrics() {
    std::vector<std::vector<double>> fp_saved;
    fp_saved.reserve(pallet_.metrics.pallet_metrics.size());
    for (auto &pm: pallet_.metrics.pallet_metrics) {
        fp_saved.push_back(std::move(pm.footprint_support_ratios));
    }
    Metrics agg = calc_metrics(*test_data_, pallet_.answer);
    ASSERT(agg.pallet_metrics.size() == fp_saved.size(), "pallet_metrics size");
    for (size_t p = 0; p < fp_saved.size(); ++p) {
        agg.pallet_metrics[p].footprint_support_ratios = std::move(fp_saved[p]);
    }
    agg.box_footprint_support_ratios.clear();
    for (const auto &pm: agg.pallet_metrics) {
        for (double v: pm.footprint_support_ratios) {
            agg.box_footprint_support_ratios.push_back(v);
        }
    }
    pallet_.metrics = std::move(agg);
}

void GenomHandler::rebuild_all() {
    const uint32_t pc = test_data_->pallet_count;
    ASSERT(order.size() == pc, "order/pallet_count mismatch");
    pallet_.answer.pallets.assign(pc, {});
    pallet_.metrics.pallet_metrics.assign(pc, PalletMetrics{});
    pallet_.metrics.box_footprint_support_ratios.clear();
    for (uint32_t p = 0; p < pc; ++p) {
        run_single_pallet(p);
    }
    flatten_support_and_metrics();
}

void GenomHandler::recompute_with_mask(std::vector<bool> pallet_dirty) {
    ASSERT(pallet_dirty.size() == order.size(), "dirty mask size");
    bool any = false;
    for (uint32_t p = 0; p < pallet_dirty.size(); ++p) {
        if (pallet_dirty[p]) {
            run_single_pallet(p);
            any = true;
        }
    }
    if (any) {
        flatten_support_and_metrics();
    }
}
