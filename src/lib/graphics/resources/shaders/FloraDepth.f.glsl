#version 150

uniform sampler2D						tex;

in vec2									vertTexCoord;

void main()
{
	if(texture(tex, vertTexCoord).a < .15)
		discard;
}
