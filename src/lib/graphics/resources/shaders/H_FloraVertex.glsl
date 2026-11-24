uniform float fogDensity;
uniform vec4 glowAlpha;
#ifndef PER_PIXEL_LIGHTING
uniform LightSources light;
#endif
uniform mat4 mv, p, plantM;
uniform vec3 vForce, vForces[10];
uniform int instanceBase;
uniform samplerBuffer instances;

in vec3 normal, position, texCoordMovement;
out vec3 vertTexCoordFog;
flat out float vertFade, vertFar;
#ifdef PER_PIXEL_LIGHTING
out vec4 vertLitScale;
out vec3 vertLitP, vertLitN;
flat out vec3 vertGlow, vertCityLight;
#else
out vec4 vertColor;
#endif

void main()
{
	mat4 placement = plantM;
	vec4 color = glowAlpha;
	vec3 force = vForce;
	vec3 cityLight = vec3(0.);
	vertFade = 0.;
	vertFar = 0.;
	if(instanceBase >= 0)
	{
		int at = (instanceBase + gl_InstanceID) * FLORA_INSTANCE_TEXELS;
		placement = mat4(texelFetch(instances, at), texelFetch(instances, at + 1),
				texelFetch(instances, at + 2), texelFetch(instances, at + 3));
		vec4 glowWind = texelFetch(instances, at + 4);
		force = vForces[int(glowWind.w)];
		vec4 cityAlpha = texelFetch(instances, at + 5);
		color = vec4(glowWind.rgb, cityAlpha.a);
		cityLight = cityAlpha.rgb;
		vec4 lod = texelFetch(instances, at + 6);
		vertFade = lod.x;
		vertFar = lod.y;
	}

	mat4 mvplant = mv * placement;
	vec4 eyeP = mvplant * vec4(position, 1);
	eyeP.xyz += force * texCoordMovement.z;
	vec3 eyeN = normalize(mat3(mvplant) * normal);
#ifdef PER_PIXEL_LIGHTING
	vertLitScale = vec4(1, 1, 1, color.a);
	vertLitP = eyeP.xyz;
	vertLitN = eyeN;
	vertGlow = color.rgb;
	vertCityLight = cityLight;
#else
	vertColor = vec4(computeLighting(light, eyeP.xyz, eyeN, cityLight), color.a);
	vertColor.rgb = min(vertColor.rgb + color.rgb, vec3(1.5));
#endif
	gl_Position = p * eyeP;
	vertTexCoordFog.xy = texCoordMovement.xy;
	float d = max(gl_Position.z, 0.) * fogDensity;
	vertTexCoordFog.z = 1. - clamp(1. / exp(d*d), 0., 1.);
}
