#include <objects/metrics.hpp>
#include <solvers/greedy/greedy_solver.hpp>
#include <solvers/lns/genetic_solver.hpp>
#include <solvers/lns/lns_solver.hpp>
#include <solvers/solver.hpp>
#include <solvers/stability.hpp>
#include <utils/assert.hpp>
#include <utils/tools.hpp>

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>

struct FullMetrics {
    Metrics metrics;
    StabilityMetrics stability;
};

template<typename SolverType>
FullMetrics launch_one_solver(uint32_t test) {
    std::ifstream input("tests/" + std::to_string(test) + ".csv");
    TestData test_data;
    input >> test_data;

    Answer answer = SolverType(test_data).solve(get_now() + Milliseconds(300'000));

    std::ofstream output("answers/" + std::to_string(test) + ".csv");
    output << answer;

    FullMetrics result;
    result.metrics = calc_metrics(test_data, answer);
    result.stability = calc_stability(test_data, answer);
    return result;
}

template<typename SolverType>
void launch_solvers() {
    Timer timer;
    double sum_relative_volume = 0;
    double max_relative_volume = 0;
    double min_relative_volume = 1;
    
    // Метрики устойчивости
    double sum_stability_score = 0;
    double sum_interlocking_ratio = 0;

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

    std::vector<FullMetrics> tests_metrics(tests.size() + 1);

    launch_threads(THREADS_NUM, [&](uint32_t thr) {
        for (uint32_t test = 1; test < visited.size(); test++) {

            bool expected = false;
            if (!visited[test].compare_exchange_strong(expected, true)) {
                continue;
            }

            FullMetrics full_metrics = launch_one_solver<SolverType>(test);

            tests_metrics[test] = full_metrics;

            {
                std::unique_lock locker(mutex);

                std::cout << test << ' ' << full_metrics.metrics.relative_volume 
                          << " stability:" << full_metrics.stability.stability_score << std::endl;

                sum_relative_volume += full_metrics.metrics.relative_volume;
                max_relative_volume = std::max(max_relative_volume, full_metrics.metrics.relative_volume);
                min_relative_volume = std::min(min_relative_volume, full_metrics.metrics.relative_volume);
                
                sum_stability_score += full_metrics.stability.stability_score;
                sum_interlocking_ratio += full_metrics.stability.interlocking_ratio;
            }
        }
    });

    std::ofstream metrics_output("answers/metrics.csv");
    metrics_output << "test,boxes_num,length,width,height,boxes_volume,pallet_volume,relative_volume,"
                   << "stability_score,interlocking_ratio,center_of_mass_z" << std::endl;
    for (uint32_t test = 1; test < tests_metrics.size(); test++) {
        auto& fm = tests_metrics[test];
        metrics_output << test << ',' << fm.metrics.boxes << ',' << fm.metrics.length << ',' << fm.metrics.width
                       << ',' << fm.metrics.height << ',' << fm.metrics.boxes_volume << ',' << fm.metrics.pallet_volume
                       << ',' << fm.metrics.relative_volume
                       << ',' << fm.stability.stability_score
                       << ',' << fm.stability.interlocking_ratio
                       << ',' << fm.stability.center_of_mass_z << '\n';
    }

    /*
    Solver:
    Relative volume: 0.0899587avg 0.0752863min 0.130973max
    Time: 38.2145ms

    GreedySolver:
    Relative volume: 0.735621avg 0.590052min 0.852097max
    Time: 308.218ms

    LNSSolver(1s):
    Relative volume: 0.771827avg 0.728568min 0.863867max
    Time: 14.4416s

    LNSSolver(5s):
    Relative volume: 0.782274avg 0.731147min 0.881534max
    Time: 70.4132s

    LNSSolver(30s):
    Relative volume: 0.789305avg 0.753623min 0.886487max
    Time: 420.339s

    LNSSolver(300s):
    Relative volume: 0.797138avg 0.760271min 0.903935max
    Time: 4200.24s

    GeneticSolver(1s):
    Relative volume: 0.698772avg 0.60223min 0.825097max
    Time: 74.9252s

    Best:
    Relative volume: 0.797138avg 0.760271min 0.903935max
    Time: 4200.24s
     */
    std::cout << "Relative volume: " << sum_relative_volume / (visited.size() - 1) << "avg " << min_relative_volume
              << "min " << max_relative_volume << "max\n";
    std::cout << "Time: " << timer << '\n';
}

int main() {
    launch_solvers<LNSSolver>();
    return 0;

    FullMetrics fm = launch_one_solver<LNSSolver>(261);
    std::cout << "Height: " << fm.metrics.height << std::endl;
    std::cout << "Relative volume: " << fm.metrics.relative_volume << std::endl;
    std::cout << "Stability score: " << fm.stability.stability_score << std::endl;
    std::cout << "Interlocking ratio: " << fm.stability.interlocking_ratio << std::endl;
    std::cout << "Center of mass Z: " << fm.stability.center_of_mass_z << std::endl;
}
