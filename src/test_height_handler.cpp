// Тесты и бенчмарки для HeightHandler реализаций
// Компиляция: cmake && make test_height_handler
// Запуск: ./bin/test_height_handler

#include <solvers/height_handler_rects.hpp>
#include <solvers/height_handler_grid.hpp>
#include <solvers/height_handler_segtree.hpp>
#include <solvers/height_handler_quadtree.hpp>
#include <objects/test_data.hpp>
#include <utils/tools.hpp>
#include <utils/randomizer.hpp>
#include <utils/time.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <memory>
#include <functional>
#include <cassert>

// ======================= ТЕСТЫ КОРРЕКТНОСТИ =======================

// Генератор случайных прямоугольников
struct RectOperation {
    uint32_t x, y, X, Y, h;
};

std::vector<RectOperation> generate_random_operations(uint32_t length, uint32_t width, uint32_t count, Randomizer& rnd) {
    std::vector<RectOperation> ops;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t x = rnd.get(0, length - 1);
        uint32_t X = rnd.get(x, length - 1);
        uint32_t y = rnd.get(0, width - 1);
        uint32_t Y = rnd.get(y, width - 1);
        uint32_t h = rnd.get(1, 2000);
        ops.push_back({x, y, X, Y, h});
    }
    return ops;
}

// Тест корректности: сравниваем две реализации
template<typename Handler1, typename Handler2>
bool test_correctness(const std::string& name1, const std::string& name2,
                      uint32_t length, uint32_t width, 
                      const std::vector<RectOperation>& ops) {
    
    // Создаём Handler1 — требует конструктор (length, width)
    std::unique_ptr<HeightHandler> h1, h2;
    
    // Все реализации теперь имеют одинаковый конструктор (length, width)
    h1 = std::make_unique<Handler1>(length, width);
    h2 = std::make_unique<Handler2>(length, width);
    
    bool passed = true;
    
    for (size_t i = 0; i < ops.size(); ++i) {
        const auto& op = ops[i];
        
        // Проверяем get перед добавлением
        uint32_t val1 = h1->get(op.x, op.y, op.X, op.Y);
        uint32_t val2 = h2->get(op.x, op.y, op.X, op.Y);
        
        if (val1 != val2) {
            std::cerr << "FAIL: get() mismatch at op " << i << ": "
                      << name1 << "=" << val1 << ", " << name2 << "=" << val2 << "\n";
            std::cerr << "  Query: [" << op.x << "," << op.y << "] - [" << op.X << "," << op.Y << "]\n";
            passed = false;
        }
        
        // Проверяем get_area_at_max_height
        uint64_t area1 = h1->get_area_at_max_height(op.x, op.y, op.X, op.Y);
        uint64_t area2 = h2->get_area_at_max_height(op.x, op.y, op.X, op.Y);
        
        if (area1 != area2) {
            std::cerr << "FAIL: get_area_at_max_height() mismatch at op " << i << ": "
                      << name1 << "=" << area1 << ", " << name2 << "=" << area2 << "\n";
            passed = false;
        }
        
        // Добавляем прямоугольник
        h1->add_rect(op.x, op.y, op.X, op.Y, op.h);
        h2->add_rect(op.x, op.y, op.X, op.Y, op.h);
    }
    
    return passed;
}

void run_correctness_tests() {
    std::cout << "=== CORRECTNESS TESTS ===\n\n";
    
    Randomizer rnd;
    uint32_t length = 1200, width = 800;
    uint32_t num_ops = 100;
    
    auto ops = generate_random_operations(length, width, num_ops, rnd);
    
    // Сравниваем все реализации с эталонной (HeightHandlerRects)
    bool all_passed = true;
    
    std::cout << "Testing HeightHandlerGrid<1,1> vs HeightHandlerRects... ";
    if (test_correctness<HeightHandlerGridT<1, 1>, HeightHandlerRects>(
            "Grid<1,1>", "Rects", length, width, ops)) {
        std::cout << "PASSED\n";
    } else {
        std::cout << "FAILED\n";
        all_passed = false;
    }
    
    std::cout << "Testing HeightHandlerSegTree<1,1> vs HeightHandlerRects... ";
    if (test_correctness<HeightHandlerSegTreeT<1, 1>, HeightHandlerRects>(
            "SegTree<1,1>", "Rects", length, width, ops)) {
        std::cout << "PASSED\n";
    } else {
        std::cout << "FAILED\n";
        all_passed = false;
    }
    
    std::cout << "Testing HeightHandlerQuadtree<1> vs HeightHandlerRects... ";
    if (test_correctness<HeightHandlerQuadtreeT<1>, HeightHandlerRects>(
            "Quadtree<1>", "Rects", length, width, ops)) {
        std::cout << "PASSED\n";
    } else {
        std::cout << "FAILED\n";
        all_passed = false;
    }
    
    std::cout << "\n";
    if (all_passed) {
        std::cout << "All correctness tests PASSED!\n";
    } else {
        std::cout << "Some correctness tests FAILED!\n";
    }
    std::cout << "\n";
}

