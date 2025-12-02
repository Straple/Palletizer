#include <solvers/lns/genetic_solver.hpp>

#include <solvers/lns/lns_solver.hpp>
#include <solvers/greedy/greedy_solver2.hpp>
#include <utils/randomizer.hpp>

#include <tuple>

GeneticSolver::GeneticSolver(TestData test_data) : Solver(test_data) {

}

Answer GeneticSolver::solve(TimePoint end_time) {
    std::sort(test_data.boxes.begin(), test_data.boxes.end(), [&](const Box &lhs, const Box &rhs) {
        // return lhs.length * lhs.width < rhs.length * rhs.width; // very bad
        return lhs.length * lhs.width > rhs.length * rhs.width; // ok
    });

    struct State {
        double score = 0;
        std::vector<uint32_t> order;
        HeightHandler height_handler;
        Answer answer;
    };

    std::vector<uint32_t> order;
    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        for (uint32_t q = 0; q < test_data.boxes[i].quantity; q++) {
            order.push_back({i});
        }
    }

    std::vector<State> states;
    {
        HeightHandler height_handler;
        height_handler.add_rect(HeightRect{0, 0, test_data.length - 1, test_data.width - 1, 0});
        states.push_back({0, order, height_handler});
    }

    while (true) {
        std::vector<State> next_states;

        for (auto &state: states) {
            for (uint32_t box_id: state.order) {
                std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>> items;
                auto box = test_data.boxes[box_id];

                double best_score = 1e300;

                auto get_score = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t box_height) {
                    uint32_t h = state.height_handler.get_height(x, y, X, Y);
                    double score = h * 100.0 + x + y; // + box_meta.x_weight * x + box_meta.y_weight * y;
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

                state.height_handler.iterate([&](HeightRect rect) {
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
                auto [x, y, length, width, height, rotate] = items[0];

                uint32_t h = state.height_handler.get_height(x, y, x + length - 1, y + width - 1);

                State next = state;

                Position pos = {
                        box.sku,
                        x,
                        y,
                        h,
                        x + length,
                        y + width,
                        h + height,
                };
                next.answer.positions.push_back(pos);

                next.height_handler.add_rect(HeightRect{
                        x, y, x + length - 1, y + width - 1, h + height});

                next.order.erase(std::find(next.order.begin(), next.order.end(), box_id));

                next.score = next.height_handler.get_height(0, 0, -1, -1);

                double unused_h = 0;
                uint32_t cnt = 0;
                uint32_t total_h = next.height_handler.get_height(0, 0, -1, -1);
                for (uint32_t x = 0; x < test_data.length; x += 10) {
                    for (uint32_t y = 0; y < test_data.width; y += 10) {
                        unused_h += total_h - next.height_handler.get_height(x, y, x + 9, y + 9);
                        cnt++;
                    }
                }
                unused_h /= cnt;
                next.score += unused_h;

                next_states.push_back(std::move(next));
            }
        }
        states = std::move(next_states);
        std::sort(states.begin(), states.end(), [&](const State &lhs, const State &rhs) {
            return lhs.score < rhs.score;
        });

        uint32_t N = 500;
        if(states.size() > N){
            states.resize(N);
        }

        std::cout << states[0].score << ' ' << states.size() << std::endl;

        if (states[0].order.empty()) {
            break;
        }
    }

    return states[0].answer;

    /*std::vector<std::tuple<double, Metrics, Answer, std::vector<BoxMeta>>> data;

    constexpr uint32_t DATA_SIZE = 1000;

    Randomizer rnd;

    {
        std::vector<BoxMeta> order;
        for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
            for (uint32_t q = 0; q < test_data.boxes[i].quantity; q++) {
                order.push_back({i});
            }
        }

        while (data.size() < DATA_SIZE) {
            auto [answer, metrics, score] = simulate(test_data, order);

            data.emplace_back(score, metrics, answer, order);

            std::shuffle(order.begin(), order.end(), rnd.generator);
        }
    }

    while (get_now() < end_time) {
        auto new_data = data;
        for (auto [score, metrics, answer, order]: data) {
            double p = rnd.get_d();
            if (p < 0.1) {
                uint32_t a = rnd.get(0, order.size() - 1);
                uint32_t b = rnd.get(0, order.size() - 1);
                if (a == b) {
                    continue;
                }
                std::swap(order[a], order[b]);
                auto [answer, metrics, score] = simulate(test_data, order);
                new_data.emplace_back(score, metrics, answer, order);
            } else if (p < 0.3) {
                uint32_t l = rnd.get(0, order.size() - 1);
                uint32_t r = rnd.get(0, order.size() - 1);
                if (l > r) {
                    std::swap(l, r);
                }
                std::reverse(order.begin() + l, order.begin() + r);
                auto [answer, metrics, score] = simulate(test_data, order);
                new_data.emplace_back(score, metrics, answer, order);
            } else if (p < 0.5) {
                uint32_t l = rnd.get(0, order.size() - 1);
                uint32_t r = rnd.get(0, order.size() - 1);
                if (l > r) {
                    std::swap(l, r);
                }
                std::shuffle(order.begin() + l, order.begin() + r, rnd.generator);
                auto [answer, metrics, score] = simulate(test_data, order);
                new_data.emplace_back(score, metrics, answer, order);
            } else {
                uint32_t i = rnd.get(0, order.size() - 1);
                order[i].k = rnd.get();
                auto [answer, metrics, score] = simulate(test_data, order);
                new_data.emplace_back(score, metrics, answer, order);
            }
        }
        data = std::move(new_data);
        std::sort(data.begin(), data.end(), [&](const auto &lhs, const auto &rhs) {
            return std::get<0>(lhs) < std::get<0>(rhs);
        });

        while(data.size() > DATA_SIZE){
            data.pop_back();
        }

        // std::cout << std::get<0>(data[0]) << "->";
        // std::cout.flush();
    }

    return std::get<2>(data[0]);*/
}
