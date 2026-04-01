#include "csv_io.h"
#include "polygon_dcel.h"
#include "geometry_utils.h"
#include "spatial_index.h"

#include <cmath>
#include <iostream>
#include <queue>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sys/resource.h>

namespace {
constexpr double kEps = 1e-12;

double cross(const Point2& a, const Point2& b, const Point2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

int orient(const Point2& a, const Point2& b, const Point2& c) {
    const double v = cross(a, b, c);
    if (v > kEps) return 1;
    if (v < -kEps) return -1;
    return 0;
}

bool same_point(const Point2& p, const Point2& q) {
    return std::abs(p.x - q.x) <= 1e-9 && std::abs(p.y - q.y) <= 1e-9;
}

bool on_segment(const Point2& a, const Point2& b, const Point2& p) {
    return p.x >= std::min(a.x, b.x) - kEps && p.x <= std::max(a.x, b.x) + kEps &&
           p.y >= std::min(a.y, b.y) - kEps && p.y <= std::max(a.y, b.y) + kEps &&
           std::abs(cross(a, b, p)) <= kEps;
}

bool segments_intersect_inclusive(const Point2& a, const Point2& b, const Point2& c, const Point2& d) {
    const int o1 = orient(a, b, c);
    const int o2 = orient(a, b, d);
    const int o3 = orient(c, d, a);
    const int o4 = orient(c, d, b);
    if (o1 != o2 && o3 != o4) return true;
    if (o1 == 0 && on_segment(a, b, c)) return true;
    if (o2 == 0 && on_segment(a, b, d)) return true;
    if (o3 == 0 && on_segment(c, d, a)) return true;
    if (o4 == 0 && on_segment(c, d, b)) return true;
    return false;
}


double signed_area_ring(const std::vector<Point2>& pts) {
    if (pts.size() < 3) return 0.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        const Point2& p = pts[i];
        const Point2& q = pts[(i + 1) % pts.size()];
        sum += p.x * q.y - q.x * p.y;
    }
    return 0.5 * sum;
}

double total_signed_area(const std::vector<RingInput>& rings) {
    double sum = 0.0;
    for (const auto& ring : rings) {
        sum += signed_area_ring(ring.points);
    }
    return sum;
}

int side_of_line(const Point2& a, const Point2& b, const Point2& p) {
    const double s = cross(a, b, p);
    if (s > kEps) return 1;
    if (s < -kEps) return -1;
    return 0;
}

double point_line_distance(const Point2& p, const Point2& a, const Point2& b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len = std::hypot(dx, dy);
    if (len < kEps) return 0.0;
    return std::abs((p.x - a.x) * dy - (p.y - a.y) * dx) / len;
}

bool intersect_line_with_segment(double a, double b, double c, const Point2& p, const Point2& q, Point2& out) {
    const double vx = q.x - p.x;
    const double vy = q.y - p.y;
    const double den = a * vx + b * vy;
    if (std::abs(den) < kEps) return false;
    const double t = -(a * p.x + b * p.y + c) / den;
    out.x = p.x + t * vx;
    out.y = p.y + t * vy;
    return true;
}

Point2 point_on_eline(double a, double b, double c, const Point2& fallback) {
    Point2 p = fallback;
    if (std::abs(b) > std::abs(a)) {
        p.y = (-a * p.x - c) / b;
    } else if (std::abs(a) > kEps) {
        p.x = (-b * p.y - c) / a;
    }
    return p;
}

double polygon_area_abs(const std::vector<Point2>& poly) {
    if (poly.size() < 3) return 0.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const Point2& p = poly[i];
        const Point2& q = poly[(i + 1) % poly.size()];
        sum += p.x * q.y - q.x * p.y;
    }
    return std::abs(0.5 * sum);
}

