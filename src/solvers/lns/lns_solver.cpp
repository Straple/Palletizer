#include <solvers/lns/lns_solver.hpp>

#include <solvers/height_handler.hpp>
#include <utils/assert.hpp>
#include <utils/randomizer.hpp>

#include <map>

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

        std::stable_sort(items.begin(), items.end());
        auto [x, y, length, width, height, rotation] = items[box_meta.k % items.size()];

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
    // std::cout << best_score << "->";
    // std::cout.flush();

    Randomizer rnd;

    double AVAILABLE_UP = 0;

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
                order[i].k = rnd.get();
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
                    // std::cout << score << "->";
                    // std::cout.flush();
                }
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
    }
    // std::cout << std::endl;
    // std::cout << cnt << ' ' << best_metrics.relative_volume << ' ' << best_score << std::endl;
    // std::cout << min_score << std::endl;
    return best_answer;
}
