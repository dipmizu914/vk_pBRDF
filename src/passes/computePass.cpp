#include "computePass.h"


extern std::vector<std::string> defaultSearchPaths;

void ComputePass::createPipeline(const std::vector<vk::DescriptorSetLayout>& descLayouts) {
	vk::PipelineLayoutCreateInfo layout_info;

	layout_info.setSetLayouts(descLayouts);
	m_pipelineLayout = m_device.createPipelineLayout(layout_info);
	vk::ComputePipelineCreateInfo computePipelineCreateInfo{ {}, {}, m_pipelineLayout };

	computePipelineCreateInfo.stage = nvvk::createShaderStageInfo(
		m_device, nvh::loadFile("src/shaders/compute.comp.spv", true, defaultSearchPaths, true),
		VK_SHADER_STAGE_COMPUTE_BIT);
	m_pipeline = static_cast<const vk::Pipeline&>(
		m_device.createComputePipeline({}, computePipelineCreateInfo));
	m_device.destroy(computePipelineCreateInfo.stage.module);
}

void ComputePass::run(const vk::CommandBuffer& cmdBuf, const std::vector<vk::DescriptorSet>& dsecSet) {
	cmdBuf.pipelineBarrier(
		vk::PipelineStageFlagBits::eComputeShader,
		vk::PipelineStageFlagBits::eComputeShader,
		{}, {}, {}, {}
	);

	cmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline);
	cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipelineLayout, 0,
		dsecSet, {});
	cmdBuf.dispatch(m_size.width, m_size.height, 1);
}