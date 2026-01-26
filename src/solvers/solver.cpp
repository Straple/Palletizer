#include <solvers/solver.hpp>

Solver::Solver(TestData test_data) : test_data(std::move(test_data)) {
}

Answer Solver::solve(TimePoint end_time) {
    Answer answer;
    uint32_t h = 0;
    for (auto box: test_data.boxes) {
        for (uint32_t q = 0; q < box.quantity; q++) {
            Position pos = {
                    box.sku,
                    0,
                    0,
                    h,
                    box.length,
                    box.width,
                    h + box.height,
            };
            h += box.height;
            answer.positions.push_back(pos);
        }
    }
    pallets_computed = 1;
    return answer;
}
