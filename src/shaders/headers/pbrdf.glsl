

#ifndef PBRDF_GLSL
#define PBRDF_GLSL 1

#include "random.glsl"
#include "sampling.glsl"
#include "brdf.glsl"


void fresnelTransparent(float cos_theta, float sin_theta, float ior, out float T_v, out float T_h) {
	float s_sin = sqrt(1.0f - (sin_theta / ior) * (sin_theta / ior));

	float nume = 4.0 * ior * cos_theta * s_sin;
	float v_denom = (cos_theta + ior * s_sin) * (cos_theta + ior * s_sin);

	T_v = nume / v_denom;


	float h_denom = (s_sin + ior * cos_theta) * (s_sin + ior * cos_theta);
	T_h = nume / h_denom;
}

void fresnelRefrect(float T_v, float T_h, out float R_v, out float R_h) {
	R_v = 1.0f - T_v;
	R_h = 1.0f - T_h;
}

mat4 rot_y(float cos_theta, float sin_theta) {
	mat4 a;
	a[0][0] = cos_theta,  a[0][1] = 0.0f, a[0][2] = sin_theta, a[0][3] = 0.0f;
	a[1][0] = 0.0f,       a[1][1] = 1.0f, a[1][2] = 0.0f,      a[1][3] = 0.0f;
	a[2][0] = -sin_theta, a[2][1] = 0.0f, a[2][2] = cos_theta, a[2][3] = 0.0f;
	a[3][0] = 0.0f,       a[3][1] = 0.0f, a[3][2] = 0.0f,      a[3][3] = 1.0f;
	return a;
}

mat4 rot_z(float cos_theta, float sin_theta) {
	mat4 a;
	a[0][0] = 1.0f, a[0][1] = 0.0f, a[0][2] = 0.0f, a[0][3] = 0.0f;
	a[1][0] = 0.0f, a[1][1] = cos_theta, a[1][2] = sin_theta, a[1][3] = 0.0f;
	a[2][0] = 0.0f, a[2][1] = -sin_theta, a[2][2] = cos_theta, a[2][3] = 0.0f;
	a[3][0] = 0.0f, a[3][1] = 0.0f, a[3][2] = 0.0f, a[3][3] = 1.0f;
	return a;
}


mat4 fresnel_mat(float vert, float horiz, float delta) {
	float alpha = (vert + horiz) / 2.0f;
	float beta = (vert - horiz) / 2.0f;
	float chi = sqrt(vert * horiz);

	float c = cos(delta);
	float s = sin(delta);
	mat4 a;
	a[0][0] = alpha, a[0][1] = beta,  a[0][2] = 0.0f,   a[0][3] = 0.0f;
	a[1][0] = beta,  a[1][1] = alpha, a[1][2] = 0.0f,   a[1][3] = 0.0f;
	a[2][0] = 0.0f,  a[2][1] = 0.0f,  a[2][2] = chi*c,  a[2][3] = chi*s;
	a[3][0] = 0.0f,  a[3][1] = 0.0f,  a[3][2] = -chi*s, a[3][3] = chi*c;
	return a;
}

void one_mat(out mat4 a) {
	a[0][0] = 1.0f, a[0][1] = 0.0f, a[0][2] = 0.0f, a[0][3] = 0.0f;
	a[1][0] = 0.0f, a[1][1] = 0.0f, a[1][2] = 0.0f, a[1][3] = 0.0f;
	a[2][0] = 0.0f, a[2][1] = 0.0f, a[2][2] = 0.0f, a[2][3] = 0.0f;
	a[3][0] = 0.0f, a[3][1] = 0.0f, a[3][2] = 0.0f, a[3][3] = 0.0f;

}
void pBRDF_diffuse(out mat4 result, vec3 T_i, vec3 T_o, vec3 T, vec3 L, vec3 V, vec3 N,
	in Material mat)
{
	float T_vi, T_hi, T_vo, T_ho;

	float cos_theta_i = clamp(dot(L, N),0.001,1.0);
	float sin_theta_i = sqrt(1.0f - cos_theta_i * cos_theta_i);
	fresnelTransparent(cos_theta_i, sin_theta_i, mat.ior, T_vi, T_hi);

	float cos_theta_o = clamp(dot(V, N), 0.001, 1.0);
	float sin_theta_o = sqrt(1.0f - cos_theta_o * cos_theta_o);
	fresnelTransparent(cos_theta_o, sin_theta_o, mat.ior, T_vo, T_ho);

	float phi_o = acos(dot(T_o, projectToPlane(N, V)));
	float phi_i = acos(dot(T_i, projectToPlane(N, L)));


	mat4 rot_o = rot_z(cos(2.0f * phi_o), sin(2.0f * phi_o));
	mat4 rot_i = rot_z(cos(-2.0f * phi_i), sin(-2.0f * phi_i));

	mat4 F_i = fresnel_mat(T_vi, T_hi, 0.0f);
	mat4 F_o = fresnel_mat(T_vo, T_ho, 0.0f);

	mat4 one;
	one_mat(one);

	mat4 i = F_i * rot_i;
	mat4 o = rot_o * F_o;

	result = (o * one * i);
}


void pBRDF_specular(out mat4 result, vec3 T_i, vec3 T_o, vec3 T, vec3 L, vec3 V, vec3 N,
	in Material mat)
{
	vec3  H = normalize(L + V);
	float T_v, T_h, R_v, R_h;
	float cos_theta_d = clamp(dot(L, H), 0.001, 1.0);
	float sin_theta_d = sqrt(1.0f - cos_theta_d * cos_theta_d);
	fresnelTransparent(cos_theta_d, sin_theta_d, mat.ior, T_v, T_h);
	fresnelRefrect(T_v, T_h, R_v, R_h);



	float phy_o = acos(dot(T_o, projectToPlane(H, V)));
	float phy_i = acos(dot(T_i, projectToPlane(H, L)));

	float theta_b = atan(mat.ior);
	float delta = (mat.metallic > 0.5f) && (acos(cos_theta_d) < theta_b) ? M_PI : 0.0f;


	mat4 rot_o = rot_z(cos(2.0f * phy_o), sin(2.0f * phy_o));
	mat4 rot_i = rot_z(cos(-2.0f * phy_i), sin(-2.0f * phy_i));

	mat4 F = fresnel_mat(T_v, T_h, delta);
	F = rot_o * F * rot_i ;


	float alphaRoughness = max(0.001, mat.roughness);

	float V_g = V_GGX(abs(dot(N, V)), abs(dot(N, L)), alphaRoughness);
	float D_g = D_GGX(abs(dot(N, H)), max(0.001, alphaRoughness));

	result = V_g * D_g * F;

}

void pBsdfEval(out mat4 muller,out vec3 albedo, in Ray ray, in vec3 N, in vec3 T, in vec3 B, in Material mat, in vec3 bsdfDir, vec3 bsdfTangent) {
	vec3 V = -ray.direction;
	vec3 L = bsdfDir;

	mat4 diffuse;
	pBRDF_diffuse(diffuse, bsdfTangent, ray.tangent, T, L, V, N, mat);

	mat4 specular;
	pBRDF_specular(specular, bsdfTangent, ray.tangent, T, L, V, N, mat);

	muller = diffuse + specular;
	
	albedo = mat.albedo.xyz;

}

#endif  // BRDF_GLSL
