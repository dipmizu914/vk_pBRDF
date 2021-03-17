#include "raytracePass.h"

extern std::vector<std::string> defaultSearchPaths;


void RayTracePass::run(const vk::CommandBuffer& cmdBuf, const std::vector<vk::DescriptorSet>& descSets) {
	cmdBuf.pipelineBarrier(
		vk::PipelineStageFlagBits::eAllCommands,
		vk::PipelineStageFlagBits::eAllCommands,
		{}, {}, {}, {}
	);
	cmdBuf.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, m_pipeline);
	cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, m_pipelineLayout, 0,
		descSets, {});
	// Size of a program identifier
	uint32_t groupSize =
		nvh::align_up(m_rtProperties.shaderGroupHandleSize, m_rtProperties.shaderGroupBaseAlignment);
	uint32_t          groupStride = groupSize;
	vk::DeviceAddress sbtAddress = m_device.getBufferAddress({ m_SBTBuffer.buffer });

	using Stride = vk::StridedDeviceAddressRegionKHR;
	std::array<Stride, 4> strideAddresses{
		Stride{sbtAddress + 0u * groupSize, groupStride, groupSize * 1},  // raygen
		Stride{sbtAddress + 1u * groupSize, groupStride, groupSize * 2},  // miss
		Stride{sbtAddress + 3u * groupSize, groupStride, groupSize * 1},  // hit
		Stride{0u, 0u, 0u} };                                              // callable

	cmdBuf.traceRaysKHR(&strideAddresses[0], &strideAddresses[1], &strideAddresses[2],
		&strideAddresses[3],  //
		m_size.width, m_size.height,
		1);  //

}

void RayTracePass::setup(const vk::Instance& instance, const vk::Device& device, const vk::PhysicalDevice& physicalDevice, uint32_t graphicsQueueIndex, nvvk::Allocator* allocator) {
	m_instance = instance;
	m_device = device;
	m_graphicsQueueIndex = graphicsQueueIndex;
	m_physicalDevice = physicalDevice;
	m_alloc = allocator;
	auto properties =
		m_physicalDevice.getProperties2<vk::PhysicalDeviceProperties2,
		vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
	m_rtProperties = properties.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
}

void RayTracePass::createRenderPass(vk::Extent2D outputSize) {
	m_size = outputSize;
	for (auto& t : m_renderResults) {
		m_alloc->destroy(t);
	}
	auto colorCreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenColorFormat,
		vk::ImageUsageFlagBits::eColorAttachment
		| vk::ImageUsageFlagBits::eSampled
		| vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc);

	for (int i = 0; i < ERenderResult::last_elem; i++) {
		nvvk::Image             image = m_alloc->createImage(colorCreateInfo);
		vk::ImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, colorCreateInfo);
		m_renderResults[i] = m_alloc->createTexture(image, ivInfo, vk::SamplerCreateInfo());
		m_renderResults[i].descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	{
		nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
		auto              cmdBuf = genCmdBuf.createCommandBuffer();
		for (int i = 0; i < ERenderResult::last_elem; i++) {
			nvvk::cmdBarrierImageLayout(cmdBuf, m_renderResults[i].image, vk::ImageLayout::eUndefined,
				vk::ImageLayout::eGeneral);
		}

		genCmdBuf.submitAndWait(cmdBuf);
	}
	m_alloc->finalizeAndReleaseStaging();
}

