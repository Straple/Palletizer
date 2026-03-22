#include <solvers/lns/lns_solver.hpp>

#include <solvers/lns/pallet_simulation.hpp>
#include <utils/assert.hpp>
#include <utils/randomizer.hpp>

LNSSolver::LNSSolver(TestData test_data) : Solver(std::move(test_data)) {
}

LNSSolver::LNSSolver(TestData test_data, MutableParams params)
    : Solver(std::move(test_data)), mutable_params(std::move(params)) {
}

Answer LNSSolver::solve(TimePoint end_time) {
    Randomizer rnd;

    GenomHandler best(test_data);
    best.rebuild_all();
    pallets_computed = test_data.pallet_count;
    Pallet best_pallet = best.pallet_copy();

    while (get_now() < end_time) {
        GenomHandler trial(best);
        std::vector<bool> dirty;
        trial.mutate(rnd, best_pallet, mutable_params, &dirty);
        uint32_t n_dirty = 0;
        for (bool b: dirty) {
            if (b) {
                ++n_dirty;
            }
        }
        trial.recompute_with_mask(std::move(dirty));
        pallets_computed += n_dirty;

        if (trial.pallet().get_score(test_data.header) + 1e-6 > best_pallet.get_score(test_data.header)) {
            best_pallet = trial.pallet_copy();
            best = std::move(trial);
        }
    }
    return best.pallet().answer;
}
