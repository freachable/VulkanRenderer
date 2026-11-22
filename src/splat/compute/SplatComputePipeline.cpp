// P2
#include "splat/compute/SplatComputePipeline.hpp"

#include "assets/AssetPaths.hpp"
#include "graphics/gpu/Shader.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <utility>

namespace
{
	vk::raii::DescriptorSetLayout createSet0Layout(VulkanContext const& context)
	{
		vk::DescriptorSetLayoutBinding binding
		{
			.binding         = 0,
			.descriptorType  = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 1,
			.stageFlags      = vk::ShaderStageFlagBits::eCompute
		};

		return vk::raii::DescriptorSetLayout(context.getDevice(), vk::DescriptorSetLayoutCreateInfo
		{
			.bindingCount = 1,
			.pBindings    = &binding
		});
	}

	vk::raii::DescriptorSetLayout createSet1Layout(VulkanContext const& context)
	{
		std::array<vk::DescriptorSetLayoutBinding, 7> bindings{};
		for (uint32_t i = 0; i < bindings.size(); ++i)
		{
			bindings[i] = vk::DescriptorSetLayoutBinding
			{
				.binding         = i,
				.descriptorType  = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags      = vk::ShaderStageFlagBits::eCompute
			};
		}

		return vk::raii::DescriptorSetLayout(context.getDevice(), vk::DescriptorSetLayoutCreateInfo
		{
			.bindingCount = static_cast<uint32_t>(bindings.size()),
			.pBindings    = bindings.data()
		});
	}

	vk::raii::Pipeline createComputePipeline(
		VulkanContext const& context,
		vk::PipelineLayout layout,
		std::filesystem::path const& spvPath)
	{
		Shader shader(context, spvPath, { { vk::ShaderStageFlagBits::eCompute, "computeMain" } });
		vk::ComputePipelineCreateInfo info
		{
			.stage  = shader.getStage(vk::ShaderStageFlagBits::eCompute),
			.layout = layout
		};
		return vk::raii::Pipeline(context.getDevice(), nullptr, info);
	}
}

SplatComputePipeline::SplatComputePipeline(VulkanContext const& context, uint32_t maxGaussians)
	: maxGaussians(maxGaussians)
	, inputGaussians(StorageBuffer::createDeviceLocal(context, sizeof(Gaussian3D) * maxGaussians))
	, projectedBuffer(StorageBuffer::createDeviceLocal(context, sizeof(ProjectedSplat2D) * maxGaussians))
	, indirectBuffer(StorageBuffer::createDeviceLocal(context, sizeof(DrawIndirectCommand), vk::BufferUsageFlagBits::eIndirectBuffer))
	, sortKeysA(StorageBuffer::createDeviceLocal(context, sizeof(uint32_t) * maxGaussians))
	, sortKeysB(StorageBuffer::createDeviceLocal(context, sizeof(uint32_t) * maxGaussians))
	, sortIndicesA(StorageBuffer::createDeviceLocal(context, sizeof(uint32_t) * maxGaussians))
	, sortIndicesB(StorageBuffer::createDeviceLocal(context, sizeof(uint32_t) * maxGaussians))
	, testUniformBuffer(Buffer::forUniform(context, sizeof(FrameUniforms)))
	, set0Layout(createSet0Layout(context))
	, set1Layout(createSet1Layout(context))
{
	std::array<vk::DescriptorPoolSize, 2> poolSizes =
	{
		vk::DescriptorPoolSize{ .type = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1 },
		vk::DescriptorPoolSize{ .type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 7 }
	};
	descriptorPool = vk::raii::DescriptorPool(context.getDevice(), vk::DescriptorPoolCreateInfo
	{
		.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets       = 2,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes    = poolSizes.data()
	});

	std::array<vk::DescriptorSetLayout, 2> allocLayouts = { *set0Layout, *set1Layout };
	auto sets = context.getDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo
	{
		.descriptorPool     = *descriptorPool,
		.descriptorSetCount = static_cast<uint32_t>(allocLayouts.size()),
		.pSetLayouts        = allocLayouts.data()
	});
	set0TestDescriptor = std::move(sets[0]);
	set1Descriptor     = std::move(sets[1]);

	vk::DescriptorBufferInfo uboInfo{ .buffer = *testUniformBuffer.getHandle(), .offset = 0, .range = sizeof(FrameUniforms) };
	std::array<vk::DescriptorBufferInfo, 7> ssboInfos =
	{
		vk::DescriptorBufferInfo{ .buffer = *inputGaussians.buffer,  .offset = 0, .range = inputGaussians.size },
		vk::DescriptorBufferInfo{ .buffer = *projectedBuffer.buffer, .offset = 0, .range = projectedBuffer.size },
		vk::DescriptorBufferInfo{ .buffer = *indirectBuffer.buffer,  .offset = 0, .range = indirectBuffer.size },
		vk::DescriptorBufferInfo{ .buffer = *sortKeysA.buffer,       .offset = 0, .range = sortKeysA.size },
		vk::DescriptorBufferInfo{ .buffer = *sortKeysB.buffer,       .offset = 0, .range = sortKeysB.size },
		vk::DescriptorBufferInfo{ .buffer = *sortIndicesA.buffer,    .offset = 0, .range = sortIndicesA.size },
		vk::DescriptorBufferInfo{ .buffer = *sortIndicesB.buffer,    .offset = 0, .range = sortIndicesB.size }
	};

	std::array<vk::WriteDescriptorSet, 8> writes{};
	writes[0] = vk::WriteDescriptorSet
	{
		.dstSet          = *set0TestDescriptor,
		.dstBinding      = 0,
		.descriptorCount = 1,
		.descriptorType  = vk::DescriptorType::eUniformBuffer,
		.pBufferInfo     = &uboInfo
	};
	for (uint32_t i = 0; i < ssboInfos.size(); ++i)
	{
		writes[i + 1] = vk::WriteDescriptorSet
		{
			.dstSet          = *set1Descriptor,
			.dstBinding      = i,
			.descriptorCount = 1,
			.descriptorType  = vk::DescriptorType::eStorageBuffer,
			.pBufferInfo     = &ssboInfos[i]
		};
	}
	context.getDevice().updateDescriptorSets(writes, {});

	vk::PushConstantRange pushRange
	{
		.stageFlags = vk::ShaderStageFlagBits::eCompute,
		.offset     = 0,
		.size       = sizeof(SplatComputePushConstants)
	};
	std::array<vk::DescriptorSetLayout, 2> setLayouts = { *set0Layout, *set1Layout };
	pipelineLayout = vk::raii::PipelineLayout(context.getDevice(), vk::PipelineLayoutCreateInfo
	{
		.setLayoutCount         = static_cast<uint32_t>(setLayouts.size()),
		.pSetLayouts            = setLayouts.data(),
		.pushConstantRangeCount = 1,
		.pPushConstantRanges    = &pushRange
	});

	projectPipeline = createComputePipeline(context, *pipelineLayout, AssetPaths::getShader("splat/project"));
	sortPipeline    = createComputePipeline(context, *pipelineLayout, AssetPaths::getShader("splat/radix_sort"));
}

