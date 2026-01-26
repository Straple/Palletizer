#pragma once

#include <objects/answer.hpp>
#include <objects/test_data.hpp>
#include <utils/time.hpp>

class Solver {
protected:
    TestData test_data;
    uint64_t pallets_computed = 0;  // Количество вычисленных паллет

public:
    explicit Solver(TestData test_data);

    virtual Answer solve(TimePoint end_time);
    
    // Получить количество вычисленных паллет
    [[nodiscard]] uint64_t get_pallets_computed() const { return pallets_computed; }
};
