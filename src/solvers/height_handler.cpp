#include <solvers/height_handler.hpp>

#include <algorithm>

uint32_t HeightHandler::get(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
    for (auto rect: height_rects) {
        if (!(X < rect.x || rect.X < x) &&
            !(Y < rect.y || rect.Y < y)) {
            return rect.h;
        }
    }
    return -1;
}

void HeightHandler::add_rect(HeightRect rect) {
    height_rects.push_back(rect);

    // TODO: сортировка здесь излишне
    std::sort(height_rects.begin(), height_rects.end(), [&](const HeightRect &lhs, const HeightRect &rhs) {
        return lhs.h > rhs.h;
    });

    // TODO: имеет смысл разделить прямоугольники и убрать лишние, чтобы все прямоугольники не пересекались
}

std::vector<std::pair<uint32_t, uint32_t>>
HeightHandler::get_dots(const TestDataHeader &header, const BoxSize &box) const {
    std::vector<std::pair<uint32_t, uint32_t>> result;
    for (auto rect: height_rects) {
        std::vector<std::pair<uint32_t, uint32_t>> dots = {
                {rect.x,                  rect.y},
                {rect.x,                  rect.Y + 1},
                {rect.X + 1,              rect.y},
                {rect.X + 1,              rect.Y + 1},

                {rect.X - box.length + 1, rect.y},
                {rect.x,                  rect.Y - box.width + 1},
                {rect.X - box.length + 1, rect.Y - box.width + 1},

                {rect.x - box.length,     rect.y},
                {rect.x,                  rect.y - box.width},
                {rect.x - box.length,     rect.y - box.width},

                {rect.X + 1 - box.length, rect.Y + 1},
                {rect.X + 1 - box.length, rect.y - box.width},

                // TODO: добавить больше точек, сейчас недостает
                // идеальным решением все же будет разбивать на непересекающиеся прямоугольники
        };

        for (auto &[x, y]: dots) {
            if (x + box.length <= header.length && y + box.width <= header.width) {
                result.emplace_back(x, y);
            }
        }
    }
    // замедляет в 2 раза
    // std::sort(result.begin(), result.end());
    // result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}
