#ifndef AuLegacyCityState_H
#define AuLegacyCityState_H

#include "audata.h"
#include "auglobell.h"
#include "aupoint.h"
#include <QByteArray>
#include <QVector>

namespace AuLegacyCity
{
constexpr quint8 StateVersion = 3;
inline bool SupportedStateVersion(quint32 version) { return version >= 1 && version <= StateVersion; }
constexpr int MaximumStateBytes = 8 * 1024 * 1024;
constexpr int MaximumDevelopments = 32768;

// These are owned records, not modern building atoms. IDs are local to a city;
// original blueprint and commodity IDs remain in their historical namespaces.
struct ConstructionComponent
{
	quint16 commodityId = 0; // Historical GD ID.
	quint8 effect = 5, quality = 0;
	quint32 quantity = 0, applied = 0;
};

struct Development
{
	quint32 id = 0;
	quint32 designId = 0;
	quint8 industryId = 0;
	quint16 materialId = 51;
	AuGlobeLLF location;
	double altitudeMSL = 0;
	double facingRadians = 0;
	qint8 minimumDeck = 0;
	qint8 maximumDeck = 0;
	bool constructed = true;
	quint8 completedLevels = 1;
	quint8 technology = 0;
	quint32 employees = 0; // Preserve the original 32-bit counter, including road accumulation.
	// Original DeConstructionProcess MPID/WorkerOk, retained until the new
	// level's production slot is initialized. Zero requests original defaults.
	quint16 selectedManufacturingRecipe = 0;
	bool manufacturingCitizenEnabled = true;
	quint8 constructionProcessId = 0;
	quint8 targetLevels = 1;
	quint32 laborRemaining = 0;
	bool halted = false, citizenWorker = true;
	qint64 lastPlayerLabor = 0;
	QVector<ConstructionComponent> components;
	// Procedural roads use a rectangular strip, centered at location, with its
	// long axis along local +Y. Original process IDs distinguish all road types.
	float roadLengthMeters = 10, roadWidthMeters = 10;
	bool roadSlab = false;
	double roadStartAltitudeMSL = 0, roadEndAltitudeMSL = 0;
	float biodomeRadiusMeters = 100;
	bool demolished = false;
	// Original FIND_SkipLandscapers excludes grades and vegetation work.
	bool IsLandscaper() const
	{
		return industryId == 2 && (constructionProcessId == 5 || constructionProcessId == 74
			|| constructionProcessId == 80 || constructionProcessId == 81);
	}
};

struct Flag
{
	quint32 empire = 0;
	AuGlobeLLF location;
	quint8 level = 30;
	qint64 capturedAt = 0;
};

struct State
{
	QString name;
	quint32 controllingEmpire = 0;
	quint32 allegianceEmpire = 0;
	quint32 loyalPopulation = 0;
	qint32 population = 0;
	qint8 satisfaction = 1;
	float capacitor = 0;
	qint64 lastVisit = 0;
	qint64 lastEmployment = 0, lastPopulationCycle = 0;
	qint64 lastAttack = 0;
	QByteArray citizenSpecies;
	QVector<Development> developments;
	QVector<Flag> flags;
	// Versioned opaque groups preserve ownership of future city-wide systems
	// without substituting modern DaBd economics during infrastructure recovery.
	QByteArray inventory;
	QByteArray manufacturing;
	QByteArray accounting;
	QByteArray reportHistory;
};

// New runtime envelope, independent of the 2016 DB layout. Atomic bounded read;
// malformed/trailing data never publishes a partial city.
AUDATA_DLL bool EncodeState(const State& state, QByteArray& output, QString& error);
AUDATA_DLL bool DecodeState(const QByteArray& bytes, State& output, QString& error);
}
#endif
