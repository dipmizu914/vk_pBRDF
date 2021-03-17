#pragma once
#define NVVK_ALLOC_DEDICATED
#include <queue>
#include <vulkan/vulkan.hpp>
#include <nvmath/nvmath.h>
#include <nvmath/nvmath_glsltypes.h>
#include "nvvk/allocator_vk.hpp"
#include "nvvk/appbase_vkpp.hpp"
#include "nvh/gltfscene.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/raytraceKHR_vk.hpp"

#include "../util.h"
#include "../shaders/headers/binding.glsl"
#include "../shaderIncludes.h"


using vkBU = vk::BufferUsageFlagBits;
using vkMP = vk::MemoryPropertyFlagBits;
using vkDS = vk::DescriptorSetLayoutBinding;
using vkDT = vk::DescriptorType;
using vkSS = vk::ShaderStageFlagBits;
using vkIU = vk::ImageUsageFlagBits;

class Descriptor {
public:
	void setup(const vk::Instance& instance, const vk::Device& device, const vk::PhysicalDevice& physicalDevice, uint32_t graphicsQueueIndex, nvvk::Allocator* allocator) {
		m_debug.setup(device);
		m_instance = instance;
		m_device = device;
		m_physicalDevice = physicalDevice;
		m_graphicsQueueIndex = graphicsQueueIndex;
		m_alloc = allocator;
		m_debug.setup(m_device);

	}
	virtual void createDescriptorSetLayout() {};
	virtual void updateDescriptorSet() {};

	virtual void destroy() {
		m_device.destroy(m_descSetLayout);
		m_device.destroy(m_descPool);
	};

	virtual bool renderUI() {
		return false;
	}

	vk::DescriptorSetLayout& getDescLayout() { return m_descSetLayout; }
	vk::DescriptorSet& getDescSet() { return m_descSet; }
protected:
	vk::Instance m_instance;
	vk::Device m_device;
	vk::PhysicalDevice m_physicalDevice;
	uint32_t m_graphicsQueueIndex;
	nvvk::Allocator* m_alloc;
	vk::Extent2D m_size;
	nvvk::DebugUtil          m_debug;


	vk::DescriptorSetLayout m_descSetLayout;
	vk::DescriptorSet       m_descSet;
	vk::DescriptorPool       m_descPool;
	nvvk::DescriptorSetBindings       m_descBind;
};