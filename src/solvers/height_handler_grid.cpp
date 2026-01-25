#include <solvers/height_handler_grid.hpp>

#include <algorithm>

HeightHandlerGrid::HeightHandlerGrid(uint32_t length, uint32_t width)
    : pallet_length(length), pallet_width(width) {
    
    // Вычисляем размеры сетки (округляем вверх)
    grid_width = (length + CELL_SIZE_X - 1) / CELL_SIZE_X;
    grid_height = (width + CELL_SIZE_Y - 1) / CELL_SIZE_Y;
    
    // Инициализируем все ячейки нулевой высотой
    grid.assign(grid_width * grid_height, 0);
    
    // Добавляем начальный прямоугольник (пол)
    height_rects.push_back(HeightRect{0, 0, length - 1, width - 1, 0});
}

uint32_t HeightHandlerGrid::get(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    // Преобразуем координаты в индексы ячеек
    uint32_t cx1 = to_cell_x(x);
    uint32_t cy1 = to_cell_y(y);
    uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
    uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);
    
    // Находим максимальную высоту в прямоугольнике ячеек
    uint32_t max_h = 0;
    for (uint32_t cy = cy1; cy <= cy2; ++cy) {
        for (uint32_t cx = cx1; cx <= cx2; ++cx) {
            max_h = std::max(max_h, cell_at(cx, cy));
        }
    }
    
    return max_h;
}

void HeightHandlerGrid::add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) {
    // Преобразуем координаты в индексы ячеек
    uint32_t cx1 = to_cell_x(x);
    uint32_t cy1 = to_cell_y(y);
    uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
    uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);
    
    // Обновляем высоту в затронутых ячейках
    for (uint32_t cy = cy1; cy <= cy2; ++cy) {
        for (uint32_t cx = cx1; cx <= cx2; ++cx) {
            cell_at(cx, cy) = std::max(cell_at(cx, cy), h);
        }
    }
    
    // Добавляем прямоугольник в список для get_dots
    height_rects.push_back(HeightRect{x, y, X, Y, h});
}

uint64_t HeightHandlerGrid::get_area_at_max_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    // Преобразуем координаты в индексы ячеек
    uint32_t cx1 = to_cell_x(x);
    uint32_t cy1 = to_cell_y(y);
    uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
    uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);
    
    // Сначала находим максимальную высоту
    uint32_t max_h = 0;
    for (uint32_t cy = cy1; cy <= cy2; ++cy) {
        for (uint32_t cx = cx1; cx <= cx2; ++cx) {
            max_h = std::max(max_h, cell_at(cx, cy));
        }
    }
    
    // Теперь считаем площадь ячеек с максимальной высотой
    uint64_t area = 0;
    for (uint32_t cy = cy1; cy <= cy2; ++cy) {
        for (uint32_t cx = cx1; cx <= cx2; ++cx) {
            if (cell_at(cx, cy) == max_h) {
                // Вычисляем реальный размер ячейки с учётом границ запроса
                uint32_t cell_x_start = cx * CELL_SIZE_X;
                uint32_t cell_y_start = cy * CELL_SIZE_Y;
                uint32_t cell_x_end = (cx + 1) * CELL_SIZE_X;
                uint32_t cell_y_end = (cy + 1) * CELL_SIZE_Y;
                
                // Пересечение ячейки с запрашиваемым прямоугольником
                uint32_t ix1 = std::max(cell_x_start, x);
                uint32_t iy1 = std::max(cell_y_start, y);
                uint32_t ix2 = std::min(cell_x_end, X + 1);
                uint32_t iy2 = std::min(cell_y_end, Y + 1);
                
                if (ix1 < ix2 && iy1 < iy2) {
                    area += static_cast<uint64_t>(ix2 - ix1) * (iy2 - iy1);
                }
            }
        }
    }
    
    return area;
}

std::vector<std::pair<uint32_t, uint32_t>> HeightHandlerGrid::get_dots(
    const TestDataHeader& header, const BoxSize& box) const {
    
    // Используем тот же алгоритм что и в HeightHandlerRects
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

        for (auto& [x, y] : dots) {
            if (std::max(x, x + box.length) <= header.length && std::max(y, y + box.width) <= header.width) {
                result.emplace_back(x, y);
            }
        }
    }
    
    return result;
}
