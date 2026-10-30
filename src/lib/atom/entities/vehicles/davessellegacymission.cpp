#include "davessel.h"
#include "auglobal.h"
#include "aulegacycitycatalogue.h"
#include "aulegacyscstats.h"
#include "dalegacycity.h"
#include "dasc.h"
#include "dgdata.h"
#include "daatomlist.h"

namespace
{
int StanceRank(const DaVessel& ship, const DaAtom& destination)
{
	switch(PoliticalStance(ship.EmpireId(), destination.EmpireId()))
	{
	case POL_Empire: case POL_Vassal: return 0;
	case POL_Friend: return 1;
	case POL_Neutral: return 2;
	case POL_Enemy: return 3;
	}
	return 3;
}

bool ExportAllowed(const DaLegacyCity& city, const DaVessel& ship, COMMODITYID commodity)
{
	const auto& data = g_Au.LockDataForRead();
	const auto stance = data.EmpirePoliticalStance(city.EmpireId(), ship.EmpireId());
	const bool restricted = data.EmpireCommerceRestricted(city.EmpireId(), commodity, stance);
	g_Au.UnlockData();
	return !restricted;
}

int RepairTechnology(const DaLegacyCity& city)
{
	int technology = -1;
	const auto state = city.Snapshot();
	for(const auto& development : state.developments)
		if(!development.demolished && development.completedLevels && development.industryId == 110)
			technology = qMax(technology, int(development.technology));
	return technology;
}

QPair<int, int> StationTechnology(const DaVessel& vessel)
{
	const auto* ship = dynamic_cast<const DaSc*>(&vessel);
	if(!ship) return qMakePair(-1, -1);
	const auto& design = ship->LockDesignAnalysis();
	const auto stats = design.LegacyStats();
	const int technology = stats ? int(stats->Performance().minimumTechLevel) : -1;
	ship->UnlockDesignAnalysis();
	const auto& systems = ship->LockSystems();
	int lowest = systems.SystemTechLevel(SYS_Hull, 0);
	lowest = qMin(lowest, int(systems.SystemTechLevel(SYS_PowerPlant, 0)));
	lowest = qMin(lowest, int(systems.SystemTechLevel(SYS_Capacitor, 0)));
	for(const auto& system : systems) lowest = qMin(lowest, int(system.TechLevel()));
	ship->UnlockSystems();
	return qMakePair(technology, lowest);
}
}

void DaVessel::FindLegacyServiceCities(DaAtomList& cities, LegacyCityService service, COMMODITYID commodity) const
{
	if(!g_Au.IsLegacyGeneration() || InArena() || InDesignEnvironment()) return;
	const DaAtom* environment = FindEnvironmentAtom();
	if(!environment) return;
	DaAtomList candidates;
	environment->FindLegacyCitiesRecursive(candidates);
	for(DaAtom* atom : candidates)
	{
		auto* city = dynamic_cast<DaLegacyCity*>(atom);
		if(!city || city->IsDead() || !city->CanAccessSpacecraft(*this)) continue;
		bool available = false;
		switch(service)
		{
		case LegacyReachable: available = true; break;
		case LegacyRepair:
			available = city->HasSpacecraftService(110, LegacyMinimumRepairTechnology())
				&& ExportAllowed(*city, *this, CID_Sunlight);
			break;
		case LegacyTrade:
			available = ExportAllowed(*city, *this, commodity);
			break;
		case LegacyCrew: available = city->SpacecraftPersonnelAvailable(*this, ROLE_Crew) > 0; break;
		case LegacyTroops: available = city->SpacecraftPersonnelAvailable(*this, ROLE_Troop) > 0; break;
		case LegacyPassengersLoad: available = city->SpacecraftPersonnelAvailable(*this, ROLE_Passenger) > 0; break;
		case LegacyPassengersUnload:
			available = StanceRank(*this, *city) < 3;
			break;
		}
		if(available && !cities.contains(city)) cities.append(city);
	}
}

