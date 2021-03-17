

#include "sceneBuffers.h"
#include "fileformats/stb_image.h"
#include "../shaders/headers/common.glsl"
#include "nvh/fileoperations.hpp"
#include <iostream>


extern std::string environmentalTextureFile;
extern std::string colorLamp;
extern std::vector<std::string> defaultSearchPaths;
#include "nvh/fileoperations.hpp"

#include<filesystem>
namespace fs = std::filesystem;

#include "../tools.hpp"

void SceneBuffers::create(const std::string& fileName) {
	m_gltfScene = {};

	tinygltf::TinyGLTF tcontext;
	std::string        warn, error;
	MilliTimer         timer;

	bool        result;
	std::string extension = fs::path(fileName).extension().string();

	if (extension == ".gltf")
	{
		result = tcontext.LoadASCIIFromFile(&m_tmodel, &error, &warn, fileName);
		timer.print();

	}
	else
	{
		result = tcontext.LoadBinaryFromFile(&m_tmodel, &error, &warn, fileName);
		timer.print();

	}
	if (result == false)
	{
		LOGE(error.c_str());
		assert(!"Error while loading scene");
		return;
	}
	LOGW(warn.c_str());
	timer.print();

	LOGI("Convert to internal GLTF");
	m_gltfScene.importMaterials(m_tmodel);
	m_gltfScene.importDrawableNodes(m_tmodel, nvh::GltfAttributes::Normal | nvh::GltfAttributes::Texcoord_0
		| nvh::GltfAttributes::Tangent | nvh::GltfAttributes::Color_0);

	ImGuiH::SetCameraJsonFile(fs::path(fileName).stem().string());
	if (!m_gltfScene.m_cameras.empty())
	{
		auto& c = m_gltfScene.m_cameras[0];
		CameraManip.setCamera({ c.eye, c.center, c.up, (float)rad2deg(c.cam.perspective.yfov) });
		ImGuiH::SetHomeCamera({ c.eye, c.center, c.up, (float)rad2deg(c.cam.perspective.yfov) });

		for (auto& c : m_gltfScene.m_cameras)
		{
			ImGuiH::AddCamera({ c.eye, c.center, c.up, (float)rad2deg(c.cam.perspective.yfov) });
		}
	}
	else
	{
		// Re-adjusting camera to fit the new scene
		CameraManip.fit(m_gltfScene.m_dimensions.min, m_gltfScene.m_dimensions.max, true);
	}

	m_triangleLights = collectTriangleLights(m_gltfScene);
	if (m_triangleLights.size() == 0) {
		//Dummy
		m_triangleLights.push_back(shader::triangleLight{});
	}
	std::vector<float> pdf;
	for (auto& lt : m_triangleLights) {
		float triangleLightPower = lt.emission_luminance.w * lt.normalArea.w;
		pdf.push_back(triangleLightPower);
	}
	std::vector<shader::aliasTableCell> aliasTable = createAliasTable(pdf);
	m_aliasTableCount = static_cast<uint32_t>(aliasTable.size());

	auto              queue = m_device.getQueue(m_graphicsQueueIndex, 1);
	nvvk::CommandPool cmdBufGet(m_device, m_graphicsQueueIndex, vk::CommandPoolCreateFlagBits::eTransient, queue);
	vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
	m_buffers[eVertex] = m_alloc->createBuffer(cmdBuf, m_gltfScene.m_positions, vkBU::eVertexBuffer|vkBU::eStorageBuffer | vkBU::eShaderDeviceAddress);

	m_buffers[eIndex] = m_alloc->createBuffer(cmdBuf, m_gltfScene.m_indices, vkBU::eIndexBuffer | vkBU::eStorageBuffer | vkBU::eShaderDeviceAddress);
	m_buffers[eNormal] = m_alloc->createBuffer(cmdBuf, m_gltfScene.m_normals, vkBU::eVertexBuffer | vkBU::eStorageBuffer);
	m_buffers[eTexCoord] = m_alloc->createBuffer(cmdBuf, m_gltfScene.m_texcoords0, vkBU::eVertexBuffer | vkBU::eStorageBuffer);
	m_buffers[eTangent] = m_alloc->createBuffer(cmdBuf, m_gltfScene.m_tangents, vkBU::eVertexBuffer | vkBU::eStorageBuffer);
	m_buffers[eColor] = m_alloc->createBuffer(cmdBuf, m_gltfScene.m_colors0, vkBU::eVertexBuffer | vkBU::eStorageBuffer);

	std::vector<shader::GltfShadeMaterial> shadeMaterials;
	for (auto& m : m_gltfScene.m_materials)
	{
		shader::GltfShadeMaterial smat;
		smat.pbrBaseColorFactor = m.pbrBaseColorFactor;
		smat.pbrBaseColorTexture = m.pbrBaseColorTexture;
		smat.pbrMetallicFactor = m.pbrMetallicFactor;
		smat.pbrRoughnessFactor = m.pbrRoughnessFactor;
		smat.pbrMetallicRoughnessTexture = m.pbrMetallicRoughnessTexture;
		smat.khrDiffuseFactor = m.khrDiffuseFactor;
		smat.khrSpecularFactor = m.khrSpecularFactor;
		smat.khrDiffuseTexture = m.khrDiffuseTexture;
		smat.shadingModel = m.shadingModel;
		smat.khrGlossinessFactor = m.khrGlossinessFactor;
		smat.khrSpecularGlossinessTexture = m.khrSpecularGlossinessTexture;
		smat.emissiveTexture = m.emissiveTexture;
		smat.emissiveFactor = m.emissiveFactor;
		smat.alphaMode = m.alphaMode;
		smat.alphaCutoff = m.alphaCutoff;
		smat.doubleSided = m.doubleSided;
		smat.normalTexture = m.normalTexture;
		smat.normalTextureScale = m.normalTextureScale;
		smat.uvTransform = m.uvTransform;
		smat.unlit = m.unlit;
		smat.anisotropy = m.anisotropy;
		smat.anisotropyDirection = m.anisotropyDirection;
		smat.ior= m.ior;

		//std::cout << smat.ior << std::endl;
		shadeMaterials.emplace_back(smat);
	}
	m_buffers[eMaterial] = m_alloc->createBuffer(cmdBuf, shadeMaterials, vkBU::eVertexBuffer | vkBU::eStorageBuffer);
	std::vector<shader::InstanceMatrices> nodeMatrices;
	for (auto& node : m_gltfScene.m_nodes)
	{
		shader::InstanceMatrices mat;
		mat.object2World = node.worldMatrix;
		mat.world2Object = invert(node.worldMatrix);
		nodeMatrices.emplace_back(mat);
	}
	m_buffers[eMatrix] = m_alloc->createBuffer(cmdBuf, nodeMatrices, vkBU::eVertexBuffer | vkBU::eStorageBuffer);
	m_buffers[eTriangleLight] = m_alloc->createBuffer(cmdBuf, m_triangleLights, vkBU::eVertexBuffer | vkBU::eStorageBuffer);
	m_buffers[eAliasTable] = m_alloc->createBuffer(cmdBuf, aliasTable, vkBU::eVertexBuffer | vkBU::eStorageBuffer);

	m_debug.setObjectName(m_buffers[eVertex].buffer, "Vertex");
	m_debug.setObjectName(m_buffers[eIndex].buffer, "Index");
	m_debug.setObjectName(m_buffers[eNormal].buffer, "Normal");
	m_debug.setObjectName(m_buffers[eTexCoord].buffer, "TexCoord");
	m_debug.setObjectName(m_buffers[eTangent].buffer, "Tangents");
	m_debug.setObjectName(m_buffers[eColor].buffer, "Color");
	m_debug.setObjectName(m_buffers[eMaterial].buffer, "Material");
	m_debug.setObjectName(m_buffers[eMatrix].buffer, "Matrix");

	_createTextureImages(cmdBuf);
	_createColorLampTexture(cmdBuf);
	cmdBufGet.submitAndWait(cmdBuf);
	m_alloc->finalizeAndReleaseStaging();
	_createRtBuffer();
}

