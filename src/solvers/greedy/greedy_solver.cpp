#include <solvers/greedy/greedy_solver.hpp>

#include <utils/assert.hpp>
#include <solvers/height_handler.hpp>

#include <tuple>

GreedySolver::GreedySolver(TestData test_data) : Solver(test_data) {

}

Answer GreedySolver::solve(TimePoint end_time) {
    Answer answer;
    std::sort(test_data.boxes.begin(), test_data.boxes.end(), [&](const Box &lhs, const Box &rhs) {
        // TODO: мы же можем поворачивать коробку
        return lhs.length * lhs.width > rhs.length * rhs.width;
    });

    HeightHandler height_handler;
    height_handler.add_rect(HeightRect{0, 0, test_data.length - 1, test_data.width - 1, 0});

    std::vector<std::pair<uint32_t, uint32_t>> order;
    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        order.emplace_back(i, test_data.boxes[i].quantity);
    }

    while (!order.empty()) {
        double best_score = 1e300;
        uint32_t best_i = -1;
        uint32_t best_x = -1;
        uint32_t best_y = -1;
        uint32_t best_length = -1;
        uint32_t best_width = -1;
        uint32_t best_height = -1;
        uint32_t best_rotate = -1;
        for (uint32_t i = 0; i < order.size(); i++) {
            uint32_t box_id = order[i].first;
            auto box = test_data.boxes[box_id];

            auto get_score = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t box_height) {
                uint32_t h = height_handler.get(x, y, X, Y);
                double score = 1000.0 * h + (x + y);
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
                            if (score < best_score) {
                                best_score = score;
                                best_i = i;
                                best_x = x;
                                best_y = y;
                                best_length = box_length;
                                best_width = box_width;
                                best_height = box_height;
                                best_rotate = box_rotate;
                            }
                        }
                    }
                }
            });
        }
        ASSERT(best_i != -1, "unable to put box");
        auto box = test_data.boxes[order[best_i].first];

        uint32_t best_h = height_handler.get(best_x, best_y, best_x + best_length - 1, best_y + best_width - 1);
        Position pos = {
                box.sku,
                best_x,
                best_y,
                best_h,
                best_x + best_length,
                best_y + best_width,
                best_h + best_height,
        };
        answer.positions.push_back(pos);
        height_handler.add_rect(HeightRect{
                best_x, best_y, best_x + best_length - 1, best_y + best_width - 1, best_h + best_height});

        order[best_i].second--;
        if (order[best_i].second == 0) {
            order.erase(order.begin() + best_i);
        }
    }
    return answer;
}
