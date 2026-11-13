#pragma once

#include "graphics/vulkan/Vulkan.hpp"

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

struct VulkanContextConfig
{
	char const* appName    = "Vulkan Renderer";
	uint32_t    appVersion = VK_MAKE_VERSION(1, 0, 0);
	uint32_t    apiVersion = vk::ApiVersion14;

	bool enableValidation = false;

	std::span<char const* const> requiredInstanceExtensions;
	std::span<char const* const> requiredDeviceExtensions;

	// This class should not depend on GLFW or other platform-specific code
	std::function<VkSurfaceKHR(VkInstance)> createSurface;
};


class VulkanContext
{
public:
	explicit VulkanContext(VulkanContextConfig const& config);

	// Other objects will hold references to the context, so it stays put.
	VulkanContext(VulkanContext const&) = delete;
	VulkanContext& operator=(VulkanContext const&) = delete;
	VulkanContext(VulkanContext&&) = delete;
	VulkanContext& operator=(VulkanContext&&) = delete;

	// RAII pointers
	vk::raii::Context const&  getContext() const { return context; }
	vk::raii::Instance const& getInstance() const { return instance; }
	vk::raii::SurfaceKHR const& getSurface() const { return surface; }
	vk::raii::PhysicalDevice const& getPhysicalDevice() const { return physicalDevice; }
	vk::raii::Device const& getDevice() const { return device; }
	vk::raii::Queue const& getGraphicsQueue() const { return graphicsQueue; }

	// Queue family of the graphics queue; it also supports presenting to the surface.
	uint32_t getGraphicsQueueFamilyIndex() const { return graphicsQueueFamilyIndex; }
	std::span<char const* const> getDeviceExtensions() const { return deviceExtensions; }
	bool isValidationEnabled() const { return validationEnabled; }

	// Index of a memory type allowed by typeFilter (a memoryTypeBits mask) that has all the given properties.
	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

	// Records commands with `record`, submits them to the graphics queue and waits until they are done.
	// Meant for one-off work such as uploads; far too slow to use every frame. Not thread-safe.
	void immediateSubmit(std::function<void(vk::raii::CommandBuffer const&)> const& record) const;

private:
	void createInstance(VulkanContextConfig const& config);
	void setupDebugMessenger();
	void createSurface(VulkanContextConfig const& config);
	void pickPhysicalDevice();
	bool isDeviceSuitable(vk::raii::PhysicalDevice const& candidate) const;
	void createLogicalDevice();
	void createImmediateCommandPool();

	bool validationEnabled = false;
	std::vector<char const*> deviceExtensions;
	uint32_t graphicsQueueFamilyIndex = 0;

	vk::raii::Context                context;
	vk::raii::Instance               instance       = nullptr;
	vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
	vk::raii::SurfaceKHR             surface        = nullptr;
	vk::raii::PhysicalDevice         physicalDevice = nullptr;
	vk::raii::Device                 device         = nullptr;
	vk::raii::Queue                  graphicsQueue  = nullptr;

	// Only for immediateSubmit; per-frame command buffers live in the RenderContext.
	vk::raii::CommandPool            immediateCommandPool = nullptr;
};
