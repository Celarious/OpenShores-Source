//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: aufireparticles.h,v 1.3 2022/02/19 18:30:40 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef AuFireParticles_H
#define AuFireParticles_H

#include "auParticleSystem.h"

class AUGLOBAL_DLL AuFireParticles : public AuParticleSystem
{
private:
	float								m_Radius;

public:
	AuFireParticles(float radius);
	virtual ~AuFireParticles();
	// Shared unit/building effect. The caller owns and deletes particles.
	static void RenderEffect(AuFireParticles*& particles, const AuMatrix4x4& projection,
		const AuMatrix4x4& model, AuGLEffects& re, float size, float scale = 1.f);

	// Overrides
	virtual void MoveParticle(auParticle* pParticle);
	virtual void InitParticle(auParticle* pParticle);
	virtual bool HasParticleExpired(const auParticle* pParticle);
};

#endif	// AuFireParticles_H
