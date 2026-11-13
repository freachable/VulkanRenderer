#include "Image.hpp"

#include "Buffer.hpp"

namespace ImageUtils
{
	std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(
	    VulkanContext const& context, uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling,
	    vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties)
	{
		vk::ImageCreateInfo imageInfo
		{
			.imageType   = vk::ImageType::e2D,
		    .format      = format,
		    .extent      = { width, height, 1 },
		    .mipLevels   = 1,
		    .arrayLayers = 1,
		    .samples     = vk::SampleCountFlagBits::e1,
		    .tiling      = tiling,
		    .usage       = usage,
		    .sharingMode = vk::SharingMode::eExclusive
		};

		vk::raii::Image image = vk::raii::Image(context.getDevice(), imageInfo);

		vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo
		{
			.allocationSize  = memRequirements.size,
		    .memoryTypeIndex = Buffer::findMemoryType(context, memRequirements.memoryTypeBits, properties)
		};

		vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(context.getDevice(), allocInfo);
		image.bindMemory(imageMemory, 0);

		return {std::move(image), std::move(imageMemory)};
	}

	vk::raii::ImageView createImageView(
	    VulkanContext const& context, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectMask)
	{
		vk::ImageViewCreateInfo viewInfo
		{
			.image            = image,
			.viewType         = vk::ImageViewType::e2D,
			.format           = format,
			.subresourceRange =
			{
				.aspectMask     = aspectMask,
				.baseMipLevel   = 0,
				.levelCount     = 1,
				.baseArrayLayer = 0,
				.layerCount     = 1
			}
		};

		return vk::raii::ImageView(context.getDevice(), viewInfo);
	}
}
