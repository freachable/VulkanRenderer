#pragma once

#include "graphics/vulkan/Vulkan.hpp"
#include "graphics/vulkan/VulkanContext.hpp"

// A 2D image with one mip level and layer, in its own device-local memory, plus a view over it.
// The building block for textures and render targets alike.
class Image
{
public:
	Image(VulkanContext const& context, vk::Extent2D extent, vk::Format format, vk::ImageUsageFlags usage,
	      vk::ImageAspectFlags aspectMask);

	Image(Image&& other) noexcept = default;
	Image& operator=(Image&& other) noexcept;
	Image(Image const&) = delete;
	Image& operator=(Image const&) = delete;

	vk::raii::Image const&     getHandle() const { return image; }
	vk::raii::ImageView const& getView() const { return view; }
	vk::Format                 getFormat() const { return format; }
	vk::Extent2D               getExtent() const { return extent; }
	vk::ImageAspectFlags       getAspectMask() const { return aspectMask; }

private:
	vk::Format           format;
	vk::Extent2D         extent;
	vk::ImageAspectFlags aspectMask;

	// Destroyed in reverse order: view, then image, then memory.
	vk::raii::DeviceMemory memory = nullptr;
	vk::raii::Image        image  = nullptr;
	vk::raii::ImageView    view   = nullptr;
};
