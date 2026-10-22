#version 150

uniform sampler2D						texSurface[15];

in vec2									vertTexCoord;
flat in float							vertTexIndex;

void main()
{
	float alpha;
	if(vertTexIndex == 0)
		alpha = texture(texSurface[0], vertTexCoord).a;
	else if(vertTexIndex == 1)
		alpha = texture(texSurface[1], vertTexCoord).a;
	else if(vertTexIndex == 2)
		alpha = texture(texSurface[2], vertTexCoord).a;
	else if(vertTexIndex == 3)
		alpha = texture(texSurface[3], vertTexCoord).a;
	else if(vertTexIndex == 4)
		alpha = texture(texSurface[4], vertTexCoord).a;
	else if(vertTexIndex == 5)
		alpha = texture(texSurface[5], vertTexCoord).a;
	else if(vertTexIndex == 6)
		alpha = texture(texSurface[6], vertTexCoord).a;
	else if(vertTexIndex == 7)
		alpha = texture(texSurface[7], vertTexCoord).a;
	else if(vertTexIndex == 8)
		alpha = texture(texSurface[8], vertTexCoord).a;
	else if(vertTexIndex == 9)
		alpha = texture(texSurface[9], vertTexCoord).a;
	else if(vertTexIndex == 10)
		alpha = texture(texSurface[10], vertTexCoord).a;
	else if(vertTexIndex == 11)
		alpha = texture(texSurface[11], vertTexCoord).a;
	else if(vertTexIndex == 12)
		alpha = texture(texSurface[12], vertTexCoord).a;
	else if(vertTexIndex == 13)
		alpha = texture(texSurface[13], vertTexCoord).a;
	else
		alpha = texture(texSurface[14], vertTexCoord).a;

	if(alpha <= .01)
		discard;
}
