//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: Flora.f.glsl,v 1.5 2015/08/01 00:08:17 daveh Exp $
// Copyright (c) 2026 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#version 150

flat in float							vertFar;
uniform int								depthPass;

#ifdef PER_PIXEL_LIGHTING

struct PointLight
{
	bool								enabled;
	float								kL;
	float								kQ;
	vec3								color;
	vec3								position;

	float								cosOuterCone;
	float								cosInnerCone;
	vec3								direction;
};

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
	vec3								ambient;
	vec3								ambientUp;
	vec3								globeP;
	PointLight							light[MAX_LIGHTS];
	SunLight							sun0;
	SunLight							sun1;
	SunLight							sun2;
};

#ifdef SHADOWS
#define MAX_SHADOW_BODIES 8
uniform int								shadowBodyCount;
uniform vec4							shadowBody[MAX_SHADOW_BODIES];

float sunUnshadowed(vec3 P, vec3 sunPosition, float sunRadius, vec3 globeP)
{
	vec3 toSun = sunPosition - P;
	float distSun = max(length(toSun), 1e-6);
	vec3 L = toSun/distSun;
	float angSun = max(sunRadius/distSun, 1e-6);
	float lit = 1.;
	for(int b=0; b<MAX_SHADOW_BODIES; b++)
	{
		if(b >= shadowBodyCount)
			break;
		vec3 toBody = shadowBody[b].xyz - P;
		float distBody = max(length(toBody), 1e-6);
		float along = dot(toBody, L);
		if(along <= 0. || distBody >= distSun)
			continue;
		if(distance(shadowBody[b].xyz, globeP) < shadowBody[b].w*.5)
			continue;
		float angBody = asin(clamp(shadowBody[b].w/distBody, 0., 1.));
		float angBetween = atan(length(cross(toBody, L)), along);
		float covers = min(1., (angBody*angBody)/(angSun*angSun));
		lit *= 1. - covers*(1. - smoothstep(abs(angBody - angSun), angBody + angSun + 1e-6, angBetween));
	}
	return lit;
}

#ifdef SHADOW_MAPS
#define SHADOW_MAP_CASCADES 3
uniform sampler2DArrayShadow			sunShadowMap;
uniform mat4							sunShadowMapMatrix[SHADOW_MAP_CASCADES*2];
uniform vec4							sunShadowMapCascade[SHADOW_MAP_CASCADES*2];
uniform vec3							sunShadowMapDir;
uniform vec3							sunShadowMapDir2;
uniform vec4							sunShadowMapInfo;

float sunShadowMapSample(int k, vec3 P, vec3 N, float slope, out float edge)
{
	vec3 Q = P + N*(sunShadowMapCascade[k].x*(.5 + 2.*slope));
	vec3 c = (sunShadowMapMatrix[k]*vec4(Q, 1.)).xyz;
	vec3 e = abs(c*2. - 1.);
	edge = max(max(e.x, e.y), e.z);
	if(edge >= 1.)
		return 1.;
	float depth = c.z - sunShadowMapCascade[k].y;

	float lit = 0.;
	int pcf = vertFar > .5 ? 0 : SHADOW_MAP_PCF;
	for(int j=-pcf; j<=pcf; j++)
		for(int i=-pcf; i<=pcf; i++)
			lit += texture(sunShadowMap, vec4(c.xy + vec2(float(i), float(j))*sunShadowMapInfo.y, float(k), depth));
	return lit/float((2*pcf + 1)*(2*pcf + 1));
}

