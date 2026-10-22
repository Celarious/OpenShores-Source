//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: ScRingworld.f.glsl,v 1.3 2019/02/13 17:16:19 daveh Exp $
// Copyright (c) 2026 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#version 150

#ifdef PER_PIXEL_LIGHTING
#SunLightRingworld

uniform LightSources					light;
#endif

uniform float							effectApply;			// How to apply the effect texture: 1=lit by env, 2=unlit(glows)
uniform vec4							glowDiscard;			// Glow(rgb), Alpha below which we discard fragments(a).
uniform sampler2D						texEffect;				// Texture for effects applied to the hull.
uniform sampler2D						texSurface[15];			// Textures for surfaces.

#ifdef PER_PIXEL_LIGHTING
in vec4									vertLitScale;
in float								vertLitShininess;
in float								vertLitReflectiveness;
in vec3									vertLitP;
in vec3									vertLitN;
in vec4									vertLitGlowSelfIllum;
#else
in vec4									vertColor;				// Incoming fragment color interpolated from the vertex color.
#endif
in vec3									vertEffCoord;			// Incoming effect texture coordinate
in vec4									vertPaint;				// Color applied to the surface. Alpha is blend amount.
in vec2									vertTexCoord;			// Incoming texture coordinate (xy)
flat in vec2							vertTexIndex;			// Surface texture index(x), effect(y) 0=no, 1=yes

out vec4								fragColor;				// Outgoing fragment color.

uniform int legacySurfaceMode;
uniform vec2 legacyTextureInset;
uniform float legacySurfaceOpacity;

void main()
{
	vec2 surfaceUV = legacySurfaceMode == 0 ? vertTexCoord
		: mix(legacyTextureInset, vec2(1.0) - legacyTextureInset, clamp(vertTexCoord, vec2(0.0), vec2(1.0)));
	if(vertTexIndex.x == 0)
		fragColor = texture(texSurface[0], surfaceUV);
	else if(vertTexIndex.x == 1)
		fragColor = texture(texSurface[1], surfaceUV);
	else if(vertTexIndex.x == 2)
		fragColor = texture(texSurface[2], surfaceUV);
	else if(vertTexIndex.x == 3)
		fragColor = texture(texSurface[3], surfaceUV);
	else if(vertTexIndex.x == 4)
		fragColor = texture(texSurface[4], surfaceUV);
	else if(vertTexIndex.x == 5)
		fragColor = texture(texSurface[5], surfaceUV);
	else if(vertTexIndex.x == 6)
		fragColor = texture(texSurface[6], surfaceUV);
	else if(vertTexIndex.x == 7)
		fragColor = texture(texSurface[7], surfaceUV);
	else if(vertTexIndex.x == 8)
		fragColor = texture(texSurface[8], surfaceUV);
	else if(vertTexIndex.x == 9)
		fragColor = texture(texSurface[9], surfaceUV);
	else if(vertTexIndex.x == 10)
		fragColor = texture(texSurface[10], surfaceUV);
	else if(vertTexIndex.x == 11)
		fragColor = texture(texSurface[11], surfaceUV);
	else if(vertTexIndex.x == 12)
		fragColor = texture(texSurface[12], surfaceUV);
	else if(vertTexIndex.x == 13)
		fragColor = texture(texSurface[13], surfaceUV);
	else
		fragColor = texture(texSurface[14], surfaceUV);

	if(legacySurfaceMode != 0)
		fragColor.a *= legacySurfaceOpacity;
	if(fragColor.a <= glowDiscard.a)
		discard;
	if(legacySurfaceMode == 1)
		fragColor.a = 1.0;
	else if(legacySurfaceMode == 2 && fragColor.a < .999)
		discard;
	else if(legacySurfaceMode == 3 && fragColor.a >= .999)
		discard;

	// Apply paint.
	fragColor.rgb = mix(fragColor.rgb, vertPaint.rgb, vertPaint.a);

	// Modulate with lighting and effects.
	float appl = vertTexIndex.y*effectApply;
#ifdef PER_PIXEL_LIGHTING
	vec4 vertColor = vec4(computeLighting(light, vertLitShininess, vertLitReflectiveness, vertLitP, normalize(vertLitN)), 1) * vertLitScale;
	vertColor.rgb = mix(vertColor.rgb, vec3(1,1,1), vertLitGlowSelfIllum.w);
	vertColor.rgb = min(vertColor.rgb + vertLitGlowSelfIllum.rgb, vec3(1., 1., 1.));
#endif
	if(appl == 1)
	{
		// Blend in the "lit" effect texture color before modulating with light.
		vec4 litEffect = texture(texEffect, vertEffCoord.xy);	// Effect texture color.
		fragColor.rgb = mix(fragColor.rgb, litEffect.rgb, litEffect.a);

		// Modulate using light-modulated color.
		fragColor *= vertColor;

		// Glow
		fragColor.rgb = min(fragColor.rgb + glowDiscard.rgb, vec3(1., 1., 1.));
	}
	else if(appl == 2)
	{
		// Modulate using light-modulated color.
		fragColor *= vertColor;

		// Glow
		fragColor.rgb = min(fragColor.rgb + glowDiscard.rgb, vec3(1., 1., 1.));

		// Blend in the "unlit" effect texture color after light modulation applied.
		vec4 unlitEffect = texture(texEffect, vertEffCoord.xy);	// Effect texture color.
		unlitEffect *= vertEffCoord.z;							// Modulate intensity of glowing effect texture.
		fragColor.rgb = mix(fragColor.rgb, unlitEffect.rgb, unlitEffect.a);

		// Bring up the alpha to make the effect bright even on transparent surfaces.
		fragColor.a = max(unlitEffect.a, fragColor.a);
	}
	else
	{
		// Modulate using light-modulated color.
		fragColor *= vertColor;

		// Glow
		fragColor.rgb = min(fragColor.rgb + glowDiscard.rgb, vec3(1., 1., 1.));
	}
}
