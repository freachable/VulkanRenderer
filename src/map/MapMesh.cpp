// p1_008 (GitHub #12): CPU extrusion of buildings and OBJ export, see MapMesh.hpp.
#include "MapMesh.hpp"

#include <fstream>
#include <stdexcept>

MapMesh terrainToMapMesh(TerrainMesh const& terrain)
{
	MapMesh mesh;
	mesh.vertices.reserve(terrain.vertices.size());
	for (TerrainVertex const& v : terrain.vertices)
	{
		mesh.vertices.push_back({ .position = v.position, .normal = v.normal, .color = glm::vec3(1.0f), .uv = v.uv });
	}
	mesh.indices = terrain.indices;
	return mesh;
}

MapMesh extrudeBuildings(MapData const& map, BuildingMesh const& buildings)
{
	MapMesh mesh;

	auto addVertex = [&](glm::vec2 footprint, float height, glm::vec3 normal, glm::vec3 color)
	{
		mesh.vertices.push_back(
		{
			.position = { footprint, height },
			.normal   = normal,
			.color    = color,
			.uv       = mapWorldToUv(map, footprint)
		});
		return static_cast<uint32_t>(mesh.vertices.size() - 1);
	};

	// The geometry shader input is a list of footprint triangles, counter-clockwise seen from above.
	for (size_t t = 0; t + 2 < buildings.vertices.size(); t += 3)
	{
		BuildingVertex const* tri = &buildings.vertices[t];
		glm::vec3 const color = tri[0].color;

		// Roof: the triangle at roof height.
		for (int i = 0; i < 3; i++)
		{
			mesh.indices.push_back(addVertex(tri[i].footprint, tri[i].roofHeight, { 0.0f, 0.0f, 1.0f }, color));
		}

		// Walls on outline edges. Edge a -> b runs counter-clockwise seen from above, so the quad
		// (aBottom, bBottom, aTop, bTop) faces outwards.
		for (int i = 0; i < 3; i++)
		{
			if (tri[i].wallMask < 0.5f)
			{
				continue;
			}

			BuildingVertex const& a = tri[i];
			BuildingVertex const& b = tri[(i + 1) % 3];

			glm::vec2 const edge = b.footprint - a.footprint;
			glm::vec3 const normal = glm::normalize(glm::vec3(edge.y, -edge.x, 0.0f));

			uint32_t const aBottom = addVertex(a.footprint, a.baseHeight, normal, color);
			uint32_t const bBottom = addVertex(b.footprint, b.baseHeight, normal, color);
			uint32_t const aTop    = addVertex(a.footprint, a.roofHeight, normal, color);
			uint32_t const bTop    = addVertex(b.footprint, b.roofHeight, normal, color);

			mesh.indices.insert(mesh.indices.end(), { aBottom, bBottom, aTop, aTop, bBottom, bTop });
		}
	}

	return mesh;
}

MeshData toMeshData(MapMesh const& mesh)
{
	MeshData data;
	data.vertices.reserve(mesh.vertices.size());
	for (MapMeshVertex const& v : mesh.vertices)
	{
		data.vertices.push_back({ .pos = v.position, .color = v.color, .texCoord = v.uv });
	}
	data.indices = mesh.indices;
	return data;
}

namespace
{
	glm::vec3 toObjSpace(glm::vec3 p)
	{
		return { p.x, p.z, -p.y };
	}

	// sRGB, since viewers show OBJ vertex colors as is.
	glm::vec3 linearToSrgb(glm::vec3 c)
	{
		return glm::pow(glm::clamp(c, glm::vec3(0.0f), glm::vec3(1.0f)), glm::vec3(1.0f / 2.2f));
	}

	void writeGroup(std::ofstream& obj, char const* name, char const* material, MapMesh const& mesh, size_t& vertexOffset)
	{
		obj << "o " << name << "\nusemtl " << material << "\n";
		for (MapMeshVertex const& v : mesh.vertices)
		{
			glm::vec3 const p = toObjSpace(v.position);
			glm::vec3 const c = linearToSrgb(v.color);
			obj << "v " << p.x << ' ' << p.y << ' ' << p.z << ' ' << c.r << ' ' << c.g << ' ' << c.b << '\n';
		}
		for (MapMeshVertex const& v : mesh.vertices)
		{
			glm::vec3 const n = toObjSpace(v.normal);
			obj << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
		}
		for (MapMeshVertex const& v : mesh.vertices)
		{
			obj << "vt " << v.uv.x << ' ' << 1.0f - v.uv.y << '\n';  // OBJ's v grows upwards.
		}
		for (size_t k = 0; k + 2 < mesh.indices.size(); k += 3)
		{
			obj << 'f';
			for (size_t i = 0; i < 3; i++)
			{
				size_t const index = vertexOffset + mesh.indices[k + i] + 1;
				obj << ' ' << index << '/' << index << '/' << index;
			}
			obj << '\n';
		}
		vertexOffset += mesh.vertices.size();
	}
}

void writeMapObj(std::filesystem::path const& objPath, MapData const& map, MapMesh const& terrain, MapMesh const& buildings)
{
	std::filesystem::path const directory = objPath.has_parent_path() ? objPath.parent_path() : std::filesystem::path(".");
	std::filesystem::create_directories(directory);

	std::filesystem::path mtlPath = objPath;
	mtlPath.replace_extension(".mtl");
	std::filesystem::path const albedoName = objPath.stem().string() + "_albedo" + map.albedoPath.extension().string();
	std::filesystem::copy_file(map.albedoPath, directory / albedoName, std::filesystem::copy_options::overwrite_existing);

	std::ofstream mtl(mtlPath);
	mtl << "newmtl terrain\nKd 1 1 1\nmap_Kd " << albedoName.string() << "\n\n"
	    << "newmtl building\nKd 0.75 0.75 0.75\n";

	std::ofstream obj(objPath);
	if (!obj || !mtl)
	{
		throw std::runtime_error("failed to write " + objPath.string());
	}
	obj << "# " << map.name << ", exported by MapCheck. Y-up: OBJ (x, y, z) = world (x, z, -y).\n"
	    << "mtllib " << mtlPath.filename().string() << '\n';

	size_t vertexOffset = 0;
	writeGroup(obj, "terrain", "terrain", terrain, vertexOffset);
	writeGroup(obj, "buildings", "building", buildings, vertexOffset);
}
