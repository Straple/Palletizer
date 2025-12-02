#include <solvers/greedy/greedy_solver.hpp>

#include <solvers/height_handler.hpp>
#include <utils/assert.hpp>

#include <tuple>

GreedySolver::GreedySolver(TestData test_data) : Solver(test_data) {

}

Answer GreedySolver::solve(TimePoint end_time) {
    Answer answer;

    HeightHandler height_handler;
    height_handler.add_rect(HeightRect{0, 0, test_data.header.length - 1, test_data.header.width - 1, 0});

    std::vector<std::pair<uint32_t, uint32_t>> order;
    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        order.emplace_back(i, test_data.boxes[i].quantity);
    }

    while (!order.empty()) {
        uint32_t best_score = -1;
        uint32_t best_i = -1;
        uint32_t best_x = -1;
        uint32_t best_y = -1;
        uint32_t best_length = -1;
        uint32_t best_width = -1;
        uint32_t best_height = -1;
        uint32_t best_rotation = -1;
        for (uint32_t i = 0; i < order.size(); i++) {
            uint32_t box_id = order[i].first;
            auto box = test_data.boxes[box_id];

            auto get_score = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t box_height) {
                return height_handler.get(x, y, X, Y);
            };

            auto available_boxes = get_available_boxes(test_data.header, box);
            for (const auto &box: available_boxes) {
                auto dots = height_handler.get_dots(test_data.header, box);
                for (auto [x, y]: dots) {
                    uint32_t score = get_score(x, y, x + box.length - 1, y + box.width - 1, box.height);
                    if (std::tie(score, x, y) < std::tie(best_score, best_x, best_y)) {
                        best_score = score;
                        best_i = i;
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
