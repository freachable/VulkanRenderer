#pragma once

#include "Vulkan.hpp"
#include "VulkanContext.hpp"

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

	vk::raii::SwapchainKHR const& getHandle() const { return swapChain; }
	std::vector<vk::Image> const& getImages() const { return swapChainImages; }
	std::vector<vk::raii::ImageView> const& getImageViews() const { return swapChainImageViews; }
	vk::SurfaceFormatKHR const& getSurfaceFormat() const { return swapChainSurfaceFormat; }
	vk::Extent2D const& getExtent() const { return swapChainExtent; }

private:
	void createSwapChain(vk::Extent2D framebufferExtent);
	void createImageViews();
	void cleanupSwapChain();

	static vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities, vk::Extent2D framebufferExtent);
	static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats);
	static vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes);
	static uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities);

	VulkanContext const& context;

	vk::raii::SwapchainKHR           swapChain = nullptr;
	std::vector<vk::Image>           swapChainImages;
	vk::SurfaceFormatKHR             swapChainSurfaceFormat;
	vk::Extent2D                     swapChainExtent;
	std::vector<vk::raii::ImageView> swapChainImageViews;
};
