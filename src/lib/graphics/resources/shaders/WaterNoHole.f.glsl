//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: WaterNoHole.f.glsl,v 1.2 2016/11/08 23:14:52 daveh Exp $
// Copyright (c) 2026 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#version 150

uniform vec3							fogColor;				// Fog color
uniform float							opacity;				// Opacity of water
uniform sampler2D						tex;					// Texture.
uniform sampler2D						texBump;				// Texture.

in vec3									lightColor;				// Incoming fragment color interpolated from the light color.
in vec3									sunColor;				// Incoming fragment color interpolated from the sun color.
in vec3									vertTexCoordFog;		// Incoming texture coordinate (xy) and fog density (z).
in vec2									vertTexCoord2;			// Incoming texture coordinate.
in vec2									vertTexCoord3;			// Incoming texture coordinate.
in vec3									vertUp;					// Vertex 'up' vector in globe space.

in vec3									eyeDir;
in vec3									sunCol0;
in vec3									sunDir0;
in float								sunInt0;
in vec3									sunCol1;
in vec3									sunDir1;
in float								sunInt1;
in vec3									sunCol2;
in vec3									sunDir2;
in float								sunInt2;

out vec4								fragColor;				// Outgoing fragment color.

#ifdef SHADOW_MAPS
#define SHADOW_MAP_CASCADES 3
uniform sampler2DArrayShadow			sunShadowMap;
uniform mat4							sunShadowMapMatrixLocal[SHADOW_MAP_CASCADES*2];
uniform vec4							sunShadowMapCascadeLocal[SHADOW_MAP_CASCADES*2];
uniform vec3							sunShadowMapDirLocal;
uniform vec3							sunShadowMapDir2Local;
uniform vec4							sunShadowMapInfoLocal;

float sunShadowMapSample(int k, vec3 P, vec3 N, float slope, out float edge)
{
	vec3 Q = P + N*(sunShadowMapCascadeLocal[k].x*(.5 + 2.*slope));
	vec3 c = (sunShadowMapMatrixLocal[k]*vec4(Q, 1.)).xyz;
	vec3 e = abs(c*2. - 1.);
	edge = max(max(e.x, e.y), e.z);
	if(edge >= 1.)
		return 1.;
	float depth = c.z - sunShadowMapCascadeLocal[k].y;

	float lit = 0.;
	for(int j=-SHADOW_MAP_PCF; j<=SHADOW_MAP_PCF; j++)
		for(int i=-SHADOW_MAP_PCF; i<=SHADOW_MAP_PCF; i++)
			lit += texture(sunShadowMap, vec4(c.xy + vec2(float(i), float(j))*sunShadowMapInfoLocal.y, float(k), depth));
	return lit/float((2*SHADOW_MAP_PCF + 1)*(2*SHADOW_MAP_PCF + 1));
}

float sunShadowMapLit(vec3 P, vec3 N, vec3 sunPosition)
{
	int count = int(sunShadowMapInfoLocal.x + .5);
	if(count < 1)
		return 1.;
	vec3 L = normalize(sunPosition - P);
	int first = 0;
	if(dot(L, sunShadowMapDirLocal) < .999)
	{
		count = int(sunShadowMapInfoLocal.z + .5);
		if(count < 1 || dot(L, sunShadowMapDir2Local) < .999)
			return 1.;
		first = SHADOW_MAP_CASCADES;
	}
	float NdotL = dot(N, L);
	if(NdotL <= 0.)
		return 1.;
	float slope = sqrt(max(0., 1. - NdotL*NdotL));

	for(int k=0; k<SHADOW_MAP_CASCADES; k++)
	{
		if(k >= count)
			break;
		float edge;
		float lit = sunShadowMapSample(first + k, P, N, slope, edge);
		if(edge >= 1.)
			continue;
		float blend = smoothstep(.8, 1., edge);
		if(blend <= 0.)
			return lit;
		float next = 1.;
		if(k + 1 < count)
		{
			float nextEdge;
			next = sunShadowMapSample(first + k + 1, P, N, slope, nextEdge);
		}
		return mix(lit, next, blend);
	}
	return 1.;
}

in vec3									vertMapP;
in vec3									vertMapN;
in vec3									vertMapSuns;

vec3 sunShadowMapSuns()
{
	vec3 N = normalize(vertMapN);
	float first = sunShadowMapLit(vertMapP, N, vertMapP + sunShadowMapDirLocal*1e6);
	float second = sunShadowMapInfoLocal.z > .5 ? sunShadowMapLit(vertMapP, N, vertMapP + sunShadowMapDir2Local*1e6) : 1.;
	vec3 isFirst = step(.5, vertMapSuns) - step(1.5, vertMapSuns);
	vec3 isSecond = step(1.5, vertMapSuns);
	return vec3(1.) + isFirst*(first - 1.) + isSecond*(second - 1.);
}
#define MAP_SUNS_DECL vec3 mapSuns = sunShadowMapSuns();
#define MAP_SUN0 * mapSuns.x
#define MAP_SUN1 * mapSuns.y
#define MAP_SUN2 * mapSuns.z
#else
#define MAP_SUNS_DECL
#define MAP_SUN0
#define MAP_SUN1
#define MAP_SUN2
#endif

