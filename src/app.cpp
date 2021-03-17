#include <sstream>
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE


#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

extern std::vector<std::string> defaultSearchPaths;
extern std::string environmentalTextureFile;


extern int const SAMPLE_WIDTH = 1024;
extern int const SAMPLE_HEIGHT = 1024;


#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION

#include "app.h"
#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvh/gltfscene.hpp"
#include "nvh/nvprint.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/renderpasses_vk.hpp"
#include "nvvk/shaders_vk.hpp"
#include "nvvk/context_vk.hpp"

#include "nvh/alignment.hpp"
#include "shaders/headers/binding.glsl"


void App::setup(const vk::Instance& instance,
	const vk::Device& device,
	const vk::PhysicalDevice& physicalDevice,
	uint32_t                  queueFamily)
{
	AppBase::setup(instance, device, physicalDevice, queueFamily);
	m_alloc.init(device, physicalDevice);
	m_debug.setup(m_device);

	m_raytracePass.setup(instance, device, physicalDevice, queueFamily, &m_alloc);
	//m_rasterizerPass.setup(instance, device, physicalDevice, queueFamily, &m_alloc);
	//m_computePass.setup(instance, device, physicalDevice, queueFamily, &m_alloc);

	m_passes.push_back(&m_raytracePass);
	//m_passes.push_back(&m_rasterizerPass);
	//m_passes.push_back(&m_computePass);

	m_environmentalLight.setup(instance, device, physicalDevice, queueFamily, &m_alloc);
	m_sceneBuffers.setup(instance, device, physicalDevice, queueFamily, &m_alloc);
	m_outputImages.setup(instance, device, physicalDevice, queueFamily, &m_alloc);

	m_descriptors.push_back(&m_sceneBuffers);
	m_descriptors.push_back(&m_environmentalLight);
	m_descriptors.push_back(&m_outputImages);
}

void App::createScene(std::string scene) {
	std::string filename = nvh::findFile(scene, defaultSearchPaths);

	_createDescriptorPool();

	m_sceneBuffers.create(filename);
	m_sceneBuffers.createDescriptorSetLayout();
	m_sceneBuffers.updateDescriptorSet();


	m_environmentalLight.loadEnvironment(nvh::findFile(environmentalTextureFile, defaultSearchPaths));
	m_environmentalLight.createDescriptorSetLayout();
	m_environmentalLight.updateDescriptorSet();

	m_outputImages.setPasses(&m_raytracePass);
	m_outputImages.createDescriptorSetLayout();


	const float aspectRatio = m_size.width / static_cast<float>(m_size.height);
	m_sceneUniforms.prevFrameProjectionViewMatrix = CameraManip.getMatrix() * nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.1f, 1000.0f);

	_createUniformBuffer();
	_createDescriptorSet();

	vk::Extent2D size{ SAMPLE_WIDTH ,SAMPLE_HEIGHT };

	LOGI("Create Ray Trace Pass\n");
	m_raytracePass.setGeometry(&m_sceneBuffers.getScene(), &m_sceneBuffers);
	m_raytracePass.createRenderPass(size);
	m_raytracePass.createPipeline({ m_sceneSetLayout, m_sceneBuffers.getDescLayout(),  m_outputImages.getDescLayout(), m_environmentalLight.getDescLayout() });


	//LOGI("Create Rasterize Pass\n");
	//m_rasterizerPass.setGeometry(&m_sceneBuffers.getScene(), &m_sceneBuffers);
	//m_rasterizerPass.createRenderPass(m_size);
	//m_rasterizerPass.createPipeline({ m_sceneSetLayout, m_sceneBuffers.getDescLayout(),  m_outputImages.getDescLayout(), m_environmentalLight.getDescLayout() });

	//LOGI("Create Compute Pass\n");
	//m_computePass.setGeometry(&m_sceneBuffers.getScene(), &m_sceneBuffers);
	//m_computePass.createRenderPass(size);
	//m_computePass.createPipeline({ m_sceneSetLayout, m_sceneBuffers.getDescLayout(),  m_outputImages.getDescLayout(), m_environmentalLight.getDescLayout() });

	createDepthBuffer();
	createRenderPass();
	initGUI(0);
	createFrameBuffers();
	_createPostPipeline();

	m_outputImages.updateDescriptorSet();

	m_pushC.initialize = 1;
	_createMainCommandBuffer();

	m_device.waitIdle();
	LOGI("Prepared\n");


}