float sunShadowMapLit(vec3 P, vec3 N, vec3 sunPosition)
{
	int count = int(sunShadowMapInfo.x + .5);
	if(count < 1)
		return 1.;
	vec3 L = normalize(sunPosition - P);
	int first = 0;
	if(dot(L, sunShadowMapDir) < .999)
	{
		count = int(sunShadowMapInfo.z + .5);
		if(count < 1 || dot(L, sunShadowMapDir2) < .999)
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
#ifdef LIGHT_SHADOW_MAPS
#define LIGHT_SHADOW_SLOTS 4
uniform sampler2DArrayShadow			lightShadowMap;
uniform float							lightShadowOf[MAX_LIGHTS];
uniform mat3							lightShadowRot[LIGHT_SHADOW_SLOTS];
uniform vec4							lightShadowInfo[LIGHT_SHADOW_SLOTS];
uniform vec4							lightShadowParams;

float lightShadowLit(int l, vec3 P, vec3 N, vec3 lightPosition)
{
	float s = lightShadowOf[l];
	if(s < -.5 || s + .5 >= lightShadowParams.x)
		return 1.;
	int slot = int(s + .5);
	vec4 info = lightShadowInfo[slot];
	vec3 toLight = lightPosition - P;
	float dist = max(length(toLight), 1e-6);
	float NdotL = dot(N, toLight)/dist;
	if(NdotL <= 0.)
		return 1.;
	float slope = sqrt(max(0., 1. - NdotL*NdotL));

	vec3 Q = P + N*(dist*lightShadowParams.y*(1. + 2.*slope));
	vec3 v = lightShadowRot[slot]*(Q - lightPosition);

	vec3 a = abs(v);
	float m;
	float face;
	vec2 across;
	if(a.x >= a.y && a.x >= a.z)
	{
		m = a.x;
		face = v.x > 0. ? 0. : 1.;
		across = v.yz;
	}
	else if(a.y >= a.z)
	{
		m = a.y;
		face = v.y > 0. ? 2. : 3.;
		across = v.xz;
	}
	else
	{
		m = a.z;
		face = v.z > 0. ? 4. : 5.;
		across = v.xy;
	}
	if(info.y > .5 && face != 4.)
		return 1.;
	if(m <= info.z || m >= info.w)
		return 1.;
	vec2 c = across/(m*lightShadowParams.w)*.5 + .5;
	float nearer = max(info.z, m*.985 - .03);
	float depth = ((info.w + info.z) - 2.*info.w*info.z/nearer)/(info.w - info.z)*.5 + .5;

	float lit = 0.;
	int pcf = vertFar > .5 ? 0 : SHADOW_MAP_PCF;
	for(int j=-pcf; j<=pcf; j++)
		for(int i=-pcf; i<=pcf; i++)
			lit += texture(lightShadowMap, vec4(c + vec2(float(i), float(j))*lightShadowParams.z, info.x + face, depth));
	return lit/float((2*pcf + 1)*(2*pcf + 1));
}
#endif
#endif

void computeSunLight(SunLight sun,
			vec3 P,
			vec3 N,
			vec3 globeP,
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

	vec3 U = normalize(P - globeP);
	float LdotU = dot(L, U);
	float sunang2 = acos(clamp(LdotU, -1., 1.));
	float rd3 = cos(sunang2 + diskang);
	float rd4 = cos(sunang2 - diskang);
	LdotU = max(rd3, LdotU);
	LdotU = max(rd4, LdotU);
	NdotL *= max(0, LdotU);

	float lighting = max(0.0, NdotL);
	oDiffuse = sun.color * lighting * attenuation;
#ifdef SHADOWS

	float unshadowed = sunUnshadowed(P, sun.position, sun.radius, globeP);
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

void computePointLight(PointLight light,
			vec3 P,
			vec3 N,
		out vec3 oDiffuse)
{
	vec3 toLight = light.position - P;
	float distLight = length(toLight);
	vec3 L = toLight/distLight;
	float NdotL = dot(N, L);

	float attenuation = (1 / (light.kL*distLight + light.kQ*pow(distLight,2)));

	float lighting = max(0.0, NdotL);
	oDiffuse = light.color * lighting * attenuation;
}

vec3 computeLighting(LightSources light,
			vec3 P,
			vec3 N)
{
	vec3 diffuseLight = vec3(0,0,0);
	vec3 diffuseSum = vec3(0,0,0);

	{
		if(light.sun0.enabled)
		{
			computeSunLight(light.sun0, P, N, light.globeP, diffuseLight);
			diffuseSum += diffuseLight;
		}
		if(light.sun1.enabled)
		{
			computeSunLight(light.sun1, P, N, light.globeP, diffuseLight);
			diffuseSum += diffuseLight;
		}
		if(light.sun2.enabled)
		{
			computeSunLight(light.sun2, P, N, light.globeP, diffuseLight);
			diffuseSum += diffuseLight;
		}
	}

	for(int l=0; l<MAX_LIGHTS; l++)
	{
		if(light.light[l].enabled)
		{
			computePointLight(light.light[l], P, N, diffuseLight);

			if(light.light[l].cosOuterCone > -1 || light.light[l].cosInnerCone > -1)
			{
				vec3 Vs = normalize(P - light.light[l].position);
				float cosDirection = dot(Vs, light.light[l].direction);
				float spotEffect = smoothstep(light.light[l].cosOuterCone, light.light[l].cosInnerCone, cosDirection);
				diffuseLight *= spotEffect;
			}

#ifdef LIGHT_SHADOW_MAPS
			{
				float lightLit = lightShadowLit(l, P, N, light.light[l].position);
				diffuseLight *= lightLit;
			}
#endif
			diffuseSum += diffuseLight;
		}
	}

	vec3 ambient = mix(light.ambient * .15, light.ambient, (dot(N, light.ambientUp) + 1.)/2.);

	vec3 color = ambient + diffuseSum;

	return clamp(color, vec3(0,0,0), vec3(1,1,1));
}

uniform LightSources					light;
uniform vec4							glowAlpha;
#endif

uniform vec3							fogColor;				// Fog color
uniform sampler2D						tex;					// Texture.

#ifdef PER_PIXEL_LIGHTING
in vec4									vertLitScale;
in vec3									vertLitP;
in vec3									vertLitN;
in vec3									vertGlow;
#else
in vec4									vertColor;				// Incoming fragment color interpolated from the vertex color.
#endif
in vec3									vertTexCoordFog;			// Incoming texture coordinate (xy) and fog density (z)

out vec4								fragColor;				// Outgoing fragment color.
flat in float							vertFade;

float ditherThreshold()
{
	const float bayer[16] = float[16](0., 8., 2., 10., 12., 4., 14., 6., 3., 11., 1., 9., 15., 7., 13., 5.);
	ivec2 p = ivec2(gl_FragCoord.xy) & 3;
	return (bayer[p.y * 4 + p.x] + .5) / 16.;
}

void main()
{
#ifndef FLORA_SHADE_PASS
	if(vertFade != 0.)
	{
		float d = ditherThreshold();
		if(vertFade > 0. ? d < vertFade : d >= -vertFade)
			discard;
	}
#endif
#ifdef PER_PIXEL_LIGHTING
	fragColor = texture(tex, vertTexCoordFog.xy);
#ifndef FLORA_SHADE_PASS
	if(fragColor.a * vertLitScale.a < .15)
		discard;
	if(depthPass != 0)
		return;
#endif

	vec4 vertColor = vec4(computeLighting(light, vertLitP, normalize(vertLitN)), 1) * vertLitScale;
	vertColor.rgb = min(vertColor.rgb+vertGlow, vec3(1.5, 1.5, 1.5));
	fragColor *= vertColor;
#else
	fragColor = texture(tex, vertTexCoordFog.xy) * vertColor;
#ifndef FLORA_SHADE_PASS
	if(fragColor.a < .15)
		discard;
	if(depthPass != 0)
		return;
#endif
#endif

	// Calculate fog effect on fragment color but do not modify the alpha using fog.
	// Fog density value is 1 for full fog, 0 for no fog.
	fragColor.rgb = mix(fragColor.rgb, fogColor, vertTexCoordFog.z);
}
