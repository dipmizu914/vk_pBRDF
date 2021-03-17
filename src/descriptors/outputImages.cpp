#include "outputImages.h"


void OutputImage::createDescriptorSetLayout() {
	using vkDT = vk::DescriptorType;
	using vkSS = vk::ShaderStageFlagBits;
	using vkDSLB = vk::DescriptorSetLayoutBinding;
	auto flag = vkSS::eRaygenKHR | vkSS::eClosestHitKHR | vkSS::eAnyHitKHR | vkSS::eMissKHR | vkSS::eCompute | vkSS::eVertex | vkSS::eFragment;

	auto& bind = m_descBind;
	bind.addBinding(vkDSLB(B_RAYTRACE_RESULT, vkDT::eCombinedImageSampler, 1, flag));
	bind.addBinding(vkDSLB(B_RAYTRACE_STORAGE, vkDT::eStorageImage, 1, flag));
	bind.addBinding(vkDSLB(B_STORKS_STATUS, vkDT::eCombinedImageSampler, 1, flag));
	bind.addBinding(vkDSLB(B_STORKS_STATUS_STORAGE, vkDT::eStorageImage, 1, flag));
	bind.addBinding(vkDSLB(B_PATHTRACE_RESULT, vkDT::eCombinedImageSampler, 1, flag));
	bind.addBinding(vkDSLB(B_PATHTRACE_STORAGE, vkDT::eStorageImage, 1, flag));

	m_descSetLayout = bind.createLayout(m_device);
	m_descPool = bind.createPool(m_device);
	m_descSet = nvvk::allocateDescriptorSet(m_device, m_descPool, m_descSetLayout);
}

void OutputImage::setPasses(const RayTracePass* rtPass) {
	m_raytracePass = rtPass;
}

void OutputImage::updateDescriptorSet() {
	auto& bind = m_descBind;
	using ERenderResult = RayTracePass::ERenderResult;
	std::vector<vk::WriteDescriptorSet> writes;
	const vk::DescriptorImageInfo& w1 = m_raytracePass->getRenderResult(ERenderResult::eColor).descriptor;
	const vk::DescriptorImageInfo& w2 = m_raytracePass->getRenderResult(ERenderResult::eStorksStatus).descriptor;
	const vk::DescriptorImageInfo& w3 = m_raytracePass->getRenderResult(ERenderResult::ePathTrace).descriptor;

	writes.emplace_back(bind.makeWrite(m_descSet, B_RAYTRACE_RESULT, &w1));
	writes.emplace_back(bind.makeWrite(m_descSet, B_RAYTRACE_STORAGE, &w1));
	writes.emplace_back(bind.makeWrite(m_descSet, B_STORKS_STATUS, &w2));
	writes.emplace_back(bind.makeWrite(m_descSet, B_STORKS_STATUS_STORAGE, &w2));
	writes.emplace_back(bind.makeWrite(m_descSet, B_PATHTRACE_RESULT, &w3));
	writes.emplace_back(bind.makeWrite(m_descSet, B_PATHTRACE_STORAGE, &w3));

	m_device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void OutputImage::destroy() {

}