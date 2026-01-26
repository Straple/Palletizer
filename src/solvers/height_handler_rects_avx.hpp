#pragma once

#include <solvers/height_handler.hpp>
#include <solvers/height_handler_rects.hpp>
#include <vector>
#include <algorithm>
#include <cstdint>

#ifdef __AVX2__
#include <immintrin.h>
#endif

// HeightHandlerRectsAVX — версия HeightHandlerRects с SIMD оптимизациями
// Хранит прямоугольники в SoA формате для эффективной векторизации
// Использует AVX2 для проверки 8 прямоугольников за раз
class HeightHandlerRectsAVX : public HeightHandler {
    // SoA (Structure of Arrays) для SIMD-friendly доступа
    // Выровнены по 32 байта для AVX2
    alignas(32) std::vector<uint32_t> rect_x;  // левая граница
    alignas(32) std::vector<uint32_t> rect_X;  // правая граница
    alignas(32) std::vector<uint32_t> rect_y;  // нижняя граница
    alignas(32) std::vector<uint32_t> rect_Y;  // верхняя граница
    alignas(32) std::vector<uint32_t> rect_h;  // высота
    
    // Также храним как AoS для subtract операций
    std::vector<HeightRect> height_rects;

public:
    HeightHandlerRectsAVX(uint32_t length, uint32_t width) {
        HeightRect floor{0, 0, length - 1, width - 1, 0};
        add_to_soa(floor);
        height_rects.push_back(floor);
    }
    
    [[nodiscard]] uint32_t get_h(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override {
// #ifdef __AVX2__
        return get_avx(x, y, X, Y);
// #else
        return get_scalar(x, y, X, Y);
// #endif
    }
    
    [[nodiscard]] uint64_t get_area(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const override {
        uint64_t area = 0;
        uint32_t max_height = 0;
        bool found_max = false;
        
        // Прямоугольники отсортированы по h↓, первый найденный = максимум
        size_t n = rect_h.size();
        for (size_t i = 0; i < n; ++i) {
            // Проверяем пересечение
            if (rect_x[i] <= X && x <= rect_X[i] && rect_y[i] <= Y && y <= rect_Y[i]) {
                if (!found_max) {
                    max_height = rect_h[i];
                    found_max = true;
                }
                
                if (rect_h[i] == max_height) {
                    uint32_t ix = std::max(x, rect_x[i]);
                    uint32_t iX = std::min(X, rect_X[i]);
                    uint32_t iy = std::max(y, rect_y[i]);
                    uint32_t iY = std::min(Y, rect_Y[i]);
                    
                    area += static_cast<uint64_t>(iX - ix + 1) * (iY - iy + 1);
                } else {
                    break;  // Прямоугольники отсортированы по h↓
                }
            }
        }
        
        return area;
    }

    void add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) override {
        HeightRect new_rect{x, y, X, Y, h};
        
        if (!new_rect.is_valid()) {
            return;
        }

        std::vector<HeightRect> new_height_rects;

        for (const auto& existing : height_rects) {
            if (!existing.intersects(new_rect)) {
                new_height_rects.push_back(existing);
            } else if (existing.h > new_rect.h) {
                new_height_rects.push_back(existing);
            } else {
                auto parts = subtract(existing, new_rect);
                for (const auto& part : parts) {
                    new_height_rects.push_back(part);
                }
            }
        }

        std::vector<HeightRect> new_parts = {new_rect};

        for (const auto& existing : height_rects) {
            if (existing.h > new_rect.h && existing.intersects(new_rect)) {
                std::vector<HeightRect> updated_parts;
                for (const auto& part : new_parts) {
                    auto subtracted = subtract(part, existing);
                    for (const auto& s : subtracted) {
                        updated_parts.push_back(s);
                    }
                }
                new_parts = std::move(updated_parts);
            }
        }

        for (const auto& part : new_parts) {
            new_height_rects.push_back(part);
        }

        // Сортируем по убыванию высоты
        std::sort(new_height_rects.begin(), new_height_rects.end(),
                  [](const HeightRect& a, const HeightRect& b) {
                      return a.h > b.h;
                  });
        
        height_rects = std::move(new_height_rects);
        
        // Обновляем SoA представление
        rebuild_soa();
    }

    [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> get_dots(
        const TestDataHeader& header, const BoxSize& box) const override {
        
        std::vector<std::pair<uint32_t, uint32_t>> result;
        for (const auto& rect : height_rects) {
            std::vector<std::pair<uint32_t, uint32_t>> dots = {
                {rect.x, rect.y},
                {rect.x, rect.Y + 1},
                {rect.X + 1, rect.y},
                {rect.X + 1, rect.Y + 1},

                {rect.X - box.length + 1, rect.y},
                {rect.x, rect.Y - box.width + 1},
                {rect.X - box.length + 1, rect.Y - box.width + 1},

                {rect.x - box.length, rect.y},
                {rect.x, rect.y - box.width},
                {rect.x - box.length, rect.y - box.width},

                {rect.X + 1 - box.length, rect.Y + 1},
                {rect.X + 1 - box.length, rect.y - box.width},
            };

            for (auto& [px, py] : dots) {
                if (std::max(px, px + box.length) <= header.length && 
                    std::max(py, py + box.width) <= header.width) {
                    result.emplace_back(px, py);
                }
            }
        }
        return result;
    }

    [[nodiscard]] uint32_t size() const override { return height_rects.size(); }

private:
    void add_to_soa(const HeightRect& rect) {
        rect_x.push_back(rect.x);
        rect_X.push_back(rect.X);
        rect_y.push_back(rect.y);
        rect_Y.push_back(rect.Y);
        rect_h.push_back(rect.h);
    }
    
    void rebuild_soa() {
        rect_x.clear();
        rect_X.clear();
        rect_y.clear();
        rect_Y.clear();
        rect_h.clear();
        
        rect_x.reserve(height_rects.size());
        rect_X.reserve(height_rects.size());
        rect_y.reserve(height_rects.size());
        rect_Y.reserve(height_rects.size());
        rect_h.reserve(height_rects.size());
        
        for (const auto& rect : height_rects) {
            add_to_soa(rect);
        }
        
        // Добавляем padding до кратного 8 для AVX2
        while (rect_x.size() % 8 != 0) {
            // Добавляем "пустые" прямоугольники, которые никогда не пересекаются
            rect_x.push_back(UINT32_MAX);
            rect_X.push_back(0);
            rect_y.push_back(UINT32_MAX);
            rect_Y.push_back(0);
            rect_h.push_back(0);
        }
    }

// #ifdef __AVX2__
    [[nodiscard]] uint32_t get_avx(uint32_t qx, uint32_t qy, uint32_t qX, uint32_t qY) const {
        size_t n = rect_h.size();
        
        // Broadcast query bounds
        __m256i vqx = _mm256_set1_epi32(qx);
        __m256i vqy = _mm256_set1_epi32(qy);
        __m256i vqX = _mm256_set1_epi32(qX);
        __m256i vqY = _mm256_set1_epi32(qY);
        
        // Проверяем по 8 прямоугольников за раз
        for (size_t i = 0; i < n; i += 8) {
            // Загружаем 8 прямоугольников
            __m256i vx = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rect_x[i]));
            __m256i vX = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rect_X[i]));
            __m256i vy = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rect_y[i]));
            __m256i vY = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rect_Y[i]));
            
            // Проверяем условия пересечения: rect.x <= qX && qx <= rect.X && rect.y <= qY && qy <= rect.Y
            // Для unsigned сравнения используем трюк с XOR
            // a <= b эквивалентно !(a > b) для unsigned
            
            // rect.x <= qX
            __m256i cmp1 = _mm256_or_si256(
                _mm256_cmpgt_epi32(vqX, vx),  // qX > rect.x
                _mm256_cmpeq_epi32(vqX, vx)   // qX == rect.x
            );
            
            // qx <= rect.X
            __m256i cmp2 = _mm256_or_si256(
                _mm256_cmpgt_epi32(vX, vqx),  // rect.X > qx
                _mm256_cmpeq_epi32(vX, vqx)   // rect.X == qx
            );
            
            // rect.y <= qY
            __m256i cmp3 = _mm256_or_si256(
                _mm256_cmpgt_epi32(vqY, vy),  // qY > rect.y
                _mm256_cmpeq_epi32(vqY, vy)   // qY == rect.y
            );
            
            // qy <= rect.Y
            __m256i cmp4 = _mm256_or_si256(
                _mm256_cmpgt_epi32(vY, vqy),  // rect.Y > qy
                _mm256_cmpeq_epi32(vY, vqy)   // rect.Y == qy
            );
            
            // AND всех условий
            __m256i intersect = _mm256_and_si256(
                _mm256_and_si256(cmp1, cmp2),
                _mm256_and_si256(cmp3, cmp4)
            );
            
            // Получаем маску результатов
            int mask = _mm256_movemask_ps(_mm256_castsi256_ps(intersect));
            
            if (mask != 0) {
                // Найдено пересечение — возвращаем высоту первого (максимального)
                int first_set = __builtin_ctz(mask);  // индекс первого установленного бита
                return rect_h[i + first_set];
            }
        }
        
        return 0;
    }
