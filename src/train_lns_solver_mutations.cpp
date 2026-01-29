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

const uint32_t NUM_MUTATIONS = 11;

// Parameters for mutating weights
constexpr double MUTATE_WEIGHTS_SMALL_PROBABILITY = 0.6;

const std::vector<std::string> MUTATION_NAMES = {
        "position_rotation",
        "reverse",
        "shuffle",
        "swap",
        "swap_adjacent",
        "threshold",
        "group_by_sku",
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

double evaluate_params(const std::vector<TestData> &test_data_list,
                       const MutableParams &mp,
                       uint32_t time_per_test_ms) {
    double total_score = 0;
    for (const auto &test_data: test_data_list) {
        LNSSolver solver(test_data, mp);
        Answer answer = solver.solve(get_now() + Milliseconds(time_per_test_ms));
        Metrics metrics = calc_metrics(test_data, answer);
        StabilityMetrics stability = calc_stability(test_data, answer);
        total_score += metrics.percolation + stability.min_support_ratio * 2;
    }
    return total_score;
}

void print_params(const MutableParams &mp, double score) {
    std::cout << "Score: " << std::fixed << std::setprecision(4) << score << "\n";
    
    std::cout << "Weights: {";
    for (uint32_t i = 0; i < mp.weights.size(); i++) {
        std::cout << mp.weights[i];
        if (i < mp.weights.size() - 1) std::cout << ", ";
    }
    std::cout << "}\n";

    std::cout << "Params:\n";
    std::cout << "  segment_small_probability:       " << mp.segment_small_probability << "\n";
    std::cout << "  segment_small_relative_len:      " << mp.segment_small_relative_len << "\n";
    std::cout << "  position_vs_rotation_probability:" << mp.position_vs_rotation_probability << "\n";
    std::cout << "\n";
}

void mutate_params(MutableParams &mp, Randomizer &rnd) {
    // Mutate weights
    if (rnd.get_d() < MUTATE_WEIGHTS_SMALL_PROBABILITY) {
        uint32_t from = rnd.get(0, mp.weights.size() - 1);
        uint32_t to = rnd.get(0, mp.weights.size() - 1);
        if (from == to) {
            to = (from + 1) % mp.weights.size();
        }

        uint32_t delta = rnd.get(1, mp.weights[from]);

        if (mp.weights[from] > delta) {
            mp.weights[from] -= delta;
            mp.weights[to] += delta;
        }
    } else {
        int s = mp.weights.size() * 100;
        mp.weights = std::vector<uint32_t>(NUM_MUTATIONS, 0);
        for (; s; s -= 10) {
            mp.weights[rnd.get(0, mp.weights.size() - 1)] += 10;
        }
    }

    if (rnd.get_d() < 0.5){
        mp.segment_small_probability = rnd.get_d(0, 1);
    }
    if (rnd.get_d() < 0.5){
        mp.segment_small_relative_len = rnd.get_d(0, 1);
    }
    if (rnd.get_d() < 0.5){
        mp.position_vs_rotation_probability = rnd.get_d(0, 1);
    }
}

int main() {
    std::cout << "=== LNS Mutation Params Training ===\n\n";

    // TODO: подобрать небольшой пул хороших тестов
    std::vector<int> test_ids = {17, 423, 338};
    auto test_data_list = load_test_data(test_ids);
    std::cout << "Loaded " << test_data_list.size() << " tests\n";
    std::cout << "Threads: " << THREADS_NUM << "\n\n";

    MutableParams best_params;
    double best_score = 0;
    std::mutex mutex;
    uint32_t total_iterations = 0;

    uint32_t TIMELIMIT = 3'000;

    {
        double init_score = evaluate_params(test_data_list, best_params, TIMELIMIT);
        best_score = init_score;
        std::cout << "Initial ";
        print_params(best_params, best_score);
    }

    launch_threads(THREADS_NUM, [&](uint32_t thr) {
        Randomizer rnd(thr * 12345 + 249 + 8712831);

        MutableParams local_params;
        {
            std::unique_lock lock(mutex);
            local_params = best_params;
        }

        while (true) {
            MutableParams new_params = local_params;
            mutate_params(new_params, rnd);

            double new_score = evaluate_params(test_data_list, new_params, TIMELIMIT);
            {
                std::unique_lock lock(mutex);

                uint32_t iter = ++total_iterations;

                if (new_score > best_score) {
                    best_score = new_score;
                    best_params = new_params;
                    local_params = new_params;

                    std::cout << "Thread " << thr << ", Iteration " << iter << " - NEW BEST ";
                    print_params(best_params, best_score);
                } else {
                    local_params = best_params;
                }
            }
        }
    });
}
