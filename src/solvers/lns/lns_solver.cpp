#include <solvers/lns/lns_solver.hpp>

#include <solvers/height_handler_rects.hpp>
#include <solvers/stability.hpp>
#include <utils/assert.hpp>
#include <utils/randomizer.hpp>

#include <map>

struct SimulateResult {
    Answer answer;
    Metrics metrics;
    double min_support_ratio = 0;
    uint32_t unable_to_put = 0;
    std::vector<double> per_box_support;

    double get_score() const {
        if (unable_to_put) {
            return -1e9;
        }
        return metrics.percolation + 0.3 * min_support_ratio;
    }

    uint32_t get_worst_box_idx() const {
        if (per_box_support.empty()) return 0;
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
};

struct SimulationParams {
    std::vector<BoxMeta> order;
    double support_threshold = 0.0;

    std::pair<uint32_t, uint32_t> get_random_segment(Randomizer &rnd,
                                                     double small_probability = 0.5,
                                                     double small_relative_len = 0.1) {
        if (order.size() < 2) return {0, order.size()};

        uint32_t l, r;
        if (rnd.get_d() < small_probability) {
            uint32_t max_len = std::max((uint32_t) 2, (uint32_t) (small_relative_len * order.size()));
            max_len = std::min(max_len, (uint32_t) order.size());
            uint32_t len = rnd.get(2, max_len);
            l = rnd.get(0, order.size() - len);
            r = l + len;
            ASSERT(l + 1 < r, "invalid segment");
        } else {
            l = rnd.get(0, order.size() - 1);
            r = rnd.get(0, order.size() - 1);
            if (l > r) std::swap(l, r);
            r++;
        }
        return {l, r};
    }

    void mutate_position_rotation(Randomizer &rnd, uint32_t available_rotations, const MutableParams &mp) {
        uint32_t i = rnd.get(0, order.size() - 1);
        if (rnd.get_d() < mp.position_vs_rotation_probability) {
            order[i].position = rnd.get(0, 3);
        } else {
            order[i].rotation = rnd.get(-1, available_rotations - 1);
        }
    }

    void mutate_reverse(Randomizer &rnd, const MutableParams &mp) {
        auto [l, r] = get_random_segment(rnd, mp.segment_small_probability, mp.segment_small_relative_len);
        std::reverse(order.begin() + l, order.begin() + r);
    }

    void mutate_shuffle(Randomizer &rnd, const MutableParams &mp) {
        auto [l, r] = get_random_segment(rnd, mp.segment_small_probability, mp.segment_small_relative_len);
        std::shuffle(order.begin() + l, order.begin() + r, rnd.generator);
    }

    void mutate_sort_by_area(Randomizer &rnd, const TestData &test_data, const MutableParams &mp) {
        auto [l, r] = get_random_segment(rnd, mp.segment_small_probability, mp.segment_small_relative_len);
        std::stable_sort(order.begin() + l, order.begin() + r,
                         [&](const BoxMeta &a, const BoxMeta &b) {
                             auto &ba = test_data.boxes[a.box_id];
                             auto &bb = test_data.boxes[b.box_id];
                             return ba.length * ba.width > bb.length * bb.width;
                         });
    }

    void mutate_sort_by_height(Randomizer &rnd, const TestData &test_data, const MutableParams &mp) {
        auto [l, r] = get_random_segment(rnd, mp.segment_small_probability, mp.segment_small_relative_len);
        std::stable_sort(order.begin() + l, order.begin() + r,
                         [&](const BoxMeta &a, const BoxMeta &b) {
                             return test_data.boxes[a.box_id].height > test_data.boxes[b.box_id].height;
                         });
    }

    void mutate_sort_by_weight(Randomizer &rnd, const TestData &test_data, const MutableParams &mp) {
        auto [l, r] = get_random_segment(rnd, mp.segment_small_probability, mp.segment_small_relative_len);
        std::stable_sort(order.begin() + l, order.begin() + r,
                         [&](const BoxMeta &a, const BoxMeta &b) {
                             return test_data.boxes[a.box_id].weight > test_data.boxes[b.box_id].weight;
                         });
    }

    void mutate_swap(Randomizer &rnd) {
        uint32_t a = rnd.get(0, order.size() - 1);
        uint32_t b = rnd.get(0, order.size() - 1);
        if (a != b) std::swap(order[a], order[b]);
    }

