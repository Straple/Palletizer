#include <utils/assert.hpp>
#include <utils/tools.hpp>
#include <objects/metrics.hpp>
#include <solvers/solver.hpp>
#include <solvers/greedy/greedy_solver.hpp>
#include <solvers/greedy/greedy_solver2.hpp>
#include <solvers/lns/lns_solver.hpp>

#include <iostream>
#include <fstream>
#include <mutex>
#include <atomic>

template<typename SolverType>
Metrics launch_one_solver(uint32_t test) {
    std::ifstream input("tests/" + std::to_string(test) + ".csv");
    TestData test_data;
    input >> test_data;

    Answer answer = SolverType(test_data).solve(get_now() + Milliseconds(30'000));

    std::ofstream output("answers/" + std::to_string(test) + ".csv");
    output << answer;

    Metrics metrics = calc_metrics(test_data, answer);
    return metrics;
}

template<typename SolverType>
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

    std::vector<Metrics> tests_metrics(tests.size() + 1);

    launch_threads(THREADS_NUM, [&](uint32_t thr) {
        for (uint32_t test = 1; test < visited.size(); test++) {

            bool expected = false;
            if (!visited[test].compare_exchange_strong(expected, true)) {
                continue;
            }

            Metrics metrics = launch_one_solver<SolverType>(test);

            tests_metrics[test] = metrics;

            {
                std::unique_lock locker(mutex);

                std::cout << test << ' ' << metrics.relative_volume << std::endl;

                total_relative_volume += metrics.relative_volume;
            }
        }
    });

    std::ofstream metrics_output("answers/metrics.csv");
    metrics_output << "test,boxes_num,length,width,height,boxes_volume,pallet_volume,relative_volume" << std::endl;
    for (uint32_t test = 1; test < tests_metrics.size(); test++) {
        auto metrics = tests_metrics[test];
        metrics_output << test << ',' << metrics.boxes << ',' << metrics.length << ',' << metrics.width
                       << ',' << metrics.height << ',' << metrics.boxes_volume << ',' << metrics.pallet_volume
                       << ',' << metrics.relative_volume << '\n';
    }

    /*
     Solver:
     Total relative volume: 0.0899587
     Total time: 31.5898ms

     GreedySolver:
     Total relative volume: 0.619928
     Total time: 69.3727s

     GreedySolver2:
     Total relative volume: 0.753517
     Total time: 1.03333s

     LNSSolver:
     Total relative volume: 0.742927
     Total time: 420.043s
     */
    std::cout << "Total relative volume: " << total_relative_volume / (visited.size() - 1) << '\n';
    std::cout << "Total time: " << timer << '\n';
}

int main() {
    launch_solvers<GreedySolver2>();
    /*Metrics metrics = launch_one_solver<GreedySolver2>(261);
    std::cout << "Height: " << metrics.height << std::endl;
    std::cout << "Relative volume: " << metrics.relative_volume << std::endl;*/
}
