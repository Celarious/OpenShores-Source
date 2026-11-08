#ifndef AuLegacyCityLifecycle_H
#define AuLegacyCityLifecycle_H

#include "aulegacycitystate.h"

class AuCityReport;
class AuItemHash;
class AuLegacyCityCatalogue;
namespace AuLegacyCity
{
struct PopulationRuntimeFacts;
struct PopulationRuntimeReport;
struct ProductionState;
struct LifecycleResult
{
	bool changed = false, finalDecay = false, citizenSpeciesChanged = false;
	int decayed = 0;
	quint32 deaths = 0;
	QVector<Development> removed;
	QVector<quint32> reduced;
};

void AppendCityEvent(State& state, quint8 eventType, const QString& text, qint64 now);
void CopyCityEvents(const State& state, AuCityReport& report);
void DrainCityEvents(State& state, AuCityReport& report);

// Transaction-only operations. The caller has already activated legacy item
// metadata, decoded the inventory, and gathered world facts without city locks.
void ApplyCityCrises(const AuLegacyCityCatalogue& catalogue, State& state,
	AuItemHash& inventory, const PopulationRuntimeFacts& facts,
	AuCityReport& report, qint64 now, LifecycleResult& result);
void ApplyCityDecay(const AuLegacyCityCatalogue& catalogue, State& state,
	ProductionState& production, AuCityReport& report, qint64 createdAt,
	qint64 now, LifecycleResult& result);
void GenerateCityCrisis(State& state, const PopulationRuntimeReport& population, int decayed, qint64 now);
}
#endif
