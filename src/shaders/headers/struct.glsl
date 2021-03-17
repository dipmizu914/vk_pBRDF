#ifndef STRUCTS_GLSL
#define STRUCTS_GLSL 1

struct InstanceMatrices
{
	mat4 object2World;
	mat4 world2Object;
};

#ifdef __cplusplus
enum DebugMode
{
#else
const uint
#endif  // __cplusplus
eNoDebug = 0,
eBaseColor = 1,
eNormal = 2,
eMetallic = 3,
eEmissive = 4,
eAlpha = 5,
eRoughness = 6,
eTextcoord = 7,
eTangent = 8,
eRadiance = 9,
eWeight = 10,
eRayDir = 11


#ifdef __cplusplus
}
#endif  // __cplusplus
;
// Camera of the scene
struct CameraMatrices
{
	mat4 view;
	mat4 proj;
	mat4 viewInverse;
	mat4 projInverse;
};

#define ALPHA_MODE_OPAQUE 0
#define ALPHA_MODE_MASK 1
#define ALPHA_MODE_BLEND 2

struct pointLight {
	vec4 pos;
	vec4 emission_luminance; // w is luminance
};

struct triangleLight {
	vec4 p1; //position
	vec4 p2;
	vec4 p3;
	vec4 t1; //tangent
	vec4 t2;
	vec4 t3;
	vec4 n1; //normal
	vec4 n2;
	vec4 n3;
	vec4 emission_luminance; // w is luminance
	vec4 normalArea;
	mat4 worldToObject;
	mat4 objectToWorld;
};

#define LIGHT_KIND_POINT 0
#define LIGHT_KIND_TRIANGLE 1
#define LIGHT_KIND_ENVIRONMENT 2 //in future


struct aliasTableCell {
	int alias;
	float prob;
	float pdf;
	float aliasPdf;
};



struct Payload {
	vec4 worldPos;
	vec3 worldNormal;
	vec4 albedo;
	vec3 emissive;
	float roughness;
	float metallic;
	bool exist;
};




struct RtPrimitiveLookup
{
	uint indexOffset;
	uint vertexOffset;
	int  materialIndex;
};


// GLTF material
#define MATERIAL_METALLICROUGHNESS 0
#define MATERIAL_SPECULARGLOSSINESS 1
#define ALPHA_OPAQUE 0
#define ALPHA_MASK 1
#define ALPHA_BLEND 2
struct GltfShadeMaterial
{

	vec4 pbrBaseColorFactor;

	int   pbrBaseColorTexture;
	float pbrMetallicFactor;
	float pbrRoughnessFactor;
	int   pbrMetallicRoughnessTexture;

	// KHR_materials_pbrSpecularGlossiness
	vec4 khrDiffuseFactor;
	vec3 khrSpecularFactor;
	int  khrDiffuseTexture;

	int   shadingModel;  // 0: metallic-roughness, 1: specular-glossiness
	float khrGlossinessFactor;
	int   khrSpecularGlossinessTexture;
	int   emissiveTexture;

	vec3 emissiveFactor;
	int  alphaMode;

	float alphaCutoff;
	int   doubleSided;
	int   normalTexture;
	float normalTextureScale;

	mat4 uvTransform;
	int  unlit;

	float anisotropy;
	vec3  anisotropyDirection;

	float ior;
};

#define USE_DoF (1<<0)
#define USE_NNE (1<<1)
struct SceneUniforms {
	mat4 view;
	mat4 proj;
	mat4 viewInverse;
	mat4 projInverse;
	mat4 projectionViewMatrix;
	mat4 prevFrameProjectionViewMatrix;
	vec4 cameraPos;
	vec4 prevCamPos;
	vec4 lightStorks;
	uvec2 screenSize;

	int flags;
	int present_mode;
	int debugging_mode;

	float gamma;
	float exposure;

	float fireflyClampThreshold;

	int frame;
	int maxSamples;
	int maxDepth;

	float focalDist;
	float aperture;

	float environment_intensity_factor;

	uint aliasTableCount;
};

#endif