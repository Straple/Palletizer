#pragma once

#include <algorithm>
#include <cstdint>
#include <solvers/height_handler.hpp>
#include <vector>

// 2D Segment Tree с Lazy Propagation
// X-дерево содержит Y-деревья. Каждый Y-узел хранит count_2d — количество 2D-ячеек с max_h.
//
// При UPDATE: обновляем полностью покрытые X-узлы, затем пересчитываем родителей
// При QUERY: стандартный запрос по полностью покрытым X-узлам
//
// Сложность: O(log W × H) для update (merge Y-деревьев), O(log² n) для query
template<uint32_t CELL_SIZE_X = 10, uint32_t CELL_SIZE_Y = 10>
class HeightHandlerSegTreeT {
private:
    struct QueryResult {
        uint32_t max_h = 0;
        uint64_t count_2d = 0;

        static QueryResult merge(const QueryResult &a, const QueryResult &b) {
            QueryResult result;
            if (a.max_h > b.max_h) {
                result.max_h = a.max_h;
                result.count_2d = a.count_2d;
            } else if (a.max_h < b.max_h) {
                result.max_h = b.max_h;
                result.count_2d = b.count_2d;
            } else {
                result.max_h = a.max_h;
                result.count_2d = a.count_2d + b.count_2d;
            }
            return result;
        }
    };

    // Y-узел хранит count_2d — количество 2D-ячеек (X,Y) с данным max_h
    // x_size — количество X-ячеек в X-диапазоне, к которому относится это Y-дерево
    struct YNode {
        uint32_t max_h = 0;
        uint64_t count_2d = 0;// Количество 2D-ячеек с max_h
        uint32_t lazy = 0;
        uint32_t y_size = 0;// Количество Y-ячеек в диапазоне этого узла
        uint32_t x_size = 0;// Количество X-ячеек в X-диапазоне (для расчёта count_2d)

        void apply_lazy(uint32_t val) {
            if (val > max_h) {
                max_h = val;
                count_2d = (uint64_t) y_size * x_size;
            }
            lazy = std::max(lazy, val);
        }

        static YNode merge(const YNode &left, const YNode &right) {
            YNode result;
            result.y_size = left.y_size + right.y_size;
            result.x_size = left.x_size;// Должны быть одинаковые
            if (left.max_h > right.max_h) {
                result.max_h = left.max_h;
                result.count_2d = left.count_2d;
            } else if (left.max_h < right.max_h) {
                result.max_h = right.max_h;
                result.count_2d = right.count_2d;
            } else {
                result.max_h = left.max_h;
                result.count_2d = left.count_2d + right.count_2d;
            }
            return result;
        }
    };

    struct XNode {
        std::vector<YNode> y_tree;
        uint32_t x_size = 0;// Количество X-ячеек в диапазоне этого X-узла

        void init(uint32_t y_size, uint32_t xs) {
            y_tree.resize(4 * y_size);
            x_size = xs;
        }
    };

    std::vector<XNode> x_tree;
    uint32_t grid_width;
    uint32_t grid_height;
    uint32_t pallet_length;
    uint32_t pallet_width;

    std::vector<HeightRect> height_rects;

    [[nodiscard]] uint32_t to_cell_x(uint32_t x) const { return x / CELL_SIZE_X; }
    [[nodiscard]] uint32_t to_cell_y(uint32_t y) const { return y / CELL_SIZE_Y; }

    void y_push(XNode &node, uint32_t v) {
        if (node.y_tree[v].lazy > 0 && 2 * v + 1 < node.y_tree.size()) {
            node.y_tree[2 * v].apply_lazy(node.y_tree[v].lazy);
            node.y_tree[2 * v + 1].apply_lazy(node.y_tree[v].lazy);
            node.y_tree[v].lazy = 0;
        }
    }

    void y_build(XNode &node, uint32_t v, uint32_t tl, uint32_t tr) {
        node.y_tree[v].y_size = tr - tl + 1;
        node.y_tree[v].x_size = node.x_size;
        node.y_tree[v].count_2d = (uint64_t) node.y_tree[v].y_size * node.y_tree[v].x_size;
        node.y_tree[v].max_h = 0;
        node.y_tree[v].lazy = 0;

        if (tl < tr) {
            uint32_t tm = (tl + tr) / 2;
            y_build(node, 2 * v, tl, tm);
            y_build(node, 2 * v + 1, tm + 1, tr);
        }
    }

