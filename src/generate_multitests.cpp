#include <utils/assert.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main() {
    uint64_t seed = 12345;
    std::string tests_dir = "tests";
    std::string out_root = "multitests";

    std::vector<uint32_t> test_ids;
    ASSERT(fs::is_directory(tests_dir), "tests directory missing: " + tests_dir);
    for (const auto &entry: fs::directory_iterator(tests_dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".csv") {
            continue;
        }
        std::string stem = entry.path().stem().string();
        try {
            test_ids.push_back(static_cast<uint32_t>(std::stoul(stem)));
        } catch (...) {
            continue;
        }
    }
    ASSERT(!test_ids.empty(), "no tests/*.csv found");
    std::sort(test_ids.begin(), test_ids.end());
    const auto num_tests = static_cast<uint32_t>(test_ids.size());
    const auto max_k = std::min<uint32_t>(6, num_tests);

    fs::create_directories(out_root);

    for (uint32_t x = 0; x < num_tests; x++) {
        uint32_t multitest_id = test_ids[x];
        std::mt19937_64 rng(seed ^ (static_cast<uint64_t>(multitest_id) * 0x9e3779b97f4a7c15ULL));
        std::uniform_int_distribution<uint32_t> dist_k(2, max_k);
        uint32_t k = dist_k(rng);

        std::set<uint32_t> picked;
        std::uniform_int_distribution<uint32_t> pick_idx(0, num_tests - 1);
        while (picked.size() < k) {
            picked.insert(test_ids[pick_idx(rng)]);
        }

        fs::path out_dir = fs::path(out_root) / std::to_string(multitest_id);
        fs::create_directories(out_dir);

        for (uint32_t tid: picked) {
            fs::path src = fs::path(tests_dir) / (std::to_string(tid) + ".csv");
            fs::path dst = out_dir / (std::to_string(tid) + ".csv");
            ASSERT(fs::exists(src), "missing source test file");
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        }
    }

    std::cout << "Generated multitests in >" << out_root << "< for " << num_tests << " folders (seed=" << seed
              << ").\n";
    return 0;
}
