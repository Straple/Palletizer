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

    SimulationParams params = make_initial_simulation_params(test_data);

    pallets_computed = 1;
    SimulationResult best_result = run_simulation(test_data, params);

    while (get_now() < end_time) {
        SimulationParams new_params = params;
        new_params.mutate(rnd, test_data, best_result, mutable_params);

        SimulationResult new_result = run_simulation(test_data, new_params);

        if (new_result.get_score(test_data.header) + 1e-6 > best_result.get_score(test_data.header)) {
            best_result = new_result;
            params = new_params;
        }
        pallets_computed++;
    }
    ASSERT(best_result.metrics.unable_to_put_boxes == 0,
           "unable to put some boxes: " + std::to_string(best_result.metrics.unable_to_put_boxes));
    return best_result.answer;
}
