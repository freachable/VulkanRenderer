#pragma once

#include "MeshData.hpp"

#include <filesystem>

// Loads every shape in a Wavefront .obj file into a single triangulated mesh.
// Vertices sharing the same position/texcoord/normal indices are merged.
// Texture coordinates are flipped to Vulkan's top-left origin; vertex color is white.
// Throws std::runtime_error if the file cannot be read or parsed.
MeshData loadObj(std::filesystem::path const& path);
