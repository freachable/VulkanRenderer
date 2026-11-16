#include "Barriers.hpp"

void transitionImage(vk::raii::CommandBuffer const& commandBuffer, vk::Image image, vk::ImageAspectFlags aspectMask,
                     ImageTransition const& transition)
{
	vk::ImageMemoryBarrier2 barrier
	{
		.srcStageMask        = transition.srcStage,
		.srcAccessMask       = transition.srcAccess,
		.dstStageMask        = transition.dstStage,
		.dstAccessMask       = transition.dstAccess,
		.oldLayout           = transition.oldLayout,
		.newLayout           = transition.newLayout,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image               = image,
		.subresourceRange    =
		{
			.aspectMask     = aspectMask,
			.baseMipLevel   = 0,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = 1
		}
	};

	commandBuffer.pipelineBarrier2(vk::DependencyInfo
	{
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers    = &barrier
	});
}
