#include <solvers/lns/genetic_solver.hpp>

#include <solvers/lns/lns_solver.hpp>

#include <utils/randomizer.hpp>

#include <tuple>

GeneticSolver::GeneticSolver(TestData test_data) : Solver(test_data) {

}

Answer GeneticSolver::solve(TimePoint end_time) {
    std::sort(test_data.boxes.begin(), test_data.boxes.end(), [&](const Box &lhs, const Box &rhs) {
        // return lhs.length * lhs.width < rhs.length * rhs.width; // very bad
        return lhs.length * lhs.width > rhs.length * rhs.width; // ok
    });

    std::vector<std::tuple<double, Metrics, Answer, std::vector<BoxMeta>>> data;

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

    return std::get<2>(data[0]);
}
