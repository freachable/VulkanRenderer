#include "Swapchain.hpp"

#include <algorithm>
#include <cassert>
#include <limits>

Swapchain::Swapchain(VulkanContext const& context, vk::Extent2D framebufferExtent)
	: context(context)
{
	createSwapchain(framebufferExtent);
	createImageViews();
}

void Swapchain::recreate(vk::Extent2D framebufferExtent)
{
	destroySwapchain();

	createSwapchain(framebufferExtent);
	createImageViews();
}

void Swapchain::destroySwapchain()
{
	imageViews.clear();
	swapchain = nullptr;
}

vk::Extent2D Swapchain::chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities, vk::Extent2D framebufferExtent)
{
	// currentExtent is only set to the special "undefined" value described above
	// when the window manager lets us choose the extent ourselves; any other value
	// means the surface already dictates a fixed extent that we must use as-is.
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}

	return {
		std::clamp<uint32_t>(framebufferExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
		std::clamp<uint32_t>(framebufferExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
	};
}

vk::SurfaceFormatKHR Swapchain::chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats)
{
	const auto formatIt = std::ranges::find_if(
		availableFormats,
		[](const auto& format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
	return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

vk::PresentModeKHR Swapchain::chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes)
{
	assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
	return std::ranges::any_of(availablePresentModes,
		[](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
		vk::PresentModeKHR::eMailbox :
		vk::PresentModeKHR::eFifo;
}

uint32_t Swapchain::chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities)
{
	auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
	if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
	{
		minImageCount = surfaceCapabilities.maxImageCount;
	}
	return minImageCount;
}

void Swapchain::createSwapchain(vk::Extent2D framebufferExtent)
{
	vk::SurfaceCapabilitiesKHR surfaceCapabilities = context.getPhysicalDevice().getSurfaceCapabilitiesKHR(*context.getSurface());
	extent = chooseSwapExtent(surfaceCapabilities, framebufferExtent);
	uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

	std::vector<vk::SurfaceFormatKHR> availableFormats = context.getPhysicalDevice().getSurfaceFormatsKHR(*context.getSurface());
	surfaceFormat = chooseSwapSurfaceFormat(availableFormats);

	std::vector<vk::PresentModeKHR> availablePresentModes = context.getPhysicalDevice().getSurfacePresentModesKHR(*context.getSurface());

	vk::SwapchainCreateInfoKHR createInfo{
		.surface = *context.getSurface(),
		.minImageCount = minImageCount,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.preTransform = surfaceCapabilities.currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = chooseSwapPresentMode(availablePresentModes),
		.clipped = true
	};

	swapchain = vk::raii::SwapchainKHR(context.getDevice(), createInfo);
	images = swapchain.getImages();
}

void Swapchain::createImageViews()
{
	assert(imageViews.empty());

	vk::ImageViewCreateInfo imageViewCreateInfo
	{
		.viewType = vk::ImageViewType::e2D,
		.format = surfaceFormat.format,
		.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
	};

	for (auto& image : images)
	{
		imageViewCreateInfo.image = image;
		imageViews.emplace_back(context.getDevice(), imageViewCreateInfo);
	}
}
