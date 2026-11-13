#pragma once

#include "graphics/vulkan/Vulkan.hpp"

#include <cstdint>
#include <span>

#include <glm/glm.hpp>

// Forward declaration, so GLFW stays out of every file that includes this header.
struct GLFWwindow;

enum class Key { W, A, S, D, Q, E };

struct WindowConfig
{
	uint32_t    width     = 800;
	uint32_t    height    = 600;
	char const* title     = "Vulkan";
	bool        resizable = true;
};

// Owns GLFW and the native window. The only class that talks to GLFW;
// the Vulkan side only gets extents, instance extensions and a surface from it.
// Must outlive every Vulkan object that uses its surface.
class Window
{
public:
	explicit Window(WindowConfig const& config);
	~Window();

	// GLFW's user pointer points at `this`, so the object must stay put.
	Window(Window const&) = delete;
	Window& operator=(Window const&) = delete;
	Window(Window&&) = delete;
	Window& operator=(Window&&) = delete;

	bool shouldClose() const;
	void pollEvents();

	vk::Extent2D getFramebufferExtent() const;

	// Blocks while the window is minimized (framebuffer is 0x0).
	// Returns {0, 0} if the window was closed while waiting.
	vk::Extent2D waitForDrawableExtent();

	// Returns true once after the framebuffer was resized, then clears the flag.
	bool consumeResized();

	// Instance extensions GLFW needs to create a surface for this window.
	std::span<char const* const> getRequiredInstanceExtensions() const;

	// Creates a surface for this window. The caller owns the returned surface.
	VkSurfaceKHR createSurface(VkInstance instance) const;

	bool isKeyDown(Key key) const;
	bool isRightMouseDown() const;

	// Hides the cursor and keeps it in the window. Calling it with the current state does nothing.
	void setCursorCaptured(bool captured);

	// Returns how far the mouse moved since the last call, in screen pixels, then resets it.
	glm::vec2 consumeMouseDelta();

private:
	static void framebufferResizeCallback(GLFWwindow* handle, int width, int height);
	static void cursorPosCallback(GLFWwindow* handle, double x, double y);

	GLFWwindow* handle = nullptr;
	bool framebufferResized = false;

	glm::dvec2 lastCursorPos{};
	bool       hasLastCursorPos = false;
	bool       cursorCaptured   = false;
	glm::vec2  mouseDelta{};
};
