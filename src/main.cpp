#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <optional>

#include "graphics/Vulkan.hpp"
#include "graphics/VulkanContext.hpp"
#include "graphics/Swapchain.hpp"
#include "graphics/Shader.hpp"
#include "graphics/GraphicsPipeline.hpp"
#include "graphics/Vertex.hpp"
#include "graphics/Buffer.hpp"
#include "graphics/FrameData.hpp"
#include "platform/Window.hpp"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

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

class HelloTriangleApplication
{
public:
	void run()
	{
		initVulkan();
		mainLoop();
	}

private:
	// First member: created first, destroyed last, so it outlives the surface in vulkanContext.
	Window window{ { .width = WIDTH, .height = HEIGHT, .title = "Vulkan" } };

	std::optional<VulkanContext> vulkanContext;
	std::optional<Swapchain> swapChain;
	std::optional<GraphicsPipeline> graphicsPipeline;

	vk::raii::CommandPool commandPool = nullptr;

	// One per swapchain image, not per frame: the semaphore may only be reused once its present is done.
	std::vector<vk::raii::Semaphore> renderFinishedSemaphores;

	std::optional<Buffer> vertexBuffer;
	std::optional<Buffer> indexBuffer;

	vk::raii::DescriptorPool descriptorPool = nullptr;

	// Declared after commandPool and descriptorPool, so it is destroyed before them.
	std::vector<FrameData> frames;
	uint32_t frameIndex = 0;

	void initVulkan()
	{
		createVulkanContext();
		swapChain.emplace(*vulkanContext, window.getFramebufferExtent());
		createGraphicsPipeline();
		createCommandPool();
		createVertexBuffer();
		createIndexBuffer();
		createDescriptorPool();
		createFrames();
		createRenderFinishedSemaphores();
	}

	void mainLoop()
	{
		while (!window.shouldClose())
		{
			window.pollEvents();
			drawFrame();
		}

		vulkanContext->getDevice().waitIdle();
	}

	void drawFrame()
	{
		FrameData& frame = frames[frameIndex];

		auto fenceResult = vulkanContext->getDevice().waitForFences(*frame.inFlight, vk::True, UINT64_MAX);
		if (fenceResult != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to wait for fence!");
		}

		auto [result, imageIndex] = swapChain->getHandle().acquireNextImage(UINT64_MAX, *frame.imageAvailable, nullptr);

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
		vulkanContext->getDevice().resetFences(*frame.inFlight);

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

		vulkanContext->getGraphicsQueue().submit(submitInfo, *frame.inFlight);

		const vk::PresentInfoKHR presentInfoKHR
		{ 
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*renderFinishedSemaphores[imageIndex],
			.swapchainCount = 1,
			.pSwapchains = &*swapChain->getHandle(),
			.pImageIndices = &imageIndex
		};

		result = vulkanContext->getGraphicsQueue().presentKHR(presentInfoKHR);
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

	void createDescriptorPool()
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

		descriptorPool = vk::raii::DescriptorPool(vulkanContext->getDevice(), poolInfo);
	}

	void createFrames()
	{
		frames.reserve(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			frames.emplace_back(
				*vulkanContext,
				commandPool,
				descriptorPool,
				graphicsPipeline->getDescriptorSetLayout(),
				sizeof(UniformBufferObject)
			);
		}
	}

	void updateUniformBuffer(FrameData& frame)
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{};
		ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChain->getExtent().width) / static_cast<float>(swapChain->getExtent().height), 0.1f, 10.0f);

		memcpy(frame.uniformBuffer.getMapped(), &ubo, sizeof(ubo));
	}

	void recreateSwapChain()
	{
		const vk::Extent2D extent = window.waitForDrawableExtent();
		if (window.shouldClose())
		{
			return;
		}

		vulkanContext->getDevice().waitIdle();

		swapChain->recreate(extent);
	}

	void createVulkanContext()
	{
		const std::vector<char const*> requiredDeviceExtensions =
		{
			vk::KHRSwapchainExtensionName
		};

		vulkanContext.emplace(VulkanContextConfig
		{
			.enableValidation           = enableValidationLayers,
			.requiredInstanceExtensions = window.getRequiredInstanceExtensions(),
			.requiredDeviceExtensions   = requiredDeviceExtensions,
			.createSurface              = [this](VkInstance instance) { return window.createSurface(instance); }
		});
	}

	void createGraphicsPipeline() 
	{
		Shader shader(*vulkanContext, VR_SHADER_PATH,
		{
			{ vk::ShaderStageFlagBits::eVertex,   "vertMain" },
			{ vk::ShaderStageFlagBits::eFragment, "fragMain" }
		});

		graphicsPipeline.emplace(*vulkanContext, shader, swapChain->getSurfaceFormat().format);
	}

	void createCommandPool()
	{
		vk::CommandPoolCreateInfo poolInfo
		{ 
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = vulkanContext->getGraphicsQueueFamilyIndex()
		};

		commandPool = vk::raii::CommandPool(vulkanContext->getDevice(), poolInfo);
	}

	void recordCommandBuffer(FrameData& frame, uint32_t imageIndex)
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
			.imageView = swapChain->getImageViews()[imageIndex],
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
				.extent = swapChain->getExtent() 
			},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &attachmentInfo 
		};

		commandBuffer.beginRendering(renderingInfo);

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline->getHandle());

		commandBuffer.bindVertexBuffers(0, *vertexBuffer->getHandle(), { 0 });
		commandBuffer.bindIndexBuffer(*indexBuffer->getHandle(), 0, vk::IndexType::eUint16);

		commandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(swapChain->getExtent().height), static_cast<float>(swapChain->getExtent().width), -static_cast<float>(swapChain->getExtent().height), 0.0f, 1.0f));
		commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChain->getExtent()));

		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline->getLayout(), 0, *frame.descriptorSet, nullptr);
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

	void transition_image_layout(
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
			.image = swapChain->getImages()[imageIndex],
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

	void createRenderFinishedSemaphores()
	{
		assert(renderFinishedSemaphores.empty());

		for (size_t i = 0; i < swapChain->getImages().size(); i++)
		{
			renderFinishedSemaphores.emplace_back(vulkanContext->getDevice(), vk::SemaphoreCreateInfo());
		}
	}

	void createVertexBuffer()
	{
		vertexBuffer.emplace(Buffer::forVertices(*vulkanContext, commandPool, vertices));
	}

	void createIndexBuffer()
	{
		indexBuffer.emplace(Buffer::forIndices(*vulkanContext, commandPool, indices));
	}

};

int main()
{
	// Point the Vulkan loader at the validation layers built by vcpkg. Must
	// happen before the loader scans for layers; an explicit
	// VK_ADD_LAYER_PATH in the environment takes precedence.
	if (enableValidationLayers && std::getenv("VK_ADD_LAYER_PATH") == nullptr)
	{
#ifdef _WIN32
		_putenv_s("VK_ADD_LAYER_PATH", VR_VALIDATION_LAYER_DIR);
#else
		setenv("VK_ADD_LAYER_PATH", VR_VALIDATION_LAYER_DIR, 0);
#endif
	}

	try
	{
		HelloTriangleApplication app;
		app.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