void SceneBuffers::createDescriptorSetLayout() {
	using vkDT = vk::DescriptorType;
	using vkSS = vk::ShaderStageFlagBits;
	using vkDSLB = vk::DescriptorSetLayoutBinding;
	auto nbTextures = static_cast<uint32_t>(m_textures.size());
	auto flag = vkSS::eRaygenKHR | vkSS::eClosestHitKHR | vkSS::eAnyHitKHR | vkSS::eMissKHR | vkSS::eCompute | vkSS::eVertex | vkSS::eFragment;

	auto & bind = m_descBind;
	bind.addBinding(vkDSLB(B_ACCELERATION_STRUCTURE, vkDT::eAccelerationStructureKHR, 1,
		flag));  // TLAS
	bind.addBinding(vkDSLB(B_PLIM_LOOK_UP, vkDT::eStorageBuffer, 1, flag));
	bind.addBinding(vkDSLB(B_VERTICES, vkDT::eStorageBuffer, 1, flag));
	bind.addBinding(vkDSLB(B_NORMALS, vkDT::eStorageBuffer, 1, flag));
	bind.addBinding(vkDSLB(B_TEXCOORDS, vkDT::eStorageBuffer, 1, flag));
	bind.addBinding(vkDSLB(B_INDICES, vkDT::eStorageBuffer, 1, flag));
	bind.addBinding(vkDSLB(B_MATERIALS, vkDT::eStorageBuffer, 1, flag));
	bind.addBinding(vkDSLB(B_MATRICES, vkDT::eStorageBuffer, 1, flag));
	bind.addBinding(vkDSLB(B_TEXTURES, vkDT::eCombinedImageSampler, nbTextures, flag));
	bind.addBinding(vkDSLB(B_TANGENTS, vkDT::eStorageBuffer, 1, flag));
	bind.addBinding(vkDSLB(B_COLORS, vkDT::eStorageBuffer, 1, flag));
	bind.addBinding(vkDSLB(B_TRIANGLE_LIGHT, vkDT::eStorageBuffer, 1, flag));
	bind.addBinding(vkDSLB(B_ALIAS_TABLE, vkDT::eStorageBuffer, 1, flag));
	bind.addBinding(vkDSLB(B_COLOR_LAMP, vkDT::eCombinedImageSampler, 1, flag));

	m_descSetLayout = bind.createLayout(m_device);
	m_descPool = bind.createPool(m_device);
	m_descSet = nvvk::allocateDescriptorSet(m_device, m_descPool, m_descSetLayout);

}
void SceneBuffers::updateDescriptorSet() {
	
	auto& bind = m_descBind;

	vk::AccelerationStructureKHR                   tlas = m_rtBuilder.getAccelerationStructure();
	vk::WriteDescriptorSetAccelerationStructureKHR descASInfo;
	descASInfo.setAccelerationStructureCount(1);
	descASInfo.setPAccelerationStructures(&tlas);

	std::array<vk::DescriptorBufferInfo, 13> dbi;
	dbi[B_VERTICES] = vk::DescriptorBufferInfo{ m_buffers[eVertex].buffer, 0, VK_WHOLE_SIZE };
	dbi[B_INDICES] = vk::DescriptorBufferInfo{ m_buffers[eIndex].buffer, 0, VK_WHOLE_SIZE };
	dbi[B_NORMALS] = vk::DescriptorBufferInfo{ m_buffers[eNormal].buffer, 0, VK_WHOLE_SIZE };
	dbi[B_TEXCOORDS] = vk::DescriptorBufferInfo{ m_buffers[eTexCoord].buffer, 0, VK_WHOLE_SIZE };
	dbi[B_MATERIALS] = vk::DescriptorBufferInfo{ m_buffers[eMaterial].buffer, 0, VK_WHOLE_SIZE };
	dbi[B_MATRICES] = vk::DescriptorBufferInfo{ m_buffers[eMatrix].buffer, 0, VK_WHOLE_SIZE };
	dbi[B_TANGENTS] = vk::DescriptorBufferInfo{ m_buffers[eTangent].buffer, 0, VK_WHOLE_SIZE };
	dbi[B_COLORS] = vk::DescriptorBufferInfo{ m_buffers[eColor].buffer, 0, VK_WHOLE_SIZE };
	dbi[B_PLIM_LOOK_UP] = vk::DescriptorBufferInfo{ m_buffers[ePrimLookup].buffer, 0, VK_WHOLE_SIZE };
	dbi[B_TRIANGLE_LIGHT] = vk::DescriptorBufferInfo{ m_buffers[eTriangleLight].buffer, 0, VK_WHOLE_SIZE };
	dbi[B_ALIAS_TABLE] = vk::DescriptorBufferInfo{ m_buffers[eAliasTable].buffer, 0, VK_WHOLE_SIZE };

	std::vector<vk::WriteDescriptorSet> writes;
	writes.emplace_back(bind.makeWrite(m_descSet, B_ACCELERATION_STRUCTURE, &descASInfo));
	writes.emplace_back(bind.makeWrite(m_descSet, B_PLIM_LOOK_UP, &dbi[B_PLIM_LOOK_UP]));
	writes.emplace_back(bind.makeWrite(m_descSet, B_VERTICES, &dbi[B_VERTICES]));
	writes.emplace_back(bind.makeWrite(m_descSet, B_INDICES, &dbi[B_INDICES]));
	writes.emplace_back(bind.makeWrite(m_descSet, B_NORMALS, &dbi[B_NORMALS]));
	writes.emplace_back(bind.makeWrite(m_descSet, B_TEXCOORDS, &dbi[B_TEXCOORDS]));
	writes.emplace_back(bind.makeWrite(m_descSet, B_TANGENTS, &dbi[B_TANGENTS]));
	writes.emplace_back(bind.makeWrite(m_descSet, B_COLORS, &dbi[B_COLORS]));
	writes.emplace_back(bind.makeWrite(m_descSet, B_MATERIALS, &dbi[B_MATERIALS]));
	writes.emplace_back(bind.makeWrite(m_descSet, B_MATRICES, &dbi[B_MATRICES]));
	writes.emplace_back(bind.makeWrite(m_descSet, B_TRIANGLE_LIGHT, &dbi[B_TRIANGLE_LIGHT]));
	writes.emplace_back(bind.makeWrite(m_descSet, B_ALIAS_TABLE, &dbi[B_ALIAS_TABLE]));
	writes.emplace_back(bind.makeWrite(m_descSet, B_COLOR_LAMP, &m_colorLamp.descriptor));

	std::vector<vk::DescriptorImageInfo> diit;
	for (auto& texture : m_textures)
		diit.emplace_back(texture.descriptor);
	writes.emplace_back(bind.makeWriteArray(m_descSet, B_TEXTURES, diit.data()));

	m_device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}


