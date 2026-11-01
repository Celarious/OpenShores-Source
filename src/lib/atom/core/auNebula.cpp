//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: auNebula.cpp,v 1.74 2022/03/14 15:35:58 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "auNebula.h"
//
#include "auatomrenderinfo.h"
#include "auglobal.h"
#include "aunoise.h"
#include "dagalaxy.h"
#include <QDataStream>
#include <QThread>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#define FARPOINT 2.5980762113533159402911695122588	//.3*sqrt(75);	// Farthest point possible away from nebula to be visible

auNebula::auNebula()
{
	m_CachedNebulaPrepared = false;
	m_SlicedWork = false;
	m_CpuGeometryPrepared = false;
	m_GeometryUploadStage = 0;
	m_GeometryUploadOffset = -1;
	m_CloudDensity = 0;
	m_CloudTexture = 0;
	m_CoreRadPc = DaGalaxy::CoreRadius(ShoresOfHazeron);	// Radius of the galaxy core in pc.
	m_GalaxyRadPc = DaGalaxy::Radius(ShoresOfHazeron);	// Radius of the galaxy in pc.
	m_iParticles = 1048576;
	m_iStars = m_iParticles/40;			// 1/40 the number of particles for the nebula
	m_fA = -2.9924924f;
	m_fB = 0.38151205f;
	m_fC = -1.8401746f;
	m_fD = -1.8524430f;
	m_fParticleSize = 25;
	m_iColor = 0;
	m_bReverseColor = false;
	m_Center = AuPoint(0,0,0);			// This is sector coordinates
	m_Seed = 0;
	m_Type = 0;
	m_fTotalMax = (float)0xffffffff;
	m_img = 0;
}

auNebula::auNebula(int iParticles, int iStars, float fA, float fB, float fC, float fD,
				   float fParticleSize, int iColor, bool bReverse, AuPoint center, int seed, int type, const QColor& galaxyColor)
{
	m_CachedNebulaPrepared = false;
	m_SlicedWork = false;
	m_CpuGeometryPrepared = false;
	m_GeometryUploadStage = 0;
	m_GeometryUploadOffset = -1;
	m_CloudDensity = 0;
	m_CloudTexture = 0;
	m_CoreRadPc = DaGalaxy::CoreRadius(ShoresOfHazeron);	// Radius of the galaxy core in pc.
	m_GalaxyRadPc = DaGalaxy::Radius(ShoresOfHazeron);	// Radius of the galaxy in pc.
	m_iParticles = iParticles;
	m_iStars = iStars;
	m_fA = fA;
	m_fB = fB;
	m_fC = fC;
	m_fD = fD;
	m_fParticleSize = fParticleSize;
	m_GalaxyColor = galaxyColor;
	m_iColor = iColor;
	m_bReverseColor = bReverse;
	m_Center = center;
	m_Seed = seed;
	m_Type = type;
	m_fTotalMax = (float)0xffffffff;
	m_img = 0;
}

auNebula::auNebula(const auNebula &rhs) : m_vNebula(rhs.m_vNebula), m_vStars(rhs.m_vStars), m_vUniStars(rhs.m_vUniStars)
{
	m_CachedNebulaPrepared = rhs.m_CachedNebulaPrepared;
	m_SlicedWork = false;
	m_CpuGeometryPrepared = false;
	m_GeometryUploadStage = 0;
	m_GeometryUploadOffset = -1;
	m_GalacticSector = rhs.m_GalacticSector;
	m_GalacticDice = rhs.m_GalacticDice;
	m_CloudColor = rhs.m_CloudColor;
	m_CloudDensity = rhs.m_CloudDensity;
	m_CloudTexture = rhs.m_CloudTexture;
	m_CoreRadPc = rhs.m_CoreRadPc;
	m_GalaxyRadPc = rhs.m_GalaxyRadPc;
	m_iParticles = rhs.m_iParticles;
	m_iStars = rhs.m_iStars;
	m_fA = rhs.m_fA;
	m_fB = rhs.m_fB;
	m_fC = rhs.m_fC;
	m_fD = rhs.m_fD;
	m_fParticleSize = rhs.m_fParticleSize;
	m_GalaxyColor = rhs.m_GalaxyColor;
	m_iColor = rhs.m_iColor;
	m_bReverseColor = rhs.m_bReverseColor;
	m_Center = rhs.m_Center;
	m_Seed = rhs.m_Seed;
	m_img = 0;

	m_fMaxAngle = rhs.m_fMaxAngle;
	m_fMaxSpeed = rhs.m_fMaxSpeed;
	m_Dimensions = rhs.m_Dimensions;
	m_Dice = rhs.m_Dice;
	m_fTotalMax = rhs.m_fTotalMax;
	m_Type = rhs.m_Type;
}

auNebula::~auNebula()
{
}

void auNebula::DeleteDeviceObjects(AuOpenGLFunctions& GLf)
{
	DeletePreparedGeometry();
	if(m_img)
	{
		GLf.glDeleteTextures(1, &m_img);
		m_img = 0;
	}
	if(m_SkyBoxVAO.VAO())
		m_SkyBoxVAO.DeleteGL();
}

void auNebula::CreateGalacticStarField(const DaGalaxy& galaxy, const AuPoint& sectorLoc, AuDice& dice)
{
	m_vStars.clear();
	const int radius = galaxy.Radius() / 10;
	for(int x = -radius; x <= radius;)
		x = AppendGalacticStarFieldSlice(galaxy, sectorLoc, dice, x);
}

int auNebula::AppendGalacticStarFieldSlice(const DaGalaxy& galaxy, const AuPoint& sectorLoc, AuDice& dice, int ix)
{
	int iDistance = galaxy.Radius() / 10;
	int zDist = galaxy.CoreRadius() / 10;
	int step = sectorLoc.Length() / (iDistance*2);
	if(step < 1)
		step = 1;
	for(int iy=-iDistance; iy<=iDistance; iy+=step)
	{
		for(int iz=-zDist; iz<=zDist; iz+=step)
		{
			AuPoint nbsec(ix, iy, iz);				// Neighboring sector. ix, iy, iz are sectors.
			float fDensity = galaxy.StarDensity(nbsec*10);	// Returns a number from 0 to 1.
			if(fDensity > 0)
			{
				double sdist = (nbsec - sectorLoc).Length() + 1;	// Distance from my sector, in sectors.
				int iStars = (int)ceil(fDensity*50 / (sdist*8));
				for(int i=0; i<iStars; i++)
				{
					float fX = dice.RandomMinMax(-step, step);
					float fY = dice.RandomMinMax(-step, step);
					float fZ = dice.RandomMinMax(-step, step);
					galParticle nuParticle;
					nuParticle.pos = nbsec + AuPoint(fX, fY, fZ) - sectorLoc;
					m_vStars.append(nuParticle);
				}
			}
		}
	}
	return ix + step;
}

