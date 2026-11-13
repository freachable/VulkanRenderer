#include "Texture.hpp"

#include "graphics/gpu/Buffer.hpp"
#include "graphics/vulkan/Barriers.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>

namespace
{
	Image createTextureImage(VulkanContext const& context, ImageData const& data)
	{
		if (data.width == 0 || data.height == 0 || data.pixels.size() != size_t{ data.width } * data.height * 4)
		{
			throw std::runtime_error("texture data must be non-empty, tightly packed RGBA8 pixels");
		}

		return Image(context, { data.width, data.height }, vk::Format::eR8G8B8A8Srgb,
		             vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		             vk::ImageAspectFlagBits::eColor);
	}
}

Texture::Texture(VulkanContext const& context, ImageData const& data)
	: image(createTextureImage(context, data))
{
	Buffer stagingBuffer = Buffer::forStaging(context, std::as_bytes(std::span(data.pixels)));

	context.immediateSubmit([&](vk::raii::CommandBuffer const& commandBuffer)
	{
		transitionImage(commandBuffer, *image.getHandle(), vk::ImageAspectFlagBits::eColor,
		{
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eTransferDstOptimal,
			.srcStage  = vk::PipelineStageFlagBits2::eNone,
			.srcAccess = {},
			.dstStage  = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccess = vk::AccessFlagBits2::eTransferWrite
		});

		vk::BufferImageCopy region
		{
			.bufferOffset      = 0,
			.bufferRowLength   = 0, // 0 = tightly packed
			.bufferImageHeight = 0,
			.imageSubresource  = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
			.imageOffset       = { 0, 0, 0 },
			.imageExtent       = { data.width, data.height, 1 }
		};

		commandBuffer.copyBufferToImage(stagingBuffer.getHandle(), image.getHandle(), vk::ImageLayout::eTransferDstOptimal, region);

		transitionImage(commandBuffer, *image.getHandle(), vk::ImageAspectFlagBits::eColor,
		{
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.srcStage  = vk::PipelineStageFlagBits2::eTransfer,
			.srcAccess = vk::AccessFlagBits2::eTransferWrite,
			.dstStage  = vk::PipelineStageFlagBits2::eFragmentShader,
			.dstAccess = vk::AccessFlagBits2::eShaderRead
		});
	});
}
