// p1_006, p1_007, p1_008: checks the map loader, the terrain mesh and the building mesh on the CPU,
// without a window or GPU. Run: MapCheck [map directory] [--obj out.obj]  (default: assets/maps/sample)
// --obj also writes terrain and CPU-extruded buildings as an OBJ file for Blender or MeshLab.
// Exit code 0 when all checks pass.

#include "map/BuildingMesh.hpp"
#include "map/MapData.hpp"
#include "map/MapMesh.hpp"
#include "map/TerrainMesh.hpp"

#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
	int failures = 0;

	void check(bool condition, std::string const& what)
	{
		if (!condition)
		{
			std::printf("FAIL: %s\n", what.c_str());
			failures++;
		}
	}

	bool near(float a, float b, float epsilon = 1e-3f)
	{
		return std::abs(a - b) <= epsilon;
	}

	bool inside(glm::vec3 p, glm::vec3 boundsMin, glm::vec3 boundsMax)
	{
		return glm::all(glm::greaterThanEqual(p, boundsMin - 1e-3f)) && glm::all(glm::lessThanEqual(p, boundsMax + 1e-3f));
	}

	// p1_006: world space, texture coordinates and height sampling.
	void checkMapData(MapData const& map)
	{
		glm::vec2 const half = map.size * 0.5f;
		HeightField const& field = map.heightField;

		check(field.width >= 2 && field.height >= 2, "heightmap is at least 2x2");
		check(field.heights.size() == size_t(field.width) * field.height, "heightmap has width * height samples");

		glm::vec2 const uvNorthWest = mapWorldToUv(map, { -half.x, half.y });
		glm::vec2 const uvSouthEast = mapWorldToUv(map, { half.x, -half.y });
		check(near(uvNorthWest.x, 0.0f) && near(uvNorthWest.y, 0.0f), "north-west corner maps to uv (0, 0)");
		check(near(uvSouthEast.x, 1.0f) && near(uvSouthEast.y, 1.0f), "south-east corner maps to uv (1, 1)");

		check(near(sampleTerrainHeight(map, { -half.x, half.y }), field.heights.front()), "north-west height is the first heightmap sample");
		check(near(sampleTerrainHeight(map, { half.x, -half.y }), field.heights.back()), "south-east height is the last heightmap sample");
		check(near(sampleTerrainHeight(map, { -half.x * 2.0f, half.y * 2.0f }), field.heights.front()), "heights outside the map are clamped");

		for (BuildingData const& building : map.buildings)
		{
			for (glm::vec2 const& p : building.footprint)
			{
				if (std::abs(p.x) > half.x + 1e-2f || std::abs(p.y) > half.y + 1e-2f)
				{
					check(false, "building footprints lie inside the map");
					return;
				}
			}
		}
	}

	// p1_007: Z-up grid, upward normals, counter-clockwise from above, chunk bounds.
	void checkTerrain(MapData const& map)
	{
		TerrainMesh const mesh = buildTerrainMesh(map);

		check(!mesh.vertices.empty() && mesh.indices.size() % 3 == 0, "terrain has whole triangles");
		check(!mesh.chunks.empty(), "terrain has chunks");

		size_t chunkIndices = 0;
		for (TerrainChunk const& chunk : mesh.chunks)
		{
			chunkIndices += chunk.indexCount;
			for (uint32_t k = chunk.firstIndex; k < chunk.firstIndex + chunk.indexCount; k++)
			{
				if (!inside(mesh.vertices[mesh.indices[k]].position, chunk.boundsMin, chunk.boundsMax))
				{
					check(false, "chunk bounds contain their vertices");
					break;
				}
			}
		}
		check(chunkIndices == mesh.indices.size(), "chunks cover all indices exactly once");

		bool normalsUp = true;
		for (TerrainVertex const& v : mesh.vertices)
		{
			normalsUp = normalsUp && v.normal.z > 0.0f && near(glm::length(v.normal), 1.0f);
		}
		check(normalsUp, "terrain normals are unit length and point up (+z)");

		bool counterClockwise = true;
		for (size_t k = 0; k < mesh.indices.size(); k += 3)
		{
			glm::vec3 const a = mesh.vertices[mesh.indices[k]].position;
			glm::vec3 const b = mesh.vertices[mesh.indices[k + 1]].position;
			glm::vec3 const c = mesh.vertices[mesh.indices[k + 2]].position;
			counterClockwise = counterClockwise && glm::cross(b - a, c - a).z > 0.0f;
		}
		check(counterClockwise, "terrain triangles are counter-clockwise seen from above");

		// The north-west vertex sits at row 0, column 0 of the heightmap.
		glm::vec3 const first = mesh.vertices.front().position;
		check(near(first.x, -map.size.x * 0.5f) && near(first.y, map.size.y * 0.5f), "first terrain vertex is the north-west corner");

		std::printf("terrain:   %zu vertices, %zu triangles, %zu chunks\n", mesh.vertices.size(), mesh.indices.size() / 3, mesh.chunks.size());
	}

	// p1_008: geometry shader input, counter-clockwise from above, walls on the outline only.
	void checkBuildings(MapData const& map)
	{
		BuildingMesh const mesh = buildBuildingMesh(map);

		check(mesh.buildings.size() <= map.buildings.size(), "no more draws than buildings");
		check(map.buildings.empty() || !mesh.buildings.empty(), "buildings are not all dropped");

		size_t walls = 0;
		bool counterClockwise = true;
		bool roofsAboveBase = true;
		bool boundsOk = true;
		bool aboveGround = true;
		for (BuildingDraw const& draw : mesh.buildings)
		{
			check(draw.vertexCount % 3 == 0 && draw.vertexCount > 0, "building draws hold whole triangles");
			for (uint32_t k = draw.firstVertex; k + 2 < draw.firstVertex + draw.vertexCount; k += 3)
			{
				BuildingVertex const& a = mesh.vertices[k];
				BuildingVertex const& b = mesh.vertices[k + 1];
				BuildingVertex const& c = mesh.vertices[k + 2];
				glm::vec2 const u = b.footprint - a.footprint;
				glm::vec2 const v = c.footprint - a.footprint;
				counterClockwise = counterClockwise && u.x * v.y - u.y * v.x > 0.0f;
			}
			for (uint32_t k = draw.firstVertex; k < draw.firstVertex + draw.vertexCount; k++)
			{
				BuildingVertex const& v = mesh.vertices[k];
				roofsAboveBase = roofsAboveBase && v.roofHeight > v.baseHeight;
				boundsOk = boundsOk && inside({ v.footprint, v.baseHeight }, draw.boundsMin, draw.boundsMax)
				                    && inside({ v.footprint, v.roofHeight }, draw.boundsMin, draw.boundsMax);
				aboveGround = aboveGround && v.roofHeight > sampleTerrainHeight(map, v.footprint);
				walls += v.wallMask > 0.5f ? 1 : 0;
			}
		}
		check(counterClockwise, "building triangles are counter-clockwise seen from above");
		check(roofsAboveBase, "roofs are above the wall bottoms");
		check(boundsOk, "building bounds contain roof and base");
		check(aboveGround, "roofs are above the terrain");

		// Every footprint outline edge gets exactly one wall.
		size_t outlineEdges = 0;
		for (BuildingData const& building : map.buildings)
		{
			outlineEdges += building.footprint.size() >= 3 && building.height > 0.0f ? building.footprint.size() : 0;
		}
		check(walls == outlineEdges, "one wall per outline edge (" + std::to_string(walls) + " walls, " + std::to_string(outlineEdges) + " edges)");

		std::printf("buildings: %zu of %zu drawn, %zu vertices, %zu walls\n", mesh.buildings.size(), map.buildings.size(), mesh.vertices.size(), walls);
	}

	// p1_008: CPU extrusion, every triangle faces along its normal (outwards / up).
	void checkMapMesh(MapMesh const& mesh, char const* what)
	{
		bool facing = true;
		for (size_t k = 0; k + 2 < mesh.indices.size(); k += 3)
		{
			MapMeshVertex const& a = mesh.vertices[mesh.indices[k]];
			glm::vec3 const faceNormal = glm::cross(mesh.vertices[mesh.indices[k + 1]].position - a.position,
			                                        mesh.vertices[mesh.indices[k + 2]].position - a.position);
			facing = facing && glm::dot(faceNormal, a.normal) > 0.0f;
		}
		check(facing, std::string(what) + " triangles are counter-clockwise seen from their normal side");
	}

	// p1_001: readers reject other format versions.
	void checkVersionRejected()
	{
		std::filesystem::path const directory = std::filesystem::temp_directory_path() / "VulkanRendererMapCheck";
		std::filesystem::create_directories(directory);
		std::ofstream(directory / "map.json") << R"({ "version": 2, "size": [1, 1], "heightRange": [0, 1], "heightmap": "h.png", "albedo": "a.png" })";

		bool rejected = false;
		try
		{
			loadMapData(directory);
		}
		catch (std::exception const&)
		{
			rejected = true;
		}
		std::filesystem::remove_all(directory);
		check(rejected, "format version 2 is rejected");
	}
}

