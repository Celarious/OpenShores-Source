#version 150

uniform sampler2D tex;
uniform float alphaCutoff;
in vec2 vertTexCoord;

void main()
{
	// Match the building's permanent openings; camera-proximity window
	// fading does not change the physical wall that casts the shadow.
	if(texture(tex, vertTexCoord).a < alphaCutoff)
		discard;
}
