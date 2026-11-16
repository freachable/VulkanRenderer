#pragma once

#include "graphics/gpu/Image.hpp"
#include "graphics/vulkan/Vulkan.hpp"
#include "graphics/vulkan/VulkanContext.hpp"

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

	Image const& getImage() const { return image; }

	// Needed up front for PipelineRenderingCreateInfo::depthAttachmentFormat; never changes.
	vk::Format getFormat() const { return image.getFormat(); }

private:
	VulkanContext const& context;
	Image                image;
};
