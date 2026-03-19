#include <solvers/lns/genetic_solver.hpp>

#include <solvers/lns/pallet_simulation.hpp>
#include <utils/assert.hpp>
#include <utils/randomizer.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace {

    struct Evaluated {
        SimulationParams params;
        SimulationResult result;
        double score = 0;
    };

}// namespace

GeneticSolver::GeneticSolver(TestData test_data) : Solver(std::move(test_data)) {
}

GeneticSolver::GeneticSolver(TestData test_data, MutableParams params)
    : Solver(std::move(test_data)), mutable_params(std::move(params)) {
}

Answer GeneticSolver::solve(TimePoint end_time) {
    Randomizer rnd;
    constexpr uint32_t kPop = 24;

    std::vector<Evaluated> population;
    population.resize(kPop);
    for (uint32_t i = 0; i < kPop; i++) {
        population[i].params = make_initial_simulation_params(test_data);
        for (auto &row: population[i].params.order) {
            std::shuffle(row.begin(), row.end(), rnd.generator);
        }
        population[i].result = run_simulation(test_data, population[i].params);
        population[i].score = population[i].result.get_score(test_data.header);
    }

    auto cmp = [](const Evaluated &a, const Evaluated &b) { return a.score > b.score; };

    std::sort(population.begin(), population.end(), cmp);
    Evaluated best = population[0];
    pallets_computed = kPop;

    while (get_now() < end_time) {
        std::vector<Evaluated> next;
        next.reserve(kPop);
        next.push_back(best);

        while (next.size() < kPop) {
            uint32_t top = std::min<uint32_t>(8, static_cast<uint32_t>(population.size()));
            uint32_t pidx = rnd.get(0, top - 1);
            Evaluated child;
            child.params = population[pidx].params;
            child.params.mutate(rnd, test_data, population[pidx].result, mutable_params);
            child.result = run_simulation(test_data, child.params);
            child.score = child.result.get_score(test_data.header);
            next.push_back(std::move(child));
            pallets_computed++;
        }

        population = std::move(next);
        std::sort(population.begin(), population.end(), cmp);
        if (population[0].score > best.score + 1e-9) {
            best = population[0];
        }
    }

    ASSERT(best.result.metrics.unable_to_put_boxes == 0,
           "unable to put some boxes: " + std::to_string(best.result.metrics.unable_to_put_boxes));
    return best.result.answer;
}
