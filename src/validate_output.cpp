#include "csv_io.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Args {
    std::string input_path;
    std::string output_path;
    int target{0};
    double tol{0.0};
};

struct OutputMetrics {
    bool has_input_total{false};
    bool has_output_total{false};
    bool has_disp{false};
    double input_total{0.0};
    double output_total{0.0};
    double disp{0.0};
};

struct ValidateResult {
    bool ok{false};
    bool vertex_warn{false};
    std::string summary;
};

using RingMap = std::map<int, std::vector<Point2>>;

std::string trim(std::string s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

bool parse_int(const std::string& s, int& out) {
    std::size_t pos = 0;
    try {
        out = std::stoi(s, &pos);
    } catch (...) {
        return false;
    }
    return pos == s.size();
}

bool parse_double(const std::string& s, double& out) {
    std::size_t pos = 0;
    try {
        out = std::stod(s, &pos);
    } catch (...) {
        return false;
    }
    return pos == s.size();
}

bool split_csv4(const std::string& line, std::array<std::string, 4>& cols) {
    std::stringstream ss(line);
    std::string tok;
    int i = 0;
    while (std::getline(ss, tok, ',')) {
        if (i >= 4) {
            return false;
        }
        cols[static_cast<std::size_t>(i)] = trim(tok);
        ++i;
    }
    return i == 4;
}

bool parse_metric_line(const std::string& line, const std::string& prefix, double& out) {
    if (line.rfind(prefix, 0) != 0) {
        return false;
    }
    return parse_double(trim(line.substr(prefix.size())), out);
}

bool parse_args(int argc, char** argv, Args& args, std::string& err) {
    for (int i = 1; i < argc; ++i) {
        const std::string cur = argv[i];
        if (cur == "--input") {
            if (i + 1 >= argc) {
                err = "missing value for --input";
                return false;
            }
            args.input_path = argv[++i];
        } else if (cur == "--output") {
            if (i + 1 >= argc) {
                err = "missing value for --output";
                return false;
            }
            args.output_path = argv[++i];
        } else if (cur == "--target") {
            if (i + 1 >= argc) {
                err = "missing value for --target";
                return false;
            }
            if (!parse_int(argv[++i], args.target)) {
                err = "invalid --target value";
                return false;
            }
        } else if (cur == "--tol") {
            if (i + 1 >= argc) {
                err = "missing value for --tol";
                return false;
            }
            if (!parse_double(argv[++i], args.tol)) {
                err = "invalid --tol value";
                return false;
            }
        } else {
            err = "unknown argument: " + cur;
            return false;
        }
    }

    if (args.input_path.empty() || args.output_path.empty()) {
        err = "usage: validate_output --input <path> --output <path> --target <n> --tol <value>";
        return false;
    }
    return true;
}

RingMap input_rings_to_map(const std::vector<RingInput>& rings) {
    RingMap out;
    for (const auto& ring : rings) {
        out[ring.ring_id] = ring.points;
    }
    return out;
}

bool read_output_file(const std::string& path, RingMap& rings, OutputMetrics& metrics, std::string& err) {
    std::ifstream in(path);
    if (!in) {
        err = "failed to open output file";
        return false;
    }

    std::map<int, std::vector<std::pair<int, Point2>>> by_ring;
    std::string line;
    while (std::getline(in, line)) {
        const std::string s = trim(line);
        if (s.empty()) {
            continue;
        }

        std::array<std::string, 4> cols{};
        if (split_csv4(s, cols)) {
            int rid = 0;
            int vid = 0;
            double x = 0.0;
            double y = 0.0;
            if (parse_int(cols[0], rid) && parse_int(cols[1], vid) &&
                parse_double(cols[2], x) && parse_double(cols[3], y)) {
                by_ring[rid].push_back({vid, Point2{x, y}});
                continue;
            }
        }

        double val = 0.0;
        if (parse_metric_line(s, "Total signed area in input:", val)) {
            metrics.has_input_total = true;
            metrics.input_total = val;
            continue;
        }
        if (parse_metric_line(s, "Total signed area in output:", val)) {
            metrics.has_output_total = true;
            metrics.output_total = val;
            continue;
        }
        if (parse_metric_line(s, "Total areal displacement:", val)) {
            metrics.has_disp = true;
            metrics.disp = val;
            continue;
        }
    }

    for (auto& kv : by_ring) {
        auto& rows = kv.second;
        std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        std::vector<Point2> pts;
        pts.reserve(rows.size());
        for (const auto& row : rows) {
            pts.push_back(row.second);
        }
        rings[kv.first] = std::move(pts);
    }

    return true;
}

double signed_area(const std::vector<Point2>& pts) {
    if (pts.size() < 3) {
        return 0.0;
    }
    long double acc = 0.0L;
    const std::size_t n = pts.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Point2& a = pts[i];
        const Point2& b = pts[(i + 1U) % n];
        acc += static_cast<long double>(a.x) * static_cast<long double>(b.y) -
               static_cast<long double>(b.x) * static_cast<long double>(a.y);
    }
    return static_cast<double>(acc * 0.5L);
}

