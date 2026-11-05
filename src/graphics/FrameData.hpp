#pragma once

#include "Vulkan.hpp"
#include "VulkanContext.hpp"
#include "Buffer.hpp"

// Everything one frame in flight owns. MAX_FRAMES_IN_FLIGHT of these are cycled.
//
// The command buffer and descriptor set come from pools owned elsewhere, so a FrameData
// must be destroyed before those pools.
struct FrameData
{
	// The descriptor set layout must have a uniform buffer at binding 0; it is pointed at uniformBuffer.
	FrameData(VulkanContext const& context,
	          vk::raii::CommandPool const& commandPool,
	          vk::raii::DescriptorPool const& descriptorPool,
	          vk::raii::DescriptorSetLayout const& descriptorSetLayout,
	          vk::DeviceSize uniformBufferSize);

	FrameData(FrameData&&) noexcept = default;
	FrameData& operator=(FrameData&&) noexcept = default;
	FrameData(FrameData const&) = delete;
	FrameData& operator=(FrameData const&) = delete;

	vk::raii::CommandBuffer commandBuffer  = nullptr;
	vk::raii::Semaphore     imageAvailable = nullptr; // Signaled when the acquired swapchain image is ready.
	vk::raii::Fence         inFlight       = nullptr; // Signaled when the GPU has finished this frame; created signaled.
	Buffer                  uniformBuffer;
	vk::raii::DescriptorSet descriptorSet  = nullptr;
};
