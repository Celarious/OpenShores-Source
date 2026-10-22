//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: H_SunLightRingworld.glsl,v 1.2 2019/10/02 14:42:00 daveh Exp $
// Copyright (c) 2026 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|




// Lighting with suns and point light sources. Casts shadows from ringworld shadow panels.

// Defines a point light that shines in all direction and spot lights that are
// confined to a direction and cone of dispersion.
struct PointLight
{
	bool								enabled;			// 1 if enabled
	float								kL;					// Linear light attenuation
	float								kQ;					// Quadratic light attenuation
	vec3								color;				// color of light emitted
	vec3								position;			// position of light source

	// Spot light values define a cone of dispersion for the light. If not a spot light, the light is emitted in all directions.
	float								cosOuterCone;		// outer light cone for spot lights, -1 = full, no cone
	float								cosInnerCone;		// inner light cone for spot lights
	vec3								direction;			// direction to point spot light
};

struct SunLight
{
	bool								enabled;        		// 1 if enabled
	float								attenuation;    		// light attenuation
	vec3								color;          		// color of light emitted
	vec3								position;       		// position of light source, center of sun in eye space
	float								radius;         		// radius of sun
	float								shadowAngle;			// angle of rotation of shadow panel
	float								shadowCount;			// shadow panels in circle
	vec3								shadowXVec;       		// X vector of sun plane for shadow panel calcs
	vec3								shadowYVec;       		// Y vector of sun plane for shadow panel calcs
};

struct LightSources
{
	vec3								ambient;				// Color/intensity of ambient lighting.
	vec3								ambientUp;				// Up vector in eye space for spherical ambient lighting calc.
	PointLight							light[MAX_LIGHTS];
	SunLight							sun0;
	SunLight							sun1;
	SunLight							sun2;
};

#ifdef SHADOWS
#define MAX_SHADOW_BODIES 8
uniform int								shadowBodyCount;
uniform vec4							shadowBody[MAX_SHADOW_BODIES];

float sunUnshadowed(vec3 P, vec3 sunPosition, float sunRadius)
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
	for(int j=-SHADOW_MAP_PCF; j<=SHADOW_MAP_PCF; j++)
		for(int i=-SHADOW_MAP_PCF; i<=SHADOW_MAP_PCF; i++)
			lit += texture(sunShadowMap, vec4(c.xy + vec2(float(i), float(j))*sunShadowMapInfo.y, float(k), depth));
	return lit/float((2*SHADOW_MAP_PCF + 1)*(2*SHADOW_MAP_PCF + 1));
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
	for(int j=-SHADOW_MAP_PCF; j<=SHADOW_MAP_PCF; j++)
		for(int i=-SHADOW_MAP_PCF; i<=SHADOW_MAP_PCF; i++)
			lit += texture(lightShadowMap, vec4(c + vec2(float(i), float(j))*lightShadowParams.z, info.x + face, depth));
	return lit/float((2*SHADOW_MAP_PCF + 1)*(2*SHADOW_MAP_PCF + 1));
}
#endif
#endif

void computeSunLight(SunLight sun,
			float shininess,
			float reflectiveness,
			vec3 P,
			vec3 N,
		out vec3 oDiffuse,
		out vec3 oSpecular)
{
	// Compute the diffuse lighting from center of sun.
	vec3 toSun = sun.position - P;
	float distSun = length(toSun);
	vec3 L = toSun/distSun;										// Unit vector from P pointing toward the light.
	float NdotL = dot(N, L);

	// Compute attenuation.
	float attenuation = 1 / (sun.attenuation * pow(distSun,2));

	// Account for radius of solar disk.
	float diskang = atan(sun.radius, distSun);
	float sunang = acos(clamp(NdotL, -1., 1.));
	float rd1 = cos(sunang + diskang);
	float rd2 = cos(sunang - diskang);
	NdotL = max(rd1, NdotL);
	NdotL = max(rd2, NdotL);

	// Compute specular lighting terms.
	vec3 V = normalize(-P);										// Vector to the eye from P. We are lighting in eye-space.
	vec3 H = normalize(L + V);									// Half-vector for specular lighting.

	// Compute final diffuse lighting result.
	float lighting = max(0.0, NdotL);
	float specular = (NdotL > 0. && shininess >= 1) ? pow(max(0., dot(N,H)), shininess) : 0.;
	oDiffuse = sun.color * lighting * attenuation;
	oSpecular = sun.color * specular * attenuation * reflectiveness;

	// Modulate result by shadow square effect.
	if(sun.shadowCount > 0.)
	{
		vec3 Ps = P - sun.position;								// Position relative to the sun.
		float sunx = dot(Ps, sun.shadowXVec);
		float suny = dot(Ps, sun.shadowYVec);
		float ang = atan(suny, sunx) - sun.shadowAngle;			// Angle between P and shadow square in plane of sun
		float sunlight = sin(ang * sun.shadowCount) * 2.;
		sunlight = clamp(sunlight, 0, 1);						// Prevent negative or excess sunlight.
		oDiffuse *= sunlight;
		oSpecular *= sunlight;
	}
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
	oSpecular *= unshadowed*mapLit;
#endif
}

