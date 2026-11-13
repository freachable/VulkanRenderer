#pragma once

#include "Vulkan.hpp"
#include "VulkanContext.hpp"
#include "RenderContext.hpp"
#include "GraphicsPipeline.hpp"
#include "Buffer.hpp"
#include "FrameData.hpp"
#include "Texture.hpp"
#include "DepthBuffer.hpp"

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
	void updateUniformBuffer(FrameData& frameData, vk::Extent2D extent);

	// Declared before graphicsPipeline: the pipeline will be created with its format.
	DepthBuffer depthBuffer;

	GraphicsPipeline graphicsPipeline;

	// Only for one-off uploads; per-frame command buffers live in the RenderContext.
	vk::raii::CommandPool uploadCommandPool;

	Buffer vertexBuffer;
	Buffer indexBuffer;

	Texture texture;

	// Shared by all textures; it only says how they are sampled.
	vk::raii::Sampler textureSampler;

	vk::raii::DescriptorPool descriptorPool;

	// One per frame in flight, indexed by Frame::frameIndex.
	// Declared after descriptorPool, so it is destroyed before it.
	std::vector<FrameData> frames;

	std::chrono::steady_clock::time_point startTime;
};
