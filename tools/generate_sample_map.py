"""Generates a small synthetic map in the renderer's data format.

No real data and no extra packages needed: hilly terrain, a street grid,
parks and random buildings (rectangles and L-shapes). Useful for working on
the renderer before the OpenStreetMap pipeline (osm_export.py) is ready.

    python tools/generate_sample_map.py            # writes assets/maps/sample
    python tools/generate_sample_map.py --seed 7 --out assets/maps/sample2
"""

from __future__ import annotations

import argparse
import math
import random
from pathlib import Path

import mapformat

MAP_SIZE = 1024.0          # meters along x and z
HEIGHTMAP_RESOLUTION = 257
ALBEDO_RESOLUTION = 1024
BLOCK = 96.0               # street grid spacing in meters
ROAD_WIDTH = 12.0
PARK_PROBABILITY = 0.15


def terrain_height(x: float, z: float) -> float:
    rolling = 12.0 * math.sin(x / 170.0) * math.cos(z / 210.0) + 5.0 * math.sin((x + z) / 90.0)
    hill = 45.0 * math.exp(-((x - 260.0) ** 2 + (z + 240.0) ** 2) / (2 * 170.0 ** 2))
    return 20.0 + rolling + hill


def block_index(v: float) -> int:
    return math.floor((v + MAP_SIZE / 2) / BLOCK)


def block_origin(index: int) -> float:
    return -MAP_SIZE / 2 + index * BLOCK


def point_in_polygon(x: float, z: float, polygon) -> bool:
    inside = False
    j = len(polygon) - 1
    for i in range(len(polygon)):
        xi, zi = polygon[i]
        xj, zj = polygon[j]
        if (zi > z) != (zj > z) and x < (xj - xi) * (z - zi) / (zj - zi) + xi:
            inside = not inside
        j = i
    return inside


def rotate(points, cx, cz, angle):
    c, s = math.cos(angle), math.sin(angle)
    return [(cx + (px - cx) * c - (pz - cz) * s, cz + (px - cx) * s + (pz - cz) * c) for px, pz in points]


