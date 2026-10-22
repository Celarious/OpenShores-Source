//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: AnimalRingworldFog.f.glsl,v 1.1 2019/10/01 22:45:48 daveh Exp $
// Copyright (c) 2026 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#version 150

#ifdef PER_PIXEL_LIGHTING
#ifndef PER_PIXEL_SKIN_GLOSS
#define PER_PIXEL_SKIN_GLOSS 0.4
#endif
#SunLightRingworld

uniform LightSources					light;
uniform vec4							glowAlpha;
#endif

uniform vec4							bottomSkinColor;		// Color to modulate bottom skin texture color.
uniform vec3							fogColor;				// Fog color
uniform vec4							topSkinColor;			// Color to modulate top skin texture color.
uniform sampler2D						texSkinTop;				// Texture of top skin.
uniform sampler2D						texSkinBottom;			// Texture of bottom skin.
uniform sampler2D						texSkinDetail;			// Texture of decals on the skin.

#ifdef PER_PIXEL_LIGHTING
in vec4									vertLitScale;
in float								vertLitShininess;
in float								vertLitReflectiveness;
in vec3									vertLitP;
in vec3									vertLitN;
#else
in vec4									vertColor;				// Incoming fragment color interpolated from the vertex color.
#endif
in vec4									texCoordTopBottom;		// Incoming texture coordinate top (xy) and bottom (yz)
in vec4									texCoordDetailBlend;	// Detail tex coord (xy), top to bottom blend (z), decal blend (w)
in float								fog;

out vec4								fragColor;				// Outgoing fragment color.

void main()
{
	// Get top and bottom skin colors from their textures.
	vec4 topColor = texture(texSkinTop, texCoordTopBottom.xy);
	vec4 bottomColor = texture(texSkinBottom, texCoordTopBottom.zw);

	// Modulate using skin color values.
	topColor *= topSkinColor;
	bottomColor *= bottomSkinColor;

	// Base skin color is a blend between the bottom texture and the top texture.
	vec4 skinColor = mix(bottomColor, topColor, texCoordDetailBlend.z);

	// Skin decals are blended onto the base skin color.
	vec4 decalColor = texture(texSkinDetail, texCoordDetailBlend.xy);
	skinColor = mix(skinColor, decalColor, texCoordDetailBlend.w * decalColor.a);
	skinColor.a = 1;

#ifdef PER_PIXEL_LIGHTING
	vec4 vertColor = vec4(computeLighting(light, vertLitShininess, vertLitReflectiveness * PER_PIXEL_SKIN_GLOSS, vertLitP, normalize(vertLitN)), 1) * vertLitScale;
	vertColor.rgb = min(vertColor.rgb + glowAlpha.rgb, vec3(1.5, 1.5, 1.5));
#endif
	// Final color is modulated with incoming vertex color, for lighting.
	fragColor = vertColor * skinColor;

	// Calculate fog effect on fragment color but do not modify the alpha using fog.
	// Fog density value is 1 for full fog, 0 for no fog.
	fragColor.rgb = mix(fragColor.rgb, fogColor, fog);
}
