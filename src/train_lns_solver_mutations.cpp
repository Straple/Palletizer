#include <objects/metrics.hpp>
#include <settings.hpp>
#include <solvers/lns/lns_solver.hpp>
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

const uint32_t NUM_MUTATIONS = 13;

// Parameters for mutating weights
constexpr double MUTATE_WEIGHTS_SMALL_PROBABILITY = 0.6;

const std::vector<std::string> MUTATION_NAMES = {
        // Position/Rotation
        "position_rotation_fixed",
        "position_rotation_random",
        // Order
        "reverse",
        "shuffle",
        "swap",
        "swap_adjacent",
        // Sort
        "sort_by_area",
        "sort_by_volume",
        "sort_by_height",
        "sort_by_weight",
        // Special
        "threshold",
        "group_by_sku",
        "move_bad_box"};

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
        const auto &header = test_data.header;
        double score = 0;
        score += header.score_percolation_mult * metrics.percolation;
        score += header.score_min_support_ratio_mult * metrics.min_support_ratio;
        score += header.score_center_of_mass_z_mult * (1 - metrics.relative_center_of_mass.z);
        total_score += score;
    }
    return total_score / test_data_list.size();
}

void print_params(const MutableParams &mp, double score) {
    std::cout << "Score: " << std::fixed << std::setprecision(4) << score << "\n";
    std::cout << "Weights: {";
    for (uint32_t i = 0; i < mp.weights.size(); i++) {
        std::cout << mp.weights[i];
        if (i < mp.weights.size() - 1) std::cout << ", ";
    }
    std::cout << "}\n\n";
}

