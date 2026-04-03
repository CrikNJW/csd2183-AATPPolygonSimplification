#include "csv_io.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <psapi.h>
#include <windows.h>
#else
#include <sys/resource.h>
#endif

namespace {

struct Vec2 {
    double x{};
    double y{};
};

struct Vertex {
    double x{};
    double y{};
    int original_id{-1};
    bool removed{false};
    Vertex* prev{nullptr};
    Vertex* next{nullptr};
};

class Ring {
public:
    explicit Ring(int id) : ring_id(id) {}
    ~Ring() {
        flush_garbage();
        clear();
    }

    Ring(const Ring&) = delete;
    Ring& operator=(const Ring&) = delete;

    void push_back(double x, double y, int vid) {
        Vertex* v = new Vertex{x, y, vid, false, nullptr, nullptr};
        if (vid >= 0 && vid + 1 > next_generated_id) next_generated_id = vid + 1;
        if (!head) {
            head = v;
            v->prev = v;
            v->next = v;
        } else {
            Vertex* tail = head->prev;
            tail->next = v;
            v->prev = tail;
            v->next = head;
            head->prev = v;
        }
        ++size;
    }

    Vertex* remove(Vertex* v) {
        Vertex* nxt = v->next;
        v->prev->next = v->next;
        v->next->prev = v->prev;
        if (head == v) head = nxt;
        v->removed = true;
        --size;
        garbage.push_back(v);
        return nxt;
    }

    Vertex* insert_after(Vertex* prev_v, double x, double y) {
        Vertex* v = new Vertex{x, y, next_generated_id++, false, nullptr, nullptr};
        Vertex* nxt = prev_v->next;
        prev_v->next = v;
        v->prev = prev_v;
        v->next = nxt;
        nxt->prev = v;
        ++size;
        return v;
    }

    void flush_garbage() {
        for (Vertex* v : garbage) delete v;
        garbage.clear();
    }

    int ring_id{-1};
    size_t size{0};
    Vertex* head{nullptr};

private:
    int next_generated_id{0};
    std::vector<Vertex*> garbage;

    void clear() {
        if (!head) return;
        Vertex* cur = head->next;
        while (cur != head) {
            Vertex* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
        delete head;
        head = nullptr;
        size = 0;
    }
};

struct Polygon {
    std::vector<std::unique_ptr<Ring>> rings;

