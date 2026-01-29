#include <solvers/lns/genetic_solver.hpp>

#include <solvers/height_handler_rects.hpp>
#include <utils/randomizer.hpp>

#include <tuple>

GeneticSolver::GeneticSolver(TestData test_data) : Solver(test_data) {
}

Answer GeneticSolver::solve(TimePoint end_time) {
    return Answer();
}
