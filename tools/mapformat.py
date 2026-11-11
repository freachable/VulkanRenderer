"""Writer for the preprocessed map data format read by the renderer.

The format is described in docs/map-data-format.md. Only the standard library
is used here, so the synthetic sample generator runs without extra packages.

World coordinates: x points east, y up, z south. The map is centered on the
origin and spans [-size_x/2, size_x/2] x [-size_z/2, size_z/2] meters.
Image row 0 is the northern edge (z = -size_z/2), column 0 the western edge.
"""

from __future__ import annotations

import json
import struct
import zlib
from pathlib import Path
from typing import Iterable, Sequence

FORMAT_VERSION = 1

Point = tuple[float, float]


# ---------------------------------------------------------------------------
# PNG output (no Pillow needed)
# ---------------------------------------------------------------------------

def _png_chunk(tag: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def _write_png(path: Path, width: int, height: int, bit_depth: int, color_type: int, rows: Iterable[bytes]) -> None:
    raw = b"".join(b"\x00" + row for row in rows)  # filter type 0 per row
    header = struct.pack(">IIBBBBB", width, height, bit_depth, color_type, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(_png_chunk(b"IHDR", header))
        f.write(_png_chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(_png_chunk(b"IEND", b""))


def write_heightmap_png(path: Path, heights: Sequence[Sequence[float]], height_min: float, height_max: float) -> None:
    """Writes heights (meters, rows north to south) as a 16-bit grayscale PNG."""
    span = max(height_max - height_min, 1e-6)
    width = len(heights[0])
    rows = []
    for row in heights:
        values = [max(0, min(65535, round((h - height_min) / span * 65535))) for h in row]
        rows.append(struct.pack(f">{width}H", *values))
    _write_png(path, width, len(heights), 16, 0, rows)


def write_rgb_png(path: Path, width: int, height: int, pixels: bytearray) -> None:
    """pixels: width*height*3 bytes, row-major, row 0 = north."""
    stride = width * 3
    rows = (bytes(pixels[y * stride:(y + 1) * stride]) for y in range(height))
    _write_png(path, width, height, 8, 2, rows)


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------

def signed_area(polygon: Sequence[Point]) -> float:
    """Shoelace area in the (x, z) plane."""
    area = 0.0
    for i, (x0, z0) in enumerate(polygon):
        x1, z1 = polygon[(i + 1) % len(polygon)]
        area += x0 * z1 - x1 * z0
    return area * 0.5


def clean_polygon(polygon: Sequence[Point], epsilon: float = 1e-4) -> list[Point]:
    """Drops a repeated closing point and consecutive duplicates."""
    result: list[Point] = []
    for p in polygon:
        if not result or abs(p[0] - result[-1][0]) > epsilon or abs(p[1] - result[-1][1]) > epsilon:
            result.append((float(p[0]), float(p[1])))
    if len(result) > 1 and abs(result[0][0] - result[-1][0]) <= epsilon and abs(result[0][1] - result[-1][1]) <= epsilon:
        result.pop()
    return result


def triangulate(polygon: Sequence[Point]) -> list[int]:
    """Ear clipping for a simple polygon (convex or concave, no holes).

    Returns a flat index list. The renderer fixes the winding itself, so the
    orientation of the input does not matter.
    """
    n = len(polygon)
    if n < 3:
        return []

    # Work in counter-clockwise order of the shoelace formula.
    order = list(range(n)) if signed_area(polygon) > 0 else list(range(n - 1, -1, -1))

    def cross(o: Point, a: Point, b: Point) -> float:
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    def inside(p: Point, a: Point, b: Point, c: Point) -> bool:
        return cross(a, b, p) >= 0 and cross(b, c, p) >= 0 and cross(c, a, p) >= 0

    triangles: list[int] = []
    guard = 0
    while len(order) > 3 and guard < n * n:
        guard += 1
        for k in range(len(order)):
            i0, i1, i2 = order[k - 1], order[k], order[(k + 1) % len(order)]
            a, b, c = polygon[i0], polygon[i1], polygon[i2]
            if cross(a, b, c) <= 0:
                continue  # reflex or degenerate corner
            if any(inside(polygon[j], a, b, c) for j in order if j not in (i0, i1, i2)):
                continue
            triangles += [i0, i1, i2]
            del order[k]
            break
        else:
            break  # self-intersecting input; give up on the rest
    if len(order) == 3:
        triangles += order
    return triangles


# ---------------------------------------------------------------------------
# Map output
# ---------------------------------------------------------------------------

def make_building(footprint: Sequence[Point], height: float, color: Sequence[float]) -> dict | None:
    """color: sRGB in [0, 1]. Returns None for unusable footprints."""
    footprint = clean_polygon(footprint)
    triangles = triangulate(footprint)
    if len(footprint) < 3 or not triangles:
        return None
    return {
        "footprint": [[round(x, 3), round(z, 3)] for x, z in footprint],
        "triangles": triangles,
        "height": round(float(height), 3),
        "color": [round(float(c), 4) for c in color],
    }


def make_foliage_area(polygon: Sequence[Point], kind: str) -> dict | None:
    polygon = clean_polygon(polygon)
    if len(polygon) < 3:
        return None
    return {"type": kind, "polygon": [[round(x, 3), round(z, 3)] for x, z in polygon]}


def write_map(directory: Path, name: str, size: Point, height_range: Point,
              buildings: list[dict], foliage_areas: list[dict]) -> None:
    """Writes map.json, buildings.json and foliage.json.

    heightmap.png and albedo.png are written separately by the caller.
    """
    directory.mkdir(parents=True, exist_ok=True)
    manifest = {
        "version": FORMAT_VERSION,
        "name": name,
        "size": [float(size[0]), float(size[1])],
        "heightRange": [float(height_range[0]), float(height_range[1])],
        "heightmap": "heightmap.png",
        "albedo": "albedo.png",
        "buildings": "buildings.json",
        "foliage": "foliage.json",
    }
    (directory / "map.json").write_text(json.dumps(manifest, indent=2))
    (directory / "buildings.json").write_text(json.dumps({"buildings": buildings}, separators=(",", ":")))
    (directory / "foliage.json").write_text(json.dumps({"areas": foliage_areas}, separators=(",", ":")))
