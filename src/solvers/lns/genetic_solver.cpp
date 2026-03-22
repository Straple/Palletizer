#include <solvers/lns/genetic_solver.hpp>

#include <solvers/lns/pallet_simulation.hpp>
#include <utils/assert.hpp>
#include <utils/randomizer.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace {

    struct Evaluated {
        GenomHandler handler;
        double score = 0;

        explicit Evaluated(const TestData &test_data) : handler(test_data) {
        }
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
    population.reserve(kPop);
    for (uint32_t i = 0; i < kPop; i++) {
        population.emplace_back(test_data);
        for (auto &row: population.back().handler.order) {
            std::shuffle(row.begin(), row.end(), rnd.generator);
        }
        population.back().handler.rebuild_all();
        population.back().score = population.back().handler.pallet().get_score(test_data.header);
    }

    auto cmp = [](const Evaluated &a, const Evaluated &b) { return a.score > b.score; };

    std::sort(population.begin(), population.end(), cmp);
    Evaluated best = population[0];
    pallets_computed = static_cast<uint64_t>(kPop) * test_data.pallet_count;

    while (get_now() < end_time) {
        std::vector<Evaluated> next;
        next.reserve(kPop);
        next.push_back(best);

        while (next.size() < kPop) {
            uint32_t top = std::min<uint32_t>(8, static_cast<uint32_t>(population.size()));
            uint32_t pidx = rnd.get(0, top - 1);
            Evaluated child(population[pidx]);
            child.handler.mutate(rnd, population[pidx].handler.pallet(), mutable_params);
            child.handler.rebuild_all();
            child.score = child.handler.pallet().get_score(test_data.header);
            next.push_back(std::move(child));
            pallets_computed += test_data.pallet_count;
        }

        population = std::move(next);
        std::sort(population.begin(), population.end(), cmp);
        if (population[0].score > best.score + 1e-9) {
            best = population[0];
        }
    }

    return best.handler.pallet().answer;
}
