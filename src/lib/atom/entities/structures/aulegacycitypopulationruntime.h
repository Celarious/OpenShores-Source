#ifndef AuLegacyCityPopulationRuntime_H
#define AuLegacyCityPopulationRuntime_H

#include "auatom.h"
#include "aulegacycitypopulation.h"
#include "aulegacycitystate.h"
#include <memory>

class AuDNABase;
class AuItemHash;
class DaLegacyCity;
class DaWorldLL;

namespace AuLegacyCity
{
// Reads either original DNA3 or current DNA4 without converting the species.
// Invalid or truncated keys leave dna empty.
AUATOM_DLL bool ReadSpeciesDNA(const QByteArray& key, std::unique_ptr<AuDNABase>& dna);

struct PopulationRuntimeFacts
{
	PopulationContext context;
	QByteArray nativeSpecies;
	QByteArray citizenSpeciesFallback;
	bool onRingworld = false;
	int daysSinceCityVisit = 0;
	bool pirateEmpire = false;
	quint32 tributeDestination = 0;
	quint8 tributePercent = 0;
	bool tributeUsesPercentage = false; // Capital-to-overlord arithmetic differs from the ordinary half.
	quint32 revoltEmpire = 0;
	bool revoltDestinationKnown = false;
	// Original connections/attack caches are owned by commerce and combat.
	// The caller supplies these explicitly before enabling affected branches.
	bool connectionsKnown = false;
	bool attackKnown = false;
};

struct PopulationRuntimeReport
{
	PopulationCycleResult change;
	PopulationCycleResult reportedChange;
	bool occupied = false;
	quint32 allegianceEmpire = 0, loyalPopulation = 0;
	quint32 airConsumed = 0, foodUnits = 0;
	quint16 foodPerMeal = 0;
	quint8 foodTypes = 0;
	bool ateEnough = true, consumedLiquor = false, citizenSpeciesChanged = false;
	float loyaltyAtStart = 1;
	bool revoltRequested = false;
	bool cycleInterrupted = false, revoltCaptured = false;
	quint32 revoltOldEmpire = 0, revoltNewEmpire = 0;
	double reportedLastTribute = 0;
	quint32 tributeDestination = 0;
	float tributeAmount = 0;
};

// Must run outside the city's write lock. Other legacy cities are read here to
// count the original resource-zone population; apply never reads another atom.
AUATOM_DLL PopulationRuntimeFacts GatherPopulationFacts(DaLegacyCity& city, DaWorldLL& world,
	const State& state);

// Applies one already-due report cycle to caller-owned transaction copies.
// On validation failure it consumes nothing and changes no supplied state.
AUATOM_DLL bool ApplyCityPopulation(State& state, AuItemHash& inventory, AccountingState& accounting,
	const TaxRates& taxes, const PopulationRuntimeFacts& facts, qint64 now, int jobs,
	PopulationRuntimeReport& report, QString& error);
}

#endif