    void y_update(XNode &node, uint32_t v, uint32_t tl, uint32_t tr,
                  uint32_t l, uint32_t r, uint32_t val) {
        if (l > r || l > tr || r < tl) return;

        if (l <= tl && tr <= r) {
            node.y_tree[v].apply_lazy(val);
            return;
        }

        y_push(node, v);

        uint32_t tm = (tl + tr) / 2;
        y_update(node, 2 * v, tl, tm, l, r, val);
        y_update(node, 2 * v + 1, tm + 1, tr, l, r, val);

        node.y_tree[v] = YNode::merge(node.y_tree[2 * v], node.y_tree[2 * v + 1]);
        node.y_tree[v].lazy = 0;
    }

    YNode y_query(XNode &node, uint32_t v, uint32_t tl, uint32_t tr,
                  uint32_t l, uint32_t r) {
        if (l > r || l > tr || r < tl) {
            return YNode{0, 0, 0, 0, 0};
        }

        if (l <= tl && tr <= r) {
            return node.y_tree[v];
        }

        y_push(node, v);

        uint32_t tm = (tl + tr) / 2;
        YNode left_result = y_query(node, 2 * v, tl, tm, l, r);
        YNode right_result = y_query(node, 2 * v + 1, tm + 1, tr, l, r);

        return YNode::merge(left_result, right_result);
    }

    // Мержим два Y-дерева: dst = merge(left, right)
    // Post-order: сначала мержим детей, потом пересчитываем родителя
    void y_merge_trees(XNode &dst, XNode &left, XNode &right, uint32_t v, uint32_t tl, uint32_t tr) {
        dst.y_tree[v].y_size = left.y_tree[v].y_size;
        dst.y_tree[v].x_size = dst.x_size;
        dst.y_tree[v].lazy = 0;

        if (tl < tr) {
            // Пропушим lazy в источниках перед рекурсией
            y_push(left, v);
            y_push(right, v);

            uint32_t tm = (tl + tr) / 2;
            y_merge_trees(dst, left, right, 2 * v, tl, tm);
            y_merge_trees(dst, left, right, 2 * v + 1, tm + 1, tr);

            // Пересчитываем текущий узел из детей
            dst.y_tree[v] = YNode::merge(dst.y_tree[2 * v], dst.y_tree[2 * v + 1]);
            dst.y_tree[v].lazy = 0;
        } else {
            // Лист: мержим напрямую
            uint32_t left_max = left.y_tree[v].max_h;
            uint32_t right_max = right.y_tree[v].max_h;

            if (left_max > right_max) {
                dst.y_tree[v].max_h = left_max;
                dst.y_tree[v].count_2d = left.y_tree[v].count_2d;
            } else if (left_max < right_max) {
                dst.y_tree[v].max_h = right_max;
                dst.y_tree[v].count_2d = right.y_tree[v].count_2d;
            } else {
                dst.y_tree[v].max_h = left_max;
                dst.y_tree[v].count_2d = left.y_tree[v].count_2d + right.y_tree[v].count_2d;
            }
        }
    }

    void x_build(uint32_t v, uint32_t tl, uint32_t tr) {
        uint32_t xs = tr - tl + 1;
        x_tree[v].init(grid_height, xs);
        y_build(x_tree[v], 1, 0, grid_height - 1);

        if (tl < tr) {
            uint32_t tm = (tl + tr) / 2;
            x_build(2 * v, tl, tm);
            x_build(2 * v + 1, tm + 1, tr);
        }
    }

    // UPDATE: всегда идём до листьев X-дерева, затем пересчитываем вверх
    void x_update(uint32_t v, uint32_t tl, uint32_t tr,
                  uint32_t x_l, uint32_t x_r, uint32_t y_l, uint32_t y_r, uint32_t val) {
        if (x_l > x_r || x_l > tr || x_r < tl) return;

        if (tl == tr) {
            // Лист X-дерева — обновляем Y-дерево
            y_update(x_tree[v], 1, 0, grid_height - 1, y_l, y_r, val);
            return;
        }

        // Идём в детей
        uint32_t tm = (tl + tr) / 2;
        x_update(2 * v, tl, tm, x_l, x_r, y_l, y_r, val);
        x_update(2 * v + 1, tm + 1, tr, x_l, x_r, y_l, y_r, val);

        // Пересчитываем текущий узел как merge детей
        y_merge_trees(x_tree[v], x_tree[2 * v], x_tree[2 * v + 1], 1, 0, grid_height - 1);
    }