    void mutate_swap_adjacent(Randomizer &rnd) {
        if (order.size() >= 2) {
            uint32_t i = rnd.get(0, order.size() - 2);
            std::swap(order[i], order[i + 1]);
        }
    }

    void mutate_threshold(Randomizer &rnd) {
        support_threshold = rnd.get_d(0, 1);
    }

    void mutate_move_bad_box(Randomizer &rnd, const SimulateResult &last_result) {
        uint32_t from_idx = last_result.get_worst_box_idx();
        if (from_idx == 0) return;
        uint32_t to_idx = rnd.get(0, from_idx - 1);
        BoxMeta box = order[from_idx];
        order.erase(order.begin() + from_idx);
        order.insert(order.begin() + to_idx, box);
    }

    void mutate_group_by_sku(Randomizer &rnd, const TestData &test_data) {
        if (order.size() < 2) return;

        uint32_t random_idx = rnd.get(0, order.size() - 1);
        uint32_t target_sku = test_data.boxes[order[random_idx].box_id].sku;

        std::vector<BoxMeta> same_sku;
        std::vector<BoxMeta> others;
        for (auto &box: order) {
            if (test_data.boxes[box.box_id].sku == target_sku) {
                same_sku.push_back(box);
            } else {
                others.push_back(box);
            }
        }

        if (others.empty()) return;

        uint32_t insert_pos = rnd.get(0, others.size());
        order.clear();
        for (uint32_t i = 0; i < insert_pos; i++) {
            order.push_back(others[i]);
        }
        for (auto &box: same_sku) {
            order.push_back(box);
        }
        for (uint32_t i = insert_pos; i < others.size(); i++) {
            order.push_back(others[i]);
        }
    }

