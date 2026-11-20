#version 150

uniform mat4 mv;
uniform mat4 p;

in vec3 position;
in vec3 texCoordShine;
out vec2 vertTexCoord;

void main()
{
	gl_Position = p * (mv * vec4(position, 1));
	vertTexCoord = texCoordShine.xy;
}
