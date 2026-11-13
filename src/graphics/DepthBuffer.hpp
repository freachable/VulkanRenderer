#pragma once

#include "Vulkan.hpp"
#include "VulkanContext.hpp"

// Depth attachment sized to match the swapchain. Its contents only live for one frame,
// so it needs no upload; recreate it whenever the swapchain extent changes.
class DepthBuffer
{
public:
	// Picks the first depth format the device supports as an optimal-tiling attachment.
	DepthBuffer(VulkanContext const& context, vk::Extent2D extent);

	// Holds a reference to the context, so it stays put.
	DepthBuffer(DepthBuffer const&) = delete;
	DepthBuffer& operator=(DepthBuffer const&) = delete;
	DepthBuffer(DepthBuffer&&) = delete;
	DepthBuffer& operator=(DepthBuffer&&) = delete;

	// The caller must make sure the device is idle (the image is no longer in use).
	void recreate(vk::Extent2D extent);

	// Needed up front for PipelineRenderingCreateInfo::depthAttachmentFormat; never changes.
	vk::Format getFormat() const { return format; }
	vk::ImageAspectFlags getAspectMask() const { return aspectMask; }

	vk::raii::Image const&     getImage() const { return depthImage; }
	vk::raii::ImageView const& getImageView() const { return depthImageView; }

private:
	void create(vk::Extent2D extent);
	void destroy();

	static vk::Format findDepthFormat(vk::raii::PhysicalDevice const& physicalDevice);
	static bool hasStencilComponent(vk::Format format);

	VulkanContext const& context;

	vk::Format           format;
	vk::ImageAspectFlags aspectMask;

	// Destroyed in reverse order: view, then image, then memory.
	vk::raii::DeviceMemory depthImageMemory = nullptr;
	vk::raii::Image        depthImage       = nullptr;
	vk::raii::ImageView    depthImageView   = nullptr;
};
