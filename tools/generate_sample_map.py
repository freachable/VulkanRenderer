"""Generates a small synthetic map in the renderer's data format.

No real data and no extra packages needed: hilly terrain, a street grid,
parks and random buildings (rectangles and L-shapes). Useful for working on
the renderer before the OpenStreetMap pipeline (osm_export.py) is ready.

    python tools/generate_sample_map.py            # writes assets/maps/sample
    python tools/generate_sample_map.py --seed 7 --out assets/maps/sample2

Coordinates as in mapformat.py: x east, y north, z up; 2D points are (x, y).
"""

from __future__ import annotations

import argparse
import math
import random
from pathlib import Path

import mapformat

MAP_SIZE = 1024.0          # meters along x and y
HEIGHTMAP_RESOLUTION = 257
ALBEDO_RESOLUTION = 1024
BLOCK = 96.0               # street grid spacing in meters
ROAD_WIDTH = 12.0
PARK_PROBABILITY = 0.15


def terrain_height(x: float, y: float) -> float:
    rolling = 12.0 * math.sin(x / 170.0) * math.cos(y / 210.0) + 5.0 * math.sin((x - y) / 90.0)
    hill = 45.0 * math.exp(-((x - 260.0) ** 2 + (y - 240.0) ** 2) / (2 * 170.0 ** 2))  # north-east
    return 20.0 + rolling + hill


def block_index(v: float) -> int:
    return math.floor((v + MAP_SIZE / 2) / BLOCK)


def block_origin(index: int) -> float:
    return -MAP_SIZE / 2 + index * BLOCK