void App::_renderUI() {
	bool changed = false;
	if (showGui())
	{
		using GuiH = ImGuiH::Control;
		auto Normal = ImGuiH::Control::Flags::Normal;

		ImGuiH::Panel::Begin();

		changed |= GuiH::Selection("Debug Mode", "Display unique values of material", &m_sceneUniforms.debugging_mode, nullptr, Normal,
			{ "No Debug", "BaseColor", "Normal", "Metallic", "Emissive", "Alpha", "Roughness",
			 "Textcoord", "Tangent", "Radiance", "Weight", "RayDir" });
	/*	changed |= GuiH::Selection("Present Mode", "Present Mode", &m_sceneUniforms.present_mode, nullptr, Normal,
			{ "s0","DoP","ALoP","ToP", "Path Tracing" });*/
		if (ImGui::CollapsingHeader("Ray Tracing", ImGuiTreeNodeFlags_DefaultOpen)) {
			changed |= GuiH::Slider("Max Ray Depth", "", &m_sceneUniforms.maxDepth, nullptr, Normal, 1, 30);
			changed |= GuiH::Slider("Samples Per Frame", "", &m_sceneUniforms.maxSamples, nullptr, Normal, 1, 10);
			changed |= GuiH::Slider("Max Iteration ", "", &m_maxFrames, nullptr, Normal, 1, 10000);
		}
		if (ImGui::CollapsingHeader("Tone Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
			changed |= GuiH::Slider("Exposure", "", &m_sceneUniforms.exposure, nullptr, Normal, 1.0f, 10.0f);
			changed |= GuiH::Slider("Gamma", "", &m_sceneUniforms.gamma, nullptr, Normal, 1.0f, 10.0f);
		}

		if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
			changed |= GuiH::Slider("Intensity of the Light (s0)", "", &m_sceneUniforms.lightStorks[0], nullptr,GuiH::Flags::Normal, 0.f, 5.f);
			
			float s1Max = sqrt(std::max(pow(m_sceneUniforms.lightStorks[0],2.0f)- pow(m_sceneUniforms.lightStorks[2], 2.0f)- pow(m_sceneUniforms.lightStorks[3], 2.0f),0.001f));
			float s2Max = sqrt(std::max(pow(m_sceneUniforms.lightStorks[0], 2.0f) - pow(m_sceneUniforms.lightStorks[1], 2.0f) - pow(m_sceneUniforms.lightStorks[3], 2.0f), 0.001f));
			float s3Max = sqrt(std::max(pow(m_sceneUniforms.lightStorks[0], 2.0f) - pow(m_sceneUniforms.lightStorks[1], 2.0f) - pow(m_sceneUniforms.lightStorks[2], 2.0f), 0.001f));
			changed |= GuiH::Slider("Intensity of the s1", "", &m_sceneUniforms.lightStorks[1], nullptr, GuiH::Flags::Normal, -s1Max, s1Max);
			changed |= GuiH::Slider("Intensity of the s2", "", &m_sceneUniforms.lightStorks[2], nullptr, GuiH::Flags::Normal, -s2Max, s2Max);
			changed |= GuiH::Slider("Intensity of the s3", "", &m_sceneUniforms.lightStorks[3], nullptr, GuiH::Flags::Normal,-s3Max, s3Max);

		}
		//changed |= ImGui::Checkbox("Use Next Event Estimation", &m_enableNNE);

		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
		{
			changed |= ImGuiH::CameraWidget();
		}

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
			1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGuiH::Control::Info("", "", "(F10) Toggle Pane", ImGuiH::Control::Flags::Disabled);
		if (changed) {
			_resetFrame();
		}
		ImGuiH::Panel::End();
	}

}


