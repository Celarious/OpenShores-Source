//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: FarGlobeLitLavaClouds.f.glsl,v 1.1 2016/09/05 22:06:16 daveh Exp $
// Copyright (c) 2026 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#version 150

#ifdef PER_PIXEL_LIGHTING

struct SunLight
{
	bool								enabled;
	float								attenuation;
	vec3								color;
	vec3								position;
	float								radius;
};

struct LightSources
{
	float								ambient;
	SunLight							sun0;
	SunLight							sun1;
	SunLight							sun2;
};

#ifdef SHADOWS
#define MAX_SHADOW_BODIES 8
uniform int								shadowBodyLocalCount;
uniform vec4							shadowBodyLocal[MAX_SHADOW_BODIES];

float sunUnshadowed(vec3 P, vec3 sunPosition, float sunRadius)
{
	vec3 toSun = sunPosition - P;
	float distSun = max(length(toSun), 1e-6);
	vec3 L = toSun/distSun;
	float angSun = max(sunRadius/distSun, 1e-6);
	float lit = 1.;
	for(int b=0; b<MAX_SHADOW_BODIES; b++)
	{
		if(b >= shadowBodyLocalCount)
			break;
		vec3 toBody = shadowBodyLocal[b].xyz - P;
		float distBody = max(length(toBody), 1e-6);
		float along = dot(toBody, L);
		if(along <= 0. || distBody >= distSun)
			continue;
		float angBody = asin(clamp(shadowBodyLocal[b].w/distBody, 0., 1.));
		float angBetween = atan(length(cross(toBody, L)), along);
		float covers = min(1., (angBody*angBody)/(angSun*angSun));
		lit *= 1. - covers*(1. - smoothstep(abs(angBody - angSun), angBody + angSun + 1e-6, angBetween));
	}
	return lit;
}

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
#endif
#endif

void computeSunLight(SunLight sun,
			vec3 P,
			vec3 N,
		out vec3 oDiffuse)
{
	vec3 toSun = sun.position - P;
	float distSun = length(toSun);
	vec3 L = toSun/distSun;
	float NdotL = dot(N, L);

	float attenuation = 1 / (sun.attenuation * pow(distSun,2));

	float diskang = atan(sun.radius, distSun);
	float sunang = acos(clamp(NdotL, -1., 1.));
	float rd1 = cos(sunang + diskang);
	float rd2 = cos(sunang - diskang);
	NdotL = max(rd1, NdotL);
	NdotL = max(rd2, NdotL);

	float lighting = max(0.0, NdotL);
	oDiffuse = sun.color * lighting * attenuation;
#ifdef SHADOWS

	float unshadowed = sunUnshadowed(P, sun.position, sun.radius);
#ifdef SHADOW_MAPS
	float mapLit = sunShadowMapLit(P, N, sun.position);
#else
	float mapLit = 1.;
#endif
#ifdef SHADOWS_DEBUG
	oDiffuse = mix(vec3(.7, 0., .7), oDiffuse, unshadowed);
	oDiffuse = mix(vec3(0., .6, .7), oDiffuse, mapLit);
#else
	oDiffuse *= unshadowed*mapLit;
#endif
#endif
}

vec3 computeSolarLighting(LightSources light,
			vec3 P,
			vec3 N)
{
	vec3 diffuseLight = vec3(0,0,0);
	vec3 diffuseSum = vec3(0,0,0);

	{
		if(light.sun0.enabled)
		{
			computeSunLight(light.sun0, P, N, diffuseLight);
			diffuseSum += diffuseLight;
		}
		if(light.sun1.enabled)
		{
			computeSunLight(light.sun1, P, N, diffuseLight);
			diffuseSum += diffuseLight;
		}
		if(light.sun2.enabled)
		{
			computeSunLight(light.sun2, P, N, diffuseLight);
			diffuseSum += diffuseLight;
		}
	}

	vec3 color = vec3(light.ambient,light.ambient,light.ambient) + diffuseSum;

	return clamp(color, vec3(0,0,0), vec3(1,1,1));
}

uniform LightSources					light;
#endif

uniform vec4							atmosphereColor;		// Color of the atmosphere. Alpha is the atmosphere density.
uniform vec2							lowMaxWx;				// low layer maximum x=SaturationP, y=VaporP
uniform vec2							lowMinWx;				// low layer minimum x=SaturationP, y=VaporP
uniform sampler2D						lowTexWx;				// Low layer wx texture r=SaturationP, g=VaporP, b=instability
uniform vec2							offWx;					// horiz offset x=SaturationP, y=VaporP
uniform sampler2D						texPlanet;				// Texture of the planet surface.

#ifdef PER_PIXEL_LIGHTING
in vec4									vertLitScale;
in vec3									vertLitP;
in vec3									vertLitN;
#else
in vec4									vertColor;				// Incoming fragment color interpolated from the vertex color.
#endif
in vec2									vertTexCoord;			// Incoming texture coordinate.

out vec4								fragColor;				// Outgoing fragment color.

void main()
{
	// Calculate alpha value due to low cloud layer.
	float lowClouds;
	{
		// Calculate saturation pressure without banding as if flow east is 0 and flow west is 1.
		vec2 tcoord;
		tcoord.y = vertTexCoord.y;
		tcoord.x = fract(vertTexCoord.x - offWx.x + 1);
		vec3 colW = texture(lowTexWx, tcoord).rgb;
		float sp = lowMinWx.x + colW.r * (lowMaxWx.x - lowMinWx.x);
		if(sp > 0)
		{
			// Calculate vapor pressure without banding.
			vec2 vpcoord;
			vpcoord.y = vertTexCoord.y;
			vpcoord.x = fract(vertTexCoord.x - offWx.y + 1);
			float vp = lowMinWx.y + texture(lowTexWx, vpcoord).g * (lowMaxWx.y - lowMinWx.y);
			vp *= 1 + (colW.b - .5) / 100;
			if(vp > sp)
				lowClouds = vp / sp - 1;
			else
				lowClouds = 0;
		}
		else
			lowClouds = 0;
	}

	// Atmosphere alpha is the combined density of the atmosphere and both cloud layers, maxing out at 1.
	float atmoAlpha = min(atmosphereColor.a + lowClouds, 1);
#ifdef PER_PIXEL_LIGHTING
	vec4 vertColor = vec4(computeSolarLighting(light, vertLitP, normalize(vertLitN)), 1) * vertLitScale;
#endif
	vec3 atmoColor = atmosphereColor.rgb * vertColor.rgb;

	// Combine the result.
	// Incoming vertex color provides modulation due to lighting.
	//fragColor = vertColor * vec4(mix(texture(texPlanet, vertTexCoord + vec2(.5,0)).rgb, atmosphereColor.rgb, atmoAlpha), 1);

	// 1-alpha in the far globe texture is a glow factor.
	vec4 surfaceColor = texture(texPlanet, vertTexCoord + vec2(.5,0));
	float glow = 1-surfaceColor.a;								// 1-alpha in the near globe texture is a glow factor.
	vec3 glowColor = vertColor.rgb + vec3(glow,glow,glow);		// Brighten incoming vertex color.
	glowColor = min(glowColor, vec3(1,1,1));
	surfaceColor.rgb *= glowColor;

	fragColor = vec4(mix(surfaceColor.rgb, atmoColor, atmoAlpha), 1);
}
