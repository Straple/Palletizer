#include <utils/randomizer.hpp>

Randomizer::Randomizer() : generator(42) {
}

Randomizer::Randomizer(uint64_t seed) : generator(seed) {
}

uint64_t Randomizer::get() {
    std::uniform_int_distribution<uint64_t> distrib;
    return distrib(generator);
}

int64_t Randomizer::get(int64_t left, int64_t right) {
    ASSERT(left <= right, "invalid segment");
    std::uniform_int_distribution<int64_t> distrib(left, right);
    return distrib(generator);
}

double Randomizer::get_d() {
    double p = static_cast<double>(get()) / static_cast<double>(UINT64_MAX);
    ASSERT(0 <= p && p <= 1, "invalid result");
    return p;
}

double Randomizer::get_d(double left, double right) {
    ASSERT(left <= right, "invalid segment");
    double p = (get_d() * (right - left)) + left;
    ASSERT(left <= p && p <= right, "invalid result");
    return p;
}

uint32_t Randomizer::get(const std::vector<uint32_t> &w) {
    uint32_t sum = std::accumulate(w.begin(), w.end(), 0);
    uint32_t x = get(0, sum - 1);
    for (uint32_t i = 0; i < w.size(); i++) {
        if (x < w[i]) {
            return i;
        }
        x -= w[i];
    }
    ASSERT(false, "invalid get");
    return -1;
}
