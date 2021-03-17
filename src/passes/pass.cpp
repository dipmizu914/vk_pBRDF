#include "pass.h"

void Pass::setup(const vk::Instance& instance, const vk::Device& device, const vk::PhysicalDevice& physicalDevice, uint32_t graphicsQueueIndex, nvvk::Allocator* allocator) {
	m_instance = instance;
	m_device = device;
	m_physicalDevice = physicalDevice;
	m_graphicsQueueIndex = graphicsQueueIndex;
	m_alloc = allocator;
};

void Pass::setGeometry(const nvh::GltfScene* gltfScene, const SceneBuffers* sceneBuffers) {
	m_gltfScene = gltfScene;
	m_sceneBuffers = sceneBuffers;
}

void Pass::createDescriptorSet() {}

void Pass::createPipeline(const std::vector<vk::DescriptorSetLayout>&){}

bool Pass::renderUI() { return false; }

void Pass::createRenderPass(vk::Extent2D outputSize) { m_size = outputSize; }

void Pass::run(const vk::CommandBuffer& cmdBuf, const std::vector<vk::DescriptorSet>&) {}

void Pass::destroy() {
	m_device.destroy(m_renderPass);
	m_device.destroy(m_pipelineLayout);
	m_device.destroy(m_pipeline);
}