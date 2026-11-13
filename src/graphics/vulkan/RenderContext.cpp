#include "RenderContext.hpp"

#include "graphics/vulkan/Barriers.hpp"

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

	transitionImage(frame.commandBuffer, image, vk::ImageAspectFlagBits::eColor,
	{
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.srcStage  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.srcAccess = {}, // The old contents are discarded, so there are no writes to wait for.
		.dstStage  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.dstAccess = vk::AccessFlagBits2::eColorAttachmentWrite
	});

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

	transitionImage(sync.commandBuffer, frame.image, vk::ImageAspectFlagBits::eColor,
	{
		.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.newLayout = vk::ImageLayout::ePresentSrcKHR,
		.srcStage  = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		.srcAccess = vk::AccessFlagBits2::eColorAttachmentWrite,
		.dstStage  = vk::PipelineStageFlagBits2::eBottomOfPipe,
		.dstAccess = {}
	});

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
