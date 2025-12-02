#include <solvers/lns/lns_solver.hpp>

#include <solvers/tools.hpp>
#include <solvers/height_handler.hpp>
#include <utils/assert.hpp>
#include <utils/randomizer.hpp>

#include <map>

std::tuple<Answer, Metrics, double> simulate(const TestData &test_data, const std::vector<BoxMeta> &order) {
    Answer answer;
    HeightHandler height_handler;
    height_handler.add_rect(HeightRect{0, 0, test_data.header.length - 1, test_data.header.width - 1, 0});

    double A = 0;
    double B = 0;
    for (auto box_meta: order) {
        double best_score = 1e300;

        // (x, y, length, width, height, rotate)
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>> items;
        auto box = test_data.boxes[box_meta.box_id];

        auto get_score = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t box_height) {
            uint32_t h = height_handler.get(x, y, X, Y);
            double score = box_meta.h_weight * h; // + box_meta.x_weight * x + box_meta.y_weight * y;
            return score;
        };

        auto available_boxes = get_available_boxes(test_data.header, box);

        height_handler.iterate([&](HeightRect rect) {
            std::vector<std::pair<uint32_t, uint32_t>> xys = {
                    {rect.x,     rect.y},
                    {rect.x,     rect.Y + 1},
                    {rect.X + 1, rect.y},
                    {rect.X + 1, rect.Y + 1},
            };
            for (auto [x, y]: xys) {
                for (auto [box_length, box_width, box_height, box_rotate]: available_boxes) {
                    if (x + box_length <= test_data.header.length && y + box_width <= test_data.header.width) {
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

        /*{
            uint32_t unused_h = 0;
            uint32_t cnt = 0;
            for (uint32_t xi = x; xi < x + length; xi += 10) {
                for (uint32_t yi = y; yi < y + width; yi += 10) {
                    unused_h += h - height_handler.get_height(x, y, x + 9, y + 9);
                    cnt++;
                }
            }
            A += unused_h * 1.0 / cnt;
        }*/

        height_handler.add_rect(HeightRect{
                x, y, x + length - 1, y + width - 1, h + height});

        // B += (h + height);

        /*double unused_h = 0;
        uint32_t cnt = 0;
        uint32_t total_h = height_handler.get_height(0, 0, -1, -1);
        for (uint32_t x = 0; x < test_data.length; x += 10) {
            for (uint32_t y = 0; y < test_data.width; y += 10) {
                unused_h += total_h - height_handler.get_height(x, y, x + 9, y + 9);
                cnt++;
            }
        }
        unused_h /= cnt;
        score += unused_h;*/
    }
    // A /= answer.positions.size();
    // B /= answer.positions.size();
    auto metrics = calc_metrics(test_data, answer);

    double score = metrics.height;// + A + B;

    // Relative volume: 0.75083avg 0.683584min 0.833746max

    // score += metrics.height * 5;

    /*double unused_h = 0;
    uint32_t cnt = 0;
    for (uint32_t x = 0; x < test_data.length; x += 10) {
        for (uint32_t y = 0; y < test_data.width; y += 10) {
            unused_h += metrics.height - height_handler.get_height(x, y, x + 9, y + 9);
            cnt++;
        }
    }
    unused_h /= cnt;
    score += unused_h;*/

    // score += metrics.height / 10.0;
    return {answer, metrics, score};
}

LNSSolver::LNSSolver(TestData test_data) : Solver(test_data) {

}

Answer LNSSolver::solve(TimePoint end_time) {
    std::sort(test_data.boxes.begin(), test_data.boxes.end(), [&](const Box &lhs, const Box &rhs) {
        // return lhs.length * lhs.width < rhs.length * rhs.width; // very bad
        return lhs.length * lhs.width > rhs.length * rhs.width; // ok
    });

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

    Timer last_updated;

    uint32_t t = 0;
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
                    last_updated.reset();
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
        t++;
        min_score = std::min(min_score, best_score);
        temp *= 0.999;

        /*if(last_updated.get_ms() > 1000){
            temp = 1;
            last_updated.reset();
        }*/
    }
    /*std::cout << std::endl;
    std::cout << t << ' ' << best_metrics.relative_volume << ' ' << best_score << std::endl;
    std::cout << min_score << std::endl;*/

    /*for (uint32_t i: order) {
        std::cout << i << ' ';
    }
    std::cout << '\n';*/
    return best_answer;
}
