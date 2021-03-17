#pragma once

#include "pass.h"

#include <vulkan/vulkan.hpp>

#include "../util.h"
#include "nvh/fileoperations.hpp"
#include "nvvk/shaders_vk.hpp"
#include "../descriptors/sceneBuffers.h"

#include "nvvk/raytraceKHR_vk.hpp"
#include "nvh/alignment.hpp"

class ComputePass : public Pass {
public:
	virtual void createPipeline(const std::vector<vk::DescriptorSetLayout>&) override;

	virtual void run(const vk::CommandBuffer& cmdBuf, const std::vector<vk::DescriptorSet>&) override;

};