def point_in_polygon(x: float, y: float, polygon) -> bool:
    inside = False
    j = len(polygon) - 1
    for i in range(len(polygon)):
        xi, yi = polygon[i]
        xj, yj = polygon[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            inside = not inside
        j = i
    return inside


def rotate(points, cx, cy, angle):
    c, s = math.cos(angle), math.sin(angle)
    return [(cx + (px - cx) * c - (py - cy) * s, cy + (px - cx) * s + (py - cy) * c) for px, py in points]


def make_lot_building(rng: random.Random, x0: float, y0: float, size: float):
    """A rectangle or L-shape inside the square lot [x0, x0+size] x [y0, y0+size]."""
    margin = 4.0
    w = rng.uniform(0.45, 0.85) * (size - 2 * margin)
    d = rng.uniform(0.45, 0.85) * (size - 2 * margin)
    x = x0 + margin + rng.uniform(0, size - 2 * margin - w)
    y = y0 + margin + rng.uniform(0, size - 2 * margin - d)

    if rng.random() < 0.3:
        # L-shape: a concave footprint to exercise triangulation and wall detection
        cw, cd = w * rng.uniform(0.35, 0.6), d * rng.uniform(0.35, 0.6)
        footprint = [(x, y), (x + w, y), (x + w, y + d - cd), (x + w - cw, y + d - cd), (x + w - cw, y + d), (x, y + d)]
    else:
        footprint = [(x, y), (x + w, y), (x + w, y + d), (x, y + d)]

    angle = math.radians(rng.uniform(-6, 6))
    return rotate(footprint, x + w / 2, y + d / 2, angle)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=Path(__file__).resolve().parent.parent / "assets" / "maps" / "sample")
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    rng = random.Random(args.seed)
    half = MAP_SIZE / 2
    blocks = int(MAP_SIZE // BLOCK) + 1

    # Pick park blocks
    parks = {(bx, by) for bx in range(blocks) for by in range(blocks) if rng.random() < PARK_PROBABILITY}

    # Buildings: up to 2x2 lots per non-park block
    buildings = []
    building_polygons = []
    lot = (BLOCK - ROAD_WIDTH) / 2
    for bx in range(blocks):
        for by in range(blocks):
            if (bx, by) in parks:
                continue
            ox = block_origin(bx) + ROAD_WIDTH / 2
            oy = block_origin(by) + ROAD_WIDTH / 2
            if ox + BLOCK - ROAD_WIDTH > half or oy + BLOCK - ROAD_WIDTH > half:
                continue  # partial block at the map edge
            for lx in range(2):
                for ly in range(2):
                    if rng.random() < 0.15:
                        continue
                    footprint = make_lot_building(rng, ox + lx * lot, oy + ly * lot, lot)
                    height = rng.choice([rng.uniform(6, 15), rng.uniform(15, 40), rng.uniform(40, 90) if rng.random() < 0.2 else 12])
                    grey = rng.uniform(0.55, 0.85)
                    tint = rng.uniform(-0.05, 0.05)
                    building = mapformat.make_building(footprint, height, (grey + tint, grey, grey - tint))
                    if building:
                        buildings.append(building)
                        building_polygons.append(footprint)

    # Foliage areas: the park blocks minus the roads
    foliage = []
    for bx, by in sorted(parks):
        x0 = max(block_origin(bx) + ROAD_WIDTH / 2, -half)
        y0 = max(block_origin(by) + ROAD_WIDTH / 2, -half)
        x1 = min(block_origin(bx) + BLOCK - ROAD_WIDTH / 2, half)
        y1 = min(block_origin(by) + BLOCK - ROAD_WIDTH / 2, half)
        if x1 - x0 > 5 and y1 - y0 > 5:
            area = mapformat.make_foliage_area([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], "park")
            if area:
                foliage.append(area)

    # Heightmap: row 0 is the northern edge (y = +half)
    heights = []
    for j in range(HEIGHTMAP_RESOLUTION):
        y = half - MAP_SIZE * j / (HEIGHTMAP_RESOLUTION - 1)
        heights.append([terrain_height(-half + MAP_SIZE * i / (HEIGHTMAP_RESOLUTION - 1), y) for i in range(HEIGHTMAP_RESOLUTION)])
    height_min = math.floor(min(map(min, heights)))
    height_max = math.ceil(max(map(max, heights)))

    # Albedo: ground, roads, parks, building footprints. Pixel row 0 is the northern edge.
    res = ALBEDO_RESOLUTION
    pixels = bytearray(res * res * 3)
    meters_per_pixel = MAP_SIZE / res
    for row in range(res):
        y = half - (row + 0.5) * meters_per_pixel
        by = block_index(y)
        fy = (y + half) % BLOCK
        on_road_y = fy < ROAD_WIDTH / 2 or fy > BLOCK - ROAD_WIDTH / 2
        for col in range(res):
            x = -half + (col + 0.5) * meters_per_pixel
            fx = (x + half) % BLOCK
            noise = rng.randint(-6, 6)
            if on_road_y or fx < ROAD_WIDTH / 2 or fx > BLOCK - ROAD_WIDTH / 2:
                color = (88 + noise // 2, 90 + noise // 2, 96 + noise // 2)
            elif (block_index(x), by) in parks:
                color = (76 + noise, 128 + noise, 64 + noise)
            else:
                color = (196 + noise, 190 + noise, 178 + noise)
            o = (row * res + col) * 3
            pixels[o:o + 3] = bytes(max(0, min(255, c)) for c in color)

    for polygon in building_polygons:
        xs = [p[0] for p in polygon]
        ys = [p[1] for p in polygon]
        col0, col1 = int((min(xs) + half) / meters_per_pixel), int((max(xs) + half) / meters_per_pixel) + 1
        row0, row1 = int((half - max(ys)) / meters_per_pixel), int((half - min(ys)) / meters_per_pixel) + 1
        for row in range(max(row0, 0), min(row1, res)):
            y = half - (row + 0.5) * meters_per_pixel
            for col in range(max(col0, 0), min(col1, res)):
                x = -half + (col + 0.5) * meters_per_pixel
                if point_in_polygon(x, y, polygon):
                    o = (row * res + col) * 3
                    pixels[o:o + 3] = bytes((150, 148, 146))

    out: Path = args.out
    mapformat.write_map(out, "sample", (MAP_SIZE, MAP_SIZE), (height_min, height_max), buildings, foliage)
    mapformat.write_heightmap_png(out / "heightmap.png", heights, height_min, height_max)
    mapformat.write_rgb_png(out / "albedo.png", res, res, pixels)
    print(f"wrote {out}: {len(buildings)} buildings, {len(foliage)} foliage areas, heights {height_min}..{height_max} m")


if __name__ == "__main__":
    main()
