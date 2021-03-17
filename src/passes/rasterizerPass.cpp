#include "rasterizerPass.h"

#include "nvvk/renderpasses_vk.hpp"
#include "nvvk/pipeline_vk.hpp"

extern std::vector<std::string> defaultSearchPaths;


using EBuffer = SceneBuffers::EBuffer;
void RasterizerPass::run(const vk::CommandBuffer& cmdBuf, const std::vector<vk::DescriptorSet>& descSets) {
	using vkPBP = vk::PipelineBindPoint;
	using vkSS = vk::ShaderStageFlagBits;
	std::vector<vk::DeviceSize> offsets = { 0, 0, 0 };
	
	std::array<vk::ClearValue, 2> clearValues;
	clearValues[0].setColor(std::array<float, 4>({ 0.0f, 0.0f, 0.0f, 0.f }));
	clearValues[1].setDepthStencil({ 1.0f, 0 });

	vk::RenderPassBeginInfo offscreenRenderPassBeginInfo;
	offscreenRenderPassBeginInfo.setClearValueCount(2);
	offscreenRenderPassBeginInfo.setPClearValues(clearValues.data());
	offscreenRenderPassBeginInfo.setRenderPass(m_renderPass);
	offscreenRenderPassBeginInfo.setFramebuffer(m_framebuffer);
	offscreenRenderPassBeginInfo.setRenderArea({ {}, m_size });
	cmdBuf.beginRenderPass(offscreenRenderPassBeginInfo, vk::SubpassContents::eInline);

	cmdBuf.setViewport(0, { vk::Viewport(0, 0, (float)m_size.width, (float)m_size.height, 0, 1) });
	cmdBuf.setScissor(0, { {{0, 0}, {m_size.width, m_size.height}} });
	cmdBuf.bindPipeline(vkPBP::eGraphics, m_pipeline);
	cmdBuf.bindDescriptorSets(vkPBP::eGraphics, m_pipelineLayout, 0, descSets, {});

	std::vector<vk::Buffer> vertexBuffers = { m_sceneBuffers->getBuffer(EBuffer::eVertex), m_sceneBuffers->getBuffer(EBuffer::eNormal),
											 m_sceneBuffers->getBuffer(EBuffer::eTexCoord) };
	cmdBuf.bindVertexBuffers(0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data(),
		offsets.data());
	cmdBuf.bindIndexBuffer(m_sceneBuffers->getBuffer(EBuffer::eIndex), 0, vk::IndexType::eUint32);
	uint32_t idxNode = 0;
	for (auto& node : m_gltfScene->m_nodes)
	{
		auto& primitive = m_gltfScene->m_primMeshes[node.primMesh];
		m_pushConstant.instanceId = idxNode++;
		m_pushConstant.materialId = primitive.materialIndex;
		cmdBuf.pushConstants<PushConstant>(
			m_pipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
			m_pushConstant);
		cmdBuf.drawIndexed(primitive.indexCount, 1, primitive.firstIndex, primitive.vertexOffset, 0);
	}
	cmdBuf.endRenderPass();

}

void RasterizerPass::createRenderPass(vk::Extent2D outputSize) {
	m_size = outputSize;
	m_alloc->destroy(m_colorImage);
	m_alloc->destroy(m_depthImage);

	// Creating the color image
	{
		auto colorCreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenColorFormat,
			vk::ImageUsageFlagBits::eColorAttachment
			| vk::ImageUsageFlagBits::eSampled
			| vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc);

		nvvk::Image             image = m_alloc->createImage(colorCreateInfo);
		vk::ImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, colorCreateInfo);
		m_colorImage = m_alloc->createTexture(image, ivInfo, vk::SamplerCreateInfo());
		m_colorImage.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	// Creating the depth buffer
	{
		auto depthCreateInfo =
			nvvk::makeImage2DCreateInfo(m_size, m_offscreenDepthFormat,
				vk::ImageUsageFlagBits::eDepthStencilAttachment);
		nvvk::Image image = m_alloc->createImage(depthCreateInfo);

		vk::ImageViewCreateInfo depthStencilView;
		depthStencilView.setViewType(vk::ImageViewType::e2D);
		depthStencilView.setFormat(m_offscreenDepthFormat);
		depthStencilView.setSubresourceRange({ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
		depthStencilView.setImage(image.image);
		m_depthImage = m_alloc->createTexture(image, depthStencilView);
	}

	{
		nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
		auto              cmdBuf = genCmdBuf.createCommandBuffer();
		nvvk::cmdBarrierImageLayout(cmdBuf, m_colorImage.image, vk::ImageLayout::eUndefined,
			vk::ImageLayout::eGeneral);
		nvvk::cmdBarrierImageLayout(cmdBuf, m_depthImage.image, vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthStencilAttachmentOptimal,
			vk::ImageAspectFlagBits::eDepth);
		genCmdBuf.submitAndWait(cmdBuf);
	}
	m_alloc->finalizeAndReleaseStaging();

	m_renderPass = nvvk::createRenderPass(m_device, { m_offscreenColorFormat }, m_offscreenDepthFormat, 1, true,
		true, vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral);

	std::vector<vk::ImageView> attachments = { m_colorImage.descriptor.imageView,
											  m_depthImage.descriptor.imageView };

	m_device.destroy(m_framebuffer);

	vk::FramebufferCreateInfo info;
	info.setRenderPass(m_renderPass);
	info.setAttachmentCount(2);
	info.setPAttachments(attachments.data());
	info.setWidth(m_size.width);
	info.setHeight(m_size.height);
	info.setLayers(1);
	m_framebuffer = m_device.createFramebuffer(info);
}

void RasterizerPass::createPipeline(const std::vector<vk::DescriptorSetLayout>& layouts) {
	using vkDT = vk::DescriptorType;
	using vkSS = vk::ShaderStageFlagBits;
	using vkDSLB = vk::DescriptorSetLayoutBinding;
	std::vector<std::string> paths = defaultSearchPaths;

	vk::PushConstantRange pushConstantRanges = { vkSS::eVertex | vkSS::eFragment , 0,
											sizeof(PushConstant) };

	vk::PipelineLayoutCreateInfo         pipelineLayoutCreateInfo;
	pipelineLayoutCreateInfo.setPushConstantRangeCount(1);
	pipelineLayoutCreateInfo.setPPushConstantRanges(&pushConstantRanges);
	pipelineLayoutCreateInfo.setSetLayoutCount(static_cast<uint32_t>(layouts.size()));
	pipelineLayoutCreateInfo.setPSetLayouts(layouts.data());
	m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutCreateInfo);
	nvvk::GraphicsPipelineGeneratorCombined gpb(m_device, m_pipelineLayout, m_renderPass);
	gpb.depthStencilState.depthTestEnable = true;
	gpb.addShader(nvh::loadFile("src/shaders/rasterizer.vert.spv", true, paths), vkSS::eVertex);
	gpb.addShader(nvh::loadFile("src/shaders/rasterizer.frag.spv", true, paths), vkSS::eFragment);
	gpb.addBindingDescriptions(
		{ {0, sizeof(nvmath::vec3)}, {1, sizeof(nvmath::vec3)}, {2, sizeof(nvmath::vec2)} });
	gpb.addAttributeDescriptions({
			{0, 0, vk::Format::eR32G32B32Sfloat, 0},  // Position
			{1, 1, vk::Format::eR32G32B32Sfloat, 0},  // Normal
			{2, 2, vk::Format::eR32G32Sfloat, 0}
			// Texcoord0
		});
	m_pipeline = gpb.createPipeline();

}

void RasterizerPass::destroy() {

}