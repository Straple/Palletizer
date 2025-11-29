#include <solvers/greedy/greedy_solver2.hpp>

#include <utils/assert.hpp>

#include <map>

GreedySolver2::GreedySolver2(TestData test_data) : Solver(test_data),
                                                   height_rects(
                                                           {HeightRect{0, 0, test_data.length - 1, test_data.width - 1,
                                                                       0}}) {

}


Answer GreedySolver2::solve(TimePoint end_time) {
    Answer answer;
    std::sort(test_data.boxes.begin(), test_data.boxes.end(), [&](const Box &lhs, const Box &rhs) {
        // return lhs.length * lhs.width < rhs.length * rhs.width; // very bad
        return lhs.length * lhs.width > rhs.length * rhs.width; // ok
    });

    std::vector<std::pair<uint32_t, uint32_t>> order;
    for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
        order.emplace_back(i, test_data.boxes[i].quantity);
    }
    while (!order.empty()) {
        uint32_t best_h = -1;
        uint32_t best_i = -1;
        uint32_t best_x = -1;
        uint32_t best_y = -1;
        bool best_rotate = false;
        for (uint32_t i = 0; i < order.size(); i++) {
            uint32_t box_id = order[i].first;
            auto box = test_data.boxes[box_id];

            auto get_h = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y) {
                for (auto rect: height_rects) {
                    if (!(X < rect.x || rect.X < x) &&
                        !(Y < rect.y || rect.Y < y)) {
                        return rect.h;
                    }
                }
                ASSERT(false, "unable to get_h");
            };

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
                            best_i = i;
                            best_x = x;
                            best_y = y;
                            best_rotate = false;
                        }
                    }

                    if (x + box.width <= test_data.length && y + box.length <= test_data.width) {
                        uint32_t h = get_h(x, y, x + box.width - 1, y + box.length - 1);
                        if (h < best_h) {
                            best_h = h;
                            best_i = i;
                            best_x = x;
                            best_y = y;
                            best_rotate = true;
                        }
                    }
                }
            }
        }
        ASSERT(best_h != -1, "unable to put box");
        auto box = test_data.boxes[order[best_i].first];

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

        order[best_i].second--;
        if (order[best_i].second == 0) {
            order.erase(order.begin() + best_i);
        }

        std::sort(height_rects.begin(), height_rects.end(), [&](const HeightRect &lhs, const HeightRect &rhs) {
            return lhs.h > rhs.h;
        });
    }
    return answer;
}
