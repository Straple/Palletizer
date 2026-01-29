#include <objects/metrics.hpp>
#include <settings.hpp>
#include <solvers/lns/lns_solver.hpp>
#include <solvers/stability.hpp>
#include <utils/randomizer.hpp>
#include <utils/time.hpp>
#include <utils/tools.hpp>

#include <atomic>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

const uint32_t NUM_MUTATIONS = 12;
const std::vector<std::string> MUTATION_NAMES = {
        "position_rotation",
        "reverse",
        "shuffle",
        "swap",
        "swap_adjacent",
        "threshold",
        "group_by_sku",
        "2opt_small",
        "move_bad_box",
        "sort_by_area",
        "sort_by_height",
        "sort_by_weight"};

std::vector<TestData> load_test_data(const std::vector<int> &test_ids) {
    std::vector<TestData> result;
    for (int id: test_ids) {
        std::ifstream input("tests/" + std::to_string(id) + ".csv");
        if (input) {
            TestData data;
            input >> data;
            result.push_back(data);
        }
    }
    return result;
}

double evaluate_weights(const std::vector<TestData> &test_data_list,
                        const std::vector<uint32_t> &weights,
                        uint32_t time_per_test_ms) {
    double total_score = 0;
    for (const auto &test_data: test_data_list) {
        LNSSolver solver(test_data, weights);
        Answer answer = solver.solve(get_now() + Milliseconds(time_per_test_ms));
        Metrics metrics = calc_metrics(test_data, answer);
        StabilityMetrics stability = calc_stability(test_data, answer);
        total_score += metrics.percolation + stability.min_support_ratio * 2;
    }
    return total_score;
}

void print_weights(const std::vector<uint32_t> &weights, double score) {
    std::cout << "Score: " << std::fixed << std::setprecision(4) << score << " | Weights: {";
    for (uint32_t i = 0; i < weights.size(); i++) {
        std::cout << weights[i];
        if (i < weights.size() - 1) std::cout << ", ";
    }
    std::cout << "}\n";

    std::cout << "Details:\n";
    for (uint32_t i = 0; i < weights.size(); i++) {
        std::cout << "  " << std::setw(20) << std::left << MUTATION_NAMES[i]
                  << ": " << std::setw(5) << weights[i] << "\n";
    }
    std::cout << "\n";
}

void mutate_weights(std::vector<uint32_t> &weights, Randomizer &rnd) {
    if (rnd.get_d() < 0.6) {
        uint32_t from = rnd.get(0, weights.size() - 1);
        uint32_t to = rnd.get(0, weights.size() - 1);
        if (from == to) {
            to = (from + 1) % weights.size();
        }

        uint32_t delta = rnd.get(1, weights[from]);

        if (weights[from] > delta) {
            weights[from] -= delta;
            weights[to] += delta;
        }
    } else {
        int s = weights.size() * 100;
        weights = std::vector<uint32_t>(NUM_MUTATIONS, 0);
        for (; s; s -= 10) {
            weights[rnd.get(0, weights.size() - 1)] += 10;
        }
    }
}

int main() {
    std::cout << "=== LNS Mutation Weights Training ===\n\n";

    // TODO: подобрать небольшой пул хороших тестов
    std::vector<int> test_ids = {17, 423, 338};
    auto test_data_list = load_test_data(test_ids);
    std::cout << "Loaded " << test_data_list.size() << " tests\n";
    std::cout << "Threads: " << THREADS_NUM << "\n\n";

    // {104, 100, 87, 26, 190, 113, 59, 59, 162, 100, 100, 100}
    // {140, 40, 90, 110, 90, 110, 120, 70, 110, 130, 80, 110}
    // {211, 40, 19, 113, 90, 110, 131, 71, 109, 130, 66, 110}
    // {48, 189, 13, 120, 100, 140, 110, 90, 60, 1, 80, 249}
    // {154, 40, 127, 13, 143, 110, 120, 51, 93, 130, 114, 105}
    std::vector<uint32_t> best_weights(NUM_MUTATIONS, 100);
    double best_score = 0;
    std::mutex mutex;
    uint32_t total_iterations = 0;

    uint32_t TIMELIMIT = 3'000;

    {
        double init_score = evaluate_weights(test_data_list, best_weights, TIMELIMIT);
        best_score = init_score;
        std::cout << "Initial ";
        print_weights(best_weights, best_score);
    }

    launch_threads(THREADS_NUM, [&](uint32_t thr) {
        Randomizer rnd(thr * 12345 + 249 + 8712831);

        std::vector<uint32_t> local_weights;
        {
            std::unique_lock lock(mutex);
            local_weights = best_weights;
        }

        while (true) {
            std::vector<uint32_t> new_weights = local_weights;
            mutate_weights(new_weights, rnd);

            double new_score = evaluate_weights(test_data_list, new_weights, TIMELIMIT);
            {
                std::unique_lock lock(mutex);

                uint32_t iter = ++total_iterations;

                if (new_score > best_score) {
                    best_score = new_score;
                    best_weights = new_weights;
                    local_weights = new_weights;

                    std::cout << "Thread " << thr << ", Iteration " << iter << " - NEW BEST ";
                    print_weights(best_weights, best_score);
                } else {
                    local_weights = best_weights;
                }
            }
        }
    });
}
