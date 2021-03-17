#pragma once

#include "pass.h"

#include <vulkan/vulkan.hpp>

#include "../util.h"
#include "nvh/fileoperations.hpp"
#include "nvvk/shaders_vk.hpp"


#include "nvvk/raytraceKHR_vk.hpp"
#include "nvh/alignment.hpp"

class RayTracePass : public Pass {
public:
	virtual void setup(const vk::Instance& instance, const vk::Device& device, const vk::PhysicalDevice& physicalDevice, uint32_t graphicsQueueIndex, nvvk::Allocator* allocator) override;

	virtual void createRenderPass(vk::Extent2D outputSize) override;
	virtual void createPipeline(const std::vector<vk::DescriptorSetLayout>&) override;

	virtual void run(const vk::CommandBuffer& cmdBuf, const std::vector<vk::DescriptorSet>&) override;

	virtual void destroy() override;

	enum ERenderResult
	{
		eColor,
		eStorksStatus,
		ePathTrace,
		last_elem
	};

	const nvvk::Texture& getRenderResult(ERenderResult e) const { return m_renderResults[e]; };
private:
	vk::PhysicalDeviceRayTracingPipelinePropertiesKHR   m_rtProperties;
	std::vector<vk::RayTracingShaderGroupCreateInfoKHR> m_rtShaderGroups;

	nvvk::Buffer m_SBTBuffer;

	std::array<nvvk::Texture, ERenderResult::last_elem> m_renderResults;

	void _createShaderBindingTable();

};