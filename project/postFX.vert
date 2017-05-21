#version 420

layout(location = 0) in vec3 position;
in vec2 texcoord;
out vec2 texCoord;

void main() 
{
	texCoord = (position.xy + 1) / 2;
	gl_Position = vec4(position,1.0);

}

