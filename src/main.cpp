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
#include <iomanip>
#include <mutex>
#include <algorithm>
#include <numeric>

struct FullMetrics {
    Metrics metrics;
    StabilityMetrics stability;
    uint64_t pallets_computed = 1;  // Количество вычисленных паллет
};

// Структура для агрегированной статистики
struct AggregatedStats {
    double min_val = std::numeric_limits<double>::max();
    double max_val = std::numeric_limits<double>::lowest();
    double sum = 0;
    uint32_t count = 0;
    
    void add(double val) {
        min_val = std::min(min_val, val);
        max_val = std::max(max_val, val);
        sum += val;
        count++;
    }
    
    [[nodiscard]] double avg() const { 
        return count > 0 ? sum / count : 0; 
    }
};

template<typename SolverType>
FullMetrics launch_one_solver(uint32_t test) {
    std::ifstream input("tests/" + std::to_string(test) + ".csv");
    TestData test_data;
    input >> test_data;

    SolverType solver(test_data);
    Answer answer = solver.solve(get_now() + Milliseconds(5'000));

    std::ofstream output("answers/" + std::to_string(test) + ".csv");
    output << answer;

    FullMetrics result;
    result.metrics = calc_metrics(test_data, answer);
    result.stability = calc_stability(test_data, answer);
    result.pallets_computed = solver.get_pallets_computed();
    
    return result;
}

// Красивый вывод таблицы
void print_separator(int width = 75) {
    std::cout << std::string(width, '=') << "\n";
}

void print_table_row(const std::string& name, double min_val, double avg_val, double max_val) {
    std::cout << std::left << std::setw(20) << name
              << std::right << std::fixed << std::setprecision(4)
              << std::setw(15) << min_val
              << std::setw(15) << avg_val
              << std::setw(15) << max_val
              << "\n";
}

void print_table_row_int(const std::string& name, int64_t min_val, double avg_val, int64_t max_val) {
    std::cout << std::left << std::setw(20) << name
              << std::right
              << std::setw(15) << min_val
              << std::setw(15) << std::fixed << std::setprecision(1) << avg_val
              << std::setw(15) << max_val
              << "\n";
}

template<typename SolverType>
void launch_solvers() {
    Timer timer;
    
    // Агрегированные статистики
    AggregatedStats percolation_stats;
    AggregatedStats boxes_stats;
    AggregatedStats height_stats;
    AggregatedStats stability_stats;
    AggregatedStats interlocking_stats;
    AggregatedStats pallets_stats;

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
    std::atomic<int> completed{0};

    std::cout << "\n";
    print_separator();
    std::cout << "                    PALLETIZER SOLVER BENCHMARK\n";
    print_separator();
    std::cout << "Tests: " << tests.size() << ", Threads: " << THREADS_NUM << ", Time limit: 5000ms\n\n";
    
    std::cout << "Progress:\n";

    launch_threads(THREADS_NUM, [&](uint32_t thr) {
        for (uint32_t test = 1; test < visited.size(); test++) {

            bool expected = false;
            if (!visited[test].compare_exchange_strong(expected, true)) {
                continue;
            }

            FullMetrics full_metrics = launch_one_solver<SolverType>(test);
            tests_metrics[test] = full_metrics;
            
            int done = ++completed;

            {
                std::unique_lock locker(mutex);

                // Обновляем статистики
                percolation_stats.add(full_metrics.metrics.percolation);
                boxes_stats.add(full_metrics.metrics.boxes);
                height_stats.add(full_metrics.metrics.height);
                stability_stats.add(full_metrics.stability.stability);
                interlocking_stats.add(full_metrics.stability.interlocking_ratio);
                pallets_stats.add(full_metrics.pallets_computed);

                std::cout << "  Test " << std::setw(3) << test 
                          << ": perc=" << std::fixed << std::setprecision(4) << full_metrics.metrics.percolation
                          << ", boxes=" << std::setw(3) << full_metrics.metrics.boxes
                          << ", height=" << std::setw(4) << full_metrics.metrics.height
                          << ", stability=" << std::setprecision(3) << full_metrics.stability.stability
                          << " [" << done << "/" << tests.size() << "]\n";
            }
        }
    });

    // Сохраняем детальные метрики в CSV
    std::ofstream metrics_output("answers/metrics.csv");
    metrics_output << "test,boxes_num,length,width,height,boxes_volume,pallet_volume,percolation,"
                   << "stability,stability_area,stability_area_sq,min_support_ratio,unstable_boxes_count,total_boxes_above_floor,"
                   << "interlocking_ratio,l_sum_per,l_sum,interlocking,"
                   << "supported_area,hanging_area,total_area,"
                   << "center_of_mass_x,center_of_mass_y,center_of_mass_z,total_weight,"
                   << "center_of_mass_z_relative,pallets_computed" << std::endl;
                   
    for (uint32_t test = 1; test < tests_metrics.size(); test++) {
        auto &fm = tests_metrics[test];
        double com_z_relative = fm.stability.center_of_mass.z / fm.metrics.height;

        metrics_output << test << ',' << fm.metrics.boxes << ',' << fm.metrics.length << ',' << fm.metrics.width
                       << ',' << fm.metrics.height << ',' << fm.metrics.boxes_volume << ',' << fm.metrics.pallet_volume
                       << ',' << fm.metrics.percolation
                       << ',' << fm.stability.stability
                       << ',' << fm.stability.stability_area
                       << ',' << fm.stability.stability_area_sq
                       << ',' << fm.stability.min_support_ratio
                       << ',' << fm.stability.unstable_boxes_count
                       << ',' << fm.stability.total_boxes_above_floor
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
                       << ',' << com_z_relative 
                       << ',' << fm.pallets_computed << '\n';
    }

    // Выводим красивую сводную таблицу
    std::cout << "\n";
    print_separator();
    std::cout << "                         SUMMARY STATISTICS\n";
    print_separator();
    
    std::cout << std::left << std::setw(20) << "Metric"
              << std::right << std::setw(15) << "Min"
              << std::setw(15) << "Avg"
              << std::setw(15) << "Max"
              << "\n";
    std::cout << std::string(65, '-') << "\n";
    
    print_table_row("Percolation", percolation_stats.min_val, percolation_stats.avg(), percolation_stats.max_val);
    print_table_row_int("Boxes", (int64_t)boxes_stats.min_val, boxes_stats.avg(), (int64_t)boxes_stats.max_val);
    print_table_row_int("Height", (int64_t)height_stats.min_val, height_stats.avg(), (int64_t)height_stats.max_val);
    print_table_row("Stability", stability_stats.min_val, stability_stats.avg(), stability_stats.max_val);
    print_table_row("Interlocking", interlocking_stats.min_val, interlocking_stats.avg(), interlocking_stats.max_val);
    print_table_row_int("Pallets computed", (int64_t)pallets_stats.min_val, pallets_stats.avg(), (int64_t)pallets_stats.max_val);
    
    std::cout << std::string(65, '-') << "\n";
    
    // Итоговая информация
    std::cout << "\nTotal tests:      " << tests.size() << "\n";
    std::cout << "Total time:       " << timer << "\n";
    std::cout << "Avg time/test:    " << std::fixed << std::setprecision(1) 
              << timer.get_ms() / tests.size() << " ms\n";
    
    print_separator();
    std::cout << "\n";
}

int main() {
    launch_solvers<LNSSolver>();
    return 0;

    FullMetrics fm = launch_one_solver<LNSSolver>(228);
    std::cout << "Height: " << fm.metrics.height << std::endl;
    std::cout << "Percolation: " << fm.metrics.percolation << std::endl;
    std::cout << "Stability: " << fm.stability.stability << std::endl;
    std::cout << "Interlocking ratio: " << fm.stability.interlocking_ratio << std::endl;
    std::cout << "Center of mass Z: " << fm.stability.center_of_mass.z << std::endl;
    std::cout << "min_support_ratio: " << fm.stability.min_support_ratio << std::endl;
}

/*
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.2109         0.4005         0.7587
Boxes                            18          116.8            465
Height                          435         4392.6          13232
Stability                    0.2224         0.5010         0.8590
Interlocking                 0.1410         0.4990         0.7776
Pallets computed                  5          341.1           7209
-----------------------------------------------------------------

Total tests:      436
                Total time:       71.2s
                Avg time/test:    163 ms
===========================================================================
*/