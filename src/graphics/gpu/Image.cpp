#include "Image.hpp"

#include <utility>

Image::Image(VulkanContext const& context, vk::Extent2D extent, vk::Format format, vk::ImageUsageFlags usage,
             vk::ImageAspectFlags aspectMask)
	: format(format)
	, extent(extent)
	, aspectMask(aspectMask)
{
	vk::ImageCreateInfo imageInfo
	{
		.imageType   = vk::ImageType::e2D,
		.format      = format,
		.extent      = { extent.width, extent.height, 1 },
		.mipLevels   = 1,
		.arrayLayers = 1,
		.samples     = vk::SampleCountFlagBits::e1,
		.tiling      = vk::ImageTiling::eOptimal,
		.usage       = usage,
		.sharingMode = vk::SharingMode::eExclusive
	};

	image = vk::raii::Image(context.getDevice(), imageInfo);

	vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
	vk::MemoryAllocateInfo allocInfo
	{
		.allocationSize  = memRequirements.size,
		.memoryTypeIndex = context.findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
	};

	memory = vk::raii::DeviceMemory(context.getDevice(), allocInfo);
	image.bindMemory(memory, 0);

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

	view = vk::raii::ImageView(context.getDevice(), viewInfo);
}

Image& Image::operator=(Image&& other) noexcept
{
	if (this != &other)
	{
		// Release the old view before its image, and the image before its memory.
		view   = std::move(other.view);
		image  = std::move(other.image);
		memory = std::move(other.memory);

		format     = other.format;
		extent     = other.extent;
		aspectMask = other.aspectMask;
	}
	return *this;
}