void SceneBuffers::_createTextureImages(vk::CommandBuffer cmdBuf) {
	vk::Format format = vk::Format::eR8G8B8A8Unorm;

	// Make dummy image(1,1), needed as we cannot have an empty array
	auto addDefaultTexture = [this, cmdBuf]() {
		std::array<uint8_t, 4> white = { 255, 255, 255, 255 };
		m_textures.emplace_back(m_alloc->createTexture(cmdBuf, 4, white.data(), nvvk::makeImage2DCreateInfo(vk::Extent2D{ 1, 1 }), {}));
		m_debug.setObjectName(m_textures.back().image, "dummy");
	};

	if (m_tmodel.images.empty())
	{
		// No images, add a default one.
		addDefaultTexture();
		return;
	}
	LOGI("import textures\n");
	m_textures.reserve(m_tmodel.textures.size());
	for (size_t i = 0; i < m_tmodel.textures.size(); i++)
	{
		int sourceImage = m_tmodel.textures[i].source;
		if (sourceImage >= m_tmodel.images.size() || sourceImage < 0)
		{
			// Incorrect source image
			addDefaultTexture();
			continue;
		}

		auto& gltfimage = m_tmodel.images[sourceImage];
		if (gltfimage.width == -1 || gltfimage.height == -1 || gltfimage.image.empty())
		{
			// Image not present or incorrectly loaded (image.empty)
			addDefaultTexture();
			continue;
		}

		void* buffer = &gltfimage.image[0];
		VkDeviceSize bufferSize = gltfimage.image.size();
		auto         imgSize = vk::Extent2D(gltfimage.width, gltfimage.height);

		// Sampler
		vk::SamplerCreateInfo samplerCreateInfo{ {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear };
		if (m_tmodel.textures[i].sampler > -1)
		{
			// Retrieve the texture sampler
			auto gltfSampler = m_tmodel.samplers[m_tmodel.textures[i].sampler];
			samplerCreateInfo = _gltfSamplerToVulkan(gltfSampler);
		}

		// Creating an image, the sampler and generating mipmaps
		vk::ImageCreateInfo imageCreateInfo = nvvk::makeImage2DCreateInfo(imgSize, format, vkIU::eSampled, true);

		nvvk::Image image = m_alloc->createImage(cmdBuf, bufferSize, buffer, imageCreateInfo);
		nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize, imageCreateInfo.mipLevels);
		vk::ImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
		m_textures.emplace_back(m_alloc->createTexture(image, ivInfo, samplerCreateInfo));
		m_debug.setObjectName(m_textures[i].image, std::string("Txt" + std::to_string(i)).c_str());
	}

}

