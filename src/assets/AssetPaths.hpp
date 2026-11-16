#pragma once

#include <filesystem>
#include <string_view>

// Resolves asset names to full paths. Names are relative to their folder and may contain subfolders.
// Throws std::runtime_error if the file does not exist.
class AssetPaths
{
public:
	static std::filesystem::path getTexture(std::string_view name); // assets/textures/<name>
	static std::filesystem::path getMesh(std::string_view name);    // assets/meshes/<name>
	static std::filesystem::path getShader(std::string_view name);  // <bin>/shaders/<name>.spv (compiled from assets/shaders/<name>.slang)

private:
	static std::filesystem::path resolve(std::filesystem::path const& path);
};
