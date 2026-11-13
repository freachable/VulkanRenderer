#pragma once

#include "graphics/vulkan/Vulkan.hpp"

// One image layout transition, plus what it waits for (src) and what waits for it (dst).
struct ImageTransition
{
	vk::ImageLayout         oldLayout;
	vk::ImageLayout         newLayout;
	vk::PipelineStageFlags2 srcStage;
	vk::AccessFlags2        srcAccess;
	vk::PipelineStageFlags2 dstStage;
	vk::AccessFlags2        dstAccess;
};

// Records a synchronization2 barrier that moves the first mip level and layer of image to a new layout.
void transitionImage(vk::raii::CommandBuffer const& commandBuffer, vk::Image image, vk::ImageAspectFlags aspectMask,
                     ImageTransition const& transition);
