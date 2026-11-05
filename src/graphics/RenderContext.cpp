#include "RenderContext.hpp"

#include <cassert>
#include <stdexcept>
#include <utility>

namespace
{
	vk::raii::CommandPool createCommandPool(VulkanContext const& context)
	{
		vk::CommandPoolCreateInfo poolInfo
		{
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = context.getGraphicsQueueFamilyIndex()
		};

		return vk::raii::CommandPool(context.getDevice(), poolInfo);
	}

	vk::raii::CommandBuffer allocateCommandBuffer(VulkanContext const& context, vk::raii::CommandPool const& commandPool)
	{
		vk::CommandBufferAllocateInfo allocInfo
		{
			.commandPool = commandPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1
		};

		return std::move(vk::raii::CommandBuffers(context.getDevice(), allocInfo).front());
	}

	void transitionImageLayout(
		vk::raii::CommandBuffer const& commandBuffer,
		vk::Image                      image,
		vk::ImageLayout                oldLayout,
		vk::ImageLayout                newLayout,
		vk::AccessFlags2               srcAccessMask,
		vk::AccessFlags2               dstAccessMask,
		vk::PipelineStageFlags2        srcStageMask,
		vk::PipelineStageFlags2        dstStageMask
	)
	{
		vk::ImageMemoryBarrier2 barrier =
		{
			.srcStageMask = srcStageMask,
			.srcAccessMask = srcAccessMask,
			.dstStageMask = dstStageMask,
			.dstAccessMask = dstAccessMask,
			.oldLayout = oldLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange =
			{
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		vk::DependencyInfo dependencyInfo =
		{
			.dependencyFlags = {},
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier
		};

		commandBuffer.pipelineBarrier2(dependencyInfo);
	}
}

RenderContext::RenderContext(VulkanContext const& context, RenderContextConfig renderConfig)
	: context(context)
	, config(std::move(renderConfig))
	, swapchain(context, config.getDrawableExtent())
	, commandPool(createCommandPool(context))
{
	assert(config.framesInFlight > 0);

	frames.reserve(config.framesInFlight);
	for (uint32_t i = 0; i < config.framesInFlight; i++)
	{
		frames.push_back(FrameSync
		{
			.commandBuffer  = allocateCommandBuffer(context, commandPool),
			.imageAvailable = vk::raii::Semaphore(context.getDevice(), vk::SemaphoreCreateInfo{}),
			.inFlight       = vk::raii::Fence(context.getDevice(), vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled })
		});
	}

	createRenderFinishedSemaphores();
}

std::optional<Frame> RenderContext::beginFrame()
{
	assert(!frameInProgress && "endFrame was not called for the previous frame");

	if (swapchainDirty && !recreateSwapchain())
	{
		return std::nullopt;
	}

	FrameSync& frame = frames[frameIndex];

	auto fenceResult = context.getDevice().waitForFences(*frame.inFlight, vk::True, UINT64_MAX);
	if (fenceResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("failed to wait for fence!");
	}

	auto [result, imageIndex] = swapchain.getHandle().acquireNextImage(UINT64_MAX, *frame.imageAvailable, nullptr);

	if (result == vk::Result::eErrorOutOfDateKHR)
	{
		// Nothing was signaled and the fence is untouched, so the frame slot can simply be retried.
		swapchainDirty = true;
		return std::nullopt;
	}
	if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
	{
		assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	// Only reset the fence once we know work will be submitted that signals it again.
	context.getDevice().resetFences(*frame.inFlight);

	frame.commandBuffer.reset();
	frame.commandBuffer.begin({});

	vk::Image image = swapchain.getImages()[imageIndex];

	transitionImageLayout(
		frame.commandBuffer,
		image,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		{},                                                 // srcAccessMask (no need to wait for previous operations)
		vk::AccessFlagBits2::eColorAttachmentWrite,         // dstAccessMask
		vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
		vk::PipelineStageFlagBits2::eColorAttachmentOutput  // dstStage
	);

	frameInProgress = true;

	return Frame
	{
		.commandBuffer = frame.commandBuffer,
		.frameIndex    = frameIndex,
		.imageIndex    = imageIndex,
		.image         = image,
		.imageView     = swapchain.getImageViews()[imageIndex],
		.extent        = swapchain.getExtent()
	};
}

void RenderContext::endFrame(Frame const& frame)
{
	assert(frameInProgress && "endFrame called without a matching beginFrame");
	assert(frame.frameIndex == frameIndex);

	FrameSync& sync = frames[frameIndex];

	transitionImageLayout(
		sync.commandBuffer,
		frame.image,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::ePresentSrcKHR,
		vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
		{},                                                 // dstAccessMask
		vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
		vk::PipelineStageFlagBits2::eBottomOfPipe           // dstStage
	);

	sync.commandBuffer.end();

	vk::Semaphore const renderFinished = *renderFinishedSemaphores[frame.imageIndex];
	vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);

	const vk::SubmitInfo submitInfo
	{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*sync.imageAvailable,
		.pWaitDstStageMask = &waitDestinationStageMask,
		.commandBufferCount = 1,
		.pCommandBuffers = &*sync.commandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &renderFinished
	};

	context.getGraphicsQueue().submit(submitInfo, *sync.inFlight);

	const vk::PresentInfoKHR presentInfo
	{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &renderFinished,
		.swapchainCount = 1,
		.pSwapchains = &*swapchain.getHandle(),
		.pImageIndices = &frame.imageIndex
	};

	vk::Result result = context.getGraphicsQueue().presentKHR(presentInfo);
	if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
	{
		swapchainDirty = true;
	}
	else
	{
		// There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
		assert(result == vk::Result::eSuccess);
	}

	frameInProgress = false;
	frameIndex = (frameIndex + 1) % config.framesInFlight;
}

bool RenderContext::recreateSwapchain()
{
	const vk::Extent2D extent = config.getDrawableExtent();
	if (extent.width == 0 || extent.height == 0)
	{
		return false;
	}

	context.getDevice().waitIdle();

	swapchain.recreate(extent);

	// The image count may have changed, so the per-image semaphores are rebuilt with it.
	createRenderFinishedSemaphores();

	swapchainDirty = false;

	if (onSwapchainRecreated)
	{
		onSwapchainRecreated(swapchain);
	}

	return true;
}

void RenderContext::createRenderFinishedSemaphores()
{
	renderFinishedSemaphores.clear();
	renderFinishedSemaphores.reserve(swapchain.getImages().size());

	for (size_t i = 0; i < swapchain.getImages().size(); i++)
	{
		renderFinishedSemaphores.emplace_back(context.getDevice(), vk::SemaphoreCreateInfo{});
	}
}
