#include "spatial_index.h"
#include <cmath>

SpatialIndex::SpatialIndex(double cell_size) : cell_size_(cell_size)
{
    if (cell_size_ <= 0.0)
    {
        cell_size_ = 1.0;
    }
}

std::uint64_t SpatialIndex::hash_cell(int x, int y) const
{
    std::uint64_t ux = (std::uint64_t)(static_cast<std::int64_t>(x));
    std::uint64_t uy = (std::uint64_t)(static_cast<std::int64_t>(y));
    return (ux << 32) | (uy & 0xFFFFFFFF);
}

std::vector<std::uint64_t> SpatialIndex::get_cells_for_aabb(const AABB &box) const
{
    std::vector<std::uint64_t> cells;
    int min_cx = static_cast<int>(std::floor(box.min_x / cell_size_));
    int max_cx = static_cast<int>(std::floor(box.max_x / cell_size_));
    int min_cy = static_cast<int>(std::floor(box.min_y / cell_size_));
    int max_cy = static_cast<int>(std::floor(box.max_y / cell_size_));

    for (int x = min_cx; x <= max_cx; ++x)
    {
        for (int y = min_cy; y <= max_cy; ++y)
        {
            cells.push_back(hash_cell(x, y));
        }
    }
    return cells;
}

void SpatialIndex::insert_halfedge(int he_id, const Point2 &p1, const Point2 &p2)
{
    AABB box = compute_aabb(p1, p2);
    for (auto cell_hash : get_cells_for_aabb(box))
    {
        grid_[cell_hash].insert(he_id);
    }
}

void SpatialIndex::remove_halfedge(int he_id, const Point2 &p1, const Point2 &p2)
{
    AABB box = compute_aabb(p1, p2);
    for (auto cell_hash : get_cells_for_aabb(box))
    {
        auto it = grid_.find(cell_hash);
        if (it != grid_.end())
        {
            it->second.erase(he_id);

            if (it->second.empty())
            {
                grid_.erase(it);
            }
        }
    }
}

std::vector<int> SpatialIndex::query_intersecting_edges(const AABB &query_box) const
{
    std::unordered_set<int> unique_edges;
    for (auto cell_hash : get_cells_for_aabb(query_box))
    {
        auto it = grid_.find(cell_hash);
        if (it != grid_.end())
        {
            for (int he_id : it->second)
            {
                unique_edges.insert(he_id);
            }
        }
    }
    return std::vector<int>(unique_edges.begin(), unique_edges.end());
}