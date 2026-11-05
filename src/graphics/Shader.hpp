#pragma once

#include "Vulkan.hpp"
#include "VulkanContext.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

struct ShaderEntryPoint
{
	vk::ShaderStageFlagBits stage;
	std::string             name;   // e.g. "vertMain"
};

// One compiled SPIR-V module plus the entry points used from it. A Slang file
// can hold several entry points (vertex + fragment, or compute), so the stages
// are a list rather than one class per stage type.
class Shader
{
public:
	Shader(VulkanContext const& context, std::filesystem::path const& spirvPath, std::vector<ShaderEntryPoint> entryPoints);

	// The stage infos point into entryPoints' strings, so the object stays put.
	Shader(Shader const&) = delete;
	Shader& operator=(Shader const&) = delete;
	Shader(Shader&&) = delete;
	Shader& operator=(Shader&&) = delete;

	// Ready to plug into GraphicsPipelineCreateInfo::pStages.
	std::span<vk::PipelineShaderStageCreateInfo const> getStages() const { return stages; }

	// For a single stage, e.g. ComputePipelineCreateInfo::stage. Throws if the stage is missing.
	vk::PipelineShaderStageCreateInfo const& getStage(vk::ShaderStageFlagBits stage) const;
	bool hasStage(vk::ShaderStageFlagBits stage) const;

	vk::raii::ShaderModule const& getModule() const { return module; }

private:
	static std::vector<uint32_t> readSpirv(std::filesystem::path const& path);

	vk::raii::ShaderModule                         module = nullptr;
	std::vector<ShaderEntryPoint>                  entryPoints;
	std::vector<vk::PipelineShaderStageCreateInfo> stages;
};
