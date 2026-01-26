#include <solvers/lns/genetic_solver.hpp>

#include <solvers/height_handler_rects.hpp>
#include <solvers/tools.hpp>
#include <utils/randomizer.hpp>

#include <tuple>

GeneticSolver::GeneticSolver(TestData test_data) : Solver(test_data) {

}

Answer GeneticSolver::solve(TimePoint end_time) {
    return Answer();
    /*std::sort(test_data.boxes.begin(), test_data.boxes.end(), [&](const Box &lhs, const Box &rhs) {
        return lhs.length * lhs.width > rhs.length * rhs.width; // ok
    });

    struct State {
        double score = 0;
        std::vector<uint32_t> order;
        HeightHandlerRects height_handler;
        Answer answer;
        
        State(uint32_t length, uint32_t width) : height_handler(length, width) {}
        State(const State& other) = default;
        State(State&& other) = default;
        State& operator=(const State& other) = default;
        State& operator=(State&& other) = default;
    };

    std::vector<uint32_t> order;
    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        for (uint32_t q = 0; q < test_data.boxes[i].quantity; q++) {
            order.push_back({i});
        }
    }

    std::vector<State> states;
    {
        State initial(test_data.header.length, test_data.header.width);
        initial.score = 0;
        initial.order = order;
        states.push_back(std::move(initial));
    }

    while (true) {
        std::vector<State> next_states;

        for (auto &state: states) {
            for (uint32_t box_id: state.order) {
                std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>> items;
                auto box = test_data.boxes[box_id];

                double best_score = 1e300;

                auto get_score = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t box_height) {
                    uint32_t h = state.height_handler.get(x, y, X, Y);
                    double score = h * 100.0 + x + y; // + box_meta.x_weight * x + box_meta.y_weight * y;
                    return score;
                };

                auto available_boxes = get_available_boxes(test_data.header, box);

                state.height_handler.iterate([&](HeightRect rect) {
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
                auto [x, y, length, width, height, rotate] = items[0];

                uint32_t h = state.height_handler.get(x, y, x + length - 1, y + width - 1);

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

                next.score = next.height_handler.get(0, 0, -1, -1);

                double unused_h = 0;
                uint32_t cnt = 0;
                uint32_t total_h = next.height_handler.get(0, 0, -1, -1);
                for (uint32_t x = 0; x < test_data.header.length; x += 10) {
                    for (uint32_t y = 0; y < test_data.header.width; y += 10) {
                        unused_h += total_h - next.height_handler.get(x, y, x + 9, y + 9);
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
        if (states.size() > N) {
            states.resize(N);
        }

        std::cout << states[0].score << ' ' << states.size() << std::endl;

        if (states[0].order.empty()) {
            break;
        }
    }

    return states[0].answer;*/
}
