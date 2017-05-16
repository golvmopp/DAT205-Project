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

///////////////////////////
//MOTION BLUR CRAP
///////////////////////////
vec4 motionBlur(vec4 color) {

	//Extract the per-pixel world-space positions
	float zOverW = texture2D(colorMap, texCoord).z;
	
	vec4 H = vec4(texCoord.x * 2 - 1, (1 - texCoord.y) * 2 - 1, zOverW, 1.0);

	vec4 D = viewInverse * H;

	vec4 worldPos = D/D.w;

	//comppute the per-pixel velocity

	vec4 previousPos = previousViewProjectionMatrix * worldPos;

	previousPos /= previousPos.w;

	vec2 velocity = ((H - previousPos) / 2.0f).xy;


	//Perform the motion blur
	vec4 theColor = texture2D(colorMap, texCoord);
	vec2 nextPos = texCoord + velocity;
	int numSamples = 4;

	for (int i = 1; i < numSamples; ++i, nextPos += velocity) {
		vec4 currentColor = texture2D(colorMap, nextPos);
		theColor += currentColor;
	}

	vec4 finalColor = theColor / numSamples;
	return finalColor;
}


void main() 
{
	float visibility = 1.0;
	float attenuation = 1.0;

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