void SceneBuffers::_createColorLampTexture(vk::CommandBuffer cmdBuf) {
	vk::Format format = vk::Format::eR8G8B8A8Unorm;
	std::string filename = nvh::findFile(colorLamp, defaultSearchPaths);
	int width, height, component;
	unsigned char* pixels = stbi_load(filename.c_str(), &width, &height, &component, STBI_rgb_alpha);
	vk::DeviceSize bufferSize = width * height * 4 * sizeof(unsigned char);
	vk::Extent2D   imgSize(width, height);
	vk::SamplerCreateInfo samplerCreateInfo{ {}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear };
	vk::ImageCreateInfo   icInfo = nvvk::makeImage2DCreateInfo(imgSize, format);
	nvvk::Image              image = m_alloc->createImage(cmdBuf, bufferSize, pixels, icInfo);
	vk::ImageViewCreateInfo  ivInfo = nvvk::makeImageViewCreateInfo(image.image, icInfo);
	m_colorLamp=m_alloc->createTexture(image, ivInfo, samplerCreateInfo);
	stbi_image_free(pixels);
	//m_debug.setObjectName(m_colorLamp.image, std::string("colorLamp".c_str());
}
[[nodiscard]] void SceneBuffers::_createRtBuffer() {
	nvvk::CommandPool cmdBufGet(m_device, m_graphicsQueueIndex);
	vk::CommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
	auto properties = m_physicalDevice.getProperties2<vk::PhysicalDeviceProperties2,
		vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
	m_rtProperties = properties.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
	m_rtBuilder.setup(m_device, m_alloc, m_graphicsQueueIndex);
	// BLAS - Storing each primitive in a geometry
	std::vector<nvvk::RaytracingBuilderKHR::BlasInput> allBlas;
	allBlas.reserve(m_gltfScene.m_primMeshes.size());
	for (auto& primMesh : m_gltfScene.m_primMeshes)
	{
		auto geo = _primitiveToGeometry(primMesh);
		allBlas.push_back({ geo });
	}
	m_rtBuilder.buildBlas(allBlas, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);

	std::vector<nvvk::RaytracingBuilderKHR::Instance> tlas;
	tlas.reserve(m_gltfScene.m_nodes.size());
	uint32_t instID = 0;
	for (auto& node : m_gltfScene.m_nodes)
	{
		nvvk::RaytracingBuilderKHR::Instance rayInst;
		rayInst.transform = node.worldMatrix;
		rayInst.instanceCustomId = node.primMesh;  // gl_InstanceCustomIndexEXT: to find which primitive
		rayInst.blasId = node.primMesh;
		rayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		rayInst.hitGroupId = 0;  // We will use the same hit group for all objects
		tlas.emplace_back(rayInst);
	}
	m_rtBuilder.buildTlas(tlas, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);

	std::vector<shader::RtPrimitiveLookup> primLookup;
	for (auto& primMesh : m_gltfScene.m_primMeshes)
		primLookup.push_back({ primMesh.firstIndex, primMesh.vertexOffset, primMesh.materialIndex });
	m_buffers[ePrimLookup] = m_alloc->createBuffer(cmdBuf, primLookup, vk::BufferUsageFlagBits::eStorageBuffer);


	m_debug.setObjectName(m_buffers[ePrimLookup].buffer, "PrimLookup");

	cmdBufGet.submitAndWait(cmdBuf);
	m_alloc->finalizeAndReleaseStaging();
}

