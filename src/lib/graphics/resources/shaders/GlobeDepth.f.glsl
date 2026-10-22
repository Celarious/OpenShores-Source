#version 150

uniform float							clipCos;
uniform vec3							clipUp;

in vec3									vertUp;

void main()
{
	if(dot(normalize(vertUp), clipUp) > clipCos)
		discard;
}
