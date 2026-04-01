#ifndef GEOMETRY_UTILS_H
#define GEOMETRY_UTILS_H

#include <algorithm>
#include "polygon_dcel.h"

// Returns true if point C is counter-clockwise to the line AB
inline bool ccw(const Point2& A, const Point2& B, const Point2& C) {
    return (C.y - A.y) * (B.x - A.x) > (B.y - A.y) * (C.x - A.x);
}

// Returns true if line segments AB and CD intersect
inline bool segments_intersect(const Point2& A, const Point2& B, 
                               const Point2& C, const Point2& D) {
    return ccw(A, C, D) != ccw(B, C, D) && ccw(A, B, C) != ccw(A, B, D);
}

// Bounding box structure used by the Spatial Index
struct AABB {
    double min_x, min_y, max_x, max_y;
    
    void expand(double epsilon = 1e-9) {
        min_x -= epsilon; min_y -= epsilon;
        max_x += epsilon; max_y += epsilon;
    }
    
    bool intersects(const AABB& other) const {
        return !(max_x < other.min_x || min_x > other.max_x || 
                 max_y < other.min_y || min_y > other.max_y);
    }
};

// Helper to compute an AABB from two segment endpoints
inline AABB compute_aabb(const Point2& p1, const Point2& p2) {
    AABB box;
    box.min_x = std::min(p1.x, p2.x);
    box.max_x = std::max(p1.x, p2.x);
    box.min_y = std::min(p1.y, p2.y);
    box.max_y = std::max(p1.y, p2.y);
    box.expand();
    return box;
}

#endif // GEOMETRY_UTILS_H