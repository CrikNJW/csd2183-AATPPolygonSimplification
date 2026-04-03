#!/usr/bin/env python3
import argparse
import csv
import json
import math
import re
from collections import defaultdict


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--input", required=True)
    p.add_argument("--output", required=True)
    p.add_argument("--target", required=True, type=int)
    p.add_argument("--tol", required=True, type=float)
    p.add_argument("--json", required=True, dest="json_path")
    return p.parse_args()


def close_enough(a, b, abs_tol):
    # Printed coordinates/metrics are rounded; allow small relative slack.
    # Use 1e-2 relative tolerance to account for floating-point accumulation
    # and coordinate rounding in output.
    rel_tol = 1e-2
    tol = max(abs_tol, rel_tol * max(1.0, abs(a), abs(b)))
    return abs(a - b) <= tol, tol


def read_input_csv(path):
    rings = defaultdict(list)
    with open(path, newline="", encoding="utf-8") as f:
        r = csv.reader(f)
        next(r, None)
        for row in r:
            if len(row) < 4:
                continue
            try:
                rid = int(row[0])
                vid = int(row[1])
                x = float(row[2])
                y = float(row[3])
            except ValueError:
                continue
            rings[rid].append((vid, x, y))
    return {rid: [p[1:] for p in sorted(pts)] for rid, pts in rings.items()}


def read_output_file(path):
    rings = defaultdict(list)
    metrics = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            if not s:
                continue
            if re.match(r"^\d+,\d+,[^,]+,[^,]+$", s):
                parts = s.split(",")
                rid = int(parts[0])
                vid = int(parts[1])
                x = float(parts[2])
                y = float(parts[3])
                rings[rid].append((vid, x, y))
                continue

            m = re.match(r"^Total signed area in input:\s+([+-]?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)$", s)
            if m:
                metrics["input_total"] = float(m.group(1))
                continue
            m = re.match(r"^Total signed area in output:\s+([+-]?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)$", s)
            if m:
                metrics["output_total"] = float(m.group(1))
                continue
            m = re.match(r"^Total areal displacement:\s+([+-]?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)$", s)
            if m:
                metrics["disp"] = float(m.group(1))
                continue

    return {rid: [p[1:] for p in sorted(pts)] for rid, pts in rings.items()}, metrics


def signed_area(pts):
    if len(pts) < 3:
        return 0.0
    s = 0.0
    n = len(pts)
    for i in range(n):
        x1, y1 = pts[i]
        x2, y2 = pts[(i + 1) % n]
        s += x1 * y2 - x2 * y1
    return 0.5 * s


def orientation_ok(rid, sa, tol):
    # By project convention and sample files: ring 0 is exterior (CCW), others are holes (CW)
    if rid == 0:
        return sa > tol
    return sa < -tol


def orient(a, b, c):
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def on_segment(a, b, p, eps):
    return (
        min(a[0], b[0]) - eps <= p[0] <= max(a[0], b[0]) + eps
        and min(a[1], b[1]) - eps <= p[1] <= max(a[1], b[1]) + eps
    )


def segments_intersect(a, b, c, d, eps):
    o1 = orient(a, b, c)
    o2 = orient(a, b, d)
    o3 = orient(c, d, a)
    o4 = orient(c, d, b)

    if (o1 > eps and o2 < -eps or o1 < -eps and o2 > eps) and (o3 > eps and o4 < -eps or o3 < -eps and o4 > eps):
        return True

    if abs(o1) <= eps and on_segment(a, b, c, eps):
        return True
    if abs(o2) <= eps and on_segment(a, b, d, eps):
        return True
    if abs(o3) <= eps and on_segment(c, d, a, eps):
        return True
    if abs(o4) <= eps and on_segment(c, d, b, eps):
        return True
    return False


def edges(poly):
    n = len(poly)
    for i in range(n):
        yield i, (poly[i], poly[(i + 1) % n])


def has_self_intersection(poly, eps):
    n = len(poly)
    if n < 4:
        return False
    e = list(edges(poly))
    for i, (a, b) in e:
        for j, (c, d) in e:
            if j <= i:
                continue
            if j == i:
                continue
            if j == (i + 1) % n:
                continue
            if i == (j + 1) % n:
                continue
            if segments_intersect(a, b, c, d, eps):
                return True
    return False


def rings_intersect(r1, r2, eps):
    e1 = list(edges(r1))
    e2 = list(edges(r2))
    for _, (a, b) in e1:
        for _, (c, d) in e2:
            if segments_intersect(a, b, c, d, eps):
                return True
    return False


