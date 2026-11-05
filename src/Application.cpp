#include "Application.hpp"

#include "graphics/Shader.hpp"
#include "graphics/Vertex.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <cstring>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
	constexpr uint32_t WIDTH = 800;
	constexpr uint32_t HEIGHT = 600;
	constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

	const std::vector<Vertex> vertices = {
		{ { -0.5f, -0.5f },{ 1.0f, 0.0f, 0.0f } },
		{ { 0.5f, -0.5f },{ 0.0f, 1.0f, 0.0f } },
		{ { 0.5f, 0.5f },{ 0.0f, 0.0f, 1.0f } },
		{ { -0.5f, 0.5f },{ 1.0f, 1.0f, 1.0f } }
	};

	const std::vector<uint16_t> indices = {
		0, 1, 2, 2, 3, 0
	};

	struct UniformBufferObject
	{
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
	};

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

	GraphicsPipeline createGraphicsPipeline(VulkanContext const& context, Swapchain const& swapChain)
	{
		Shader shader(context, VR_SHADER_PATH,
		{
			{ vk::ShaderStageFlagBits::eVertex,   "vertMain" },
			{ vk::ShaderStageFlagBits::eFragment, "fragMain" }
		});

		return GraphicsPipeline(context, shader, swapChain.getSurfaceFormat().format);
	}

	vk::raii::CommandPool createCommandPool(VulkanContext const& context)
	{
		vk::CommandPoolCreateInfo poolInfo
		{
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = context.getGraphicsQueueFamilyIndex()
		};

		return vk::raii::CommandPool(context.getDevice(), poolInfo);
	}

	vk::raii::DescriptorPool createDescriptorPool(VulkanContext const& context)
	{
		vk::DescriptorPoolSize poolSize
		{
			.type = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = MAX_FRAMES_IN_FLIGHT
		};

		vk::DescriptorPoolCreateInfo poolInfo
		{
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets = MAX_FRAMES_IN_FLIGHT,
			.poolSizeCount = 1,
			.pPoolSizes = &poolSize
		};

		return vk::raii::DescriptorPool(context.getDevice(), poolInfo);
	}

	std::vector<FrameData> createFrames(VulkanContext const& context,
	                                    vk::raii::CommandPool const& commandPool,
	                                    vk::raii::DescriptorPool const& descriptorPool,
	                                    GraphicsPipeline const& graphicsPipeline)
	{
		std::vector<FrameData> frames;
		frames.reserve(MAX_FRAMES_IN_FLIGHT);

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			frames.emplace_back(
				context,
				commandPool,
				descriptorPool,
				graphicsPipeline.getDescriptorSetLayout(),
				sizeof(UniformBufferObject)
			);
		}

		return frames;
	}

	std::vector<vk::raii::Semaphore> createRenderFinishedSemaphores(VulkanContext const& context, Swapchain const& swapChain)
	{
		std::vector<vk::raii::Semaphore> semaphores;
		semaphores.reserve(swapChain.getImages().size());

		for (size_t i = 0; i < swapChain.getImages().size(); i++)
		{
			semaphores.emplace_back(context.getDevice(), vk::SemaphoreCreateInfo());
		}

		return semaphores;
	}
}

Application::Application()
	: window({ .width = WIDTH, .height = HEIGHT, .title = "Vulkan" })
	, vulkanContext(createVulkanContext(window))
	, swapChain(vulkanContext, window.getFramebufferExtent())
	, graphicsPipeline(createGraphicsPipeline(vulkanContext, swapChain))
	, commandPool(createCommandPool(vulkanContext))
	, vertexBuffer(Buffer::forVertices(vulkanContext, commandPool, vertices))
	, indexBuffer(Buffer::forIndices(vulkanContext, commandPool, indices))
	, descriptorPool(createDescriptorPool(vulkanContext))
	, frames(createFrames(vulkanContext, commandPool, descriptorPool, graphicsPipeline))
	, renderFinishedSemaphores(createRenderFinishedSemaphores(vulkanContext, swapChain))
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
	FrameData& frame = frames[frameIndex];

	auto fenceResult = vulkanContext.getDevice().waitForFences(*frame.inFlight, vk::True, UINT64_MAX);
	if (fenceResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("failed to wait for fence!");
	}

	auto [result, imageIndex] = swapChain.getHandle().acquireNextImage(UINT64_MAX, *frame.imageAvailable, nullptr);

	if (result == vk::Result::eErrorOutOfDateKHR)
	{
		recreateSwapChain();
		return;
	}
	if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
	{
		assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	// Only reset the fence if we are submitting work
	vulkanContext.getDevice().resetFences(*frame.inFlight);

	frame.commandBuffer.reset();
	recordCommandBuffer(frame, imageIndex);

	vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);

	updateUniformBuffer(frame);

	const vk::SubmitInfo submitInfo
	{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*frame.imageAvailable,
		.pWaitDstStageMask = &waitDestinationStageMask,
		.commandBufferCount = 1,
		.pCommandBuffers = &*frame.commandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &*renderFinishedSemaphores[imageIndex]
	};

	vulkanContext.getGraphicsQueue().submit(submitInfo, *frame.inFlight);

	const vk::PresentInfoKHR presentInfoKHR
	{
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &*renderFinishedSemaphores[imageIndex],
		.swapchainCount = 1,
		.pSwapchains = &*swapChain.getHandle(),
		.pImageIndices = &imageIndex
	};

	result = vulkanContext.getGraphicsQueue().presentKHR(presentInfoKHR);
	// Always consume the flag, so a recreate for another reason also clears a pending resize.
	const bool resized = window.consumeResized();
	if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || resized)
	{
		recreateSwapChain();
	}
	else
	{
		// There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
		assert(result == vk::Result::eSuccess);
	}

	frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::updateUniformBuffer(FrameData& frame)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo{};
	ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChain.getExtent().width) / static_cast<float>(swapChain.getExtent().height), 0.1f, 10.0f);

	memcpy(frame.uniformBuffer.getMapped(), &ubo, sizeof(ubo));
}

