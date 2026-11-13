#include "Renderer.hpp"

#include "assets/AssetPaths.hpp"
#include "assets/ImageLoader.hpp"
#include "assets/ObjLoader.hpp"
#include "graphics/gpu/Shader.hpp"
#include "graphics/gpu/VertexLayout.hpp"
#include "graphics/vulkan/Barriers.hpp"

#include <cstdint>
#include <cstring>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
	// The helpers below build one member each for the constructor's initializer list.
	// They return by value; for the non-movable types this relies on C++17 guaranteed copy elision.

	GraphicsPipeline createGraphicsPipeline(VulkanContext const& context, vk::DescriptorSetLayout descriptorSetLayout,
	                                        vk::Format colorFormat, vk::Format depthFormat)
	{
		Shader shader(context, AssetPaths::getShader("shader"),
		{
			{ vk::ShaderStageFlagBits::eVertex,   "vertMain" },
			{ vk::ShaderStageFlagBits::eFragment, "fragMain" }
		});

		auto const vertexBinding    = getVertexBindingDescription();
		auto const vertexAttributes = getVertexAttributeDescriptions();

		return GraphicsPipeline(context, shader, GraphicsPipelineConfig
		{
			.colorFormat          = colorFormat,
			.depthFormat          = depthFormat,
			.descriptorSetLayouts = { &descriptorSetLayout, 1 },
			.vertexBindings       = { &vertexBinding, 1 },
			.vertexAttributes     = vertexAttributes
		});
	}

	vk::raii::Sampler createTextureSampler(VulkanContext const& context)
	{
		vk::PhysicalDeviceProperties properties = context.getPhysicalDevice().getProperties();

		vk::SamplerCreateInfo samplerInfo
		{
			.magFilter        = vk::Filter::eLinear,
			.minFilter        = vk::Filter::eLinear,
			.mipmapMode       = vk::SamplerMipmapMode::eLinear,
			.addressModeU     = vk::SamplerAddressMode::eRepeat,
			.addressModeV     = vk::SamplerAddressMode::eRepeat,
			.addressModeW     = vk::SamplerAddressMode::eRepeat,
			.mipLodBias       = 0.0f,
			.anisotropyEnable = vk::True,
			.maxAnisotropy    = properties.limits.maxSamplerAnisotropy,
			.compareEnable    = vk::False,
			.compareOp        = vk::CompareOp::eAlways
		};

		return vk::raii::Sampler(context.getDevice(), samplerInfo);
	}

	std::vector<FrameResources> createFrames(VulkanContext const& context,
	                                         vk::raii::DescriptorPool const& descriptorPool,
	                                         vk::raii::DescriptorSetLayout const& descriptorSetLayout,
	                                         uint32_t framesInFlight,
	                                         Texture const& texture,
	                                         vk::raii::Sampler const& textureSampler)
	{
		std::vector<FrameResources> frames;
		frames.reserve(framesInFlight);

		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			frames.emplace_back(context, descriptorPool, descriptorSetLayout, texture.getImage().getView(), textureSampler);
		}

		return frames;
	}
}

Renderer::Renderer(VulkanContext const& context, RenderContext const& renderContext)
	: depthBuffer(context, renderContext.getSwapchain().getExtent())
	, descriptorSetLayout(FrameResources::createDescriptorSetLayout(context))
	, graphicsPipeline(createGraphicsPipeline(context, *descriptorSetLayout, renderContext.getSwapchain().getSurfaceFormat().format, depthBuffer.getFormat()))
	, mesh(context, loadObj(AssetPaths::getMesh("viking_room.obj")))
	, texture(context, loadImage(AssetPaths::getTexture("viking_room.png")))
	, textureSampler(createTextureSampler(context))
	, descriptorPool(FrameResources::createDescriptorPool(context, renderContext.getFramesInFlight()))
	, frames(createFrames(context, descriptorPool, descriptorSetLayout, renderContext.getFramesInFlight(), texture, textureSampler))
	, startTime(std::chrono::steady_clock::now())
{
}

void Renderer::onSwapchainRecreated(Swapchain const& swapchain)
{
	depthBuffer.recreate(swapchain.getExtent());
}

void Renderer::draw(Frame const& frame)
{
	FrameResources& frameResources = frames[frame.frameIndex];

	// Safe to write: beginFrame waited until the GPU was done with this frame slot.
	updateUniformBuffer(frameResources, frame.extent);

	// beginFrame has begun the command buffer and put the image in eColorAttachmentOptimal;
	// endFrame transitions it for presenting and ends the command buffer.
	auto const& commandBuffer = frame.commandBuffer;

	Image const& depthImage = depthBuffer.getImage();

	// The depth image is shared by all frames in flight. Wait for the previous frame's depth writes,
	// then move it to the attachment layout; eUndefined discards the old contents, which get cleared anyway.
	transitionImage(commandBuffer, *depthImage.getHandle(), depthImage.getAspectMask(),
	{
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
		.srcStage  = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		.srcAccess = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		.dstStage  = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		.dstAccess = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite
	});

	vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);

	vk::RenderingAttachmentInfo attachmentInfo =
	{
		.imageView = frame.imageView,
		.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eStore,
		.clearValue = clearColor
	};

	vk::RenderingAttachmentInfo depthAttachmentInfo =
	{
		.imageView = *depthImage.getView(),
		.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
		.loadOp = vk::AttachmentLoadOp::eClear,
		.storeOp = vk::AttachmentStoreOp::eDontCare, // Nothing reads depth after the frame.
		.clearValue = vk::ClearDepthStencilValue(1.0f, 0) // 1.0 = far plane.
	};

	vk::RenderingInfo renderingInfo =
	{
		.renderArea =
		{
			.offset = { 0, 0 },
			.extent = frame.extent
		},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &attachmentInfo,
		.pDepthAttachment = &depthAttachmentInfo
	};

	commandBuffer.beginRendering(renderingInfo);

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline.getHandle());

	mesh.bind(commandBuffer);

	commandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(frame.extent.height), static_cast<float>(frame.extent.width), -static_cast<float>(frame.extent.height), 0.0f, 1.0f));
	commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), frame.extent));

	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getLayout(), 0, *frameResources.descriptorSet, nullptr);
	mesh.draw(commandBuffer);

	commandBuffer.endRendering();
}

void Renderer::updateUniformBuffer(FrameResources& frameResources, vk::Extent2D extent)
{
	const float time = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();

	UniformBufferObject ubo{};
	ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(extent.width) / static_cast<float>(extent.height), 0.1f, 10.0f);

	std::memcpy(frameResources.uniformBuffer.getMapped(), &ubo, sizeof(ubo));
}
