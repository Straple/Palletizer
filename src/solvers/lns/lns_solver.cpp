#include <solvers/lns/lns_solver.hpp>

#include <solvers/height_handler.hpp>
#include <utils/assert.hpp>
#include <utils/randomizer.hpp>

#include <map>

bool print_debug = false;

double total_time = 0;

std::tuple<Answer, Metrics, double> simulate(const TestData &test_data, const std::vector<BoxMeta> &order) {
    Answer answer;
    HeightHandler height_handler;
    height_handler.add_rect(HeightRect{0, 0, test_data.header.length - 1, test_data.header.width - 1, 0});

    for (auto box_meta: order) {
        auto box = test_data.boxes[box_meta.box_id];

        uint32_t best_score = -1;
        uint32_t best_x = -1;
        uint32_t best_y = -1;
        uint32_t best_length = -1;
        uint32_t best_width = -1;
        uint32_t best_height = -1;
        uint32_t best_rotation = -1;

        auto get_score = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t box_height) {
            return height_handler.get(x, y, X, Y) + box_height;
        };

        auto get_position_dist = [&](uint32_t x, uint32_t y, uint32_t length, uint32_t width, uint32_t height) {
            auto get_position_dist = [&](uint64_t x, uint64_t y) -> uint64_t {
                if (box_meta.position == 0) {
                    return x * x + y * y;
                } else if (box_meta.position == 1) {
                    return x * x + (test_data.header.width - y) * (test_data.header.width - y);
                } else if (box_meta.position == 2) {
                    return (test_data.header.length - x) * (test_data.header.length - x) + y * y;
                } else if (box_meta.position == 3) {
                    return (test_data.header.length - x) * (test_data.header.length - x) + (test_data.header.width - y) * (test_data.header.width - y);
                } else {
                    FAILED_ASSERT("invalid position");
                    return 0;
                }
            };

            uint64_t dist = std::min(std::min(get_position_dist(x, y),
                                              get_position_dist(x + length - 1, y)),
                                     std::min(get_position_dist(x, y + width - 1),
                                              get_position_dist(x + length - 1, y + width - 1)));
            return dist;
        };

        auto available_boxes = get_available_boxes(test_data.header, box);

        // ~30s
        for (const auto &box: available_boxes) {
            if (box_meta.rotation != -1 && box_meta.rotation != box.rotation) {
                continue;
            }

            auto dots = height_handler.get_dots(test_data.header, box);// 10/30s

            // 20/30s
            for (auto [x, y]: dots) {
                // Timer timer;
                uint32_t score = get_score(x, y, x + box.length - 1, y + box.width - 1, box.height);
                // total_time += timer.get_ns();
                if (score < best_score) {
                    best_score = score;
                    best_x = x;
                    best_y = y;
                    best_length = box.length;
                    best_width = box.width;
                    best_height = box.height;
                    best_rotation = box.rotation;
                } else if (score == best_score) {
                    if (get_position_dist(x, y, box.length, box.width, box.height) < get_position_dist(best_x, best_y, best_length, best_width, best_height)) {
                        best_x = x;
                        best_y = y;
                        best_length = box.length;
                        best_width = box.width;
                        best_height = box.height;
                        best_rotation = box.rotation;
                    }
                }
            }
        }

        ASSERT(best_score != -1, "unable to put box");

        auto [x, y, length, width, height, rotation] = std::tie(best_x, best_y, best_length, best_width, best_height, best_rotation);

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

        height_handler.add_rect(HeightRect{
                x, y, x + length - 1, y + width - 1, h + height});
    }
    auto metrics = calc_metrics(test_data, answer);
    double score = metrics.height;
    if (print_debug) {
        // // std::cout << height_handler.size() << std::endl;
        // height_handler.print();
    }
    return {answer, metrics, score};
}

LNSSolver::LNSSolver(TestData test_data) : Solver(test_data) {
}

Answer LNSSolver::solve(TimePoint end_time) {
    Randomizer rnd;

    std::vector<BoxMeta> order;
    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        for (uint32_t q = 0; q < test_data.boxes[i].quantity; q++) {
            order.push_back({i, (uint32_t) -1 /*rnd.get(-1, test_data.header.available_rotations - 1)*/, 0 /*(uint32_t)rnd.get(0, 3)*/});
        }
    }
    // std::shuffle(order.begin(), order.end(), rnd.generator);

    auto [best_answer, best_metrics, best_score] = simulate(test_data, order);
    // std::cout << best_score << "->";
    // std::cout.flush();

    // Answer real_answer = best_answer;
    // auto real_score = best_score;

    Timer last_update;

    uint32_t cnt = 0;
    double min_score = 1e300;
    double temp = 1;
    while (get_now() < end_time) {
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
            // // std::cout << '\n' << score << ' ' << best_score << ' ' << (best_score - score) << ' ' << (best_score - score) / score << ' ' << 100 * ((best_score - score) / score) / temp << ' ' << exp(((best_score - score) / score) / temp) << std::endl;
            if (score < best_score || rnd.get_d() <//exp(100 * ((best_score - score) / score) / temp)
                                              exp((best_score - score) / temp)) {
                if (score + 1e-6 < best_score) {
                    last_update.reset();
                    // std::cout << score << "->";
                    // std::cout.flush();
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

        if (last_update.get_ms() > 1'000) {
            last_update.reset();
            temp = 1;//std::min(1.0, temp + 0.03);
            // // std::cout << "RESET->";
            // // std::cout.flush();

            /*std::shuffle(order.begin(), order.end(), rnd.generator);
            for (auto &box_meta: order) {
                box_meta.position = rnd.get(-1, 3);
                box_meta.rotation = rnd.get(-1, test_data.header.available_rotations - 1);
            }
            std::tie(best_answer, best_metrics, best_score) = simulate(test_data, order);*/
        }
    }
    // auto real_metrics = calc_metrics(test_data, real_answer);
    // std::cout << std::endl;
    // // std::cout << cnt << ' ' << real_metrics.percolation << ' ' << real_metrics.height << std::endl;
    // std::cout << cnt << ' ' << best_metrics.percolation << ' ' << best_metrics.height << std::endl;
    // std::cout << min_score << std::endl;

    print_debug = true;
    simulate(test_data, order);

    // std::cout << "Time m: " << total_time / 1e9 << "s\n";

    return best_answer;
}

/*
test 261
1594->1466->1466->1400->1369->1368->1368->1300->1298->1295->1300->1300->1292->1281->1277->1264->1244->1243->1227->1226->1224->1222->1221->1217->1205->1185->1180->1178->
107606 0.834896 1178
1178
Height: 1178
Percolation: 0.834896

1285->1283->1283->1281->1281->1283->1283->1281->1281->1281->1281->1281->1281->1281->1278->1273->1271->1252->1246->1242->1240->
82928 0.793151 1240
1240
Height: 1240
Percolation: 0.793151


1357->1305->1282->1258->1258->1259->1258->1242->1240->1237->1236->1233->1225->1204->1204->1198->1196->
43588 0.822331 1196
1196
Height: 1196
Percolation: 0.822331

*/

/*
test 217
7851->7825->7803->7799->7799->7796->7795->7753->7747->
296 0.759813 7747
7747
Height: 7747
Percolation: 0.759813

8061->7978->7960->7948->7926->7920->7792->7777->7775->7780->7767->7750->7753->7752->7745->7747->7756->7754->7741->
715 0.760401 7741
7741
Height: 7741
Percolation: 0.760401

156 0.766341 7681
7681
198
*/