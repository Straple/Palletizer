#include <solvers/solver.hpp>

Solver::Solver(TestData test_data) : test_data(std::move(test_data)) {
}

Answer Solver::solve(TimePoint /*end_time*/) {
    Answer answer;
    const uint32_t pc = test_data.pallet_count;
    answer.pallets.assign(pc, {});

    uint32_t p = 0;
    for (auto box: test_data.boxes) {
        for (uint32_t q = 0; q < box.quantity; q++) {
            uint32_t idx = p % pc;
            uint32_t h = 0;
            if (!answer.pallets[idx].empty()) {
                h = answer.pallets[idx].back().Z;
            }
            Position pos = {
                    box.sku,
                    0,
                    0,
                    h,
                    box.length,
                    box.width,
                    h + box.height,
            };
            answer.pallets[idx].push_back(pos);
            p++;
        }
    }
    pallets_computed = 1;
    return answer;
}
