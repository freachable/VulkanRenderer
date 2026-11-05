#pragma once

#include "graphics/Vulkan.hpp"
#include "graphics/VulkanContext.hpp"
#include "graphics/Swapchain.hpp"
#include "graphics/GraphicsPipeline.hpp"
#include "graphics/Buffer.hpp"
#include "graphics/FrameData.hpp"
#include "platform/Window.hpp"

#include <cstdint>
#include <vector>

class Application
{
public:
#ifdef NDEBUG
	static constexpr bool enableValidationLayers = false;
#else
	static constexpr bool enableValidationLayers = true;
#endif

	Application();

	// Members hold references to each other (e.g. swapChain -> vulkanContext), so the object stays put.
	Application(Application const&) = delete;
	Application& operator=(Application const&) = delete;
	Application(Application&&) = delete;
	Application& operator=(Application&&) = delete;

	void run();

private:
	void drawFrame();
	void recreateSwapChain();
	void updateUniformBuffer(FrameData& frame);
	void recordCommandBuffer(FrameData& frame, uint32_t imageIndex);
	void transition_image_layout(
		vk::raii::CommandBuffer& commandBuffer,
		uint32_t                imageIndex,
		vk::ImageLayout         old_layout,
		vk::ImageLayout         new_layout,
		vk::AccessFlags2        src_access_mask,
		vk::AccessFlags2        dst_access_mask,
		vk::PipelineStageFlags2 src_stage_mask,
		vk::PipelineStageFlags2 dst_stage_mask
	);

	// Declaration order is construction order (dependencies first) and the reverse is
	// destruction order. The constructor's initializer list relies on it: keep it that way.

	// Created first, destroyed last, so it outlives the surface in vulkanContext.
	Window        window;
	VulkanContext vulkanContext;

	Swapchain        swapChain;
	GraphicsPipeline graphicsPipeline;

	vk::raii::CommandPool commandPool;

	Buffer vertexBuffer;
	Buffer indexBuffer;

	vk::raii::DescriptorPool descriptorPool;

	// Declared after commandPool and descriptorPool, so it is destroyed before them.
	std::vector<FrameData> frames;
	uint32_t frameIndex = 0;

	// One per swapchain image, not per frame: the semaphore may only be reused once its present is done.
	std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
};
