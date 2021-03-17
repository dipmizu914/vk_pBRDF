#pragma once
//////////////////////////////////////////////////////////////////////////

#define NVVK_ALLOC_DEDICATED

#include "nvvk/allocator_vk.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/images_vk.hpp"
#include <array>
#include <vector>
#include <vulkan/vulkan.hpp>
#include "descriptors.h"

#include "../passes/raytracePass.h"
#include "../passes/rasterizerPass.h"

class OutputImage : public Descriptor {
public:

	void setPasses(const RayTracePass*);

	void createDescriptorSetLayout() override;
	void updateDescriptorSet() override;

	void destroy() override;
private:

	const RayTracePass* m_raytracePass;

};
