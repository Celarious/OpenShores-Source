#version 150

uniform mat4							mv;
uniform mat4							p;

in vec3									position;

void main()
{
	gl_Position = p * (mv * vec4(position, 1));
}