void auNebula::PrepareCachedGalacticStarField(const DaGalaxy& galaxy, const AuPoint& sectorLoc,
	const AuDice& dice, quint64 galaxyIdentity)
{
	struct Entry
	{
		GALAXY galaxy;
		quint64 identity;
		AuPoint sector;
		quint32 sequence;
		QVector<galParticle> samples;
		bool ready, failed;
		Entry() : ready(false), failed(false) {}
		quint64 Bytes() const { return quint64(samples.size()) * sizeof(galParticle); }
	};
	static std::mutex cacheMutex;
	static std::condition_variable cacheReady;
	static std::list<std::shared_ptr<Entry> > cache;
	std::shared_ptr<Entry> entry;
	// The LCG is invertible. A copied next value uniquely identifies its state
	// without consuming any values from the sampler's original sequence.
	AuDice sequenceDice(dice);
	const quint32 sequence = sequenceDice.Random();
	bool hit = false;
	{
		std::unique_lock<std::mutex> lock(cacheMutex);
		for(std::list<std::shared_ptr<Entry> >::iterator it = cache.begin(); it != cache.end(); ++it)
		{
			if((*it)->galaxy == galaxy.Galaxy() && (*it)->identity == galaxyIdentity
				&& (*it)->sector == sectorLoc && (*it)->sequence == sequence)
			{
				entry = *it;
				cache.splice(cache.begin(), cache, it);
				hit = true;
				while(!entry->ready) cacheReady.wait(lock);
				if(entry->failed) throw std::runtime_error("Skybox CPU preparation failed.");
				break;
			}
		}
		if(!entry)
		{
			entry.reset(new Entry);
			entry->galaxy = galaxy.Galaxy(); entry->identity = galaxyIdentity;
			entry->sector = sectorLoc; entry->sequence = sequence;
			cache.push_front(entry);
		}
	}
	if(!hit)
	{
		try
		{
			AuDice sampleDice(dice);
			CreateGalacticStarField(galaxy, sectorLoc, sampleDice);
			entry->samples = m_vStars;
			{
				std::lock_guard<std::mutex> lock(cacheMutex);
				entry->ready = true;
				quint64 bytes = 0;
				for(std::list<std::shared_ptr<Entry> >::const_iterator it = cache.begin(); it != cache.end(); ++it)
					if((*it)->ready) bytes += (*it)->Bytes();
				while(cache.size() > 4 || bytes > 128U * 1024U * 1024U)
				{
					std::list<std::shared_ptr<Entry> >::iterator it = cache.end(); --it;
					if(!(*it)->ready) break;
					bytes -= (*it)->Bytes(); cache.erase(it);
				}
			}
			cacheReady.notify_all();
		}
		catch(...)
		{
			{
				std::lock_guard<std::mutex> lock(cacheMutex);
				entry->failed = true; entry->ready = true; cache.remove(entry);
			}
			cacheReady.notify_all();
			throw;
		}
	}
	m_vStars = entry->samples;
}

void auNebula::PrepareCachedNebula()
{
	if(m_CachedNebulaPrepared) return;
	QByteArray key;
	QDataStream out(&key, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_4_1);
	out << m_Seed << m_iParticles << m_iStars << m_iColor << m_bReverseColor
		<< m_Type << m_fA << m_fB << m_fC << m_fD << m_fParticleSize
		<< m_fTotalMax << m_Dimensions;
	struct Entry
	{
		QByteArray key;
		QVector<nebParticle> nebula;
		QVector<galParticle> stars;
		QVector<AuPoint> universe;
		float a, b, c, d, maxSpeed, maxAngle;
		AuPoint dimensions;
		AuDice dice;
		QColor cloudColor;
		bool ready, failed;
		Entry() : ready(false), failed(false) {}
		quint64 Bytes() const
		{
			return quint64(nebula.size()) * sizeof(nebParticle)
				+ quint64(stars.size()) * sizeof(galParticle) + quint64(universe.size()) * sizeof(AuPoint);
		}
	};
	static std::mutex cacheMutex;
	static std::condition_variable cacheReady;
	static std::list<std::shared_ptr<Entry> > cache;
	std::shared_ptr<Entry> entry;
	bool hit = false;
	{
		std::unique_lock<std::mutex> lock(cacheMutex);
		for(std::list<std::shared_ptr<Entry> >::iterator it = cache.begin(); it != cache.end(); ++it)
		{
			if((*it)->key == key)
			{
				entry = *it; cache.splice(cache.begin(), cache, it);
				hit = true;
				while(!entry->ready) cacheReady.wait(lock);
				if(entry->failed) throw std::runtime_error("Skybox CPU preparation failed.");
				break;
			}
		}
		if(!entry)
		{
			entry.reset(new Entry); entry->key = key; cache.push_front(entry);
		}
	}
	if(!hit)
	{
		try
		{
			m_vNebula.clear(); m_vUniStars.clear();
			const int oldStars = m_vStars.size();
			CreateNebula();
			entry->nebula = m_vNebula; entry->universe = m_vUniStars;
			if(m_iStars) entry->stars = m_vStars.mid(oldStars);
			entry->a = m_fA; entry->b = m_fB; entry->c = m_fC; entry->d = m_fD;
			entry->maxSpeed = m_fMaxSpeed; entry->maxAngle = m_fMaxAngle;
			entry->dimensions = m_Dimensions; entry->dice = m_Dice; entry->cloudColor = m_CloudColor;
			{
				std::lock_guard<std::mutex> lock(cacheMutex);
				entry->ready = true;
				quint64 bytes = 0;
				for(std::list<std::shared_ptr<Entry> >::const_iterator it = cache.begin(); it != cache.end(); ++it)
					if((*it)->ready) bytes += (*it)->Bytes();
				while(cache.size() > 4 || bytes > 128U * 1024U * 1024U)
				{
					std::list<std::shared_ptr<Entry> >::iterator it = cache.end(); --it;
					if(!(*it)->ready) break;
					bytes -= (*it)->Bytes(); cache.erase(it);
				}
			}
			cacheReady.notify_all();
		}
		catch(...)
		{
			{
				std::lock_guard<std::mutex> lock(cacheMutex);
				entry->failed = true; entry->ready = true; cache.remove(entry);
			}
			cacheReady.notify_all();
			throw;
		}
	}
	else
	{
		m_vNebula = entry->nebula; m_vUniStars = entry->universe;
		if(m_iStars) m_vStars += entry->stars;
		m_fA = entry->a; m_fB = entry->b; m_fC = entry->c; m_fD = entry->d;
		m_fMaxSpeed = entry->maxSpeed; m_fMaxAngle = entry->maxAngle;
		m_Dimensions = entry->dimensions; m_Dice = entry->dice; m_CloudColor = entry->cloudColor;
	}
	m_CachedNebulaPrepared = true;
}

