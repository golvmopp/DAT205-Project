#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

uniform vec3 material_color;



layout(binding = 0) uniform sampler2D frameBufferTexture;
layout(location = 0) out vec4 fragmentColor;

vec3 grayscale(vec3 rgbSample) {
	return vec3(rgbSample.r * 0.2126 + rgbSample.g * 0.7152 + rgbSample.b * 0.0722);
}


vec4 textureRect(in sampler2D tex, vec2 rectangleCoord) {
	return texture(tex, rectangleCoord / textureSize(tex,0));
}

void main() 
{

	fragmentColor = textureRect(frameBufferTexture, gl_FragCoord.xy); //#nofilter
		
}

	
