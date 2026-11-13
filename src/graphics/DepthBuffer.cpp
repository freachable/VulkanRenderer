#include "DepthBuffer.hpp"

#include "Image.hpp"

#include <array>
#include <stdexcept>
#include <tuple>

DepthBuffer::DepthBuffer(VulkanContext const& context, vk::Extent2D extent)
    : context(context)
    , format(findDepthFormat(context.getPhysicalDevice()))
    , aspectMask(hasStencilComponent(format)
                     ? vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil
                     : vk::ImageAspectFlagBits::eDepth)
{
	create(extent);
}

void DepthBuffer::recreate(vk::Extent2D extent)
{
	destroy();
	create(extent);
}

void DepthBuffer::create(vk::Extent2D extent)
{
	std::tie(depthImage, depthImageMemory) = ImageUtils::createImage(
	    context,
	    extent.width,
	    extent.height,
	    format,
	    vk::ImageTiling::eOptimal,
	    vk::ImageUsageFlagBits::eDepthStencilAttachment,
	    vk::MemoryPropertyFlagBits::eDeviceLocal
	);

	depthImageView = ImageUtils::createImageView(context, *depthImage, format, aspectMask);
}

void DepthBuffer::destroy()
{
	// Same order as the implicit destruction: view, then image, then memory.
	depthImageView   = nullptr;
	depthImage       = nullptr;
	depthImageMemory = nullptr;
}

vk::Format DepthBuffer::findDepthFormat(vk::raii::PhysicalDevice const& physicalDevice)
{
	// Preferred first: pure 32-bit float depth, then formats that carry a stencil too.
	constexpr std::array candidates = {
	    vk::Format::eD32Sfloat,
	    vk::Format::eD32SfloatS8Uint,
	    vk::Format::eD24UnormS8Uint,
	};

	for (vk::Format candidate : candidates)
	{
		vk::FormatProperties properties = physicalDevice.getFormatProperties(candidate);
		if (properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
		{
			return candidate;
		}
	}

	throw std::runtime_error("failed to find a supported depth format!");
}

bool DepthBuffer::hasStencilComponent(vk::Format format)
{
	return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}