void auNebula::DeletePreparedGeometry()
{
	if(m_GalaxyCloudVAO.VAO()) m_GalaxyCloudVAO.DeleteGL();
	if(m_GalaxyStarsVAO.VAO()) m_GalaxyStarsVAO.DeleteGL();
	if(m_UniverseStarsVAO.VAO()) m_UniverseStarsVAO.DeleteGL();
	if(m_NebulaVAO.VAO()) m_NebulaVAO.DeleteGL();
	QVector<GLfloat>().swap(m_CloudPositions); QVector<GLfloat>().swap(m_CloudTexCoords);
	QVector<GLfloat>().swap(m_GalaxyPositions); QVector<GLfloat>().swap(m_UniversePositions);
	QVector<GLfloat>().swap(m_NebulaTexCoords); QVector<GLfloat>().swap(m_NebulaColors);
	m_CpuGeometryPrepared = false;
	m_GeometryUploadStage = 0;
	m_GeometryUploadOffset = -1;
}

void auNebula::PrepareSkyBoxGeometry()
{
	if(m_CpuGeometryPrepared) return;
	const int cloudVertices = m_vStars.size() * 6;
	m_CloudPositions.resize(cloudVertices * 3); m_CloudTexCoords.resize(cloudVertices * 2);
	m_GalaxyPositions.resize(m_vStars.size() * 3);
	static const int order[6] = {0,3,2,2,1,0};
	static const GLfloat tex[6][2] = {{0,0},{0,1},{1,1},{1,1},{1,0},{0,0}};
	for(int i = 0; i < m_vStars.size(); ++i)
	{
		const AuPoint& center = m_vStars.at(i).pos;
		m_GalaxyPositions[i*3] = center.x; m_GalaxyPositions[i*3+1] = center.y; m_GalaxyPositions[i*3+2] = center.z;
		AuPoint normal(-center), right;
		normal.Unit(); normal.ArbitraryAxis(right);
		AuPoint up = normal.Cross(right); up.Unit();
		// Keep the original double arithmetic and final float conversion.
		const AuPoint points[4] = {center + (-right-up), center + (right-up),
			center + (right+up), center + (-right+up)};
		for(int corner = 0; corner < 6; ++corner)
		{
			const AuPoint& point = points[order[corner]];
			const int index = (i*6+corner)*3, uv = (i*6+corner)*2;
			m_CloudPositions[index] = point.x; m_CloudPositions[index+1] = point.y; m_CloudPositions[index+2] = point.z;
			m_CloudTexCoords[uv] = tex[corner][0]; m_CloudTexCoords[uv+1] = tex[corner][1];
		}
	}
	m_UniversePositions.resize(m_vUniStars.size()*3);
	for(int i = 0; i < m_vUniStars.size(); ++i)
	{
		m_UniversePositions[i*3] = m_vUniStars.at(i).x;
		m_UniversePositions[i*3+1] = m_vUniStars.at(i).y;
		m_UniversePositions[i*3+2] = m_vUniStars.at(i).z;
	}
	const int nebulaVertices = m_vNebula.size()*6;
	m_NebulaTexCoords.resize(nebulaVertices*2); m_NebulaColors.resize(nebulaVertices*3);
	for(int i = 0; i < m_vNebula.size(); ++i)
		for(int corner = 0; corner < 6; ++corner)
		{
			const int index = (i*6+corner)*3, uv = (i*6+corner)*2;
			m_NebulaColors[index] = m_vNebula.at(i).r;
			m_NebulaColors[index+1] = m_vNebula.at(i).g;
			m_NebulaColors[index+2] = m_vNebula.at(i).b;
			m_NebulaTexCoords[uv] = tex[corner][0]; m_NebulaTexCoords[uv+1] = tex[corner][1];
		}
	m_CpuGeometryPrepared = true;
}

