#ifndef AUSUNSHADOW_H
#define AUSUNSHADOW_H

#include <cmath>

#define SUN_SHADOW_MAP_UNIT 16
#define LIGHT_SHADOW_MAP_UNIT 17

#define SUN_SHADOW_MAP_DEPTH 1.5

#define SUN_SHADOW_MAP_CASTER_REACH 16.

#define SUN_SHADOW_CASCADES 3
#define SUN_SHADOW_SUNS 2
#define SUN_SHADOW_CASCADE_RATIO 4.

struct AuSunShadowFit
{
	double								m_EyeToClip[16];
	double								m_EyeToTex[16];
	double								m_Dir[3];
	double								m_AxisX[3];
	double								m_AxisY[3];
	double								m_Center[3];
	double								m_Radius;
	double								m_TexelWorld;
	double								m_TexelUV;
	double								m_DepthBias;
	double								m_CasterReach;
};

struct AuSunShadowCascades
{
	int									m_Count;
	AuSunShadowFit						m_Fit[SUN_SHADOW_CASCADES];
};

inline bool AuSunShadowFitCompute(const double sun[3], const double refOrigin[3], const double refAxisA[3],
		const double refAxisB[3], double radius, int size, double bias, AuSunShadowFit& fit)
{
	double len = sqrt(sun[0]*sun[0] + sun[1]*sun[1] + sun[2]*sun[2]);
	if(!(len > 1e-6) || !(radius > 0) || size < 16)
		return false;
	double z[3] = { sun[0]/len, sun[1]/len, sun[2]/len };

	double adot = refAxisA[0]*z[0] + refAxisA[1]*z[1] + refAxisA[2]*z[2];
	const double* a = fabs(adot) < .9 ? refAxisA : refAxisB;
	double x[3] = { a[1]*z[2] - a[2]*z[1], a[2]*z[0] - a[0]*z[2], a[0]*z[1] - a[1]*z[0] };
	double xl = sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2]);
	if(!(xl > 1e-9))
		return false;
	x[0] /= xl; x[1] /= xl; x[2] /= xl;
	double y[3] = { z[1]*x[2] - z[2]*x[1], z[2]*x[0] - z[0]*x[2], z[0]*x[1] - z[1]*x[0] };

	double texel = 2*radius/size;
	double ex = -(refOrigin[0]*x[0] + refOrigin[1]*x[1] + refOrigin[2]*x[2]);
	double ey = -(refOrigin[0]*y[0] + refOrigin[1]*y[1] + refOrigin[2]*y[2]);
	double cx = -(ex - floor(ex/texel)*texel);
	double cy = -(ey - floor(ey/texel)*texel);

	double depth = 2*SUN_SHADOW_MAP_DEPTH*radius;
	for(int i=0; i<3; i++)
	{
		fit.m_Dir[i] = z[i];
		fit.m_AxisX[i] = x[i];
		fit.m_AxisY[i] = y[i];
		fit.m_Center[i] = cx*x[i] + cy*y[i];
	}
	fit.m_Radius = radius;
	fit.m_TexelWorld = texel;
	fit.m_TexelUV = 1./size;
	fit.m_DepthBias = bias*.75*texel/depth;
	fit.m_CasterReach = SUN_SHADOW_MAP_CASTER_REACH*radius;

	for(int i=0; i<16; i++)
		fit.m_EyeToClip[i] = fit.m_EyeToTex[i] = 0;
	for(int i=0; i<3; i++)
	{
		fit.m_EyeToClip[i*4] = x[i]/radius;
		fit.m_EyeToClip[i*4 + 1] = y[i]/radius;
		fit.m_EyeToClip[i*4 + 2] = -2*z[i]/depth;
		fit.m_EyeToTex[i*4] = .5*x[i]/radius;
		fit.m_EyeToTex[i*4 + 1] = .5*y[i]/radius;
		fit.m_EyeToTex[i*4 + 2] = -z[i]/depth;
	}
	fit.m_EyeToClip[12] = -cx/radius;
	fit.m_EyeToClip[13] = -cy/radius;
	fit.m_EyeToClip[14] = 0;
	fit.m_EyeToClip[15] = 1;
	fit.m_EyeToTex[12] = .5 - .5*cx/radius;
	fit.m_EyeToTex[13] = .5 - .5*cy/radius;
	fit.m_EyeToTex[14] = .5;
	fit.m_EyeToTex[15] = 1;
	return true;
}

