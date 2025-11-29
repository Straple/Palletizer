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

    for (auto box_id: order) {
        uint32_t best_h = -1;
        uint32_t best_x = -1;
        uint32_t best_y = -1;
        bool best_rotate = false;
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
                        best_x = x;
                        best_y = y;
                        best_rotate = false;
                    }
                }

                if (x + box.width <= test_data.length && y + box.length <= test_data.width) {
                    uint32_t h = get_h(x, y, x + box.width - 1, y + box.length - 1);
                    if (h < best_h) {
                        best_h = h;
                        best_x = x;
                        best_y = y;
                        best_rotate = true;
                    }
                }
            }
        }

        ASSERT(best_h != -1, "unable to put box");

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

    uint64_t sum_h = 0;
    for (uint32_t x = 0; x < test_data.length; x += 5) {
        for (uint32_t y = 0; y < test_data.width; y += 5) {
            sum_h += metrics.height - get_h(x, y, x + 4, y + 4);
        }
    }
    double score = 0;
    // score -= sum_h * 1.0 / test_data.length / test_data.width;
    // score += 1000 * metrics.relative_volume;
    score -= metrics.height;
    return {answer, metrics, score};
}

Answer LNSSolver::solve(TimePoint end_time) {
    std::sort(test_data.boxes.begin(), test_data.boxes.end(), [&](const Box &lhs, const Box &rhs) {
        // return lhs.length * lhs.width < rhs.length * rhs.width; // very bad
        return lhs.length * lhs.width > rhs.length * rhs.width; // ok
    });

    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        for (uint32_t q = 0; q < test_data.boxes[i].quantity; q++) {
            order.push_back(i);
        }
    }

    auto [best_answer, best_metrics, best_score] = simulate();
    // std::cout << best_metrics.relative_volume << "->";

    Randomizer rnd;

    auto try_swap = [&]() {
        uint32_t a = rnd.get(0, order.size() - 1);
        uint32_t b = rnd.get(0, order.size() - 1);
        if (order[a] == order[b]) {
            return;
        }

        std::swap(order[a], order[b]);

        auto [answer, metrics, score] = simulate();
        if (score >= best_score) {
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

        std::reverse(order.begin() + l, order.begin() + r);

        auto [answer, metrics, score] = simulate();
        if (score >= best_score) {
            best_answer = answer;
            best_metrics = metrics;
            best_score = score;
        } else {
            std::reverse(order.begin() + l, order.begin() + r);
        }
    };

    /*
     Total relative volume: 0.624956
     Total time: 178.259ms

     Total relative volume: 0.709774
     Total time: 14.0317s

     Total relative volume: 0.64383 -> 0.724288
     Total time: 70.0421s*/
    uint32_t t = 0;
    while (get_now() < end_time) {
        if (rnd.get_d() < 0.7) {
            try_reverse();
        } else {
            try_swap();
        }
        t++;
    }
    std::cout << t << ' ' << best_metrics.relative_volume << ' ' << best_score << std::endl;
    return best_answer;
}