// ======================= БЕНЧМАРКИ =======================

struct BenchmarkResult {
    std::string name;
    double add_rect_time_ms = 0;
    double get_time_ms = 0;
    double get_area_time_ms = 0;
    double get_dots_time_ms = 0;
    double total_time_ms = 0;
    uint64_t operations = 0;
};

// Загрузить данные теста
TestData load_test(int test_num) {
    std::ifstream input("tests/" + std::to_string(test_num) + ".csv");
    TestData test_data;
    if (input) {
        input >> test_data;
    }
    return test_data;
}

// Универсальный бенчмарк
template<typename HandlerFactory>
BenchmarkResult benchmark_handler(const std::string& name,
                                   const TestData& test_data, 
                                   const std::vector<uint32_t>& box_order,
                                   HandlerFactory factory) {
    BenchmarkResult result;
    result.name = name;
    
    Timer total_timer;
    Timer add_rect_timer, get_timer, get_area_timer, get_dots_timer;
    
    double add_rect_time = 0, get_time = 0, get_area_time = 0, get_dots_time = 0;
    
    auto height_handler = factory(test_data.header.length, test_data.header.width);
    
    for (uint32_t box_idx : box_order) {
        const auto& box = test_data.boxes[box_idx];
        auto available_boxes = get_available_boxes(test_data.header, box);
        
        for (const auto& rotated_box : available_boxes) {
            // get_dots
            get_dots_timer.reset();
            auto dots = height_handler->get_dots(test_data.header, rotated_box);
            get_dots_time += get_dots_timer.get_ms();
            
            uint32_t best_score = -1;
            uint32_t best_x = 0, best_y = 0;
            
            for (auto [x, y] : dots) {
                // get
                get_timer.reset();
                uint32_t h = height_handler->get(x, y, x + rotated_box.length - 1, y + rotated_box.width - 1);
                get_time += get_timer.get_ms();
                
                // get_area_at_max_height
                get_area_timer.reset();
                [[maybe_unused]] uint64_t area = height_handler->get_area_at_max_height(
                    x, y, x + rotated_box.length - 1, y + rotated_box.width - 1);
                get_area_time += get_area_timer.get_ms();
                
                uint32_t score = h + rotated_box.height;
                if (score < best_score) {
                    best_score = score;
                    best_x = x;
                    best_y = y;
                }
                result.operations++;
            }
            
            if (best_score != (uint32_t)-1) {
                uint32_t h = height_handler->get(best_x, best_y, 
                    best_x + rotated_box.length - 1, best_y + rotated_box.width - 1);
                
                // add_rect
                add_rect_timer.reset();
                height_handler->add_rect(best_x, best_y, 
                    best_x + rotated_box.length - 1, best_y + rotated_box.width - 1, h + rotated_box.height);
                add_rect_time += add_rect_timer.get_ms();
                
                break;
            }
        }
    }
    
    result.add_rect_time_ms = add_rect_time;
    result.get_time_ms = get_time;
    result.get_area_time_ms = get_area_time;
    result.get_dots_time_ms = get_dots_time;
    result.total_time_ms = total_timer.get_ms();
    
    return result;
}

