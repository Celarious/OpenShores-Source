#version 150

uniform mat4							mv;
uniform mat4							p;

in vec4									positionShine;

void main()
{
	gl_Position = p * (mv * vec4(positionShine.xyz, 1));
}
