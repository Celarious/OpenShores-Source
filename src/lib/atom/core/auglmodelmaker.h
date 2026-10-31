//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: auglmodelmaker.h,v 1.71 2022/02/19 17:31:35 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef AuGLModelMaker_H
#define AuGLModelMaker_H

#include "auatom.h"
#include "auanimalia.h"
#include "auweather.h"
#include "dabd.h"
#include "daworldll.h"
#include <QRunnable>
#include <QElapsedTimer>
#include <QSharedPointer>
#include <QSet>
#include <atomic>

class AuId;
class Au3dsModelInstance;
class DaWorldSphere;
class DaWorldRingSection;
class DePlanetFlora;
class AuLegacyScModel;
class AuScDesign;
class AuGLEffects;

//-+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AuRunnable : public QRunnable
{
protected:
	bool								m_Killed;				// Used in StopRunning logic.

private:
	std::atomic<quint8>					m_RunState;				// Publishes worker results when set to 2.

public:
	inline AuRunnable()
	{
		m_Killed = false;
		m_RunState = 0;
		setAutoDelete(false);
	}

	virtual ~AuRunnable()
	{
	}

	// Returns true if this runnable has been run at all.
	inline bool HasRun() const
	{
		return m_RunState != 0;
	}

	inline bool IsDoneRunning() const
	{
		return m_RunState.load(std::memory_order_acquire) == 2;
	}

	inline void StopRunning()
	{
		m_Killed = true;
	}

protected:
	bool RunStart();

	inline void RunFinish()
	{
		m_RunState.store(2, std::memory_order_release);
	}
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// Model of a spacecraft.

class AuAnimaliaModel : public AuRunnable
{
private:
	AuAnimalia							m_Animalia;
	AuId								m_AnimalId;
	AuClothingMesh						m_ClothingMesh;
	AuTime								m_LastUsed;
	AuMeatMesh							m_SkinMesh;

public:
	AuAnimaliaModel(const AuId& animalId, const AuAnimalia& animalia);
	virtual ~AuAnimaliaModel();

	// Deletes any GL objects that have been created.
	void DeleteGL();

	inline const AuTime& LastUsed() const
	{
		return m_LastUsed;
	}

	// Creates the VAO from the cache data and sets m_Animalia to the result.
	// NOTE: A gl context must be current when this function is called.
	// 'animalia' is an output parameter.
	// Sets LastUsed time.
	void VAO(AuOpenGLFunctions& GLf, AuAnimalia& animalia);

	// Compares 'animalia' to this model. Returns a number from 0 to 100, to indicate how close of a visual match this is.
	// Used when selecting a model to display the person so returns 0 if thread is not done running.
	int VisualCompare(const AuAnimalia& animalia) const;

	// This guy does the grunt work to create the model of the globe.
	virtual void run();
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AuBdModel : public AuRunnable
{
	AuBdRender							m_BdRender;
	quint32								m_DesignId;
	quint32								m_EmpireId;				// Empire for theme colors.

public:
	AuBdModel(quint32 designId, quint32 empireId);

	// NOTE: A gl context must be current when this function is called.
	void DeleteGL();

	inline quint32 EmpireId() const
	{
		return m_EmpireId;
	}

	// Returns the VAOs of this building.
	// Creates the VAOs from the cache data and preserves the result.
	// Returns false if not ready yet.
	// NOTE: A gl context must be current when this function is called.
	bool VAO(AuOpenGLFunctions& GLf, AuBdRender& bdRender);

	// This guy does the grunt work to create the model for as ship design.
	virtual void run();
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AuBdFrame : public AuRunnable
{
	AuBdRender							m_BdRender;
	quint32								m_DesignId;
	COMMODITYID							m_HullMaterial;
	int									m_PercentComplete;

public:
	AuBdFrame(quint32 designId, COMMODITYID hullMaterial, int percentComplete);

	// NOTE: A gl context must be current when this function is called.
	void DeleteGL();

	inline COMMODITYID HullMaterial() const
	{
		return m_HullMaterial;
	}

	inline int PercentComplete() const
	{
		return m_PercentComplete;
	}

	// Returns the VAOs of this building.
	// Creates the VAOs from the cache data and preserves the result.
	// Returns false if not ready yet.
	// NOTE: A gl context must be current when this function is called.
	bool VAO(AuOpenGLFunctions& GLf, AuBdRender& bdRender);

	// This guy does the grunt work to create the model for as ship design.
	virtual void run();
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AuFlatGlobeModel : public AuRunnable
{
private:
	bool								m_CacheEnabled;			// True to load/save files from/to cache dir.
	QList<AuGlobeTriAltVeg>				m_GlobeTris;			// Globe surface triangles.
	float								m_MaxAltMSL;
	float								m_MinAltMSL;
	AuGLVAO								m_VAO;
	bool								m_WorldCanHaveFlora;
	AuId								m_WorldId;
	AuTime								m_WorldModifyTime;

public:
	AuFlatGlobeModel(const AuId& worldId, const AuTime& worldModTime, bool worldCanHaveFlora, bool cacheEnabled);

	// Deletes any GL objects that have been created.
	void DeleteGL();

	// Returns the VAO of this flat world model.
	// Creates the VAO from the cache data and sets m_VAO to the result.
	// NOTE: A gl context must be current when this function is called.
	// 'minAltMSL' and 'maxAltMSL' are output parameters.
	AuGLVAO VAO(AuOpenGLFunctions& GLf, float& minAltMSL, float& maxAltMSL);

	// Returns the current GL VAO id. Does not attempt to create the vertex array object.
	inline GLuint VAOId() const
	{
		return m_VAO.VAO();
	}

	// This guy does the grunt work to create the model of the globe.
	virtual void run();

private:
	inline bool CacheEnabled() const
	{
		return m_CacheEnabled;
	}
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AuGlobeModel : public AuRunnable
{
private:
	bool								m_CacheEnabled;			// True to load/save files from/to cache dir.
	QList<AuGlobeTriAltVegN>			m_GlobeTris;			// Globe surface triangles.
	AuAtGlobeVAO						m_VAO;
	bool								m_WorldCanHaveFlora;
	AuId								m_WorldId;
	AuTime								m_WorldModifyTime;
	float								m_WorldRadius;			// Sea level radius of the world.

public:
	AuGlobeModel(const AuId& worldId, const AuTime& worldModTime, float worldRadius, bool worldCanHaveFlora, bool cacheEnabled);

	// Deletes any GL objects that have been created.
	void DeleteGL();

	// Returns the VAOs of this world.
	// Creates the VAOs from the cache data and sets m_VAO to the result.
	// 'vao' m_TriVAOs will be empty on return if the VAOs are not ready yet.
	// NOTE: A gl context must be current when this function is called.
	void VAOs(AuOpenGLFunctions& GLf, AuAtGlobeVAO& vao);

	// This guy does the grunt work to create the model of the globe.
	virtual void run();

private:
	inline bool CacheEnabled() const
	{
		return m_CacheEnabled;
	}
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AuGlobeModelTri : public AuRunnable
{
private:
	QList<AuGlobeTriAltVegNPave>		m_GlobeTris;			// Globe surface triangles.
	float								m_MaxAltMSL;
	float								m_MinAltMSL;
	AuGlobeLLTri						m_TerrainTri;			// The terrain triangle we are tessellating.
	int									m_TriIndex;
	AuGLTriVAO							m_VAO;
	bool								m_WorldCanHaveFlora;
	AuId								m_WorldId;
	float								m_WorldRadius;			// Sea level radius of the world.

public:
	AuGlobeModelTri(const AuId& worldId, int triIndex, const AuGlobeLLTri& tri, float worldRadius, bool worldCanHaveFlora,
			float maxAltMSL, float minAltMSL);

	// Deletes any GL objects that have been created.
	void DeleteGL();

	// If not valid, the model can still be used until a replacement is ready.
	inline bool IsValid() const
	{
		return m_VAO.IsValid();
	}

	// This guy does the grunt work to create the model of the globe.
	virtual void run();

	inline void SetValid(bool valid)
	{
		m_VAO.SetValid(valid);
	}

	int TriIndex() const
	{
		return m_TriIndex;
	}

	// Returns the VAO of this portion of the world.
	// Creates the VAO from the cache data and sets m_VAO to the result.
	// NOTE: A gl context must be current when this function is called.
	AuGLTriVAO VAO(AuOpenGLFunctions& GLf);
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AuNearGlobeModel : public AuRunnable
{
private:
	bool								m_CacheEnabled;			// True to load/save files from/to cache dir.
	QList<AuGlobeTriTexNShineSnow>		m_GlobeTris;			// Globe surface triangles.
	AuGLVAO								m_VAO;
	AuId								m_WorldId;
	AuTime								m_WorldModifyTime;

public:
	AuNearGlobeModel(const AuId& worldId, const AuTime& worldModTime, bool cacheEnabled);

	// Returns the VAO of this world.
	// Creates the VAO from the cache data and sets m_VAO to the result.
	// Returns m_VAO, which will be 0 if it's not ready yet.
	// NOTE: A gl context must be current when this function is called.
	AuGLVAO VAO(AuOpenGLFunctions& GLf);

	// Returns the VAO when it is known to exist.
	AuGLVAO VAOEx()
	{
		Q_ASSERT(VAOId());
		return m_VAO;
	}

	// Returns the current GL VAO id. Does not attempt to create the vertex array object.
	inline GLuint VAOId() const
	{
		return m_VAO.VAO();
	}

	// This guy does the grunt work to create the model of the globe.
	virtual void run();

private:
	inline bool CacheEnabled() const
	{
		return m_CacheEnabled;
	}
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AuFloraImage : public AuRunnable
{
private:
	DePlanetFlora						m_Flora;				// Flora, updated with size information when finished.
	QImage								m_FloraImage;			// The resulting image.
	quint32								m_GenSeed;				// Planet genseed
	int									m_ResourceZone;
	AuTime m_FloraChange;

public:
	AuFloraImage(const DePlanetFlora& flora, quint32 genSeed, int resourceZone, const AuTime& floraChange);

	// Sets 'imgFlora' to a valid image when ready.
	// Updates 'flora' with my data when done, which includes DeFlora::m_TextureSize gleaned while painting the flora image.
	void FloraImage(DePlanetFlora& flora, QImage& imgFlora);

	int ResourceZone() const
	{
		return m_ResourceZone;
	}

	// This guy does the grunt work to create the grass.
	virtual void run();
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AuGrassImages : public AuRunnable
{
private:
	QVector<DePlanetFlora> m_Flora;
	QImage m_Grass;
	quint32 m_GenSeed;
	AuTime m_FloraChange;

public:
	AuGrassImages(const DePlanetFlora* flora, int resourceZones, quint32 genSeed, const AuTime& floraChange);

	// Returns the complete ground-cover tile, including its density composition.
	bool GrassImage(QImage& grass);

	// This guy does the grunt work to create the grass.
	virtual void run();
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AuFloraModel : public AuRunnable
{
private:
	QMap<AuResKey, AuGlobeFlora>		m_AquaticGrass;			// Aquatic Grass locations.
	QMap<AuResKey, AuGlobeFlora>		m_AquaticShrubs;		// Aquatic Shrub locations.
	QMap<AuResKey, AuGlobeFlora>		m_Grass;				// Grass locations.
	AuPoint								m_GroundLoc;			// Ground location at center of flora distribution.
	QMap<AuResKey, AuGlobeFlora>		m_Shrubs;				// Shrub locations.
	QMap<AuResKey, AuGlobeTree>			m_Trees;				// Tree locations.
	AuId								m_WorldId;

public:
	AuFloraModel(const AuId& worldId, const AuPoint& groundLoc);

	// Returns true when the foliage has been generated in an area.
	// Sets 'grass' to the resulting grass distribution, which may be empty.
	bool Flora(QMap<AuResKey, AuGlobeFlora>& grass, QMap<AuResKey, AuGlobeFlora>& shrubs, QMap<AuResKey, AuGlobeTree>& trees,
			QMap<AuResKey, AuGlobeFlora>& aquaticgrass, QMap<AuResKey, AuGlobeFlora>& aquaticshrubs);

	const AuPoint& GroundLoc() const
	{
		return m_GroundLoc;
	}

	// This guy does the grunt work to create the grass.
	virtual void run();
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AuRockModel : public AuRunnable
{
private:
	AuPoint								m_GroundLoc;			// Ground location at center of flora distribution.
	QMap<AuResKey, AuGlobeRock>			m_Rock;					// Rock locations.
	AuId								m_WorldId;

public:
	AuRockModel(const AuId& worldId, const AuPoint& groundLoc);

	const AuPoint& GroundLoc() const
	{
		return m_GroundLoc;
	}

	// Returns true when the rocks have been generated in an area.
	// Sets 'rocks' to the resulting rock distribution, which may be empty.
	bool Rock(QMap<AuResKey, AuGlobeRock>& rocks);

	// This guy does the grunt work to create the grass.
	virtual void run();
};

//-+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AuWeatherData : public AuRunnable
{
private:
	AuId								m_WorldId;
	AuWeather							m_Wx;

public:
	AuWeatherData(const AuId& worldId, const AuWeather& wx);

	// This guy does the grunt work to create the weather data.
	virtual void run();

	// Sets 'wx' when ready.
	// Returns true if the returned weather is valid.
	bool Weather(AuWeather& wx);
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AUATOM_DLL AuGLModelMaker
{
public:
	AuGLModelMaker(bool cacheEnabled);
	~AuGLModelMaker();

	void Animalia(AuOpenGLFunctions& GLf, const AuId& id, AuAnimalia& animalia); 

	bool Bd(AuOpenGLFunctions& GLf, quint32 designId, quint32 empireId, AuBdRender& bdRender); 

	// Prepares once on a worker, uploads in bounded render-thread steps, then
	// shares the immutable model. Output retains pending/ready model ownership.
	bool LegacySc(AuGLEffects& re, const AuScDesign& design, QSharedPointer<AuLegacyScModel>& model);
	void BeginLegacyScFrame() { m_LegacyScUploadBudget.invalidate(); m_LegacySceneWorkPending = false; }
	// Cities and spacecraft share the same per-frame GL upload allowance.
	QElapsedTimer& LegacyUploadBudget() { return m_LegacyScUploadBudget; }
	// Initial scene loading is staged independently of drawing order. Once
	// ready, keep existing scenery available while new terrain patches load.
	bool LegacyTerrainReady(const AuId& worldId) const;
	void LegacyTerrainPrepared(const AuId& worldId);
	bool LegacyDecorationsReady(const AuId& worldId) const;
	void LegacyBuildingsPrepared(const AuId& worldId);
	// Render-thread decoration work shares the existing city/ship frame budget.
	bool LegacyDecorationStep();
	void DeferLegacySceneWork();
	bool LegacySceneWorkPending() const { return m_LegacySceneWorkPending; }
	void DeleteLegacySc(quint32 designId);
	void ClearLegacySc(); // Call with the owning GL context current before teardown.

	bool BdFrame(AuOpenGLFunctions& GLf, quint32 designId, COMMODITYID hullMaterial, int percentComplete, AuBdRender& bdRender); 

	// Query for 3ds model corresponding to a creature body.
	// Causes the model to be created if needed.
	// Returns a body with an empty models array if the model is not created yet.
	// Eventually returns an array of model parts when the model is created.
	//QVector<Au3dsModelInstance*> BodyModel(const DpBody& body, AuPoint& min, AuPoint& max, AuPoint& eyePos);

	// Returns number of our worker threads still running.
	int BusyThreadCount() const;

	// Returns true when the return result is valid, which may mean no plants at all.
	// Does not modify groundLoc until true is returned.
	bool CreateFlora(const DaWorldLL* world, AuPoint& groundLoc, QMap<AuResKey, AuGlobeFlora>& grass,
			QMap<AuResKey, AuGlobeFlora>& shrubs, QMap<AuResKey, AuGlobeTree>& trees, QMap<AuResKey, AuGlobeFlora>& aquaticgrass,
			QMap<AuResKey, AuGlobeFlora>& aquaticshrubs);

	// Returns true when the return result is valid, which may mean no rocks at all.
	// Does not modify groundLoc until true is returned.
	bool CreateRocks(const DaWorldLL* world, AuPoint& groundLoc, QMap<AuResKey, AuGlobeRock>& rocks);

	void DeleteAnimalia(const AuId& id);

	void DeleteBd(quint32 designId);

	void DeleteBdFrame(quint32 designId);

	void DeleteFlatGlobeModel(const AuId& worldId);

	void DeleteFlatRingworldSectionModel(const AuId& worldId);

	void DeleteFloraImage(const AuId& worldId, int resourceZone);

	// Deletes all flora images related to 'worldId'.
	void DeleteFloraImages(const AuId& worldId);

	void DeleteFloraModel(const AuId& worldId);

	void DeleteGlobeModel(const AuId& worldId);

	void DeleteGlobeModelTri(const AuId& worldId, int triIndex);

	void DeleteGrassImages(const AuId& worldId);

	void DeleteNearGlobeModel(const AuId& worldId);

	void DeleteNearRingworldSectionModel(const AuId& worldId);

	void DeleteRingworldSectionModel(const AuId& worldId);

	void DeleteRingworldSectionModelTri(const AuId& worldId, int triIndex);

	void DeleteRockModel(const AuId& worldId);

	void DeleteWeather(const AuId& worldId);

	AuGLVAO FlatGlobeModel(AuOpenGLFunctions& GLf, const DaWorldSphere* world, float& minAltMSL, float& maxAltMSL);

	AuGLVAO FlatRingworldSectionModel(AuOpenGLFunctions& GLf, const DaWorldRingSection* world, float& minAltMSL,
			float& maxAltMSL);

	// Prepares the plant palette on a worker in Legacy scenes, then uploads it
	// within the frame budget. Modern scenes retain synchronous preparation.
	void FloraTexture(AuGLEffects& re, DePlanetFlora& flora, const AuId& planetId, quint32 genSeed, int resourceZone,
		const AuTime& floraChange);

	// Query for gl model id corresponding to a globe atom id.
	// Causes the model to be created if needed.
	// Eventually 'vao' return with non-empty m_TriVAOs when the model is created.
	void GlobeModel(AuOpenGLFunctions& GLf, AuAtGlobeVAO& vao, const DaWorldSphere* world);

	void GlobeModelTri(AuOpenGLFunctions& GLf, AuGLTriVAO& vao, const DaWorldSphere* world, int triIndex,
			const AuGlobeLLTri& tri, float maxAltMSL, float minAltMSL);

	void InvalidateGlobeModelTri(const AuId& worldId, int triIndex);

	bool GrassTexture(QImage& grass, const DePlanetFlora* flora, int resourceZones, const AuId& planetId,
		quint32 genSeed, const AuTime& floraChange);

	// Returns true if any of our worker threads are still running.
	bool IsBusy() const;

	// Query for gl model id corresponding to a globe atom id.
	// Causes the model to be created if needed.
	// Returns 0 if the model is not created yet.
	// Eventually returns non-zero when the model is created.
	AuGLVAO NearGlobeModel(AuOpenGLFunctions& GLf, const DaWorldSphere* world);

	// Query for gl model id corresponding to a globe atom id.
	// Causes the model to be created if needed.
	// Returns 0 if the model is not created yet.
	// Eventually returns non-zero when the model is created.
	AuGLVAO NearRingworldSectionModel(AuOpenGLFunctions& GLf, const DaWorldRingSection* world);

	// Query for gl model id corresponding to a globe atom id.
	// Causes the model to be created if needed.
	// Eventually 'vao' return with non-empty m_TriVAOs when the model is created.
	void RingworldSectionModel(AuOpenGLFunctions& GLf, AuAtGlobeVAO& vao, const DaWorldRingSection* world);

	void RingworldSectionModelTri(AuOpenGLFunctions& GLf, AuGLTriVAO& vao, const DaWorldRingSection* world, int triIndex,
			const AuGlobeLLTri& tri, float maxAltMSL, float minAltMSL);

	void InvalidateRingworldSectionModelTri(const AuId& worldId, int triIndex);

	// Returns true if the weather is initialized.
	bool Weather(AuWeather& wx, const DaWorldLL* world);

private:
	inline bool CacheEnabled() const
	{
		return m_CacheEnabled;
	}

private:
	bool								m_CacheEnabled;							// True to load/save files from/to cache dir.
	unordered_multimap<AuId, AuAnimaliaModel*, AuIdHash>	m_Animalia;			// Animalia models, key is by animal id.
	QList<AuAnimaliaModel*>				m_AnimaliaToDelete;

	unordered_multimap<quint32, AuBdModel*, AuDesignIdHash>	m_Bd;				// Bd models, key is design id
	QList<AuBdModel*>					m_BdToDelete;

	QHash<quint32, QWeakPointer<AuLegacyScModel>> m_LegacySc;
	QList<QSharedPointer<AuLegacyScModel>> m_LegacyScKeepAlive;
	QList<QWeakPointer<AuLegacyScModel>> m_LegacyScModels;
	QElapsedTimer m_LegacyScUploadBudget;
	QSet<AuId> m_LegacyTerrainReady;
	QSet<AuId> m_LegacyBuildingsReady;
	bool m_LegacySceneWorkPending = false;
	void RetainLegacySc(const QSharedPointer<AuLegacyScModel>& model);

	unordered_multimap<quint32, AuBdFrame*, AuDesignIdHash>	m_BdFrame;			// Bd under construction models, key is design id
	QList<AuBdFrame*>					m_BdFrameToDelete;

	unordered_map<AuId, AuFlatGlobeModel*, AuIdHash>	m_FlatGlobes;			// Globe models, key is world id.
	QList<AuFlatGlobeModel*>			m_FlatGlobesToDelete;

	unordered_map<AuId, AuFlatGlobeModel*, AuIdHash>	m_FlatRingworldSections;	// Ringworld section models, key is world id.
	QList<AuFlatGlobeModel*>			m_FlatRingworldSectionsToDelete;

	unordered_map<AuId, AuFloraModel*, AuIdHash>	m_Flora;					// Flora models, key is world id.
	QList<AuFloraModel*>				m_FloraToDelete;

	unordered_multimap<AuId, AuFloraImage*, AuIdHash>	m_FloraImages;			// Flora images used for texture on flora models.
	QList<AuFloraImage*>				m_FloraImagesToDelete;

	unordered_map<AuId, AuGlobeModel*, AuIdHash>	m_Globes;					// Globe models, key is world id.
	QList<AuGlobeModel*>				m_GlobesToDelete;

	unordered_multimap<AuId, AuGlobeModelTri*, AuIdHash>	m_GlobeTris;		// Globe high-tessellation models, key is world id.
	QList<AuGlobeModelTri*>				m_GlobeTrisToDelete;

	unordered_multimap<AuId, AuGrassImages*, AuIdHash>	m_GrassImages;			// Grass images used for texture on planet models.
	QList<AuGrassImages*>				m_GrassImagesToDelete;

	unordered_map<AuId, AuNearGlobeModel*, AuIdHash>	m_NearGlobes;			// Near globe models, key is world id.
	QList<AuNearGlobeModel*>			m_NearGlobesToDelete;

	unordered_map<AuId, AuNearGlobeModel*, AuIdHash>	m_NearRingworldSections;	// Near globe models, key is world id.
	QList<AuNearGlobeModel*>			m_NearRingworldSectionsToDelete;

	unordered_map<AuId, AuGlobeModel*, AuIdHash>	m_RingworldSections;		// Ringworld section models, key is world id.
	QList<AuGlobeModel*>				m_RingworldSectionsToDelete;

	unordered_multimap<AuId, AuGlobeModelTri*, AuIdHash>	m_RingworldSectionTris;	// Globe high-tess models, key is world id.
	QList<AuGlobeModelTri*>				m_RingworldSectionTrisToDelete;

	unordered_map<AuId, AuRockModel*, AuIdHash>	m_Rock;							// Rock models, key is world id.
	QList<AuRockModel*>					m_RockToDelete;

	unordered_map<AuId, AuWeatherData*, AuIdHash>	m_Wx;						// Weather data models, key is world id.
	QList<AuWeatherData*>				m_WxToDelete;
};

#endif // AuGLModelMaker_H