inline bool AuSunShadowCascadesCompute(const double sun[3], const double refOrigin[3], const double refAxisA[3],
		const double refAxisB[3], double farRadius, int size, double bias, AuSunShadowCascades& cascades)
{
	cascades.m_Count = 0;
	for(int k=0; k<SUN_SHADOW_CASCADES; k++)
	{
		double radius = farRadius;
		for(int j=k; j<SUN_SHADOW_CASCADES-1; j++)
			radius /= SUN_SHADOW_CASCADE_RATIO;
		if(!AuSunShadowFitCompute(sun, refOrigin, refAxisA, refAxisB, radius, size, bias, cascades.m_Fit[k]))
			return false;
		cascades.m_Fit[k].m_CasterReach = SUN_SHADOW_MAP_CASTER_REACH*farRadius;
	}
	cascades.m_Count = SUN_SHADOW_CASCADES;
	return true;
}

inline void AuSunShadowFitLocal(const AuSunShadowFit& fit, const double model[16], double localToTex[16], double dir[3],
		double& scale)
{
	for(int c=0; c<4; c++)
		for(int r=0; r<4; r++)
		{
			double v = 0;
			for(int k=0; k<4; k++)
				v += fit.m_EyeToTex[k*4 + r]*model[c*4 + k];
			localToTex[c*4 + r] = v;
		}

	scale = sqrt(model[0]*model[0] + model[1]*model[1] + model[2]*model[2]);
	if(!(scale > 1e-12))
		scale = 1;
	double len = 0;
	for(int i=0; i<3; i++)
	{
		dir[i] = model[i*4]*fit.m_Dir[0] + model[i*4 + 1]*fit.m_Dir[1] + model[i*4 + 2]*fit.m_Dir[2];
		len += dir[i]*dir[i];
	}
	len = sqrt(len);
	if(len > 1e-12)
		for(int i=0; i<3; i++)
			dir[i] /= len;
}

#define LIGHT_SHADOW_SLOTS 4
#define LIGHT_SHADOW_FACES 6
#define LIGHT_SHADOW_FACE_TAN 1.03
#define LIGHT_SHADOW_NEAR .75

struct AuLightShadowSlot
{
	int									m_Light;
	bool								m_ForwardOnly;
	double								m_Near;
	double								m_Far;
	double								m_Pos[3];
	double								m_Rot[9];
};

struct AuLightShadowMaps
{
	int									m_Count;
	int									m_Size;
	AuLightShadowSlot					m_Slot[LIGHT_SHADOW_SLOTS];
};

inline void AuLightShadowFaceMatrix(const AuLightShadowSlot& slot, int face, double eyeToClip[16])
{
	static const int forwardAxis[LIGHT_SHADOW_FACES] = { 0, 0, 1, 1, 2, 2 };
	static const int rightAxis[LIGHT_SHADOW_FACES] = { 1, 1, 0, 0, 0, 0 };
	static const int upAxis[LIGHT_SHADOW_FACES] = { 2, 2, 2, 2, 1, 1 };
	double sign = (face & 1) ? -1 : 1;
	const double* r = &slot.m_Rot[3*rightAxis[face]];
	const double* u = &slot.m_Rot[3*upAxis[face]];
	const double* fr = &slot.m_Rot[3*forwardAxis[face]];
	double f[3] = { sign*fr[0], sign*fr[1], sign*fr[2] };
	const double t = LIGHT_SHADOW_FACE_TAN;
	double a = (slot.m_Far + slot.m_Near)/(slot.m_Far - slot.m_Near);
	double b = -2*slot.m_Far*slot.m_Near/(slot.m_Far - slot.m_Near);
	double rp = 0, up = 0, fp = 0;
	for(int i=0; i<3; i++)
	{
		eyeToClip[i*4] = r[i]/t;
		eyeToClip[i*4 + 1] = u[i]/t;
		eyeToClip[i*4 + 2] = a*f[i];
		eyeToClip[i*4 + 3] = f[i];
		rp += r[i]*slot.m_Pos[i];
		up += u[i]*slot.m_Pos[i];
		fp += f[i]*slot.m_Pos[i];
	}
	eyeToClip[12] = -rp/t;
	eyeToClip[13] = -up/t;
	eyeToClip[14] = -a*fp + b;
	eyeToClip[15] = -fp;
}

