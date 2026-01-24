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
    double sum_percolation = 0;
    double max_percolation = 0;
    double min_percolation = 1;
    
    // Метрики устойчивости
    double sum_stability = 0;
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

                std::cout << test << ' ' << full_metrics.metrics.percolation 
                          << " stability:" << full_metrics.stability.stability << std::endl;

                sum_percolation += full_metrics.metrics.percolation;
                max_percolation = std::max(max_percolation, full_metrics.metrics.percolation);
                min_percolation = std::min(min_percolation, full_metrics.metrics.percolation);
                sum_stability += full_metrics.stability.stability;
                sum_interlocking_ratio += full_metrics.stability.interlocking_ratio;
            }
        }
    });

    std::ofstream metrics_output("answers/metrics.csv");
    metrics_output << "test,boxes_num,length,width,height,boxes_volume,pallet_volume,percolation,"
                   << "stability,stability_area,interlocking_ratio,"
                   << "l_sum_per,l_sum,interlocking,"
                   << "supported_area,hanging_area,total_area,"
                   << "center_of_mass_x,center_of_mass_y,center_of_mass_z,total_weight,"
                   << "center_of_mass_z_relative" << std::endl;
    for (uint32_t test = 1; test < tests_metrics.size(); test++) {
        auto& fm = tests_metrics[test];
        double com_z_relative = fm.stability.center_of_mass.z / fm.metrics.height;
         
        metrics_output << test << ',' << fm.metrics.boxes << ',' << fm.metrics.length << ',' << fm.metrics.width
                       << ',' << fm.metrics.height << ',' << fm.metrics.boxes_volume << ',' << fm.metrics.pallet_volume
                       << ',' << fm.metrics.percolation
                       << ',' << fm.stability.stability
                       << ',' << fm.stability.stability_area
                       << ',' << fm.stability.interlocking_ratio
                       << ',' << fm.stability.l_sum_per
                       << ',' << fm.stability.l_sum
                       << ',' << fm.stability.interlocking
                       << ',' << fm.stability.supported_area
                       << ',' << fm.stability.hanging_area
                       << ',' << fm.stability.total_area
                       << ',' << fm.stability.center_of_mass.x
                       << ',' << fm.stability.center_of_mass.y
                       << ',' << fm.stability.center_of_mass.z
                       << ',' << fm.stability.center_of_mass.total_weight
                       << ',' << com_z_relative << '\n';
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
    std::cout << "Percolation: " << sum_percolation / (visited.size() - 1) << "avg " << min_percolation
              << "min " << max_percolation << "max\n";
    std::cout << "Time: " << timer << '\n';
}

int main() {
    launch_solvers<LNSSolver>();
    return 0;

    FullMetrics fm = launch_one_solver<LNSSolver>(261);
    std::cout << "Height: " << fm.metrics.height << std::endl;
    std::cout << "Percolation: " << fm.metrics.percolation << std::endl;
    std::cout << "Stability: " << fm.stability.stability << std::endl;
    std::cout << "Interlocking ratio: " << fm.stability.interlocking_ratio << std::endl;
    std::cout << "Center of mass Z: " << fm.stability.center_of_mass.z << std::endl;
}
