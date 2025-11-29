#include <solvers/greedy/greedy_solver.hpp>

#include <utils/assert.hpp>

#include <map>

GreedySolver::GreedySolver(TestData test_data) : Solver(test_data),
                                                 heights(test_data.length, std::vector<uint32_t>(test_data.width)) {

}

uint32_t GreedySolver::get_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    ASSERT(x <= X && X < test_data.length, "invalid x");
    ASSERT(Y <= Y && Y < test_data.width, "invalid y");
    uint32_t h = 0;
    for (uint32_t xi = x; xi <= X; xi++) {
        for (uint32_t yi = y; yi <= Y; yi++) {
            h = std::max(h, heights[xi][yi]);
        }
    }
    return h;
}

void GreedySolver::set_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) {
    ASSERT(x <= X && X < test_data.length, "invalid x");
    ASSERT(Y <= Y && Y < test_data.width, "invalid y");
    for (uint32_t xi = x; xi <= X; xi++) {
        for (uint32_t yi = y; yi <= Y; yi++) {
            heights[xi][yi] = h;
        }
    }
}

Answer GreedySolver::solve(TimePoint end_time) {
    Answer answer;
    std::sort(test_data.boxes.begin(), test_data.boxes.end(), [&](const Box &lhs, const Box &rhs) {
        // return lhs.length * lhs.width < rhs.length * rhs.width; // very bad
        return lhs.length * lhs.width > rhs.length * rhs.width; // ok
    });

    for (auto box: test_data.boxes) {
        for (uint32_t q = 0; q < box.quantity; q++) {
            uint32_t best_h = -1;
            uint32_t best_x = -1;
            uint32_t best_y = -1;

            auto dp = heights;
            for (uint32_t x = 0; x < test_data.length; x++) {
                std::map<uint32_t, uint32_t> map;
                uint32_t r = 0;
                for (uint32_t y = 0; y + box.width <= test_data.width; y++) {
                    while (r < y + box.width) {
                        map[heights[x][r]]++;
                        r++;
                    }
                    dp[x][y] = (--map.end())->first;

                    map[heights[x][y]]--;
                    if (map[heights[x][y]] == 0) {
                        map.erase(heights[x][y]);
                    }
                }
            }

            auto dp2 = dp;

            for (uint32_t y = 0; y < test_data.width; y++) {
                std::map<uint32_t, uint32_t> map;
                uint32_t r = 0;
                for (uint32_t x = 0; x + box.length <= test_data.length; x++) {
                    while (r < x + box.length) {
                        map[dp[r][y]]++;
                        r++;
                    }
                    dp2[x][y] = (--map.end())->first;
                    map[dp[x][y]]--;
                    if (map[dp[x][y]] == 0) {
                        map.erase(dp[x][y]);
                    }
                }
            }

            auto get_h = [&](uint32_t x, uint32_t y) {
                /*uint32_t h = 0;
                for (uint32_t xi = x; xi < x + box.length; xi++) {
                    h = std::max(h, dp[xi][y]);
                }
                ASSERT(h == dp2[x][y], "invalid h");*/
                return dp2[x][y];
            };

            for (uint32_t x = 0; x < test_data.length; x++) {
                for (uint32_t y = 0; y + box.width <= test_data.width; y++) {
                    if (x + box.length <= test_data.length && y + box.width <= test_data.width) {
                        uint32_t h = get_h(x, y);
                        if (h < best_h) {
                            best_h = h;
                            best_x = x;
                            best_y = y;
                        }
                    }
                }
            }
            ASSERT(best_h != -1, "unable to put box");
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
            set_height(best_x, best_y, best_x + box.length - 1, best_y + box.width - 1, best_h + box.height);
        }
    }
    return answer;
}
