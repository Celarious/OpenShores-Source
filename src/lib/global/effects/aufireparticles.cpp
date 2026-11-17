//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: aufireparticles.cpp,v 1.3 2022/02/19 18:33:12 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "aufireparticles.h"
#include "augleffects.h"
//
#include "auglobal.h"

AuFireParticles::AuFireParticles(float radius) : AuParticleSystem()
{
	m_Radius = radius;
}

AuFireParticles::~AuFireParticles()
{

}

void AuFireParticles::RenderEffect(AuFireParticles*& particles, const AuMatrix4x4& projection,
	const AuMatrix4x4& model, AuGLEffects& re, float size, float scale)
{
	static GLuint texture = 0;
	if(!texture) texture = re.CreateTextureFromBitmap(STATIC_TEXTURE_ID, "tBCM", ":data/particle.bmp");
	if(!particles)
	{
		particles = new AuFireParticles(size);
		particles->SetMaxParticles(1000);
		particles->SetLifeCycle(1.5f);
		particles->SetVelocityVar(size * .7f);
		particles->SetNumToRelease(20);
		particles->SetReleaseInterval(.05f);
		QColor color; color.setRgbF(1, .25, .15);
		particles->SetColor(color);
		particles->SetSize(size);
		particles->SetTextureID(texture);
		particles->SetPosition(AuPoint(0, 0, 0));
		particles->Init(re);
	}
	auto& shader = re.GLSL().SpriteSizeShader();
	shader.Select();
	shader.SetProjectionModelView(projection * model);
	shader.SetTexture(texture);
	shader.SetColor(particles->GetColor());
	shader.SetSize(particles->GetSize() * 100 * c_UnitScale * scale);
	re.glEnable(GL_DEPTH_TEST);
	re.glDepthMask(GL_FALSE);
	re.glEnable(GL_BLEND);
	re.glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	if(particles->Update()) particles->Render(projection, model, re, scale);
	re.glDepthMask(GL_TRUE);
	re.glDisable(GL_BLEND);
	shader.Deselect();
}

bool AuFireParticles::HasParticleExpired(const auParticle* pParticle)
{
	float fTimePassed  = GetCurTime() - pParticle->m_fInitTime;
	if(fTimePassed >= GetLifeCycle())
		return true;
	return false;
}

void AuFireParticles::MoveParticle(auParticle* pParticle)
{
	// Update position with respect to velocity
	pParticle->m_vOldPos = pParticle->m_vCurPos;
	pParticle->m_vCurPos += pParticle->m_vCurVel * GetElapsedTime();

	// Create a vector to draw the particles toward the center, faster as the radius increases.
	double rad = pParticle->m_vCurPos.Length_2D();
	double vx = -pParticle->m_vCurPos.x/rad;
	double vy = -pParticle->m_vCurPos.y/rad;
	double vel = rad/2;
	pParticle->m_vCurVel.x = vel * vx;
	pParticle->m_vCurVel.y = vel * vy;

	// Fire particles rise ever faster over time.
	if(GetVelocityVar() != 0.f)
		pParticle->m_vCurVel.z += GetVelocityVar() * GetElapsedTime();
	else
		pParticle->m_vCurVel.z += GetElapsedTime();
}

void AuFireParticles::InitParticle(auParticle* pParticle)
{
	AuPoint vRandomVec = GetRandomVector();

	pParticle->m_vCurVel = GetVelocity();
	if(GetVelocityVar() != 0.0f)
		pParticle->m_vCurVel += vRandomVec * GetVelocityVar();

	float randAngle = g_Au.Dice().Roll(1,91,-1) * DEG2RADf;		// Random angle from 0 to 90 degrees.
	float radFact = sinf(randAngle);							// Radius factor from 0 to 1.
	vRandomVec.x *= (radFact * m_Radius);
	vRandomVec.y *= (radFact * m_Radius);
	vRandomVec.z *= m_Radius;
	pParticle->m_vCurPos = GetPosition() + vRandomVec;

	pParticle->m_InitDist	= pParticle->m_vCurPos.Length2() + 1;	// Particle is killed when it moves farther away.
	pParticle->m_fInitTime  = GetCurTime();
	pParticle->m_vOldPos	= pParticle->m_vCurPos;
}
