#pragma once

#include "graphics/gpu/Shader.hpp"
#include "graphics/vulkan/Vulkan.hpp"
#include "graphics/vulkan/VulkanContext.hpp"

#include <span>

// What differs between pipelines. The spans are only read while the pipeline is created.
struct GraphicsPipelineConfig
{
	vk::Format colorFormat = vk::Format::eUndefined;
	vk::Format depthFormat = vk::Format::eUndefined;

	std::span<vk::DescriptorSetLayout const>             descriptorSetLayouts;
	std::span<vk::VertexInputBindingDescription const>   vertexBindings;
	std::span<vk::VertexInputAttributeDescription const> vertexAttributes;

	vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;
};

// A pipeline for dynamic rendering into one color attachment (plus an optional depth attachment).
// Viewport and scissor are dynamic: set them while recording.
class GraphicsPipeline
{
public:
	GraphicsPipeline(VulkanContext const& context, Shader const& shader, GraphicsPipelineConfig const& config);

	GraphicsPipeline(GraphicsPipeline const&) = delete;
	GraphicsPipeline& operator=(GraphicsPipeline const&) = delete;
	GraphicsPipeline(GraphicsPipeline&&) = delete;
	GraphicsPipeline& operator=(GraphicsPipeline&&) = delete;

	vk::raii::PipelineLayout const& getLayout() const { return pipelineLayout; }
	vk::raii::Pipeline const& getHandle() const { return pipeline; }

private:
	vk::raii::PipelineLayout pipelineLayout = nullptr;
	vk::raii::Pipeline       pipeline       = nullptr;
};
