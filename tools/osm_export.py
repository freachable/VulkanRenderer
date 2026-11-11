"""Exports a map area from OpenStreetMap into the renderer's data format.

    pip install -r tools/requirements.txt
    python tools/osm_export.py --bbox 19.030 47.490 19.070 47.515 --out data/budapest
    python tools/osm_export.py --bbox ... --dem dem.tif --out data/budapest

--bbox is west south east north in degrees (WGS84).

OpenStreetMap has no elevation data. Pass --dem with a GeoTIFF digital
elevation model covering the area (e.g. Copernicus GLO-30 or SRTM, downloadable
from OpenTopography). Without --dem the terrain is flat.

Building heights come from the OSM `height` or `building:levels` tags when
present, otherwise a random height is used (as in the proposal).
"""

from __future__ import annotations

import argparse
import math
import random
from pathlib import Path

import mapformat

try:
    import numpy as np
    import osmnx as ox
    from PIL import Image, ImageDraw
    from shapely.geometry import MultiPolygon, Polygon, box
except ImportError as error:  # pragma: no cover
    raise SystemExit(f"missing dependency ({error.name}); run: pip install -r tools/requirements.txt")

GREEN_TAGS = {
    "leisure": ["park", "garden", "golf_course"],
    "landuse": ["grass", "forest", "meadow", "recreation_ground", "village_green"],
    "natural": ["wood", "scrub", "grassland"],
}
WATER_TAGS = {"natural": ["water"], "waterway": ["riverbank"]}

ROAD_WIDTHS = {  # meters, drawn into the albedo
    "motorway": 18, "trunk": 16, "primary": 14, "secondary": 12, "tertiary": 10,
    "residential": 8, "unclassified": 7, "service": 5, "living_street": 6,
}

COLORS = {
    "ground": (214, 208, 196),
    "green": (150, 196, 120),
    "water": (150, 190, 230),
    "road": (110, 112, 118),
    "building": (170, 166, 160),
}


def polygons_of(geometry):
    if isinstance(geometry, Polygon):
        yield geometry
    elif isinstance(geometry, MultiPolygon):
        yield from geometry.geoms


def parse_height(tags, rng: random.Random) -> float:
    for key in ("height", "building:height"):
        value = tags.get(key)
        if isinstance(value, str):
            try:
                return float(value.replace("m", "").strip())
            except ValueError:
                pass
    levels = tags.get("building:levels")
    if isinstance(levels, str):
        try:
            return float(levels) * 3.2
        except ValueError:
            pass
    return rng.uniform(8.0, 25.0)


def parse_color(value, rng: random.Random):
    if isinstance(value, str) and value.startswith("#") and len(value) == 7:
        return tuple(int(value[i:i + 2], 16) / 255 for i in (1, 3, 5))
    grey = rng.uniform(0.6, 0.85)
    return (grey, grey, grey)