void SplatComputePipeline::recordCompute(vk::raii::CommandBuffer const& cmd, vk::DescriptorSet set0Uniforms, uint32_t numGaussians) const
{
	// Reset DrawIndirectCommand on GPU: vertexCount = 6, instanceCount = 0, firstVertex = 0, firstInstance = 0
	DrawIndirectCommand resetCmd{};
	cmd.updateBuffer<DrawIndirectCommand>(*indirectBuffer.buffer, 0, resetCmd);

	computeMemoryBarrier(
		cmd,
		vk::PipelineStageFlagBits2::eTransfer,
		vk::AccessFlagBits2::eTransferWrite,
		vk::PipelineStageFlagBits2::eComputeShader,
		vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);

	std::array<vk::DescriptorSet, 2> sets = { set0Uniforms, *set1Descriptor };
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, sets, {});

	SplatComputePushConstants pc{ .numGaussians = numGaussians, .shift = 0 };
	cmd.pushConstants<SplatComputePushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, pc);

	uint32_t const groupCount = (numGaussians + 255) / 256;
	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *projectPipeline);
	cmd.dispatch(groupCount, 1, 1);

	computeMemoryBarrier(
		cmd,
		vk::PipelineStageFlagBits2::eComputeShader,
		vk::AccessFlagBits2::eShaderWrite,
		vk::PipelineStageFlagBits2::eComputeShader,
		vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);

	// Step 5 will plug the 4 radix sort passes in here.
	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *sortPipeline);
	cmd.dispatch(1, 1, 1);
}

void SplatComputePipeline::runSelfTest(VulkanContext const& context) const
{
	context.immediateSubmit([&](vk::raii::CommandBuffer const& cmd)
	{
		recordCompute(cmd, *set0TestDescriptor, 256);
	});

	auto cmdReadback = StorageBuffer::readback<DrawIndirectCommand>(context, indirectBuffer, 1);
	std::cout << "[P2 SelfTest] vertexCount=" << cmdReadback[0].vertexCount
	          << " instanceCount=" << cmdReadback[0].instanceCount << "\n";
}
