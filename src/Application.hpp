#pragma once

#include "graphics/vulkan/Vulkan.hpp"
#include "graphics/vulkan/VulkanContext.hpp"
#include "graphics/vulkan/RenderContext.hpp"
#include "graphics/renderer/Renderer.hpp"
#include "platform/Window.hpp"
#include "scene/Camera.hpp"

class Application
{
public:
#ifdef NDEBUG
	static constexpr bool enableValidationLayers = false;
#else
	static constexpr bool enableValidationLayers = true;
#endif

	Application();

	// Members hold references to each other (e.g. renderContext -> vulkanContext), so the object stays put.
	Application(Application const&) = delete;
	Application& operator=(Application const&) = delete;
	Application(Application&&) = delete;
	Application& operator=(Application&&) = delete;

	void run();

private:
	void drawFrame();


	Window        window;
	VulkanContext vulkanContext;
	RenderContext renderContext;
	Renderer      renderer;
	Camera        camera;
};