    size_t total_vertices() const {
        size_t n = 0;
        for (const auto& r : rings) n += r->size;
        return n;
    }
};

double signed_area(const Ring& ring) {
    if (!ring.head || ring.size < 3) return 0.0;
    double area = 0.0;
    const Vertex* v = ring.head;
    do {
        area += v->x * v->next->y - v->next->x * v->y;
        v = v->next;
    } while (v != ring.head);
    return area * 0.5;
}

bool point_eq(Vec2 a, Vec2 b) {
    const double eps = 1e-12;
    return std::abs(a.x - b.x) <= eps && std::abs(a.y - b.y) <= eps;
}

int orient_sign(Vec2 a, Vec2 b, Vec2 c) {
    long double abx = static_cast<long double>(b.x) - static_cast<long double>(a.x);
    long double aby = static_cast<long double>(b.y) - static_cast<long double>(a.y);
    long double acx = static_cast<long double>(c.x) - static_cast<long double>(a.x);
    long double acy = static_cast<long double>(c.y) - static_cast<long double>(a.y);
    long double v = abx * acy - aby * acx;
    long double tol = 1e-12L * (fabsl(abx) + fabsl(aby) + fabsl(acx) + fabsl(acy) + 1.0L);
    if (fabsl(v) <= tol) return 0;
    return (v > 0) ? 1 : -1;
}

bool on_segment(Vec2 a, Vec2 b, Vec2 p) {
    const double eps = 1e-12;
    if (orient_sign(a, b, p) != 0) return false;
    return (p.x >= std::min(a.x, b.x) - eps && p.x <= std::max(a.x, b.x) + eps &&
            p.y >= std::min(a.y, b.y) - eps && p.y <= std::max(a.y, b.y) + eps);
}

bool collinear_overlap_nontrivial(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
    if (orient_sign(a, b, c) != 0 || orient_sign(a, b, d) != 0) return false;
    const double eps = 1e-12;
    const double abx = std::abs(b.x - a.x);
    const double aby = std::abs(b.y - a.y);
    auto proj = [&](Vec2 p) { return (abx >= aby) ? p.x : p.y; };
    double a0 = proj(a), a1 = proj(b);
    double c0 = proj(c), c1 = proj(d);
    if (a0 > a1) std::swap(a0, a1);
    if (c0 > c1) std::swap(c0, c1);
    double overlap = std::min(a1, c1) - std::max(a0, c0);
    return overlap > eps;
}

bool segments_intersect_nontrivial(Vec2 p1, Vec2 p2, Vec2 p3, Vec2 p4, bool ignore_shared_endpoints) {
    const bool shared_endpoint =
        point_eq(p1, p3) || point_eq(p1, p4) || point_eq(p2, p3) || point_eq(p2, p4);

    if (collinear_overlap_nontrivial(p1, p2, p3, p4)) return true;

    int o1 = orient_sign(p1, p2, p3);
    int o2 = orient_sign(p1, p2, p4);
    int o3 = orient_sign(p3, p4, p1);
    int o4 = orient_sign(p3, p4, p2);

    if (o1 * o2 < 0 && o3 * o4 < 0) return true;

    bool intersects =
        (o1 == 0 && on_segment(p1, p2, p3)) ||
        (o2 == 0 && on_segment(p1, p2, p4)) ||
        (o3 == 0 && on_segment(p3, p4, p1)) ||
        (o4 == 0 && on_segment(p3, p4, p2));

    if (!intersects) return false;
    if (ignore_shared_endpoints && shared_endpoint) return false;
    return true;
}

Vec2 intersect_Estar_with_line(double a, double b, double c, Vec2 p1, Vec2 p2) {
    double dx = p2.x - p1.x;
    double dy = p2.y - p1.y;
    double denom = a * dx + b * dy;
    if (std::abs(denom) < 1e-15) {
        Vec2 mid{(p1.x + p2.x) * 0.5, (p1.y + p2.y) * 0.5};
        double n2 = a * a + b * b;
        double val = a * mid.x + b * mid.y + c;
        return {mid.x - a * val / n2, mid.y - b * val / n2};
    }
    double t = -(a * p1.x + b * p1.y + c) / denom;
    return {p1.x + t * dx, p1.y + t * dy};
}

double tri_area(Vec2 p1, Vec2 p2, Vec2 p3) {
    return 0.5 * ((p2.x - p1.x) * (p3.y - p1.y) - (p3.x - p1.x) * (p2.y - p1.y));
}

double poly_area(const std::vector<Vec2>& pts) {
    if (pts.size() < 3) return 0.0;
    long double acc = 0.0L;
    for (size_t i = 0, n = pts.size(); i < n; ++i) {
        const Vec2& a = pts[i];
        const Vec2& b = pts[(i + 1) % n];
        acc += static_cast<long double>(a.x) * static_cast<long double>(b.y)
             - static_cast<long double>(b.x) * static_cast<long double>(a.y);
    }
    return std::abs(static_cast<double>(acc * 0.5L));
}

bool seg_intersect_params(Vec2 p1, Vec2 p2, Vec2 p3, Vec2 p4, double& t, double& s) {
    double dx12 = p2.x - p1.x, dy12 = p2.y - p1.y;
    double dx34 = p4.x - p3.x, dy34 = p4.y - p3.y;
    double denom = dx12 * dy34 - dy12 * dx34;
    if (std::abs(denom) < 1e-15) return false;
    double dx13 = p3.x - p1.x, dy13 = p3.y - p1.y;
    t = (dx13 * dy34 - dy13 * dx34) / denom;
    s = (dx13 * dy12 - dy13 * dx12) / denom;
    const double eps = 1e-9;
    return (t >= -eps && t <= 1.0 + eps && s >= -eps && s <= 1.0 + eps);
}

double areal_displacement(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Vec2 e) {
    double t, s;
    const double epsCross = 1e-10;

    if (seg_intersect_params(e, d, b, c, t, s)) {
        if (!(t <= epsCross || t >= 1.0 - epsCross || s <= epsCross || s >= 1.0 - epsCross)) {
            Vec2 i{e.x + t * (d.x - e.x), e.y + t * (d.y - e.y)};
            double a1 = tri_area(a, b, i) + tri_area(a, i, e);
            double a2 = tri_area(i, c, d) + tri_area(i, d, e);
            return std::abs(a1) + std::abs(a2);
        }
    }

    if (seg_intersect_params(a, e, b, c, t, s)) {
        if (!(t <= epsCross || t >= 1.0 - epsCross || s <= epsCross || s >= 1.0 - epsCross)) {
            Vec2 i{a.x + t * (e.x - a.x), a.y + t * (e.y - a.y)};
            double a1 = std::abs(tri_area(a, b, i));
            double a2 = std::abs(tri_area(i, c, d) + tri_area(i, d, e));
            return a1 + a2;
        }
    }

    if (seg_intersect_params(e, d, a, b, t, s)) {
        if (!(t <= epsCross || t >= 1.0 - epsCross || s <= epsCross || s >= 1.0 - epsCross)) {
            Vec2 i{e.x + t * (d.x - e.x), e.y + t * (d.y - e.y)};
            double a1 = std::abs(tri_area(a, i, e));
            double a2 = poly_area({i, b, c, d, e});
            return a1 + a2;
        }
    }

    if (seg_intersect_params(a, e, c, d, t, s)) {
        if (!(t <= epsCross || t >= 1.0 - epsCross || s <= epsCross || s >= 1.0 - epsCross)) {
            Vec2 j{a.x + t * (e.x - a.x), a.y + t * (e.y - a.y)};
            double a1 = std::abs(tri_area(j, d, e));
            double a2 = poly_area({a, b, c, j, e});
            return a1 + a2;
        }
    }

    double area = (a.x * b.y - b.x * a.y) +
                  (b.x * c.y - c.x * b.y) +
                  (c.x * d.y - d.x * c.y) +
                  (d.x * e.y - e.x * d.y) +
                  (e.x * a.y - a.x * e.y);
    double shoelace_disp = std::abs(area) * 0.5;
    double alt1 = std::abs(tri_area(a, b, c) + tri_area(a, c, e)) + std::abs(tri_area(c, d, e));
    double alt2 = std::abs(tri_area(a, b, e)) + std::abs(tri_area(b, c, d) + tri_area(b, d, e));
    double alt3 = std::abs(tri_area(a, b, e)) + std::abs(tri_area(e, c, d));
    return std::max(std::max(shoelace_disp, alt1), std::max(alt2, alt3));
}

Vec2 compute_E(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
    double aa = d.y - a.y;
    double bb = a.x - d.x;
    double cc = -b.y * a.x + (a.y - c.y) * b.x + (b.y - d.y) * c.x + c.y * d.x;

    double ad2 = (d.x - a.x) * (d.x - a.x) + (d.y - a.y) * (d.y - a.y);
    if (ad2 < 1e-18) {
        return {(b.x + c.x) * 0.5, (b.y + c.y) * 0.5};
    }

    double val_A = aa * a.x + bb * a.y + cc;
    double scale = std::abs(aa) + std::abs(bb) + 1.0;
    if (std::abs(val_A) < 1e-10 * scale) {
        return {(a.x + d.x) * 0.5, (a.y + d.y) * 0.5};
    }

    Vec2 eab = intersect_Estar_with_line(aa, bb, cc, a, b);
    Vec2 ecd = intersect_Estar_with_line(aa, bb, cc, c, d);
    double dab = areal_displacement(a, b, c, d, eab);
    double dcd = areal_displacement(a, b, c, d, ecd);
    return (dab <= dcd) ? eab : ecd;
}

bool ring_contains_point(const Ring& ring, Vec2 p) {
    if (!ring.head || ring.size == 0) return false;
    const Vertex* v = ring.head;
    do {
        if (!v->removed && point_eq({v->x, v->y}, p)) return true;
        v = v->next;
    } while (v != ring.head);
    return false;
}

bool collapse_causes_intersection(const Ring& ring, Vertex* a, Vertex* b, Vertex* c, Vertex* d, Vec2 e) {
    Vec2 va{a->x, a->y};
    Vec2 vd{d->x, d->y};
    if (point_eq(va, e) || point_eq(vd, e)) return true;
    if (!point_eq(va, e) && !point_eq(vd, e) && ring_contains_point(ring, e)) return true;

    const Vertex* u = ring.head;
    do {
        const Vertex* w = u->next;
        if (u == b || u == c || w == b || w == c) {
            u = w;
            continue;
        }
        Vec2 pu{u->x, u->y};
        Vec2 pw{w->x, w->y};
        if (segments_intersect_nontrivial(va, e, pu, pw, true)) return true;
        if (segments_intersect_nontrivial(e, vd, pu, pw, true)) return true;
        u = w;
    } while (u != ring.head);
    return false;
}

bool collapse_causes_cross_ring_intersection(const Polygon& poly, int ring_id, Vertex* a, Vertex* b, Vertex* c, Vertex* d, Vec2 e) {
    const Ring& ring = *poly.rings[ring_id];
    if (collapse_causes_intersection(ring, a, b, c, d, e)) return true;

    Vec2 va{a->x, a->y};
    Vec2 vd{d->x, d->y};
    for (size_t j = 0; j < poly.rings.size(); ++j) {
        if (static_cast<int>(j) == ring_id) continue;
        const Ring& other = *poly.rings[j];
        if (!other.head || other.size < 2) continue;
        if (ring_contains_point(other, e)) return true;
        const Vertex* u = other.head;
        do {
            const Vertex* w = u->next;
            Vec2 pu{u->x, u->y};
            Vec2 pw{w->x, w->y};
            if (segments_intersect_nontrivial(va, e, pu, pw, false)) return true;
            if (segments_intersect_nontrivial(e, vd, pu, pw, false)) return true;
            u = w;
        } while (u != other.head);
    }
    return false;
}

struct Candidate {
    double displacement{};
    double ex{};
    double ey{};
    int ring_id{};
    Vertex* A{};
    Vertex* B{};
    Vertex* C{};
    Vertex* D{};