void print_results_table(const std::vector<BenchmarkResult>& results) {
    // Заголовок
    std::cout << std::left << std::setw(20) << "Implementation"
              << std::right << std::setw(12) << "Total(ms)"
              << std::setw(12) << "add_rect"
              << std::setw(12) << "get"
              << std::setw(12) << "get_area"
              << std::setw(12) << "get_dots"
              << std::setw(12) << "Ops"
              << "\n";
    std::cout << std::string(92, '-') << "\n";
    
    for (const auto& r : results) {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::left << std::setw(20) << r.name
                  << std::right << std::setw(12) << r.total_time_ms
                  << std::setw(12) << r.add_rect_time_ms
                  << std::setw(12) << r.get_time_ms
                  << std::setw(12) << r.get_area_time_ms
                  << std::setw(12) << r.get_dots_time_ms
                  << std::setw(12) << r.operations
                  << "\n";
    }
}

void print_speedup_table(const std::vector<BenchmarkResult>& results) {
    if (results.empty()) return;
    
    const auto& baseline = results[0];  // Первая реализация — базовая
    
    std::cout << "\nSpeedup vs " << baseline.name << ":\n";
    std::cout << std::left << std::setw(20) << "Implementation"
              << std::right << std::setw(10) << "Total"
              << std::setw(10) << "add_rect"
              << std::setw(10) << "get"
              << std::setw(10) << "get_area"
              << "\n";
    std::cout << std::string(60, '-') << "\n";
    
    for (const auto& r : results) {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::left << std::setw(20) << r.name
                  << std::right 
                  << std::setw(10) << (baseline.total_time_ms / std::max(r.total_time_ms, 0.001)) << "x"
                  << std::setw(10) << (baseline.add_rect_time_ms / std::max(r.add_rect_time_ms, 0.001)) << "x"
                  << std::setw(10) << (baseline.get_time_ms / std::max(r.get_time_ms, 0.001)) << "x"
                  << std::setw(10) << (baseline.get_area_time_ms / std::max(r.get_area_time_ms, 0.001)) << "x"
                  << "\n";
    }
}

void run_benchmarks() {
    std::cout << "=== BENCHMARKS ===\n\n";
    
    Randomizer rnd;
    
    // Тесты для бенчмарка
    std::vector<int> test_nums = {1, 10, 100, 200};
    
    for (int test_num : test_nums) {
        TestData test_data = load_test(test_num);
        if (test_data.boxes.empty()) {
            std::cout << "Test " << test_num << ": not found\n\n";
            continue;
        }
        
        // Создаём порядок коробок
        std::vector<uint32_t> box_order;
        for (uint32_t i = 0; i < test_data.boxes.size(); i++) {
            for (uint32_t q = 0; q < test_data.boxes[i].quantity; q++) {
                box_order.push_back(i);
            }
        }
        
        // Перемешиваем
        std::shuffle(box_order.begin(), box_order.end(), rnd.generator);
        
        std::cout << "Test " << test_num << " (" << box_order.size() << " boxes):\n";
        std::cout << std::string(60, '=') << "\n";
        
        std::vector<BenchmarkResult> results;
        
        // HeightHandlerRects
        results.push_back(benchmark_handler("HeightHandlerRects", test_data, box_order,
            [](uint32_t length, uint32_t width) {
                return std::make_unique<HeightHandlerRects>(length, width);
            }));
        
        // HeightHandlerGrid
        results.push_back(benchmark_handler("HeightHandlerGrid", test_data, box_order,
            [](uint32_t length, uint32_t width) {
                return std::make_unique<HeightHandlerGrid>(length, width);
            }));
        
        // HeightHandlerSegTree
        results.push_back(benchmark_handler("HeightHandlerSegTree", test_data, box_order,
            [](uint32_t length, uint32_t width) {
                return std::make_unique<HeightHandlerSegTree>(length, width);
            }));
        
        // HeightHandlerQuadtree
        results.push_back(benchmark_handler("HeightHandlerQuadtree", test_data, box_order,
            [](uint32_t length, uint32_t width) {
                return std::make_unique<HeightHandlerQuadtree>(length, width);
            }));
        
        print_results_table(results);
        print_speedup_table(results);
        
        std::cout << "\n\n";
    }
}

int main() {
    std::cout << "HeightHandler Test Suite\n";
    std::cout << "========================\n\n";
    
    // Тесты корректности
    run_correctness_tests();
    
    std::cout << "\n";
    
    // Бенчмарки
    run_benchmarks();
    
    return 0;
}
