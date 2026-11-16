#include "GraphicsPipeline.hpp"

#include <array>
#include <cstdint>

GraphicsPipeline::GraphicsPipeline(VulkanContext const& context, Shader const& shader, GraphicsPipelineConfig const& config)
{
	bool const hasDepth = config.depthFormat != vk::Format::eUndefined;

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo
	{
		.vertexBindingDescriptionCount   = static_cast<uint32_t>(config.vertexBindings.size()),
		.pVertexBindingDescriptions      = config.vertexBindings.data(),
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(config.vertexAttributes.size()),
		.pVertexAttributeDescriptions    = config.vertexAttributes.data()
	};

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly
	{
		.topology = vk::PrimitiveTopology::eTriangleList
	};

	vk::PipelineViewportStateCreateInfo viewportState
	{
		.viewportCount = 1,
		.scissorCount = 1
	};

	vk::PipelineRasterizationStateCreateInfo rasterizer
	{
		.depthClampEnable = vk::False,
		.rasterizerDiscardEnable = vk::False,
		.polygonMode = vk::PolygonMode::eFill,
		.cullMode = config.cullMode,
		.frontFace = vk::FrontFace::eCounterClockwise,
		.depthBiasEnable = vk::False,
		.lineWidth = 1.0f
	};

	vk::PipelineMultisampleStateCreateInfo multisampling
	{
		.rasterizationSamples = vk::SampleCountFlagBits::e1,
		.sampleShadingEnable = vk::False
	};

	vk::PipelineDepthStencilStateCreateInfo depthStencil
	{
		.depthTestEnable = hasDepth ? vk::True : vk::False,
		.depthWriteEnable = hasDepth ? vk::True : vk::False,
		.depthCompareOp = vk::CompareOp::eLess, // Closer fragments win; clear depth to 1.0.
		.depthBoundsTestEnable = vk::False,
		.stencilTestEnable = vk::False
	};

	vk::PipelineColorBlendAttachmentState colorBlendAttachment
	{
		.blendEnable = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	};

	vk::PipelineColorBlendStateCreateInfo colorBlending
	{
		.logicOpEnable = vk::False,
		.logicOp = vk::LogicOp::eCopy,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment
	};

	std::array dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

	vk::PipelineDynamicStateCreateInfo dynamicState
	{
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()
	};

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo
	{
		.setLayoutCount = static_cast<uint32_t>(config.descriptorSetLayouts.size()),
		.pSetLayouts = config.descriptorSetLayouts.data(),
		.pushConstantRangeCount = 0
	};

	pipelineLayout = vk::raii::PipelineLayout(context.getDevice(), pipelineLayoutInfo);

	vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain =
	{
		{
			.stageCount = static_cast<uint32_t>(shader.getStages().size()),
			.pStages = shader.getStages().data(),
			.pVertexInputState = &vertexInputInfo,
			.pInputAssemblyState = &inputAssembly,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizer,
			.pMultisampleState = &multisampling,
			.pDepthStencilState = &depthStencil,
			.pColorBlendState = &colorBlending,
			.pDynamicState = &dynamicState,
			.layout = pipelineLayout,
			.renderPass = nullptr
		},
		{
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &config.colorFormat,
			.depthAttachmentFormat = config.depthFormat
		}
	};

	pipeline = vk::raii::Pipeline(context.getDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}
