#ifndef GEOMETRY_UTILS_H
#define GEOMETRY_UTILS_H

#include <algorithm>
#include "polygon_dcel.h"

struct Vec2
{
    double x{};
    double y{};
};

struct Vertex
{
    double x{};
    double y{};
    int original_id{-1};
    bool removed{false};
    Vertex *prev{nullptr};
    Vertex *next{nullptr};
};

// 2. Add an AABB bounding box structure for Spatial Index
struct AABB
{
    double min_x, min_y, max_x, max_y;

    void expand(double epsilon = 1e-9)
    {
        min_x -= epsilon;
        min_y -= epsilon;
        max_x += epsilon;
        max_y += epsilon;
    }

    bool intersects(const AABB &other) const
    {
        return !(max_x < other.min_x || min_x > other.max_x ||
                 max_y < other.min_y || min_y > other.max_y);
    }
};

// 3. Helper to create AABB from two Vec2 points
inline AABB compute_aabb(const Vec2 &p1, const Vec2 &p2)
{
    AABB box;
    box.min_x = std::min(p1.x, p2.x);
    box.max_x = std::max(p1.x, p2.x);
    box.min_y = std::min(p1.y, p2.y);
    box.max_y = std::max(p1.y, p2.y);
    box.expand();
    return box;
}

#endif // GEOMETRY_UTILS_H