#include "aulegacycityvehicles.h"
#include "auatompos.h"
#include "aubox.h"
#include "aucommpacket.h"
#include "audice.h"
#include "auether.h"
#include "auglobal.h"
#include "auitem.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityinventory.h"
#include "aulegacycityproduction.h"
#include "aulegacycitystate.h"
#include "auqueryenvironment.h"
#include "dalegacycity.h"
#include "daperson.h"
#include "davehicle.h"
#include "daworldll.h"
#include "dgdata.h"
#include <QWriteLocker>
#include <algorithm>
#include <cmath>
#include <vector>

namespace AuLegacyCity
{
namespace
{
const double SpawnRadius = 16.4041994750656; // Original five-metre obstruction test.
const int BoatSearchRings = 4;
const int BoatSearchSamplesPerRing = 16;

AuPoint SearchOffset(int index)
{
	// DpBuilding::SpawnVehicle 0x5f15e0 and SpawnBoat 0x52d6a0 share
	// these eight positions, in this order. Vehicles then try eight more.
	static const int offsets[8][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {-1,1}, {-1,-1}, {1,-1}};
	if(index < 8) return AuPoint(offsets[index][0], offsets[index][1], 0);
	// Assembly 0x1805f19fb..0x1805f1abb: normalize cosf/sinf by the
	// larger absolute component, placing the samples on a square, not a circle.
	const float angle = float(index - 8) * .125f * 6.2831855f + .3926991f;
	float x = std::cos(angle), y = std::sin(angle);
	const float scale = std::max(std::abs(x), std::abs(y));
	x /= scale; y /= scale;
	return AuPoint(x, -y, 0);
}

bool RoadContains(DaWorldLL& world, const Development& road, const AuGlobeLLF& point)
{
	AuMatrix4x4 placement;
	world.BuildingConstructionTransform(world.LocOfLL(road.location, road.altitudeMSL),
		road.facingRadians, nullptr, AuPoint(), placement);
	const double halfLength = road.roadLengthMeters * FeetPerMeter() / 2;
	AuGlobeLLF start, end;
	if(!world.LLOfLoc(start, placement.Project(AuPoint(0, -halfLength, 0)))
		|| !world.LLOfLoc(end, placement.Project(AuPoint(0, halfLength, 0)))) return false;
	float fraction = 0;
	const AuGlobeLLF closest = point.CoordOnSegment(start, end, &fraction, world.LLShape());
	if(fraction < 0 || fraction > 1) return false;
	const float altitude = fraction <= 0 ? float(road.roadStartAltitudeMSL)
		: fraction >= 1 ? float(road.roadEndAltitudeMSL) : float(road.roadStartAltitudeMSL)
			+ (float(road.roadEndAltitudeMSL) - float(road.roadStartAltitudeMSL)) * fraction;
	const float halfWidth = float(road.roadWidthMeters * FeetPerMeter() / 2);
	return float(world.LocOfLL(point, altitude).Distance2(world.LocOfLL(closest, altitude))) <= halfWidth * halfWidth;
}

AuBox SpawnBounds(DaWorldLL& world, const AuPoint& location)
{
	// Match IsSpawnAreaObstructed's box as well as its centre-distance test.
	const AuPoint up = -world.GravityDir(location);
	AuPoint x; up.ArbitraryAxis(x);
	const AuPoint y = up.Cross(x);
	const AuPoint extent = (x + y + up) * SpawnRadius;
	return AuBox(location - extent, location + extent);
}
}

bool IsSpawnedProductionVehicle(quint16 commodity)
{
	switch(commodity)
	{
	case 6: case 7: case 8: case 9: case 28: case 77:
	case 82: case 103: case 104: case 132: case 133: case 231: return true;
	default: return false;
	}
}

struct VehicleProductionBatch::Data
{
	struct Candidate
	{
		AuPoint ground;
		AuMatrix4x4 placement;
		AuBox bounds;
		quint32 slab = 0;
		bool obstructed = false;
	};
	struct Attempt
	{
		quint32 development = 0;
		quint16 slot = 0, commodity = 0;
		std::unique_ptr<DaVehicle> vehicle;
		QVector<Candidate> candidates;
		Candidate selected;
		AuBox occupied;
		bool applied = false, fuelled = false;
	};
	DaWorldLL* world = nullptr;
	std::vector<Attempt> attempts;
	QVector<int> appliedOrder;
	bool published = false;
};

VehicleProductionBatch::VehicleProductionBatch() : m_Data(std::make_shared<Data>()) {}
VehicleProductionBatch::~VehicleProductionBatch() = default;

bool VehicleProductionBatch::Apply(quint32 development, quint16 slot, const ProductionOutput& output,
	const State& state, AuItemHash& inventory)
{
	for(int index = 0; index < int(m_Data->attempts.size()); ++index)
	{
		auto& attempt = m_Data->attempts[index];
		if(attempt.development != development || attempt.slot != slot) continue;
		if(attempt.applied || !attempt.vehicle || attempt.commodity != output.commodityId
			|| attempt.vehicle->Quality() != output.quality) return false;
		for(const auto& candidate : attempt.candidates)
		{
			if(candidate.obstructed) continue;
			if(candidate.slab && std::none_of(state.developments.cbegin(), state.developments.cend(),
				[&candidate](const Development& road) { return road.id == candidate.slab && !road.demolished
					&& road.industryId == 2 && road.roadSlab && road.completedLevels; })) continue;
			bool blocked = false;
			for(const int previous : m_Data->appliedOrder)
			{
				const auto& reserved = m_Data->attempts[previous];
				if(reserved.selected.placement.Translation().Distance2(candidate.ground) < SpawnRadius * SpawnRadius
					|| reserved.occupied.DoesIntersect(candidate.bounds)) { blocked = true; break; }
			}
			if(blocked) continue;
			attempt.selected = candidate;
			attempt.selected.placement.RotateZ(g_Au.Dice().Roll(1, 360, -1) * DEG2RAD);
			attempt.vehicle->RoughExtents(attempt.occupied);
			attempt.occupied.Transform(attempt.selected.placement);
			const COMMODITYID fuel = attempt.vehicle->FuelType();
			attempt.fuelled = fuel != CID_Invalid && inventory.ConsumeCommodity(fuel, 1) == 1;
			attempt.applied = true;
			m_Data->appliedOrder.append(index);
			return true;
		}
		return false;
	}
	return false;
}

void VehicleProductionBatch::Publish(quint32 empire)
{
	if(m_Data->published || !m_Data->world || !m_Data->world->Ether()) return;
	m_Data->published = true;
	for(const int index : m_Data->appliedOrder)
	{
		auto& attempt = m_Data->attempts[index];
		if(!attempt.applied || !attempt.vehicle) continue;
		DaVehicle* vehicle = attempt.vehicle.release();
		vehicle->ConnectSignals();
		vehicle->SetPosAndOrient(AuAtomPos(m_Data->world->Id(), attempt.selected.placement));
		m_Data->world->Ether()->AddAtom(vehicle);
		vehicle->SetEmpireId(empire);
		vehicle->ResetHitPoints();
		if(attempt.fuelled) vehicle->Refuel();
		// Factory outputs stay unused, so the normal claim/vehicle-fetch paths
		// can find them. No building/mothership parking identity is assigned.
		g_Au.AtomAdded(vehicle->Id());
		vehicle->SetNormal();
		vehicle->SetInMotion();
		vehicle->TickTock();
	}
}

bool PrepareVehicleProduction(DaLegacyCity& city, DaWorldLL& world, const State& state,
	const ProductionState& production, const QHash<quint32, AuZoneResource>& resources,
	qint64 now, VehicleProductionBatch& output, QString& error)
{
	error.clear();
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue || !AuLegacyItemTechnologyActive() || !world.Ether() || city.Parent() != &world)
	{ error = QStringLiteral("Invalid legacy vehicle transaction context."); return false; }
	VehicleProductionBatch batch;
	batch.m_Data->world = &world;
	QVector<Development> roads;
	for(const auto& development : state.developments)
		if(development.industryId == 2 && !development.demolished && development.completedLevels) roads.append(development);
	std::stable_sort(roads.begin(), roads.end(), [](const Development& a, const Development& b) { return a.location < b.location; });
	for(const auto& process : production.processes)
	{
		if(!process.completeAt || now < process.completeAt) continue;
		const auto* recipe = catalogue->ManufacturingProcess(ManufacturingProcessId(process.processId));
		if(!recipe || !IsSpawnedProductionVehicle(recipe->commodity.value)) continue;
		const auto found = std::find_if(state.developments.cbegin(), state.developments.cend(),
			[&process](const Development& development) { return development.id == process.developmentId; });
		if(found == state.developments.cend() || found->demolished || !found->completedLevels) continue;
		const auto* construction = found->constructionProcessId
			? catalogue->ConstructionProcess(ConstructionProcessId(found->constructionProcessId))
			: catalogue->ConstructionProcessForIndustry(IndustryId(found->industryId));
		if(!construction || !resources.contains(found->id)) continue;
		const quint16 runtime = catalogue->RuntimeCommodityId(recipe->commodity);
		if(!runtime || !g_Au.HasLegacyCommodity(COMMODITYID(runtime)))
		{ error = QStringLiteral("Legacy vehicle metadata has not been activated."); return false; }
		ProductionProcess preview = process;
		SetProductionTechnology(*catalogue, preview, TechnologyOfProduct(found->industryId, found->technology,
			recipe->commodity.value, state.developments));
		ProductionOutput result;
		if(!FinishProduction(*catalogue, preview, resources.value(found->id), now, result)) continue;
		VehicleProductionBatch::Data::Attempt attempt;
		attempt.development = found->id; attempt.slot = process.slot; attempt.commodity = result.commodityId;
		const bool boat = result.commodityId == 77 || result.commodityId == 132;
		const AuPoint center = world.LocOfLL(found->location, boat ? 0 : found->altitudeMSL);
		AuMatrix4x4 building;
		world.BuildingConstructionTransform(world.LocOfLL(found->location, found->altitudeMSL),
			found->facingRadians, nullptr, AuPoint(), building);
		// Boats search outside the level site, including its 2.828427-foot
		// border. Land vehicles retain their original slab search radius.
		const double radius = construction->buildRadius * FeetPerMeter() + (boat ? 2.828427f : 49.2125984251968);
		const int samples = boat ? BoatSearchRings * BoatSearchSamplesPerRing : 16;
		int tried = 0;
		for(int index = 0; index < samples; ++index)
		{
			const AuPoint offset = SearchOffset(boat ? index % BoatSearchSamplesPerRing : index);
			AuPoint local(offset.x * radius, offset.y * radius, 0);
			if(boat)
			{
				// Sample 20, 30, 40 and 50 metres beyond the square boundary.
				// Move perpendicular to each side; normalize corner diagonals
				// so they also stay within 50 metres. The first ring clears
				// the site's ten-metre terrain blend and spawn obstruction radius.
				AuPoint outward(std::abs(offset.x) == 1 ? offset.x : 0,
					std::abs(offset.y) == 1 ? offset.y : 0, 0);
				outward.Unit();
				local += outward * ((20 + 10 * (index / BoatSearchSamplesPerRing)) * FeetPerMeter());
			}
			const AuPoint proposed = center + building.ProjectVector(local);
			AuGlobeLLF ll;
			if(!world.LLOfLoc(ll, proposed)) continue;
			VehicleProductionBatch::Data::Candidate candidate;
			if(boat)
			{
				if(!world.Hydrographics()) continue;
				// Include developed terrain, so neighboring sites and slabs
				// cannot turn a naturally wet sample into a launch onto land.
				const double bottom = world.TerrainAltitudeMSL(AuGlobeLL(ll.Longitude(), ll.Latitude()));
				const double minimumDepth = FeetPerMeter() * (result.commodityId == 132 ? .5 : 1);
				if(bottom > -minimumDepth) continue;
				// Undersea submarine factories launch above the sea floor;
				// other boats start on the water surface (SpawnBoat 0x52d6a0).
				const bool submarine = result.commodityId == 77 && found->altitudeMSL <= -32;
				candidate.ground = world.LocOfLL(ll, submarine ? bottom + 56 : 0);
			}
			else
			{
				const auto road = std::find_if(roads.cbegin(), roads.cend(),
					[&world, &ll](const Development& value) { return RoadContains(world, value, ll); });
				if(road == roads.cend() || !road->roadSlab) continue;
				candidate.slab = road->id;
				candidate.ground = world.GetSurfaceLocation(ll);
			}
			// Land vehicles retain the original four qualifying slab limit.
			// Keep boat alternatives for simultaneous production; Apply picks
			// the first unobstructed position not reserved by another output.
			if(!boat && ++tried > 4) break;
			candidate.obstructed = world.IsSpawnAreaObstructed(candidate.ground, SpawnRadius);
			if(boat && candidate.obstructed) continue;
			candidate.bounds = SpawnBounds(world, candidate.ground);
			candidate.placement.Translate(candidate.ground);
			candidate.placement *= AuPoint(0, 0, 1).GetAlignmentTransform(-world.GravityDir(candidate.ground));
			attempt.candidates.append(candidate);
		}
		if(std::any_of(attempt.candidates.cbegin(), attempt.candidates.cend(),
			[](const VehicleProductionBatch::Data::Candidate& candidate) { return !candidate.obstructed; }))
		{
			attempt.vehicle.reset(new DaVehicle(COMMODITYID(runtime), result.quality));
			attempt.vehicle->Create(); // Identity/model only; no insertion or callbacks.
			const float height = attempt.vehicle->GroundHeightAboveCG();
			for(auto& candidate : attempt.candidates) candidate.placement.Translate(0, 0, height);
		}
		batch.m_Data->attempts.push_back(std::move(attempt));
	}
	output = batch;
	return true;
}
}