bool close_enough(double a, double b, double abs_tol, double& used_tol) {
    const double rel_tol = 1e-2;
    used_tol = std::max(abs_tol, rel_tol * std::max({1.0, std::abs(a), std::abs(b)}));
    return std::abs(a - b) <= used_tol;
}

bool orientation_ok(int rid, double sa, double tol) {
    if (rid == 0) {
        return sa > tol;
    }
    return sa < -tol;
}

double orient(const Point2& a, const Point2& b, const Point2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool on_segment(const Point2& a, const Point2& b, const Point2& p, double eps) {
    return (
        std::min(a.x, b.x) - eps <= p.x && p.x <= std::max(a.x, b.x) + eps &&
        std::min(a.y, b.y) - eps <= p.y && p.y <= std::max(a.y, b.y) + eps
    );
}

bool segments_intersect(const Point2& a, const Point2& b, const Point2& c, const Point2& d, double eps) {
    const double o1 = orient(a, b, c);
    const double o2 = orient(a, b, d);
    const double o3 = orient(c, d, a);
    const double o4 = orient(c, d, b);

    if (((o1 > eps && o2 < -eps) || (o1 < -eps && o2 > eps)) &&
        ((o3 > eps && o4 < -eps) || (o3 < -eps && o4 > eps))) {
        return true;
    }

    if (std::abs(o1) <= eps && on_segment(a, b, c, eps)) {
        return true;
    }
    if (std::abs(o2) <= eps && on_segment(a, b, d, eps)) {
        return true;
    }
    if (std::abs(o3) <= eps && on_segment(c, d, a, eps)) {
        return true;
    }
    if (std::abs(o4) <= eps && on_segment(c, d, b, eps)) {
        return true;
    }
    return false;
}

bool has_self_intersection(const std::vector<Point2>& poly, double eps) {
    const int n = static_cast<int>(poly.size());
    if (n < 4) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        const Point2& a = poly[static_cast<std::size_t>(i)];
        const Point2& b = poly[static_cast<std::size_t>((i + 1) % n)];
        for (int j = i + 1; j < n; ++j) {
            if (j == i) {
                continue;
            }
            if (j == (i + 1) % n) {
                continue;
            }
            if (i == (j + 1) % n) {
                continue;
            }
            const Point2& c = poly[static_cast<std::size_t>(j)];
            const Point2& d = poly[static_cast<std::size_t>((j + 1) % n)];
            if (segments_intersect(a, b, c, d, eps)) {
                return true;
            }
        }
    }
    return false;
}

bool rings_intersect(const std::vector<Point2>& r1, const std::vector<Point2>& r2, double eps) {
    const int n1 = static_cast<int>(r1.size());
    const int n2 = static_cast<int>(r2.size());
    if (n1 < 2 || n2 < 2) {
        return false;
    }
    for (int i = 0; i < n1; ++i) {
        const Point2& a = r1[static_cast<std::size_t>(i)];
        const Point2& b = r1[static_cast<std::size_t>((i + 1) % n1)];
        for (int j = 0; j < n2; ++j) {
            const Point2& c = r2[static_cast<std::size_t>(j)];
            const Point2& d = r2[static_cast<std::size_t>((j + 1) % n2)];
            if (segments_intersect(a, b, c, d, eps)) {
                return true;
            }
        }
    }
    return false;
}

bool point_in_ring(const Point2& pt, const std::vector<Point2>& ring, double eps) {
    const double x = pt.x;
    const double y = pt.y;
    bool inside = false;
    const int n = static_cast<int>(ring.size());
    if (n < 3) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        const Point2& a = ring[static_cast<std::size_t>(i)];
        const Point2& b = ring[static_cast<std::size_t>((i + 1) % n)];
        if (std::abs(orient(a, b, pt)) <= eps && on_segment(a, b, pt, eps)) {
            return true;
        }
        const bool straddles = (a.y > y) != (b.y > y);
        if (straddles) {
            const double x_int = (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x;
            if (x < x_int) {
                inside = !inside;
            }
        }
    }
    return inside;
}

std::vector<int> ring_ids(const RingMap& rings) {
    std::vector<int> ids;
    ids.reserve(rings.size());
    for (const auto& kv : rings) {
        ids.push_back(kv.first);
    }
    return ids;
}

std::string format_sci(double v) {
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(6) << v;
    return oss.str();
}

std::string format_int_list(const std::vector<int>& ids) {
    std::ostringstream oss;
    oss << "[";
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << ids[i];
    }
    oss << "]";
    return oss.str();
}

