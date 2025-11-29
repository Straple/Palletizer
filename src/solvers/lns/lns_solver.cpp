#include <solvers/lns/lns_solver.hpp>

#include <utils/assert.hpp>
#include <utils/randomizer.hpp>

#include <map>

LNSSolver::LNSSolver(TestData test_data) : Solver(test_data) {

}


std::tuple<Answer, Metrics, double> LNSSolver::simulate() const {
    Answer answer;
    std::vector<HeightRect> height_rects = {HeightRect{0, 0, test_data.length - 1, test_data.width - 1, 0}};

    auto get_h = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y) {
        for (auto rect: height_rects) {
            if (!(X < rect.x || rect.X < x) &&
                !(Y < rect.y || rect.Y < y)) {
                return rect.h;
            }
        }
        ASSERT(false, "unable to get_h");
    };

    for (auto [box_id, k]: order) {
        uint32_t best_h = -1;
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t, bool>> items;
        auto box = test_data.boxes[box_id];
        for (auto rect: height_rects) {
            std::vector<std::pair<uint32_t, uint32_t>> xys = {
                    {rect.x,     rect.y},
                    {rect.x,     rect.Y + 1},
                    {rect.X + 1, rect.y},
                    {rect.X + 1, rect.Y + 1},
            };
            for (auto [x, y]: xys) {
                if (x + box.length <= test_data.length && y + box.width <= test_data.width) {
                    uint32_t h = get_h(x, y, x + box.length - 1, y + box.width - 1);
                    if (h < best_h) {
                        best_h = h;
                        items.clear();
                    }
                    if (h == best_h) {
                        items.emplace_back(h, x, y, false);
                    }
                }

                if (x + box.width <= test_data.length && y + box.length <= test_data.width) {
                    uint32_t h = get_h(x, y, x + box.width - 1, y + box.length - 1);
                    if (h < best_h) {
                        best_h = h;
                        items.clear();
                    }
                    if (h == best_h) {
                        items.emplace_back(h, x, y, true);
                    }
                }
            }
        }

        ASSERT(!items.empty(), "unable to put box");

        std::stable_sort(items.begin(), items.end());
        auto [_, best_x, best_y, best_rotate] = items[k % items.size()];

        if (best_rotate) {
            Position pos = {
                    box.sku,
                    best_x,
                    best_y,
                    best_h,
                    best_x + box.width,
                    best_y + box.length,
                    best_h + box.height,
            };
            answer.positions.push_back(pos);
            height_rects.push_back(
                    HeightRect{best_x, best_y, best_x + box.width - 1, best_y + box.length - 1, best_h + box.height});
        } else {
            Position pos = {
                    box.sku,
                    best_x,
                    best_y,
                    best_h,
                    best_x + box.length,
                    best_y + box.width,
                    best_h + box.height,
            };
            answer.positions.push_back(pos);
            height_rects.push_back(
                    HeightRect{best_x, best_y, best_x + box.length - 1, best_y + box.width - 1, best_h + box.height});
        }

        std::sort(height_rects.begin(), height_rects.end(), [&](const HeightRect &lhs, const HeightRect &rhs) {
            return lhs.h > rhs.h;
        });
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
            order.push_back({i, 0});
        }
    }

    auto [best_answer, best_metrics, best_score] = simulate();
    std::cout << best_score << "->";
    std::cout.flush();

    Randomizer rnd;

    auto try_swap = [&]() {
        uint32_t a = rnd.get(0, order.size() - 1);
        uint32_t b = rnd.get(0, order.size() - 1);
        if (a == b) {
            return;
        }
        std::swap(order[a], order[b]);

        auto [answer, metrics, score] = simulate();
        if (score < best_score) {
            best_answer = answer;
            best_metrics = metrics;
            best_score = score;
            std::cout << best_score << "->";
            std::cout.flush();
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

        std::reverse(order.begin() + l, order.begin() + r);

        auto [answer, metrics, score] = simulate();
        if (score < best_score) {
            best_answer = answer;
            best_metrics = metrics;
            best_score = score;
            std::cout << best_score << "->";
            std::cout.flush();
        } else {
            std::reverse(order.begin() + l, order.begin() + r);
        }
    };

    auto try_change_meta = [&]() {
        uint32_t i = rnd.get(0, order.size() - 1);

        auto old_k = order[i].k;
        order[i].k = rnd.get();// rnd.get_d() < 0.2 ? -1 : rnd.get();

        auto [answer, metrics, score] = simulate();
        if (score < best_score) {
            best_answer = answer;
            best_metrics = metrics;
            best_score = score;
            std::cout << best_score << "->";
            std::cout.flush();
        } else {
            order[i].k = old_k;
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
    while (get_now() < end_time) {
        if (rnd.get_d() < 0.3) {
            try_reverse();
        } else if (rnd.get_d() < 0.5) {
            try_change_meta();
        } else {
            try_swap();
        }
        t++;
    }
    std::cout << std::endl;
    std::cout << t << ' ' << best_metrics.relative_volume << ' ' << best_score << std::endl;
    /*for (uint32_t i: order) {
        std::cout << i << ' ';
    }
    std::cout << '\n';*/
    return best_answer;
}