void main()
{
	vec3 waterColor =
		(texture(tex, vertTexCoordFog.xy).rgb + texture(tex, vertTexCoord2).rgb + texture(tex, vertTexCoord3).rgb) / 6;

	// Bump map tuning.
	/*
	{
		// Bump mapping.
		vec3 normDelta = vec3(0,0,1);

		float specularFactor = .5;
		float shininess = 150.0;

		vec3 reflectDir = normalize(reflect(normalize(sunDir0), normDelta));
		float spec0 = max(dot(normalize(eyeDir), reflectDir), 0.0);
		spec0 = pow(spec0, shininess);
		spec0 *= specularFactor * sunInt0;

		reflectDir = normalize(reflect(normalize(sunDir1), normDelta));
		float spec1 = max(dot(normalize(eyeDir), reflectDir), 0.0);
		spec1 = pow(spec1, shininess);
		spec1 *= specularFactor * sunInt1;

		reflectDir = normalize(reflect(normalize(sunDir2), normDelta));
		float spec2 = max(dot(normalize(eyeDir), reflectDir), 0.0);
		spec2 = pow(spec2, shininess);
		spec2 *= specularFactor * sunInt2;

		fragColor.rgb = min(fragColor.rgb + sunCol0 * spec0 + sunCol1 * spec1 + sunCol2 * spec2, vec3(1.,1.,1.));
	}
	*/

	// Bump mapping.
	{
		vec3 normDelta = texture(texBump, vertTexCoord2).rgb - vec3(.5,.5,.5);
		/*
		{
			// Procedurally generated bumps.
			float bumpDensity = 1;
			float bumpSize = .15;

			vec2 c = bumpDensity * vertTexCoordFog.xy;
			vec2 p = fract(c) - vec2(.5, .5);

			float d = dot(p,p);
			float f = inversesqrt(d + 1.);
			if(d >= bumpSize)
			{
				p = vec2(0.0);
				f = 1.0;
			}

			normDelta = vec3(p.x, p.y, 1.) * f;
		}
		*/
		normDelta = normalize(normDelta);

		// Specular highlighting.
		vec3 ed = normalize(eyeDir);
		float specularFactor = .5;
		float shininess = 100.0;

		// Modulate sunlight.
		MAP_SUNS_DECL
		float sunFact;
		vec3 specCol;
		if(sunInt0 > 0)
		{
			vec3 s0 = normalize(sunDir0);
			sunFact = max(dot(normDelta, s0), 0.0) * sunInt0 MAP_SUN0;

			vec3 reflectDir = normalize(reflect(s0, normDelta));
			float spec0 = max(dot(ed, reflectDir), 0.0);
			spec0 = pow(spec0, shininess);
			spec0 *= specularFactor * sunInt0 MAP_SUN0;
			specCol = sunCol0 * spec0;
		}
		else
		{
			sunFact = 0;
			specCol = vec3(0);
		}

		if(sunInt1 > 0)
		{
			vec3 s1 = normalize(sunDir1);
			sunFact += max(dot(normDelta, s1), 0.0) * sunInt1 MAP_SUN1;

			vec3 reflectDir = normalize(reflect(s1, normDelta));
			float spec1 = max(dot(ed, reflectDir), 0.0);
			spec1 = pow(spec1, shininess);
			spec1 *= specularFactor * sunInt1 MAP_SUN1;
			specCol += sunCol1 * spec1;
		}

		if(sunInt2 > 0)
		{
			vec3 s2 = normalize(sunDir2);
			sunFact += max(dot(normDelta, s2), 0.0) * sunInt2 MAP_SUN2;

			vec3 reflectDir = normalize(reflect(s2, normDelta));
			float spec2 = max(dot(ed, reflectDir), 0.0);
			spec2 = pow(spec2, shininess);
			spec2 *= specularFactor * sunInt2 MAP_SUN2;
			specCol += sunCol2 * spec2;
		}

		float ambient = dot(normDelta, vec3(0,0,1));

		fragColor.rgb = min(waterColor * (sunColor * min(sunFact, 1)) + specCol +
			(waterColor * lightColor * vec3(ambient,ambient,ambient)), vec3(1,1,1));
	}

	// Calculate fog effect on fragment color that increases alpha with fog density.
	// Fog density value is 1 for full fog, 0 for no fog.
	fragColor.rgb = mix(fragColor.rgb, fogColor, vertTexCoordFog.z);
	fragColor.a = opacity;
}
