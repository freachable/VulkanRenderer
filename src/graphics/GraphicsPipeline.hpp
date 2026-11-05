#pragma once

#include "Vulkan.hpp"
#include "VulkanContext.hpp"
#include "Shader.hpp"

class GraphicsPipeline
{
public:
	GraphicsPipeline(VulkanContext const& context, Shader const& shader, vk::Format colorFormat);

	GraphicsPipeline(GraphicsPipeline const&) = delete;
	GraphicsPipeline& operator=(GraphicsPipeline const&) = delete;
	GraphicsPipeline(GraphicsPipeline&&) = delete;
	GraphicsPipeline& operator=(GraphicsPipeline&&) = delete;

	vk::raii::DescriptorSetLayout const& getDescriptorSetLayout() const { return descriptorSetLayout; }
	vk::raii::PipelineLayout const& getLayout() const { return pipelineLayout; }
	vk::raii::Pipeline const& getHandle() const { return pipeline; }

private:
	void createDescriptorSetLayout(VulkanContext const& context);

	vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
	vk::raii::PipelineLayout      pipelineLayout      = nullptr;
	vk::raii::Pipeline            pipeline            = nullptr;
};
