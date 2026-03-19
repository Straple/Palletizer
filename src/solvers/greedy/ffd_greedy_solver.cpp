#include <solvers/greedy/ffd_greedy_solver.hpp>

#include <solvers/height_handler_rects.hpp>
#include <utils/assert.hpp>

#include <tuple>
#include <vector>

FfdGreedySolver::FfdGreedySolver(TestData test_data) : Solver(std::move(test_data)) {
}

Answer FfdGreedySolver::solve(TimePoint /*end_time*/) {
    Answer answer;
    const uint32_t pc = test_data.pallet_count;
    ASSERT(pc >= 1, "pallet_count");
    answer.pallets.assign(pc, {});

    std::vector<uint32_t> instances;
    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        for (uint32_t q = 0; q < test_data.boxes[i].quantity; q++) {
            instances.push_back(i);
        }
    }
    std::stable_sort(instances.begin(), instances.end(), [&](uint32_t a, uint32_t b) {
        const auto &ba = test_data.boxes[a];
        const auto &bb = test_data.boxes[b];
        uint64_t va = static_cast<uint64_t>(ba.length) * ba.width * ba.height;
        uint64_t vb = static_cast<uint64_t>(bb.length) * bb.width * bb.height;
        return va > vb;
    });

    const uint32_t n = static_cast<uint32_t>(instances.size());
    const uint32_t base = n / pc;
    const uint32_t rem = n % pc;

    std::vector<std::vector<std::pair<uint32_t, uint32_t>>> orders(pc);
    uint32_t off = 0;
    for (uint32_t p = 0; p < pc; ++p) {
        const uint32_t sz = base + (p < rem ? 1u : 0u);
        for (uint32_t k = 0; k < sz; ++k) {
            uint32_t bid = instances[off++];
            if (!orders[p].empty() && orders[p].back().first == bid) {
                orders[p].back().second++;
            } else {
                orders[p].push_back({bid, 1});
            }
        }
    }

    for (uint32_t pallet_idx = 0; pallet_idx < pc; ++pallet_idx) {
        auto &order = orders[pallet_idx];
        HeightHandlerRects height_handler(test_data.header.length, test_data.header.width);

        auto is_center_of_mass_supported = [&](uint32_t x, uint32_t y, uint32_t length, uint32_t width) -> bool {
            uint32_t h = height_handler.get_h(x, y, x + length - 1, y + width - 1);
            if (h == 0) {
                return true;
            }
            uint32_t com_x = x + length / 2;
            uint32_t com_y = y + width / 2;
            uint32_t h_at_com = height_handler.get_h(com_x, com_y, com_x, com_y);
            return h_at_com == h;
        };

        while (!order.empty()) {
            uint32_t best_score = static_cast<uint32_t>(-1);
            uint32_t best_i = static_cast<uint32_t>(-1);
            uint32_t best_x = static_cast<uint32_t>(-1);
            uint32_t best_y = static_cast<uint32_t>(-1);
            uint32_t best_length = static_cast<uint32_t>(-1);
            uint32_t best_width = static_cast<uint32_t>(-1);
            uint32_t best_height = static_cast<uint32_t>(-1);
            {
                uint32_t i = 0;
                uint32_t box_id = order[i].first;
                auto box = test_data.boxes[box_id];

                auto get_score = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t box_height) {
                    return height_handler.get_h(x, y, X, Y) + box_height;
                };

                auto available_boxes = get_available_boxes(test_data.header, box);
                for (const auto &ab: available_boxes) {
                    auto dots = height_handler.get_dots(test_data.header, ab);
                    for (auto [x, y]: dots) {
                        if (!is_center_of_mass_supported(x, y, ab.length, ab.width)) {
                            continue;
                        }
                        uint32_t score = get_score(x, y, x + ab.length - 1, y + ab.width - 1, ab.height);
                        if (std::tie(score, x, y) < std::tie(best_score, best_x, best_y)) {
                            best_score = score;
                            best_i = i;
                            best_x = x;
                            best_y = y;
                            best_length = ab.length;
                            best_width = ab.width;
                            best_height = ab.height;
                        }
                    }
                }
            }
            ASSERT(best_i != static_cast<uint32_t>(-1), "unable to put box");
            auto box = test_data.boxes[order[best_i].first];

            uint32_t best_h = height_handler.get_h(best_x, best_y, best_x + best_length - 1, best_y + best_width - 1);
            Position pos = {
                    box.sku,
                    best_x,
                    best_y,
                    best_h,
                    best_x + best_length,
                    best_y + best_width,
                    best_h + best_height,
            };
            answer.pallets[pallet_idx].push_back(pos);
            height_handler.add_rect(best_x, best_y, best_x + best_length - 1, best_y + best_width - 1,
                                    best_h + best_height);

            order[best_i].second--;
            if (order[best_i].second == 0) {
                order.erase(order.begin() + static_cast<std::ptrdiff_t>(best_i));
            }
        }
    }

    pallets_computed = 1;
    return answer;
}
