#include "MapData.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>
#include <stb_image.h>  // The implementation is compiled in graphics/Texture.cpp.

namespace
{
	using json = nlohmann::json;

	constexpr int supportedFormatVersion = 1;

	json readJson(std::filesystem::path const& path)
	{
		std::ifstream file(path);
		if (!file)
		{
			throw std::runtime_error("failed to open map file: " + path.string());
		}

		try
		{
			return json::parse(file);
		}
		catch (json::exception const& e)
		{
			throw std::runtime_error("failed to parse " + path.string() + ": " + e.what());
		}
	}

	glm::vec2 readVec2(json const& value)
	{
		return { value.at(0).get<float>(), value.at(1).get<float>() };
	}

	std::vector<glm::vec2> readPolygon(json const& value)
	{
		std::vector<glm::vec2> polygon;
		polygon.reserve(value.size());
		for (json const& point : value)
		{
			polygon.push_back(readVec2(point));
		}
		return polygon;
	}

	glm::vec3 srgbToLinear(glm::vec3 color)
	{
		return glm::pow(glm::clamp(color, glm::vec3(0.0f), glm::vec3(1.0f)), glm::vec3(2.2f));
	}

	HeightField loadHeightField(std::filesystem::path const& path, float heightMin, float heightMax)
	{
		int width = 0, height = 0, channels = 0;
		// 8-bit images are widened to 16 bits by stb.
		stbi_us* pixels = stbi_load_16(path.string().c_str(), &width, &height, &channels, 1);
		if (pixels == nullptr)
		{
			throw std::runtime_error("failed to load heightmap " + path.string() + ": " + stbi_failure_reason());
		}

		if (width < 2 || height < 2)
		{
			stbi_image_free(pixels);
			throw std::runtime_error("heightmap must be at least 2x2: " + path.string());
		}

		HeightField field
		{
			.width   = static_cast<uint32_t>(width),
			.height  = static_cast<uint32_t>(height),
			.heights = std::vector<float>(static_cast<size_t>(width) * height)
		};

		for (size_t i = 0; i < field.heights.size(); i++)
		{
			field.heights[i] = heightMin + (heightMax - heightMin) * (static_cast<float>(pixels[i]) / 65535.0f);
		}

		stbi_image_free(pixels);
		return field;
	}

	std::vector<BuildingData> loadBuildings(std::filesystem::path const& path)
	{
		json const root = readJson(path);

		std::vector<BuildingData> buildings;
		buildings.reserve(root.at("buildings").size());

		for (json const& entry : root.at("buildings"))
		{
			BuildingData building
			{
				.footprint = readPolygon(entry.at("footprint")),
				.triangles = entry.value("triangles", std::vector<uint32_t>{}),
				.height    = entry.at("height").get<float>()
			};

			if (entry.contains("color"))
			{
				json const& c = entry.at("color");
				building.color = srgbToLinear({ c.at(0).get<float>(), c.at(1).get<float>(), c.at(2).get<float>() });
			}

			buildings.push_back(std::move(building));
		}

		return buildings;
	}

	std::vector<FoliageArea> loadFoliage(std::filesystem::path const& path)
	{
		json const root = readJson(path);

		std::vector<FoliageArea> areas;
		for (json const& entry : root.at("areas"))
		{
			FoliageArea area
			{
				.type    = entry.value("type", std::string("green")),
				.polygon = readPolygon(entry.at("polygon"))
			};

			if (area.polygon.size() < 3)
			{
				continue;
			}

			area.boundsMin = area.boundsMax = area.polygon.front();
			for (glm::vec2 const& p : area.polygon)
			{
				area.boundsMin = glm::min(area.boundsMin, p);
				area.boundsMax = glm::max(area.boundsMax, p);
			}

			areas.push_back(std::move(area));
		}

		return areas;
	}
}

MapData loadMapData(std::filesystem::path const& directory)
{
	json const manifest = readJson(directory / "map.json");

	int const version = manifest.value("version", 0);
	if (version != supportedFormatVersion)
	{
		throw std::runtime_error("unsupported map format version " + std::to_string(version) + " in " + directory.string());
	}

	glm::vec2 const heightRange = readVec2(manifest.at("heightRange"));

	MapData map
	{
		.name        = manifest.value("name", directory.filename().string()),
		.size        = readVec2(manifest.at("size")),
		.heightField = loadHeightField(directory / manifest.at("heightmap").get<std::string>(), heightRange.x, heightRange.y),
		.albedoPath  = directory / manifest.at("albedo").get<std::string>()
	};

	if (map.size.x <= 0.0f || map.size.y <= 0.0f)
	{
		throw std::runtime_error("map size must be positive in " + directory.string());
	}

	if (manifest.contains("buildings"))
	{
		map.buildings = loadBuildings(directory / manifest.at("buildings").get<std::string>());
	}

	if (manifest.contains("foliage"))
	{
		map.foliageAreas = loadFoliage(directory / manifest.at("foliage").get<std::string>());
	}

	return map;
}

glm::vec2 mapWorldToUv(MapData const& map, glm::vec2 xz)
{
	return (xz + map.size * 0.5f) / map.size;
}

float sampleTerrainHeight(MapData const& map, glm::vec2 xz)
{
	HeightField const& field = map.heightField;

	glm::vec2 const uv = glm::clamp(mapWorldToUv(map, xz), glm::vec2(0.0f), glm::vec2(1.0f));
	float const fx = uv.x * static_cast<float>(field.width - 1);
	float const fy = uv.y * static_cast<float>(field.height - 1);

	uint32_t const x0 = static_cast<uint32_t>(fx);
	uint32_t const y0 = static_cast<uint32_t>(fy);
	uint32_t const x1 = std::min(x0 + 1, field.width - 1);
	uint32_t const y1 = std::min(y0 + 1, field.height - 1);
	float const tx = fx - static_cast<float>(x0);
	float const ty = fy - static_cast<float>(y0);

	auto at = [&field](uint32_t x, uint32_t y) { return field.heights[static_cast<size_t>(y) * field.width + x]; };

	float const top    = glm::mix(at(x0, y0), at(x1, y0), tx);
	float const bottom = glm::mix(at(x0, y1), at(x1, y1), tx);
	return glm::mix(top, bottom, ty);
}