const DaAtom* DaVessel::MissionDestLegacyCity(bool closest, bool patrol) const
{
	const DaAtom* environment = FindEnvironmentAtom();
	if(!g_Au.IsLegacyGeneration() || !environment) return nullptr;
	DaAtomList cities;
	environment->FindLegacyCitiesRecursive(cities);
	for(auto city = cities.begin(); city != cities.end();)
	{
		const auto* legacy = dynamic_cast<const DaLegacyCity*>(*city);
		if(!legacy || legacy->IsDead() || !legacy->HasSpacecraftService(111)
			|| (patrol && legacy->EmpireId() != EmpireId())) city = cities.erase(city);
		else ++city;
	}
	if(patrol)
	{
		// DaSpaceShip::MissionDestPatrolCities, AuAtom13:180406160.
		if(HasWaypoint())
		{
			const auto* waypoint = LockWaypoint(WaypointCount() - 1);
			const AuId destination = waypoint->Destination();
			UnlockWaypoint();
			for(const auto* city : cities)
				if(city->Parent() && city->Parent()->Is(destination)) return city;
		}
		m_MissionLock.lockForRead();
		const AuId previous = m_Mission.LastArrivalId();
		m_MissionLock.unlock();
		for(auto city = cities.cbegin(); city != cities.cend(); ++city)
			if((*city)->Is(previous)) { ++city; if(city != cities.cend()) return *city; break; }
		return cities.isEmpty() ? nullptr : cities.first();
	}
	const DaLegacyCity* best = nullptr;
	int bestRank = 3, bestTechnology = -1;
	double bestDistance = 0;
	quint32 bestPopulation = 0;
	for(const auto* atom : cities)
	{
		const auto* city = static_cast<const DaLegacyCity*>(atom);
		const int rank = StanceRank(*this, *city);
		if(rank == 3) continue;
		const int technology = RepairTechnology(*city);
		const double distance = city->EnvLocation().Distance2(EnvLocation());
		const quint32 population = city->Population();
		const bool better = closest ? distance < bestDistance
			: technology > bestTechnology || (technology == bestTechnology && population > bestPopulation);
		// Original best-city order ranks repair availability/TL then population;
		// closest-city ranks distance. Both prefer empire, friends, then neutral.
		if(!best || rank < bestRank || (rank == bestRank && better))
		{
			best = city; bestRank = rank; bestTechnology = technology;
			bestDistance = distance; bestPopulation = population;
		}
	}
	return best;
}

const DaAtom* DaVessel::MissionDestLegacyStationOrCity(bool closest, bool allowCities) const
{
	const DaAtom* environment = FindEnvironmentAtom();
	if(!environment || !g_Au.IsLegacyGeneration()) return nullptr;
	const DaVessel* station = nullptr;
	DaAtomList vessels;
	environment->FindVesselsRecursive(vessels);
	int bestRank = 3;
	double bestDistance = 0;
	QPair<int, int> bestTechnology(-1, -1);
	for(const DaAtom* atom : vessels)
	{
		const auto* candidate = dynamic_cast<const DaVessel*>(atom);
		if(!candidate || candidate == this || candidate->IsDead() || candidate->IsDormant()
			|| !candidate->IsSpaceStation()) continue;
		const int rank = StanceRank(*this, *candidate);
		if(rank == 3) continue;
		const double distance = candidate->EnvLocation().Distance2(EnvLocation());
		const auto technology = StationTechnology(*candidate);
		if(!station || rank < bestRank || (rank == bestRank
			&& (closest ? distance < bestDistance : technology > bestTechnology)))
		{
			station = candidate; bestRank = rank; bestDistance = distance; bestTechnology = technology;
		}
	}
	const DaAtom* city = allowCities ? MissionDestLegacyCity(closest) : nullptr;
	// In the same political category the recovered combined orders prefer a station.
	return station && (!city || bestRank <= StanceRank(*this, *city)) ? station : city;
}
