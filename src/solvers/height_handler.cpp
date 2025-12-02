#include <solvers/height_handler.hpp>

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

    std::sort(height_rects.begin(), height_rects.end(), [&](const HeightRect &lhs, const HeightRect &rhs) {
        return lhs.h > rhs.h;
    });

    // TODO: имеет смысл разделить прямоугольники и убрать лишние, чтобы все прямоугольники не пересекались
}
