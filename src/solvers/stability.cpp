#include <solvers/stability.hpp>
#include <solvers/height_handler.hpp>

#include <utils/assert.hpp>

#include <algorithm>
#include <cmath>
#include <map>

// Проверка, совпадают ли два отрезка на одной линии
// Возвращает длину совпадения
uint32_t segment_overlap_length(uint32_t a1, uint32_t a2, uint32_t b1, uint32_t b2) {
    // Убедимся что a1 < a2 и b1 < b2
    if (a1 > a2) std::swap(a1, a2);
    if (b1 > b2) std::swap(b1, b2);
    
    // Найдём пересечение отрезков [a1, a2] и [b1, b2]
    uint32_t overlap_start = std::max(a1, b1);
    uint32_t overlap_end = std::min(a2, b2);
    
    if (overlap_start < overlap_end) {
        return overlap_end - overlap_start;
    }
    return 0;
}

// Рассчитать длину совпадающих рёбер между двумя коробками
// Коробка 1 должна быть выше коробки 2 (box1.z == box2.Z)
uint32_t calc_overlapping_edges(const Position& box1, const Position& box2) {
    // Проверяем, что коробки соприкасаются по Z
    // box1 стоит на box2: нижняя грань box1 совпадает с верхней гранью box2
    if (box1.z != box2.Z) {
        return 0;
    }
    
    // Проверяем, пересекаются ли проекции на плоскость XY
    bool x_overlap = box1.x < box2.X && box2.x < box1.X;
    bool y_overlap = box1.y < box2.Y && box2.y < box1.Y;
    
    if (!x_overlap || !y_overlap) {
        return 0;  // Коробки не пересекаются в плоскости XY
    }
    
    uint32_t total_overlap = 0;
    
    // Проверяем совпадение вертикальных рёбер (параллельных оси Y)
    // Рёбра box1: x = box1.x и x = box1.X
    // Рёбра box2: x = box2.x и x = box2.X
    
    // Если x-координаты рёбер совпадают, проверяем перекрытие по Y
    if (box1.x == box2.x) {
        total_overlap += segment_overlap_length(box1.y, box1.Y, box2.y, box2.Y);
    }
    if (box1.x == box2.X) {
        total_overlap += segment_overlap_length(box1.y, box1.Y, box2.y, box2.Y);
    }
    if (box1.X == box2.x) {
        total_overlap += segment_overlap_length(box1.y, box1.Y, box2.y, box2.Y);
    }
    if (box1.X == box2.X) {
        total_overlap += segment_overlap_length(box1.y, box1.Y, box2.y, box2.Y);
    }
    
    // Проверяем совпадение горизонтальных рёбер (параллельных оси X)
    // Рёбра box1: y = box1.y и y = box1.Y
    // Рёбра box2: y = box2.y и y = box2.Y
    
    if (box1.y == box2.y) {
        total_overlap += segment_overlap_length(box1.x, box1.X, box2.x, box2.X);
    }
    if (box1.y == box2.Y) {
        total_overlap += segment_overlap_length(box1.x, box1.X, box2.x, box2.X);
    }
    if (box1.Y == box2.y) {
        total_overlap += segment_overlap_length(box1.x, box1.X, box2.x, box2.X);
    }
    if (box1.Y == box2.Y) {
        total_overlap += segment_overlap_length(box1.x, box1.X, box2.x, box2.X);
    }
    
    return total_overlap;
}

CenterOfMass calc_center_of_mass(const TestData& test_data, const Answer& answer) {
    CenterOfMass result;
    
    if (answer.positions.empty()) {
        return result;
    }
    
    // Создаём map SKU -> Box для быстрого поиска веса
    std::map<uint32_t, const Box*> sku_to_box;
    for (const auto& box : test_data.boxes) {
        sku_to_box[box.sku] = &box;
    }
    
    double weighted_x = 0, weighted_y = 0, weighted_z = 0;
    double total_weight = 0;
    
    for (const auto& pos : answer.positions) {
        auto it = sku_to_box.find(pos.sku);
        ASSERT(it != sku_to_box.end(), "invalid SKU");
        double weight = static_cast<double>(it->second->weight);
        
        // Центр коробки
        double center_x = (pos.x + pos.X) / 2.0;
        double center_y = (pos.y + pos.Y) / 2.0;
        double center_z = (pos.z + pos.Z) / 2.0;
        
        weighted_x += center_x * weight;
        weighted_y += center_y * weight;
        weighted_z += center_z * weight;
        total_weight += weight;
    }
    
    if (total_weight > 0) {
        result.x = weighted_x / total_weight;
        result.y = weighted_y / total_weight;
        result.z = weighted_z / total_weight;
        result.total_weight = total_weight;
    }
    
    return result;
}

