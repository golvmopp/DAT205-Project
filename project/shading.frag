#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

///////////////////////////////////////////////////////////////////////////////
// Material
///////////////////////////////////////////////////////////////////////////////
uniform vec3 material_color;
uniform float material_reflectivity;
uniform float material_metalness;
uniform float material_fresnel;
uniform float material_shininess;
uniform float material_emission;

uniform vec3 viewSpaceLightDir;
uniform float spotInnerAngle;
uniform float spotOuterAngle;
uniform float texmapscale;

uniform int has_emission_texture;
uniform int has_color_texture;
layout(binding = 0) uniform sampler2D colorMap;
layout(binding = 5) uniform sampler2D emissiveMap;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
layout(binding = 6) uniform sampler2D environmentMap;
layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D reflectionMap;
layout(binding = 10) uniform sampler2D shadowMapTex;
uniform float environment_multiplier;

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
uniform vec3 point_light_color = vec3(1.0, 1.0, 1.0);
uniform float point_light_intensity_multiplier = 50.0;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////
#define PI 3.14159265359

///////////////////////////////////////////////////////////////////////////////
// Input varyings from vertex shader
///////////////////////////////////////////////////////////////////////////////
in vec2 texCoord;
in vec3 viewSpaceNormal;
in vec3 viewSpacePosition;
in vec4 shadowMapCoord;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;
uniform vec3[] hemisphere_samples;
uniform mat4 previousViewProjectionMatrix;


///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;



vec3 calculateDirectIllumiunation(vec3 wo, vec3 n)
{
	float d = distance(viewSpaceLightPosition, viewSpacePosition);
	vec3 Li = point_light_intensity_multiplier * point_light_color / pow(d, 2.0);
	vec3 wi = normalize(viewSpaceLightPosition - viewSpacePosition);
	if (dot(n, wi) <= 0.0) {
		return vec3(0.0, 0.0, 0.0);
	}
	vec3 diffuse_term = material_color * (1.0 / PI) * abs(dot(n, wi)) * Li;
	float F = material_fresnel + (1 - material_fresnel) * pow(1 - dot(n, wi), 5.0);
	vec3 wh = normalize(wi + wo);
	float D = (material_shininess + 2) * pow(dot(n, wh), material_shininess) / (2 * PI);
	float G = min(1, min(2 * dot(n, wh)*dot(n, wo) / dot(wo, wh), 2 * dot(n, wh)*dot(n, wi) / dot(wo, wh)));
	float brdf = F * D * G / (4 * dot(n, wo) * dot(n, wi));
	vec3 dielectric_term = brdf * dot(n, wi) * Li + (1 - F) * diffuse_term;
	vec3 metal_term = brdf * material_color * dot(n, wi) * Li;
	vec3 microfacet_term = material_metalness * metal_term + (1 - material_metalness) * dielectric_term;
	return material_reflectivity * microfacet_term + (1 - material_reflectivity) * diffuse_term;

}

vec3 calculateIndirectIllumination(vec3 wo, vec3 n)
{
	vec3 worldSpaceNormal = vec3(viewInverse * vec4(n, 0.0));
	vec3 dir = worldSpaceNormal;
	float theta = acos(max(-1.0f, min(1.0f, dir.y)));
	float phi = atan(dir.z, dir.x);
	if (phi < 0.0f) phi = phi + 2.0f * PI;
	vec2 lookup = vec2(phi / (2.0 * PI), theta / PI);
	vec4 irradience = environment_multiplier * texture(irradianceMap, lookup);
	vec3 diffuse_term = material_color * (1.0 / PI) * vec3(irradience);

	vec3 wi = vec3(viewInverse * vec4(reflect(-wo, n), 0.0));
	dir = wi;
	theta = acos(max(-1.0f, min(1.0f, dir.y)));
	phi = atan(dir.z, dir.x);
	if (phi < 0.0f) phi = phi + 2.0f * PI;
	lookup = vec2(phi / (2.0 * PI), theta / PI);
	float roughness = sqrt(sqrt(2 / (material_shininess + 2)));
	vec3 Li = environment_multiplier * textureLod(reflectionMap, lookup, roughness * 7.0).xyz;
	float F = material_fresnel + (1 - material_fresnel) * pow(1 - dot(worldSpaceNormal, wi), 5.0);
	vec3 dielectric_term = F * Li + (1 - F) * diffuse_term;
	vec3 metal_term = F * material_color * Li;
	vec3 microfacet_term = material_metalness * metal_term + (1 - material_metalness) * dielectric_term;
	return material_reflectivity * microfacet_term + (1 - material_reflectivity) * diffuse_term;

}

float pcf(float scale) {
	float sum = 0;
	float x, y;
	float offset = scale/2 - 1;

	for (y = -(0.5 + offset); y <= (0.5 + offset); y += 1.0) {
		for (x = -(0.5 + offset); x <= (0.5 + offset); x += 1.0) {
		float depth = texture(shadowMapTex, (shadowMapCoord.xy + (vec2(x,y) * texmapscale * shadowMapCoord.w)) / shadowMapCoord.w).x;
			sum += (depth >= (shadowMapCoord.z / shadowMapCoord.w)) ? 1.0 : 0.0;
		}
	}
	return sum / (scale * scale);
}

void main() 
{
	float depth = texture(shadowMapTex, shadowMapCoord.xy / shadowMapCoord.w).x;
	float visibility = pcf(6); // percentage closer filtering. only use even numbers
	//float visibility = (depth >= (shadowMapCoord.z / shadowMapCoord.w)) ? 1.0 : 0.0; //traditional shadow maps
	float attenuation = 1.0;

	vec3 posToLight = normalize(viewSpaceLightPosition - viewSpacePosition);
	float cosAngle = dot(posToLight, -viewSpaceLightDir);

	//spotlight, smooth border
	float spotAttenuation = smoothstep(spotOuterAngle, spotInnerAngle, cosAngle);
	visibility *= spotAttenuation;

	vec3 wo = -normalize(viewSpacePosition);
	vec3 n = normalize(viewSpaceNormal);

	// Direct illumination
	vec3 direct_illumination_term = visibility * calculateDirectIllumiunation(wo, n);

	// Indirect illumination
	vec3 indirect_illumination_term = calculateIndirectIllumination(wo, n);

	///////////////////////////////////////////////////////////////////////////
	// Add emissive term. If emissive texture exists, sample this term.
	///////////////////////////////////////////////////////////////////////////
	vec3 emission_term = material_emission * material_color;
	if (has_emission_texture == 1) {
		emission_term = texture(emissiveMap, texCoord).xyz;
	}

	vec3 shading = 
		direct_illumination_term +
		indirect_illumination_term +
		emission_term;

	fragmentColor = vec4(shading, 1.0f);
	return;

}
