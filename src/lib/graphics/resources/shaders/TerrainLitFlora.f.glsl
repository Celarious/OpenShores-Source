//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: TerrainLitFlora.f.glsl,v 1.5 2020/09/14 18:51:43 daveh Exp $
// Copyright (c) 2026 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#version 150

uniform float							altNoiseFactor;			// Multiplier for altitude noise.
uniform vec3							fogColor;				// Fog color
uniform sampler2D						texGrass;				// Grass
uniform sampler2D						texLookup;				// 1D lookup table of terrain texture indices by altitude
uniform sampler2D						texNoise;				// Noise texture.
uniform sampler2D						texPavement[7];			// Textures for various kinds of pavement, in WORLDTEX_TYPE order.
uniform sampler2D						texSnow;				// Snow texture.
uniform sampler2D						texTerrain[5];			// Texture palette for terrain.

in vec4									vertColor;				// Incoming fragment color interpolated from the vertex color.
in vec4									vertColorPaved;			// Incoming fragment color without vegetation shading, for pavement.
in vec4									vertColorHardPaved;		// Incoming fragment color for hard pavement, fully lit by the city.
in vec3									vertFogSnowPave;		// Incoming fog density(x), snow coverage(y), pavement coverage(z)
in vec4									vertTexCoordAltVeg;		// Incoming texture coordinate(xy), altitude(z), veg density(w)
flat in float							vertPaveType;			// Pavement type must not be interpolated.

out vec4								fragColor;				// Outgoing fragment color.

