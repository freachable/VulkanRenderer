#include "TerrainMesh.hpp"

#include <algorithm>
#include <cassert>

TerrainMesh buildTerrainMesh(MapData const& map, uint32_t maxVerticesPerSide, uint32_t chunkCells)
{
	assert(maxVerticesPerSide >= 2 && chunkCells >= 1);

	// Grid column i runs west to east, row j north to south, like the heightmap image.
	uint32_t const nx = std::min(map.heightField.width, maxVerticesPerSide);
	uint32_t const ny = std::min(map.heightField.height, maxVerticesPerSide);

	glm::vec2 const northWest = { -map.size.x * 0.5f, map.size.y * 0.5f };
	glm::vec2 const step      = { map.size.x / static_cast<float>(nx - 1), -map.size.y / static_cast<float>(ny - 1) };

	auto vertexIndex = [nx](uint32_t i, uint32_t j) { return j * nx + i; };

	TerrainMesh mesh;

	// Vertices: positions first, normals need the neighbors' heights.
	mesh.vertices.resize(static_cast<size_t>(nx) * ny);
	for (uint32_t j = 0; j < ny; j++)
	{
		for (uint32_t i = 0; i < nx; i++)
		{
			glm::vec2 const xy = northWest + step * glm::vec2(static_cast<float>(i), static_cast<float>(j));
			mesh.vertices[vertexIndex(i, j)] =
			{
				.position = { xy.x, xy.y, sampleTerrainHeight(map, xy) },
				.normal   = { 0.0f, 0.0f, 1.0f },
				.uv       = { static_cast<float>(i) / static_cast<float>(nx - 1), static_cast<float>(j) / static_cast<float>(ny - 1) }
			};
		}
	}

	// Normals from central differences (one-sided at the border).
	for (uint32_t j = 0; j < ny; j++)
	{
		for (uint32_t i = 0; i < nx; i++)
		{
			uint32_t const iW = i > 0 ? i - 1 : i, iE = std::min(i + 1, nx - 1);
			uint32_t const jN = j > 0 ? j - 1 : j, jS = std::min(j + 1, ny - 1);

			float const dhdx = (mesh.vertices[vertexIndex(iE, j)].position.z - mesh.vertices[vertexIndex(iW, j)].position.z)
			                 / (static_cast<float>(iE - iW) * step.x);
			// step.y is negative: going south (j + 1) lowers y.
			float const dhdy = (mesh.vertices[vertexIndex(i, jS)].position.z - mesh.vertices[vertexIndex(i, jN)].position.z)
			                 / (static_cast<float>(jS - jN) * step.y);

			mesh.vertices[vertexIndex(i, j)].normal = glm::normalize(glm::vec3(-dhdx, -dhdy, 1.0f));
		}
	}

	// Indices, grouped by chunk.
	uint32_t const cellsX = nx - 1;
	uint32_t const cellsY = ny - 1;
	mesh.indices.reserve(static_cast<size_t>(cellsX) * cellsY * 6);

	for (uint32_t cy = 0; cy < cellsY; cy += chunkCells)
	{
		for (uint32_t cx = 0; cx < cellsX; cx += chunkCells)
		{
			uint32_t const endX = std::min(cx + chunkCells, cellsX);
			uint32_t const endY = std::min(cy + chunkCells, cellsY);

			TerrainChunk chunk{ .firstIndex = static_cast<uint32_t>(mesh.indices.size()) };
			chunk.boundsMin = chunk.boundsMax = mesh.vertices[vertexIndex(cx, cy)].position;

			for (uint32_t j = cy; j < endY; j++)
			{
				for (uint32_t i = cx; i < endX; i++)
				{
					uint32_t const v00 = vertexIndex(i, j);         // north-west corner of the cell
					uint32_t const v10 = vertexIndex(i + 1, j);     // north-east
					uint32_t const v01 = vertexIndex(i, j + 1);     // south-west
					uint32_t const v11 = vertexIndex(i + 1, j + 1); // south-east

					// Counter-clockwise seen from +z (x east, y north).
					mesh.indices.insert(mesh.indices.end(), { v00, v01, v10, v10, v01, v11 });
				}
			}

			for (uint32_t j = cy; j <= endY; j++)
			{
				for (uint32_t i = cx; i <= endX; i++)
				{
					glm::vec3 const& p = mesh.vertices[vertexIndex(i, j)].position;
					chunk.boundsMin = glm::min(chunk.boundsMin, p);
					chunk.boundsMax = glm::max(chunk.boundsMax, p);
				}
			}

			chunk.indexCount = static_cast<uint32_t>(mesh.indices.size()) - chunk.firstIndex;
			mesh.chunks.push_back(chunk);
		}
	}

	return mesh;
}
