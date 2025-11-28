#include <utils/assert.hpp>
#include <utils/tools.hpp>
#include <objects/test_data.hpp>

#include <iostream>
#include <fstream>

int main() {
    for (int test = 1;; test++) {
        std::ifstream input("tests/" + std::to_string(test) + ".csv");
        if (!input) {
            break;
        }

        std::cout << test << std::endl;

        TestData test_data;
        input >> test_data;
    }
}
