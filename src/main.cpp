#include <objects/metrics.hpp>
#include <solvers/greedy/greedy_solver.hpp>
#include <solvers/lns/genetic_solver.hpp>
#include <solvers/lns/lns_solver.hpp>
#include <solvers/solver.hpp>
#include <utils/assert.hpp>
#include <utils/tools.hpp>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>

/*
 * CSV формат для записи результатов запуска бенчмарка:
 * 
 * BENCHMARK_CSV_HEADER:
 * algorithm,timelimit_ms,avg_time_per_test_ms,
 * pallet_length,pallet_width,score_normalization_height,available_rotations,
 * score_percolation_mult,score_min_support_ratio_mult,score_center_of_mass_z_mult,
 * pallets_computed_min,pallets_computed_ci_low,pallets_computed_avg,pallets_computed_ci_high,pallets_computed_max,
 * percolation_min,percolation_ci_low,percolation_avg,percolation_ci_high,percolation_max,
 * min_support_ratio_min,min_support_ratio_ci_low,min_support_ratio_avg,min_support_ratio_ci_high,min_support_ratio_max,
 * height_min,height_ci_low,height_avg,height_ci_high,height_max,
 * com_z_rel_min,com_z_rel_ci_low,com_z_rel_avg,com_z_rel_ci_high,com_z_rel_max
 */

constexpr uint32_t TIMELIMIT = 100'000;

// Структура для агрегированной статистики с доверительным интервалом
struct AggregatedStats {
    std::vector<double> values;
    double sum = 0;

    void add(double val) {
        values.push_back(val);
        sum += val;
    }

    [[nodiscard]] double min_val() const {
        return values.empty() ? 0 : *std::min_element(values.begin(), values.end());
    }

    [[nodiscard]] double max_val() const {
        return values.empty() ? 0 : *std::max_element(values.begin(), values.end());
    }

    [[nodiscard]] double avg() const {
        return values.empty() ? 0 : sum / values.size();
    }

    // Вычисляет доверительный интервал X% (по умолчанию 90%)
    // Возвращает пару (lower_bound, upper_bound) - границы интервала,
    // в который попадает X% значений (центральная часть распределения)
    [[nodiscard]] std::pair<double, double> confidence_interval(double confidence_percent = 90.0) const {
        if (values.empty()) return {0, 0};
        if (values.size() == 1) return {values[0], values[0]};

        std::vector<double> sorted = values;
        std::sort(sorted.begin(), sorted.end());

        double tail_percent = (100.0 - confidence_percent) / 2.0 / 100.0;
        size_t lower_idx = static_cast<size_t>(sorted.size() * tail_percent);
        size_t upper_idx = static_cast<size_t>(sorted.size() * (1.0 - tail_percent)) - 1;

        // Гарантируем корректные индексы
        upper_idx = std::min(upper_idx, sorted.size() - 1);

        return {sorted[lower_idx], sorted[upper_idx]};
    }

