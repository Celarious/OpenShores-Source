#include "dalegacycity.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityvehicles.h"
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

bool ClearLanding(const DaWorldLL& world, const DaVessel& ship, AuPoint& point)
{
	AuGlobeLLF ll;
	if(!world.LLOfLoc(ll, point)) return false;
	const double altitude = world.SurfaceAltitudeMSL(ll);
	if(world.Hydrographics() && altitude < 0) return false;
	point = world.LocOfLL(ll, altitude);
	return !world.IsLandingAreaObstructed(point, ship.RoughRadius() + 5 * FeetPerMeter(), &ship);
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
	QList<AuPoint> airport;
	AirportTerminalLandingSpots(airport);
	for(AuPoint point : airport) if(ClearLanding(*world, ship, point)) return point;
	// DaSentient::FindLandingSite, AuAtom13:1803844e0. Scan road
	// centerlines at ship-clearance intervals; prefer completed slabs beside
	// terminals, then ordinary city buildings, then industrial buildings.
	auto state = Snapshot();
	std::stable_sort(state.developments.begin(), state.developments.end(),
		[](const AuLegacyCity::Development& a, const AuLegacyCity::Development& b) { return a.location < b.location; });
	const AuPoint center = Transform().Translation();
	const double clearance = ship.RoughRadius() + 5 * FeetPerMeter();
	int bestRank = std::numeric_limits<int>::max();
	AuPoint best;
	for(const auto& road : state.developments)
	{
		if(road.demolished || road.IsLandscaper() || road.industryId != 2) continue;
		AuGlobeLLF start, end;
		if(!AuLegacyCity::ProductionRoadEndpoints(*world, road, start, end)) continue;
		const AuPoint first = world->LocOfLL(start, road.roadStartAltitudeMSL);
		const AuPoint last = world->LocOfLL(end, road.roadEndAltitudeMSL);
		AuPoint direction = last - first;
		const double length = direction.Length();
		if(length < clearance * 2) continue;
		direction /= length;
		for(double distance = clearance; distance < length; distance += clearance)
		{
			AuPoint point = first + direction * distance;
			if(center.Distance2(point) > 4900.0 * 4900.0 || !ClearLanding(*world, ship, point)) continue;
			int adjacent = 3; // No neighboring building.
			if(road.roadSlab)
				for(const auto& building : state.developments)
				{
					if(building.industryId == 2 || building.industryId == 129 || !AtSite(*world, building, point, 40 * FeetPerMeter())) continue;
					if(building.industryId == 111) { adjacent = 0; break; }
					const bool industrial = building.industryId == 70 || building.industryId == 126
						|| building.industryId == 87 || building.industryId == 88;
					adjacent = industrial ? 2 : 1;
					break;
				}
			if(road.roadSlab && adjacent == 3) continue;
			const int rank = !road.roadSlab ? (road.completedLevels ? 3 : 7)
				: adjacent + (road.completedLevels ? 0 : 4);
			if(!rank) return point;
			if(rank <= bestRank) { bestRank = rank; best = point; }
		}
	}
	if(!best.IsZero()) return best;
	// Sixteen original fallback directions, at 4 * 980 feet from the square.
	AuPoint vertical = -world->GravityDir(center), axis;
	vertical.ArbitraryAxis(axis);
	const AuPoint other = vertical.Cross(axis);
	QVector<AuLegacyCity::Development> developments = state.developments;
	DaAtomList cities;
	world->FindLegacyCitiesRecursive(cities);
	for(const auto* atom : cities)
		if(const auto* city = dynamic_cast<const DaLegacyCity*>(atom))
			if(city != this) developments += city->Snapshot().developments;
	for(int index = 0; index < 16; ++index)
	{
		const float angle = float(index) * PIf * 2.0f * .0625f;
		AuPoint point = center + axis * (std::cos(angle) * 3920) + other * (std::sin(angle) * 3920);
		bool developed = false;
		for(const auto& site : developments)
			if(AtSite(*world, site, point)) { developed = true; break; }
		if(!developed && ClearLanding(*world, ship, point)) return point;
	}
	return AuPoint();
}
