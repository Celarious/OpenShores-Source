//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: Sc.v.glsl,v 1.13 2019/10/01 19:41:07 daveh Exp $
// Copyright (c) 2026 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#version 150

#define AU_LEGACY_SPACECRAFT_LIGHTING
#ifndef PER_PIXEL_LIGHTING
#SunLight
#endif

#ifndef PER_PIXEL_LIGHTING
uniform LightSources					light;					// Light sources.
#endif
uniform mat4							mv;						// Model view matrix.
uniform mat4							p;						// Projection matrix.
uniform float							vertAlpha;				// Alpha of vertColor.

in vec4									positionShine;			// Vertex location (xyz) and shininess (w).
in vec4									normalReflect;			// Vertex normal (xyz) and reflectiveness (w).
in vec4									texCoordIndex;			// Surface tex coord(xy), texture index(z), surface alpha(w).
in vec4									texEffectAppl;			// Effect tex coord(xy), intensity(z), effect appl(w) 0=no, 1=yes
in vec4									paint;					// Paint (rgb) and its blend amount (a), 0=no paint.
in vec4									glowSelfIllum;			// Glow (rgb), self illumination (w).

#ifdef PER_PIXEL_LIGHTING
out vec4								vertLitScale;
out float								vertLitShininess;
out float								vertLitReflectiveness;
out vec3								vertLitP;
out vec3								vertLitN;
out vec4								vertLitGlowSelfIllum;
#else
out vec4								vertColor;				// Outgoing vertex color.
#endif
out vec3								vertEffCoord;			// Outgoing effect texture coordinate(xy).
out vec4								vertPaint;				// Outgoing paint (rgb) and its blend amount (a), 0=no paint.
out vec2								vertTexCoord;			// Outgoing texture coordinate (xy)
flat out vec2							vertTexIndex;			// Surface texture index(x), effect appl(y)

void main()
{
	// Calculate vertex position in eye space.
	gl_Position = mv * vec4(positionShine.xyz, 1);

	// Calculate lighting using eye space position and normal.
	vec4 eyeN = normalize(mv * vec4(normalReflect.xyz, 1) - mv * vec4(0,0,0,1));	// Surface normal in eye space.
#ifdef PER_PIXEL_LIGHTING
	vertLitScale = vec4(1, 1, 1, texCoordIndex.w * vertAlpha);
	vertLitShininess = positionShine.w;
	vertLitReflectiveness = normalReflect.w;
	vertLitP = gl_Position.xyz;
	vertLitN = eyeN.xyz;
	vertLitGlowSelfIllum = glowSelfIllum;
#else
	vertColor = vec4(computeLighting(light, positionShine.w, normalReflect.w, gl_Position.xyz, eyeN.xyz),
			texCoordIndex.w * vertAlpha);

	// Self Illumination
	vertColor.rgb = mix(vertColor.rgb, vec3(1,1,1), glowSelfIllum.w);

	// Glow
	vertColor.rgb = min(vertColor.rgb + glowSelfIllum.rgb, vec3(1., 1., 1.));
#endif

	gl_Position = p * gl_Position;

	vertPaint = paint;								// Paint color is passed on without changes.
	vertTexCoord = texCoordIndex.xy;				// Surface texture does not get offset.
	vertTexIndex = vec2(texCoordIndex.z, texEffectAppl.w);	// These values do not change or interpolate.
	vertEffCoord = texEffectAppl.xyz;
}
