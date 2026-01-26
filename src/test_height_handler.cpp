#include <solvers/height_handler_rects.hpp>
#include <solvers/height_handler_rects_avx.hpp>
#include <solvers/height_handler_grid.hpp>
#include <solvers/height_handler_segtree.hpp>
#include <solvers/height_handler_quadtree.hpp>
#include <objects/test_data.hpp>
#include <utils/tools.hpp>
#include <utils/randomizer.hpp>
#include <utils/time.hpp>
#include <settings.hpp>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <mutex>
#include <thread>
// ======================= ЮНИТ ТЕСТЫ =======================

// Тест одной реализации с известными значениями
template<typename Handler>
bool run_unit_tests(const std::string& name) {
    bool passed = true;

    std::cout << "  " << name << ": ";

    // Тест 1: пустой handler
    {
        Handler h(100, 100);
        if (h.get_h(0, 0, 99, 99) != 0) {
            std::cerr << "\n    FAIL: empty handler should return 0\n";
            passed = false;
        }
        if (h.get_area(0, 0, 99, 99) != 100*100) {
            std::cerr << "\n    FAIL: empty handler area should be full\n";
            passed = false;
        }
    }

    // Тест 2: один прямоугольник
    {
        Handler h(100, 100);
        h.add_rect(10, 10, 29, 29, 50);  // 20x20 с высотой 50

        // Внутри прямоугольника
        if (h.get_h(10, 10, 29, 29) != 50) {
            std::cerr << "\n    FAIL: get_h inside rect should be 50, got " << h.get_h(10, 10, 29, 29) << "\n";
            passed = false;
        }

        // Снаружи прямоугольника
        if (h.get_h(0, 0, 9, 9) != 0) {
            std::cerr << "\n    FAIL: get_h outside rect should be 0\n";
            passed = false;
        }

        // Пересечение
        if (h.get_h(0, 0, 15, 15) != 50) {
            std::cerr << "\n    FAIL: get_h intersecting should be 50\n";
            passed = false;
        }
    }

    // Тест 3: два непересекающихся прямоугольника
    {
        Handler h(100, 100);
        h.add_rect(0, 0, 9, 9, 30);    // Первый: 10x10 с высотой 30
        h.add_rect(50, 50, 59, 59, 70); // Второй: 10x10 с высотой 70

        if (h.get_h(0, 0, 9, 9) != 30) {
            std::cerr << "\n    FAIL: first rect should be 30\n";
            passed = false;
        }

        if (h.get_h(50, 50, 59, 59) != 70) {
            std::cerr << "\n    FAIL: second rect should be 70\n";
            passed = false;
        }

        // Запрос охватывающий оба
        if (h.get_h(0, 0, 59, 59) != 70) {
            std::cerr << "\n    FAIL: query covering both should be max=70\n";
            passed = false;
        }
    }

    // Тест 4: перекрывающиеся прямоугольники (max операция)
    {
        Handler h(100, 100);
        h.add_rect(0, 0, 49, 49, 100);  // Первый
        h.add_rect(25, 25, 74, 74, 80); // Второй перекрывается

        // В области только первого
        if (h.get_h(0, 0, 24, 24) != 100) {
            std::cerr << "\n    FAIL: first only area should be 100\n";
            passed = false;
        }

        // В области перекрытия — max(100, 80) = 100
        if (h.get_h(25, 25, 49, 49) != 100) {
            std::cerr << "\n    FAIL: overlap area should be max=100\n";
            passed = false;
        }

        // В области только второго
        if (h.get_h(50, 50, 74, 74) != 80) {
            std::cerr << "\n    FAIL: second only area should be 80\n";
            passed = false;
        }
    }

    // Тест 5: get_area
    {
        Handler h(100, 100);
        h.add_rect(0, 0, 49, 49, 100);   // 50x50 с высотой 100
        h.add_rect(50, 0, 99, 49, 50);   // 50x50 с высотой 50

        // В запросе [0,0]-[99,49] максимум 100, площадь на этой высоте = 50*50 = 2500
        uint64_t area = h.get_area(0, 0, 99, 49);
        if (area != 2500) {
            std::cerr << "\n    FAIL: area at max height should be 2500, got " << area << "\n";
            passed = false;
        }
    }

    if (passed) {
        std::cout << "PASSED\n";
    }

    return passed;
}

