#include "AssetPaths.hpp"

#include <stdexcept>
#include <string>

std::filesystem::path AssetPaths::getTexture(std::string_view name)
{
	return resolve(std::filesystem::path(VR_ASSET_DIR) / "textures" / name);
}

std::filesystem::path AssetPaths::getMesh(std::string_view name)
{
	return resolve(std::filesystem::path(VR_ASSET_DIR) / "meshes" / name);
}

std::filesystem::path AssetPaths::getShader(std::string_view name)
{
	return resolve(std::filesystem::path(VR_SHADER_DIR) / (std::string(name) + ".spv"));
}

std::filesystem::path AssetPaths::resolve(std::filesystem::path const& path)
{
	if (!std::filesystem::exists(path))
	{
		throw std::runtime_error("Asset not found: " + path.string());
	}

	return path;
}
