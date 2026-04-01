#include "polygon_dcel.h"
#include <algorithm>
#include <cmath>

PolygonDCEL PolygonDCEL::from_rings(const std::vector<RingInput>& rings) {
    PolygonDCEL out;

    int vid = 0;
    int hid = 0;

    for (const auto& in : rings) {
        Ring r;
        r.ring_id = in.ring_id;
        r.live_vertices = (int)in.points.size();
        int ring_index = (int)out.rings_.size();
        out.rings_.push_back(r);
        out.ring_id_to_index_[r.ring_id] = ring_index;

        std::vector<int> vids;

        for (auto& p : in.points) {
            Vertex v;
            v.id = vid++;
            v.p = p;
            out.vertices_.push_back(v);
            vids.push_back(v.id);
        }

        std::vector<int> hes;

        for (int i = 0; i < (int)vids.size(); i++) {
            HalfEdge he;
            he.id = hid++;
            he.origin = vids[i];
            he.ring_index = ring_index;
            out.halfedges_.push_back(he);
            hes.push_back(he.id);
        }

        int n = (int)hes.size();
        for (int i = 0; i < n; i++) {
            out.halfedges_[hes[i]].next = hes[(i+1)%n];
            out.halfedges_[hes[i]].prev = hes[(i-1+n)%n];
        }

        out.rings_[ring_index].first_halfedge = hes[0];
    }

    out.he_version.resize(out.halfedges_.size(), 0);

    return out;
}

std::size_t PolygonDCEL::total_vertices() const {
    std::size_t total = 0;
    for (auto& r : rings_) total += r.live_vertices;
    return total;
}

std::vector<int> PolygonDCEL::ring_ids_sorted() const {
    std::vector<int> ids;
    for (auto& r : rings_) ids.push_back(r.ring_id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<int> PolygonDCEL::ring_halfedges(int ring_index) const {
    std::vector<int> res;
    int start = rings_[ring_index].first_halfedge;
    int he = start;
    do {
        res.push_back(he);
        he = halfedges_[he].next;
    } while (he != start);
    return res;
}

std::vector<Point2> PolygonDCEL::ring_points(int ring_id) const {
    int idx = ring_id_to_index_.at(ring_id);
    std::vector<Point2> pts;
    for (int he : ring_halfedges(idx)) {
        pts.push_back(vertices_[halfedges_[he].origin].p);
    }
    return pts;
}

double PolygonDCEL::total_signed_area() const {
    double sum = 0;
    for (auto& r : rings_) {
        auto pts = ring_points(r.ring_id);
        for (int i = 0; i < (int)pts.size(); i++) {
            auto& p = pts[i];
            auto& q = pts[(i+1)%pts.size()];
            sum += p.x*q.y - q.x*p.y;
        }
    }
    return 0.5 * sum;
}

bool PolygonDCEL::collapse_quad(int ring_id, int b_index, const Point2& e) {
    int ring_index = ring_id_to_index_.at(ring_id);
    auto hes = ring_halfedges(ring_index);

    if (hes.size() <= 4) return false;

    int he_ab = hes[(b_index-1+hes.size())%hes.size()];
    int he_bc = halfedges_[he_ab].next;
    int he_cd = halfedges_[he_bc].next;

    int vb = halfedges_[he_bc].origin;
    int vc = halfedges_[he_cd].origin;

    vertices_[vb].p = e;

    halfedges_[he_ab].next = he_cd;
    halfedges_[he_cd].prev = he_ab;
    halfedges_[he_cd].origin = vb;

    halfedges_[he_bc].alive = false;
    vertices_[vc].alive = false;

    rings_[ring_index].live_vertices--;

    return true;
}

bool PolygonDCEL::collapse_quad_by_halfedge(int he_ab) {
    std::vector<int> ignored;
    return collapse_quad_by_halfedge(he_ab, ignored);
}

bool PolygonDCEL::collapse_quad_by_halfedge(int he_ab, std::vector<int>& updated_halfedges) {
    if (!halfedges_[he_ab].alive) return false;

    int he_bc = halfedges_[he_ab].next;
    if (he_bc < 0 || !halfedges_[he_bc].alive) return false;

    int he_cd = halfedges_[he_bc].next;
    if (he_cd < 0 || !halfedges_[he_cd].alive) return false;

    int ring_index = halfedges_[he_ab].ring_index;
    if (rings_[ring_index].live_vertices <= 4) return false;

    int he_za = halfedges_[he_ab].prev;
    int he_de = halfedges_[he_cd].next;

    int vb = halfedges_[he_bc].origin;
    int vc = halfedges_[he_cd].origin;

    Point2 e = vertices_[vb].p;
    vertices_[vb].p = e;

    halfedges_[he_ab].next = he_cd;
    halfedges_[he_cd].prev = he_ab;
    halfedges_[he_cd].origin = vb;

    halfedges_[he_bc].alive = false;
    vertices_[vc].alive = false;

    if (rings_[ring_index].first_halfedge == he_bc) {
        rings_[ring_index].first_halfedge = he_cd;
    }

    rings_[ring_index].live_vertices--;

    updated_halfedges.clear();
    auto mark_updated = [&](int he_id) {
        if (he_id < 0) return;
        if (!halfedges_[he_id].alive) return;
        he_version[he_id]++;
        updated_halfedges.push_back(he_id);
    };

    mark_updated(he_za);
    mark_updated(he_ab);
    mark_updated(he_cd);
    mark_updated(he_de);
    if (he_bc >= 0) {
        he_version[he_bc]++;
    }

    return true;
}