void main()
{
	// Noise helps to create irregular boundaries between textures, instead of perfectly straight lines.
	vec4 noiseColor = texture(texNoise, vertTexCoordAltVeg.xy/(2*30));
	float noise = (noiseColor.r + noiseColor.g + noiseColor.b)/3.;				// Range is 0..1 so altitude only goes up, not down.

	// The texLookup texture is only 1 pixel tall. It is created with GL_NEAREST so pixel values are not interpolated.
	// The texTerrain texture is a palette of textures, one for each TER_ELEV_TYPE.
	// The TER_ELEV_TYPE is the red value of the lookup texture, at the interpolated altitude.
	// Determine the starting terrain subtile texture coordinate based on the altitude of this fragment.
	float teralt = clamp(vertTexCoordAltVeg.z + noise*altNoiseFactor, 0., 1023./1024.);	// Terrain altitude with noise.

	// Look up the tile index value from the lookup table. It is in the range 0..1 and must be converted to the tile coordinates.
	float terlu = texture(texLookup, vec2(teralt, 0.)).r;		// Index value in the range 0..1
	int tileIndex = min(int(round(terlu * 5)), 4);				// Terrain texture index 0..4

	// Terrain. Incoming texture coordinate is for smallest pattern so we do not have to multiply texture coordinates to get there.
	// That loses precision. It is much better to divide at this point.
	vec2 terCoord1 = vertTexCoordAltVeg.xy/30.;			// Bigger pattern size.
	vec2 terCoord2 = vertTexCoordAltVeg.xy/(30.*30.);	// Biggest pattern size.
	vec2 terCoord3 = vertTexCoordAltVeg.xy;				// Small pattern size.
	{
		// Samples are taken at different frequencies.
		vec4 terColor1;
		vec4 terColor2;
		if(tileIndex == 0)
		{
			terColor1 = texture(texTerrain[0], terCoord1);
			terColor2 = texture(texTerrain[0], terCoord2);
			fragColor = mix(terColor1, terColor2, .5);
			vec4 terColor3 = texture(texTerrain[0], terCoord3);
			fragColor = mix(fragColor, terColor3, .5);
		}
		else if(tileIndex == 1)
		{
			terColor1 = texture(texTerrain[1], terCoord1);
			terColor2 = texture(texTerrain[1], terCoord2);
			fragColor = mix(terColor1, terColor2, .5);
			vec4 terColor3 = texture(texTerrain[1], terCoord3);
			fragColor = mix(fragColor, terColor3, .5);
		}
		else if(tileIndex == 2)
		{
			terColor1 = texture(texTerrain[2], terCoord1);
			terColor2 = texture(texTerrain[2], terCoord2);
			fragColor = mix(terColor1, terColor2, .5);
			vec4 terColor3 = texture(texTerrain[2], terCoord3);
			fragColor = mix(fragColor, terColor3, .5);
		}
		else if(tileIndex == 3)
		{
			terColor1 = texture(texTerrain[3], terCoord1);
			terColor2 = texture(texTerrain[3], terCoord2);
			fragColor = mix(terColor1, terColor2, .5);
			vec4 terColor3 = texture(texTerrain[3], terCoord3);
			fragColor = mix(fragColor, terColor3, .5);
		}
		else
		{
			terColor1 = texture(texTerrain[4], terCoord1);
			terColor2 = texture(texTerrain[4], terCoord2);
			fragColor = mix(terColor1, terColor2, .5);
			vec4 terColor3 = texture(texTerrain[4], terCoord3);
			fragColor = mix(fragColor, terColor3, .5);
		}
	}

	// Pavement coverage is the signed distance to the pavement edge in feet, negative inside, stored as
	// 0.5 - dist/(2*TERRAIN_PAVE_EDGE_RANGE) so it stays in 0..1. It interpolates linearly across a triangle, so the edge
	// is straight at any angle. Soften the edge over about a pixel, or a foot up close.
	float paveDist = (0.5 - vertFogSnowPave.z) * (2.0 * TERRAIN_PAVE_EDGE_RANGE);
	float paveSoft = max(fwidth(paveDist), 1.0);
	float paveCover = 1.0 - smoothstep(-paveSoft, paveSoft, paveDist);
	bool paveHard = false;										// Hard pavement, set by the pavement block.

	// Pavement
	{
		int paveTileIndex = min(int(round(vertPaveType * 7)), 6);				// Tile index 0..6
		paveHard = paveTileIndex <= TERRAIN_PAVE_HARD_MAX;						// See DaWorldLL::PavementHidesSnow.
		vec4 paveColor;
		if(paveTileIndex == 0)
			paveColor = texture(texPavement[0], terCoord3);
		else if(paveTileIndex == 1)
			paveColor = texture(texPavement[1], terCoord3);
		else if(paveTileIndex == 2)
			paveColor = texture(texPavement[2], terCoord3);
		else if(paveTileIndex == 3)
			paveColor = texture(texPavement[3], terCoord3);
		else if(paveTileIndex == 4)
			paveColor = texture(texPavement[4], terCoord3);
		else if(paveTileIndex == 5)
			paveColor = texture(texPavement[5], terCoord3);
		else
			paveColor = texture(texPavement[6], terCoord3);

		// Combine pavement with terrain color.
		fragColor = mix(fragColor, paveColor, paveCover*paveColor.a);
	}

	// Vegetation
	{
		// The .125 here matches the 1/8 in DaWorldGlobe::CreateGrass.
		float vegden = vertTexCoordAltVeg.w + noise*10./100.;
		if(vegden >= .125f)
		{
			// Samples are taken at different frequencies to reduce the moire patterns a little.
			vec2 grassCoord1 = vertTexCoordAltVeg.xy/(17*30);
			vec2 grassCoord2 = vertTexCoordAltVeg.xy/30;
			vec2 grassCoord3 = vertTexCoordAltVeg.xy*(17./30.);
			vec4 grassColor1 = texture(texGrass, grassCoord1);
			vec4 grassColor2 = texture(texGrass, grassCoord2);
			vec4 grassColor3 = texture(texGrass, grassCoord3);
			vec4 grassColor = mix(grassColor1, grassColor2, .5);
			grassColor = mix(grassColor, grassColor3, .5);

			// Combine grass with terrain color. No grass grows on pavement.
			fragColor = vec4(mix(fragColor.rgb, grassColor.rgb, min(grassColor.a*1.5,1)*(1.0 - paveCover)), 1);
		}
	}

	// Snow
	{
		// Samples are taken at different frequencies to reduce the moire patterns a little.
		vec4 snowColor1 = texture(texSnow, terCoord1);
		vec4 snowColor2 = texture(texSnow, terCoord3);
		vec4 snowColor = mix(snowColor1, snowColor2, .5);

		// Combine snow with terrain color.
		fragColor = mix(fragColor, snowColor, vertFogSnowPave.y);
	}

	// Modulate with vertColor for lighting effect. Pavement is not shaded by vegetation, and hard pavement is fully lit
	// by the city.
	fragColor *= mix(vertColor, paveHard ? vertColorHardPaved : vertColorPaved, paveCover);

	// Calculate fog effect on fragment color but do not modify the alpha using fog.
	// Fog density value is 1 for full fog, 0 for no fog.
	fragColor.rgb = mix(fragColor.rgb, fogColor, vertFogSnowPave.x);
}
