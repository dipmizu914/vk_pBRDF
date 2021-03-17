#pragma once

#include "pass.h"

#include <vulkan/vulkan.hpp>
#include "nvh/fileoperations.hpp"
#include "nvvk/shaders_vk.hpp"
#include "../descriptors/sceneBuffers.h"

class RasterizerPass : public Pass {
public:
	virtual void createRenderPass(vk::Extent2D outputSize) override;

	virtual void createPipeline(const std::vector<vk::DescriptorSetLayout>&) override;

	virtual void run(const vk::CommandBuffer& cmdBuf, const std::vector<vk::DescriptorSet>&) override;

	virtual void destroy() override;

private:
	

	vk::Framebuffer m_framebuffer;

	struct PushConstant {
		uint instanceId;
		uint materialId;
	} m_pushConstant;
};