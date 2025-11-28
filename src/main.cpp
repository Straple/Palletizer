#include <utils/assert.hpp>
#include <utils/tools.hpp>

#include <iostream>
#include <fstream>

int main() {
    for (int test = 1;; test++) {
        std::ifstream input("tests/" + std::to_string(test) + ".csv");
        if (!input) {
            break;
        }

        std::cout << test << std::endl;

        std::ofstream output("tests_cleaned/" + std::to_string(test) + ".csv");

        int x;
        ASSERT((input >> x) && x == 1, "invalid test");
        std::string line;
        std::getline(input, line);

        std::string header;
        ASSERT(std::getline(input, header), "unable to read header");
        output << header << std::endl;
        ASSERT(header == "SKU,Quantity,Length,Width,Height,Weight,Strength,Aisle,Caustic", "invalid header");


        while (std::getline(input, line)) {
            auto data = split(line, ',');
            ASSERT(data.size() == 9, "invalid data");

            for(int i = 0; i < data.size(); i++){
                if(i != 0){
                    output << ',';
                }
                output << data[i];
            }
            output << std::endl;
        }
    }
}
