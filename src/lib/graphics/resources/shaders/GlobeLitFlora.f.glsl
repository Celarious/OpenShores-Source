//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: GlobeLitFlora.f.glsl,v 1.3 2016/08/05 21:48:45 daveh Exp $
// Copyright (c) 2026 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#version 150

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
	float								ambient;
	PointLight							light[MAX_LIGHTS];
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
#ifdef LIGHT_SHADOW_MAPS
#define LIGHT_SHADOW_SLOTS 4
uniform sampler2DArrayShadow			lightShadowMap;
uniform float							lightShadowOf[MAX_LIGHTS];
uniform mat3							lightShadowRotLocal[LIGHT_SHADOW_SLOTS];
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
	vec3 v = lightShadowRotLocal[slot]*(Q - lightPosition);

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

	vec3 U = normalize(P);
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

	vec3 upVec = normalize(P);

	float ambient = mix(light.ambient * .15, light.ambient, (dot(N, upVec) + 1.)/2.);

	vec3 color = vec3(ambient,ambient,ambient) + diffuseSum;

	return clamp(color, vec3(0,0,0), vec3(1,1,1));
}

uniform LightSources					light;
#endif

uniform float							altNoiseFactor;			// Multiplier for altitude noise.
uniform float							clipCos;				// Clip area cosine.
uniform vec3							clipUp;					// Clip area 'up' vector in globe space.
uniform vec3							fogColor;				// Fog color
uniform sampler2D						texGrass;				// Grass texture.
uniform sampler2D						texLookup;				// 1D lookup table of terrain tile indices by altitude
uniform sampler2D						texNoise;				// Noise texture.
uniform sampler2D						texSnow;				// Snow texture.
uniform sampler2D						texTerrain[5];			// Texture palette for terrain.

#ifdef PER_PIXEL_LIGHTING
in vec4									vertLitScale;
in vec3									vertLitP;
in vec3									vertLitN;
#else
in vec4									vertColor;				// Incoming fragment color interpolated from the vertex color.
#endif
in vec2									vertFogSnow;			// Incoming fog density(x), snow coverage(y).
in vec4									vertTexCoordAltVeg;		// Incoming texture coordinate(xy), altitude(z), veg density(w)
in vec3									vertUp;					// Vertex 'up' vector in globe space.

out vec4								fragColor;				// Outgoing fragment color.

void main()
{
	// Discard fragments that are too close to the clip area.
	float VdotC = dot(normalize(vertUp), clipUp);
	if(VdotC > clipCos)
		discard;

	// Noise helps to create irregular boundaries between textures, instead of perfectly straight lines.
	vec4 noiseColor = texture(texNoise, vertTexCoordAltVeg.xy/(2*30));
	float noise = (noiseColor.r + noiseColor.g + noiseColor.b)/3.;	// Range is 0..1 so altitude only goes up, not down.

	// The texLookup texture is only 1 pixel tall. It is created with GL_NEAREST so pixel values are not interpolated.
	// The TERTEX_TYPE is the red value of the lookup texture, at the interpolated altitude.
	// Determine the starting terrain subtile texture coordinate based on the altitude of this fragment.
	float teralt = clamp(vertTexCoordAltVeg.z + noise*altNoiseFactor, 0., 1023./1024.);	// Terrain altitude with noise.

	// Look up the tile index value from the lookup table. It is in the range 0..1 and must be converted to the tile coordinates.
	float terlu = texture(texLookup, vec2(teralt, 0)).r;		// Index value in the range 0..1
	int tileIndex = min(int(round(terlu * 5)), 4);				// Terrain texture index 0..4

	// Terrain. Incoming texture coordinate is for smallest pattern so we do not have to multiply texture coordinates to get there.
	// That loses precision. It is much better to divide at this point.
	vec2 terCoord1 = vertTexCoordAltVeg.xy/30.;			// Bigger pattern size.
	vec2 terCoord2 = vertTexCoordAltVeg.xy/(30.*30.);	// Biggest pattern size.
	{
		// Samples are taken at different frequencies.
		vec4 terColor1;
		vec4 terColor2;
		if(tileIndex == 0)
		{
			terColor1 = texture(texTerrain[0], terCoord1);
			terColor2 = texture(texTerrain[0], terCoord2);
		}
		else if(tileIndex == 1)
		{
			terColor1 = texture(texTerrain[1], terCoord1);
			terColor2 = texture(texTerrain[1], terCoord2);
		}
		else if(tileIndex == 2)
		{
			terColor1 = texture(texTerrain[2], terCoord1);
			terColor2 = texture(texTerrain[2], terCoord2);
		}
		else if(tileIndex == 3)
		{
			terColor1 = texture(texTerrain[3], terCoord1);
			terColor2 = texture(texTerrain[3], terCoord2);
		}
		else
		{
			terColor1 = texture(texTerrain[4], terCoord1);
			terColor2 = texture(texTerrain[4], terCoord2);
		}
		fragColor = mix(terColor1, terColor2, .5);
	}

	// Vegetation
	{
		// The .125 here matches the 1/8 in DaWorldGlobe::CreateGrass.
		float vegden = vertTexCoordAltVeg.w + noise*10./100.;
		if(vegden >= .125f)
		{
			vec2 grassCoord1 = vertTexCoordAltVeg.xy/(17*30);
			vec2 grassCoord2 = vertTexCoordAltVeg.xy/30;
			vec4 grassColor1 = texture(texGrass, grassCoord1);
			vec4 grassColor2 = texture(texGrass, grassCoord2);
			vec4 grassColor = mix(grassColor1, grassColor2, .5);
			fragColor = vec4(mix(fragColor.rgb, grassColor.rgb, min(grassColor.a*1.5,1)), 1);
		}
	}

	// Snow
	{
		vec4 snowColor = texture(texSnow, terCoord1);

		// Combine snow with terrain color.
		fragColor = mix(fragColor, snowColor, vertFogSnow.y);
	}

#ifdef PER_PIXEL_LIGHTING
	vec4 vertColor = vec4(computeLighting(light, vertLitP, normalize(vertLitN)), 1) * vertLitScale;
#endif
	// Modulate with vertColor for lighting effect.
	fragColor *= vertColor;

	// Calculate fog effect on fragment color but do not modify the alpha using fog.
	// Fog density value is 1 for full fog, 0 for no fog.
	fragColor.rgb = mix(fragColor.rgb, fogColor, vertFogSnow.x);
}