[[nodiscard]] nvvk::RaytracingBuilderKHR::BlasInput  SceneBuffers::_primitiveToGeometry(
	 const nvh::GltfPrimMesh& prim)
{
	// Building part
	vk::DeviceAddress vertexAddress = m_device.getBufferAddress({ m_buffers[eVertex].buffer });
	vk::DeviceAddress indexAddress = m_device.getBufferAddress({ m_buffers[eIndex].buffer });

	vk::AccelerationStructureGeometryTrianglesDataKHR triangles;
	triangles.setVertexFormat(vk::Format::eR32G32B32Sfloat);
	triangles.setVertexData(vertexAddress);
	triangles.setVertexStride(sizeof(nvmath::vec3f));
	triangles.setIndexType(vk::IndexType::eUint32);
	triangles.setIndexData(indexAddress);
	triangles.setTransformData({});
	triangles.setMaxVertex(prim.vertexCount);

	// Setting up the build info of the acceleration
	vk::AccelerationStructureGeometryKHR asGeom;
	asGeom.setGeometryType(vk::GeometryTypeKHR::eTriangles);
	asGeom.setFlags(vk::GeometryFlagBitsKHR::eOpaque);
	asGeom.geometry.setTriangles(triangles);

	vk::AccelerationStructureBuildRangeInfoKHR offset;
	offset.setFirstVertex(prim.vertexOffset);
	offset.setPrimitiveCount(prim.indexCount / 3);
	offset.setPrimitiveOffset(prim.firstIndex * sizeof(uint32_t));
	offset.setTransformOffset(0);

	nvvk::RaytracingBuilderKHR::BlasInput input;
	input.asGeometry.emplace_back(asGeom);
	input.asBuildOffsetInfo.emplace_back(offset);
	return input;
}

