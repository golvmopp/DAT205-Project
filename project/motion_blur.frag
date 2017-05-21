#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(binding = 0) uniform sampler2D colorMap;
layout(binding = 2) uniform sampler2D shadowMap;

in vec2 texCoord;
uniform mat4 previousViewProjectionMatrix;
uniform mat4 viewProjectionInverse;


layout(location = 0) out vec4 fragmentColor;

vec4 textureRect(in sampler2D tex, vec2 rectangleCoord) {
	return texture(tex, rectangleCoord / textureSize(tex, 0));
}

void main()
{

	//Extract the per-pixel world-space positions
	float zOverW = textureRect(shadowMap, gl_FragCoord.xy).z;// gl_FragCoord.w;

	vec4 H = vec4(texCoord.x, (1 - texCoord.y), zOverW, 1.0);

	vec4 D = viewProjectionInverse * H;

	vec4 worldPos = D / D.w;

	//compute the per-pixel velocity

	vec4 currentPos = H;

	vec4 previousPos = previousViewProjectionMatrix * worldPos;

	previousPos /= previousPos.w;

	vec2 velocity = ((currentPos - previousPos) / 2.0f).xy;


	//Perform the motion blur
	vec4 theColor = textureRect(colorMap, gl_FragCoord.xy);
	vec2 nextPos = texCoord.xy + velocity;
	float numSamples = 100.f;

	for (int i = 1; i < numSamples; ++i, nextPos += velocity) {
		vec4 currentColor = textureRect(colorMap, nextPos);
		theColor += currentColor;
	}
	fragmentColor = vec4(texCoord, 0.f, 1.f);//theColor / numSamples;
	return;
}