#include "Shader.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

Shader::Shader(VulkanContext const& context, std::filesystem::path const& spirvPath, std::vector<ShaderEntryPoint> entryPoints)
	: entryPoints(std::move(entryPoints))
{
	if (this->entryPoints.empty())
	{
		throw std::runtime_error("shader has no entry points: " + spirvPath.string());
	}

	std::vector<uint32_t> code = readSpirv(spirvPath);

	vk::ShaderModuleCreateInfo createInfo
	{
		.codeSize = code.size() * sizeof(uint32_t),
		.pCode = code.data()
	};

	module = vk::raii::ShaderModule(context.getDevice(), createInfo);

	// Built only after entryPoints is final, since pName points into its strings.
	stages.reserve(this->entryPoints.size());
	for (ShaderEntryPoint const& entryPoint : this->entryPoints)
	{
		stages.push_back(vk::PipelineShaderStageCreateInfo
		{
			.stage = entryPoint.stage,
			.module = *module,
			.pName = entryPoint.name.c_str()
		});
	}
}

vk::PipelineShaderStageCreateInfo const& Shader::getStage(vk::ShaderStageFlagBits stage) const
{
	auto const stageIt = std::ranges::find(stages, stage, &vk::PipelineShaderStageCreateInfo::stage);
	if (stageIt == stages.end())
	{
		throw std::runtime_error("shader has no " + vk::to_string(stage) + " stage");
	}
	return *stageIt;
}

bool Shader::hasStage(vk::ShaderStageFlagBits stage) const
{
	return std::ranges::find(stages, stage, &vk::PipelineShaderStageCreateInfo::stage) != stages.end();
}

std::vector<uint32_t> Shader::readSpirv(std::filesystem::path const& path)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		throw std::runtime_error("failed to open file: " + path.string());
	}

	auto const fileSize = static_cast<size_t>(file.tellg());
	if (fileSize == 0 || fileSize % sizeof(uint32_t) != 0)
	{
		throw std::runtime_error("invalid SPIR-V file size: " + path.string());
	}

	// Read straight into 32-bit words, so pCode is correctly aligned.
	std::vector<uint32_t> code(fileSize / sizeof(uint32_t));
	file.seekg(0, std::ios::beg);
	file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(fileSize));

	return code;
}
