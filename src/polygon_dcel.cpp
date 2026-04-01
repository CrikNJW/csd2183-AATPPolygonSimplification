#include "polygon_dcel.h"

#include <algorithm>
#include <cmath>
#include <limits>

PolygonDCEL PolygonDCEL::from_rings(const std::vector<RingInput>& rings) {
    PolygonDCEL out;
    out.rings_.reserve(rings.size());

    int next_vertex_id = 0;
    int next_halfedge_id = 0;

    for (std::size_t i = 0; i < rings.size(); ++i) {
        const RingInput& in = rings[i];
        if (in.points.size() < 3) {
            throw std::runtime_error("Each ring must contain at least 3 vertices.");
        }

        Ring ring;
        ring.ring_id = in.ring_id;
        ring.is_hole = (in.ring_id != 0);
        ring.live_vertices = static_cast<int>(in.points.size());

        const int ring_index = static_cast<int>(out.rings_.size());
        out.rings_.push_back(ring);
        out.ring_id_to_index_[in.ring_id] = ring_index;

        std::vector<int> ring_vertex_ids;
        ring_vertex_ids.reserve(in.points.size());
        for (const Point2& p : in.points) {
            Vertex v;
            v.id = next_vertex_id++;
            v.p = p;
            out.vertices_.push_back(v);
            ring_vertex_ids.push_back(v.id);
        }

        std::vector<int> ring_halfedge_ids;
        ring_halfedge_ids.reserve(in.points.size());
        for (std::size_t j = 0; j < in.points.size(); ++j) {
            HalfEdge he;
            he.id = next_halfedge_id++;
            he.origin = ring_vertex_ids[j];
            he.ring_index = ring_index;
            out.halfedges_.push_back(he);
            ring_halfedge_ids.push_back(he.id);
        }

        const int n = static_cast<int>(ring_halfedge_ids.size());
        for (int j = 0; j < n; ++j) {
            const int he_id = ring_halfedge_ids[j];
            const int next_id = ring_halfedge_ids[(j + 1) % n];
            const int prev_id = ring_halfedge_ids[(j - 1 + n) % n];
            out.halfedges_[he_id].next = next_id;
            out.halfedges_[he_id].prev = prev_id;
            out.vertices_[out.halfedges_[he_id].origin].incident_halfedge = he_id;
        }

        out.rings_[ring_index].first_halfedge = ring_halfedge_ids.front();
    }

    return out;
}

std::size_t PolygonDCEL::ring_count() const {
    return rings_.size();
}

std::size_t PolygonDCEL::total_vertices() const {
    std::size_t total = 0;
    for (const Ring& r : rings_) {
        total += static_cast<std::size_t>(r.live_vertices);
    }
    return total;
}

int PolygonDCEL::ring_index_for_id(int ring_id) const {
    auto it = ring_id_to_index_.find(ring_id);
    if (it == ring_id_to_index_.end()) {
        throw std::runtime_error("Unknown ring_id.");
    }
    return it->second;
}

std::vector<int> PolygonDCEL::ring_halfedges(int ring_index) const {
    const Ring& ring = rings_[ring_index];
    if (ring.first_halfedge < 0) {
        return {};
    }

    std::vector<int> result;
    result.reserve(static_cast<std::size_t>(std::max(ring.live_vertices, 0)));

    const int start = ring.first_halfedge;
    int he = start;
    int guard = 0;
    const int max_guard = static_cast<int>(halfedges_.size()) + 5;
    do {
        if (he < 0 || he >= static_cast<int>(halfedges_.size())) {
            throw std::runtime_error("Invalid halfedge linkage.");
        }
        const HalfEdge& e = halfedges_[he];
        if (!e.alive) {
            throw std::runtime_error("Dead halfedge encountered in active ring traversal.");
        }
        result.push_back(he);
        he = e.next;
        ++guard;
        if (guard > max_guard) {
            throw std::runtime_error("Ring traversal overflow (possibly broken cycle).");
        }
    } while (he != start);

    return result;
}

std::vector<int> PolygonDCEL::ring_ids_sorted() const {
    std::vector<int> ids;
    ids.reserve(rings_.size());
    for (const Ring& r : rings_) {
        ids.push_back(r.ring_id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<Point2> PolygonDCEL::ring_points(int ring_id) const {
    const int ring_index = ring_index_for_id(ring_id);
    std::vector<Point2> pts;
    for (int he_id : ring_halfedges(ring_index)) {
        const HalfEdge& he = halfedges_[he_id];
        const Vertex& v = vertices_[he.origin];
        pts.push_back(v.p);
    }
    return pts;
}

double PolygonDCEL::ring_signed_area(int ring_id) const {
    const std::vector<Point2> pts = ring_points(ring_id);
    if (pts.size() < 3) {
        return 0.0;
    }
    long double a = 0.0L;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        const Point2& p = pts[i];
        const Point2& q = pts[(i + 1) % pts.size()];
        a += static_cast<long double>(p.x) * static_cast<long double>(q.y) -
             static_cast<long double>(q.x) * static_cast<long double>(p.y);
    }
    return static_cast<double>(0.5L * a);
}

double PolygonDCEL::total_signed_area() const {
    long double sum = 0.0L;
    for (const Ring& r : rings_) {
        sum += static_cast<long double>(ring_signed_area(r.ring_id));
    }
    return static_cast<double>(sum);
}

int PolygonDCEL::halfedge_at_local_index(int ring_index, int local_index) const {
    std::vector<int> hes = ring_halfedges(ring_index);
    if (local_index < 0 || local_index >= static_cast<int>(hes.size())) {
        throw std::runtime_error("local_index out of range.");
    }
    return hes[local_index];
}

bool PolygonDCEL::collapse_quad(int ring_id, int b_local_index, const Point2& e) {
    const int ring_index = ring_index_for_id(ring_id);
    Ring& ring = rings_[ring_index];
    if (ring.live_vertices <= 4) {
        return false;
    }

    std::vector<int> hes = ring_halfedges(ring_index);
    if (hes.size() < 5 || b_local_index < 0 || b_local_index >= static_cast<int>(hes.size())) {
        return false;
    }
    const int he_ab = hes[(b_local_index - 1 + static_cast<int>(hes.size())) % static_cast<int>(hes.size())];

    const int he_bc = halfedges_[he_ab].next;
    const int he_cd = halfedges_[he_bc].next;

    const int v_b = halfedges_[he_ab].origin;
    const int v_c = halfedges_[he_bc].origin;
    const int v_d = halfedges_[he_cd].origin;
    (void)v_d;

    if (!halfedges_[he_ab].alive || !halfedges_[he_bc].alive || !halfedges_[he_cd].alive) {
        return false;
    }
    if (!vertices_[v_b].alive || !vertices_[v_c].alive) {
        return false;
    }

    vertices_[v_b].p = e;
    halfedges_[he_ab].next = he_cd;
    halfedges_[he_cd].prev = he_ab;

    halfedges_[he_bc].alive = false;
    vertices_[v_c].alive = false;
    ring.live_vertices -= 1;

    if (ring.first_halfedge == he_bc) {
        ring.first_halfedge = he_cd;
    }

    return true;
}
