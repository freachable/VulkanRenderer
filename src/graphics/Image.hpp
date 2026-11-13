#pragma once

#include "Vulkan.hpp"
#include "VulkanContext.hpp"

#include <cstdint>
#include <utility>

// Shared helpers for the image + memory + view triple that Texture, DepthBuffer and
// other render targets are made of. They only create; ownership stays with the caller.
namespace ImageUtils
{
	// A 2D image with a single mip level and layer, bound to its own freshly allocated memory.
	std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(
	    VulkanContext const& context, uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling,
	    vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

	// A 2D view over the first mip level and layer of image.
	vk::raii::ImageView createImageView(
	    VulkanContext const& context, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectMask);
}