    // QUERY: стандартный запрос по полностью покрытым X-узлам
    QueryResult x_query(uint32_t v, uint32_t tl, uint32_t tr,
                        uint32_t x_l, uint32_t x_r, uint32_t y_l, uint32_t y_r) {
        if (x_l > x_r || x_l > tr || x_r < tl) {
            return QueryResult{0, 0};
        }

        if (x_l <= tl && tr <= x_r) {
            // Полностью покрыт — возвращаем результат из Y-дерева
            YNode y_res = y_query(x_tree[v], 1, 0, grid_height - 1, y_l, y_r);
            QueryResult result;
            result.max_h = y_res.max_h;
            result.count_2d = y_res.count_2d;// Уже в 2D-ячейках!
            return result;
        }

        // Частично покрыт — идём в детей
        uint32_t tm = (tl + tr) / 2;
        QueryResult left = x_query(2 * v, tl, tm, x_l, x_r, y_l, y_r);
        QueryResult right = x_query(2 * v + 1, tm + 1, tr, x_l, x_r, y_l, y_r);

        return QueryResult::merge(left, right);
    }

public:
    HeightHandlerSegTreeT(uint32_t length, uint32_t width)
        : pallet_length(length), pallet_width(width) {

        grid_width = (length + CELL_SIZE_X - 1) / CELL_SIZE_X;
        grid_height = (width + CELL_SIZE_Y - 1) / CELL_SIZE_Y;

        x_tree.resize(4 * grid_width);
        x_build(1, 0, grid_width - 1);

        height_rects.push_back(HeightRect{0, 0, length - 1, width - 1, 0});
    }

    [[nodiscard]] uint32_t get_h(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
        uint32_t cx1 = to_cell_x(x);
        uint32_t cy1 = to_cell_y(y);
        uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
        uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);

        QueryResult result = const_cast<HeightHandlerSegTreeT *>(this)->x_query(
                1, 0, grid_width - 1, cx1, cx2, cy1, cy2);
        return result.max_h;
    }

    [[nodiscard]] uint64_t get_area(uint32_t x, uint32_t y, uint32_t X, uint32_t Y) const {
        uint32_t cx1 = to_cell_x(x);
        uint32_t cy1 = to_cell_y(y);
        uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
        uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);

        QueryResult result = const_cast<HeightHandlerSegTreeT *>(this)->x_query(
                1, 0, grid_width - 1, cx1, cx2, cy1, cy2);

        return result.count_2d * CELL_SIZE_X * CELL_SIZE_Y;
    }

    void add_rect(uint32_t x, uint32_t y, uint32_t X, uint32_t Y, uint32_t h) {
        uint32_t cx1 = to_cell_x(x);
        uint32_t cy1 = to_cell_y(y);
        uint32_t cx2 = std::min(to_cell_x(X), grid_width - 1);
        uint32_t cy2 = std::min(to_cell_y(Y), grid_height - 1);

        x_update(1, 0, grid_width - 1, cx1, cx2, cy1, cy2, h);

        height_rects.push_back(HeightRect{x, y, X, Y, h});
    }

    [[nodiscard]] std::vector<std::pair<uint32_t, uint32_t>> get_dots(
            const TestDataHeader &header, const BoxSize &box) const {

        std::vector<std::pair<uint32_t, uint32_t>> result;
        for (const auto &rect: height_rects) {
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

            for (auto &[px, py]: dots) {
                if (std::max(px, px + box.length) <= header.length &&
                    std::max(py, py + box.width) <= header.width) {
                    result.emplace_back(px, py);
                }
            }
        }
        return result;
    }

    [[nodiscard]] uint32_t size() const { return height_rects.size(); }
};

using HeightHandlerSegTree = HeightHandlerSegTreeT<10, 10>;
