#include "Command.hpp"

#include <utility>

Command::Command(VulkanContext const& context, vk::raii::CommandPool const& commandPool)
	: context(context)
	, commandPool(commandPool)
{
}

vk::raii::CommandBuffer Command::beginSingleTimeCommands() const
{
	vk::CommandBufferAllocateInfo allocInfo
	{
		.commandPool        = commandPool,
		.level              = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	};

	vk::raii::CommandBuffer commandBuffer = std::move(vk::raii::CommandBuffers(context.getDevice(), allocInfo).front());

	vk::CommandBufferBeginInfo beginInfo
	{
		.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	};

	commandBuffer.begin(beginInfo);

	return commandBuffer;
}

void Command::endSingleTimeCommands(vk::raii::CommandBuffer&& commandBuffer) const
{
	commandBuffer.end();

	vk::SubmitInfo submitInfo
	{
		.commandBufferCount = 1,
		.pCommandBuffers    = &*commandBuffer
	};

	context.getGraphicsQueue().submit(submitInfo, nullptr);
	context.getGraphicsQueue().waitIdle();
}