def features(bbox, tags):
    try:
        return ox.features_from_bbox(bbox, tags)
    except Exception as error:  # osmnx raises when nothing matches
        print(f"  no features for {tags}: {error}")
        return None


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--bbox", type=float, nargs=4, required=True, metavar=("WEST", "SOUTH", "EAST", "NORTH"))
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--dem", type=Path, help="GeoTIFF elevation model covering the bbox")
    parser.add_argument("--heightmap-resolution", type=int, default=513)
    parser.add_argument("--albedo-resolution", type=int, default=2048)
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    rng = random.Random(args.seed)
    west, south, east, north = args.bbox
    bbox = (west, south, east, north)  # osmnx 2.x order: left, bottom, right, top

    # Local metric frame: UTM, centered on the bbox, x east, z south
    import geopandas as gpd
    area = gpd.GeoDataFrame(geometry=[box(west, south, east, north)], crs="EPSG:4326")
    area_utm = ox.projection.project_gdf(area)
    crs = area_utm.crs
    minx, miny, maxx, maxy = area_utm.total_bounds
    cx, cy = (minx + maxx) / 2, (miny + maxy) / 2
    size_x, size_z = maxx - minx, maxy - miny
    clip = box(minx, miny, maxx, maxy)

    def to_local(points):
        return [(x - cx, -(y - cy)) for x, y in points]

    print("downloading buildings...")
    buildings = []
    building_gdf = features(bbox, {"building": True})
    if building_gdf is not None:
        building_gdf = building_gdf.to_crs(crs)
        for _, row in building_gdf.iterrows():
            tags = row.to_dict()
            height = parse_height(tags, rng)
            color = parse_color(tags.get("building:colour"), rng)
            for polygon in polygons_of(row.geometry.intersection(clip)):
                if polygon.area < 4.0:
                    continue
                # Holes (courtyards) are ignored: only the outline is extruded.
                building = mapformat.make_building(to_local(polygon.exterior.coords), height, color)
                if building:
                    buildings.append(building)

    print("downloading green areas...")
    foliage = []
    green_polygons = []
    green_gdf = features(bbox, GREEN_TAGS)
    if green_gdf is not None:
        green_gdf = green_gdf.to_crs(crs)
        for _, row in green_gdf.iterrows():
            kind = next((str(row[k]) for k in GREEN_TAGS if k in row and isinstance(row[k], str)), "green")
            for polygon in polygons_of(row.geometry.intersection(clip)):
                if polygon.area < 25.0:
                    continue
                area_dict = mapformat.make_foliage_area(to_local(polygon.exterior.coords), kind)
                if area_dict:
                    foliage.append(area_dict)
                    green_polygons.append(polygon)

    water_gdf = features(bbox, WATER_TAGS)
    water_polygons = []
    if water_gdf is not None:
        for geometry in water_gdf.to_crs(crs).geometry:
            water_polygons += list(polygons_of(geometry.intersection(clip)))

    print("downloading roads...")
    roads = []
    try:
        graph = ox.project_graph(ox.graph_from_bbox(bbox, network_type="all", truncate_by_edge=True), to_crs=crs)
        edges = ox.graph_to_gdfs(graph, nodes=False)
        for _, row in edges.iterrows():
            kind = row.get("highway")
            kind = kind[0] if isinstance(kind, list) else kind
            roads.append((row.geometry, ROAD_WIDTHS.get(kind, 4)))
    except Exception as error:
        print(f"  no roads: {error}")

    # Albedo
    res = args.albedo_resolution
    scale_x, scale_y = res / size_x, res / size_z
    image = Image.new("RGB", (res, res), COLORS["ground"])
    draw = ImageDraw.Draw(image)

    def to_pixels(coords):
        return [((x - minx) * scale_x, (maxy - y) * scale_y) for x, y in coords]

    for polygon in green_polygons:
        draw.polygon(to_pixels(polygon.exterior.coords), fill=COLORS["green"])
    for polygon in water_polygons:
        draw.polygon(to_pixels(polygon.exterior.coords), fill=COLORS["water"])
    for line, width in roads:
        for part in getattr(line, "geoms", [line]):
            draw.line(to_pixels(part.coords), fill=COLORS["road"], width=max(1, round(width * scale_x)), joint="curve")
    for building in buildings:
        coords = [(x + cx, cy - z) for x, z in building["footprint"]]
        draw.polygon(to_pixels(coords), fill=COLORS["building"])

    args.out.mkdir(parents=True, exist_ok=True)
    image.save(args.out / "albedo.png")

    # Heightmap
    n = args.heightmap_resolution
    if args.dem:
        import rasterio
        from rasterio.transform import from_bounds
        from rasterio.warp import Resampling, reproject

        heights = np.zeros((n, n), dtype=np.float32)
        with rasterio.open(args.dem) as dem:
            reproject(
                source=rasterio.band(dem, 1), destination=heights,
                dst_transform=from_bounds(minx, miny, maxx, maxy, n, n), dst_crs=crs,
                resampling=Resampling.bilinear,
            )
    else:
        print("no --dem given: flat terrain")
        heights = np.zeros((n, n), dtype=np.float32)

    height_min, height_max = math.floor(float(heights.min())), math.ceil(float(heights.max()))
    if height_max <= height_min:
        height_max = height_min + 1
    mapformat.write_heightmap_png(args.out / "heightmap.png", heights.tolist(), height_min, height_max)

    mapformat.write_map(args.out, args.out.name, (size_x, size_z), (height_min, height_max), buildings, foliage)
    print(f"wrote {args.out}: {size_x:.0f} x {size_z:.0f} m, {len(buildings)} buildings, {len(foliage)} foliage areas")


if __name__ == "__main__":
    main()