def make_lot_building(rng: random.Random, x0: float, z0: float, size: float):
    """A rectangle or L-shape inside the square lot [x0, x0+size] x [z0, z0+size]."""
    margin = 4.0
    w = rng.uniform(0.45, 0.85) * (size - 2 * margin)
    d = rng.uniform(0.45, 0.85) * (size - 2 * margin)
    x = x0 + margin + rng.uniform(0, size - 2 * margin - w)
    z = z0 + margin + rng.uniform(0, size - 2 * margin - d)

    if rng.random() < 0.3:
        # L-shape: a concave footprint to exercise triangulation and wall detection
        cw, cd = w * rng.uniform(0.35, 0.6), d * rng.uniform(0.35, 0.6)
        footprint = [(x, z), (x + w, z), (x + w, z + d - cd), (x + w - cw, z + d - cd), (x + w - cw, z + d), (x, z + d)]
    else:
        footprint = [(x, z), (x + w, z), (x + w, z + d), (x, z + d)]

    angle = math.radians(rng.uniform(-6, 6))
    return rotate(footprint, x + w / 2, z + d / 2, angle)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=Path(__file__).resolve().parent.parent / "assets" / "maps" / "sample")
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    rng = random.Random(args.seed)
    half = MAP_SIZE / 2
    blocks = int(MAP_SIZE // BLOCK) + 1

    # Pick park blocks
    parks = {(bx, bz) for bx in range(blocks) for bz in range(blocks) if rng.random() < PARK_PROBABILITY}

    # Buildings: up to 2x2 lots per non-park block
    buildings = []
    building_polygons = []
    lot = (BLOCK - ROAD_WIDTH) / 2
    for bx in range(blocks):
        for bz in range(blocks):
            if (bx, bz) in parks:
                continue
            ox = block_origin(bx) + ROAD_WIDTH / 2
            oz = block_origin(bz) + ROAD_WIDTH / 2
            if ox + BLOCK - ROAD_WIDTH > half or oz + BLOCK - ROAD_WIDTH > half:
                continue  # partial block at the map edge
            for lx in range(2):
                for lz in range(2):
                    if rng.random() < 0.15:
                        continue
                    footprint = make_lot_building(rng, ox + lx * lot, oz + lz * lot, lot)
                    height = rng.choice([rng.uniform(6, 15), rng.uniform(15, 40), rng.uniform(40, 90) if rng.random() < 0.2 else 12])
                    grey = rng.uniform(0.55, 0.85)
                    tint = rng.uniform(-0.05, 0.05)
                    building = mapformat.make_building(footprint, height, (grey + tint, grey, grey - tint))
                    if building:
                        buildings.append(building)
                        building_polygons.append(footprint)

    # Foliage areas: the park blocks minus the roads
    foliage = []
    for bx, bz in sorted(parks):
        x0 = max(block_origin(bx) + ROAD_WIDTH / 2, -half)
        z0 = max(block_origin(bz) + ROAD_WIDTH / 2, -half)
        x1 = min(block_origin(bx) + BLOCK - ROAD_WIDTH / 2, half)
        z1 = min(block_origin(bz) + BLOCK - ROAD_WIDTH / 2, half)
        if x1 - x0 > 5 and z1 - z0 > 5:
            area = mapformat.make_foliage_area([(x0, z0), (x1, z0), (x1, z1), (x0, z1)], "park")
            if area:
                foliage.append(area)

    # Heightmap
    heights = []
    for j in range(HEIGHTMAP_RESOLUTION):
        z = -half + MAP_SIZE * j / (HEIGHTMAP_RESOLUTION - 1)
        heights.append([terrain_height(-half + MAP_SIZE * i / (HEIGHTMAP_RESOLUTION - 1), z) for i in range(HEIGHTMAP_RESOLUTION)])
    height_min = math.floor(min(map(min, heights)))
    height_max = math.ceil(max(map(max, heights)))

    # Albedo: ground, roads, parks, building footprints
    res = ALBEDO_RESOLUTION
    pixels = bytearray(res * res * 3)
    meters_per_pixel = MAP_SIZE / res
    for py in range(res):
        z = -half + (py + 0.5) * meters_per_pixel
        bz = block_index(z)
        fz = (z + half) % BLOCK
        on_road_z = fz < ROAD_WIDTH / 2 or fz > BLOCK - ROAD_WIDTH / 2
        for px in range(res):
            x = -half + (px + 0.5) * meters_per_pixel
            fx = (x + half) % BLOCK
            noise = rng.randint(-6, 6)
            if on_road_z or fx < ROAD_WIDTH / 2 or fx > BLOCK - ROAD_WIDTH / 2:
                color = (88 + noise // 2, 90 + noise // 2, 96 + noise // 2)
            elif (block_index(x), bz) in parks:
                color = (76 + noise, 128 + noise, 64 + noise)
            else:
                color = (196 + noise, 190 + noise, 178 + noise)
            o = (py * res + px) * 3
            pixels[o:o + 3] = bytes(max(0, min(255, c)) for c in color)

    for polygon in building_polygons:
        xs = [p[0] for p in polygon]
        zs = [p[1] for p in polygon]
        px0, px1 = int((min(xs) + half) / meters_per_pixel), int((max(xs) + half) / meters_per_pixel) + 1
        py0, py1 = int((min(zs) + half) / meters_per_pixel), int((max(zs) + half) / meters_per_pixel) + 1
        for py in range(max(py0, 0), min(py1, res)):
            z = -half + (py + 0.5) * meters_per_pixel
            for px in range(max(px0, 0), min(px1, res)):
                x = -half + (px + 0.5) * meters_per_pixel
                if point_in_polygon(x, z, polygon):
                    o = (py * res + px) * 3
                    pixels[o:o + 3] = bytes((150, 148, 146))

    out: Path = args.out
    mapformat.write_map(out, "sample", (MAP_SIZE, MAP_SIZE), (height_min, height_max), buildings, foliage)
    mapformat.write_heightmap_png(out / "heightmap.png", heights, height_min, height_max)
    mapformat.write_rgb_png(out / "albedo.png", res, res, pixels)
    print(f"wrote {out}: {len(buildings)} buildings, {len(foliage)} foliage areas, heights {height_min}..{height_max} m")


if __name__ == "__main__":
    main()
