#pragma once
#include <vulkan/vulkan.hpp>

#define NVVK_ALLOC_DEDICATED
#include "nvvk/allocator_vk.hpp"
#include "nvvk/appbase_vkpp.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/context_vk.hpp"
#include "nvvk/profiler_vk.hpp"

// #VKRay
#include "nvh/gltfscene.hpp"
#include "nvvk/raytraceKHR_vk.hpp"

#include "descriptors/sceneBuffers.h"
#include "descriptors/environmentalLight.hpp"
#include "descriptors/outputImages.h"

#include "util.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"

#include "passes/rasterizerPass.h"
#include "passes/raytracePass.h"
#include "passes/computePass.h"

class App : public nvvk::AppBase
{
public:
	constexpr static std::size_t numGBuffers = 2;
	App() {};
	~App() {};
	void setup(const vk::Instance& instance,
		const vk::Device& device,
		const vk::PhysicalDevice& physicalDevice,
		uint32_t                  queueFamily) override;
	void createScene(std::string scene);
	void render();
	void destroyResources();

private:
	nvvk::AllocatorDedicated m_alloc;
	nvvk::DebugUtil          m_debug;


	//Resources
	nvh::GltfScene m_gltfScene;
	tinygltf::Model m_tmodel;


	//Uniform Buffer for all pass
	//Constants

	bool m_enableNNE= false;
	int m_maxFrames = 3000;
	int m_curFrame = 0;

	shader::PushConstant m_pushC;
	shader::SceneUniforms m_sceneUniforms;
	nvvk::Buffer               m_sceneUniformBuffer;

	vk::DescriptorPool          m_descStaticPool;
	nvvk::DescriptorSetBindings m_sceneSetLayoutBind;
	vk::DescriptorSetLayout     m_sceneSetLayout;
	vk::DescriptorSet           m_sceneSet;

	//Pipeline
	vk::Pipeline                m_postPipeline;
	vk::PipelineLayout          m_postPipelineLayout;

	vk::CommandBuffer m_mainCommandBuffer;
	vk::Fence m_mainFence;

	//Descriptors
	std::vector<Descriptor*> m_descriptors;
	EnvironmentalLight m_environmentalLight;
	SceneBuffers m_sceneBuffers;
	OutputImage m_outputImages;

	//Pass
	std::vector<Pass*> m_passes;
	//RasterizerPass m_rasterizerPass;
	RayTracePass m_raytracePass;
	ComputePass m_computePass;


	void _createDescriptorPool();
	void _createUniformBuffer();
	void _createDescriptorSet();
	void _createPostPipeline();
	void _createMainCommandBuffer();

	void _updateUniformBuffer(const vk::CommandBuffer& cmdBuf);

	void _drawPost(vk::CommandBuffer cmdBuf);
	void _renderUI();
	void _submitMainCommand();

	void _updateFrame();
	void _resetFrame();

	void onResize(int /*w*/, int /*h*/) override;
};