void DaLegacyCity::ServiceVehicle(DaVehicle& vehicle, DaPerson& person)
{
	using namespace AuLegacyCity;
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!g_Au.IsLegacyGeneration() || !AuLegacyItemTechnologyActive() || !catalogue || IsDead()
		|| vehicle.IsDead() || vehicle.IsLaunching() || vehicle.Speed() >= FeetPerMeter()
		|| person.Parent() != &vehicle || vehicle.FindParentPlanetInEnv() != Parent()) return;
	// Inventory streams need the complete activated Legacy descriptor set,
	// including retained tools whose runtime IDs differ from the GD IDs.
	for(const quint16 commodity : catalogue->RuntimeCommodityIds())
		if(!g_Au.HasLegacyCommodity(COMMODITYID(commodity))) return;
	const auto* environment = vehicle.LockEnvironmentQuery();
	const bool inCity = environment->LegacyCityId() == Id();
	const quint32 developmentId = environment->LegacyDevelopmentId();
	vehicle.UnlockEnvironmentQuery();
	if(!inCity || !developmentId) return;

	const quint32 empire = EmpireId();
	const POLITICAL_STANCE stance = PoliticalStanceMakeContact(empire, &vehicle, vehicle.EmpireId());
	const COMMODITYID fuel = vehicle.FuelType();
	const bool needsFuel = fuel != CID_Invalid && vehicle.Fuel() < vehicle.FuelMax();
	const bool needsRepair = vehicle.CurrentHitPoints() < vehicle.MaxHitPoints();
	const DgData& data = g_Au.LockDataForRead();
	const bool fuelRestricted = data.EmpireCommerceRestricted(empire, fuel, stance);
	// DaVehicle::Service 0x4aeb20 uses the repairs commerce policy. Sunlight
	// is its existing placeholder commodity; no sunlight is consumed.
	const bool repairRestricted = data.EmpireCommerceRestricted(empire, CID_Sunlight, stance);
	g_Au.UnlockData();

	bool refuel = false, repair = false;
	const COMMODITYID mechanical = COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(50)));
	const COMMODITYID metal = COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(51)));
	QWriteLocker lock(&m_CityLock);
	if(m_City.controllingEmpire != empire) return;
	const auto slab = std::find_if(m_City.developments.cbegin(), m_City.developments.cend(),
		[developmentId](const Development& development) { return development.id == developmentId
			&& development.industryId == 2 && development.roadSlab && development.completedLevels
			&& !development.demolished; });
	if(slab == m_City.developments.cend()) return;
	AuItemHash inventory;
	QString error;
	if((needsFuel && !fuelRestricted) || (needsRepair && !repairRestricted))
	{
		if(!ReadCityInventory(m_City.inventory, *catalogue, inventory, error)) return;
		if(needsFuel && !fuelRestricted) refuel = inventory.ConsumeCommodity(fuel, 1) == 1;
		if(needsRepair && !repairRestricted)
		{
			ProductionState production;
			if(!DecodeProductionState(*catalogue, m_City.manufacturing, production, error)) return;
			// User-requested departure from 2016's free slab repairs: each
			// increment costs one mechanical part and one metal. City tools
			// may be in stock or retained in the shared production tool pool.
			repair = inventory.RawCount(mechanical) && inventory.RawCount(metal)
				&& HasProductionTool(*catalogue, 136, production, inventory)
				&& HasProductionTool(*catalogue, 4, production, inventory);
			if(repair)
			{
				inventory.ConsumeCommodity(mechanical, 1);
				inventory.ConsumeCommodity(metal, 1);
			}
		}
	}
	if(refuel || repair)
	{
		QByteArray encoded;
		if(!WriteCityInventory(inventory, encoded, error)) return;
		m_City.inventory = encoded;
		++m_CityRevision;
	}
	lock.unlock();
	// Actor repairs, statistics and messages can query environment/city state;
	// commit supplies first and release the city lock before those callbacks.
	if(refuel || repair) SetModified();
	if(refuel)
	{
		vehicle.Refuel();
		AuCommThought message(tr("%1 refueled on road slab.").arg(vehicle.Name()), person.Id(), person.Name());
		g_Au.SendAtomChat(message);
	}
	if(repair)
	{
		vehicle.RepairConditionsTell(&person, 2, QString());
		if(vehicle.RepairHitPoints(10 + vehicle.Quality() / 25))
		{
			AuCommThought message(tr("%1 repaired on road slab.").arg(vehicle.Name()), person.Id(), person.Name());
			g_Au.SendAtomChat(message);
			person.Stats().VehicleRepairsBought(&vehicle);
			person.IncreaseSkill(vehicle.DriverSkill(), 1);
		}
	}
	else if(needsRepair && !repairRestricted)
	{
		AuCommThought message(tr("Slab repairs require one mechanical part and one metal, plus a torque wrench and hammer in the city's supplies."),
			person.Id(), person.Name());
		g_Au.SendAtomChat(message);
	}
}
