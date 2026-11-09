#pragma once

#include "Vulkan.hpp"
#include "VulkanContext.hpp"

class Command
{
public:
	Command(VulkanContext const& context, vk::raii::CommandPool const& commandPool);

	vk::raii::CommandBuffer beginSingleTimeCommands() const;
	void endSingleTimeCommands(vk::raii::CommandBuffer&& commandBuffer) const;

private:
	VulkanContext const&         context;
	vk::raii::CommandPool const& commandPool;
};