void App::render() {
	_updateFrame();
	m_queue.waitIdle();

	{
		const vk::CommandBuffer& cmdBuf = m_mainCommandBuffer;
		cmdBuf.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
		_updateUniformBuffer(cmdBuf);
		if (m_curFrame < m_maxFrames) {
			m_raytracePass.run(cmdBuf, { m_sceneSet, m_sceneBuffers.getDescSet(),  m_outputImages.getDescSet(), m_environmentalLight.getDescSet() });
		}
		//m_rasterizerPass.run(cmdBuf, { m_sceneSet, m_sceneBuffers.getDescSet(),  m_outputImages.getDescSet(), m_environmentalLight.getDescSet() });
		//m_computePass.run(cmdBuf, { m_sceneSet, m_sceneBuffers.getDescSet(),  m_outputImages.getDescSet(), m_environmentalLight.getDescSet() });
		cmdBuf.end();
		_submitMainCommand();

	}

	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	_renderUI();
	prepareFrame();

	auto                     curFrame = getCurFrame();
	const vk::CommandBuffer& cmdBuf = getCommandBuffers()[curFrame];
	cmdBuf.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
	{

		vk::ClearValue clearValues[2];
		clearValues[0].setColor(
			std::array<float, 4>({ 0.0,0.0,0.0,0.0 }));
		clearValues[1].setDepthStencil({ 1.0f, 0 });

		vk::RenderPassBeginInfo postRenderPassBeginInfo;
		postRenderPassBeginInfo.setClearValueCount(2);
		postRenderPassBeginInfo.setPClearValues(clearValues);
		postRenderPassBeginInfo.setRenderPass(getRenderPass());
		postRenderPassBeginInfo.setFramebuffer(getFramebuffers()[curFrame]);
		postRenderPassBeginInfo.setRenderArea({ {}, getSize() });

		cmdBuf.beginRenderPass(postRenderPassBeginInfo, vk::SubpassContents::eInline);
		cmdBuf.pushConstants<shader::PushConstant>(m_postPipelineLayout,
			vk::ShaderStageFlagBits::eFragment,
			0, m_pushC);
		// Rendering tonemapper
		_drawPost(cmdBuf);
		// Rendering UI
		ImGui::Render();
		ImGui::RenderDrawDataVK(cmdBuf, ImGui::GetDrawData());
		ImGui::EndFrame();
		cmdBuf.endRenderPass();
	}
	// Submit for display
	cmdBuf.end();
	submitFrame();

}

//--------------------------------------------------------------------------------------------------
// Destroying all allocations
//
void App::destroyResources()
{

	m_device.destroy(m_sceneSetLayout);
	m_alloc.destroy(m_sceneUniformBuffer);

	m_device.destroy(m_descStaticPool);

	m_device.destroy(m_postPipeline);
	m_device.destroy(m_postPipelineLayout);

	m_device.destroy(m_mainFence);

	for (auto& p : m_passes) {
		p->destroy();
	}
	for (auto& d : m_descriptors) {
		d->destroy();
	}
	m_alloc.deinit();

}

//--------------------------------------------------------------------------------------------------


void App::_createDescriptorPool() {
	// create  D Pool

	uint32_t maxSets = 100;
	using vkDT = vk::DescriptorType;
	using vkDP = vk::DescriptorPoolSize;

	std::vector<vkDP> staticPoolSizes{
		vkDP(vkDT::eCombinedImageSampler, maxSets),
		vkDP(vkDT::eStorageBuffer, maxSets),
		vkDP(vkDT::eUniformBuffer, maxSets),
		vkDP(vkDT::eUniformBufferDynamic, maxSets),
		vkDP(vkDT::eAccelerationStructureKHR, maxSets),
		vkDP(vkDT::eStorageImage, maxSets)
	};
	m_descStaticPool = nvvk::createDescriptorPool(m_device, staticPoolSizes, maxSets);

}

