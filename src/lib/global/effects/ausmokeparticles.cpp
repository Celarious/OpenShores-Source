//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: ausmokeparticles.cpp,v 1.4 2022/02/19 18:33:30 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "ausmokeparticles.h"
#include "augleffects.h"
#include "auglobal.h"
//

AuSmokeParticles::AuSmokeParticles() : AuParticleSystem()
{
}

AuSmokeParticles::~AuSmokeParticles()
{

}

void AuSmokeParticles::RenderEffect(AuSmokeParticles*& particles, const AuMatrix4x4& projection,
	const AuMatrix4x4& model, AuGLEffects& re, float size, const AuPoint& velocity, float scale)
{
	if(g_Au.m_DebugSwitches & 0x40000000) return;
	static GLuint texture = 0;
	if(!texture) texture = re.CreateTextureFromBitmap(STATIC_TEXTURE_ID, "SmokeT", ":data/smoketransparent.png");
	if(!particles)
	{
		particles = new AuSmokeParticles();
		particles->SetMaxParticles(1000);
		particles->SetLifeCycle(7.5f);
		particles->SetVelocityVar(size / 2.f);
		particles->SetNumToRelease(1);
		particles->SetReleaseInterval(.25f);
		QColor color; color.setRgbF(.5, .5, .5);
		particles->SetColor(color);
		particles->SetSize(size * 100);
		particles->SetTextureID(texture);
		particles->SetPosition(AuPoint(0, 0, 0));
		particles->Init(re);
	}
	particles->SetReleaseInterval(velocity.IsZero() ? .25f : .05f);
	particles->SetVelocity(velocity);
	auto& shader = re.GLSL().SpriteSizeShader();
	shader.Select();
	shader.SetProjectionModelView(projection * model);
	shader.SetTexture(texture);
	shader.SetColor(particles->GetColor());
	shader.SetSize(particles->GetSize() * 10 * c_UnitScale * scale);
	re.glEnable(GL_DEPTH_TEST);
	re.glDepthMask(GL_FALSE);
	re.glEnable(GL_BLEND);
	re.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if(particles->Update()) particles->Render(projection, model, re, scale);
	re.glDepthMask(GL_TRUE);
	re.glDisable(GL_BLEND);
	shader.Deselect();
}

bool AuSmokeParticles::HasParticleExpired(const auParticle* pParticle)
{
	float fTimePassed  = GetCurTime() - pParticle->m_fInitTime;
	if(fTimePassed >= GetLifeCycle())
		return true;
	return false;
}

void AuSmokeParticles::MoveParticle(auParticle* pParticle)
{
	// Update position with respect to velocity
	pParticle->m_vOldPos = pParticle->m_vCurPos;
	pParticle->m_vCurPos += pParticle->m_vCurVel * GetElapsedTime();
}

void AuSmokeParticles::InitParticle(auParticle* pParticle)
{
	pParticle->m_vCurVel = GetVelocity();
	AuPoint vRandomVec = GetRandomVector();
	if(GetVelocityVar() != 0.0f)
		pParticle->m_vCurVel += vRandomVec * GetVelocityVar();
	pParticle->m_vCurPos = GetPosition() + GetRandomVector()/2;
	pParticle->m_InitDist	= pParticle->m_vCurPos.Length2() + 1;	// Particle is killed when it moves farther away.
	pParticle->m_fInitTime  = GetCurTime();
	pParticle->m_vOldPos	= pParticle->m_vCurPos;
}

void AuSmokeParticles::Render(const AuMatrix4x4& projection, const AuMatrix4x4& model, AuGLEffects& re, float scale)
{
	RenderCloud(projection, model, re, scale);
}