vk::SamplerCreateInfo SceneBuffers::_gltfSamplerToVulkan(tinygltf::Sampler& tsampler) {
	vk::SamplerCreateInfo vk_sampler;

	std::map<int, vk::Filter> filters;
	filters[9728] = vk::Filter::eNearest;  // NEAREST
	filters[9729] = vk::Filter::eLinear;   // LINEAR
	filters[9984] = vk::Filter::eNearest;  // NEAREST_MIPMAP_NEAREST
	filters[9985] = vk::Filter::eLinear;   // LINEAR_MIPMAP_NEAREST
	filters[9986] = vk::Filter::eNearest;  // NEAREST_MIPMAP_LINEAR
	filters[9987] = vk::Filter::eLinear;   // LINEAR_MIPMAP_LINEAR

	std::map<int, vk::SamplerMipmapMode> mipmap;
	mipmap[9728] = vk::SamplerMipmapMode::eNearest;  // NEAREST
	mipmap[9729] = vk::SamplerMipmapMode::eNearest;  // LINEAR
	mipmap[9984] = vk::SamplerMipmapMode::eNearest;  // NEAREST_MIPMAP_NEAREST
	mipmap[9985] = vk::SamplerMipmapMode::eNearest;  // LINEAR_MIPMAP_NEAREST
	mipmap[9986] = vk::SamplerMipmapMode::eLinear;   // NEAREST_MIPMAP_LINEAR
	mipmap[9987] = vk::SamplerMipmapMode::eLinear;   // LINEAR_MIPMAP_LINEAR

	std::map<int, vk::SamplerAddressMode> addressMode;
	addressMode[33071] = vk::SamplerAddressMode::eClampToEdge;
	addressMode[33648] = vk::SamplerAddressMode::eMirroredRepeat;
	addressMode[10497] = vk::SamplerAddressMode::eRepeat;

	vk_sampler.setMagFilter(filters[tsampler.magFilter]);
	vk_sampler.setMinFilter(filters[tsampler.minFilter]);
	vk_sampler.setMipmapMode(mipmap[tsampler.minFilter]);

	vk_sampler.setAddressModeU(addressMode[tsampler.wrapS]);
	vk_sampler.setAddressModeV(addressMode[tsampler.wrapT]);
	vk_sampler.setAddressModeW(addressMode[tsampler.wrapR]);

	// Always allow LOD
	vk_sampler.maxLod = FLT_MAX;
	return vk_sampler;
}

void SceneBuffers::destroy() {

	for (auto& t : m_textures) {
		m_alloc->destroy(t);
	}
	for (auto& t : m_buffers) {
		m_alloc->destroy(t);
	}

	Descriptor::destroy();
	m_rtBuilder.destroy();
}