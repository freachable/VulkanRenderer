#include "Renderer.hpp"

#include "Shader.hpp"
#include "Vertex.hpp"

#include <array>
#include <cstdint>
#include <cstring>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
	const std::vector<Vertex> vertices = 
	{
		{ { -0.5f, -0.5f, 0.0f },{ 1.0f, 0.0f, 0.0f },{ 0.0f, 0.0f } },
		{ { 0.5f, -0.5f, 0.0f },{ 0.0f, 1.0f, 0.0f },{ 1.0f, 0.0f } },
		{ { 0.5f, 0.5f, 0.0f },{ 0.0f, 0.0f, 1.0f },{ 1.0f, 1.0f } },
		{ { -0.5f, 0.5f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 1.0f } },

		{ { -0.5f, -0.5f, -0.5f },{ 1.0f, 0.0f, 0.0f },{ 0.0f, 0.0f } },
		{ { 0.5f, -0.5f, -0.5f },{ 0.0f, 1.0f, 0.0f },{ 1.0f, 0.0f } },
		{ { 0.5f, 0.5f, -0.5f },{ 0.0f, 0.0f, 1.0f },{ 1.0f, 1.0f } },
		{ { -0.5f, 0.5f, -0.5f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 1.0f } }
	};

	const std::vector<uint16_t> indices = 
	{
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4
	};

	struct UniformBufferObject
	{
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
	};

	// The helpers below build one member each for the constructor's initializer list.
	// They return by value; for the non-movable types this relies on C++17 guaranteed copy elision.

	GraphicsPipeline createGraphicsPipeline(VulkanContext const& context, vk::Format colorFormat, vk::Format depthFormat)
	{
		Shader shader(context, VR_SHADER_PATH,
		{
			{ vk::ShaderStageFlagBits::eVertex,   "vertMain" },
			{ vk::ShaderStageFlagBits::eFragment, "fragMain" }
		});

		return GraphicsPipeline(context, shader, colorFormat, depthFormat);
	}

	vk::raii::CommandPool createUploadCommandPool(VulkanContext const& context)
	{
		vk::CommandPoolCreateInfo poolInfo
		{
			.flags = vk::CommandPoolCreateFlagBits::eTransient,
			.queueFamilyIndex = context.getGraphicsQueueFamilyIndex()
		};

		return vk::raii::CommandPool(context.getDevice(), poolInfo);
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

	vk::raii::DescriptorPool createDescriptorPool(VulkanContext const& context, uint32_t framesInFlight)
	{
		std::array<vk::DescriptorPoolSize, 2> poolSize
		{ 
			{ 
				{ 
					.type = vk::DescriptorType::eUniformBuffer, 
					.descriptorCount = framesInFlight 
				},
				{ 
					.type = vk::DescriptorType::eCombinedImageSampler, 
					.descriptorCount = framesInFlight 
				} 
			} 
		};

		vk::DescriptorPoolCreateInfo poolInfo
		{ 
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets = framesInFlight,
			.poolSizeCount = static_cast<uint32_t>(poolSize.size()),
			.pPoolSizes = poolSize.data() 
		};

		return vk::raii::DescriptorPool(context.getDevice(), poolInfo);
	}

	std::vector<FrameData> createFrames(VulkanContext const& context,
	                                    vk::raii::DescriptorPool const& descriptorPool,
	                                    GraphicsPipeline const& graphicsPipeline,
	                                    uint32_t framesInFlight,
	                                    Texture const& texture,
	                                    vk::raii::Sampler const& textureSampler)
	{
		std::vector<FrameData> frames;
		frames.reserve(framesInFlight);

		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			frames.emplace_back(
				context,
				descriptorPool,
				graphicsPipeline.getDescriptorSetLayout(),
				sizeof(UniformBufferObject),
				texture.getImageView(),
				textureSampler
			);
		}

		return frames;
	}
}

Renderer::Renderer(VulkanContext const& context, RenderContext const& renderContext)
	: depthBuffer(context, renderContext.getSwapchain().getExtent())
	, graphicsPipeline(createGraphicsPipeline(context, renderContext.getSwapchain().getSurfaceFormat().format, depthBuffer.getFormat()))
	, uploadCommandPool(createUploadCommandPool(context))
	, vertexBuffer(Buffer::forVertices(context, uploadCommandPool, vertices))
	, indexBuffer(Buffer::forIndices(context, uploadCommandPool, indices))
	, texture(context, uploadCommandPool, VR_TEXTURE_PATH)
	, textureSampler(createTextureSampler(context))
	, descriptorPool(createDescriptorPool(context, renderContext.getFramesInFlight()))
	, frames(createFrames(context, descriptorPool, graphicsPipeline, renderContext.getFramesInFlight(), texture, textureSampler))
	, startTime(std::chrono::steady_clock::now())
{
}

void Renderer::onSwapchainRecreated(Swapchain const& swapchain)
{
	depthBuffer.recreate(swapchain.getExtent());
}

void Renderer::draw(Frame const& frame)
{
	FrameData& frameData = frames[frame.frameIndex];

	// Safe to write: beginFrame waited until the GPU was done with this frame slot.
	updateUniformBuffer(frameData, frame.extent);

	// beginFrame has begun the command buffer and put the image in eColorAttachmentOptimal;
	// endFrame transitions it for presenting and ends the command buffer.
	auto const& commandBuffer = frame.commandBuffer;

	// The depth image is shared by all frames in flight. Wait for the previous frame's depth writes,
	// then move it to the attachment layout; eUndefined discards the old contents, which get cleared anyway.
	vk::ImageMemoryBarrier2 depthBarrier =
	{
		.srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		.dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = *depthBuffer.getImage(),
		.subresourceRange =
		{
			.aspectMask = depthBuffer.getAspectMask(),
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		}
	};

	commandBuffer.pipelineBarrier2(vk::DependencyInfo
	{
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &depthBarrier
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
		.imageView = *depthBuffer.getImageView(),
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

	commandBuffer.bindVertexBuffers(0, *vertexBuffer.getHandle(), { 0 });
	commandBuffer.bindIndexBuffer(*indexBuffer.getHandle(), 0, vk::IndexType::eUint16);

	commandBuffer.setViewport(0, vk::Viewport(0.0f, static_cast<float>(frame.extent.height), static_cast<float>(frame.extent.width), -static_cast<float>(frame.extent.height), 0.0f, 1.0f));
	commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), frame.extent));

	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getLayout(), 0, *frameData.descriptorSet, nullptr);
	commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

	commandBuffer.endRendering();
}

void Renderer::updateUniformBuffer(FrameData& frameData, vk::Extent2D extent)
{
	const float time = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();

	UniformBufferObject ubo{};
	ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(extent.width) / static_cast<float>(extent.height), 0.1f, 10.0f);

	std::memcpy(frameData.uniformBuffer.getMapped(), &ubo, sizeof(ubo));
}
