#version 150

uniform mat4							mv;
uniform mat4							p;

in vec4									position;

out vec3								vertUp;

void main()
{
	gl_Position = p * (mv * vec4(position.xyz, 1));
	vertUp = normalize(position.xyz);
}
