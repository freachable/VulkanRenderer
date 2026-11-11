#include "BuildingMesh.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <utility>

namespace
{
	// How far walls reach below the lowest terrain point, so sloped ground shows no gap.
	constexpr float foundationDepth = 1.0f;
	// Minimum roof height above the highest terrain point under the building.
	constexpr float minimumClearance = 2.0f;

	using Edge = std::pair<uint32_t, uint32_t>;

	Edge undirected(uint32_t a, uint32_t b)
	{
		return a < b ? Edge{ a, b } : Edge{ b, a };
	}

	// > 0 when (a, b, c) is counter-clockwise seen from +y, i.e. its normal points up.
	float upwardArea(glm::vec2 a, glm::vec2 b, glm::vec2 c)
	{
		glm::vec2 const u = b - a;
		glm::vec2 const v = c - a;
		return u.y * v.x - u.x * v.y;
	}

	// Valid triangles of the building, all wound counter-clockwise seen from above.
	std::vector<std::array<uint32_t, 3>> collectTriangles(BuildingData const& building)
	{
		uint32_t const pointCount = static_cast<uint32_t>(building.footprint.size());

		std::vector<uint32_t> indices = building.triangles;
		if (indices.empty())
		{
			// No triangulation given: assume a convex outline.
			for (uint32_t i = 1; i + 1 < pointCount; i++)
			{
				indices.insert(indices.end(), { 0, i, i + 1 });
			}
		}

		std::vector<std::array<uint32_t, 3>> triangles;
		for (size_t t = 0; t + 2 < indices.size(); t += 3)
		{
			std::array<uint32_t, 3> tri{ indices[t], indices[t + 1], indices[t + 2] };
			if (std::ranges::any_of(tri, [pointCount](uint32_t i) { return i >= pointCount; }))
			{
				continue;
			}

			float const area = upwardArea(building.footprint[tri[0]], building.footprint[tri[1]], building.footprint[tri[2]]);
			if (area == 0.0f)
			{
				continue;
			}
			if (area < 0.0f)
			{
				std::swap(tri[1], tri[2]);
			}

			triangles.push_back(tri);
		}

		return triangles;
	}
}

BuildingMesh buildBuildingMesh(MapData const& map)
{
	BuildingMesh mesh;

	for (BuildingData const& building : map.buildings)
	{
		if (building.footprint.size() < 3 || building.height <= 0.0f)
		{
			continue;
		}

		std::vector<std::array<uint32_t, 3>> const triangles = collectTriangles(building);
		if (triangles.empty())
		{
			continue;
		}

		// Edges used by exactly one triangle are on the outline and get a wall.
		std::map<Edge, int> edgeUse;
		for (auto const& tri : triangles)
		{
			for (int k = 0; k < 3; k++)
			{
				edgeUse[undirected(tri[k], tri[(k + 1) % 3])]++;
			}
		}

		// Sit on the lowest terrain point; walls go a bit deeper, the roof clears the highest point.
		float groundMin = std::numeric_limits<float>::max();
		float groundMax = std::numeric_limits<float>::lowest();
		glm::vec2 boundsMin = building.footprint.front();
		glm::vec2 boundsMax = building.footprint.front();
		for (glm::vec2 const& p : building.footprint)
		{
			float const ground = sampleTerrainHeight(map, p);
			groundMin = std::min(groundMin, ground);
			groundMax = std::max(groundMax, ground);
			boundsMin = glm::min(boundsMin, p);
			boundsMax = glm::max(boundsMax, p);
		}

		float const baseHeight = groundMin - foundationDepth;
		float const roofHeight = std::max(groundMin + building.height, groundMax + minimumClearance);

		BuildingDraw draw
		{
			.firstVertex = static_cast<uint32_t>(mesh.vertices.size()),
			.boundsMin   = { boundsMin.x, baseHeight, boundsMin.y },
			.boundsMax   = { boundsMax.x, roofHeight, boundsMax.y }
		};

		for (auto const& tri : triangles)
		{
			for (int k = 0; k < 3; k++)
			{
				bool const onOutline = edgeUse[undirected(tri[k], tri[(k + 1) % 3])] == 1;
				mesh.vertices.push_back(
				{
					.footprint  = building.footprint[tri[k]],
					.baseHeight = baseHeight,
					.roofHeight = roofHeight,
					.color      = building.color,
					.wallMask   = onOutline ? 1.0f : 0.0f
				});
			}
		}

		draw.vertexCount = static_cast<uint32_t>(mesh.vertices.size()) - draw.firstVertex;
		mesh.buildings.push_back(draw);
	}

	return mesh;
}
