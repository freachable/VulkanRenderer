#include "ObjLoader.hpp"

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <tiny_obj_loader.h>

namespace
{
	// Identifies a unique vertex by the attribute indices an OBJ face corner refers to.
	struct VertexKey
	{
		int position;
		int normal;
		int texCoord;

		bool operator==(VertexKey const&) const = default;
	};

	struct VertexKeyHash
	{
		size_t operator()(VertexKey const& key) const noexcept
		{
			size_t hash = std::hash<int>{}(key.position);
			hash = hash * 31 + std::hash<int>{}(key.normal);
			hash = hash * 31 + std::hash<int>{}(key.texCoord);
			return hash;
		}
	};
}

MeshData loadObj(std::filesystem::path const& path)
{
	tinyobj::ObjReaderConfig config;
	config.triangulate = true;
	config.vertex_color = false;
	// mtl_search_path left empty: tinyobj looks for the .mtl next to the .obj. A missing one only warns.

	tinyobj::ObjReader reader;
	if (!reader.ParseFromFile(path.string(), config))
	{
		throw std::runtime_error("failed to load OBJ '" + path.string() + "': " + reader.Error());
	}

	tinyobj::attrib_t const& attrib = reader.GetAttrib();

	MeshData mesh;
	std::unordered_map<VertexKey, uint32_t, VertexKeyHash> uniqueVertices;

	for (tinyobj::shape_t const& shape : reader.GetShapes())
	{
		for (tinyobj::index_t const& index : shape.mesh.indices)
		{
			VertexKey key{ index.vertex_index, index.normal_index, index.texcoord_index };

			auto [it, inserted] = uniqueVertices.try_emplace(key, static_cast<uint32_t>(mesh.vertices.size()));
			if (inserted)
			{
				Vertex vertex{};

				vertex.pos =
				{
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				// OBJ puts the texture origin at the bottom-left, Vulkan samples from the top-left.
				if (index.texcoord_index >= 0)
				{
					vertex.texCoord =
					{
						attrib.texcoords[2 * index.texcoord_index + 0],
						1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
					};
				}

				vertex.color = { 1.0f, 1.0f, 1.0f };

				mesh.vertices.push_back(vertex);
			}

			mesh.indices.push_back(it->second);
		}
	}

	if (mesh.indices.empty())
	{
		throw std::runtime_error("OBJ '" + path.string() + "' contains no faces");
	}

	return mesh;
}