    void mutate(Randomizer &rnd, const TestData &test_data, const SimulateResult &last_result,
                const MutableParams &mp) {
        uint32_t type = rnd.get(mp.weights);

        switch (type) {
            case 0:
                mutate_position_rotation(rnd, test_data.header.available_rotations, mp);
                break;
            case 1:
                mutate_reverse(rnd, mp);
                break;
            case 2:
                mutate_shuffle(rnd, mp);
                break;
            case 3:
                mutate_swap(rnd);
                break;
            case 4:
                mutate_swap_adjacent(rnd);
                break;
            case 5:
                mutate_threshold(rnd);
                break;
            case 6:
                mutate_group_by_sku(rnd, test_data);
                break;
            case 7:
                mutate_move_bad_box(rnd, last_result);
                break;
            case 8:
                mutate_sort_by_area(rnd, test_data, mp);
                break;
            case 9:
                mutate_sort_by_height(rnd, test_data, mp);
                break;
            case 10:
                mutate_sort_by_weight(rnd, test_data, mp);
                break;
        }
    }
};

SimulateResult simulate(const TestData &test_data, const SimulationParams &params) {
    SimulateResult result;
    HeightHandlerRects height_handler(test_data.header.length, test_data.header.width);

    double min_support_seen = 1.0;

    for (auto box_meta: params.order) {
        auto box = test_data.boxes[box_meta.box_id];

        double best_score = 1e300;
        double best_support = 0;
        uint32_t best_x = -1;
        uint32_t best_y = -1;
        uint32_t best_length = -1;
        uint32_t best_width = -1;
        uint32_t best_height = -1;
        uint32_t best_rotation = -1;

        auto calc_support_ratio = [&](uint32_t x, uint32_t y, uint32_t length, uint32_t width) -> double {
            uint32_t h = height_handler.get_h(x, y, x + length - 1, y + width - 1);
            if (h == 0) return 1.0;
            uint64_t supported = height_handler.get_area(x, y, x + length - 1, y + width - 1);
            uint64_t total = static_cast<uint64_t>(length) * width;
            return total > 0 ? static_cast<double>(supported) / total : 0.0;
        };

        auto is_center_of_mass_supported = [&](uint32_t x, uint32_t y, uint32_t length, uint32_t width) -> bool {
            uint32_t h = height_handler.get_h(x, y, x + length - 1, y + width - 1);
            if (h == 0) return true;

            uint32_t com_x = x + length / 2;
            uint32_t com_y = y + width / 2;

            uint32_t h_at_com = height_handler.get_h(com_x, com_y, com_x, com_y);
            return h_at_com == h;
        };

        auto get_score = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t box_height) {
            return height_handler.get_h(x, y, X, Y) + box_height;
        };

        auto get_position_dist = [&](uint32_t x, uint32_t y, uint32_t length, uint32_t width, uint32_t height) {
            auto dist_func = [&](uint64_t px, uint64_t py) -> uint64_t {
                if (box_meta.position == 0) {
                    return px * px + py * py;
                } else if (box_meta.position == 1) {
                    return px * px + (test_data.header.width - py) * (test_data.header.width - py);
                } else if (box_meta.position == 2) {
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

        auto available_boxes = get_available_boxes(test_data.header, box);

        for (const auto &rotated_box: available_boxes) {
            if (box_meta.rotation != -1 && box_meta.rotation != rotated_box.rotation) {
                continue;
            }

            auto dots = height_handler.get_dots(test_data.header, rotated_box);

            for (auto [x, y]: dots) {
                if (!is_center_of_mass_supported(x, y, rotated_box.length, rotated_box.width)) continue;

                double support = calc_support_ratio(x, y, rotated_box.length, rotated_box.width);
                if (support < params.support_threshold) continue;

                auto score = get_score(x, y, x + rotated_box.length - 1, y + rotated_box.width - 1, rotated_box.height);

                if (score < best_score ||
                    (score == best_score && get_position_dist(x, y, rotated_box.length, rotated_box.width, rotated_box.height) <
                                                    get_position_dist(best_x, best_y, best_length, best_width, best_height))) {
                    best_score = score;
                    best_support = support;
                    best_x = x;
                    best_y = y;
                    best_length = rotated_box.length;
                    best_width = rotated_box.width;
                    best_height = rotated_box.height;
                    best_rotation = rotated_box.rotation;
                }
            }
        }

        if (best_score > 1e100) {
            result.unable_to_put++;
            result.per_box_support.push_back(0.0);
            continue;
        }

        min_support_seen = std::min(min_support_seen, best_support);
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
        result.answer.positions.push_back(pos);

        height_handler.add_rect(best_x, best_y, best_x + best_length - 1, best_y + best_width - 1, h + best_height);
    }
    result.metrics = calc_metrics(test_data, result.answer);
    result.min_support_ratio = min_support_seen;
    return result;
}

/*
{154, 40, 127, 13, 143, 110, 120, 51, 93, 130, 114, 105}
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.5901         0.7369         0.8793
Boxes                            18          116.8            465
Height                          427         2308.1           8253
Stability                    0.3806         0.7647         0.8451
Min support ratio            0.2352         0.6098         1.0000
Interlocking                 0.1549         0.2353         0.6194
Pallets computed                  9          276.8           6425
-----------------------------------------------------------------
Center of Mass:
  CoM X                    520.5307       581.8379       659.3443
  CoM Y                    329.4956       386.4755       424.8256
  CoM Z                    155.5019      1161.0020      4195.2254
Relative Center of Mass:
  CoM X rel                  0.0002         0.0192         0.0662
  CoM Y rel                  0.0002         0.0199         0.0881
  CoM Z rel                  0.3240         0.4971         0.5700
-----------------------------------------------------------------

Total tests:      436
Total time:       70.4081s
Avg time/test:    161 ms
===========================================================================


{20, 10, 20, 10, 20, 10, 15, 15, 15, 10, 10, 10}
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6809         0.7550         0.8782
Boxes                            18          116.8            465
Height                          343         2241.9           8450
Stability                    0.4139         0.7904         0.8596
Min support ratio            0.2381         0.4749         1.0000
Interlocking                 0.1404         0.2096         0.5861
Pallets computed                  9          285.1           5752
-----------------------------------------------------------------
Center of Mass:
  CoM X                    522.9119       585.1729       640.9133
  CoM Y                    350.7816       387.8627       440.2379
  CoM Z                    154.0760      1148.5858      4242.7479
Relative Center of Mass:
  CoM X rel                  0.0000         0.0169         0.0642
  CoM Y rel                  0.0001         0.0177         0.0615
  CoM Z rel                  0.3542         0.5058         0.5660
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3891s
Avg time/test:    161 ms
===========================================================================


{48, 189, 13, 120, 100, 140, 110, 90, 60, 1, 80, 249}
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.5889         0.7318         0.8794
Boxes                            18          116.8            465
Height                          343         2325.1           8355
Stability                    0.3661         0.7603         0.8500
Min support ratio            0.2334         0.6092         1.0000
Interlocking                 0.1500         0.2397         0.6339
Pallets computed                  9          275.2           6209
-----------------------------------------------------------------
Center of Mass:
  CoM X                    530.1208       584.7408       740.2443
  CoM Y                    346.8055       385.8213       419.1210
  CoM Z                    153.8393      1159.1164      4280.3664
Relative Center of Mass:
  CoM X rel                  0.0000         0.0174         0.1169
  CoM Y rel                  0.0001         0.0199         0.0665
  CoM Z rel                  0.3545         0.4922         0.5620
-----------------------------------------------------------------

Total tests:      436
Total time:       70.4036s
Avg time/test:    161 ms
===========================================================================


{211, 40, 19, 113, 90, 110, 131, 71, 109, 130, 66, 110}
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6224         0.7411         0.8771
Boxes                            18          116.8            465
Height                          342         2281.2           8112
Stability                    0.3923         0.7682         0.8653
Min support ratio            0.2352         0.5670         0.9922
Interlocking                 0.1347         0.2318         0.6077
Pallets computed                  9          276.2           5937
-----------------------------------------------------------------
Center of Mass:
  CoM X                    531.7485       582.0476       654.7976
  CoM Y                    346.8604       385.6336       427.8344
  CoM Z                    157.3268      1151.0990      4140.5116
Relative Center of Mass:
  CoM X rel                  0.0004         0.0183         0.0569
  CoM Y rel                  0.0000         0.0201         0.0664
  CoM Z rel                  0.3617         0.4981         0.5612
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3590s
Avg time/test:    161 ms
===========================================================================


 {140, 40, 90, 110, 90, 110, 120, 70, 110, 130, 80, 110}
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6459         0.7422         0.8782
Boxes                            18          116.8            465
Height                          342         2287.3           8112
Stability                    0.3521         0.7683         0.8653
Min support ratio            0.2352         0.5944         1.0000
Interlocking                 0.1347         0.2317         0.6479
Pallets computed                  9          275.3           5837
-----------------------------------------------------------------
Center of Mass:
  CoM X                    512.5594       581.9695       639.0343
  CoM Y                    354.3403       387.0639       433.4556
  CoM Z                    155.1935      1154.2291      4140.5116
Relative Center of Mass:
  CoM X rel                  0.0000         0.0183         0.0729
  CoM Y rel                  0.0001         0.0196         0.0571
  CoM Z rel                  0.3199         0.4981         0.5643
-----------------------------------------------------------------

Total tests:      436
Total time:       70.4033s
Avg time/test:    161 ms
===========================================================================



{104, 100, 87, 26, 190, 113, 59, 59, 162, 100, 100, 100}
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6483         0.7413         0.8771
Boxes                            18          116.8            465
Height                          342         2295.1           8322
Stability                    0.3753         0.7704         0.8747
Min support ratio            0.2352         0.5958         0.9959
Interlocking                 0.1253         0.2296         0.6247
Pallets computed                  9          278.1           6016
-----------------------------------------------------------------
Center of Mass:
  CoM X                    515.6637       584.1441       662.8473
  CoM Y                    336.5401       385.5339       445.0851
  CoM Z                    150.2756      1156.4067      4187.1479
Relative Center of Mass:
  CoM X rel                  0.0003         0.0175         0.0703
  CoM Y rel                  0.0001         0.0211         0.0793
  CoM Z rel                  0.3455         0.4982         0.5670
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3858s
Avg time/test:    161 ms
===========================================================================
*/
LNSSolver::LNSSolver(TestData test_data) : Solver(test_data) {
}

LNSSolver::LNSSolver(TestData test_data, MutableParams params)
    : Solver(test_data), mutable_params(std::move(params)) {
}

Answer LNSSolver::solve(TimePoint end_time) {
    Randomizer rnd;

    SimulationParams params;
    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        for (uint32_t q = 0; q < test_data.boxes[i].quantity; q++) {
            params.order.push_back({i, (uint32_t) -1, 0});
        }
    }

    pallets_computed = 1;
    SimulateResult best_result = simulate(test_data, params);
    SimulationParams best_params = params;

    while (get_now() < end_time) {
        SimulationParams new_params = params;
        new_params.mutate(rnd, test_data, best_result, mutable_params);

        SimulateResult new_result = simulate(test_data, new_params);

        if (new_result.get_score() > best_result.get_score()) {
            best_result = new_result;
            best_params = new_params;
            params = new_params;
        }
        pallets_computed++;
    }
    ASSERT(best_result.unable_to_put == 0, "unable to put some boxes: " + std::to_string(best_result.unable_to_put));
    return best_result.answer;
}