bool auNebula::UploadSkyBoxGeometry(AuGLEffects& re, bool incremental)
{
	PrepareSkyBoxGeometry();
	while(m_GeometryUploadStage < 6)
	{
		AuGLVAO* vao = nullptr;
		QVector<GLfloat>* data = nullptr;
		int buffer = 0, attribute = 0, components = 3, vertices = 0, buffers = 1;
		switch(m_GeometryUploadStage)
		{
		case 0: vao = &m_GalaxyCloudVAO; data = &m_CloudPositions; vertices = m_vStars.size()*6; buffers = 2; break;
		case 1: vao = &m_GalaxyCloudVAO; data = &m_CloudTexCoords; vertices = m_vStars.size()*6; buffers = 2; buffer = attribute = 1; components = 2; break;
		case 2: vao = &m_GalaxyStarsVAO; data = &m_GalaxyPositions; vertices = m_vStars.size(); break;
		case 3: vao = &m_UniverseStarsVAO; data = &m_UniversePositions; vertices = m_vUniStars.size(); break;
		case 4: vao = &m_NebulaVAO; data = &m_NebulaTexCoords; vertices = m_vNebula.size()*6; buffers = 3; buffer = attribute = 1; components = 2; break;
		case 5: vao = &m_NebulaVAO; data = &m_NebulaColors; vertices = m_vNebula.size()*6; buffers = 3; buffer = attribute = 2; break;
		}
		if(data->isEmpty()) { ++m_GeometryUploadStage; m_GeometryUploadOffset = -1; continue; }
		if(!vao->VAO()) vao->CreateGL(re, vertices, buffers);
		vao->SelectGL();
		re.glBindBuffer(GL_ARRAY_BUFFER, vao->Buffer(buffer));
		const qint64 bytes = qint64(data->size()) * sizeof(GLfloat);
		const bool allocate = m_GeometryUploadOffset < 0;
		if(allocate)
		{
			re.glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_STATIC_DRAW);
			re.glVertexAttribPointer(attribute, components, GL_FLOAT, GL_FALSE, 0, 0);
			m_GeometryUploadOffset = 0;
		}
		else
		{
			const qint64 count = incremental ? qMin<qint64>(4*1024*1024, bytes-m_GeometryUploadOffset) : bytes-m_GeometryUploadOffset;
			const char* source = reinterpret_cast<const char*>(data->constData());
			re.glBufferSubData(GL_ARRAY_BUFFER, m_GeometryUploadOffset, count, source+m_GeometryUploadOffset);
			m_GeometryUploadOffset += count;
		}
		re.glBindBuffer(GL_ARRAY_BUFFER, 0);
		vao->DeselectGL();
		if(m_GeometryUploadOffset == bytes)
		{
			QVector<GLfloat>().swap(*data);
			++m_GeometryUploadStage; m_GeometryUploadOffset = -1;
		}
		if(incremental) return m_GeometryUploadStage == 6;
	}
	return true;
}

void auNebula::CreateSkyBox(AuAtomRenderInfo& ari, const AuPoint& pos, unsigned int paneSize, bool mipMap)
{
	BuildSkyBox(*ari.re, pos, paneSize, mipMap, -1);
	ari.ResetViewport();
}

bool auNebula::CreateSkyBoxFace(AuAtomRenderInfo& ari, const AuPoint& pos, unsigned int paneSize, bool mipMap, int face)
{
	if(face < 0 || face > 5 || (face > 0 && !m_img)) return false;
	const bool success = BuildSkyBox(*ari.re, pos, paneSize, mipMap, face);
	ari.ResetViewport();
	return success;
}

bool auNebula::CreateSkyBoxFace(AuGLEffects& re, const AuPoint& pos, unsigned int paneSize, bool mipMap, int face)
{
	if(face < 0 || face > 5 || (face > 0 && !m_img)) return false;
	return BuildSkyBox(re, pos, paneSize, mipMap, face);
}