bool segment_line_intersection(const Point2& s, const Point2& e, const Point2& cp1, const Point2& cp2, Point2& out) {
    const double dx = e.x - s.x;
    const double dy = e.y - s.y;
    const double ex = cp2.x - cp1.x;
    const double ey = cp2.y - cp1.y;
    const double den = dx * ey - dy * ex;
    if (std::abs(den) < kEps) return false;
    const double t = ((cp1.x - s.x) * ey - (cp1.y - s.y) * ex) / den;
    out.x = s.x + t * dx;
    out.y = s.y + t * dy;
    return true;
}

std::vector<Point2> convex_polygon_intersection(std::vector<Point2> subject, const std::vector<Point2>& clip) {
    if (subject.empty() || clip.size() < 3) return {};
    const double clip_orient = (clip[1].x - clip[0].x) * (clip[2].y - clip[0].y) -
                               (clip[1].y - clip[0].y) * (clip[2].x - clip[0].x);
    const bool clip_ccw = clip_orient >= 0.0;
    for (std::size_t i = 0; i < clip.size(); ++i) {
        const Point2 cp1 = clip[i];
        const Point2 cp2 = clip[(i + 1) % clip.size()];
        std::vector<Point2> input = subject;
        subject.clear();
        if (input.empty()) break;
        Point2 s = input.back();
        for (const Point2& e : input) {
            const double se = cross(cp1, cp2, e);
            const double ss = cross(cp1, cp2, s);
            const bool e_inside = clip_ccw ? (se >= -kEps) : (se <= kEps);
            const bool s_inside = clip_ccw ? (ss >= -kEps) : (ss <= kEps);
            if (e_inside) {
                if (!s_inside) {
                    Point2 inter{};
                    if (segment_line_intersection(s, e, cp1, cp2, inter)) {
                        subject.push_back(inter);
                    }
                }
                subject.push_back(e);
            } else if (s_inside) {
                Point2 inter{};
                if (segment_line_intersection(s, e, cp1, cp2, inter)) {
                    subject.push_back(inter);
                }
            }
            s = e;
        }
    }
    std::vector<Point2> cleaned;
    for (const Point2& p : subject) {
        if (cleaned.empty() || std::abs(cleaned.back().x - p.x) > 1e-9 || std::abs(cleaned.back().y - p.y) > 1e-9) {
            cleaned.push_back(p);
        }
    }
    if (cleaned.size() > 1 && std::abs(cleaned.front().x - cleaned.back().x) < 1e-9 &&
        std::abs(cleaned.front().y - cleaned.back().y) < 1e-9) {
        cleaned.pop_back();
    }
    return cleaned;
}

bool point_in_polygon(const Point2& p, const std::vector<Point2>& poly) {
    bool inside = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const Point2& a = poly[i];
        const Point2& b = poly[j];
        const bool crosses = ((a.y > p.y) != (b.y > p.y)) &&
                             (p.x < (b.x - a.x) * (p.y - a.y) / ((b.y - a.y) + kEps) + a.x);
        if (crosses) inside = !inside;
    }
    return inside;
}

double quad_triangle_intersection_area(const Point2& a, const Point2& b, const Point2& c, const Point2& d,
                                       const std::vector<Point2>& tri) {
    const std::vector<Point2> quad = {a, b, c, d};
    const Point2 mid_ac{0.5 * (a.x + c.x), 0.5 * (a.y + c.y)};
    std::vector<std::vector<Point2>> tris;
    if (point_in_polygon(mid_ac, quad)) {
        tris = {{a, b, c}, {a, c, d}};
    } else {
        tris = {{a, b, d}, {b, c, d}};
    }
    double area = 0.0;
    for (const auto& t : tris) {
        const std::vector<Point2> inter = convex_polygon_intersection(t, tri);
        area += polygon_area_abs(inter);
    }
    return area;
}

double local_areal_displacement(const Point2& a, const Point2& b, const Point2& c, const Point2& d, const Point2& e) {
    const double area_old = polygon_area_abs({a, b, c, d});
    const double area_new = polygon_area_abs({a, e, d});
    const std::vector<Point2> tri_new = {a, e, d};
    const double inter = quad_triangle_intersection_area(a, b, c, d, tri_new);
    const double sym_diff = area_old + area_new - 2.0 * inter;
    return sym_diff > 0.0 ? sym_diff : 0.0;
}