StabilityMetrics calc_stability(const TestData& test_data, const Answer& answer) {
    StabilityMetrics metrics;
    
    if (answer.positions.empty()) {
        return metrics;
    }
    
    // Создаём map SKU -> Box для быстрого поиска веса
    std::map<uint32_t, const Box*> sku_to_box;
    for (const auto& box : test_data.boxes) {
        sku_to_box[box.sku] = &box;
    }
    
    // 1. Рассчитываем суммарный периметр всех коробок
    for (const auto& pos : answer.positions) {
        uint32_t width = pos.X - pos.x;
        uint32_t length = pos.Y - pos.y;
        metrics.l_sum_per += 2.0 * (width + length);
    }
    
    // 2. Рассчитываем суммарную длину совпадающих рёбер
    // Для каждой пары коробок, где одна стоит на другой
    for (size_t i = 0; i < answer.positions.size(); i++) {
        for (size_t j = 0; j < answer.positions.size(); j++) {
            if (i == j) continue;
            
            const auto& box_upper = answer.positions[i];
            const auto& box_lower = answer.positions[j];
            
            // Проверяем, что box_upper стоит на box_lower
            if (box_upper.z == box_lower.Z) {
                metrics.l_sum += calc_overlapping_edges(box_upper, box_lower);
            }
        }
    }
    
    // 3. Рассчитываем коэффициент перевязки
    // G = K_int × (L_SumPer − L_Sum)
    // Нормализуем: interlocking_ratio = l_sum / l_sum_per
    // Чем меньше interlocking_ratio, тем лучше перевязка
    if (metrics.l_sum_per > 0) {
        metrics.interlocking_ratio = metrics.l_sum / metrics.l_sum_per;
        metrics.interlocking = metrics.l_sum_per - metrics.l_sum;
    }
    
    // 4. Рассчитываем центр тяжести (используем отдельную функцию)
    metrics.center_of_mass = calc_center_of_mass(test_data, answer);
    
    // 5. Рассчитываем общий коэффициент устойчивости
    // stability = (1 - interlocking_ratio) — чем ближе к 1, тем лучше
    metrics.stability = 1.0 - metrics.interlocking_ratio;
    
    // 6. Рассчитываем stability_area с помощью HeightHandler
    // Для каждой коробки проверяем, какая часть площади основания опирается на что-то
    HeightHandler height_handler;
    height_handler.add_rect(HeightRect{0, 0, test_data.header.length - 1, test_data.header.width - 1, 0});
    
    for (const auto& pos : answer.positions) {
        uint32_t width = pos.X - pos.x;
        uint32_t length = pos.Y - pos.y;
        uint64_t area = static_cast<uint64_t>(width) * length;
        
        metrics.total_area += area;
        
        // Проверяем опору для каждой единичной ячейки площади
        uint64_t supported = 0;
        
        // Проходим по всей площади основания коробки
        for (uint32_t x = pos.x; x < pos.X; x++) {
            for (uint32_t y = pos.y; y < pos.Y; y++) {
                // Проверяем, опирается ли эта точка на что-то
                if (height_handler.get(x, y, x + 1, y + 1) == pos.z) {
                    supported++;
                }
            }
        }
        
        metrics.supported_area += supported;
        metrics.hanging_area += (area - supported);
        
        // Добавляем коробку в HeightHandler для следующих итераций
        height_handler.add_rect({pos.x, pos.y, pos.X, pos.Y, pos.Z});
    }
    if (metrics.total_area) {
        metrics.stability_area = metrics.supported_area / metrics.total_area;
    }
    
    return metrics;
}
