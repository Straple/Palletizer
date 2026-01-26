#include <solvers/height_handler_rects.hpp>

#include <utils/assert.hpp>

#include <algorithm>
#include <iostream>
#include <map>
#include <numeric>

HeightHandlerRects::HeightHandlerRects(uint32_t length, uint32_t width) {
    height_rects.push_back(HeightRect{0, 0, length - 1, width - 1, 0});
}

uint32_t HeightHandlerRects::get_h(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    for (auto rect: height_rects) {
        if (rect.x <= X && x <= rect.X && rect.y <= Y && y <= rect.Y) {
            return rect.h;
        }
    }
    return -1;
}

uint64_t HeightHandlerRects::get_area(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    uint64_t area = 0;
    uint32_t max_height = 0;
    bool found_max = false;
    
    for (const auto& rect : height_rects) {
        // Проверяем пересечение
        if (rect.x <= X && x <= rect.X && rect.y <= Y && y <= rect.Y) {
            if (!found_max) {
                // Первый пересекающийся — это максимальная высота
                max_height = rect.h;
                found_max = true;
            }
            
            if (rect.h == max_height) {
                // Вычисляем пересечение и добавляем площадь
                uint32_t ix = std::max(x, rect.x);
                uint32_t iX = std::min(X, rect.X);
                uint32_t iy = std::max(y, rect.y);
                uint32_t iY = std::min(Y, rect.Y);
                
                area += static_cast<uint64_t>(iX - ix + 1) * (iY - iy + 1);
            } else {
                // Высота стала меньше максимальной - выходим
                break;
            }
        }
    }

    /*{
        uint32_t supported_cells = 0;
        for (uint32_t a = x; a <= X; a += 1) {
            for (uint32_t b = y; b <= Y; b += 1) {
                if (get(a, b, a, b) == max_height) {
                    supported_cells++;
                }
            }
        }
        ASSERT(supported_cells == area, "invalid get_area_at_max_height: " + std::to_string(area) + " != " + std::to_string(supported_cells));
    }*/
    
    return area;
}

std::vector<std::pair<uint32_t, uint32_t>>
HeightHandlerRects::get_dots(const TestDataHeader &header, const BoxSize &box) const {
    std::vector<std::pair<uint32_t, uint32_t>> result;
    for (auto rect: height_rects) {
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

                // TODO: добавить больше точек, сейчас недостает
                // идеальным решением все же будет разбивать на непересекающиеся прямоугольники
        };

        for (auto &[x, y]: dots) {
            if (std::max(x, x + box.length) <= header.length && std::max(y, y + box.width) <= header.width) {
                result.emplace_back(x, y);
            }
        }
    }
    // замедляет в 2 раза
    // std::sort(result.begin(), result.end());
    // result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

// Разбивает existing прямоугольник, вырезая из него область cutter
// Возвращает до 4 прямоугольников (части, не пересекающиеся с cutter)
[[nodiscard]] static std::vector<HeightRect> subtract(const HeightRect &existing,
                                                      const HeightRect &cutter) {
    std::vector<HeightRect> result;

    // Если не пересекаются - возвращаем исходный
    if (!existing.intersects(cutter)) {
        result.push_back(existing);
        return result;
    }

    // Если cutter полностью покрывает existing - ничего не остаётся
    if (cutter.x <= existing.x && cutter.X >= existing.X &&
        cutter.y <= existing.y && cutter.Y >= existing.Y) {
        return result;
    }

    // Вырезаем до 4 частей:
    //
    //     +------------------+
    //     |      TOP         |
    //     +------+----+------+
    //     | LEFT |cut | RIGHT|
    //     +------+----+------+
    //     |     BOTTOM       |
    //     +------------------+

    // Находим границы пересечения
    uint32_t ix = std::max(existing.x, cutter.x);
    uint32_t iX = std::min(existing.X, cutter.X);
    uint32_t iy = std::max(existing.y, cutter.y);
    uint32_t iY = std::min(existing.Y, cutter.Y);

    // BOTTOM: ниже пересечения (полная ширина existing)
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

    // TOP: выше пересечения (полная ширина existing)
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

    // LEFT: слева от пересечения (только в пределах высоты пересечения)
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

    // RIGHT: справа от пересечения (только в пределах высоты пересечения)
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

void HeightHandlerRects::add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) {
    HeightRect new_rect{x, y, X, Y, h};
    
    if (!new_rect.is_valid()) {
        return;
    }

    std::vector<HeightRect> new_height_rects;

    for (const auto &existing: height_rects) {
        if (!existing.intersects(new_rect)) {
            // Не пересекаются - сохраняем как есть
            new_height_rects.push_back(existing);
        } else if (existing.h > new_rect.h) {
            // Существующий выше - он остаётся, но нужно разрезать новый
            // Это обработаем после цикла
            new_height_rects.push_back(existing);
        } else {
            // Новый выше или равен - вырезаем из существующего
            auto parts = subtract(existing, new_rect);
            for (const auto &part: parts) {
                new_height_rects.push_back(part);
            }
        }
    }

    // Теперь нужно добавить новый прямоугольник,
    // но вырезать из него части, где существующие прямоугольники выше
    std::vector<HeightRect> new_parts = {new_rect};

    for (const auto &existing: height_rects) {
        if (existing.h > new_rect.h && existing.intersects(new_rect)) {
            // Вырезаем из всех частей нового прямоугольника
            std::vector<HeightRect> updated_parts;
            for (const auto &part: new_parts) {
                auto subtracted = subtract(part, existing);
                for (const auto &s: subtracted) {
                    updated_parts.push_back(s);
                }
            }
            new_parts = std::move(updated_parts);
        }
    }

    // Добавляем оставшиеся части нового прямоугольника
    for (const auto &part: new_parts) {
        new_height_rects.push_back(part);
    }

    height_rects = std::move(new_height_rects);

    // Сортируем по убыванию высоты
    std::sort(height_rects.begin(), height_rects.end(),
              [](const HeightRect &a, const HeightRect &b) {
                  return a.h > b.h;
              });

    // TODO: можно значительно уменьшить колво прямоугольников, убрав оочень маленькие

    uint32_t s = 0;
    for (uint32_t i = 0; i < height_rects.size(); i++) {
        s += (height_rects[i].X - height_rects[i].x + 1) * (height_rects[i].Y - height_rects[i].y + 1);
        for (uint32_t j = 0; j < height_rects.size(); j++) {
            if (i != j && height_rects[i].intersects(height_rects[j])) {
                FAILED_ASSERT("intersects");
            }
        }
    }
}

void HeightHandlerRects::print() {
    std::cout << "HeightHandlerRects\n";
    for (const auto &rect: height_rects) {
        std::cout << rect.h << ' ' << rect.x << ' ' << rect.y << ' ' << rect.X << ' ' << rect.Y << '\n';
    }
}
