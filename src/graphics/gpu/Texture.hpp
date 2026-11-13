#pragma once

#include "assets/ImageData.hpp"
#include "graphics/gpu/Image.hpp"
#include "graphics/vulkan/VulkanContext.hpp"

// A sampled RGBA8 sRGB image in device-local memory, left in eShaderReadOnlyOptimal.
class Texture
{
public:
	// Uploads data through a staging buffer; blocks until the copy is done.
	Texture(VulkanContext const& context, ImageData const& data);

	Image const& getImage() const { return image; }

private:
	Image image;
};