inline double AuLightShadowRange(double kL, double kQ)
{
	const double faded = 30;
	double d;
	if(kQ > 1e-12)
		d = (-kL + sqrt(kL*kL + 4*kQ*faded))/(2*kQ);
	else if(kL > 1e-12)
		d = faded/kL;
	else
		d = 300;
	return d < 10 ? 10 : (d > 300 ? 300 : d);
}

enum AuSunShadowFamily
{
	SSF_Structures,
	SSF_Terrain,
	SSF_Creatures,
	SSF_Plants,
	SSF_Count
};

enum AuSunShadowVariant
{
	SSV_Main,
	SSV_Body,
	SSV_Model,
	SSV_Sc,
	SSV_Low,
	SSV_Count
};

struct AuSunShadowPass
{
	const AuSunShadowFit*				m_Fit;
	int									m_Cascade;
	int									m_Family;
	int									m_Variant;
	double								m_Limit[SSF_Count];
	int									m_Asked[SSF_Count];
	int									m_Drawn[SSF_Count];

	inline AuSunShadowPass(const AuSunShadowFit& fit)
	{
		m_Fit = &fit;
		m_Cascade = 0;
		m_Family = SSF_Structures;
		m_Variant = SSV_Main;
		for(int f=0; f<SSF_Count; f++)
		{
			m_Limit[f] = 1e30;
			m_Asked[f] = m_Drawn[f] = 0;
		}
	}

	inline bool Asked(double x, double y, double z, double reach)
	{
		if(m_Cascade == 0)
			m_Asked[m_Family]++;
		return InRange(x, y, z, reach);
	}

	inline bool InRange(double x, double y, double z, double reach) const
	{
		double limit = m_Limit[m_Family] + reach;
		return x*x + y*y + z*z <= limit*limit;
	}

	inline void Drew()
	{
		m_Drawn[m_Family]++;
	}

	inline bool Reaches(double x, double y, double z, double reach) const
	{
		const AuSunShadowFit& f = *m_Fit;
		double px = x - f.m_Center[0], py = y - f.m_Center[1], pz = z - f.m_Center[2];
		double lx = px*f.m_AxisX[0] + py*f.m_AxisX[1] + pz*f.m_AxisX[2];
		double ly = px*f.m_AxisY[0] + py*f.m_AxisY[1] + pz*f.m_AxisY[2];
		double lz = px*f.m_Dir[0] + py*f.m_Dir[1] + pz*f.m_Dir[2];
		double side = f.m_Radius + reach;
		if(fabs(lx) > side || fabs(ly) > side)
			return false;
		return lz >= -SUN_SHADOW_MAP_DEPTH*f.m_Radius - reach && lz <= f.m_CasterReach + reach;
	}
};

struct AuSunShadowSaved
{
	GLint								m_Framebuffer;
	GLint								m_Viewport[4];
	GLint								m_DepthFunc;
	GLfloat								m_OffsetFactor;
	GLfloat								m_OffsetUnits;
	GLboolean							m_Blend;
	GLboolean							m_Cull;
	GLboolean							m_DepthClamp;
	GLboolean							m_DepthMask;
	GLboolean							m_DepthTest;
	GLboolean							m_PolygonOffset;
	GLboolean							m_Scissor;
};

template<class GL> inline bool AuSunShadowMapCreate(GL* gl, int size, GLuint& framebuffer, GLuint& texture,
		int layers = SUN_SHADOW_CASCADES)
{
	GLint oldFramebuffer = 0, oldTexture = 0;
	gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFramebuffer);
	gl->glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &oldTexture);

	gl->glGenTextures(1, &texture);
	gl->glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
	gl->glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, size, size, layers, 0, GL_DEPTH_COMPONENT,
			GL_UNSIGNED_INT, 0);
	gl->glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	gl->glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	gl->glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	gl->glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	gl->glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	gl->glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	gl->glGenFramebuffers(1, &framebuffer);
	gl->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	gl->glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture, 0, 0);
	gl->glDrawBuffer(GL_NONE);
	gl->glReadBuffer(GL_NONE);
	bool ok = gl->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

	gl->glBindFramebuffer(GL_FRAMEBUFFER, oldFramebuffer);
	gl->glBindTexture(GL_TEXTURE_2D_ARRAY, oldTexture);
	if(!ok)
	{
		gl->glDeleteFramebuffers(1, &framebuffer);
		gl->glDeleteTextures(1, &texture);
		framebuffer = 0;
		texture = 0;
	}
	return ok;
}

