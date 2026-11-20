#version 150

uniform int legacySurfaceMode;
uniform vec2 legacyTextureInset;
uniform float legacySurfaceOpacity;
uniform float discardAlpha;

uniform sampler2D						texSurface[15];

in vec2									vertTexCoord;
flat in float							vertTexIndex;

void main()
{
	vec2 surfaceUV = legacySurfaceMode == 0 ? vertTexCoord
		: mix(legacyTextureInset, vec2(1.0) - legacyTextureInset, clamp(vertTexCoord, vec2(0.0), vec2(1.0)));
	float alpha;
	if(vertTexIndex == 0)
		alpha = texture(texSurface[0], surfaceUV).a;
	else if(vertTexIndex == 1)
		alpha = texture(texSurface[1], surfaceUV).a;
	else if(vertTexIndex == 2)
		alpha = texture(texSurface[2], surfaceUV).a;
	else if(vertTexIndex == 3)
		alpha = texture(texSurface[3], surfaceUV).a;
	else if(vertTexIndex == 4)
		alpha = texture(texSurface[4], surfaceUV).a;
	else if(vertTexIndex == 5)
		alpha = texture(texSurface[5], surfaceUV).a;
	else if(vertTexIndex == 6)
		alpha = texture(texSurface[6], surfaceUV).a;
	else if(vertTexIndex == 7)
		alpha = texture(texSurface[7], surfaceUV).a;
	else if(vertTexIndex == 8)
		alpha = texture(texSurface[8], surfaceUV).a;
	else if(vertTexIndex == 9)
		alpha = texture(texSurface[9], surfaceUV).a;
	else if(vertTexIndex == 10)
		alpha = texture(texSurface[10], surfaceUV).a;
	else if(vertTexIndex == 11)
		alpha = texture(texSurface[11], surfaceUV).a;
	else if(vertTexIndex == 12)
		alpha = texture(texSurface[12], surfaceUV).a;
	else if(vertTexIndex == 13)
		alpha = texture(texSurface[13], surfaceUV).a;
	else
		alpha = texture(texSurface[14], surfaceUV).a;

	if(legacySurfaceMode != 0)
	{
		alpha *= legacySurfaceOpacity;
		if(alpha <= discardAlpha) discard;
		if(legacySurfaceMode == 2 && alpha < .999) discard;
		if(legacySurfaceMode == 3 && alpha >= .999) discard;
	}
	else if(alpha <= .01)
		discard;
}