bool auNebula::BuildSkyBox(AuGLEffects& re, const AuPoint& pos, unsigned int paneSize, bool mipMap, int face)
{
	//qDebug() << "auNebula::CreateSkyBox pos" << pos << "size" << paneSize << "mip" << mipMap;
	if(face <= 0 && !m_CachedNebulaPrepared) DeletePreparedGeometry();
	if(face == 0 && !m_CachedNebulaPrepared)
	{
		// Rebuilding the cube after a graphics-setting change reuses galactic
		// samples, but must regenerate the same universe stars, without appending.
		m_vNebula.clear();
		m_vUniStars.clear();
		if(m_iStars) m_vStars.clear();
		m_fTotalMax = (float)0xffffffff;
	}
	if(face <= 0 && !m_CachedNebulaPrepared) CreateNebula();
	UploadSkyBoxGeometry(re, false);
	//qDebug() << "auNebula::CreateSkyBox nebula created";

	GLuint glowTexture = re.CreateTextureFromBitmap(STATIC_TEXTURE_ID, "StarGlow", ":data/smoketransparent.png",
			GL_LINEAR, GL_LINEAR, false, 2);
	GLuint sunGlowTexture = re.CreateTextureFromBitmap(STATIC_TEXTURE_ID, "StarSpriteDot", ":data/StarSpriteDot.png",
			GL_LINEAR, GL_LINEAR, false, 2);

	// Get current fbo binding. If it is not restored properly, things in the scene blink.
	GLint oldfbo;
	re.glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldfbo);

	// Setup our FBO
	GLuint fbo;
	re.glGenFramebuffers(1, &fbo);
	if(!fbo)
		return false;
	re.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

	// Now setup the environment map texture
	if(face <= 0)
	{
		if(m_img) re.glDeleteTextures(1, &m_img);
		re.glGenTextures(1, &m_img);
		re.glBindTexture(GL_TEXTURE_CUBE_MAP, m_img);
		re.glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		re.glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		re.glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		re.glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if(mipMap)
			re.glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		else
			re.glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		re.glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA8, paneSize, paneSize, 0, GL_RGBA, GL_FLOAT, NULL);
		re.glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA8, paneSize, paneSize, 0, GL_RGBA, GL_FLOAT, NULL);
		re.glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA8, paneSize, paneSize, 0, GL_RGBA, GL_FLOAT, NULL);
		re.glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA8, paneSize, paneSize, 0, GL_RGBA, GL_FLOAT, NULL);
		re.glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA8, paneSize, paneSize, 0, GL_RGBA, GL_FLOAT, NULL);
		re.glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA8, paneSize, paneSize, 0, GL_RGBA, GL_FLOAT, NULL);
	}
	else
		re.glBindTexture(GL_TEXTURE_CUBE_MAP, m_img);

	// Attach it to the FBO so we can render to it
	re.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+0, m_img, 0);
	re.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_CUBE_MAP_POSITIVE_X+1, m_img, 0);
	re.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_CUBE_MAP_POSITIVE_X+2, m_img, 0);
	re.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_CUBE_MAP_POSITIVE_X+3, m_img, 0);
	re.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_CUBE_MAP_POSITIVE_X+4, m_img, 0);
	re.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_CUBE_MAP_POSITIVE_X+5, m_img, 0);

	GLenum status = re.glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
	if(status != GL_FRAMEBUFFER_COMPLETE)
	{
		re.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldfbo);
		re.glDeleteFramebuffers(1, &fbo);
		return false;
	}

	// Determine max total returned fudge factor.  This creates different types of nebula.
	// Ones that are more swirly to ones that are more dispersed making them look more cloudy.
	switch(m_Type)
	{
	case 0:
		m_fTotalMax = (float)0xcfffffff;	// More swirly
		break;
	case 1:
		m_fTotalMax = (float)0xafffffff;
		break;
	case 2:
		m_fTotalMax = (float)0x8fffffff;
		break;
	case 4:
		m_fTotalMax = (float)0x6fffffff;
		break;
	case 5:
		m_fTotalMax = (float)0x4fffffff;
		break;
	case 6:
		m_fTotalMax = (float)0x2fffffff;	// More cloudy
		break;
	default:
		break;
	}

	// Enable some blending and other attributes before drawing
	re.glDepthMask(GL_FALSE);
	re.glEnable(GL_BLEND);
	re.glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	// Calculate position to translate nebula based on its center and player's position
	// Nebula is in Sector coordinates and player's position is Sector Coordinates
	AuPoint trans = m_Center-pos;
	double distance = pos.Distance(m_Center);

	GLuint colorAttachments[6] = 
	{
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2,
		GL_COLOR_ATTACHMENT3,
		GL_COLOR_ATTACHMENT4,
		GL_COLOR_ATTACHMENT5,
	};

	// Set up perspective projection.
	AuMatrix4x4 projection;
	projection.Perspective(90*DEG2RAD, 1, .1, 100000);

	re.glViewport(0,0, paneSize,paneSize);
	for(int i = face < 0 ? 0 : face; i < (face < 0 ? 6 : face + 1); ++i)
	{
		// Then render as normal
		re.glDrawBuffer(colorAttachments[i]);
		re.glClearColor(0,0,0,1);
		re.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// Clear Screen And Depth Buffer

		// Set up the mapping of 3-space to screen space.
		AuMatrix4x4 model;
		// Order is POS_X, NEG_X, POS_Y, NEG_Y, POS_Z, NEG_Z
		if(i == 0)			// POS_X
		{
			model.RotateAxis( -180*DEG2RAD, 0, 0, 1);
			model.RotateAxis( 90*DEG2RAD, 0, 1, 0);
		}
		else if(i == 1)		// NEG_X
		{
			model.RotateAxis( -180*DEG2RAD, 0, 0, 1);
			model.RotateAxis(  -90*DEG2RAD, 0, 1, 0);
		}
		else if(i == 2)		// POS_Y
		{
			model.RotateAxis(  -90*DEG2RAD, 1, 0, 0);
		}
		else if(i == 3)		// NEG_Y
		{
			model.RotateAxis( 90*DEG2RAD, 1, 0, 0);
		}
		else if(i == 4)		// POS_Z
		{
			model.RotateAxis( -180*DEG2RAD, 1, 0, 0);
		}
		else if(i == 5)		// NEG_Z
		{
			model.RotateAxis( -180*DEG2RAD, 0, 0, 1);
		}

		// Always draw stars. Since the stars are tiny they can be rendered as point sprites.
		// Unfortunately when the location of point sprites is off the screen
		// they get clipped on ATI cards.  On NVIDIA cards the point size is
		// taken into account before clipping.  So that means our larger point sprites
		// must be drawn as quads to support ATI cards.
		float starDotSize = paneSize / 2048.f;	// This is used more as a scaling factor than a size.
		RenderStars(starDotSize, re, projection, model, sunGlowTexture, pos);

		// Immutable unit cloud geometry serves all six faces.
		RenderGalacticCloud(re, projection, model, glowTexture);

		// Draw the nebula.
		if(m_iParticles > 0)
		{
			GLuint nebTex = re.CreateTextureFromBitmap(STATIC_TEXTURE_ID, "Ring", ":data/txAsteroids.png");

			model.Translate(trans);
			RenderNebula(distance, re, projection, model, nebTex);
		}
	}

	// Disable blending and other attributes now that drawing is done
	re.glDisable(GL_BLEND);
	re.glDepthMask(GL_TRUE);

	if(mipMap && (face < 0 || face == 5))
	{
		re.glActiveTexture(GL_TEXTURE0);
		re.glBindTexture(GL_TEXTURE_CUBE_MAP, m_img);
		re.glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	}

	// Cleanup frame buffer.
	re.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+0, 0, 0);
	re.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_CUBE_MAP_POSITIVE_X+1, 0, 0);
	re.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_CUBE_MAP_POSITIVE_X+2, 0, 0);
	re.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_CUBE_MAP_POSITIVE_X+3, 0, 0);
	re.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_CUBE_MAP_POSITIVE_X+4, 0, 0);
	re.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_CUBE_MAP_POSITIVE_X+5, 0, 0);

	re.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldfbo);	// Unbind frame buffer

	re.glDeleteFramebuffers(1, &fbo);
	if(face < 0 || face == 5) DeletePreparedGeometry();
	//qDebug() << "auNebula::CreateSkyBox done";
	return true;
}

void auNebula::DrawPreparedGeometry(AuGLEffects& re, AuGLVAO& vao, GLenum mode, int vertices, int attributes)
{
	if(!m_SlicedWork) { vao.DrawGL(mode); return; }
	for(int attribute = 0; attribute < attributes; ++attribute) re.glEnableVertexAttribArray(attribute);
	// Whole quads retain primitive and additive blending order across batches.
	const int batch = mode == GL_TRIANGLES ? 65532 : 65536;
	for(int first = 0; first < vertices; first += batch)
	{
		re.glDrawArrays(mode, first, qMin(batch, vertices-first));
		re.glFlush();
		QThread::yieldCurrentThread();
	}
	for(int attribute = 0; attribute < attributes; ++attribute) re.glDisableVertexAttribArray(attribute);
}

void auNebula::RenderGalacticCloud(AuGLEffects& re, const AuMatrix4x4& projection,
	const AuMatrix4x4& model, GLuint texture)
{
	if(m_vStars.isEmpty()) return;
	re.GLSL().GalaxyCloudShader().Select();
	re.GLSL().GalaxyCloudShader().SetProjection(projection);
	re.GLSL().GalaxyCloudShader().SetModelView(model);
	re.GLSL().GalaxyCloudShader().SetTexture(texture);
	re.GLSL().GalaxyCloudShader().SetColor(m_GalaxyColor);
	m_GalaxyCloudVAO.SelectGL();
	DrawPreparedGeometry(re, m_GalaxyCloudVAO, GL_TRIANGLES, m_vStars.size()*6, 2);
	m_GalaxyCloudVAO.DeselectGL();
	re.GLSL().GalaxyCloudShader().Deselect();
}

