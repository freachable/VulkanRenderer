#include "Application.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace
{
	constexpr uint32_t WIDTH = 800;
	constexpr uint32_t HEIGHT = 600;
	constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

	VulkanContext createVulkanContext(Window const& window)
	{
		constexpr std::array<char const*, 1> requiredDeviceExtensions =
		{
			vk::KHRSwapchainExtensionName
		};

		return VulkanContext(VulkanContextConfig
		{
			.enableValidation           = Application::enableValidationLayers,
			.requiredInstanceExtensions = window.getRequiredInstanceExtensions(),
			.requiredDeviceExtensions   = requiredDeviceExtensions,
			.createSurface              = [&window](VkInstance instance) { 
				return window.createSurface(instance); 
			}
		});
	}

	RenderContext createRenderContext(VulkanContext const& context, Window& window)
	{
		return RenderContext(context, RenderContextConfig
		{
			.framesInFlight    = MAX_FRAMES_IN_FLIGHT,
			.getDrawableExtent = [&window] { 
				return window.waitForDrawableExtent(); 
			}
		});
	}
}

Application::Application()
	: window({ .width = WIDTH, .height = HEIGHT, .title = "Vulkan" })
	, vulkanContext(createVulkanContext(window))
	, renderContext(createRenderContext(vulkanContext, window))
	, renderer(vulkanContext, renderContext)
{
	// The renderer only gets a const RenderContext, so the callback is wired up here.
	renderContext.setOnSwapchainRecreated([this](Swapchain const& swapchain)
	{
		renderer.onSwapchainRecreated(swapchain);
	});
}

void Application::run()
{
	while (!window.shouldClose())
	{
		window.pollEvents();
		drawFrame();
	}

	vulkanContext.getDevice().waitIdle();
}

void Application::drawFrame()
{
	if (window.consumeResized())
	{
		renderContext.requestSwapchainRecreate();
	}

	std::optional<Frame> frame = renderContext.beginFrame();
	if (!frame)
	{
		return;
	}

	renderer.draw(*frame);

	renderContext.endFrame(*frame);
}
