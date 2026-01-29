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
    if (rnd.get_d() < 0.3) {
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
    }

    if (rnd.get_d() < 0.5) {
        mp.segment_small_probability = rnd.get_d(0, 1);
    }
    if (rnd.get_d() < 0.5) {
        // mp.segment_small_relative_len = rnd.get_d(0, 1);
    }
    if (rnd.get_d() < 0.5) {
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
Thread 23, Iteration 657 - NEW BEST Score: 6.4314
Weights: {177, 98, 4, 100, 100, 110, 91, 80, 120, 90, 130}
Params:
  segment_small_probability:       0.3031
  segment_small_relative_len:      0.6662
  position_vs_rotation_probability:0.9679
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6234         0.7244         0.8627
Boxes                            18          116.8            465
Height                          342         2352.5           8355
Stability                    0.3431         0.7515         0.8630
Min support ratio            0.2424         0.6523         1.0000
Interlocking                 0.1370         0.2485         0.6569
Pallets computed                  8          290.5           5734
-----------------------------------------------------------------
Center of Mass:
  CoM X                    501.1509       585.5748       649.8238
  CoM Y                    335.2152       386.5364       434.9929
  CoM Z                    153.4503      1147.2058      3876.0176
Relative Center of Mass:
  CoM X rel                  0.0002         0.0174         0.0824
  CoM Y rel                  0.0000         0.0193         0.0810
  CoM Z rel                  0.3536         0.4815         0.5589
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3543s
Avg time/test:    161 ms
===========================================================================





Thread 5, Iteration 563 - NEW BEST Score: 6.3087
Weights: {205, 15, 1, 92, 64, 140, 100, 173, 120, 140, 50}
Params:
  segment_small_probability:       0.3988
  segment_small_relative_len:      0.0101
  position_vs_rotation_probability:0.9230
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6257         0.7374         0.8593
Boxes                            18          116.8            465
Height                          343         2293.0           7999
Stability                    0.3285         0.7632         0.8686
Min support ratio            0.2451         0.5949         1.0000
Interlocking                 0.1314         0.2368         0.6715
Pallets computed                 10          294.1           5469
-----------------------------------------------------------------
Center of Mass:
  CoM X                    513.8649       582.6536       695.3036
  CoM Y                    325.5344       386.0571       441.2503
  CoM Z                    157.8268      1150.6282      3980.5171
Relative Center of Mass:
  CoM X rel                  0.0002         0.0183         0.0794
  CoM Y rel                  0.0001         0.0204         0.0931
  CoM Z rel                  0.3760         0.4947         0.5640
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3743s
Avg time/test:    161 ms
===========================================================================







Thread 19, Iteration 508 - NEW BEST Score: 6.3681
Weights: {90, 60, 150, 70, 100, 130, 80, 100, 130, 70, 120}
Params:
  segment_small_probability:       0.5966
  segment_small_relative_len:      0.0337
  position_vs_rotation_probability:0.2139
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6230         0.7242         0.8593
Boxes                            18          116.8            465
Height                          342         2346.0           8234
Stability                    0.3020         0.7532         0.8630
Min support ratio            0.2285         0.6436         1.0000
Interlocking                 0.1370         0.2468         0.6980
Pallets computed                  7          296.6           5814
-----------------------------------------------------------------
Center of Mass:
  CoM X                    504.6417       584.3441       757.1150
  CoM Y                    327.4306       384.9259       431.0034
  CoM Z                    157.7538      1150.9833      4092.4628
Relative Center of Mass:
  CoM X rel                  0.0001         0.0189         0.1309
  CoM Y rel                  0.0002         0.0216         0.0907
  CoM Z rel                  0.3737         0.4832         0.5587
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3730s
Avg time/test:    161 ms
===========================================================================





Thread 29, Iteration 515 - NEW BEST Score: 6.3423
Weights: {160, 60, 80, 90, 96, 124, 50, 110, 130, 100, 100}
Params:
  segment_small_probability:       0.6546
  segment_small_relative_len:      0.0998
  position_vs_rotation_probability:0.9119
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.5985         0.7243         0.8792
Boxes                            18          116.8            465
Height                          342         2350.6           8184
Stability                    0.3257         0.7517         0.8527
Min support ratio            0.2486         0.6488         1.0000
Interlocking                 0.1473         0.2483         0.6743
Pallets computed                  8          290.1           5748
-----------------------------------------------------------------
Center of Mass:
  CoM X                    506.6725       583.8754       659.0923
  CoM Y                    354.7854       385.6370       433.0283
  CoM Z                    157.8884      1159.9724      4162.3006
Relative Center of Mass:
  CoM X rel                  0.0001         0.0173         0.0778
  CoM Y rel                  0.0001         0.0211         0.0565
  CoM Z rel                  0.3684         0.4869         0.5622
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3822s
Avg time/test:    161 ms
===========================================================================





Thread 0, Iteration 729 - NEW BEST Score: 6.2828
Weights: {120, 90, 29, 131, 103, 117, 69, 71, 139, 140, 91}
Params:
  segment_small_probability:       0.7090
  segment_small_relative_len:      0.0129
  position_vs_rotation_probability:0.6557
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6114         0.7218         0.8604
Boxes                            18          116.8            465
Height                          343         2359.2           8160
Stability                    0.3125         0.7518         0.8659
Min support ratio            0.2179         0.6449         1.0000
Interlocking                 0.1341         0.2482         0.6875
Pallets computed                  8          292.1           5506
-----------------------------------------------------------------
Center of Mass:
  CoM X                    509.5114       583.4017       635.2786
  CoM Y                    339.9424       385.5823       426.6199
  CoM Z                    159.9463      1156.6390      4137.0395
Relative Center of Mass:
  CoM X rel                  0.0000         0.0173         0.0754
  CoM Y rel                  0.0000         0.0208         0.0751
  CoM Z rel                  0.3728         0.4836         0.5655
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3871s
Avg time/test:    161 ms
===========================================================================





Thread 17, Iteration 272 - NEW BEST Score: 6.4191
Weights: {90, 110, 70, 80, 100, 140, 100, 80, 100, 120, 110}
Params:
  segment_small_probability:       0.6593
  segment_small_relative_len:      0.8780
  position_vs_rotation_probability:0.0317
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6082         0.7300         0.8604
Boxes                            18          116.8            465
Height                          342         2329.8           8057
Stability                    0.3763         0.7550         0.8653
Min support ratio            0.2230         0.6384         1.0000
Interlocking                 0.1347         0.2450         0.6237
Pallets computed                  8          297.3           6070
-----------------------------------------------------------------
Center of Mass:
  CoM X                    522.7393       584.7226       673.7351
  CoM Y                    340.7828       388.4990       447.9066
  CoM Z                    157.8884      1123.3386      3875.6449
Relative Center of Mass:
  CoM X rel                  0.0000         0.0182         0.0644
  CoM Y rel                  0.0001         0.0177         0.0740
  CoM Z rel                  0.3703         0.4765         0.5524
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3334s
Avg time/test:    161 ms
===========================================================================






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





Thread 3, Iteration 832 - NEW BEST Score: 6.4485
Weights: {137, 3, 160, 80, 78, 122, 72, 160, 58, 39, 191}
Params:
  segment_small_probability:       0.2719
  segment_small_relative_len:      1.0000
  position_vs_rotation_probability:0.3815
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6133         0.7295         0.8631
Boxes                            18          116.8            465
Height                          342         2327.0           8313
Stability                    0.3779         0.7506         0.8620
Min support ratio            0.2451         0.6381         1.0000
Interlocking                 0.1380         0.2494         0.6221
Pallets computed                  9          295.9           6437
-----------------------------------------------------------------
Center of Mass:
  CoM X                    526.6364       586.0367       653.1293
  CoM Y                    326.4340       386.0863       428.6807
  CoM Z                    155.8725      1121.8356      3919.9838
Relative Center of Mass:
  CoM X rel                  0.0000         0.0160         0.0611
  CoM Y rel                  0.0002         0.0204         0.0920
  CoM Z rel                  0.3580         0.4750         0.5717
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3566s
Avg time/test:    161 ms
===========================================================================






Thread 18, Iteration 230 - NEW BEST Score: 6.3035
Weights: {52, 110, 70, 80, 100, 140, 100, 80, 213, 29, 126}
Params:
  segment_small_probability:       0.5401
  segment_small_relative_len:      1.0000
  position_vs_rotation_probability:0.3623
===========================================================================
                         SUMMARY STATISTICS
===========================================================================
Metric                          Min            Avg            Max
-----------------------------------------------------------------
Percolation                  0.6367         0.7393         0.8806
Boxes                            18          116.8            465
Height                          343         2289.8           8201
Stability                    0.2806         0.7661         0.8540
Min support ratio            0.2539         0.5963         1.0000
Interlocking                 0.1460         0.2339         0.7194
Pallets computed                 10          300.0           6046
-----------------------------------------------------------------
Center of Mass:
  CoM X                    507.9496       581.5001       648.0049
  CoM Y                    345.2545       386.1477       440.2090
  CoM Z                    160.1030      1130.3560      3902.0949
Relative Center of Mass:
  CoM X rel                  0.0002         0.0189         0.0767
  CoM Y rel                  0.0000         0.0200         0.0684
  CoM Z rel                  0.3752         0.4874         0.5614
-----------------------------------------------------------------

Total tests:      436
Total time:       70.3649s
Avg time/test:    161 ms
===========================================================================

*/