void mutate_params(MutableParams &mp, Randomizer &rnd) {
    // Mutate weights
    if (rnd.get_d() < MUTATE_WEIGHTS_SMALL_PROBABILITY) {
        uint32_t from = rnd.get(0, mp.weights.size() - 1);
        uint32_t to = rnd.get(0, mp.weights.size() - 1);
        if (from == to) {
            to = (from + 1) % mp.weights.size();
        }
        if (mp.weights[from] > 0) {
            uint32_t delta = rnd.get(1, mp.weights[from]);
            if (rnd.get_d() < 0.5) {
                delta = mp.weights[from];
            }
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
}

int main() {
    std::cout << "=== LNS Mutation Params Training ===\n\n";

    // TODO: подобрать небольшой пул хороших тестов
    std::vector<int> test_ids = {17, 25, 101, 228, 333, 338, 400, 423};
    auto test_data_list = load_test_data(test_ids);
    std::cout << "Loaded " << test_data_list.size() << " tests\n";
    std::cout << "Threads: " << THREADS_NUM << "\n\n";

    MutableParams best_params;
    double best_score = 0;
    std::mutex mutex;
    uint32_t total_iterations = 0;

    uint32_t TIMELIMIT = 2'000;

    {
        double init_score = evaluate_params(test_data_list, best_params, TIMELIMIT);
        best_score = init_score;
        std::cout << "Initial ";
        print_params(best_params, best_score);
    }

    launch_threads(THREADS_NUM, [&](uint32_t thr) {
        Randomizer rnd(thr * 12345 + 9788615255);

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


/*
Thread 14, Iteration 360 - NEW BEST Score: 6.5068
Weights: {90, 110, 70, 80, 100, 140, 100, 80, 100, 120, 110}
Params:
  segment_small_probability:       0.3247
  segment_small_relative_len:      0.8780
  position_vs_rotation_probability:0.0684
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6190         0.7300         0.8593
Boxes                            18          116.8            465
Height                          342         2326.9           8276
Stability                    0.3300         0.7537         0.8630
Min support ratio            0.2493         0.6426         1.0000
Interlocking                 0.1370         0.2463         0.6700
Pallets computed                  9          298.3           6310
-----------------------------------------------------------------
Center of Mass:
  CoM X                    494.2461       586.5031       653.6341
  CoM Y                    344.1735       387.5305       440.9762
  CoM Z                    157.7538      1133.2276      3967.9819
Relative Center of Mass:
  CoM X rel                  0.0000         0.0161         0.0881
  CoM Y rel                  0.0001         0.0190         0.0698
  CoM Z rel                  0.3775         0.4805         0.5649
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3304s
Avg time/test:    161 ms
===========================================================================





Initial Score: 6.0635
swap_k_max_ratio: 0.1000
Weights: {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100}
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6219         0.7396         0.8833
Boxes                            18          116.8            465
Height                          341         2290.6           8225
Stability                    0.2534         0.7735         0.9322
Min support ratio            0.1956         0.5602         1.0000
Interlocking                 0.0678         0.2265         0.7466
Pallets computed                  9          307.9          11745
-----------------------------------------------------------------
Center of Mass:
  CoM X                    504.6496       586.0959       683.0965
  CoM Y                    342.6004       387.6067       439.3842
  CoM Z                    158.6700      1152.6985      4269.4982
Relative Center of Mass:
  CoM X rel                  0.0001         0.0168         0.0795
  CoM Y rel                  0.0000         0.0182         0.0717
  CoM Z rel                  0.3559         0.4965         0.5614
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3577s
Avg time/test:    161 ms
===========================================================================







Thread 26, Iteration 690 - NEW BEST Score: 6.5550
swap_k_max_ratio: 0.1000
Weights: {77, 111, 112, 100, 100, 59, 100, 141, 46, 154, 100, 100, 100}
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6488         0.7409         0.8759
Boxes                            18          116.8            465
Height                          343         2285.4           8225
Stability                    0.3750         0.7768         0.9170
Min support ratio            0.1956         0.5570         1.0000
Interlocking                 0.0830         0.2232         0.6250
Pallets computed                  9          294.0           9734
-----------------------------------------------------------------
Center of Mass:
  CoM X                    533.3960       587.7034       740.2853
  CoM Y                    315.7982       388.2885       453.3932
  CoM Z                    156.0679      1145.6621      4269.4982
Relative Center of Mass:
  CoM X rel                  0.0000         0.0162         0.1169
  CoM Y rel                  0.0001         0.0180         0.1053
  CoM Z rel                  0.3595         0.4944         0.5751
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3508s
Avg time/test:    161 ms
===========================================================================


Thread 17, Iteration 322 - NEW BEST Score: 6.3744
swap_k_max_ratio: 0.3870
Weights: {110, 90, 140, 150, 60, 70, 90, 130, 90, 120, 110, 60, 80}
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6458         0.7408         0.8638
Boxes                            18          116.8            465
Height                          350         2286.7           8307
Stability                    0.2598         0.7763         0.9403
Min support ratio            0.1954         0.5785         1.0000
Interlocking                 0.0597         0.2237         0.7402
Pallets computed                 10          298.7           7761
-----------------------------------------------------------------
Center of Mass:
  CoM X                    461.7443       587.6055       672.5079
  CoM Y                    337.7759       387.4666       437.9733
  CoM Z                    158.7647      1146.4223      4312.2023
Relative Center of Mass:
  CoM X rel                  0.0001         0.0157         0.1152
  CoM Y rel                  0.0000         0.0183         0.0778
  CoM Z rel                  0.3620         0.4945         0.5688
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3934s
Avg time/test:    161 ms
===========================================================================




Thread 27, Iteration 479 - NEW BEST Score: 6.3225
swap_k_max_ratio: 0.3093
Weights: {70, 110, 120, 100, 150, 90, 130, 90, 80, 130, 60, 100, 70}
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6509         0.7382         0.8709
Boxes                            18          116.8            465
Height                          350         2291.8           8578
Stability                    0.3230         0.7748         0.8889
Min support ratio            0.2477         0.5561         1.0000
Interlocking                 0.1111         0.2252         0.6770
Pallets computed                  9          297.3           8430
-----------------------------------------------------------------
Center of Mass:
  CoM X                    509.3234       586.9774       646.8726
  CoM Y                    339.3519       387.6863       440.7114
  CoM Z                    158.8756      1139.6985      4391.4895
Relative Center of Mass:
  CoM X rel                  0.0001         0.0160         0.0756
  CoM Y rel                  0.0000         0.0187         0.0758
  CoM Z rel                  0.3808         0.4905         0.5656
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3704s
Avg time/test:    161 ms
===========================================================================


Thread 29, Iteration 54 - NEW BEST Score: 6.2858
swap_k_max_ratio: 0.0527
Weights: {100, 130, 100, 110, 150, 70, 80, 100, 70, 140, 80, 90, 80}
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.5915         0.7368         0.8604
Boxes                            18          116.8            465
Height                          343         2296.3           8578
Stability                    0.4115         0.7751         0.8583
Min support ratio            0.2572         0.5666         1.0000
Interlocking                 0.1417         0.2249         0.5885
Pallets computed                  9          299.1           9044
-----------------------------------------------------------------
Center of Mass:
  CoM X                    480.0283       587.9374       687.3342
  CoM Y                    337.8788       388.4246       440.4970
  CoM Z                    151.7054      1147.4313      4391.4895
Relative Center of Mass:
  CoM X rel                  0.0002         0.0166         0.1000
  CoM Y rel                  0.0001         0.0176         0.0777
  CoM Z rel                  0.3625         0.4938         0.5908
-----------------------------------------------------------------

Total tests:      436
Total time:       70.4164s
Avg time/test:    161 ms
===========================================================================

 ===========================================================================
                        SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6373         0.7312         0.8672
Boxes                            18          116.8            465
Height                          350         2327.3           8367
Stability                    0.2406         0.7633         0.8909
Min support ratio            0.2177         0.6401         1.0000
Interlocking                 0.1091         0.2367         0.7594
Pallets computed                  8          286.4           8649
-----------------------------------------------------------------
Center of Mass:
 CoM X                    526.0929       588.8506       684.8290
 CoM Y                    334.6329       388.6829       440.0965
 CoM Z                    151.1221      1144.1867      4105.2176
Relative Center of Mass:
 CoM X rel                  0.0000         0.0154         0.0707
 CoM Y rel                  0.0001         0.0172         0.0817
 CoM Z rel                  0.3474         0.4859         0.5662
-----------------------------------------------------------------

Total tests:      436
Total time:       70.4128s
Avg time/test:    161 ms
===========================================================================


===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.4469         0.7109         0.8565
Boxes                            18          116.8            465
Height                          342         2402.4          13170
Stability                    0.3598         0.7360         0.8940
Min support ratio            0.6122         0.7074         1.0000
Interlocking                 0.1060         0.2640         0.6402
Pallets computed                  5          273.8           6371
-----------------------------------------------------------------
Center of Mass:
  CoM X                    443.1907       584.8611       665.6713
  CoM Y                    282.0063       384.6401       436.8570
  CoM Z                    146.7396      1175.8325      5298.9349
Relative Center of Mass:
  CoM X rel                  0.0001         0.0185         0.1307
  CoM Y rel                  0.0001         0.0231         0.1475
  CoM Z rel                  0.3373         0.4852         0.5443
-----------------------------------------------------------------

Total tests:      436
Total time:       70.4072s
Avg time/test:    161 ms
===========================================================================

*/