#include "ImageLoader.hpp"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

ImageData loadImage(std::filesystem::path const& path)
{
	int width    = 0;
	int height   = 0;
	int channels = 0; // The file's own channel count; the result always has 4.

	std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
		stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha),
		&stbi_image_free);

	if (!pixels)
	{
		throw std::runtime_error("failed to load image '" + path.string() + "': " + stbi_failure_reason());
	}

	size_t const size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;

	return ImageData
	{
		.width  = static_cast<uint32_t>(width),
		.height = static_cast<uint32_t>(height),
		.pixels = std::vector<uint8_t>(pixels.get(), pixels.get() + size)
	};
}
