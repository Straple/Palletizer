#include <solvers/lns/lns_solver.hpp>

#include <solvers/height_handler.hpp>
#include <utils/assert.hpp>
#include <utils/randomizer.hpp>

#include <map>

bool print_debug = false;

std::tuple<Answer, Metrics, double> simulate(const TestData &test_data, const std::vector<BoxMeta> &order) {
    Answer answer;
    HeightHandler height_handler;
    height_handler.add_rect(HeightRect{0, 0, test_data.header.length - 1, test_data.header.width - 1, 0});

    for (auto box_meta: order) {
        uint32_t best_score = -1;

        // (x, y, length, width, height, rotate)
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>> items;
        auto box = test_data.boxes[box_meta.box_id];

        auto get_score = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t box_height) {
            return height_handler.get(x, y, X, Y);
        };

        auto available_boxes = get_available_boxes(test_data.header, box);
        for (const auto &box: available_boxes) {
            if (box_meta.rotation != -1 && box_meta.rotation != box.rotation) {
                continue;
            }
            auto dots = height_handler.get_dots(test_data.header, box);
            for (auto [x, y]: dots) {
                uint32_t score = get_score(x, y, x + box.length - 1, y + box.width - 1, box.height);
                if (score < best_score) {
                    best_score = score;
                    items.clear();
                }
                if (score == best_score) {
                    items.emplace_back(x, y, box.length, box.width, box.height, box.rotation);
                }
            }
        }

        ASSERT(!items.empty(), "unable to put box");

        auto get_position_dist = [&](uint32_t x, uint32_t y, uint32_t length, uint32_t width, uint32_t height) {

            auto get_position_dist = [&](uint64_t x, uint64_t y) -> uint64_t {
                if (box_meta.position == 0) {
                    return x * x
                           + y * y;
                } else if (box_meta.position == 1) {
                    return x * x
                           + (test_data.header.width - y) * (test_data.header.width - y);
                } else if (box_meta.position == 2) {
                    return (test_data.header.length - x) * (test_data.header.length - x)
                           + y * y;
                } else if (box_meta.position == 3) {
                    return (test_data.header.length - x) * (test_data.header.length - x)
                           + (test_data.header.width - y) * (test_data.header.width - y);
                } else {
                    FAILED_ASSERT("invalid position");
                }
            };

            uint64_t dist = std::min(std::min(get_position_dist(x, y),
                                              get_position_dist(x + length - 1, y)),
                                     std::min(get_position_dist(x, y + width - 1),
                                              get_position_dist(x + length - 1, y + width - 1)));

            if (print_debug) {
                //std::cout << "dist from (" << x << ',' << y << "): " << dist << '\n';
            }
            return dist;
        };

        std::stable_sort(items.begin(), items.end());
        auto best_item = items[0];
        for (auto item: items) {
            auto [best_x, best_y, best_length, best_width, best_height, best_rotation] = best_item;
            auto [x, y, length, width, height, rotation] = item;

            if (get_position_dist(x, y, length, width, height) <
                get_position_dist(best_x, best_y, best_length, best_width, best_height)) {
                best_item = item;
            }
        }
        auto [x, y, length, width, height, rotation] = best_item;
        if (print_debug) {
            //std::cout << "best item: " << x << ' ' << y << std::endl;
        }

        uint32_t h = height_handler.get(x, y, x + length - 1, y + width - 1);

        Position pos = {
                box.sku,
                x,
                y,
                h,
                x + length,
                y + width,
                h + height,
        };
        answer.positions.push_back(pos);

        if (print_debug) {
            //std::cout << "pos: " << x << ' ' << y << ' ' << x + length << ' ' << y + width << std::endl;
        }

        height_handler.add_rect(HeightRect{
                x, y, x + length - 1, y + width - 1, h + height});
    }
    auto metrics = calc_metrics(test_data, answer);
    double score = metrics.height;
    if (print_debug) {
        //std::cout << "score: " << metrics.height << std::endl;
    }
    return {answer, metrics, score};
}

LNSSolver::LNSSolver(TestData test_data) : Solver(test_data) {

}

