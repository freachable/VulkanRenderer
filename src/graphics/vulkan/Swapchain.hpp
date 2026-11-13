#pragma once

#include "graphics/vulkan/Vulkan.hpp"
#include "graphics/vulkan/VulkanContext.hpp"

#include <cstdint>
#include <vector>

class Swapchain
{
public:
	Swapchain(VulkanContext const& context, vk::Extent2D framebufferExtent);

	Swapchain(Swapchain const&) = delete;
	Swapchain& operator=(Swapchain const&) = delete;
	Swapchain(Swapchain&&) = delete;
	Swapchain& operator=(Swapchain&&) = delete;

	// The caller must make sure the device is idle (no image still in use).
	void recreate(vk::Extent2D framebufferExtent);

	vk::raii::SwapchainKHR const& getHandle() const { return swapchain; }
	std::vector<vk::Image> const& getImages() const { return images; }
	std::vector<vk::raii::ImageView> const& getImageViews() const { return imageViews; }
	vk::SurfaceFormatKHR const& getSurfaceFormat() const { return surfaceFormat; }
	vk::Extent2D const& getExtent() const { return extent; }

private:
	void createSwapchain(vk::Extent2D framebufferExtent);
	void createImageViews();
	void destroySwapchain();

	static vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities, vk::Extent2D framebufferExtent);
	static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats);
	static vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes);
	static uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities);

	VulkanContext const& context;

	vk::raii::SwapchainKHR           swapchain = nullptr;
	std::vector<vk::Image>           images;
	vk::SurfaceFormatKHR             surfaceFormat;
	vk::Extent2D                     extent;
	std::vector<vk::raii::ImageView> imageViews;
};