void RayTracePass::createPipeline(const std::vector<vk::DescriptorSetLayout>& descLayouts) {
	std::vector<std::string> paths = defaultSearchPaths;
	vk::ShaderModule raygenSM =
		nvvk::createShaderModule(m_device,  //
			nvh::loadFile("src/shaders/pathtrace.rgen.spv", true, paths, true));
	vk::ShaderModule missSM =
		nvvk::createShaderModule(m_device,  //
			nvh::loadFile("src/shaders/pathtrace.rmiss.spv", true, paths, true));

	// The second miss shader is invoked when a shadow ray misses the geometry. It
	// simply indicates that no occlusion has been found
	vk::ShaderModule shadowmissSM = nvvk::createShaderModule(
		m_device, nvh::loadFile("src/shaders/pathtraceShadow.rmiss.spv", true, paths, true));

	std::vector<vk::PipelineShaderStageCreateInfo> stages;
	// Raygen
	vk::RayTracingShaderGroupCreateInfoKHR rg{ vk::RayTracingShaderGroupTypeKHR::eGeneral,
											  VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR,
											  VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR };
	stages.push_back({ {}, vk::ShaderStageFlagBits::eRaygenKHR, raygenSM, "main" });
	rg.setGeneralShader(static_cast<uint32_t>(stages.size() - 1));
	m_rtShaderGroups.push_back(rg);
	// Miss
	vk::RayTracingShaderGroupCreateInfoKHR mg{ vk::RayTracingShaderGroupTypeKHR::eGeneral,
											  VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR,
											  VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR };
	stages.push_back({ {}, vk::ShaderStageFlagBits::eMissKHR, missSM, "main" });
	mg.setGeneralShader(static_cast<uint32_t>(stages.size() - 1));
	m_rtShaderGroups.push_back(mg);
	// Shadow Miss
	stages.push_back({ {}, vk::ShaderStageFlagBits::eMissKHR, shadowmissSM, "main" });
	mg.setGeneralShader(static_cast<uint32_t>(stages.size() - 1));
	m_rtShaderGroups.push_back(mg);

	vk::ShaderModule chitSM =
		nvvk::createShaderModule(m_device,  //
			nvh::loadFile("src/shaders/pathtrace.rchit.spv", true, paths, true));
	vk::ShaderModule ahitSM =
		nvvk::createShaderModule(m_device,  //
			nvh::loadFile("src/shaders/pathtrace.rahit.spv", true, paths, true));
	vk::RayTracingShaderGroupCreateInfoKHR hg{ vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
											VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR,
											VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR };
	stages.push_back({ {}, vk::ShaderStageFlagBits::eClosestHitKHR, chitSM, "main" });
	hg.setClosestHitShader(static_cast<uint32_t>(stages.size() - 1));
	//stages.push_back({ {}, vk::ShaderStageFlagBits::eAnyHitKHR, ahitSM, "main" });
	//hg.setAnyHitShader(static_cast<uint32_t>(stages.size() - 1));
	m_rtShaderGroups.push_back(hg);

	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	pipelineLayoutCreateInfo.setSetLayouts(descLayouts);
	m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutCreateInfo);
	vk::RayTracingPipelineCreateInfoKHR rayPipelineInfo;
	rayPipelineInfo.setStageCount(static_cast<uint32_t>(stages.size()));  // Stages are shaders
	rayPipelineInfo.setPStages(stages.data());

	rayPipelineInfo.setGroupCount(static_cast<uint32_t>(
		m_rtShaderGroups.size()));  // 1-raygen, n-miss, n-(hit[+anyhit+intersect])
	rayPipelineInfo.setPGroups(m_rtShaderGroups.data());

	rayPipelineInfo.setMaxPipelineRayRecursionDepth(2);  // Ray depth
	rayPipelineInfo.setLayout(m_pipelineLayout);
	m_pipeline = static_cast<const vk::Pipeline&>(
		m_device.createRayTracingPipelineKHR({}, {}, rayPipelineInfo));

	m_device.destroy(raygenSM);
	m_device.destroy(missSM);
	m_device.destroy(shadowmissSM);
	m_device.destroy(chitSM);
	m_device.destroy(ahitSM);

	_createShaderBindingTable();
}

void RayTracePass::_createShaderBindingTable()
{
	auto groupCount =
		static_cast<uint32_t>(m_rtShaderGroups.size());               // 3 shaders: raygen, miss, chit
	uint32_t groupHandleSize = m_rtProperties.shaderGroupHandleSize;  // Size of a program identifier
	uint32_t groupSizeAligned =
		nvh::align_up(groupHandleSize, m_rtProperties.shaderGroupBaseAlignment);

	// Fetch all the shader handles used in the pipeline, so that they can be written in the SBT
	uint32_t sbtSize = groupCount * groupSizeAligned;

	std::vector<uint8_t> shaderHandleStorage(sbtSize);
	auto result = m_device.getRayTracingShaderGroupHandlesKHR(m_pipeline, 0, groupCount, sbtSize,
		shaderHandleStorage.data());
	if (result != vk::Result::eSuccess)
		LOGE("Fail getRayTracingShaderGroupHandlesKHR: %s", vk::to_string(result).c_str());

	// Write the handles in the SBT
	m_SBTBuffer = m_alloc->createBuffer(
		sbtSize,
		vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eShaderDeviceAddressKHR
		| vk::BufferUsageFlagBits::eShaderBindingTableKHR,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	// Write the handles in the SBT
	void* mapped = m_alloc->map(m_SBTBuffer);
	auto* pData = reinterpret_cast<uint8_t*>(mapped);
	for (uint32_t g = 0; g < groupCount; g++)
	{
		memcpy(pData, shaderHandleStorage.data() + g * groupHandleSize, groupHandleSize);  // raygen
		pData += groupSizeAligned;
	}
	m_alloc->unmap(m_SBTBuffer);
	m_alloc->finalizeAndReleaseStaging();
}


void RayTracePass::destroy() {
	Pass::destroy();

	m_alloc->destroy(m_SBTBuffer);
}