#pragma once

#include "graphics/vulkan/Vulkan.hpp"
#include "graphics/vulkan/VulkanContext.hpp"
#include "graphics/vulkan/Swapchain.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

struct RenderContextConfig
{
	uint32_t framesInFlight = 2;

	// Keeps graphics/ free of platform/, the same way VulkanContextConfig::createSurface does.
	// Called to size the swapchain. Returning {0, 0} skips frames until a later call returns a real
	// extent (e.g. while minimized, or once the window is closing).
	std::function<vk::Extent2D()> getDrawableExtent;
};

struct Frame
{
	vk::raii::CommandBuffer const& commandBuffer; // Already begun

	uint32_t      frameIndex;  // In [0, framesInFlight); index your own per-frame resources with it.
	uint32_t      imageIndex;  // Index of the acquired swapchain image.
	vk::Image     image;       // Already in eColorAttachmentOptimal.
	vk::ImageView imageView;
	vk::Extent2D  extent;
};

// Owns the swapchain and everything needed to cycle frames through it: the per-frame command
// buffers and sync objects, the present semaphores and swapchain recreation.
class RenderContext
{
public:
	RenderContext(VulkanContext const& context, RenderContextConfig renderConfig);

	// Holds a reference to the context, and Frame references into it, so it stays put.
	RenderContext(RenderContext const&) = delete;
	RenderContext& operator=(RenderContext const&) = delete;
	RenderContext(RenderContext&&) = delete;
	RenderContext& operator=(RenderContext&&) = delete;

	// Waits until this frame slot is free, acquires a swapchain image, begins the command buffer
	// and transitions the image to eColorAttachmentOptimal. Rendering itself is left to the caller.
	// Returns nullopt when the frame has to be skipped (swapchain out of date, or no drawable extent).
	// Every frame returned must be passed to endFrame, or the next wait on its fence never returns.
	std::optional<Frame> beginFrame();

	// Transitions the image to ePresentSrcKHR, ends and submits the command buffer, and presents.
	void endFrame(Frame const& frame);

	// Recreates the swapchain before the next frame, e.g. after the window was resized.
	void requestSwapchainRecreate() { swapchainDirty = true; }

	// Called after every swapchain recreation, e.g. to resize render targets that match the swapchain.
	void setOnSwapchainRecreated(std::function<void(Swapchain const&)> callback) { onSwapchainRecreated = std::move(callback); }

	Swapchain const& getSwapchain() const { return swapchain; }
	uint32_t getFramesInFlight() const { return config.framesInFlight; }

private:
	struct FrameSync
	{
		vk::raii::CommandBuffer commandBuffer  = nullptr;
		vk::raii::Semaphore     imageAvailable = nullptr; // Signaled when the acquired swapchain image is ready.
		vk::raii::Fence         inFlight       = nullptr; // Signaled when the GPU has finished this frame; created signaled.
	};

	// Returns false if there is no drawable extent; the swapchain then stays marked for recreation.
	bool recreateSwapchain();
	void createRenderFinishedSemaphores();

	VulkanContext const& context;
	RenderContextConfig  config;

	Swapchain             swapchain;
	vk::raii::CommandPool commandPool;

	// Declared after commandPool, so the command buffers are freed before their pool.
	std::vector<FrameSync> frames;

	// One per swapchain image, not per frame: the semaphore may only be reused once its present is done.
	std::vector<vk::raii::Semaphore> renderFinishedSemaphores;

	std::function<void(Swapchain const&)> onSwapchainRecreated;

	uint32_t frameIndex      = 0;
	bool     swapchainDirty  = false;
	bool     frameInProgress = false;
};