template<class GL> inline void AuSunShadowMapDelete(GL* gl, GLuint& framebuffer, GLuint& texture)
{
	if(framebuffer)
		gl->glDeleteFramebuffers(1, &framebuffer);
	if(texture)
		gl->glDeleteTextures(1, &texture);
	framebuffer = 0;
	texture = 0;
}

template<class GL> inline void AuSunShadowMapBegin(GL* gl, GLuint framebuffer, int size, AuSunShadowSaved& saved)
{
	gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &saved.m_Framebuffer);
	gl->glGetIntegerv(GL_VIEWPORT, saved.m_Viewport);
	gl->glGetIntegerv(GL_DEPTH_FUNC, &saved.m_DepthFunc);
	gl->glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &saved.m_OffsetFactor);
	gl->glGetFloatv(GL_POLYGON_OFFSET_UNITS, &saved.m_OffsetUnits);
	gl->glGetBooleanv(GL_DEPTH_WRITEMASK, &saved.m_DepthMask);
	saved.m_Blend = gl->glIsEnabled(GL_BLEND);
	saved.m_Cull = gl->glIsEnabled(GL_CULL_FACE);
	saved.m_DepthClamp = gl->glIsEnabled(GL_DEPTH_CLAMP);
	saved.m_DepthTest = gl->glIsEnabled(GL_DEPTH_TEST);
	saved.m_PolygonOffset = gl->glIsEnabled(GL_POLYGON_OFFSET_FILL);
	saved.m_Scissor = gl->glIsEnabled(GL_SCISSOR_TEST);

	gl->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	gl->glViewport(0, 0, size, size);
	gl->glDisable(GL_SCISSOR_TEST);
	gl->glDisable(GL_BLEND);
	gl->glDisable(GL_CULL_FACE);
	gl->glEnable(GL_DEPTH_TEST);
	gl->glDepthFunc(GL_LESS);
	gl->glDepthMask(GL_TRUE);
	gl->glEnable(GL_DEPTH_CLAMP);
	gl->glEnable(GL_POLYGON_OFFSET_FILL);
	gl->glPolygonOffset(2.f, 4.f);
}

template<class GL> inline void AuSunShadowMapLayer(GL* gl, GLuint texture, int layer)
{
	gl->glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture, 0, layer);
	gl->glClear(GL_DEPTH_BUFFER_BIT);
}

template<class GL> inline void AuSunShadowMapEnd(GL* gl, const AuSunShadowSaved& saved, GLuint texture,
		int unit = SUN_SHADOW_MAP_UNIT)
{
	gl->glBindFramebuffer(GL_FRAMEBUFFER, saved.m_Framebuffer);
	gl->glViewport(saved.m_Viewport[0], saved.m_Viewport[1], saved.m_Viewport[2], saved.m_Viewport[3]);
	gl->glDepthFunc(saved.m_DepthFunc);
	gl->glDepthMask(saved.m_DepthMask);
	gl->glPolygonOffset(saved.m_OffsetFactor, saved.m_OffsetUnits);
	if(saved.m_Blend) gl->glEnable(GL_BLEND); else gl->glDisable(GL_BLEND);
	if(saved.m_Cull) gl->glEnable(GL_CULL_FACE); else gl->glDisable(GL_CULL_FACE);
	if(saved.m_DepthClamp) gl->glEnable(GL_DEPTH_CLAMP); else gl->glDisable(GL_DEPTH_CLAMP);
	if(saved.m_DepthTest) gl->glEnable(GL_DEPTH_TEST); else gl->glDisable(GL_DEPTH_TEST);
	if(saved.m_PolygonOffset) gl->glEnable(GL_POLYGON_OFFSET_FILL); else gl->glDisable(GL_POLYGON_OFFSET_FILL);
	if(saved.m_Scissor) gl->glEnable(GL_SCISSOR_TEST); else gl->glDisable(GL_SCISSOR_TEST);

	GLint oldUnit = GL_TEXTURE0;
	gl->glGetIntegerv(GL_ACTIVE_TEXTURE, &oldUnit);
	gl->glActiveTexture(GL_TEXTURE0 + unit);
	gl->glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
	gl->glActiveTexture(oldUnit);
}

#endif
