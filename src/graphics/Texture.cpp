#include "Texture.hpp"

#include "Buffer.hpp"
#include "Image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stdexcept>
#include <tuple>
#include "Command.hpp"

Texture::Texture(VulkanContext const& context, vk::raii::CommandPool const& commandPool, std::string const& path)
{
	createTextureImage(context, commandPool, path);
	createTextureImageView(context);
}

void Texture::createTextureImage(VulkanContext const& context, vk::raii::CommandPool const& commandPool, std::string const& path)
{
	int            texWidth, texHeight, texChannels;
	stbi_uc       *pixels    = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	vk::DeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels)
	{
		throw std::runtime_error("failed to load texture image!");
	}

	Buffer stagingBuffer = Buffer::forStaging(context, pixels, imageSize);

	stbi_image_free(pixels);

	std::tie(textureImage, textureImageMemory) = ImageUtils::createImage(
		context,
	    texWidth,
	    texHeight,
	    vk::Format::eR8G8B8A8Srgb,
	    vk::ImageTiling::eOptimal,
	    vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
	    vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	Command command(context, commandPool);
	vk::raii::CommandBuffer commandBuffer = command.beginSingleTimeCommands();

	transitionImageLayout(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
	copyBufferToImage(commandBuffer, stagingBuffer, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	transitionImageLayout(commandBuffer, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

	command.endSingleTimeCommands(std::move(commandBuffer));
}

void Texture::transitionImageLayout(vk::raii::CommandBuffer& commandBuffer, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
	vk::ImageMemoryBarrier barrier
	{
		.oldLayout           = oldLayout,
		.newLayout           = newLayout,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image               = textureImage,
		.subresourceRange    = 
		{ 
			.aspectMask = vk::ImageAspectFlagBits::eColor, 
			.levelCount = 1, 
			.layerCount = 1 
		}
	};

	vk::PipelineStageFlags sourceStage;
	vk::PipelineStageFlags destinationStage;

	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		barrier.srcAccessMask = {};
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
		destinationStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		sourceStage      = vk::PipelineStageFlagBits::eTransfer;
		destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else
	{
		throw std::invalid_argument("unsupported layout transition!");
	}

	commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, barrier);
}

void Texture::copyBufferToImage(vk::raii::CommandBuffer& commandBuffer, Buffer const& buffer, uint32_t width, uint32_t height)
{
	vk::BufferImageCopy region
	{
		.bufferOffset      = 0,
		.bufferRowLength   = 0,
		.bufferImageHeight = 0,
		.imageSubresource  = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
		.imageOffset       = { 0, 0, 0 },
		.imageExtent       = { width, height, 1 }
	};

	commandBuffer.copyBufferToImage(buffer.getHandle(), textureImage, vk::ImageLayout::eTransferDstOptimal, { region });
}

void Texture::createTextureImageView(VulkanContext const& context)
{
	textureImageView = ImageUtils::createImageView(context, *textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
}