    [[nodiscard]] size_t count() const { return values.size(); }
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

void print_table_row(const std::string &name, const AggregatedStats &stats, double ci_percent = 90.0) {
    auto [ci_low, ci_high] = stats.confidence_interval(ci_percent);
    std::cout << std::left << std::setw(20) << name
              << std::right << std::fixed << std::setprecision(4)
              << std::setw(12) << stats.min_val()
              << std::setw(12) << ci_low
              << std::setw(12) << stats.avg()
              << std::setw(12) << ci_high
              << std::setw(12) << stats.max_val()
              << "\n";
}

void print_table_row_int(const std::string &name, const AggregatedStats &stats, double ci_percent = 90.0) {
    auto [ci_low, ci_high] = stats.confidence_interval(ci_percent);
    std::cout << std::left << std::setw(20) << name
              << std::right
              << std::setw(12) << (int64_t) stats.min_val()
              << std::setw(12) << (int64_t) ci_low
              << std::setw(12) << std::fixed << std::setprecision(1) << stats.avg()
              << std::setw(12) << (int64_t) ci_high
              << std::setw(12) << (int64_t) stats.max_val()
              << "\n";
}

template<typename SolverType>
void launch_solvers(const std::string &algorithm_name) {
    Timer timer;
    TestDataHeader header;// Используем дефолтные значения из TestDataHeader

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

    double sum_time_per_test = 0;

    launch_threads(THREADS_NUM, [&](uint32_t thr) {
        for (uint32_t test = 1; test < visited.size(); test++) {

            bool expected = false;
            if (!visited[test].compare_exchange_strong(expected, true)) {
                continue;
            }

            Timer timer;
            Metrics metrics = launch_one_solver<SolverType>(test);
            double time = timer.get_ms();
            tests_metrics[test] = metrics;

            int done = ++completed;

            {
                std::unique_lock locker(mutex);

                sum_time_per_test += time;

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
                          << ", height=" << std::setw(5) << metrics.height
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

    // Выводим красивую сводную таблицу с доверительным интервалом 90%
    constexpr double CI_PERCENT = 90.0;

    std::cout << "\n";
    print_separator(80);
    std::cout << "                         SUMMARY STATISTICS (CI " << CI_PERCENT << "%)\n";
    print_separator(80);

    std::cout << std::left << std::setw(20) << "Metric"
              << std::right << std::setw(12) << "Min"
              << std::setw(12) << "CI_Low"
              << std::setw(12) << "Avg"
              << std::setw(12) << "CI_High"
              << std::setw(12) << "Max"
              << "\n";
    std::cout << std::string(80, '-') << "\n";

    print_table_row("Percolation", percolation_stats, CI_PERCENT);
    print_table_row_int("Boxes", boxes_stats, CI_PERCENT);
    print_table_row_int("Height", height_stats, CI_PERCENT);
    print_table_row("Min support ratio", min_support_ratio_stats, CI_PERCENT);
    print_table_row_int("Pallets computed", pallets_stats, CI_PERCENT);

    std::cout << std::string(80, '-') << "\n";
    std::cout << "Center of Mass:\n";
    print_table_row("  CoM X", com_x_stats, CI_PERCENT);
    print_table_row("  CoM Y", com_y_stats, CI_PERCENT);
    print_table_row("  CoM Z", com_z_stats, CI_PERCENT);
    std::cout << "Relative Center of Mass:\n";
    print_table_row("  CoM X rel", com_x_rel_stats, CI_PERCENT);
    print_table_row("  CoM Y rel", com_y_rel_stats, CI_PERCENT);
    print_table_row("  CoM Z rel", com_z_rel_stats, CI_PERCENT);

    std::cout << std::string(80, '-') << "\n";

    // Итоговая информация
    std::cout << "\nTotal tests:      " << tests.size() << "\n";
    std::cout << "Total time:       " << timer << "\n";
    std::cout << "Avg time/test:    " << std::fixed << std::setprecision(1)
              << timer.get_ms() / tests.size() << " ms\n";

    print_separator();

    // Выводим CSV строку для сбора результатов бенчмарка
    auto [perc_ci_low, perc_ci_high] = percolation_stats.confidence_interval(CI_PERCENT);
    auto [msr_ci_low, msr_ci_high] = min_support_ratio_stats.confidence_interval(CI_PERCENT);
    auto [h_ci_low, h_ci_high] = height_stats.confidence_interval(CI_PERCENT);
    auto [comz_ci_low, comz_ci_high] = com_z_rel_stats.confidence_interval(CI_PERCENT);
    auto [pallets_stats_ci_low, pallets_stats_ci_high] = pallets_stats.confidence_interval(CI_PERCENT);

    double avg_time_per_test_ms = sum_time_per_test / tests.size();

    std::cout << "\nBENCHMARK_CSV_LINE:\n";
    std::cout << algorithm_name << ","
              << TIMELIMIT << ","
              << std::fixed << std::setprecision(1) << avg_time_per_test_ms << ","
              // TestDataHeader params
              << header.length << ","
              << header.width << ","
              << header.score_normalization_height << ","
              << header.available_rotations << ","
              << header.score_percolation_mult << ","
              << header.score_min_support_ratio_mult << ","
              << header.score_center_of_mass_z_mult << ","
              << std::setprecision(4)
              // pallets computed
              << pallets_stats.min_val() << ","
              << pallets_stats_ci_low << ","
              << pallets_stats.avg() << ","
              << pallets_stats_ci_high << ","
              << pallets_stats.max_val() << ","
              // Percolation stats
              << percolation_stats.min_val() << ","
              << perc_ci_low << ","
              << percolation_stats.avg() << ","
              << perc_ci_high << ","
              << percolation_stats.max_val() << ","
              // Min support ratio stats
              << min_support_ratio_stats.min_val() << ","
              << msr_ci_low << ","
              << min_support_ratio_stats.avg() << ","
              << msr_ci_high << ","
              << min_support_ratio_stats.max_val() << ","
              // Height stats
              << std::setprecision(1)
              << height_stats.min_val() << ","
              << h_ci_low << ","
              << height_stats.avg() << ","
              << h_ci_high << ","
              << height_stats.max_val() << ","
              // CoM Z rel stats
              << std::setprecision(4)
              << com_z_rel_stats.min_val() << ","
              << comz_ci_low << ","
              << com_z_rel_stats.avg() << ","
              << comz_ci_high << ","
              << com_z_rel_stats.max_val()
              << "\n\n";
}

int main() {
    launch_solvers<LNSSolver>("LNSSolver");
    return 0;

    Metrics m = launch_one_solver<LNSSolver>(228);
    std::cout << "Height: " << m.height << std::endl;
    std::cout << "Percolation: " << m.percolation << std::endl;
    std::cout << "Center of mass Z: " << m.center_of_mass.z << std::endl;
    std::cout << "min_support_ratio: " << m.min_support_ratio << std::endl;
}

/*
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.3243         0.6666         0.8549
Boxes                            18          116.8            465
Height                          424         2559.5          13170
Min support ratio            0.6391         0.7574         1.0000
Pallets computed                  6          307.5           5973
-----------------------------------------------------------------
Center of Mass:
  CoM X                    502.6877       585.9857       662.7027
  CoM Y                    318.0914       384.0097       443.0284
  CoM Z                    147.5966      1118.1440      5298.9349
Relative Center of Mass:
  CoM X rel                  0.0001         0.0180         0.0811
  CoM Y rel                  0.0001         0.0236         0.1024
  CoM Z rel                  0.2381         0.4288         0.5301
-----------------------------------------------------------------

Total tests:      436
Total time:       70.4068s
Avg time/test:    161 ms
===========================================================================



===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6145         0.7281         0.8874
Boxes                            18          116.8            465
Height                          350         2341.2           8300
Min support ratio            0.1739         0.6730         1.0000
Pallets computed                  9          317.0           6773
-----------------------------------------------------------------
Center of Mass:
  CoM X                    540.2725       589.7207       677.7617
  CoM Y                    346.3490       386.1371       439.2596
  CoM Z                    146.0785      1130.2243      4146.1190
Relative Center of Mass:
  CoM X rel                  0.0001         0.0155         0.0648
  CoM Y rel                  0.0000         0.0203         0.0671
  CoM Z rel                  0.3376         0.4750         0.5374
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3415s
Avg time/test:    161 ms
===========================================================================


===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.1629         0.4046         0.6637
Boxes                            18          116.8            465
Height                          905         4411.2          16168
Min support ratio            0.6121         0.6854         1.0000
Pallets computed                  6          436.0          10691
-----------------------------------------------------------------
Center of Mass:
  CoM X                    416.8956       567.3027       711.6701
  CoM Y                    294.5584       370.0419       458.4030
  CoM Z                    191.2755      1337.9788      5646.6103
Relative Center of Mass:
  CoM X rel                  0.0000         0.0351         0.1526
  CoM Y rel                  0.0000         0.0404         0.1318
  CoM Z rel                  0.1302         0.3028         0.5028
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3058s
Avg time/test:    161 ms
===========================================================================


===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6027         0.7277         0.8824
Boxes                            18          116.8            465
Height                          350         2308.3           8303
Min support ratio            0.1374         0.3859         0.9426
Pallets computed                 10          372.5           7425
-----------------------------------------------------------------
Center of Mass:
  CoM X                    510.6249       593.0484       657.6661
  CoM Y                    362.5285       391.7459       450.1447
  CoM Z                    144.4992      1006.1805      4144.8729
Relative Center of Mass:
  CoM X rel                  0.0001         0.0131         0.0745
  CoM Y rel                  0.0000         0.0141         0.0627
  CoM Z rel                  0.2859         0.4213         0.5329
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3046s
Avg time/test:    161 ms
===========================================================================

*/

// TODO: описание алгоритмов текстом. К псевдокоду текст. Меньше всяких пунктов, больше именно текста. Меньше иишного, более научный текст. Не: "Сложность: O(n)", а "Сложность составляет O(N)"