int main(int argc, char** argv)
{
	std::filesystem::path directory = std::filesystem::path(VR_ASSET_DIR) / "maps" / "sample";
	std::filesystem::path objPath;
	for (int i = 1; i < argc; i++)
	{
		std::string const arg = argv[i];
		if (arg == "--obj" && i + 1 < argc)
		{
			objPath = argv[++i];
		}
		else
		{
			directory = arg;
		}
	}

	try
	{
		MapData const map = loadMapData(directory);
		std::printf("map:       %s, %.0f x %.0f m, heightmap %ux%u, %zu buildings, %zu foliage areas\n",
		            map.name.c_str(), map.size.x, map.size.y, map.heightField.width, map.heightField.height,
		            map.buildings.size(), map.foliageAreas.size());

		checkMapData(map);
		checkTerrain(map);
		checkBuildings(map);
		checkVersionRejected();

		MapMesh const terrain   = terrainToMapMesh(buildTerrainMesh(map));
		MapMesh const buildings = extrudeBuildings(map, buildBuildingMesh(map));
		checkMapMesh(terrain, "terrain");
		checkMapMesh(buildings, "extruded building");
		std::printf("extruded:  %zu building triangles\n", buildings.indices.size() / 3);

		if (!objPath.empty())
		{
			writeMapObj(objPath, map, terrain, buildings);
			std::printf("wrote %s\n", objPath.string().c_str());
		}
	}
	catch (std::exception const& e)
	{
		std::printf("FAIL: %s\n", e.what());
		return 1;
	}

	std::printf(failures == 0 ? "all checks passed\n" : "%d checks failed\n", failures);
	return failures == 0 ? 0 : 1;
}
