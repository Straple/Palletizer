#include <solvers/lns/lns_solver.hpp>

#include <utils/assert.hpp>
#include <utils/randomizer.hpp>
#include <solvers/greedy/greedy_solver2.hpp>

#include <map>

LNSSolver::LNSSolver(TestData test_data) : Solver(test_data) {

}


std::tuple<Answer, Metrics, double> LNSSolver::simulate() const {
    Answer answer;
    HeightHandler height_handler;
    height_handler.add_rect(HeightRect{0, 0, test_data.length - 1, test_data.width - 1, 0});

    for (auto box_meta: order) {
        double best_score = 1e300;

        // (x, y, length, width, height, rotate)
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>> items;
        auto box = test_data.boxes[box_meta.box_id];

        auto get_score = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t box_height) {
            uint32_t h = height_handler.get_height(x, y, X, Y);
            double score = box_meta.h_weight * h + box_meta.x_weight * x + box_meta.y_weight * y;
            return score;
        };

        std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> available_box_sizes;
        {
            available_box_sizes.emplace_back(box.length, box.width, box.height, 0);
            if (test_data.can_swap_length_width) {
                available_box_sizes.emplace_back(box.width, box.length, box.height, 1);
            }
            if (test_data.can_swap_width_height) {
                available_box_sizes.emplace_back(box.length, box.height, box.width, 2);
            }
            if (test_data.can_swap_length_width && test_data.can_swap_width_height) {
                available_box_sizes.emplace_back(box.width, box.height, box.length, 3);
                available_box_sizes.emplace_back(box.height, box.length, box.width, 4);
                available_box_sizes.emplace_back(box.height, box.width, box.length, 5);
            }
        }

        height_handler.iterate([&](HeightRect rect) {
            std::vector<std::pair<uint32_t, uint32_t>> xys = {
                    {rect.x,     rect.y},
                    {rect.x,     rect.Y + 1},
                    {rect.X + 1, rect.y},
                    {rect.X + 1, rect.Y + 1},
            };
            for (auto [x, y]: xys) {
                for (auto [box_length, box_width, box_height, box_rotate]: available_box_sizes) {
                    if (x + box_length <= test_data.length && y + box_width <= test_data.width) {
                        double score = get_score(x, y, x + box_length - 1, y + box_width - 1, box_height);
                        if (score + 1e-6 < best_score) {
                            best_score = score;
                            items.clear();
                        }
                        if (std::abs(score - best_score) < 1e-6) {
                            items.emplace_back(x, y, box_length, box_width, box_height, box_rotate);
                        }
                    }
                }
            }
        });

        ASSERT(!items.empty(), "unable to put box");

        std::stable_sort(items.begin(), items.end());
        auto [x, y, length, width, height, rotate] = items[box_meta.k % items.size()];

        uint32_t h = height_handler.get_height(x, y, x + length - 1, y + width - 1);

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
    double score = 0;
    score += metrics.height;
    return {answer, metrics, score};
}

