#pragma once

#include "graphics/gpu/Buffer.hpp"
#include "graphics/vulkan/Vulkan.hpp"
#include "graphics/vulkan/VulkanContext.hpp"

#include <cstdint>

#include <glm/glm.hpp>

struct UniformBufferObject
{
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
	alignas(16) glm::vec3 cameraPos; // World space.
};

// The renderer's resources for one frame in flight: one per RenderContext frame slot,
// indexed by Frame::frameIndex. The command buffer and sync objects live in RenderContext.
//
// The descriptor set comes from a pool owned elsewhere, so a FrameResources must be destroyed before that pool.
struct FrameResources
{
	static vk::raii::DescriptorSetLayout createDescriptorSetLayout(VulkanContext const& context);

	// Big enough for frameCount FrameResources.
	static vk::raii::DescriptorPool createDescriptorPool(VulkanContext const& context, uint32_t frameCount);

	FrameResources(VulkanContext const& context,
	               vk::raii::DescriptorPool const& descriptorPool,
	               vk::raii::DescriptorSetLayout const& descriptorSetLayout,
	               vk::raii::ImageView const& textureView,
	               vk::raii::Sampler const& textureSampler);

	FrameResources(FrameResources&&) noexcept = default;
	FrameResources& operator=(FrameResources&&) noexcept = default;
	FrameResources(FrameResources const&) = delete;
	FrameResources& operator=(FrameResources const&) = delete;

	Buffer                  uniformBuffer; // Holds one UniformBufferObject; mapped for its whole lifetime.
	vk::raii::DescriptorSet descriptorSet = nullptr;
};