void Application::recreateSwapChain()
{
	const vk::Extent2D extent = window.waitForDrawableExtent();
	if (window.shouldClose())
	{
		return;
	}

	vulkanContext.getDevice().waitIdle();

	swapChain.recreate(extent);
}

void Application::recordCommandBuffer(FrameData& frame, uint32_t imageIndex)
{
	auto& commandBuffer = frame.commandBuffer;
	commandBuffer.begin({});

	// Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
	transition_image_layout(
		commandBuffer,
		imageIndex,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eColorAttachmentOptimal,
		{},                                                        // srcAccessMask (no need to wait for previous operations)
		vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
		vk::PipelineStageFlagBits2::eColorAttachmentOutput         // dstStage
	);

	vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);

	vk::RenderingAttachmentInfo attachmentInfo =
	{
		.imageView = swapChain.getImageViews()[imageIndex],
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = clearColor
	};

	vk::RenderingInfo renderingInfo =
	{
		.renderArea =
		{
			.offset = { 0, 0 },
			.extent = swapChain.getExtent()
		},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &attachmentInfo
	};

	commandBuffer.beginRendering(renderingInfo);

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline.getHandle());

	commandBuffer.bindVertexBuffers(0, *vertexBuffer.getHandle(), { 0 });
	commandBuffer.bindIndexBuffer(*indexBuffer.getHandle(), 0, vk::IndexType::eUint16);

	commandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(swapChain.getExtent().height), static_cast<float>(swapChain.getExtent().width), -static_cast<float>(swapChain.getExtent().height), 0.0f, 1.0f));
	commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChain.getExtent()));

	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getLayout(), 0, *frame.descriptorSet, nullptr);
	commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

	commandBuffer.endRendering();

	transition_image_layout(
		commandBuffer,
		imageIndex,
		vk::ImageLayout::eColorAttachmentOptimal,
		vk::ImageLayout::ePresentSrcKHR,
		vk::AccessFlagBits2::eColorAttachmentWrite,             // srcAccessMask
		{},                                                     // dstAccessMask
		vk::PipelineStageFlagBits2::eColorAttachmentOutput,     // srcStage
		vk::PipelineStageFlagBits2::eBottomOfPipe               // dstStage
	);

	commandBuffer.end();
}

void Application::transition_image_layout(
	vk::raii::CommandBuffer& commandBuffer,
	uint32_t                imageIndex,
	vk::ImageLayout         old_layout,
	vk::ImageLayout         new_layout,
	vk::AccessFlags2        src_access_mask,
	vk::AccessFlags2        dst_access_mask,
	vk::PipelineStageFlags2 src_stage_mask,
	vk::PipelineStageFlags2 dst_stage_mask
)
{
	vk::ImageMemoryBarrier2 barrier =
	{
		.srcStageMask = src_stage_mask,
		.srcAccessMask = src_access_mask,
		.dstStageMask = dst_stage_mask,
		.dstAccessMask = dst_access_mask,
		.oldLayout = old_layout,
		.newLayout = new_layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = swapChain.getImages()[imageIndex],
		.subresourceRange =
			{
			   .aspectMask = vk::ImageAspectFlagBits::eColor,
			   .baseMipLevel = 0,
			   .levelCount = 1,
			   .baseArrayLayer = 0,
			   .layerCount = 1
			}
	};

	vk::DependencyInfo dependency_info =
	{
		.dependencyFlags = {},
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrier
	};

	commandBuffer.pipelineBarrier2(dependency_info);
}
