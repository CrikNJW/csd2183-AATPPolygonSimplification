#include "csv_io.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace {
std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    return s.substr(i);
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::stringstream ss(line);
    std::string tok;
    std::vector<std::string> cols;
    while (std::getline(ss, tok, ',')) {
        cols.push_back(trim(tok));
    }
    return cols;
}
}  // namespace

std::vector<RingInput> read_input_csv(const std::string& input_file) {
    std::ifstream in(input_file);
    if (!in) {
        throw std::runtime_error("Failed to open input file: " + input_file);
    }

    std::string line;
    if (!std::getline(in, line)) {
        throw std::runtime_error("Input file is empty.");
    }
    if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) {
        line = line.substr(3);
    }
    line = trim(line);
    const std::vector<std::string> header = split_csv_line(line);
    if (header.size() != 4 || header[0] != "ring_id" || header[1] != "vertex_id" ||
        header[2] != "x" || header[3] != "y") {
        throw std::runtime_error("Unexpected CSV header. Expected: ring_id,vertex_id,x,y");
    }

    struct Row {
        int vertex_id{};
        Point2 p{};
    };

    std::unordered_map<int, std::vector<Row>> by_ring;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string> cols = split_csv_line(line);
        if (cols.size() != 4) {
            throw std::runtime_error("Invalid CSV row: " + line);
        }

        Row r;
        const int ring_id = std::stoi(cols[0]);
        r.vertex_id = std::stoi(cols[1]);
        r.p.x = std::stod(cols[2]);
        r.p.y = std::stod(cols[3]);
        by_ring[ring_id].push_back(r);
    }

    std::vector<int> ring_ids;
    ring_ids.reserve(by_ring.size());
    for (const auto& kv : by_ring) {
        ring_ids.push_back(kv.first);
    }
    std::sort(ring_ids.begin(), ring_ids.end());

    std::vector<RingInput> result;
    result.reserve(ring_ids.size());
    for (int ring_id : ring_ids) {
        auto rows = by_ring[ring_id];
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
            return a.vertex_id < b.vertex_id;
        });
        RingInput ring;
        ring.ring_id = ring_id;
        ring.points.reserve(rows.size());
        for (const Row& r : rows) {
            ring.points.push_back(r.p);
        }
        result.push_back(std::move(ring));
    }
    return result;
}