    bool operator>(const Candidate& o) const {
        const double eps = 1e-12;
        double diff = displacement - o.displacement;
        if (std::fabs(diff) > eps) return diff > 0.0;
        if (ring_id != o.ring_id) return ring_id > o.ring_id;
        auto id = [](Vertex* v) { return v ? v->original_id : -1; };
        int aA = id(A), aB = id(B), aC = id(C), aD = id(D);
        int bA = id(o.A), bB = id(o.B), bC = id(o.C), bD = id(o.D);
        if (aB != bB) return aB > bB;
        if (aC != bC) return aC > bC;
        if (aA != bA) return aA > bA;
        if (aD != bD) return aD > bD;
        return A > o.A;
    }
};

Candidate make_candidate(int ring_id, Vertex* A, Vertex* B, Vertex* C, Vertex* D) {
    Vec2 vA{A->x, A->y}, vB{B->x, B->y}, vC{C->x, C->y}, vD{D->x, D->y};
    Vec2 e = compute_E(vA, vB, vC, vD);
    double d = areal_displacement(vA, vB, vC, vD, e);
    return Candidate{d, e.x, e.y, ring_id, A, B, C, D};
}

void enqueue_ring(std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>>& pq, Ring& ring) {
    if (ring.size < 4) return;
    Vertex* A = ring.head;
    do {
        Vertex* B = A->next;
        Vertex* C = B->next;
        Vertex* D = C->next;
        pq.push(make_candidate(ring.ring_id, A, B, C, D));
        A = A->next;
    } while (A != ring.head);
}

bool is_valid(const Candidate& c) {
    if (c.A->removed || c.B->removed || c.C->removed || c.D->removed) return false;
    if (c.A->next != c.B) return false;
    if (c.B->next != c.C) return false;
    if (c.C->next != c.D) return false;
    return true;
}

double simplify(Polygon& poly, size_t target_n) {
    using MinHeap = std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>>;
    MinHeap pq;
    double total_displacement = 0.0;

    for (auto& ring : poly.rings) enqueue_ring(pq, *ring);

    while (poly.total_vertices() > target_n && !pq.empty()) {
        Candidate c = pq.top();
        pq.pop();

        if (!is_valid(c)) continue;
        Ring* ring = poly.rings[c.ring_id].get();
        if (ring->size < 4) continue;

        Vec2 e{c.ex, c.ey};
        if (collapse_causes_cross_ring_intersection(poly, c.ring_id, c.A, c.B, c.C, c.D, e)) continue;

        ring->remove(c.B);
        ring->remove(c.C);
        Vertex* e_vtx = ring->insert_after(c.A, c.ex, c.ey);
        total_displacement += c.displacement;

        if (ring->size >= 4) {
            Vertex* start = e_vtx->prev->prev->prev;
            for (int i = 0; i < 4; ++i) {
                Vertex* A = start;
                Vertex* B = A->next;
                Vertex* C = B->next;
                Vertex* D = C->next;
                pq.push(make_candidate(ring->ring_id, A, B, C, D));
                start = start->next;
            }
        }
    }

    for (auto& ring : poly.rings) ring->flush_garbage();
    return total_displacement;
}

double signed_area_ring_input(const std::vector<Point2>& pts) {
    if (pts.size() < 3) return 0.0;
    double s = 0.0;
    for (size_t i = 0; i < pts.size(); ++i) {
        const Point2& p = pts[i];
        const Point2& q = pts[(i + 1) % pts.size()];
        s += p.x * q.y - q.x * p.y;
    }
    return 0.5 * s;
}

long peak_memory_kb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<long>(pmc.PeakWorkingSetSize / 1024);
    }
    return 0;
#else
    struct rusage usage {};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
#endif
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_csv> <target_vertices>\n";
        return 1;
    }

    std::vector<RingInput> rings = read_input_csv(argv[1]);
    int target = std::atoi(argv[2]);

    Polygon poly;
    poly.rings.reserve(rings.size());
    for (size_t i = 0; i < rings.size(); ++i) {
        auto r = std::make_unique<Ring>(static_cast<int>(i));
        for (size_t j = 0; j < rings[i].points.size(); ++j) {
            const Point2& p = rings[i].points[j];
            r->push_back(p.x, p.y, static_cast<int>(j));
        }
        poly.rings.push_back(std::move(r));
    }

    double input_area = 0.0;
    for (const auto& r : rings) input_area += signed_area_ring_input(r.points);

    auto t0 = std::chrono::high_resolution_clock::now();
    double total_displacement = simplify(poly, static_cast<size_t>(target));
    auto t1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = t1 - t0;

    std::cerr << "Simplification: " << poly.total_vertices() << " vertices, "
              << (elapsed.count() * 1000.0) << " ms";
    long mem = peak_memory_kb();
    if (mem > 0) std::cerr << ", peak RSS: " << mem << " KB";
    std::cerr << "\n";

    std::cout << "ring_id,vertex_id,x,y\n";
    for (size_t rid = 0; rid < poly.rings.size(); ++rid) {
        const Ring& ring = *poly.rings[rid];
        if (!ring.head || ring.size == 0) continue;
        const Vertex* v = ring.head;
        int vid = 0;
        do {
            std::cout << rid << "," << vid++ << "," << v->x << "," << v->y << "\n";
            v = v->next;
        } while (v != ring.head);
    }

    double output_area = 0.0;
    for (const auto& ring : poly.rings) output_area += signed_area(*ring);

    std::printf("Total signed area in input: %.6e\n", input_area);
    std::printf("Total signed area in output: %.6e\n", output_area);
    std::printf("Total areal displacement: %.6e\n", total_displacement);
    return 0;
}

