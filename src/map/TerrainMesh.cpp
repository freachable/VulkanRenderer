#include "TerrainMesh.hpp"

#include <algorithm>
#include <cassert>

TerrainMesh buildTerrainMesh(MapData const& map, uint32_t maxVerticesPerSide, uint32_t chunkCells)
{
	assert(maxVerticesPerSide >= 2 && chunkCells >= 1);

	uint32_t const nx = std::min(map.heightField.width, maxVerticesPerSide);
	uint32_t const nz = std::min(map.heightField.height, maxVerticesPerSide);

	glm::vec2 const origin = -map.size * 0.5f;
	glm::vec2 const step   = map.size / glm::vec2(static_cast<float>(nx - 1), static_cast<float>(nz - 1));

	auto vertexIndex = [nx](uint32_t i, uint32_t j) { return j * nx + i; };

	TerrainMesh mesh;

	// Vertices: positions first, normals need the neighbors' heights.
	mesh.vertices.resize(static_cast<size_t>(nx) * nz);
	for (uint32_t j = 0; j < nz; j++)
	{
		for (uint32_t i = 0; i < nx; i++)
		{
			glm::vec2 const xz = origin + step * glm::vec2(static_cast<float>(i), static_cast<float>(j));
			mesh.vertices[vertexIndex(i, j)] =
			{
				.position = { xz.x, sampleTerrainHeight(map, xz), xz.y },
				.normal   = { 0.0f, 1.0f, 0.0f },
				.uv       = { static_cast<float>(i) / static_cast<float>(nx - 1), static_cast<float>(j) / static_cast<float>(nz - 1) }
			};
		}
	}

	// Normals from central differences (one-sided at the border).
	for (uint32_t j = 0; j < nz; j++)
	{
		for (uint32_t i = 0; i < nx; i++)
		{
			uint32_t const iL = i > 0 ? i - 1 : i, iR = std::min(i + 1, nx - 1);
			uint32_t const jN = j > 0 ? j - 1 : j, jS = std::min(j + 1, nz - 1);

			float const dhdx = (mesh.vertices[vertexIndex(iR, j)].position.y - mesh.vertices[vertexIndex(iL, j)].position.y)
			                 / (static_cast<float>(iR - iL) * step.x);
			float const dhdz = (mesh.vertices[vertexIndex(i, jS)].position.y - mesh.vertices[vertexIndex(i, jN)].position.y)
			                 / (static_cast<float>(jS - jN) * step.y);

			mesh.vertices[vertexIndex(i, j)].normal = glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
		}
	}

	// Indices, grouped by chunk.
	uint32_t const cellsX = nx - 1;
	uint32_t const cellsZ = nz - 1;
	mesh.indices.reserve(static_cast<size_t>(cellsX) * cellsZ * 6);

	for (uint32_t cz = 0; cz < cellsZ; cz += chunkCells)
	{
		for (uint32_t cx = 0; cx < cellsX; cx += chunkCells)
		{
			uint32_t const endX = std::min(cx + chunkCells, cellsX);
			uint32_t const endZ = std::min(cz + chunkCells, cellsZ);

			TerrainChunk chunk{ .firstIndex = static_cast<uint32_t>(mesh.indices.size()) };
			chunk.boundsMin = chunk.boundsMax = mesh.vertices[vertexIndex(cx, cz)].position;

			for (uint32_t j = cz; j < endZ; j++)
			{
				for (uint32_t i = cx; i < endX; i++)
				{
					uint32_t const v00 = vertexIndex(i, j);         // (x,     z)
					uint32_t const v10 = vertexIndex(i + 1, j);     // (x + 1, z)
					uint32_t const v01 = vertexIndex(i, j + 1);     // (x,     z + 1)
					uint32_t const v11 = vertexIndex(i + 1, j + 1); // (x + 1, z + 1)

					// Counter-clockwise seen from +y (x east, z south).
					mesh.indices.insert(mesh.indices.end(), { v00, v01, v10, v10, v01, v11 });
				}
			}

			for (uint32_t j = cz; j <= endZ; j++)
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
