#include "util.h"
#include <queue>


float luminance(float r, float g, float b) {
	return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

std::vector<shader::triangleLight> collectTriangleLights(const nvh::GltfScene& scene) {
	std::vector<shader::triangleLight> result;
	for (const nvh::GltfNode& node : scene.m_nodes) {
		const nvh::GltfPrimMesh& mesh = scene.m_primMeshes[node.primMesh];
		const nvh::GltfMaterial& material = scene.m_materials[mesh.materialIndex];
		if (material.emissiveFactor.sq_norm() > 1e-6) {
			const uint32_t* indices = scene.m_indices.data() + mesh.firstIndex;
			const nvmath::vec3* pos = scene.m_positions.data() + mesh.vertexOffset;
			const nvmath::vec4* tan = scene.m_tangents.data() + mesh.vertexOffset;
			const nvmath::vec3* nor = scene.m_normals.data() + mesh.vertexOffset;
			for (uint32_t i = 0; i < mesh.indexCount; i += 3, indices += 3) {
				// triangle
				vec4 p1 = node.worldMatrix * nvmath::vec4(pos[indices[0]], 1.0f);
				vec4 p2 = node.worldMatrix * nvmath::vec4(pos[indices[1]], 1.0f);
				vec4 p3 = node.worldMatrix * nvmath::vec4(pos[indices[2]], 1.0f);
				vec3 p1_vec3(p1.x, p1.y, p1.z), p2_vec3(p2.x, p2.y, p2.z), p3_vec3(p3.x, p3.y, p3.z);


				vec4 t1 = tan[indices[0]];
				vec4 t2 = tan[indices[1]];
				vec4 t3 = tan[indices[2]];

				vec4 n1 = nvmath::vec4(nor[indices[0]], 0.0f);
				vec4 n2 = nvmath::vec4(nor[indices[1]], 0.0f);
				vec4 n3 = nvmath::vec4(nor[indices[2]], 0.0f);

				vec3 normal = nvmath::cross(p2_vec3 - p1_vec3, p3_vec3 - p1_vec3);
				float area = normal.norm();
				normal /= area;
				area *= 0.5f;

				float emissionLuminance = luminance(
					material.emissiveFactor.x, material.emissiveFactor.y, material.emissiveFactor.z
				);

				mat4 objectToWorld = node.worldMatrix;
				mat4 worldToObject = nvmath::invert(objectToWorld);

				result.push_back(shader::triangleLight{
					p1, p2, p3, t1, t2, t3, n1, n2, n3,
					 nvmath::vec4(material.emissiveFactor, emissionLuminance),
					 nvmath::vec4(normal, area), worldToObject, objectToWorld
					});
			}
		}
	}
	return result;
}

[[nodiscard]] std::vector<shader::aliasTableCell> createAliasTable(std::vector<float>& pdf) {
	std::queue<int> biggerQueue;
	std::queue<int> smallerQueue;
	std::vector<float> lightPdf;

	float powerSum = 0.f;
	uint32_t lightNum = 0;
	lightNum = static_cast<uint32_t>(pdf.size());
	for (float p : pdf) {
		powerSum += p;
		lightPdf.push_back(p);
	}

	std::vector<shader::aliasTableCell> aliasTable(lightNum, shader::aliasTableCell{ -1,0.f, 0.f });

	for (uint32_t i = 0; i < lightNum; ++i) {
		aliasTable[i].pdf = lightPdf[i] / powerSum;
		// aliasTable[i].pdf = 1.f / lightNum;
		lightPdf[i] = float(lightPdf.size()) * lightPdf[i] / powerSum;
		if (lightPdf[i] >= 1.f) {
			biggerQueue.push(i);
		}
		else {
			smallerQueue.push(i);
		}
	}

	// Construct Alias Table
	while (!biggerQueue.empty() && !smallerQueue.empty()) {
		int g = biggerQueue.front();
		biggerQueue.pop();
		int l = smallerQueue.front();
		smallerQueue.pop();

		aliasTable[l].prob = lightPdf[l];
		aliasTable[l].alias = g;
		lightPdf[g] = (lightPdf[g] + lightPdf[l]) - 1.f;

		if (lightPdf[g] < 1.f) {
			smallerQueue.push(g);
		}
		else {
			biggerQueue.push(g);
		}
	}

	while (!biggerQueue.empty()) {
		int g = biggerQueue.front();
		biggerQueue.pop();
		aliasTable[g].prob = 1.f;
		aliasTable[g].alias = g;
	}

	while (!smallerQueue.empty()) {
		int l = smallerQueue.front();
		smallerQueue.pop();
		aliasTable[l].prob = 1.f;
		aliasTable[l].alias = l;

	}

	for (shader::aliasTableCell& col : aliasTable) {
		col.aliasPdf = aliasTable[col.alias].pdf;
	}

	return aliasTable;
}

