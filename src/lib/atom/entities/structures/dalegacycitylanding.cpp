#include "dalegacycity.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityvehicles.h"
#include "aubox.h"
#include "auglobal.h"
#include "davessel.h"
#include "daatomlist.h"
#include "daworldll.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
AuMatrix4x4 SiteTransform(const DaWorldLL& world, const AuLegacyCity::Development& site)
{
	AuMatrix4x4 transform;
	world.BuildingConstructionTransform(world.LocOfLL(site.location, site.altitudeMSL),
		site.facingRadians, nullptr, AuPoint(), transform);
	return transform;
}

bool AtSite(const DaWorldLL& world, const AuLegacyCity::Development& site, const AuPoint& point, double margin = 0)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue || site.demolished || site.IsLandscaper()) return false;
	const auto* recipe = catalogue->ConstructionProcessForIndustry(AuLegacyCity::IndustryId(site.industryId));
	const double radius = site.industryId == 129 ? site.biodomeRadiusMeters * FeetPerMeter()
		: recipe ? recipe->buildRadius * FeetPerMeter() : 0;
	const double width = site.industryId == 2 ? site.roadWidthMeters * FeetPerMeter() / 2 : radius;
	const double length = site.industryId == 2 ? site.roadLengthMeters * FeetPerMeter() / 2 : radius;
	const auto transform = SiteTransform(world, site);
	if(point.Distance2(transform.Translation()) > std::pow(width + length + margin * 2 + 100, 2)) return false;
	AuGlobeLLF ll;
	if(!world.LLOfLoc(ll, point)) return false;
	if(site.industryId == 2 && margin == 0) return AuLegacyCity::ProductionRoadContains(world, site, ll);
	const AuPoint local = transform.UnProject(world.LocOfLL(ll, site.altitudeMSL));
	if(site.industryId == 129) return std::hypot(local.x, local.y) <= radius + margin;
	return std::abs(local.x) <= width + margin && std::abs(local.y) <= length + margin;
}

bool ClearLanding(const DaWorldLL& world, const DaVessel& ship, const AuLegacyCity::Development& slab,
	const QVector<AuLegacyCity::Development>& buildings, AuPoint& point)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	const double clearance = ship.RoughRadius() + 5 * FeetPerMeter();
	if(!catalogue || slab.industryId != 2 || !slab.roadSlab || !slab.completedLevels
		|| !AtSite(world, slab, point, -clearance)) return false;
	AuGlobeLLF ll;
	if(!world.LLOfLoc(ll, point)) return false;
	const double altitude = world.SurfaceAltitudeMSL(ll);
	if(world.Hydrographics() && altitude < 0) return false;
	point = world.LocOfLL(ll, altitude);
	// Reuse manufacturing's footprint checks; legacy buildings are city
	// developments, so the shared unit obstruction query cannot see them.
	AuMatrix4x4 placement;
	world.BuildingConstructionTransform(point, slab.facingRadians, nullptr, AuPoint(), placement);
	AuBox occupied(AuPoint(-clearance, -clearance, 0), AuPoint(clearance, clearance, 0));
	occupied.Transform(placement);
	if(!AuLegacyCity::ProductionLandSpawnFits(world, *catalogue, slab, occupied, buildings)) return false;
	return !world.IsLandingAreaObstructed(point, clearance, &ship);
}
}

void DaLegacyCity::AirportTerminalLandingSpots(QList<AuPoint>& spots) const
{
	// DaCity::AirportTerminalLandingSpots, AuAtom13:18026f640.
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!g_Au.IsLegacyGeneration() || !world || !catalogue || IsDead()) return;
	auto state = Snapshot();
	std::stable_sort(state.developments.begin(), state.developments.end(),
		[](const AuLegacyCity::Development& a, const AuLegacyCity::Development& b) { return a.location < b.location; });
	for(const auto& terminal : state.developments)
	{
		if(terminal.demolished || terminal.industryId != 111) continue;
		const auto* recipe = catalogue->ConstructionProcessForIndustry(AuLegacyCity::IndustryId(111));
		if(!recipe) continue;
		const double radius = recipe->buildRadius * FeetPerMeter() + GroundSpawnSizeLimit() * .5;
		const auto transform = SiteTransform(*world, terminal);
		for(int index = 0; index < 8; ++index)
		{
			AuPoint point = transform.Project(AuLegacyCity::ProductionSpawnOffset(index) * radius);
			for(const auto& road : state.developments)
			{
				if(road.industryId != 2 || !road.roadSlab || !road.completedLevels
					|| !AtSite(*world, road, point)) continue;
				AuGlobeLLF ll;
				if(world->LLOfLoc(ll, point)) spots.append(world->LocOfLL(ll, world->SurfaceAltitudeMSL(ll)));
				break;
			}
		}
	}
}

AuPoint DaLegacyCity::FindSpacecraftLandingSite(const DaVessel& ship) const
{
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!g_Au.IsLegacyGeneration() || !world || IsDead()) return AuPoint();
	auto state = Snapshot();
	QVector<AuLegacyCity::Development> buildings = state.developments;
	DaAtomList cities;
	world->FindLegacyCitiesRecursive(cities);
	for(const auto* atom : cities)
		if(const auto* city = dynamic_cast<const DaLegacyCity*>(atom))
			if(city != this) buildings += city->Snapshot().developments;
	QList<AuPoint> airport;
	AirportTerminalLandingSpots(airport);
	for(AuPoint point : airport)
		for(const auto& slab : state.developments)
			if(ClearLanding(*world, ship, slab, buildings, point)) return point;
	// Keep the historical preference for airport/ordinary/industrial sites,
	// but city landing orders now require a completed slab with full clearance.
	std::stable_sort(state.developments.begin(), state.developments.end(),
		[](const AuLegacyCity::Development& a, const AuLegacyCity::Development& b) { return a.location < b.location; });
	const AuPoint center = Transform().Translation();
	const double clearance = ship.RoughRadius() + 5 * FeetPerMeter();
	int bestRank = std::numeric_limits<int>::max();
	AuPoint best;
	for(const auto& road : state.developments)
	{
		if(road.demolished || road.IsLandscaper() || road.industryId != 2 || !road.roadSlab
			|| !road.completedLevels || road.roadWidthMeters * FeetPerMeter() < clearance * 2) continue;
		AuGlobeLLF start, end;
		if(!AuLegacyCity::ProductionRoadEndpoints(*world, road, start, end)) continue;
		const AuPoint first = world->LocOfLL(start, road.roadStartAltitudeMSL);
		const AuPoint last = world->LocOfLL(end, road.roadEndAltitudeMSL);
		AuPoint direction = last - first;
		const double length = direction.Length();
		if(length < clearance * 2) continue;
		direction /= length;
		for(double distance = clearance; distance <= length - clearance; distance += clearance)
		{
			AuPoint point = first + direction * distance;
			if(center.Distance2(point) > 4900.0 * 4900.0 || !ClearLanding(*world, ship, road, buildings, point)) continue;
			int adjacent = 3; // No neighboring building.
			for(const auto& building : state.developments)
			{
				if(building.industryId == 2 || building.industryId == 129 || !AtSite(*world, building, point, 40 * FeetPerMeter())) continue;
				if(building.industryId == 111) { adjacent = 0; break; }
				const bool industrial = building.industryId == 70 || building.industryId == 126
					|| building.industryId == 87 || building.industryId == 88;
				adjacent = industrial ? 2 : 1;
				break;
			}
			const int rank = adjacent;
			if(!rank) return point;
			if(rank <= bestRank) { bestRank = rank; best = point; }
		}
	}
	return best;
}
