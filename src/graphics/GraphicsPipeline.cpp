#include "GraphicsPipeline.hpp"

#include "Vertex.hpp"

#include <vector>

GraphicsPipeline::GraphicsPipeline(VulkanContext const& context, Shader const& shader, vk::Format colorFormat, vk::Format depthFormat)
{
	createDescriptorSetLayout(context);

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo
	{
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.data()
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
		.cullMode = vk::CullModeFlagBits::eBack,
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
		.depthTestEnable = vk::True,
		.depthWriteEnable = vk::True,
		.depthCompareOp = vk::CompareOp::eLess, // Closer fragments win; the buffer is cleared to 1.0.
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

	std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

	vk::PipelineDynamicStateCreateInfo dynamicState
	{
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data()
	};

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo
	{
		.setLayoutCount = 1,
		.pSetLayouts = &*descriptorSetLayout,
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
			.pColorAttachmentFormats = &colorFormat,
			.depthAttachmentFormat = depthFormat
		}
	};

	pipeline = vk::raii::Pipeline(context.getDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

void GraphicsPipeline::createDescriptorSetLayout(VulkanContext const& context)
{
	std::array<vk::DescriptorSetLayoutBinding, 2> bindings
	{
		{ 
			{ 
				.binding = 0, 
				.descriptorType = vk::DescriptorType::eUniformBuffer, 
				.descriptorCount = 1, 
				.stageFlags = vk::ShaderStageFlagBits::eVertex 
			},
			{ 
				.binding = 1, 
				.descriptorType = vk::DescriptorType::eCombinedImageSampler, 
				.descriptorCount = 1, 
				.stageFlags = vk::ShaderStageFlagBits::eFragment 
			} 
		} 
	};

	vk::DescriptorSetLayoutCreateInfo layoutInfo
	{
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};

	descriptorSetLayout = vk::raii::DescriptorSetLayout(context.getDevice(), layoutInfo);
}
