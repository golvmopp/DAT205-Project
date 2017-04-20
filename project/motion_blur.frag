#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(binding = 0) uniform sampler2D colorMap;

in vec2 texCoord;
uniform mat4 previousViewProjectionMatrix;
uniform mat4 viewProjectionInverse;


layout(location = 0) out vec4 fragmentColor;

void main()
{

	//Extract the per-pixel world-space positions
	float zOverW = texture2D(colorMap, texCoord).z;

	vec4 H = vec4(texCoord.x * 2 - 1, (1 - texCoord.y) * 2 - 1, zOverW, 1.0);

	vec4 D = viewProjectionInverse * H;

	vec4 worldPos = D / D.w;

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

	fragmentColor = theColor / numSamples;
	return;
}