void auNebula::RenderSkyBox(AuGLEffects& re, const AuMatrix4x4& projection, const AuMatrix4x4& model, float opacity)
{
	Q_UNUSED(projection);
	if(!m_SkyBoxVAO.VAO())
	{
		// The skybox shader calculates texture coordinates from the position values of the vertices.
		AuGlobeLLTriList tris;
		tris.CreateGlobeTriangles(8);

		// Store count of triangles in the vertex array object.
		int vertices = tris.count()*3;

		// Copy surface triangles into gl array buffers.
		GLfloat* position = new GLfloat[vertices*3];	// 'in vec3 position' variable in the vertex shader.

		double radius = FeetPerParsec();
		unsigned int pidx = 0;
		for(AuGlobeLLTriList::const_iterator ti = tris.constBegin(); ti != tris.constEnd(); ti++)
		{
			// Calculate triangle vertex coordinates.
			AuPoint terCoord0 = (*ti).m_LL0.GlobeLocation(radius);
			AuPoint terCoord1 = (*ti).m_LL1.GlobeLocation(radius);
			AuPoint terCoord2 = (*ti).m_LL2.GlobeLocation(radius);

			// Vertex 0
			position[pidx++] = (float)terCoord0.x;
			position[pidx++] = (float)terCoord0.y;
			position[pidx++] = (float)terCoord0.z;

			// Vertex 1
			position[pidx++] = (float)terCoord2.x;
			position[pidx++] = (float)terCoord2.y;
			position[pidx++] = (float)terCoord2.z;

			// Vertex 2
			position[pidx++] = (float)terCoord1.x;
			position[pidx++] = (float)terCoord1.y;
			position[pidx++] = (float)terCoord1.z;
		}

		// Generate vertex array object with a buffer for each array.
		m_SkyBoxVAO.CreateGL(re, vertices, 1);

		m_SkyBoxVAO.SelectGL();

		// bind buffer for positions and copy data into buffer.
		re.glBindBuffer(GL_ARRAY_BUFFER, m_SkyBoxVAO.Buffer(0));
		re.glBufferData(GL_ARRAY_BUFFER, pidx*sizeof(GLfloat), position, GL_STATIC_DRAW);
		re.glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);		// Describe the data in the buffer.

		re.glBindBuffer(GL_ARRAY_BUFFER, 0);

		m_SkyBoxVAO.DeselectGL();

		// OpenGL has the data now. I can discard my local buffers.
		delete [] position;
	}

	// Modulate the color of the skybox with the opacity value. This is a lot less work than blending since nothing is already
	// on the screen that we care to blend with.
	re.GLSL().SkyBoxShader().Select(projection);
	re.GLSL().SkyBoxShader().SetModelView(model);
	re.GLSL().SkyBoxShader().SetOpacity(opacity);
	re.GLSL().SkyBoxShader().SetTexture(m_img);

	//re.glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	re.glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	m_SkyBoxVAO.SelectGL();
	m_SkyBoxVAO.DrawGL(GL_TRIANGLES);
	m_SkyBoxVAO.DeselectGL();
	//re.glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	//re.glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	re.GLSL().SkyBoxShader().Deselect();
}

void auNebula::Swap(float num1, float num2)
{
	float fStore = num1;
	num1 = num2;
	num2 = fStore;
}

// 
void auNebula::ChooseOrder()
{
	// There are 24 different combinations we can make with the 4 numbers
	// Right now we are only doing 6 of those combinations.
	int iCombo = m_Dice.Roll(1, 6);
	switch(iCombo)
	{
		case 2:
			if(fabs(m_fC) > 1.125)
				Swap(m_fC, m_fD);
			break;
		case 3:
			Swap(m_fB, m_fC);
			break;
		case 4:
			if(m_fB > 0)
			{
				Swap(m_fB, m_fC);
				Swap(m_fB, m_fD);
			}
			break;
		case 5:
			Swap(m_fB, m_fD);
			Swap(m_fB, m_fC);
			break;
		case 6:
			Swap(m_fB, m_fD);
			break;
		default:
			break;
	}
}

