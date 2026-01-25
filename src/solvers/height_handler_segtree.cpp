#include <solvers/height_handler_segtree.hpp>

#include <algorithm>

// Вспомогательная функция: найти следующую степень двойки
static uint32_t next_power_of_two(uint32_t n) {
    uint32_t p = 1;
    while (p < n) p *= 2;
    return p;
}

HeightHandlerSegTree::HeightHandlerSegTree(uint32_t length, uint32_t width)
    : pallet_length(length), pallet_width(width) {
    
    // Вычисляем размеры сетки
    grid_width = (length + CELL_SIZE_X - 1) / CELL_SIZE_X;
    grid_height = (width + CELL_SIZE_Y - 1) / CELL_SIZE_Y;
    
    // Размеры деревьев (степень двойки)
    tree_size_x = next_power_of_two(grid_width);
    tree_size_y = next_power_of_two(grid_height);
    
    // Инициализируем 2D segment tree
    // tree[i] — это 1D segment tree по Y для i-го узла по X
    tree.resize(2 * tree_size_x);
    for (auto& inner : tree) {
        inner.resize(2 * tree_size_y, 0);
    }
    
    // Добавляем начальный прямоугольник (пол)
    height_rects.push_back(HeightRect{0, 0, length - 1, width - 1, 0});
}

// Обновление внутреннего 1D segment tree по Y
void HeightHandlerSegTree::update_y(uint32_t vx, uint32_t lx, uint32_t rx, 
                                     uint32_t vy, uint32_t ly, uint32_t ry,
                                     uint32_t y1, uint32_t y2, uint32_t val) {
    if (y1 > ry || y2 < ly) return;
    if (y1 <= ly && ry <= y2) {
        tree[vx][vy] = std::max(tree[vx][vy], val);
        return;
    }
    uint32_t mid = (ly + ry) / 2;
    update_y(vx, lx, rx, 2*vy, ly, mid, y1, y2, val);
    update_y(vx, lx, rx, 2*vy+1, mid+1, ry, y1, y2, val);
}

// Обновление 2D segment tree
void HeightHandlerSegTree::update_x(uint32_t vx, uint32_t lx, uint32_t rx,
                                     uint32_t x1, uint32_t x2, uint32_t y1, uint32_t y2, uint32_t val) {
    if (x1 > rx || x2 < lx) return;
    if (x1 <= lx && rx <= x2) {
        update_y(vx, lx, rx, 1, 0, tree_size_y - 1, y1, y2, val);
        return;
    }
    uint32_t mid = (lx + rx) / 2;
    update_x(2*vx, lx, mid, x1, x2, y1, y2, val);
    update_x(2*vx+1, mid+1, rx, x1, x2, y1, y2, val);
}

// Запрос максимума из внутреннего 1D segment tree по Y
uint32_t HeightHandlerSegTree::query_y(uint32_t vx, uint32_t vy, uint32_t ly, uint32_t ry,
                                        uint32_t y1, uint32_t y2) const {
    if (y1 > ry || y2 < ly) return 0;
    if (y1 <= ly && ry <= y2) {
        return tree[vx][vy];
    }
    uint32_t mid = (ly + ry) / 2;
    return std::max(
        query_y(vx, 2*vy, ly, mid, y1, y2),
        query_y(vx, 2*vy+1, mid+1, ry, y1, y2)
    );
}

// Запрос максимума из 2D segment tree
uint32_t HeightHandlerSegTree::query_x(uint32_t vx, uint32_t lx, uint32_t rx,
                                        uint32_t x1, uint32_t x2, uint32_t y1, uint32_t y2) const {
    if (x1 > rx || x2 < lx) return 0;
    if (x1 <= lx && rx <= x2) {
        return query_y(vx, 1, 0, tree_size_y - 1, y1, y2);
    }
    uint32_t mid = (lx + rx) / 2;
    return std::max(
        query_x(2*vx, lx, mid, x1, x2, y1, y2),
        query_x(2*vx+1, mid+1, rx, x1, x2, y1, y2)
    );
}

uint32_t HeightHandlerSegTree::get(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    uint32_t cx1 = to_cell_x(x);
    uint32_t cy1 = to_cell_y(y);
    uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
    uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);
    
    return query_x(1, 0, tree_size_x - 1, cx1, cx2, cy1, cy2);
}

void HeightHandlerSegTree::add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) {
    uint32_t cx1 = to_cell_x(x);
    uint32_t cy1 = to_cell_y(y);
    uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
    uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);
    
    update_x(1, 0, tree_size_x - 1, cx1, cx2, cy1, cy2, h);
    
    // Добавляем прямоугольник для get_dots
    height_rects.push_back(HeightRect{x, y, X, Y, h});
}

uint64_t HeightHandlerSegTree::get_area_at_max_height(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    uint32_t cx1 = to_cell_x(x);
    uint32_t cy1 = to_cell_y(y);
    uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
    uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);
    
    // Находим максимальную высоту
    uint32_t max_h = query_x(1, 0, tree_size_x - 1, cx1, cx2, cy1, cy2);
    
    // Для точного подсчёта площади проходим по ячейкам
    // (можно оптимизировать, но для простоты используем наивный подход)
    uint64_t area = 0;
    for (uint32_t cx = cx1; cx <= cx2; ++cx) {
        for (uint32_t cy = cy1; cy <= cy2; ++cy) {
            uint32_t cell_h = query_x(1, 0, tree_size_x - 1, cx, cx, cy, cy);
            if (cell_h == max_h) {
                // Размер ячейки с учётом границ запроса
                uint32_t cell_x_start = cx * CELL_SIZE_X;
                uint32_t cell_y_start = cy * CELL_SIZE_Y;
                uint32_t cell_x_end = (cx + 1) * CELL_SIZE_X;
                uint32_t cell_y_end = (cy + 1) * CELL_SIZE_Y;
                
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

std::vector<std::pair<uint32_t, uint32_t>> HeightHandlerSegTree::get_dots(
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

        for (auto& [px, py] : dots) {
            if (std::max(px, px + box.length) <= header.length && std::max(py, py + box.width) <= header.width) {
                result.emplace_back(px, py);
            }
        }
    }
    
    return result;
}
