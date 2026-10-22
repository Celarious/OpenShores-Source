#version 150

uniform mat4							mv;
uniform mat4							p;

in vec4									positionShine;
in vec4									texCoordIndex;

out vec2								vertTexCoord;
flat out float							vertTexIndex;

void main()
{
	gl_Position = p * (mv * vec4(positionShine.xyz, 1));
	vertTexCoord = texCoordIndex.xy;
	vertTexIndex = texCoordIndex.z;
}