// Create the Nebula model.
// Remember that the model will be viewed from six different angles,
// Front, Back, Top, Bottom, Left, and Right.  This will create the six textures
// to use for the sky box in the scene.  You can have all 4 values be randomly
// generated rather than use the method below.  Beware though, because most of the time
// the values converge and you end up with congested points rather than a cloudy
// like substance.  This current method tries to eliminate the possibility of that
// type of occurrence by generating 1 random number and perturbing that value
// for the other values in the algorithm.  This method has given the best
// results in terms of nebula that do not converge to congested points.
void auNebula::CreateNebula()
{
	AuPoint oldPos;
	AuPoint nuPos;
	// Note we generate random FLOATS because
	// a .1 difference between any of the numbers changes
	// the overall result.
	m_Dice.Seed(m_Seed);
	m_fB = m_Dice.RandomMinMax(-3, 3, m_fTotalMax);
	if(m_fB < 0 && m_fB > -0.5)
		m_fB = m_fB-m_Dice.RandomMinMax(.5, .875, m_fTotalMax);
	if(m_fB > 0 && m_fB < 0.5)
		m_fB = m_fB+m_Dice.RandomMinMax(.5, .875, m_fTotalMax);
	m_fA = fabs(m_fB)+1.5;
	if(m_fA > 3)
		m_fA = 3;
	float fRand = m_Dice.RandomMinMax(0, 2, m_fTotalMax);
	if(fRand <= 1)
		m_fA = -m_fA;
	if(fRand < .25)
	{
		m_fD = -m_fA;
		m_fC = -m_fB;
	}
	else if(fRand < .5)
	{
		m_fD = m_fA;
		m_fC = -m_fB;
	}
	else if(fRand < .75)
	{
		m_fD = -m_fA;
		m_fC = m_fB;
	}
	else
	{
		m_fD = m_fA;
		m_fC = m_fB;
	}

	// Now randomly order them, sometimes there is no change
	ChooseOrder();

	m_fMaxSpeed = 0;
	m_fMaxAngle = 0;
	for(int i=0; i<m_iParticles; i++)
	{
		nuPos.x = sin(m_fA * oldPos.y) - oldPos.z * cos(m_fB * oldPos.x);
		nuPos.y = oldPos.z * sin(m_fC * oldPos.x) - cos(m_fD * oldPos.y);
		nuPos.z = sin(oldPos.x);
		float speed = nuPos.Distance(oldPos);	// Distance between the current and last point
		float angle = 0;
		if(i != 0 && i != m_iParticles-2)
		{
			// Law of cosines
			// b^2 = a^2 + c^2 - 2ac * cos(B)
			float c = oldPos.Distance(nuPos);
			AuPoint nextPos;
			nextPos.x = sin(m_fA * nuPos.y) - nuPos.z * cos(m_fB * nuPos.x);
			nextPos.y = nuPos.z * sin(m_fC * nuPos.x) - cos(m_fD * nuPos.y);
			nextPos.z = sin(nuPos.x);
			float a = nuPos.Distance(nextPos);
			float b = oldPos.Distance(nextPos);
			float acosNum = ( b*b - a*a - c*c ) / (-2*a*c);
			angle = acos(acosNum);
		}
		nebParticle nuParticle;
		nuParticle.r = 1;
		nuParticle.g = 1;
		nuParticle.b = 1;
		nuParticle.pos = nuPos;
		nuParticle.speed = speed;
		nuParticle.angle = angle;
		if(m_fMaxSpeed < speed)
			m_fMaxSpeed = speed;
		if(m_fMaxAngle < angle)
			m_fMaxAngle = angle;
		m_Dimensions = m_Dimensions.Max(nuPos);
		m_vNebula.append(nuParticle);	// Add Particle to list
		oldPos = nuPos;				// Keep last position for next pass
	}

	// Create starry background for nebula
	if(m_iStars == 0)
	{
		// We are not drawing any background stars in our galaxy
		// Instead draw the gackground stars in the Universe
		// This number of stars is always the same
		for(int i=0; i<10000; i++)
		{
			// getRandomVector returns a number between -1 and 1
			AuPoint vect = GetRandomVector();
			m_vUniStars.append(vect);
		}
	}
	else
	{
		for (int i=0; i<m_iStars; i++)
		{
			// getRandomVector returns a number between -1 and 1
			galParticle nuParticle;
			nuParticle.pos = GetRandomVector();
			m_vStars.append(nuParticle);
		}
	}

	// Calculate Color
	CalculateColor();
}

void auNebula::CalculateColor()
{
	if(m_vNebula.isEmpty())
	{
		m_CloudColor = QColor(0,0,0);
		return;
	}

	// Calculate Base Color
	double r = 0;
	double g = 0;
	double b = 0;
	float fDiv = .01f;
	float fMod = .005f;
	float fFact = .25f;
	if(m_iColor == 0)			// Green and Blue
	{
		// skybox blue and light green, nebula teal.
		r = fFact*fDiv;
		g = fDiv;
		b = fMod;
	}
	else if(m_iColor == 1)		// Light Green and Light Blue
	{
		// skybox green and light blue, nebula teal.
		r = fFact*fDiv;
		g = fMod;
		b = fDiv;
	}
	else if(m_iColor == 2)		// Light Pink, Light Purple, and Blue
	{
		// skybox blue and purple, nebula magenta
		r = fDiv;
		g = fFact*fDiv;
		b = fMod;
	}
	else if(m_iColor == 3)		// Dark Pink, Dark Purple, and Red
	{
		// skybox red and purple, nebula magenta
		r = fMod;
		g = fFact*fDiv;
		b = fDiv;
	}
	else if(m_iColor == 4)		// Yellow-Green and Yellow-Orange
	{
		// skybox green and yellow, nebula yellow
		r = fDiv;
		g = fMod;
		b = fFact*fDiv;
	}
	else if(m_iColor == 5)		// Yellow-Green and Red-Orange
	{
		// skybox red and yellow, nebula yellow
		r = fMod;
		g = fDiv;
		b = fFact*fDiv;
	}

	float Rd = 0;
	float Gd = 0;
	float Bd = 0;
	//int Ad = 0;
	const float kR = 1.f;
	const float kG = 1.f;
	const float kB = 1.f;
	const float kA = 1.f;

	for(QVector<nebParticle>::iterator it = m_vNebula.begin(); it != m_vNebula.end(); it++)
	{
		float angleCol = (*it).angle/m_fMaxAngle;
		float speedCol = (*it).speed/m_fMaxSpeed;
		if(m_bReverseColor)
		{
			angleCol = 1-angleCol;
			speedCol = 1-speedCol;
		}

		if(m_iColor == 0)			// Green and Blue
		{
			(*it).r = angleCol*r;
			(*it).g = speedCol*g;
			(*it).b = b;
		}
		else if(m_iColor == 1)		// Light Green and Light Blue
		{
			(*it).r = angleCol*r;
			(*it).g = g;
			(*it).b = speedCol*b;
		}
		else if(m_iColor == 2)		// Light Pink, Light Purple, and Blue
		{
			(*it).r = speedCol*r;
			(*it).g = angleCol*g;
			(*it).b = b;
		}
		else if(m_iColor == 3)		// Dark Pink, Dark Purple, and Red
		{
			(*it).r = r;
			(*it).g = angleCol*g;
			(*it).b = speedCol*b;
		}
		else if(m_iColor == 4)		// Yellow-Green and Yellow-Orange
		{
			(*it).r = speedCol*r;
			(*it).g = g;
			(*it).b = angleCol*b;
		}
		else if(m_iColor == 5)		// Yellow-Green and Red-Orange
		{
			(*it).r = r;
			(*it).g = speedCol*g;
			(*it).b = angleCol*b;
		}

		if(Rd >= kR || Gd >= kG || Bd >= kB)
			continue;

		// Calculate source color.
		// Mimic glBlend with glBlendFunc(GL_SRC_ALPHA, GL_ONE)
		float Rs = (*it).r;
		float Gs = (*it).g;
		float Bs = (*it).b;
		float As = .01f;

		// GL_SRC_ALPHA source scaling factors.
		float sR = As/kA;
		float sG = As/kA;
		float sB = As/kA;

		Rd = Rs*sR + Rd;
		if(Rd > kR)
			Rd = kR;

		Gd = Gs*sG + Gd;
		if(Gd > kG)
			Gd = kG;

		Bd = Bs*sB + Bd;
		if(Bd > kB)
			Bd = kB;
	}

	//qDebug() << "cloudColor" << Rd << Gd << Bd;
	m_CloudColor.setRgbF(Rd, Gd, Bd);
}