bool would_create_intersection(const PolygonDCEL& p, int he_ab, const Point2& e) {
    const int he_bc = p.halfedges_[he_ab].next;
    if (he_bc < 0) return true;
    const int he_cd = p.halfedges_[he_bc].next;
    if (he_cd < 0) return true;
    const int he_de = p.halfedges_[he_cd].next;
    if (he_de < 0) return true;

    const Point2& a = p.vertices_[p.halfedges_[he_ab].origin].p;
    const Point2& d = p.vertices_[p.halfedges_[he_de].origin].p;

    for (int i = 0; i < static_cast<int>(p.halfedges_.size()); ++i) {
        if (!p.halfedges_[i].alive) continue;
        if (i == he_ab || i == he_bc || i == he_cd) continue;
        const int j = p.halfedges_[i].next;
        if (j < 0 || !p.halfedges_[j].alive) continue;
        const Point2& p1 = p.vertices_[p.halfedges_[i].origin].p;
        const Point2& p2 = p.vertices_[p.halfedges_[j].origin].p;
        auto bad = [&](const Point2& u, const Point2& v) {
            if (!segments_intersect_inclusive(u, v, p1, p2)) return false;
            const bool share_endpoint =
                same_point(p1, u) || same_point(p2, u) || same_point(p1, v) || same_point(p2, v);
            if (share_endpoint) return false;
            return true;
        };
        if (bad(a, e) || bad(e, d)) return true;
    }
    return false;
}

bool compute_candidate(const PolygonDCEL& p, int he_ab, Point2& e, double& displacement) {
    if (he_ab < 0 || he_ab >= static_cast<int>(p.halfedges_.size())) return false;
    if (!p.halfedges_[he_ab].alive) return false;

    const int he_bc = p.halfedges_[he_ab].next;
    if (he_bc < 0 || !p.halfedges_[he_bc].alive) return false;
    const int he_cd = p.halfedges_[he_bc].next;
    if (he_cd < 0 || !p.halfedges_[he_cd].alive) return false;
    const int he_de = p.halfedges_[he_cd].next;
    if (he_de < 0 || !p.halfedges_[he_de].alive) return false;
    if (he_bc == he_ab || he_cd == he_ab || he_de == he_ab || he_cd == he_bc || he_de == he_bc) return false;

    const int ring_index = p.halfedges_[he_ab].ring_index;
    if (ring_index < 0 || ring_index >= static_cast<int>(p.ring_ids_sorted().size())) return false;
    if (p.halfedges_[he_ab].ring_index != p.halfedges_[he_bc].ring_index ||
        p.halfedges_[he_ab].ring_index != p.halfedges_[he_cd].ring_index ||
        p.halfedges_[he_ab].ring_index != p.halfedges_[he_de].ring_index) {
        return false;
    }

    const Point2& a = p.vertices_[p.halfedges_[he_ab].origin].p;
    const Point2& b = p.vertices_[p.halfedges_[he_bc].origin].p;
    const Point2& c = p.vertices_[p.halfedges_[he_cd].origin].p;
    const Point2& d = p.vertices_[p.halfedges_[he_de].origin].p;

    const double la = d.y - a.y;
    const double lb = a.x - d.x;
    const double lc = -b.y * a.x + (a.y - c.y) * b.x + (b.y - d.y) * c.x + c.y * d.x;

    const int side_b_ad = side_of_line(a, d, b);
    const int side_c_ad = side_of_line(a, d, c);
    const double dist_b = point_line_distance(b, a, d);
    const double dist_c = point_line_distance(c, a, d);

    const Point2 on_eline = point_on_eline(la, lb, lc, a);
    const int side_eline_ad = side_of_line(a, d, on_eline);

    bool choose_ab = true;
    if (side_b_ad == side_c_ad) {
        if (dist_b > dist_c + kEps) {
            choose_ab = true;
        } else if (dist_c > dist_b + kEps) {
            choose_ab = false;
        } else {
            choose_ab = (side_b_ad == side_eline_ad);
        }
    } else {
        choose_ab = (side_b_ad == side_eline_ad);
    }

    if (!intersect_line_with_segment(la, lb, lc, choose_ab ? a : c, choose_ab ? b : d, e)) {
        if (!intersect_line_with_segment(la, lb, lc, choose_ab ? c : a, choose_ab ? d : b, e)) {
            return false;
        }
    }

    if (would_create_intersection(p, he_ab, e)) return false;

    displacement = local_areal_displacement(a, b, c, d, e);
    return true;
}
} // namespace