void computePointLight(PointLight light,
			float shininess,
			float reflectiveness,
			vec3 P,
			vec3 N,
		out vec3 oDiffuse,
		out vec3 oSpecular)
{
	// Compute the diffuse lighting from the point light source.
	vec3 toLight = light.position - P;
	float distLight = length(toLight);
	vec3 L = toLight/distLight;									// Unit vector from P pointing toward the light.
	float NdotL = dot(N, L);

	// Compute attenuation.
	float attenuation = (1 / (light.kL*distLight + light.kQ*pow(distLight,2)));

	// Compute specular lighting terms.
	vec3 V = normalize(-P);										// Vector to the eye from P. We are lighting in eye-space.
	vec3 H = normalize(L + V);									// Half-vector for specular lighting.

	// Compute final diffuse lighting results.
	float lighting = max(0.0, NdotL);
	float specular = (NdotL > 0. && shininess >= 1) ? pow(max(0., dot(N,H)), shininess) : 0.;
	oDiffuse = light.color * lighting * attenuation;
	oSpecular = light.color * specular * attenuation * reflectiveness;
}

#ifdef AU_LEGACY_SPACECRAFT_LIGHTING
uniform int legacySurfaceMode;
#endif

vec3 computeLighting(LightSources light,
			float shininess,
			float reflectiveness,
			vec3 P,
			vec3 N)
{
	// Get a diffuse contribution from each sun that is enabled.
	vec3 diffuseLight = vec3(0,0,0);
	vec3 diffuseSum = vec3(0,0,0);
	vec3 specularLight = vec3(0,0,0);
	vec3 specularSum = vec3(0,0,0);

	// Suns
	{
		if(light.sun0.enabled)
		{
			// This sun light is enabled.
			computeSunLight(light.sun0, shininess, reflectiveness, P, N, diffuseLight, specularLight);
			diffuseSum += diffuseLight;
			specularSum += specularLight;
		}
		if(light.sun1.enabled)
		{
			// This sun light is enabled.
			computeSunLight(light.sun1, shininess, reflectiveness, P, N, diffuseLight, specularLight);
			diffuseSum += diffuseLight;
			specularSum += specularLight;
		}
		if(light.sun2.enabled)
		{
			// This sun light is enabled.
			computeSunLight(light.sun2, shininess, reflectiveness, P, N, diffuseLight, specularLight);
			diffuseSum += diffuseLight;
			specularSum += specularLight;
		}
	}

	// Point lights
	for(int l=0; l<MAX_LIGHTS; l++)
	{
		if(light.light[l].enabled)
		{
			// This other light is enabled.
			// Compute general lighting contribution.
			computePointLight(light.light[l], shininess, reflectiveness, P, N, diffuseLight, specularLight);

			// Compute spotlight effect.
			if(light.light[l].cosOuterCone > -1 || light.light[l].cosInnerCone > -1)
			{
				vec3 Vs = normalize(P - light.light[l].position);
				float cosDirection = dot(Vs, light.light[l].direction);
				float spotEffect = smoothstep(light.light[l].cosOuterCone, light.light[l].cosInnerCone, cosDirection);
				diffuseLight *= spotEffect;
				specularLight *= spotEffect;
			}

#ifdef LIGHT_SHADOW_MAPS
			{
				float lightLit = lightShadowLit(l, P, N, light.light[l].position);
				diffuseLight *= lightLit;
				specularLight *= lightLit;
			}
#endif
			diffuseSum += diffuseLight;
			specularSum += specularLight;
		}
	}

	// Calculate spherical ambient lighting that drops off to zero.
	vec3 ambient = light.ambient * (dot(N, light.ambientUp) + 1)/2;

#ifdef AU_LEGACY_SPACECRAFT_LIGHTING
	// Legacy spacecraft retain the offline renderer's 15% ambient minimum.
	if(legacySurfaceMode != 0)
		ambient = mix(light.ambient * .15, light.ambient, (dot(N, light.ambientUp) + 1.)/2.);
#endif

	// Calculate final color of lit vertex.
	vec3 color = ambient + diffuseSum + specularSum;

	// Clamp color values
	return clamp(color, vec3(0,0,0), vec3(1,1,1));
}