Answer LNSSolver::solve(TimePoint end_time) {
    std::vector<BoxMeta> order;
    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        for (uint32_t q = 0; q < test_data.boxes[i].quantity; q++) {
            order.push_back({i});
        }
    }

    auto [best_answer, best_metrics, best_score] = simulate(test_data, order);
    //std::cout << best_score << "->";
    //std::cout.flush();

    // Answer real_answer = best_answer;
    // auto real_score = best_score;

    Randomizer rnd;

    double AVAILABLE_UP = 0;

    Timer last_update;

    uint32_t cnt = 0;
    double min_score = 1e300;
    double temp = 1;
    while (get_now() < end_time) {
        AVAILABLE_UP = 10 * temp;

        auto old_order = order;
        // do
        {
            uint32_t type = rnd.get({20, 10, 20, 10, 20});
            if (type == 0) {
                uint32_t i = rnd.get(0, order.size() - 1);
                if (rnd.get_d() < 0.5) {
                    order[i].position = rnd.get(0, 3);
                } else {
                    order[i].rotation = rnd.get(-1, test_data.header.available_rotations - 1);
                }
            } else if (type == 1) {
                uint32_t l = rnd.get(0, order.size() - 1);
                uint32_t r = rnd.get(0, order.size() - 1);
                if (l > r) {
                    std::swap(l, r);
                }
                std::reverse(order.begin() + l, order.begin() + r);
            } else if (type == 2) {
                uint32_t l = rnd.get(0, order.size() - 1);
                uint32_t r = rnd.get(0, order.size() - 1);
                if (l > r) {
                    std::swap(l, r);
                }
                std::shuffle(order.begin() + l, order.begin() + r, rnd.generator);
            } else if (type == 3) {
                uint32_t a = rnd.get(0, order.size() - 1);
                uint32_t b = rnd.get(0, order.size() - 1);
                if (a == b) {
                    continue;
                }
                std::swap(order[a], order[b]);
            } else if (type == 4) {
                uint32_t i = rnd.get(0, order.size() - 2);
                std::swap(order[i], order[i + 1]);
            } else {
                ASSERT(false, "invalid type");
            }
        }

        // check
        {
            auto [answer, metrics, score] = simulate(test_data, order);
            if (score < best_score + AVAILABLE_UP) {
                if (score + 1e-6 < best_score) {
                    last_update.reset();
                    //std::cout << score << "->";
                    //std::cout.flush();
                }
                /*if (score < real_score) {
                    real_score = score;
                    real_answer = answer;
                }*/
                best_answer = answer;
                best_metrics = metrics;
                best_score = score;
            } else {
                order = std::move(old_order);
            }
        }
        cnt++;
        min_score = std::min(min_score, best_score);
        temp *= 0.999;

        /*if (last_update.get_ms() > 1'000) {
            //std::cout << "RESET->";
            temp = 1;
            last_update.reset();
            std::shuffle(order.begin(), order.end(), rnd.generator);
            for (auto &box_meta: order) {
                box_meta.position = rnd.get(-1, 3);
                box_meta.rotation = rnd.get(-1, test_data.header.available_rotations - 1);
            }
            std::tie(best_answer, best_metrics, best_score) = simulate(test_data, order);
        }*/
    }
    // auto real_metrics = calc_metrics(test_data, real_answer);
    //std::cout << std::endl;
    // //std::cout << cnt << ' ' << real_metrics.relative_volume << ' ' << real_metrics.height << std::endl;
    //std::cout << cnt << ' ' << best_metrics.relative_volume << ' ' << best_metrics.height << std::endl;
    //std::cout << min_score << std::endl;

    print_debug = true;
    simulate(test_data, order);

    return best_answer;
}

/*
Relative volume: 0.755165avg 0.672775min 0.86141max
Time: 71.2118s
*/

/*
1295->1295->1295->1294->1295->1299->1295->1299->1295->1295->1295->1294->1294->1289->1233->1223->1221->1220->1217->
85367 0.808141 1217
1217
Height: 1217
Relative volume: 0.808141


1594->1466->1466->1400->1369->1368->1368->1300->1298->1295->1300->1300->1292->1281->1277->1264->1244->1243->1227->1226->1224->1222->1221->1217->1205->1185->1180->1178->
107606 0.834896 1178
1178
Height: 1178
Relative volume: 0.834896
*/