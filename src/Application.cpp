#include "Application.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace
{
	constexpr uint32_t WIDTH = 800;
	constexpr uint32_t HEIGHT = 600;
	constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

	// The helpers below build one member each for the constructor's initializer list.
	// They return by value; for the non-movable types this relies on C++17 guaranteed copy elision.

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
			.createSurface              = [&window](VkInstance instance) { return window.createSurface(instance); }
		});
	}

	RenderContext createRenderContext(VulkanContext const& context, Window& window)
	{
		return RenderContext(context, RenderContextConfig
		{
			.framesInFlight    = MAX_FRAMES_IN_FLIGHT,
			// Blocks while minimized; returns {0, 0} once the window is closing.
			.getDrawableExtent = [&window] { return window.waitForDrawableExtent(); }
		});
	}
}

Application::Application()
	: window({ .width = WIDTH, .height = HEIGHT, .title = "Vulkan" })
	, vulkanContext(createVulkanContext(window))
	, renderContext(createRenderContext(vulkanContext, window))
	, renderer(vulkanContext, renderContext)
{
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