void auNebula::RenderNebula(double distance, AuGLEffects& re, const AuMatrix4x4& projection, const AuMatrix4x4& model,
		GLuint texture)
{
	Q_UNUSED(distance);
	if(m_vNebula.isEmpty()) return;
	re.GLSL().NebulaShader().Select();
	re.GLSL().NebulaShader().SetProjectionModelView(projection * model);
	re.GLSL().NebulaShader().SetTexture(texture);
	const AuPoint right(model[0], model[4], model[8]), up(model[1], model[5], model[9]);
	const float size = 4.0f/300.0f;
	const int vertices = m_vNebula.size()*6;
	QVector<GLfloat> positions(vertices*3);
	static const int order[6] = {0,3,2,2,1,0};
	for(int i = 0; i < m_vNebula.size(); ++i)
	{
		const AuPoint& center = m_vNebula.at(i).pos;
		// Face-specific expanded positions retain the original CPU double
		// addition and float rounding; the shader remains unchanged.
		const AuPoint points[4] = {center + ((-right-up)*size), center + ((right-up)*size),
			center + ((right+up)*size), center + ((-right+up)*size)};
		for(int corner = 0; corner < 6; ++corner)
		{
			const AuPoint& point = points[order[corner]];
			const int index = (i*6+corner)*3;
			positions[index] = point.x; positions[index+1] = point.y; positions[index+2] = point.z;
		}
	}
	m_NebulaVAO.SelectGL();
	re.glBindBuffer(GL_ARRAY_BUFFER, m_NebulaVAO.Buffer(0));
	const qint64 bytes = qint64(positions.size()) * sizeof(GLfloat);
	re.glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_STREAM_DRAW);
	const qint64 chunk = m_SlicedWork ? 4*1024*1024 : bytes;
	for(qint64 offset = 0; offset < bytes; offset += chunk)
	{
		const char* source = reinterpret_cast<const char*>(positions.constData());
		re.glBufferSubData(GL_ARRAY_BUFFER, offset, qMin(chunk, bytes-offset), source+offset);
		if(m_SlicedWork) { re.glFlush(); QThread::yieldCurrentThread(); }
	}
	re.glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	re.glBindBuffer(GL_ARRAY_BUFFER, 0);
	DrawPreparedGeometry(re, m_NebulaVAO, GL_TRIANGLES, vertices, 3);
	m_NebulaVAO.DeselectGL();
	re.GLSL().NebulaShader().Deselect();
}

void auNebula::RenderStars(float starDotSize, AuGLEffects& re, const AuMatrix4x4& projection, const AuMatrix4x4& model,
		GLuint texture, const AuPoint& pos)
{
	float sds2 = starDotSize * starDotSize;
	re.glEnable(GL_PROGRAM_POINT_SIZE); 

	// Draw Universe Stars
	if(!m_vUniStars.isEmpty())
	{
		re.GLSL().FarStarShader().Select();
		re.GLSL().FarStarShader().SetProjectionModelView(projection * model);
		re.GLSL().FarStarShader().SetSize(sds2 * 1.5);
		re.GLSL().FarStarShader().SetTexture(texture);

		m_UniverseStarsVAO.SelectGL();
		DrawPreparedGeometry(re, m_UniverseStarsVAO, GL_POINTS, m_vUniStars.size(), 1);
		m_UniverseStarsVAO.DeselectGL();

		re.GLSL().FarStarShader().Deselect();
	}

	// Draw Galaxy Stars
	if(!m_vStars.isEmpty())
	{
		re.GLSL().GalaxyStarShader().Select();
		re.GLSL().GalaxyStarShader().SetProjection(projection);
		re.GLSL().GalaxyStarShader().SetModelView(model);
		re.GLSL().GalaxyStarShader().SetSize(starDotSize * 4);
		re.GLSL().GalaxyStarShader().SetTexture(texture);

		// Calculate distance at which intensity reaches the minimum.
		double gd = pos.Length();		// Distance to the galactic core, in sectors.
		double c = cos(pos.PolarIncline());
		double i = AuNoise::Cosine_Interpolate(m_CoreRadPc/10, m_GalaxyRadPc/10, c);
		//qDebug() << "gd dist to core in sectors" << gd << "c" << c << "i" << i;
		re.GLSL().GalaxyStarShader().SetIntensityDistance(gd + i, gd - i);		// Near and far distances for intensity ranges.
		float iMin = .05f;
		float iMax = .4f*sds2;
		float tMin = .25f;
		float tMax = .5f/sds2;
		re.GLSL().GalaxyStarShader().SetIntensityRanges(iMin,iMax, tMin,tMax);	// Ranges of intensity and transparency.

		m_GalaxyStarsVAO.SelectGL();
		DrawPreparedGeometry(re, m_GalaxyStarsVAO, GL_POINTS, m_vStars.size(), 1);
		m_GalaxyStarsVAO.DeselectGL();

		re.GLSL().GalaxyStarShader().Deselect();
	}

	re.glDisable(GL_PROGRAM_POINT_SIZE); 
}

AuPoint auNebula::GetRandomVector()
{
	AuPoint vVector;

	// Pick a random Z between -1.0f and 1.0f.
	vVector.z = m_Dice.RandomMinMax( -1.0f, 1.0f );

	// Get radius of this circle
	float radius = (float)sqrt(1 - vVector.z * vVector.z);

	// Pick a random point on a circle.
	float t = m_Dice.RandomMinMax( -PIf, PIf );

	// Compute matching X and Y for our Z.
	vVector.x = (float)cosf(t) * radius;
	vVector.y = (float)sinf(t) * radius;

	return vVector;
}
