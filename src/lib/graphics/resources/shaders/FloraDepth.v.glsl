#version 150

uniform mat4							mv;
uniform mat4							p;
uniform mat4							plantM;
uniform vec3							vForce;
uniform int								instanced;
uniform int								instanceBase;
uniform samplerBuffer					instances;
uniform vec3							vForces[10];

in vec3									position;
in vec3									texCoordMovement;

out vec2								vertTexCoord;

void main()
{
	mat4 placement = plantM;
	vec3 glow = vec3(0);
	vec3 force = vForce;
	if(instanced != 0)
	{
		int at = (instanceBase + gl_InstanceID) * 6;
		placement = mat4(texelFetch(instances, at), texelFetch(instances, at + 1), texelFetch(instances, at + 2),
				texelFetch(instances, at + 3));
		vec4 glowWind = texelFetch(instances, at + 4);
		glow = glowWind.rgb;
		force = vForces[int(glowWind.a + .5)];
	}
	vec4 eyeP = (mv * placement) * vec4(position, 1);
	eyeP.xyz += force * texCoordMovement.z;
	gl_Position = p * eyeP;
	vertTexCoord = texCoordMovement.xy;
}
