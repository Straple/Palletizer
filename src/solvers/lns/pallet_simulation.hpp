#pragma once

#include <objects/metrics.hpp>
#include <objects/test_data.hpp>
#include <utils/randomizer.hpp>

#include <cstdint>
#include <vector>

struct BoxMeta {
    uint32_t box_id = 0;
    uint32_t rotation = -1;
    uint32_t position = 3;
};

struct MutableParams {
    std::vector<uint32_t> weights = {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100};
};

struct Pallet {
    Answer answer;
    Metrics metrics;

    double get_score(const TestDataHeader &header) const;

    // Index over all boxes (pallet-major order); UINT32_MAX if no support ratios.
    uint32_t get_worst_flat_idx() const;

    std::pair<uint32_t, uint32_t> flat_to_pallet_local(uint32_t flat) const;
};

class GenomHandler {
public:
    explicit GenomHandler(const TestData &test_data);

    std::vector<std::vector<BoxMeta>> order;
    double support_threshold = 0.0;

    [[nodiscard]] const TestData &test_data_ref() const { return *test_data_; }
    [[nodiscard]] const Pallet &pallet() const { return pallet_; }
    [[nodiscard]] Pallet pallet_copy() const { return pallet_; }

    void mutate(Randomizer &rnd, const Pallet &last_pallet, const MutableParams &mp,
                std::vector<bool> *pallet_dirty = nullptr);

    void rebuild_all();
    void recompute_with_mask(std::vector<bool> pallet_dirty);

private:
    const TestData *test_data_;
    Pallet pallet_;

    void mark_pallet_dirty(std::vector<bool> *pallet_dirty, uint32_t p) const;
    void mark_all_pallets_dirty(std::vector<bool> *pallet_dirty) const;

    void mutate_segment_uniform_meta(Randomizer &rnd, std::vector<bool> *pallet_dirty);
    void mutate_segment_random_meta_per_box(Randomizer &rnd, std::vector<bool> *pallet_dirty);
    void mutate_segment_reverse(Randomizer &rnd, std::vector<bool> *pallet_dirty);
    void mutate_segment_shuffle(Randomizer &rnd, std::vector<bool> *pallet_dirty);
    void mutate_swap_boxes_across_pallets(Randomizer &rnd, std::vector<bool> *pallet_dirty);
    void mutate_swap_adjacent_pair(Randomizer &rnd, std::vector<bool> *pallet_dirty);
    void mutate_segment_sort_by_footprint(Randomizer &rnd, std::vector<bool> *pallet_dirty);
    void mutate_segment_sort_by_volume(Randomizer &rnd, std::vector<bool> *pallet_dirty);
    void mutate_segment_sort_by_height(Randomizer &rnd, std::vector<bool> *pallet_dirty);
    void mutate_segment_sort_by_weight(Randomizer &rnd, std::vector<bool> *pallet_dirty);
    void mutate_resample_support_threshold(Randomizer &rnd, std::vector<bool> *pallet_dirty);
    void mutate_cluster_one_sku(Randomizer &rnd, std::vector<bool> *pallet_dirty);
    void mutate_move_low_support_box_earlier(Randomizer &rnd, const Pallet &last_pallet, std::vector<bool> *pallet_dirty);

    void run_single_pallet(uint32_t pallet_idx);
    void flatten_support_and_metrics();
};
