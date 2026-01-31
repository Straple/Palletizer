#include <objects/metrics.hpp>
#include <solvers/greedy/greedy_solver.hpp>
#include <solvers/lns/genetic_solver.hpp>
#include <solvers/lns/lns_solver.hpp>
#include <solvers/solver.hpp>
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
Metrics launch_one_solver(uint32_t test) {
    std::ifstream input("tests/" + std::to_string(test) + ".csv");
    TestData test_data;
    input >> test_data;

    SolverType solver(test_data);
    Answer answer = solver.solve(get_now() + Milliseconds(TIMELIMIT));

    std::ofstream output("answers/" + std::to_string(test) + ".csv");
    output << answer;

    Metrics metrics = calc_metrics(test_data, answer);
    metrics.pallets_computed = solver.get_pallets_computed();
    
    return metrics;
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
    std::vector<Metrics> tests_metrics(tests.size() + 1);
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

            Metrics metrics = launch_one_solver<SolverType>(test);
            tests_metrics[test] = metrics;
            
            int done = ++completed;

            {
                std::unique_lock locker(mutex);

                // Обновляем статистики
                percolation_stats.add(metrics.percolation);
                boxes_stats.add(metrics.boxes);
                height_stats.add(metrics.height);
                pallets_stats.add(metrics.pallets_computed);
                min_support_ratio_stats.add(metrics.min_support_ratio);
                
                // Center of mass
                com_x_stats.add(metrics.center_of_mass.x);
                com_y_stats.add(metrics.center_of_mass.y);
                com_z_stats.add(metrics.center_of_mass.z);
                
                // Relative center of mass
                com_x_rel_stats.add(metrics.relative_center_of_mass.x);
                com_y_rel_stats.add(metrics.relative_center_of_mass.y);
                com_z_rel_stats.add(metrics.relative_center_of_mass.z);

                std::cout << "  Test " << std::setw(3) << test 
                          << ": perc=" << std::fixed << std::setprecision(4) << metrics.percolation
                          << ", boxes=" << std::setw(3) << metrics.boxes
                          << ", height=" << std::setw(4) << metrics.height
                          << ", min_support=" << std::setprecision(3) << metrics.min_support_ratio
                          << " [" << done << "/" << tests.size() << "]\n";
            }
        }
    });

    // Сохраняем детальные метрики в CSV
    std::ofstream metrics_output("answers/metrics.csv");
    metrics_output << "test,boxes_num,length,width,height,boxes_volume,pallet_volume,percolation,"
                   << "min_support_ratio,supported_area,total_area,"
                   << "center_of_mass_x,center_of_mass_y,center_of_mass_z,total_weight,"
                   << "center_of_mass_z_relative,pallets_computed" << std::endl;
                   
    for (uint32_t test = 1; test < tests_metrics.size(); test++) {
        auto &m = tests_metrics[test];

        metrics_output << test << ',' << m.boxes << ',' << m.length << ',' << m.width
                       << ',' << m.height << ',' << m.boxes_volume << ',' << m.pallet_volume
                       << ',' << m.percolation
                       << ',' << m.min_support_ratio
                       << ',' << m.supported_area
                       << ',' << m.total_area
                       << ',' << m.center_of_mass.x
                       << ',' << m.center_of_mass.y
                       << ',' << m.center_of_mass.z
                       << ',' << m.total_weight
                       << ',' << m.relative_center_of_mass.z 
                       << ',' << m.pallets_computed << '\n';
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
    print_table_row("Min support ratio", min_support_ratio_stats.min_val, min_support_ratio_stats.avg(), min_support_ratio_stats.max_val);
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

    Metrics m = launch_one_solver<LNSSolver>(228);
    std::cout << "Height: " << m.height << std::endl;
    std::cout << "Percolation: " << m.percolation << std::endl;
    std::cout << "Center of mass Z: " << m.center_of_mass.z << std::endl;
    std::cout << "min_support_ratio: " << m.min_support_ratio << std::endl;
}