// #endif

    [[nodiscard]] uint32_t get_scalar(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
        for (const auto& rect : height_rects) {
            if (rect.x <= X && x <= rect.X && rect.y <= Y && y <= rect.Y) {
                return rect.h;
            }
        }
        return 0;
    }

    [[nodiscard]] static std::vector<HeightRect> subtract(const HeightRect& existing,
                                                          const HeightRect& cutter) {
        std::vector<HeightRect> result;

        if (!existing.intersects(cutter)) {
            result.push_back(existing);
            return result;
        }

        if (cutter.x <= existing.x && cutter.X >= existing.X &&
            cutter.y <= existing.y && cutter.Y >= existing.Y) {
            return result;
        }

        uint32_t ix = std::max(existing.x, cutter.x);
        uint32_t iX = std::min(existing.X, cutter.X);
        uint32_t iy = std::max(existing.y, cutter.y);
        uint32_t iY = std::min(existing.Y, cutter.Y);

        // BOTTOM
        if (existing.y < iy) {
            HeightRect bottom;
            bottom.x = existing.x;
            bottom.X = existing.X;
            bottom.y = existing.y;
            bottom.Y = iy - 1;
            bottom.h = existing.h;
            if (bottom.is_valid()) {
                result.push_back(bottom);
            }
        }

        // TOP
        if (existing.Y > iY) {
            HeightRect top;
            top.x = existing.x;
            top.X = existing.X;
            top.y = iY + 1;
            top.Y = existing.Y;
            top.h = existing.h;
            if (top.is_valid()) {
                result.push_back(top);
            }
        }

        // LEFT
        if (existing.x < ix) {
            HeightRect left;
            left.x = existing.x;
            left.X = ix - 1;
            left.y = iy;
            left.Y = iY;
            left.h = existing.h;
            if (left.is_valid()) {
                result.push_back(left);
            }
        }

        // RIGHT
        if (existing.X > iX) {
            HeightRect right;
            right.x = iX + 1;
            right.X = existing.X;
            right.y = iy;
            right.Y = iY;
            right.h = existing.h;
            if (right.is_valid()) {
                result.push_back(right);
            }
        }

        return result;
    }
};
