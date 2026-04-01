#include "csv_io.h"
#include "polygon_dcel.h"
#include "geometry_utils.h"
#include "spatial_index.h"

#include <iostream>
#include <queue>

struct Candidate {
    int he;
    double cost;
    std::uint64_t version;
};

struct Compare {
    bool operator()(const Candidate& a, const Candidate& b) {
        return a.cost > b.cost;
    }
};

bool build(PolygonDCEL& p, int he, Candidate& c) {
    if (!p.halfedges_[he].alive) return false;
    c.he = he;
    c.cost = 1.0;
    c.version = p.he_version[he];
    return true;
}

int main(int argc, char** argv) {
    auto rings = read_input_csv(argv[1]);
    int target = atoi(argv[2]);

    PolygonDCEL poly = PolygonDCEL::from_rings(rings);

    std::priority_queue<Candidate, std::vector<Candidate>, Compare> pq;

    for (int i = 0; i < (int)poly.halfedges_.size(); i++) {
        Candidate c;
        if (build(poly, i, c)) pq.push(c);
    }

    while (!pq.empty() && poly.total_vertices() > (size_t)target) {
        auto top = pq.top(); pq.pop();

        if (top.version != poly.he_version[top.he]) continue;

        if (!poly.collapse_quad_by_halfedge(top.he)) continue;

        int he = top.he;
        for (int i = 0; i < 4; i++) {
            Candidate c;
            if (build(poly, he, c)) pq.push(c);
            he = poly.halfedges_[he].next;
        }
    }

    std::cout << "ring_id,vertex_id,x,y\n";
    for (int rid : poly.ring_ids_sorted()) {
        auto pts = poly.ring_points(rid);
        for (int i = 0; i < (int)pts.size(); i++) {
            std::cout << rid << "," << i << "," << pts[i].x << "," << pts[i].y << "\n";
        }
    }
}