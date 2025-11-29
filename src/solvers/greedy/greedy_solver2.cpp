#include <solvers/greedy/greedy_solver2.hpp>

#include <utils/assert.hpp>

#include <map>

uint32_t GreedySolver2::get_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    for (auto rect: height_rects) {
        if (!(X < rect.x || rect.X < x) &&
            !(Y < rect.y || rect.Y < y)) {
            return rect.h;
        }
    }
    ASSERT(false, "unable to get_height");
    return 0;
}

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

    auto get_min_h = [&](Box box) {
        uint32_t h = -1;
        for (auto rect: height_rects) {
            std::vector<std::pair<uint32_t, uint32_t>> xys = {
                    {rect.x,     rect.y},
                    {rect.x,     rect.Y + 1},
                    {rect.X + 1, rect.y},
                    {rect.X + 1, rect.Y + 1},
            };
            for (auto [x, y]: xys) {
                if (x + box.length <= test_data.length && y + box.width <= test_data.width) {
                    h = std::min(h, get_height(x, y, x + box.length - 1, y + box.width - 1));
                }
                if (x + box.width <= test_data.length && y + box.length <= test_data.width) {
                    h = std::min(h, get_height(x, y, x + box.width - 1, y + box.length - 1));
                }
            }
        }
        return h;
    };

    while (!order.empty()) {
        double best_score = 1e300;
        uint32_t best_i = -1;
        uint32_t best_x = -1;
        uint32_t best_y = -1;
        bool best_rotate = false;
        for (uint32_t i = 0; i < order.size(); i++) {
            uint32_t box_id = order[i].first;
            auto box = test_data.boxes[box_id];

            auto get_score = [&](uint32_t x, uint32_t y, uint32_t X, uint32_t Y) {
                uint32_t h = get_height(x, y, X, Y);

                // нужно посмотреть что за место он занимает
                height_rects.push_back(HeightRect{x, y, X, Y, h + box.height});
                uint32_t sum_next_h = 0;
                for (uint32_t j = 0; j < order.size(); j++) {
                    if (i == j) {
                        continue;
                    }
                    auto box = test_data.boxes[order[j].first];
                    uint32_t h = get_min_h(box);
                    sum_next_h += h;
                }
                height_rects.pop_back();
                // h += box.height;
                double avg_next_h = sum_next_h * 1.0 / order.size();
                double score = h * 10 + avg_next_h;
                return score;
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
                        double score = get_score(x, y, x + box.length - 1, y + box.width - 1);
                        if (score < best_score) {
                            best_score = score;
                            best_i = i;
                            best_x = x;
                            best_y = y;
                            best_rotate = false;
                        }
                    }
                    if (x + box.width <= test_data.length && y + box.length <= test_data.width) {
                        double score = get_score(x, y, x + box.width - 1, y + box.length - 1);
                        if (score < best_score) {
                            best_score = score;
                            best_i = i;
                            best_x = x;
                            best_y = y;
                            best_rotate = true;
                        }
                    }
                }
            }
        }
        ASSERT(best_i != -1, "unable to put box");
        auto box = test_data.boxes[order[best_i].first];

        if (best_rotate) {
            uint32_t best_h = get_height(best_x, best_y, best_x + box.width - 1, best_y + box.length - 1);
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
            uint32_t best_h = get_height(best_x, best_y, best_x + box.length - 1, best_y + box.width - 1);
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

        std::cout << best_x << ' ' << best_y << ' ' << best_score << std::endl;
        /*for (uint32_t i = 0; i < height_rects.size(); i++) {
            for (uint32_t j = i + 1; j < height_rects.size(); j++) {
                if (height_rects[i].x <= height_rects[j].x &&
                    height_rects[j].X <= height_rects[i].X &&
                    height_rects[i].y <= height_rects[j].y &&
                    height_rects[j].Y <= height_rects[i].Y) {

                    height_rects.erase(height_rects.begin() + j);
                    std::cout << "remove rect " << j << std::endl;
                }
            }
        }*/
    }
    return answer;
}
