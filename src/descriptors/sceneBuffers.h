#pragma once
#define NVVK_ALLOC_DEDICATED
#include "descriptors.h"
extern bool GeneratePointLight;

class SceneBuffers : public Descriptor {
public:
	enum EBuffer
	{
		ePrimLookup,
		eVertex,
		eIndex,
		eNormal,
		eTexCoord,
		eTangent,
		eColor,
		eMaterial,
		eMatrix,
		eTriangleLight,
		eAliasTable,
		last_elem
	};

	[[nodiscard]] const nvh::GltfScene& getScene() const{ return m_gltfScene; }
	[[nodiscard]] const vk::Buffer& getBuffer(EBuffer b) const { return m_buffers[b].buffer; }
	
	[[nodiscard]] const std::vector<nvvk::Texture>& getTextures() const {
		return m_textures;
	}
	[[nodiscard]] const uint32_t& getAliasTableCount() const {
		return m_aliasTableCount;
	}
	[[nodiscard]] void create(const std::string& fileName);

	void createDescriptorSetLayout() override;
	void updateDescriptorSet() override;

	void destroy() override;

private:

	nvh::GltfScene m_gltfScene;
	tinygltf::Model    m_tmodel;
	std::array<nvvk::Buffer, EBuffer::last_elem> m_buffers;

	std::vector<nvvk::Texture> m_textures;
	nvvk::Texture m_colorLamp;

	nvvk::RaytracingBuilderKHR                          m_rtBuilder;
	vk::PhysicalDeviceRayTracingPipelinePropertiesKHR   m_rtProperties;
	nvvk::Buffer m_primlooks;

	[[nodiscard]] void _createTextureImages(vk::CommandBuffer cmdBuf);
	[[nodiscard]] void _createColorLampTexture(vk::CommandBuffer cmdBuf);
	[[nodiscard]] void _createRtBuffer();

	[[nodiscard]] nvvk::RaytracingBuilderKHR::BlasInput _primitiveToGeometry(const nvh::GltfPrimMesh&);
	vk::SamplerCreateInfo _gltfSamplerToVulkan(tinygltf::Sampler& tsampler);

	std::vector<shader::triangleLight> m_triangleLights;
	uint32_t m_aliasTableCount;
};

