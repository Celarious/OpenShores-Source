#version 150

uniform mat4							mv;
uniform mat4							p;
uniform mat4							bM[MAX_ANIMAL_BONES];

in vec4									positionShine;
in vec4									boneIndex;
in vec4									boneWeight;

void main()
{
	vec4 netPosition = vec4(0,0,0,0);
	for(int ib=0; ib<4; ib++)
	{
		if(boneWeight[ib] != 0)
		{
			int index = int(boneIndex[ib]);
			netPosition += boneWeight[ib] * (bM[index] * vec4(positionShine.xyz,1));
		}
	}

	gl_Position = p * (mv * netPosition);
}
