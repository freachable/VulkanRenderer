#pragma once

#include "assets/ImageData.hpp"

#include <filesystem>

// Loads an image file (PNG, JPEG, BMP, TGA, ...) and converts it to 8-bit RGBA.
// Throws std::runtime_error if the file cannot be read or decoded.
ImageData loadImage(std::filesystem::path const& path);