void App::_createUniformBuffer()
{
	using vkBU = vk::BufferUsageFlagBits;
	using vkMP = vk::MemoryPropertyFlagBits;


	m_sceneUniforms.debugging_mode = 0;
	m_sceneUniforms.present_mode = 0;

	m_sceneUniforms.gamma = 2.2;
	m_sceneUniforms.exposure = 1.0;

	m_sceneUniforms.screenSize = nvmath::uvec2(m_size.width, m_size.height);

	m_sceneUniforms.maxSamples = 10;
	m_sceneUniforms.maxDepth = 5;
	m_sceneUniforms.fireflyClampThreshold = 2.0;

	m_sceneUniforms.environment_intensity_factor = 1.0;

	m_sceneUniforms.lightStorks = nvmath::vec4(1.0f,0.0f,0.0f,0.0f);
	m_sceneUniforms.aliasTableCount = m_sceneBuffers.getAliasTableCount();

	m_sceneUniformBuffer = m_alloc.createBuffer(sizeof(shader::SceneUniforms),
		vkBU::eUniformBuffer | vkBU::eTransferDst, vkMP::eDeviceLocal);
	m_debug.setObjectName(m_sceneUniformBuffer.buffer, "sceneBuffer");



	nvvk::CommandPool cmdBufGet(m_device, m_graphicsQueueIndex);
	vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();

	_updateUniformBuffer(cmdBuf);

	cmdBufGet.submitAndWait(cmdBuf);
	m_alloc.finalizeAndReleaseStaging();

}
void App::_createDescriptorSet()
{
	using vkDS = vk::DescriptorSetLayoutBinding;
	using vkDT = vk::DescriptorType;
	using vkSS = vk::ShaderStageFlagBits;
	std::vector<vk::WriteDescriptorSet> writes;
	auto flag = vkSS::eRaygenKHR | vkSS::eClosestHitKHR | vkSS::eAnyHitKHR | vkSS::eMissKHR | vkSS::eCompute | vkSS::eVertex | vkSS::eFragment;

	m_sceneSetLayoutBind.addBinding(vkDS(B_SCENE, vkDT::eUniformBuffer, 1, flag));
	m_sceneSetLayout = m_sceneSetLayoutBind.createLayout(m_device);
	m_sceneSet = nvvk::allocateDescriptorSet(m_device, m_descStaticPool, m_sceneSetLayout);
	vk::DescriptorBufferInfo dbiUnif{ m_sceneUniformBuffer.buffer, 0, VK_WHOLE_SIZE };
	writes.emplace_back(m_sceneSetLayoutBind.makeWrite(m_sceneSet, B_SCENE, &dbiUnif));
	m_device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

}

//--------------------------------------------------------------------------------------------------
// The pipeline is how things are rendered, which shaders, type of primitives, depth test and more
//
void App::_createPostPipeline()
{
	// Push constants in the fragment shader
	vk::PushConstantRange pushConstantRanges = { vk::ShaderStageFlagBits::eFragment, 0, sizeof(shader::PushConstant) };

	// Creating the pipeline layout
	std::vector<vk::DescriptorSetLayout> layouts = { m_sceneSetLayout, m_sceneBuffers.getDescLayout(),  m_outputImages.getDescLayout(), m_environmentalLight.getDescLayout() };
	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	//pipelineLayoutCreateInfo.setSetLayoutCount(1);
	pipelineLayoutCreateInfo.setSetLayouts(layouts);
	pipelineLayoutCreateInfo.setPushConstantRangeCount(1);
	pipelineLayoutCreateInfo.setPPushConstantRanges(&pushConstantRanges);
	m_postPipelineLayout = m_device.createPipelineLayout(pipelineLayoutCreateInfo);

	// Pipeline: completely generic, no vertices
	std::vector<std::string> paths = defaultSearchPaths;

	nvvk::GraphicsPipelineGeneratorCombined pipelineGenerator(m_device, m_postPipelineLayout,
		m_renderPass);
	pipelineGenerator.addShader(nvh::loadFile("src/shaders/post.vert.spv", true, paths, true),
		vk::ShaderStageFlagBits::eVertex);
	pipelineGenerator.addShader(nvh::loadFile("src/shaders/post.frag.spv", true, paths, true),
		vk::ShaderStageFlagBits::eFragment);
	pipelineGenerator.rasterizationState.setCullMode(vk::CullModeFlagBits::eNone);
	m_postPipeline = pipelineGenerator.createPipeline();
	m_debug.setObjectName(m_postPipeline, "post");
}

void App::_createMainCommandBuffer() {
	m_mainCommandBuffer =
		m_device.allocateCommandBuffers({ m_cmdPool, vk::CommandBufferLevel::ePrimary, 1 })[0];
	vk::FenceCreateInfo fenceInfo;
	fenceInfo
		.setFlags(vk::FenceCreateFlagBits::eSignaled);
	m_mainFence = m_device.createFence(fenceInfo);
}

