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

constexpr uint32_t TIMELIMIT = 5'000;

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
    Answer answer = solver.solve(get_now() + Milliseconds(TIMELIMIT));

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
    AggregatedStats min_support_ratio_stats;
    AggregatedStats com_x_stats;
    AggregatedStats com_y_stats;
    AggregatedStats com_z_stats;
    AggregatedStats com_x_rel_stats;
    AggregatedStats com_y_rel_stats;
    AggregatedStats com_z_rel_stats;

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
    std::cout << "Tests: " << tests.size() << ", Threads: " << THREADS_NUM << ", Time limit: " << TIMELIMIT << "ms\n\n";
    
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
                min_support_ratio_stats.add(full_metrics.stability.min_support_ratio);
                
                // Center of mass
                com_x_stats.add(full_metrics.stability.center_of_mass.x);
                com_y_stats.add(full_metrics.stability.center_of_mass.y);
                com_z_stats.add(full_metrics.stability.center_of_mass.z);
                
                // Relative center of mass
                double rel_x = std::abs(full_metrics.stability.center_of_mass.x / full_metrics.metrics.length - 0.5);
                double rel_y = std::abs(full_metrics.stability.center_of_mass.y / full_metrics.metrics.width - 0.5);
                double rel_z = full_metrics.stability.center_of_mass.z / full_metrics.metrics.height;
                com_x_rel_stats.add(rel_x);
                com_y_rel_stats.add(rel_y);
                com_z_rel_stats.add(rel_z);

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
    print_table_row("Min support ratio", min_support_ratio_stats.min_val, min_support_ratio_stats.avg(), min_support_ratio_stats.max_val);
    print_table_row("Interlocking", interlocking_stats.min_val, interlocking_stats.avg(), interlocking_stats.max_val);
    print_table_row_int("Pallets computed", (int64_t)pallets_stats.min_val, pallets_stats.avg(), (int64_t)pallets_stats.max_val);
    
    std::cout << std::string(65, '-') << "\n";
    std::cout << "Center of Mass:\n";
    print_table_row("  CoM X", com_x_stats.min_val, com_x_stats.avg(), com_x_stats.max_val);
    print_table_row("  CoM Y", com_y_stats.min_val, com_y_stats.avg(), com_y_stats.max_val);
    print_table_row("  CoM Z", com_z_stats.min_val, com_z_stats.avg(), com_z_stats.max_val);
    std::cout << "Relative Center of Mass:\n";
    print_table_row("  CoM X rel", com_x_rel_stats.min_val, com_x_rel_stats.avg(), com_x_rel_stats.max_val);
    print_table_row("  CoM Y rel", com_y_rel_stats.min_val, com_y_rel_stats.avg(), com_y_rel_stats.max_val);
    print_table_row("  CoM Z rel", com_z_rel_stats.min_val, com_z_rel_stats.avg(), com_z_rel_stats.max_val);
    
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

GreedySolver
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.5901         0.7367         0.8521
Boxes                            18          116.8            465
Height                          436         2268.7           7819
Stability                    0.4419         0.8208         0.8998
Min support ratio            0.0000         0.0272         0.7288
Interlocking                 0.1002         0.1792         0.5581
Pallets computed                  1            1.0              1
-----------------------------------------------------------------
Center of Mass:
  CoM X                    490.1938       578.4690       657.0541
  CoM Y                    348.5515       386.4272       436.1307
  CoM Z                    155.1933      1163.7723      3943.1764
Relative Center of Mass:
  CoM X rel                  0.0001         0.0196         0.0915
  CoM Y rel                  0.0000         0.0192         0.0643
  CoM Z rel                  0.3559         0.5052         0.5708
-----------------------------------------------------------------

Total tests:      436
Total time:       243.6235ms
Avg time/test:    0 ms
===========================================================================
*/

/*LNSSolver(5s)

===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.7299         0.7878         0.8833
Boxes                            18          116.8            465
Height                          342         2136.9           7888
Stability                    0.4231         0.8077         0.9617
Min support ratio            0.0003         0.0315         0.5847
Interlocking                 0.0383         0.1923         0.5769
Pallets computed                 28          874.8          16453
-----------------------------------------------------------------
Center of Mass:
  CoM X                    539.4073       583.8889       651.9121
  CoM Y                    352.3969       388.1653       448.5634
  CoM Z                    156.0504      1109.9934      4040.5146
Relative Center of Mass:
  CoM X rel                  0.0001         0.0173         0.0505
  CoM Y rel                  0.0001         0.0175         0.0607
  CoM Z rel                  0.3578         0.5137         0.5729
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3449s
Avg time/test:    161 ms
===========================================================================

min_support_ratio >= 0.5
===========================================================================
                        SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6983         0.7603         0.8919
Boxes                            18          116.8            465
Height                          342         2228.0           8430
Stability                    0.4092         0.7876         0.9617
Min support ratio            0.5000         0.5065         0.9608
Interlocking                 0.0383         0.2124         0.5908
Pallets computed                 13          397.4           8116
-----------------------------------------------------------------
Center of Mass:
 CoM X                    456.2662       587.0245       649.2329
 CoM Y                    347.4656       389.7243       425.3347
 CoM Z                    160.4016      1136.7739      4250.6084
Relative Center of Mass:
 CoM X rel                  0.0000         0.0155         0.1198
 CoM Y rel                  0.0001         0.0160         0.0657
 CoM Z rel                  0.3583         0.5040         0.5666
-----------------------------------------------------------------

Total tests:      436
Total time:       70.7157s
Avg time/test:    162 ms
===========================================================================


разрешили ставить плохие, но уменьшаем Min support ratio
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.5901         0.7460         0.8784
Boxes                            18          116.8            465
Height                          428         2253.6           8074
Stability                    0.4114         0.7900         0.8853
Min support ratio            0.0001         0.2718         1.0000
Interlocking                 0.1147         0.2100         0.5886
Pallets computed                 13          397.9           7812
-----------------------------------------------------------------
Center of Mass:
  CoM X                    516.6738       586.2586       642.8016
  CoM Y                    355.3674       389.3907       443.2052
  CoM Z                    155.1977      1149.6158      4107.2617
Relative Center of Mass:
  CoM X rel                  0.0000         0.0156         0.0694
  CoM Y rel                  0.0001         0.0162         0.0558
  CoM Z rel                  0.3568         0.5033         0.5798
-----------------------------------------------------------------

Total tests:      436
Total time:       70.7080s
Avg time/test:    162 ms
===========================================================================
*/


// grep -rn "TODO" papers/*.tex
// rm -rf build && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8