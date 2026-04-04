#ifndef SPATIAL_INDEX_H
#define SPATIAL_INDEX_H

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>
#include "polygon_dcel.h"
#include "geometry_utils.h"

class SpatialIndex {
public:
    SpatialIndex(double cell_size);

    // Use Vertex* and Vec2 instead of int and Point2
    void insert_edge(Vertex* v, const Vec2& p1, const Vec2& p2);
    void remove_edge(Vertex* v, const Vec2& p1, const Vec2& p2);

    std::vector<Vertex*> query_intersecting_edges(const AABB& query_box) const;

    double get_cell_size() const { return cell_size_; }

private:
    double cell_size_;
    std::unordered_map<std::uint64_t, std::unordered_set<Vertex*>> grid_;
    
    std::uint64_t hash_cell(int x, int y) const;
    std::vector<std::uint64_t> get_cells_for_aabb(const AABB& box) const;
};

#endif // SPATIAL_INDEX_H