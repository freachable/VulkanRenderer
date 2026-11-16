#include "Window.hpp"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

// GLFW only declares glfwCreateWindowSurface when the Vulkan C header was
// included before it. Include it explicitly: with C++20 modules, Vulkan.hpp
// imports vulkan_hpp, which does not export the VK_VERSION_1_0 macro.
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace
{
	void applyPlatformHint()
	{
		char const* platformName = std::getenv("VR_GLFW_PLATFORM");
		if (platformName == nullptr)
		{
			return;
		}

		if (std::strcmp(platformName, "x11") == 0)
		{
			glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
		}
		else if (std::strcmp(platformName, "wayland") == 0)
		{
			glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
		}
	}
}

Window::Window(WindowConfig const& config)
{
	applyPlatformHint();

	if (!glfwInit())
	{
		throw std::runtime_error("failed to initialize GLFW!");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

	handle = glfwCreateWindow(static_cast<int>(config.width), static_cast<int>(config.height), config.title, nullptr, nullptr);
	if (handle == nullptr)
	{
		glfwTerminate();
		throw std::runtime_error("failed to create window!");
	}

	glfwSetWindowUserPointer(handle, this);
	glfwSetFramebufferSizeCallback(handle, framebufferResizeCallback);
	glfwSetCursorPosCallback(handle, cursorPosCallback);
}

Window::~Window()
{
	glfwDestroyWindow(handle);
	glfwTerminate();
}

bool Window::shouldClose() const
{
	return glfwWindowShouldClose(handle);
}

void Window::pollEvents()
{
	glfwPollEvents();
}

vk::Extent2D Window::getFramebufferExtent() const
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(handle, &width, &height);
	return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
}

vk::Extent2D Window::waitForDrawableExtent()
{
	vk::Extent2D extent = getFramebufferExtent();

	while ((extent.width == 0 || extent.height == 0) && !shouldClose())
	{
		glfwWaitEvents();
		extent = getFramebufferExtent();
	}

	if (shouldClose())
	{
		return { 0, 0 };
	}

	return extent;
}

bool Window::consumeResized()
{
	bool resized = framebufferResized;
	framebufferResized = false;
	return resized;
}

std::span<char const* const> Window::getRequiredInstanceExtensions() const
{
	uint32_t count = 0;
	char const** extensions = glfwGetRequiredInstanceExtensions(&count);
	if (extensions == nullptr)
	{
		throw std::runtime_error("GLFW found no Vulkan support for window surfaces!");
	}

	return { extensions, count };
}

VkSurfaceKHR Window::createSurface(VkInstance instance) const
{
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	if (glfwCreateWindowSurface(instance, handle, nullptr, &surface) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create window surface!");
	}

	return surface;
}

bool Window::isKeyDown(Key key) const
{
	int glfwKey = GLFW_KEY_UNKNOWN;
	switch (key)
	{
		case Key::W: glfwKey = GLFW_KEY_W; break;
		case Key::A: glfwKey = GLFW_KEY_A; break;
		case Key::S: glfwKey = GLFW_KEY_S; break;
		case Key::D: glfwKey = GLFW_KEY_D; break;
		case Key::Q: glfwKey = GLFW_KEY_Q; break;
		case Key::E: glfwKey = GLFW_KEY_E; break;
	}

	return glfwGetKey(handle, glfwKey) == GLFW_PRESS;
}

bool Window::isRightMouseDown() const
{
	return glfwGetMouseButton(handle, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
}

void Window::setCursorCaptured(bool captured)
{
	if (captured == cursorCaptured)
	{
		return;
	}

	cursorCaptured = captured;
	glfwSetInputMode(handle, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

	// Hiding or showing the cursor moves it; don't count that as mouse movement.
	hasLastCursorPos = false;
	mouseDelta = {};
}

glm::vec2 Window::consumeMouseDelta()
{
	glm::vec2 delta = mouseDelta;
	mouseDelta = {};
	return delta;
}

void Window::framebufferResizeCallback(GLFWwindow* handle, int /*width*/, int /*height*/)
{
	auto* window = static_cast<Window*>(glfwGetWindowUserPointer(handle));
	window->framebufferResized = true;
}

void Window::cursorPosCallback(GLFWwindow* handle, double x, double y)
{
	auto* window = static_cast<Window*>(glfwGetWindowUserPointer(handle));
	if (window->hasLastCursorPos)
	{
		window->mouseDelta += glm::vec2(x - window->lastCursorPos.x, y - window->lastCursorPos.y);
	}

	window->lastCursorPos    = { x, y };
	window->hasLastCursorPos = true;
}