void run_unit_tests_all() {
    std::cout << "=== UNIT TESTS ===\n\n";

    bool all_passed = true;

    all_passed &= run_unit_tests<HeightHandler>("HeightHandler (baseline)");
    all_passed &= run_unit_tests<HeightHandlerRects>("HeightHandlerRects");
    all_passed &= run_unit_tests<HeightHandlerRectsAVX>("HeightHandlerRectsAVX");
    all_passed &= run_unit_tests<HeightHandlerGridT<1, 1>>("HeightHandlerGrid<1,1>");
    all_passed &= run_unit_tests<HeightHandlerSegTreeT<1, 1>>("HeightHandlerSegTree<1,1>");
    all_passed &= run_unit_tests<HeightHandlerQuadtreeT<1>>("HeightHandlerQuadtree<1>");

    std::cout << "\n";
    if (all_passed) {
        std::cout << "All unit tests PASSED!\n";
    } else {
        std::cout << "Some unit tests FAILED!\n";
    }
    std::cout << "\n";
}

// ======================= ТЕСТЫ КОРРЕКТНОСТИ (СЛУЧАЙНЫЕ) =======================

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

    Handler1 h1(length, width);
    Handler2 h2(length, width);

    bool passed = true;

    for (size_t i = 0; i < ops.size(); ++i) {
        const auto& op = ops[i];

        uint32_t val1 = h1.get_h(op.x, op.y, op.X, op.Y);
        uint32_t val2 = h2.get_h(op.x, op.y, op.X, op.Y);

        if (val1 != val2) {
            std::cerr << "FAIL: get() mismatch at op " << i << ": "
                      << name1 << "=" << val1 << ", " << name2 << "=" << val2 << "\n";
            std::cerr << "  Query: [" << op.x << "," << op.y << "] - [" << op.X << "," << op.Y << "]\n";
            passed = false;
        }

        uint64_t area1 = h1.get_area(op.x, op.y, op.X, op.Y);
        uint64_t area2 = h2.get_area(op.x, op.y, op.X, op.Y);

        if (area1 != area2) {
            std::cerr << "FAIL: get_area() mismatch at op " << i << ": "
                      << name1 << "=" << area1 << ", " << name2 << "=" << area2 << "\n";
            passed = false;
        }

        h1.add_rect(op.x, op.y, op.X, op.Y, op.h);
        h2.add_rect(op.x, op.y, op.X, op.Y, op.h);
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

    std::cout << "Testing HeightHandlerRects vs HeightHandler... ";
    if (test_correctness<HeightHandlerRects, HeightHandler>(
            "Rects", "Baseline", length, width, ops)) {
        std::cout << "PASSED\n";
    } else {
        std::cout << "FAILED\n";
        all_passed = false;
    }

    std::cout << "Testing HeightHandlerGrid<1,1> vs HeightHandler... ";
    if (test_correctness<HeightHandlerGridT<1, 1>, HeightHandler>(
            "Grid<1,1>", "Baseline", length, width, ops)) {
        std::cout << "PASSED\n";
    } else {
        std::cout << "FAILED\n";
        all_passed = false;
    }

    std::cout << "Testing HeightHandlerRectsAVX vs HeightHandler... ";
    if (test_correctness<HeightHandlerRectsAVX, HeightHandler>(
            "RectsAVX", "Baseline", length, width, ops)) {
        std::cout << "PASSED\n";
    } else {
        std::cout << "FAILED\n";
        all_passed = false;
    }

    std::cout << "Testing HeightHandlerSegTree<1,1> vs HeightHandler... ";
    if (test_correctness<HeightHandlerSegTreeT<1, 1>, HeightHandler>(
            "SegTree<1,1>", "Baseline", length, width, ops)) {
        std::cout << "PASSED\n";
    } else {
        std::cout << "FAILED\n";
        all_passed = false;
    }

    std::cout << "Testing HeightHandlerQuadtree<1> vs HeightHandler... ";
    if (test_correctness<HeightHandlerQuadtreeT<1>, HeightHandler>(
            "Quadtree<1>", "Baseline", length, width, ops)) {
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
    double add_rect_ms = 0;
    double get_h_ms = 0;
    double get_area_ms = 0;
    double get_dots_ms = 0;
    double total_ms = 0;
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

// Структура для хранения предварительно вычисленных операций
struct BenchmarkStep {
    BoxSize box;
    std::vector<std::pair<uint32_t, uint32_t>> dots;  // Точки для проверки
    uint32_t best_x, best_y;                           // Лучшая позиция
    uint32_t final_height;                             // Высота для add_rect
};

// Предварительно вычисляем все операции используя HeightHandler (baseline)
std::vector<BenchmarkStep> precompute_benchmark_steps(
    const TestData& test_data,
    const std::vector<uint32_t>& box_order) {

    std::vector<BenchmarkStep> steps;
    HeightHandler reference(test_data.header.length, test_data.header.width);

    for (uint32_t box_idx : box_order) {
        const auto& box = test_data.boxes[box_idx];
        auto available_boxes = get_available_boxes(test_data.header, box);

        for (const auto& rotated_box : available_boxes) {
            BenchmarkStep step;
            step.box = rotated_box;
            step.dots = reference.get_dots(test_data.header, rotated_box);

            uint32_t best_score = -1;
            step.best_x = 0;
            step.best_y = 0;

            for (auto [x, y] : step.dots) {
                uint32_t h = reference.get_h(x, y, x + rotated_box.length - 1, y + rotated_box.width - 1);
                uint32_t score = h + rotated_box.height;
                if (score < best_score) {
                    best_score = score;
                    step.best_x = x;
                    step.best_y = y;
                }
            }

            if (best_score != (uint32_t)-1) {
                uint32_t h = reference.get_h(step.best_x, step.best_y,
                    step.best_x + rotated_box.length - 1, step.best_y + rotated_box.width - 1);
                step.final_height = h + rotated_box.height;

                reference.add_rect(step.best_x, step.best_y,
                    step.best_x + rotated_box.length - 1, step.best_y + rotated_box.width - 1,
                    step.final_height);

                steps.push_back(step);
                break;
            }
        }
    }

    return steps;
}

template<typename Handler>
double benchmark_total(Handler handler, const std::vector<BenchmarkStep>& steps) {
    Timer timer;
    volatile uint64_t sum_h = 0, sum_area = 0;
    for (const auto& step : steps) {
        for (auto [x, y] : step.dots) {
            sum_h += handler.get_h(
                x, y, x + step.box.length - 1, y + step.box.width - 1);
            sum_area += handler.get_area(
                x, y, x + step.box.length - 1, y + step.box.width - 1);
        }
        handler.add_rect(step.best_x, step.best_y,
            step.best_x + step.box.length - 1, step.best_y + step.box.width - 1,
            step.final_height);
    }
    (void)sum_h;
    (void)sum_area;
    return timer.get_ns() / 1'000'000.0;
}

template<typename Handler>
BenchmarkResult benchmark_handler(const std::string& name,
                                   Handler handler,
                                   const std::vector<BenchmarkStep>& steps) {
    BenchmarkResult result;
    result.name = name;

    Timer op_timer;

    uint64_t add_rect_ns = 0, get_h_ns = 0, get_area_ns = 0;

    volatile uint64_t sum_h = 0, sum_area = 0;

    for (const auto& step : steps) {
        // Проходим по всем предвычисленным точкам
        for (auto [x, y] : step.dots) {
            op_timer.reset();
            sum_h += handler.get_h(x, y, x + step.box.length - 1, y + step.box.width - 1);
            get_h_ns += op_timer.get_ns();

            op_timer.reset();
            sum_area += handler.get_area(x, y, x + step.box.length - 1, y + step.box.width - 1);
            get_area_ns += op_timer.get_ns();

            result.operations++;
        }
        op_timer.reset();
        handler.add_rect(step.best_x, step.best_y,
            step.best_x + step.box.length - 1, step.best_y + step.box.width - 1,
            step.final_height);
        add_rect_ns += op_timer.get_ns();
    }
    result.add_rect_ms = add_rect_ns / 1'000'000.0;
    result.get_h_ms = get_h_ns / 1'000'000.0;
    result.get_area_ms = get_area_ns / 1'000'000.0;
    result.get_dots_ms = 0;

    (void)sum_h;
    (void)sum_area;

    return result;
}

// Полный бенчмарк с чистым временем
template<typename Handler>
BenchmarkResult benchmark_handler_full(const std::string& name,
                                        uint32_t length, uint32_t width,
                                        const std::vector<BenchmarkStep>& steps) {
    BenchmarkResult result = benchmark_handler(name, Handler(length, width), steps);
    result.total_ms = benchmark_total(Handler(length, width), steps);
    return result;
}

void print_results_table(const std::vector<BenchmarkResult>& results) {
    // Заголовок
    std::cout << std::left << std::setw(20) << "Implementation"
              << std::right << std::setw(12) << "Total(ms)"
              << std::setw(12) << "add_rect"
              << std::setw(12) << "get_h"
              << std::setw(12) << "get_area"
              << std::setw(12) << "get_dots"
              << std::setw(12) << "Ops"
              << "\n";
    std::cout << std::string(92, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::left << std::setw(20) << r.name
                  << std::right << std::setw(12) << r.total_ms
                  << std::setw(12) << r.add_rect_ms
                  << std::setw(12) << r.get_h_ms
                  << std::setw(12) << r.get_area_ms
                  << std::setw(12) << r.get_dots_ms
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
              << std::setw(10) << "get_h"
              << std::setw(10) << "get_area"
              << "\n";
    std::cout << std::string(60, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::left << std::setw(20) << r.name
                  << std::right
                  << std::setw(10) << (baseline.total_ms / std::max(r.total_ms, 0.001)) << "x"
                  << std::setw(10) << (baseline.add_rect_ms / std::max(r.add_rect_ms, 0.001)) << "x"
                  << std::setw(10) << (baseline.get_h_ms / std::max(r.get_h_ms, 0.001)) << "x"
                  << std::setw(10) << (baseline.get_area_ms / std::max(r.get_area_ms, 0.001)) << "x"
                  << "\n";
    }
}

// ======================= МНОГОПОТОЧНЫЙ БЕНЧМАРК НА ВСЕХ ТЕСТАХ =======================

struct AggregatedResult {
    std::string name;
    double total_ms = 0;
    double add_rect_ms = 0;
    double get_h_ms = 0;
    double get_area_ms = 0;
    uint64_t operations = 0;
};

void run_benchmarks() {
    std::cout << "=== BENCHMARKS (Multithreaded, All Tests) ===\n\n";

    Timer total_timer;

    // Собираем все тесты
    std::vector<int> tests;
    for (int test = 1; ; test++) {
        std::ifstream input("tests/" + std::to_string(test) + ".csv");
        if (!input) break;
        tests.push_back(test);
    }

    uint32_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    std::cout << "Found " << tests.size() << " tests, using " << num_threads << " threads\n\n";

    // Результаты для каждого теста и каждого алгоритма
    std::vector<std::vector<BenchmarkResult>> all_results(tests.size());
    std::vector<std::atomic<bool>> visited(tests.size());
    std::mutex print_mutex;
    std::atomic<int> completed{0};

    // Многопоточный запуск
    launch_threads(num_threads, [&](uint32_t thr) {
        for (size_t i = 0; i < tests.size(); i++) {
            bool expected = false;
            if (!visited[i].compare_exchange_strong(expected, true)) {
                continue;
            }

            int test_num = tests[i];
            TestData test_data = load_test(test_num);

            if (test_data.boxes.empty()) continue;

            // Создаём порядок коробок
            std::vector<uint32_t> box_order;
            for (uint32_t j = 0; j < test_data.boxes.size(); j++) {
                for (uint32_t q = 0; q < test_data.boxes[j].quantity; q++) {
                    box_order.push_back(j);
                }
            }

            // Предвычисляем шаги
            auto steps = precompute_benchmark_steps(test_data, box_order);

            std::vector<BenchmarkResult> results;
            uint32_t len = test_data.header.length;
            uint32_t wid = test_data.header.width;

            // Все алгоритмы с полным бенчмарком
            results.push_back(benchmark_handler_full<HeightHandler>("Baseline", len, wid, steps));
            results.push_back(benchmark_handler_full<HeightHandlerRects>("Rects", len, wid, steps));
            results.push_back(benchmark_handler_full<HeightHandlerRectsAVX>("RectsAVX", len, wid, steps));
            results.push_back(benchmark_handler_full<HeightHandlerGrid>("Grid", len, wid, steps));
            results.push_back(benchmark_handler_full<HeightHandlerSegTree>("SegTree", len, wid, steps));
            results.push_back(benchmark_handler_full<HeightHandlerQuadtree>("Quadtree", len, wid, steps));

            all_results[i] = results;

            int done = ++completed;

            {
                std::lock_guard lock(print_mutex);
                std::cout << "Test " << std::setw(3) << test_num
                          << " (" << std::setw(3) << box_order.size() << " boxes, "
                          << std::setw(5) << results[0].operations << " ops): ";
                for (const auto& r : results) {
                    std::cout << r.name << "=" << std::fixed << std::setprecision(1)
                              << r.total_ms << "ms ";
                }
                std::cout << "[" << done << "/" << tests.size() << "]\n";
            }
        }
    });

    // Агрегируем результаты
    std::vector<AggregatedResult> aggregated(6);  // 6 алгоритмов
    aggregated[0].name = "Baseline";
    aggregated[1].name = "Rects";
    aggregated[2].name = "RectsAVX";
    aggregated[3].name = "Grid";
    aggregated[4].name = "SegTree";
    aggregated[5].name = "Quadtree";

    for (const auto& test_results : all_results) {
        for (size_t j = 0; j < test_results.size() && j < aggregated.size(); ++j) {
            aggregated[j].total_ms += test_results[j].total_ms;
            aggregated[j].add_rect_ms += test_results[j].add_rect_ms;
            aggregated[j].get_h_ms += test_results[j].get_h_ms;
            aggregated[j].get_area_ms += test_results[j].get_area_ms;
            aggregated[j].operations += test_results[j].operations;
        }
    }

    // Выводим SUMMARY
    std::cout << "\n";
    std::cout << "=============================================================================\n";
    std::cout << "                              SUMMARY\n";
    std::cout << "=============================================================================\n";
    std::cout << "Tests: " << tests.size() << ", Wall time: " << std::fixed << std::setprecision(1)
              << total_timer.get_ms() << "ms, Threads: " << num_threads << "\n\n";

    // Таблица результатов
    std::cout << std::left << std::setw(15) << "Algorithm"
              << std::right << std::setw(14) << "Total"
              << std::setw(12) << "add_rect"
              << std::setw(12) << "get_h"
              << std::setw(12) << "get_area"
              << std::setw(12) << "Ops"
              << std::setw(10) << "Speedup"
              << "\n";
    std::cout << std::string(87, '-') << "\n";

    double baseline_total = aggregated[0].total_ms;

    for (const auto& r : aggregated) {
        double speedup = baseline_total / std::max(r.total_ms, 0.001);
        std::cout << std::fixed << std::setprecision(1);
        std::cout << std::left << std::setw(15) << r.name
                  << std::right << std::setw(14) << r.total_ms
                  << std::setw(12) << r.add_rect_ms
                  << std::setw(12) << r.get_h_ms
                  << std::setw(12) << r.get_area_ms
                  << std::setw(12) << r.operations
                  << std::setw(8) << std::setprecision(2) << speedup << "x"
                  << "\n";
    }

    // Детальные speedup по операциям
    std::cout << "\nSpeedup vs Baseline:\n";
    std::cout << std::string(75, '-') << "\n";
    std::cout << std::left << std::setw(15) << "Algorithm"
              << std::right << std::setw(12) << "Total"
              << std::setw(12) << "add_rect"
              << std::setw(12) << "get_h"
              << std::setw(12) << "get_area"
              << "\n";
    std::cout << std::string(75, '-') << "\n";

    // Форматируем speedup как строку с "x"
    auto format_speedup = [](double base, double val) -> std::string {
        double speedup = base / std::max(val, 0.001);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << speedup << "x";
        return oss.str();
    };

    for (const auto& r : aggregated) {
        std::cout << std::fixed << std::setprecision(2);

        std::cout << std::left << std::setw(15) << r.name
                  << std::right
                  << std::setw(12) << format_speedup(aggregated[0].total_ms, r.total_ms)
                  << std::setw(12) << format_speedup(aggregated[0].add_rect_ms, r.add_rect_ms)
                  << std::setw(12) << format_speedup(aggregated[0].get_h_ms, r.get_h_ms)
                  << std::setw(12) << format_speedup(aggregated[0].get_area_ms, r.get_area_ms)
                  << "\n";
    }

    std::cout << "\n";
}

int main() {
    std::cout << "HeightHandler Test Suite\n";
    std::cout << "========================\n\n";

    // Юнит тесты
    run_unit_tests_all();

    // Тесты корректности (случайные)
    run_correctness_tests();

    std::cout << "\n";

    // Многопоточный бенчмарк на всех тестах
    run_benchmarks();

    return 0;
}