//--------------------------------------------------------------------------------------------------
// Called at each frame to update the camera matrix
//
void App::_updateUniformBuffer(const vk::CommandBuffer& cmdBuf)
{

	m_environmentalLight.updateSunAndSky(cmdBuf);
	// Prepare new UBO contents on host.
	const float aspectRatio = m_size.width / static_cast<float>(m_size.height);

	m_sceneUniforms.prevFrameProjectionViewMatrix = m_sceneUniforms.projectionViewMatrix;

	m_sceneUniforms.proj = nvmath::perspectiveVK(CameraManip.getFov(), aspectRatio, 0.1f, 1000.0f);
	m_sceneUniforms.view = CameraManip.getMatrix();
	m_sceneUniforms.projInverse = nvmath::invert(m_sceneUniforms.proj);
	m_sceneUniforms.viewInverse = nvmath::invert(m_sceneUniforms.view);
	m_sceneUniforms.projectionViewMatrix = m_sceneUniforms.proj * m_sceneUniforms.view;
	m_sceneUniforms.prevCamPos = m_sceneUniforms.cameraPos;
	m_sceneUniforms.cameraPos = CameraManip.getCamera().eye;

	if (m_enableNNE) {
		m_sceneUniforms.flags |= USE_NNE;
	}
	else {
		m_sceneUniforms.flags &= ~USE_NNE;
	}

	// UBO on the device, and what stages access it.
	vk::Buffer deviceUBO = m_sceneUniformBuffer.buffer;
	auto uboUsageStages = vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader
		| vk::PipelineStageFlagBits::eRayTracingShaderKHR;

	// Ensure that the modified UBO is not visible to previous frames.
	vk::BufferMemoryBarrier beforeBarrier;
	beforeBarrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
	beforeBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
	beforeBarrier.setBuffer(deviceUBO);
	beforeBarrier.setOffset(0);
	beforeBarrier.setSize(sizeof m_sceneUniforms);
	cmdBuf.pipelineBarrier(
		uboUsageStages,
		vk::PipelineStageFlagBits::eTransfer,
		vk::DependencyFlagBits::eDeviceGroup, {}, { beforeBarrier }, {});

	// Schedule the host-to-device upload. (hostUBO is copied into the cmd
	// buffer so it is okay to deallocate when the function returns).
	cmdBuf.updateBuffer<shader::SceneUniforms>(m_sceneUniformBuffer.buffer, 0, m_sceneUniforms);

	// Making sure the updated UBO will be visible.
	vk::BufferMemoryBarrier afterBarrier;
	afterBarrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
	afterBarrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
	afterBarrier.setBuffer(deviceUBO);
	afterBarrier.setOffset(0);
	afterBarrier.setSize(sizeof m_sceneUniforms);
	cmdBuf.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		uboUsageStages,
		vk::DependencyFlagBits::eDeviceGroup, {}, { afterBarrier }, {});
}

//--------------------------------------------------------------------------------------------------
// Draw a full screen quad with the attached image
//
void App::_drawPost(vk::CommandBuffer cmdBuf)
{
	m_debug.beginLabel(cmdBuf, "Post");

	cmdBuf.setViewport(0, { vk::Viewport(0, 0, (float)m_size.width, (float)m_size.height, 0, 1) });
	cmdBuf.setScissor(0, { {{0, 0}, {m_size.width, m_size.height}} });
	cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, m_postPipeline);
	cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_postPipelineLayout, 0,
		{
				m_sceneSet, m_sceneBuffers.getDescSet(),  m_outputImages.getDescSet(), m_environmentalLight.getDescSet()
		}, {});
	cmdBuf.draw(3, 1, 0, 0);

	m_debug.endLabel(cmdBuf);
}

void App::_submitMainCommand() {
	while (m_device.waitForFences(m_mainFence, VK_TRUE, 10000) == vk::Result::eTimeout) {
	}
	m_device.resetFences(m_mainFence);
	vk::SubmitInfo submitInfo;
	submitInfo
		.setCommandBuffers(m_mainCommandBuffer);
	m_queue.submit(submitInfo, m_mainFence);
}

void App::onResize(int /*w*/, int /*h*/)
{
	/*createOffscreenRender();
	updatePostDescriptorSet();
	updateRtDescriptorSet();*/
	_resetFrame();
}

//--------------------------------------------------------------------------------------------------
// If the camera matrix has changed, resets the frame.
// otherwise, increments frame.
//
void App::_updateFrame()
{
	static nvmath::mat4f refCamMatrix;
	static float         refFov{ CameraManip.getFov() };

	const auto& m = CameraManip.getMatrix();
	const auto  fov = CameraManip.getFov();

	if (memcmp(&refCamMatrix.a00, &m.a00, sizeof(nvmath::mat4f)) != 0 || refFov != fov)
	{
		_resetFrame();
		refCamMatrix = m;
		refFov = fov;
	}
	m_sceneUniforms.frame++;
	m_curFrame += m_sceneUniforms.maxSamples;
}

void App::_resetFrame()
{
	m_sceneUniforms.frame = -1;
	m_curFrame = 0;
}