struct Candidate {
    int he;
    double cost;
    std::uint64_t version;
    Point2 replacement;
};

struct Compare {
    bool operator()(const Candidate& a, const Candidate& b) {
        if (std::abs(a.cost - b.cost) > 1e-12) {
            return a.cost > b.cost;
        }
        return a.he > b.he;
    }
};

bool build(PolygonDCEL& p, int he, Candidate& c) {
    if (!p.halfedges_[he].alive) return false;
    Point2 e{};
    double d = 0.0;
    if (!compute_candidate(p, he, e, d)) return false;
    c.he = he;
    c.cost = d;
    c.version = p.he_version[he];
    c.replacement = e;
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_csv> <target_vertices>\n";
        return 1;
    }

    auto rings = read_input_csv(argv[1]);
    int target = atoi(argv[2]);
    const double input_area = total_signed_area(rings);

    PolygonDCEL poly = PolygonDCEL::from_rings(rings);

    auto start_time = std::chrono::high_resolution_clock::now();
    double total_displacement = 0.0;

    while (poly.total_vertices() > (size_t)target) {
        std::priority_queue<Candidate, std::vector<Candidate>, Compare> pq;
        for (int i = 0; i < static_cast<int>(poly.halfedges_.size()); ++i) {
            Candidate c;
            if (build(poly, i, c)) pq.push(c);
        }
        if (pq.empty()) {
            break;
        }
        bool collapsed = false;
        while (!pq.empty()) {
            const Candidate top = pq.top();
            pq.pop();
            const int he_bc = poly.halfedges_[top.he].next;
            if (he_bc < 0 || !poly.halfedges_[he_bc].alive) continue;
            const int v_b = poly.halfedges_[he_bc].origin;
            const Point2 old_b = poly.vertices_[v_b].p;
            poly.vertices_[v_b].p = top.replacement;
            if (!poly.collapse_quad_by_halfedge(top.he)) {
                poly.vertices_[v_b].p = old_b;
                continue;
            }
            total_displacement += top.cost;
            collapsed = true;
            break;
        }
        if (!collapsed) break;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    long peak_memory = usage.ru_maxrss; 

    // Print metrics to standard ERROR (so it doesn't break the CSV output)
    // Format: STATS, Target_Vertices, Actual_Vertices, Time_Seconds, Peak_Memory
        std::cerr << argv[1] << "," << target << "," << poly.total_vertices() << "," 
              << elapsed.count() << "," << peak_memory << "\n";

    std::cout << "ring_id,vertex_id,x,y\n";
    std::cout << std::defaultfloat << std::setprecision(10);
    for (int rid : poly.ring_ids_sorted()) {
        auto pts = poly.ring_points(rid);
        for (int i = 0; i < (int)pts.size(); i++) {
            std::cout << rid << "," << i << "," << pts[i].x << "," << pts[i].y << "\n";
        }
    }

    const double output_area = poly.total_signed_area();
    std::cout << std::scientific << std::setprecision(6);
    std::cout << "Total signed area in input: " << input_area << "\n";
    std::cout << "Total signed area in output: " << output_area << "\n";
    std::cout << "Total areal displacement: " << total_displacement << "\n";
}
