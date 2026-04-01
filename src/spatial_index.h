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
    // Constructor to initialize the grid with a specific cell size. 
    SpatialIndex(double cell_size);

    // Incremental update functions
    void insert_halfedge(int he_id, const Point2& p1, const Point2& p2);
    void remove_halfedge(int he_id, const Point2& p1, const Point2& p2);

    // Returns a list of half-edge IDs that might intersect the query box
    std::vector<int> query_intersecting_edges(const AABB& query_box) const;

private:
    double cell_size_;
    
    // Maps a 64-bit hashed grid coordinate to a set of halfedge IDs
    std::unordered_map<std::uint64_t, std::unordered_set<int>> grid_;
    
    std::uint64_t hash_cell(int x, int y) const;
    std::vector<std::uint64_t> get_cells_for_aabb(const AABB& box) const;
};

#endif // SPATIAL_INDEX_H