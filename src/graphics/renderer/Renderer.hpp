#pragma once

#include "graphics/gpu/DepthBuffer.hpp"
#include "graphics/gpu/GraphicsPipeline.hpp"
#include "graphics/gpu/Mesh.hpp"
#include "graphics/gpu/Texture.hpp"
#include "graphics/renderer/FrameResources.hpp"
#include "graphics/vulkan/RenderContext.hpp"
#include "graphics/vulkan/Vulkan.hpp"
#include "graphics/vulkan/VulkanContext.hpp"

#include <chrono>
#include <vector>

// Owns what gets drawn: the pipeline, the mesh and its per-frame uniforms.
// The RenderContext decides when a frame happens; the Renderer records what goes into it.
class Renderer
{
public:
	// Uses the render context's swapchain format and frame count; it is not kept.
	Renderer(VulkanContext const& context, RenderContext const& renderContext);

	// The pipeline cannot move, so neither can the renderer.
	Renderer(Renderer const&) = delete;
	Renderer& operator=(Renderer const&) = delete;
	Renderer(Renderer&&) = delete;
	Renderer& operator=(Renderer&&) = delete;

	// Updates this frame's uniforms and records the draw into frame.commandBuffer.
	// Call between RenderContext::beginFrame and endFrame.
	void draw(Frame const& frame);

	// Resizes the render targets that match the swapchain. Register it with
	// RenderContext::setOnSwapchainRecreated; the device is idle when it is called.
	void onSwapchainRecreated(Swapchain const& swapchain);

private:
	void updateUniformBuffer(FrameResources& frameResources, vk::Extent2D extent);

	// Declared before graphicsPipeline: the pipeline is created with its format.
	DepthBuffer depthBuffer;

	// Declared before graphicsPipeline: the pipeline layout is created from it.
	vk::raii::DescriptorSetLayout descriptorSetLayout;

	GraphicsPipeline graphicsPipeline;

	Mesh    mesh;
	Texture texture;

	// Shared by all textures; it only says how they are sampled.
	vk::raii::Sampler textureSampler;

	vk::raii::DescriptorPool descriptorPool;

	// One per frame in flight, indexed by Frame::frameIndex.
	// Declared after descriptorPool, so it is destroyed before it.
	std::vector<FrameResources> frames;

	std::chrono::steady_clock::time_point startTime;
};
