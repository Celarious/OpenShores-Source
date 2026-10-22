//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: AnimalHairGlobe.f.glsl,v 1.1 2019/10/01 22:46:09 daveh Exp $
// Copyright (c) 2026 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#version 150

#ifdef PER_PIXEL_LIGHTING
#ifndef PER_PIXEL_HAIR_GLOSS
#define PER_PIXEL_HAIR_GLOSS 0.5
#endif
#SunLightGlobe

uniform LightSources					light;
uniform vec4							glowAlpha;
#endif

uniform sampler2D						texHair;				// Texture of hair.

#ifdef PER_PIXEL_LIGHTING
in vec4									vertLitScale;
in float								vertLitShininess;
in float								vertLitReflectiveness;
in vec3									vertLitP;
in vec3									vertLitN;
#else
in vec4									vertColor;				// Incoming fragment color interpolated from the vertex color.
#endif
in vec2									texCoord;				// Incoming texture coordinate (xy).
in vec4									hairColor;				// Color of the hair.

out vec4								fragColor;				// Outgoing fragment color.

void main()
{
	vec4 texCol = texture(texHair, texCoord);
	//if(texCol.a < .1)
		//discard;
#ifdef PER_PIXEL_LIGHTING
	vec4 vertColor = vec4(computeLighting(light, vertLitShininess, vertLitReflectiveness * PER_PIXEL_HAIR_GLOSS, vertLitP, normalize(vertLitN)), 1) * vertLitScale;
	vertColor.rgb = min(vertColor.rgb + glowAlpha.rgb, vec3(1.5, 1.5, 1.5));
#endif
	fragColor = texCol * hairColor * vertColor;
}
