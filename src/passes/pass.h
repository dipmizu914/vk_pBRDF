#pragma once
#define NVVK_ALLOC_DEDICATED
#include <vulkan/vulkan.hpp>
#include "nvh/fileoperations.hpp"
#include "nvvk/shaders_vk.hpp"
#include "nvvk/raytraceKHR_vk.hpp"
#include "nvh/alignment.hpp"
#include "nvvk/allocator_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"

#include "../descriptors/sceneBuffers.h"

class Pass {
public:

	const nvvk::Texture getColorOutput() const { return m_colorImage; }

	virtual void setup(const vk::Instance& instance, const vk::Device& device, const vk::PhysicalDevice& physicalDevice, uint32_t graphicsQueueIndex, nvvk::Allocator* allocator);
	virtual void setGeometry(const nvh::GltfScene*, const SceneBuffers*);

	virtual void createDescriptorSet();
	virtual void createRenderPass(vk::Extent2D outputSize);
	virtual void createPipeline(const std::vector<vk::DescriptorSetLayout>&) ;

	virtual bool renderUI();
	virtual void run(const vk::CommandBuffer& cmdBuf, const std::vector<vk::DescriptorSet>&);

	virtual void destroy();

protected:
	vk::Instance m_instance;
	vk::Device m_device;
	vk::PhysicalDevice m_physicalDevice;
	uint32_t m_graphicsQueueIndex;
	nvvk::Allocator* m_alloc;
	vk::Extent2D m_size;

	vk::PipelineLayout m_pipelineLayout;
	vk::Pipeline     m_pipeline;

	vk::RenderPass     m_renderPass;

	nvvk::Texture m_colorImage, m_depthImage; //outPut
	vk::Format                  m_offscreenColorFormat{ vk::Format::eR32G32B32A32Sfloat };
	vk::Format                  m_offscreenDepthFormat{ vk::Format::eD32Sfloat };

	const nvh::GltfScene* m_gltfScene = nullptr;
	const SceneBuffers* m_sceneBuffers = nullptr;
};