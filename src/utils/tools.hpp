#pragma once

#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <utils/assert.hpp>

std::vector<std::string> split(const std::string &str, char separator);

template<typename T>
T parse(const std::string &str) {
    T value{};
    std::stringstream input(str);
    input >> value;
    std::stringstream output;
    output << value;
    ASSERT(output.str() == str, "failed to parse object from str: >" + str + "<, got: >" + output.str() + "<");
    return value;
}

template<typename foo_t>
void launch_threads(const uint32_t threads_num, foo_t &&foo) {
    std::vector<std::thread> threads(threads_num);
    for (uint32_t thr = 0; thr < threads.size(); thr++) {
        threads[thr] = std::thread(foo, thr);
    }
    for (uint32_t thr = 0; thr < threads.size(); thr++) {
        threads[thr].join();
    }
}
