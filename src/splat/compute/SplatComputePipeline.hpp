// P2
#pragma once

#include "graphics/gpu/Buffer.hpp"
#include "graphics/vulkan/Vulkan.hpp"
#include "graphics/vulkan/VulkanContext.hpp"
#include "splat/SplatTypes.h"
#include "splat/compute/ComputeUtils.hpp"

#include <cstdint>

class SplatComputePipeline
{
public:
	SplatComputePipeline(VulkanContext const& context, uint32_t maxGaussians);

	SplatComputePipeline(SplatComputePipeline const&) = delete;
	SplatComputePipeline& operator=(SplatComputePipeline const&) = delete;
	SplatComputePipeline(SplatComputePipeline&&) = delete;
	SplatComputePipeline& operator=(SplatComputePipeline&&) = delete;

	// Records reset -> project -> radix sort into `cmd` (Set 0 contains FrameUniforms at binding 0).
	void recordCompute(vk::raii::CommandBuffer const& cmd, vk::DescriptorSet set0Uniforms, uint32_t numGaussians) const;

	// Standalone Phase 1-3 verification: runs a GPU dispatch + readback validation check.
	void runSelfTest(VulkanContext const& context) const;

	// Outputs consumed later by P3's billboard graphics pass:
	vk::Buffer getProjectedBuffer() const { return *projectedBuffer.buffer; }
	vk::Buffer getSortedIndicesBuffer() const { return *sortIndicesA.buffer; }
	vk::Buffer getIndirectBuffer() const { return *indirectBuffer.buffer; }

private:
	uint32_t maxGaussians = 0;

	// GPU buffers (Set 1)
	StorageBuffer inputGaussians;  // binding 0
	StorageBuffer projectedBuffer; // binding 1
	StorageBuffer indirectBuffer;  // binding 2
	StorageBuffer sortKeysA;       // binding 3
	StorageBuffer sortKeysB;       // binding 4
	StorageBuffer sortIndicesA;    // binding 5
	StorageBuffer sortIndicesB;    // binding 6

	// Standalone Set 0 uniform buffer (used by runSelfTest before RenderFeature is hooked up)
	Buffer testUniformBuffer;

	vk::raii::DescriptorSetLayout set0Layout = nullptr;
	vk::raii::DescriptorSetLayout set1Layout = nullptr;
	vk::raii::DescriptorPool      descriptorPool = nullptr;
	vk::raii::DescriptorSet       set0TestDescriptor = nullptr;
	vk::raii::DescriptorSet       set1Descriptor = nullptr;

	vk::raii::PipelineLayout pipelineLayout = nullptr;
	vk::raii::Pipeline       projectPipeline = nullptr;
	vk::raii::Pipeline       sortPipeline = nullptr;
};