Answer LNSSolver::solve(TimePoint end_time) {
    std::sort(test_data.boxes.begin(), test_data.boxes.end(), [&](const Box &lhs, const Box &rhs) {
        // return lhs.length * lhs.width < rhs.length * rhs.width; // very bad
        return lhs.length * lhs.width > rhs.length * rhs.width; // ok
    });

    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        for (uint32_t q = 0; q < test_data.boxes[i].quantity; q++) {
            order.push_back({i});
        }
    }

    auto [best_answer, best_metrics, best_score] = simulate();
    //std::cout << best_score << "->";
    //std::cout.flush();

    Randomizer rnd;

    double AVAILABLE_UP = 0;

    Timer last_updated;

    auto try_swap = [&]() {
        uint32_t a = rnd.get(0, order.size() - 1);
        uint32_t b = rnd.get(0, order.size() - 1);
        if (a == b) {
            return;
        }
        std::swap(order[a], order[b]);

        auto [answer, metrics, score] = simulate();
        if (score < best_score + AVAILABLE_UP) {
            if (score + 1e-6 < best_score) {
                last_updated.reset();
                //std::cout << score << "->";
                //std::cout.flush();
            }
            best_answer = answer;
            best_metrics = metrics;
            best_score = score;
        } else {
            std::swap(order[a], order[b]);
        }
    };

    auto try_reverse = [&]() {
        uint32_t l = rnd.get(0, order.size() - 1);
        uint32_t r = rnd.get(0, order.size() - 1);
        if (l > r) {
            std::swap(l, r);
        }

        // std::reverse(order.begin() + l, order.begin() + r);
        auto old_order = order;
        if (rnd.get_d() < 0.5) {
            std::reverse(order.begin() + l, order.begin() + r);
        } else {
            std::shuffle(order.begin() + l, order.begin() + r, rnd.generator);
        }

        auto [answer, metrics, score] = simulate();
        if (score < best_score + AVAILABLE_UP) {
            if (score + 1e-6 < best_score) {
                last_updated.reset();
                //std::cout << score << "->";
                //std::cout.flush();
            }
            best_answer = answer;
            best_metrics = metrics;
            best_score = score;
        } else {
            // std::reverse(order.begin() + l, order.begin() + r);
            order = std::move(old_order);
        }
    };

    auto try_change_meta = [&]() {
        uint32_t i = rnd.get(0, order.size() - 1);

        auto old_meta = order[i];
        order[i].k = rnd.get();
        /*if (rnd.get_d() < 0.9) {
            order[i].k = rnd.get();
        }
        if (rnd.get_d() < 0.2) {
            order[i].h_weight = rnd.get_d(-1000, 1000);
        }
        if (rnd.get_d() < 0.2) {
            order[i].x_weight = rnd.get_d(-1000, 1000);
        }
        if (rnd.get_d() < 0.2) {
            order[i].y_weight = rnd.get_d(-1000, 1000);
        }*/

        auto [answer, metrics, score] = simulate();
        if (score < best_score + AVAILABLE_UP) {
            if (score + 1e-6 < best_score) {
                last_updated.reset();
                //std::cout << score << "->";
                //std::cout.flush();
            }
            best_answer = answer;
            best_metrics = metrics;
            best_score = score;
        } else {
            order[i] = old_meta;
        }
    };

    /*
     Total relative volume: 0.624956
     Total time: 178.259ms

     Total relative volume: 0.709774
     Total time: 14.0317s

     Total relative volume: 0.64383 -> 0.724288
     Total time: 70.0421s*/

    /*
1773->1771->1641->1640->1639->1637->1635->1629->1619->1618->1617->1614->1604->1591->1584->1575->1565->1564->1562->1557->1553->1550->1548->1547->1546->1545->1544->1523->1519->1510->1506->1500->1499->1497->1493->1491->1490->1489->1485->1480->1479->1477->1476->
245142 0.776314 1476
1 0.776314
Total relative volume: 0.776314
Total time: 120.002s*/
    uint32_t t = 0;
    double min_score = 1e300;
    double temp = 1;
    while (get_now() < end_time) {
        AVAILABLE_UP = 10 * temp;
        if (rnd.get_d() < 0.3) {
            try_reverse();
        } else if (rnd.get_d() < 0.5) {
            try_change_meta();
        } else {
            try_swap();
        }
        t++;
        min_score = std::min(min_score, best_score);
        temp *= 0.999;

        /*if(last_updated.get_ms() > 1000){
            temp = 1;
            last_updated.reset();
        }*/
    }
    //std::cout << std::endl;
    //std::cout << t << ' ' << best_metrics.relative_volume << ' ' << best_score << std::endl;
    //std::cout << min_score << std::endl;
    /*for (uint32_t i: order) {
        //std::cout << i << ' ';
    }
    //std::cout << '\n';*/
    return best_answer;
}
