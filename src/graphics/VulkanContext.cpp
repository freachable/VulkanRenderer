#include "VulkanContext.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	constexpr char const* validationLayerName = "VK_LAYER_KHRONOS_validation";

	void requireLayersSupported(vk::raii::Context const& context, std::span<char const* const> requiredLayers)
	{
		auto layerProperties = context.enumerateInstanceLayerProperties();
		auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
			[&layerProperties](auto const& requiredLayer) {
				return std::ranges::none_of(layerProperties, [requiredLayer](auto const& layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
			}
		);

		if (unsupportedLayerIt != requiredLayers.end())
		{
			throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
		}
	}

	void requireExtensionsSupported(vk::raii::Context const& context, std::span<char const* const> requiredExtensions)
	{
		auto extensionProperties = context.enumerateInstanceExtensionProperties();
		auto unsupportedExtensionIt = std::ranges::find_if(requiredExtensions,
			[&extensionProperties](auto const& requiredExtension) {
				return std::ranges::none_of(extensionProperties, [requiredExtension](auto const& extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
			}
		);

		if (unsupportedExtensionIt != requiredExtensions.end())
		{
			throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedExtensionIt));
		}
	}

	VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*)
	{
		if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError || severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
		{
			std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
		}

		return vk::False;
	}
}

VulkanContext::VulkanContext(VulkanContextConfig const& config)
	: validationEnabled(config.enableValidation)
	, deviceExtensions(config.requiredDeviceExtensions.begin(), config.requiredDeviceExtensions.end())
{
	createInstance(config);
	setupDebugMessenger();
	createSurface(config);
	pickPhysicalDevice();
	createLogicalDevice();
}

void VulkanContext::createSurface(VulkanContextConfig const& config)
{
	if (!config.createSurface)
	{
		throw std::runtime_error("VulkanContextConfig::createSurface is not set!");
	}

	VkSurfaceKHR rawSurface = config.createSurface(*instance);
	if (rawSurface == VK_NULL_HANDLE)
	{
		throw std::runtime_error("failed to create window surface!");
	}

	surface = vk::raii::SurfaceKHR(instance, rawSurface);
}

void VulkanContext::createInstance(VulkanContextConfig const& config)
{
	vk::ApplicationInfo const appInfo
	{
		.pApplicationName   = config.appName,
		.applicationVersion = config.appVersion,
		.pEngineName        = "No Engine",
		.engineVersion      = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion         = config.apiVersion
	};

	std::vector<char const*> requiredLayers;
	std::vector<char const*> requiredExtensions(config.requiredInstanceExtensions.begin(), config.requiredInstanceExtensions.end());
	if (validationEnabled)
	{
		requiredLayers.push_back(validationLayerName);
		requiredExtensions.push_back(vk::EXTDebugUtilsExtensionName);
	}

	requireLayersSupported(context, requiredLayers);
	requireExtensionsSupported(context, requiredExtensions);

	vk::InstanceCreateInfo const createInfo
	{
		.pApplicationInfo        = &appInfo,
		.enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
		.ppEnabledLayerNames     = requiredLayers.data(),
		.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
		.ppEnabledExtensionNames = requiredExtensions.data()
	};

	instance = vk::raii::Instance(context, createInfo);
}

void VulkanContext::setupDebugMessenger()
{
	if (!validationEnabled)
	{
		return;
	}

	vk::DebugUtilsMessageSeverityFlagsEXT const severityFlags
	(
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
	);

	vk::DebugUtilsMessageTypeFlagsEXT const messageTypeFlags
	(
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
	);

	vk::DebugUtilsMessengerCreateInfoEXT const debugUtilsMessengerCreateInfoEXT
	{
		.messageSeverity = severityFlags,
		.messageType     = messageTypeFlags,
		.pfnUserCallback = &debugCallback
	};

	debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

void VulkanContext::pickPhysicalDevice()
{
	std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();

	auto const suitableDevice = std::ranges::find_if(physicalDevices, [this](auto const& candidate) { return isDeviceSuitable(candidate); });
	if (suitableDevice == physicalDevices.end())
	{
		throw std::runtime_error("failed to find a suitable GPU!");
	}

	physicalDevice = *suitableDevice;
}

bool VulkanContext::isDeviceSuitable(vk::raii::PhysicalDevice const& candidate) const
{
	// Check if the device supports the Vulkan 1.3 API version
	bool supportsVulkan1_3 = candidate.getProperties().apiVersion >= vk::ApiVersion13;

	// Check if any of the queue families support graphics operations
	auto queueFamilies = candidate.getQueueFamilyProperties();
	bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const& qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

	// Check if all required device extensions are available
	auto availableDeviceExtensions = candidate.enumerateDeviceExtensionProperties();
	bool supportsAllRequiredExtensions =
		std::ranges::all_of(deviceExtensions,
			[&availableDeviceExtensions](auto const& requiredDeviceExtension) {
				return std::ranges::any_of(availableDeviceExtensions,
					[requiredDeviceExtension](auto const& availableDeviceExtension) { return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
			});

	// Check if the device supports the required features
	auto features = candidate.template getFeatures2<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan11Features,
		vk::PhysicalDeviceVulkan13Features,
		vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
	>();

	bool supportsRequiredFeatures =
		features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
		features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
		features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

	return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
}

void VulkanContext::createLogicalDevice()
{
	// find the index of the first queue family that supports graphics
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

	// get the first index into queueFamilyProperties which supports both graphics and present
	uint32_t queueIndex = ~0;
	for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
	{
		if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
		{
			// found a queue family that supports both graphics and present
			queueIndex = qfpIndex;
			break;
		}
	}

	if (queueIndex == ~0)
	{
		throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
	}

	// query for Vulkan 1.3 features
	vk::StructureChain<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan11Features,
		vk::PhysicalDeviceVulkan13Features,
		vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
	> featureChain = {
		{},                                                       // vk::PhysicalDeviceFeatures2
		{ .shaderDrawParameters = true },                         // vk::PhysicalDeviceVulkan11Features
		{ .synchronization2 = true, .dynamicRendering = true },   // vk::PhysicalDeviceVulkan13Features
		{ .extendedDynamicState = true }                          // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
	};

	// create a Device
	float queuePriority = 0.5f;

	vk::DeviceQueueCreateInfo deviceQueueCreateInfo
	{
		.queueFamilyIndex = queueIndex,
		.queueCount       = 1,
		.pQueuePriorities = &queuePriority
	};

	vk::DeviceCreateInfo deviceCreateInfo
	{
		.pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
		.queueCreateInfoCount    = 1,
		.pQueueCreateInfos       = &deviceQueueCreateInfo,
		.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()),
		.ppEnabledExtensionNames = deviceExtensions.data()
	};

	device = vk::raii::Device(physicalDevice, deviceCreateInfo);
	graphicsQueue = vk::raii::Queue(device, queueIndex, 0);
	graphicsQueueFamilyIndex = queueIndex;
}