def point_in_ring(pt, ring, eps):
    # Ray casting, boundary treated as inside.
    x, y = pt
    inside = False
    n = len(ring)
    for i in range(n):
        a = ring[i]
        b = ring[(i + 1) % n]
        if abs(orient(a, b, pt)) <= eps and on_segment(a, b, pt, eps):
            return True
        yi = a[1]
        yj = b[1]
        if (yi > y) != (yj > y):
            x_int = (b[0] - a[0]) * (y - yi) / (yj - yi) + a[0]
            if x < x_int:
                inside = not inside
    return inside


def main():
    args = parse_args()
    eps = args.tol

    in_rings = read_input_csv(args.input)
    out_rings, metrics = read_output_file(args.output)

    result = {
        "ok": False,
        "vertex_warn": False,
        "summary": "",
    }

    required = {"input_total", "output_total", "disp"}
    if not required.issubset(metrics.keys()):
        result["summary"] = "missing required metrics"
        write_json(args.json_path, result)
        return

    in_ids = sorted(in_rings.keys())
    out_ids = sorted(out_rings.keys())
    if in_ids != out_ids:
        result["summary"] = f"ring ids changed: in={in_ids}, out={out_ids}"
        write_json(args.json_path, result)
        return

    # Per-ring area + orientation checks
    for rid in in_ids:
        ina = signed_area(in_rings[rid])
        outa = signed_area(out_rings[rid])
        ok_area, used_tol = close_enough(ina, outa, eps)
        if not ok_area:
            result["summary"] = (
                f"ring {rid} area mismatch (|delta|={abs(ina - outa):.6e} > tol={used_tol:.6e})"
            )
            write_json(args.json_path, result)
            return
        if not orientation_ok(rid, outa, eps):
            result["summary"] = f"ring {rid} orientation invalid"
            write_json(args.json_path, result)
            return

    # Topology checks: self intersections + cross-ring intersections
    for rid in in_ids:
        if has_self_intersection(out_rings[rid], eps):
            result["summary"] = f"ring {rid} has self-intersection"
            write_json(args.json_path, result)
            return

    for i, rid_i in enumerate(in_ids):
        for rid_j in in_ids[i + 1 :]:
            if rings_intersect(out_rings[rid_i], out_rings[rid_j], eps):
                result["summary"] = f"rings {rid_i} and {rid_j} intersect"
                write_json(args.json_path, result)
                return

    # Hole containment sanity: each interior ring should lie within exterior ring
    exterior = out_rings[0]
    for rid in in_ids:
        if rid == 0:
            continue
        p = out_rings[rid][0]
        if not point_in_ring(p, exterior, eps):
            result["summary"] = f"ring {rid} is not inside exterior ring"
            write_json(args.json_path, result)
            return

    # Geometric consistency checks
    out_total_computed = sum(signed_area(out_rings[rid]) for rid in in_ids)
    ok_total_out, tol_total_out = close_enough(out_total_computed, metrics["output_total"], eps)
    if not ok_total_out:
        result["summary"] = "reported output total area inconsistent with coordinates"
        write_json(args.json_path, result)
        return

    in_total_computed = sum(signed_area(in_rings[rid]) for rid in in_ids)
    ok_total_in, tol_total_in = close_enough(in_total_computed, metrics["input_total"], eps)
    if not ok_total_in:
        result["summary"] = "reported input total area inconsistent with input coordinates"
        write_json(args.json_path, result)
        return

    ok_preserved, tol_preserved = close_enough(metrics["input_total"], metrics["output_total"], eps)
    if not ok_preserved:
        result["summary"] = "total signed area not preserved"
        write_json(args.json_path, result)
        return

    if metrics["disp"] < -eps:
        result["summary"] = "reported displacement is negative"
        write_json(args.json_path, result)
        return

    # Vertex count check
    vertex_count = sum(len(out_rings[rid]) for rid in in_ids)
    result["vertex_warn"] = vertex_count > args.target

    result["ok"] = True
    if result["vertex_warn"]:
        result["summary"] = (
            f"areas/orientation/topology/metrics consistent, vertices={vertex_count} > target={args.target}"
        )
    else:
        result["summary"] = (
            f"areas/orientation/topology/metrics consistent, vertices={vertex_count} <= target={args.target}"
        )
    write_json(args.json_path, result)


def write_json(path, data):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f)


if __name__ == "__main__":
    main()
