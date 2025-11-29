#include <utils/assert.hpp>
#include <utils/tools.hpp>
#include <objects/metrics.hpp>
#include <solvers/solver.hpp>
#include <solvers/greedy/solver_greedy.hpp>

#include <iostream>
#include <fstream>
#include <mutex>
#include <atomic>

void launch_solvers() {
    Timer timer;
    double total_relative_volume = 0;

    std::vector<int> tests;
    {
        int test = 1;
        for (;; test++) {
            std::ifstream input("tests/" + std::to_string(test) + ".csv");
            if (!input) {
                break;
            }
            tests.push_back(test);
        }
    }
    std::vector<std::atomic<bool>> visited(tests.size() + 1);
    std::mutex mutex;

    std::vector<Metrics> tests_metrics(1);

    launch_threads(THREADS_NUM, [&](uint32_t thr) {
        for (uint32_t test = 1; test < visited.size(); test++) {

            bool expected = false;
            if (!visited[test].compare_exchange_strong(expected, true)) {
                continue;
            }

            std::ifstream input("tests/" + std::to_string(test) + ".csv");
            TestData test_data;
            input >> test_data;

            Answer answer = SolverGreedy(test_data).solve(get_now() + Milliseconds(1'000));

            std::ofstream output("answers/" + std::to_string(test) + ".csv");
            output << answer;

            Metrics metrics = calc_metrics(test_data, answer);

            {
                std::unique_lock locker(mutex);

                tests_metrics.push_back(metrics);

                std::cout << test << std::endl;

                total_relative_volume += metrics.relative_volume;
            }
        }
    });

    std::ofstream metrics_output("answers/metrics.csv");
    metrics_output << "test,boxes_num,length,width,height,boxes_volume,pallet_volume,relative_volume" << std::endl;
    for (uint32_t test = 1; test < visited.size(); test++) {
        auto metrics = tests_metrics[test];
        metrics_output << test << ',' << metrics.boxes << ',' << metrics.length << ',' << metrics.width
                       << ',' << metrics.height << ',' << metrics.boxes_volume << ',' << metrics.pallet_volume
                       << ',' << metrics.relative_volume << '\n';
    }


    /*
     Solver:
     Total relative volume: 0.0899587
     Total time: 119.989ms

     SolverGreedy:
     Total relative volume: 0.618471
     Total time: 275.46s
     */

    // 0.0890463
    std::cout << "Total relative volume: " << total_relative_volume / (visited.size() - 1) << '\n';
    std::cout << "Total time: " << timer << '\n';
}

int main() {
    launch_solvers();
}
