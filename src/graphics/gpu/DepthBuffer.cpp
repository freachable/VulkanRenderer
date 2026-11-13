#include "DepthBuffer.hpp"

#include <array>
#include <stdexcept>

namespace
{
	vk::Format findDepthFormat(vk::raii::PhysicalDevice const& physicalDevice)
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

	bool hasStencilComponent(vk::Format format)
	{
		return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
	}

	Image createDepthImage(VulkanContext const& context, vk::Extent2D extent, vk::Format format)
	{
		vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eDepth;
		if (hasStencilComponent(format))
		{
			aspectMask |= vk::ImageAspectFlagBits::eStencil;
		}

		return Image(context, extent, format, vk::ImageUsageFlagBits::eDepthStencilAttachment, aspectMask);
	}
}

DepthBuffer::DepthBuffer(VulkanContext const& context, vk::Extent2D extent)
	: context(context)
	, image(createDepthImage(context, extent, findDepthFormat(context.getPhysicalDevice())))
{
}

void DepthBuffer::recreate(vk::Extent2D extent)
{
	image = createDepthImage(context, extent, image.getFormat());
}
