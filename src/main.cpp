#include <utils/assert.hpp>
#include <utils/tools.hpp>
#include <objects/metrics.hpp>
#include <solvers/solver.hpp>

#include <iostream>
#include <fstream>

int main() {
    std::ofstream metrics_output("answers/metrics.csv");
    metrics_output << "test,boxes_num,length,width,height,boxes_volume,pallet_volume,relative_volume\n";
    double total_relative_volume = 0;
    int test = 1;
    for (;; test++) {
        std::ifstream input("tests/" + std::to_string(test) + ".csv");
        if (!input) {
            break;
        }

        TestData test_data;
        input >> test_data;

        // std::cout << test << ' ' << test_data.boxes.size() << std::endl;

        Answer answer = Solver(test_data).solve(get_now() + Milliseconds(1'000));

        std::ofstream output("answers/" + std::to_string(test) + ".csv");
        output << answer;

        Metrics metrics = calc_metrics(test_data, answer);

        std::cout << test << std::endl;

        metrics_output << test << ',' << test_data.boxes.size() << ',' << test_data.length << ',' << test_data.width
                       << ',' << metrics.height << ',' << metrics.boxes_volume << ',' << metrics.pallet_volume << ','
                       << metrics.relative_volume << '\n';

        total_relative_volume += metrics.relative_volume;
    }

    std::cout << "Total relative volume: " << total_relative_volume / test << '\n';
}
