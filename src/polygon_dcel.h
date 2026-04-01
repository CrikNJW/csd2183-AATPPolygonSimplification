#ifndef POLYGON_DCEL_H
#define POLYGON_DCEL_H

#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <vector>

struct Point2 {
    double x{};
    double y{};
};

struct RingInput {
    int ring_id{};
    std::vector<Point2> points;
};

class PolygonDCEL {
public:
    struct Vertex {
        int id{-1};
        Point2 p{};
        int incident_halfedge{-1};
        bool alive{true};
    };

    struct HalfEdge {
        int id{-1};
        int origin{-1};
        int next{-1};
        int prev{-1};
        int ring_index{-1};
        bool alive{true};
    };

    struct Ring {
        int ring_id{-1};
        int first_halfedge{-1};
        int live_vertices{0};
        bool is_hole{false};
    };

    static PolygonDCEL from_rings(const std::vector<RingInput>& rings);

    std::size_t ring_count() const;
    std::size_t total_vertices() const;

    std::vector<int> ring_ids_sorted() const;
    std::vector<Point2> ring_points(int ring_id) const;

    double ring_signed_area(int ring_id) const;
    double total_signed_area() const;

    // Collapse A->B->C->D to A->E->D for the ring-local vertex index of B.
    // Returns false when the operation cannot be applied.
    bool collapse_quad(int ring_id, int b_local_index, const Point2& e);

private:
    std::vector<Vertex> vertices_;
    std::vector<HalfEdge> halfedges_;
    std::vector<Ring> rings_;
    std::unordered_map<int, int> ring_id_to_index_;

    int ring_index_for_id(int ring_id) const;
    std::vector<int> ring_halfedges(int ring_index) const;
    int halfedge_at_local_index(int ring_index, int local_index) const;
};

#endif