ValidateResult validate(
    const RingMap& in_rings,
    const RingMap& out_rings,
    const OutputMetrics& metrics,
    int target,
    double eps
) {
    ValidateResult result;

    if (!(metrics.has_input_total && metrics.has_output_total && metrics.has_disp)) {
        result.summary = "missing required metrics";
        return result;
    }

    const std::vector<int> in_ids = ring_ids(in_rings);
    const std::vector<int> out_ids = ring_ids(out_rings);
    if (in_ids != out_ids) {
        result.summary = "ring ids changed: in=" + format_int_list(in_ids) + ", out=" + format_int_list(out_ids);
        return result;
    }

    for (int rid : in_ids) {
        const auto& in_pts = in_rings.at(rid);
        const auto& out_pts = out_rings.at(rid);
        const double in_area = signed_area(in_pts);
        const double out_area = signed_area(out_pts);
        double used_tol = 0.0;
        if (!close_enough(in_area, out_area, eps, used_tol)) {
            result.summary =
                "ring " + std::to_string(rid) +
                " area mismatch (|delta|=" + format_sci(std::abs(in_area - out_area)) +
                " > tol=" + format_sci(used_tol) + ")";
            return result;
        }
        if (!orientation_ok(rid, out_area, eps)) {
            result.summary = "ring " + std::to_string(rid) + " orientation invalid";
            return result;
        }
    }

    for (int rid : in_ids) {
        if (has_self_intersection(out_rings.at(rid), eps)) {
            result.summary = "ring " + std::to_string(rid) + " has self-intersection";
            return result;
        }
    }

    for (std::size_t i = 0; i < in_ids.size(); ++i) {
        for (std::size_t j = i + 1; j < in_ids.size(); ++j) {
            const int rid_i = in_ids[i];
            const int rid_j = in_ids[j];
            if (rings_intersect(out_rings.at(rid_i), out_rings.at(rid_j), eps)) {
                result.summary = "rings " + std::to_string(rid_i) + " and " + std::to_string(rid_j) + " intersect";
                return result;
            }
        }
    }

    const auto exterior_it = out_rings.find(0);
    if (exterior_it == out_rings.end() || exterior_it->second.empty()) {
        result.summary = "missing exterior ring";
        return result;
    }
    const std::vector<Point2>& exterior = exterior_it->second;
    for (int rid : in_ids) {
        if (rid == 0) {
            continue;
        }
        const auto& ring = out_rings.at(rid);
        if (ring.empty()) {
            result.summary = "ring " + std::to_string(rid) + " is empty";
            return result;
        }
        if (!point_in_ring(ring.front(), exterior, eps)) {
            result.summary = "ring " + std::to_string(rid) + " is not inside exterior ring";
            return result;
        }
    }

    double out_total_computed = 0.0;
    for (int rid : in_ids) {
        out_total_computed += signed_area(out_rings.at(rid));
    }
    double used_tol = 0.0;
    if (!close_enough(out_total_computed, metrics.output_total, eps, used_tol)) {
        result.summary = "reported output total area inconsistent with coordinates";
        return result;
    }

    double in_total_computed = 0.0;
    for (int rid : in_ids) {
        in_total_computed += signed_area(in_rings.at(rid));
    }
    if (!close_enough(in_total_computed, metrics.input_total, eps, used_tol)) {
        result.summary = "reported input total area inconsistent with input coordinates";
        return result;
    }

    if (!close_enough(metrics.input_total, metrics.output_total, eps, used_tol)) {
        result.summary = "total signed area not preserved";
        return result;
    }

    if (metrics.disp < -eps) {
        result.summary = "reported displacement is negative";
        return result;
    }

    std::size_t vertex_count = 0;
    for (int rid : in_ids) {
        vertex_count += out_rings.at(rid).size();
    }
    result.vertex_warn = static_cast<int>(vertex_count) > target;
    result.ok = true;
    if (result.vertex_warn) {
        result.summary = "areas/orientation/topology/metrics consistent, vertices=" +
            std::to_string(vertex_count) + " > target=" + std::to_string(target);
    } else {
        result.summary = "areas/orientation/topology/metrics consistent, vertices=" +
            std::to_string(vertex_count) + " <= target=" + std::to_string(target);
    }
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    Args args;
    std::string err;
    if (!parse_args(argc, argv, args, err)) {
        std::cerr << err << "\n";
        return 1;
    }

    RingMap in_rings;
    try {
        in_rings = input_rings_to_map(read_input_csv(args.input_path));
    } catch (const std::exception& ex) {
        std::cerr << "failed to parse input: " << ex.what() << "\n";
        return 1;
    }

    RingMap out_rings;
    OutputMetrics metrics;
    if (!read_output_file(args.output_path, out_rings, metrics, err)) {
        std::cerr << err << "\n";
        return 1;
    }

    const ValidateResult result = validate(in_rings, out_rings, metrics, args.target, args.tol);
    std::cout << result.summary << "\n";
    if (!result.ok) {
        return 1;
    }
    return result.vertex_warn ? 2 : 0;
}
