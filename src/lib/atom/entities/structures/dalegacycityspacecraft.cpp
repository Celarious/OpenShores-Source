#include "aulegacycityspacecraft.h"
#include "aulegacycityvehicles.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityinventory.h"
#include "aulegacycitypopulation.h"
#include "aulegacycityproduction.h"
#include "aulegacyscstats.h"
#include "auatompos.h"
#include "aubox.h"
#include "audice.h"
#include "auether.h"
#include "auglobal.h"
#include "augear.h"
#include "auitem.h"
#include "auscdesign.h"
#include "auscparts.h"
#include "daatomlist.h"
#include "dalegacycity.h"
#include "danpc.h"
#include "daperson.h"
#include "dasc.h"
#include "dasolarsystem.h"
#include "davehicle.h"
#include "daworldll.h"
#include "dgdata.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace AuLegacyCity
{
bool CanConfigureSpacecraftFactory(const DaPerson& person, const DaLegacyCity& city)
{
	const quint32 empire = city.EmpireId();
	if(!empire || person.EmpireId() != empire) return false;
	AuIdList places, governors; places.append(city.Id()); city.ChainOfPlaces(places);
	const DgData& data = g_Au.LockDataForRead();
	data.EmpireDomainGovernors(empire, places, governors);
	g_Au.UnlockData();
	return governors.contains(person.Id());
}

bool CarriedSpacecraftBlueprint(const DaPerson& person, quint32 designId)
{
	if(!g_Au.IsLegacyGeneration() || !AuLegacyItemTechnologyActive()
		|| !g_Au.HasLegacyCommodity(CID_ScDesignDisk)) return false;
	bool available = false;
	const AuGear* gear = person.LockGearForRead();
	AuGearPosList positions; gear->ListAll(positions);
	for(const auto& position : positions)
	{
		const AuItem* item = gear->ItemAt(position);
		const auto permitted = [&](const AuScDesignExchangeEntry& entry) {
			return entry.DesignId() == designId && entry.CanBeManufactured() && !entry.UseOnce()
				&& entry.AccessibleTo(person.Id(), person.EmpireId());
		};
		if(const auto* disk = dynamic_cast<const AuItemScDesign*>(item)) available |= permitted(disk->Exchange());
		if(const auto* media = dynamic_cast<const AuItemStorageMedia*>(item))
			for(const auto& entry : media->BlueprintsSc()) available |= permitted(entry);
	}
	person.UnlockGear(); return available;
}

bool NewerSpacecraftDesign(const AuScReport& design, const DaVessel& previous)
{
	AuScReport old;
	const DgData& data = g_Au.LockScDesignsForRead();
	const bool loaded = data.ScDesignReport(previous.DesignId(), old);
	g_Au.UnlockScDesigns();
	if(!loaded || !design.LegacyStats() || !old.LegacyStats()) return false;
	const int technology = design.LegacyStats()->Performance().minimumTechLevel;
	const int oldTechnology = old.LegacyStats()->Performance().minimumTechLevel;
	return technology > oldTechnology || (technology == oldTechnology && design.DesignId() > previous.DesignId());
}

bool AvailableSpacecraftBlueprint(const DaPerson& person, quint32 empire, quint32 designId)
{
	if(CarriedSpacecraftBlueprint(person, designId)) return true;
	QList<quint32> published; QString error;
	return g_Au.ListLegacySpacecraftPublications(empire, published, error) && published.contains(designId);
}

struct SpacecraftProductionBatch::Data
{
	struct Parked
	{
		AuMatrix4x4 floor;
		std::unique_ptr<DaVehicle> vehicle;
	};
	struct Candidate
	{
		AuMatrix4x4 placement;
		DaVessel* replaced = nullptr;
		quint32 slab = 0;
	};
	struct Attempt
	{
		quint32 development = 0, design = 0;
		quint16 slot = 0;
		quint8 quality = 0, technology = 0;
		qint32 failure = -5;
		bool orbital = false, station = false, applied = false;
		std::unique_ptr<DaSc> ship;
		std::vector<Parked> parked;
		QVector<Candidate> candidates;
		Candidate selected;
		AuItemHash fuel;
		AuId architect;
		double commission = 0;
		DaPerson* buyer = nullptr;
		quint32 purchaseSeconds = 0;
	};
	DaLegacyCity* city = nullptr;
	DaWorldLL* world = nullptr;
	quint32 empire = 0;
	std::vector<Attempt> attempts;
	QVector<int> applied;
	bool published = false;
};

SpacecraftProductionBatch::SpacecraftProductionBatch() : m_Data(std::make_shared<Data>()) {}
SpacecraftProductionBatch::~SpacecraftProductionBatch() = default;

qint32 SpacecraftProductionBatch::Apply(ProductionProcess& process, const ProductionOutput& output,
	const State& state, AuItemHash& inventory, AccountingState& accounting)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue || state.controllingEmpire != m_Data->empire) return -5;
	for(int index = 0; index < int(m_Data->attempts.size()); ++index)
	{
		auto& attempt = m_Data->attempts[index];
		if(attempt.development != process.developmentId || attempt.slot != process.slot
			|| attempt.design != output.spacecraftDesign || attempt.quality != output.quality
			|| attempt.technology != output.technology) continue;
		if(!attempt.ship || attempt.applied) return attempt.failure;
		for(const auto& candidate : attempt.candidates)
		{
			if(candidate.slab && std::none_of(state.developments.cbegin(), state.developments.cend(),
				[&candidate](const Development& development) {
					return development.id == candidate.slab && development.industryId == 2 && development.roadSlab
						&& development.completedLevels && !development.demolished;
				})) continue;
			bool blocked = false;
			for(const int previous : m_Data->applied)
			{
				const auto& reserved = m_Data->attempts[previous];
				const double clearance = reserved.ship->RoughRadius() + attempt.ship->RoughRadius();
				if(reserved.selected.placement.Translation().Distance2(candidate.placement.Translation()) < clearance * clearance
					|| (candidate.replaced && candidate.replaced == reserved.selected.replaced)) { blocked = true; break; }
			}
			if(blocked) continue;
			attempt.selected = candidate;
			attempt.applied = true; m_Data->applied.append(index);
			quint32 wanted = attempt.ship->FuelCapacity();
			const auto hydrogen = COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(111)));
			while(wanted)
			{
				auto fuel = inventory.find(hydrogen);
				if(fuel == inventory.end()) break;
				std::unique_ptr<AuItem> taken(fuel->second->Clone());
				if(!taken) break;
				const quint32 count = qMin(wanted, fuel->second->Count());
				taken->SetCount(count); attempt.fuel.insert(std::make_pair(hydrogen, taken.release()));
				fuel->second->Consume(count); wanted -= count;
				if(!fuel->second->Count()) inventory.Erase(fuel);
			}
			const float cost = ActualProductionCost(*catalogue, process);
			if(attempt.buyer)
			{
				attempt.commission = double(cost) * .01;
				accounting.accounts.Credit(CitizenIncome::Sales, double(cost) - attempt.commission, accounting.taxes);
			}
			else
			{
				attempt.commission = qMin(accounting.accounts.citizens, double(cost) * .01);
				accounting.accounts.Debit(CitizenExpense::Commission, attempt.commission);
			}
			return 1;
		}
		return attempt.failure;
	}
	return -5;
}

void SpacecraftProductionBatch::Publish()
{
	if(m_Data->published || !m_Data->city || !m_Data->world || !m_Data->world->Ether()) return;
	m_Data->published = true;
	for(const int index : m_Data->applied)
	{
		auto& attempt = m_Data->attempts[index];
		if(!attempt.applied || !attempt.ship) continue;
		DaSc* ship = attempt.ship.release();
		ship->ConnectSignals();
		ship->SetPosAndOrient(AuAtomPos(m_Data->world->Id(), attempt.selected.placement));
		m_Data->world->Ether()->AddAtom(ship);
		ship->SetWhereMade(QStringLiteral("%1 on %2").arg(m_Data->city->Name(), m_Data->world->Name()));
		for(const auto& fuel : attempt.fuel) ship->AddCargo(*fuel.second);
		if(attempt.buyer)
		{
			ship->SetPrivateOwnership(true); ship->SetPrivateOwner(attempt.buyer->Id(), attempt.buyer->Name());
			ship->SetEmpireId(attempt.buyer->EmpireId()); ship->RechargeCapacitor();
			ship->InteriorLightToggle(INTLIGHT_Bright); ship->Moor(); ship->SetGravityDriveState(DaVessel::GD_Off);
			ship->AddRepairTime(attempt.purchaseSeconds); ship->SetNotNew();
		}
		else if(attempt.station)
		{
			const QString previousName = attempt.selected.replaced ? attempt.selected.replaced->AssignedName() : QString();
			ship->SetName(previousName.isEmpty() ? m_Data->city->Name() : previousName);
			ship->TurnPowerPlantOn(); ship->RechargeCapacitor();
			ship->SetGravityDriveState(DaVessel::GD_On); ship->Unmoor();
		}
		else
		{
			ship->DischargeCapacitor(ship->CapacitorLevel());
			ship->TurnPowerPlantOff(); ship->InteriorLightToggle(INTLIGHT_Dark);
		}
		ship->DaAtom::SetAtRest(); ship->SetModified();
		g_Au.AtomAdded(ship->Id()); ship->SetNormal();
		if(attempt.station && attempt.selected.replaced)
		{
			// NPC boarding uses the same berth placement helper as troop and
			// colonist transfers; offline avatars retain their ordinary relocation.
			DaAtomList occupants; attempt.selected.replaced->ListNormalSubatomsRecursive(occupants);
			for(DaAtom* atom : occupants)
			{
				if(auto* npc = dynamic_cast<DaNpc*>(atom))
				{
					npc->Stand();
					attempt.selected.replaced->RemoveBerthOccupant(npc->Id());
					npc->ClearShipAssignment();
					const bool berth = npc->IsNpcOfficer() ? ship->OfficerBerthsAvailable()
						: npc->IsNpcCrew() ? ship->CrewBerthsAvailable() : npc->IsNpcTroop()
						? ship->TroopBerthsAvailable() : ship->PassengerBerthsAvailable();
					if(berth) { ship->MoveBerthOccupantAboard(*npc); continue; }
				}
				auto* creature = dynamic_cast<DaCreature*>(atom);
				if(!creature || (!dynamic_cast<DaCitizen*>(atom) && !dynamic_cast<DaPerson*>(atom))) continue;
				creature->Stand();
				if(auto* person = dynamic_cast<DaPerson*>(creature))
					if(person->SpawnShip() == attempt.selected.replaced->Id()) person->SetSpawnShip(AuId(), QString(), AuRosterEntry::Invalid, false);
				AuPoint location;
				if(ship->ColonistVoidLocation(location) || ship->TroopOrCrewVoidLocation(location) || ship->TransporterVoidLocation(location))
				{
					AuMatrix4x4 placement; placement.Translate(location);
					creature->ChangePosAndOrient(AuAtomPos(ship->Id(), placement)); creature->InvalidateEnvironment(); creature->SetModified();
				}
				else if(auto* person = dynamic_cast<DaPerson*>(creature)) person->Respawn(false);
			}
		}
		if(attempt.selected.replaced) attempt.selected.replaced->Kill();
		if(attempt.buyer)
		{
			attempt.buyer->Stand();
			AuMatrix4x4 placement; placement.Translate(ship->HelmLocation() + AuPoint(1, -1, attempt.buyer->CGHeight()));
			attempt.buyer->ChangePosAndOrient(AuAtomPos(ship->Id(), placement)); attempt.buyer->InvalidateEnvironment();
			attempt.buyer->SetInMotion(); attempt.buyer->SetModified();
		}
		for(auto& parked : attempt.parked)
		{
			DaVehicle* vehicle = parked.vehicle.release();
			vehicle->ConnectSignals(); vehicle->SetPosAndOrient(AuAtomPos(ship->Id(), parked.floor));
			m_Data->world->Ether()->AddAtom(vehicle);
			vehicle->SetEmpireId(ship->EmpireId()); vehicle->ResetHitPoints(); vehicle->Refuel();
			vehicle->SetMotherShip(ship->Id(), ship->Name());
			vehicle->Park(ship, parked.floor, vehicle->ParkingSpotType()); vehicle->SetParked(true);
			g_Au.AtomAdded(vehicle->Id()); vehicle->SetNormal(); vehicle->TickTock();
		}
		if(auto* system = m_Data->world->FindParentSolarSystem()) system->ExploredBy(ship->EmpireId());
		if(attempt.commission > 0 && !attempt.architect.IsNull())
			emit m_Data->city->SendFunds(m_Data->city->Id(), m_Data->city->Name(), attempt.architect,
				attempt.commission, m_Data->city->Galaxy(), 0, attempt.design);
		ship->TickTock();
	}
}

bool PrepareSpacecraftProduction(DaLegacyCity& city, DaWorldLL& world, const State& state,
	const ProductionState& production, const QHash<quint32, AuZoneResource>& resources,
	qint64 now, SpacecraftProductionBatch& output, QString& error, DaPerson* buyer)
{
	typedef SpacecraftProductionBatch::Data Data;
	error.clear();
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !catalogue
		|| !AuLegacyItemTechnologyActive() || !world.Ether() || city.Parent() != &world)
	{ error = QStringLiteral("Invalid legacy spacecraft production context."); return false; }
	for(const quint16 commodity : catalogue->RuntimeCommodityIds())
		if(!g_Au.HasLegacyCommodity(COMMODITYID(commodity)))
		{ error = QStringLiteral("Legacy commodity metadata is not ready."); return false; }
	SpacecraftProductionBatch batch; batch.m_Data->city = &city; batch.m_Data->world = &world;
	batch.m_Data->empire = state.controllingEmpire;
	for(const auto& process : production.processes)
	{
		if(!process.spacecraftDesign || ((!process.completeAt || now < process.completeAt)
			&& (!process.spacecraftRetryAt || now < process.spacecraftRetryAt))) continue;
		const auto development = std::find_if(state.developments.cbegin(), state.developments.cend(),
			[&process](const Development& value) { return value.id == process.developmentId; });
		if(development == state.developments.cend() || development->demolished || !development->completedLevels
			|| development->industryId != 87 || !resources.contains(development->id)) continue;
		ProductionProcess preview = process; preview.buildingTechnology = development->technology;
		ProductionOutput result;
		if(!FinishProduction(*catalogue, preview, resources.value(development->id), now, result)) continue;
		Data::Attempt attempt; attempt.development = development->id; attempt.slot = process.slot;
		attempt.design = process.spacecraftDesign; attempt.quality = result.quality; attempt.technology = result.technology;
		attempt.buyer = buyer; attempt.purchaseSeconds = quint32((quint64(process.laborNeeded) + 3) / 4);
		AuScDesign design; AuScReport report;
		const DgData& data = g_Au.LockScDesignsForRead();
		const bool loaded = data.ScDesign(process.spacecraftDesign, design);
		if(loaded) design.ReportDesign(report);
		g_Au.UnlockScDesigns();
		if(loaded && design.State() != AuDesign::Final) attempt.failure = -4;
		if(loaded && design.State() == AuDesign::Final && report.LegacyStats() && report.LegacyStats()->IsValid())
		{
			attempt.architect = design.ArchitectId(); attempt.orbital = buyer || report.MustBeBuiltInSpace();
			attempt.station = report.IsSpaceStation();
			attempt.ship.reset(new DaSc); attempt.ship->Create();
			if(attempt.ship->Id().IsNull() || !attempt.ship->SetDesign(report)) attempt.ship.reset();
			if(attempt.ship)
			{
				attempt.ship->InitSystems(result.quality, result.technology);
				attempt.ship->SetEmpireId(state.controllingEmpire);
				attempt.ship->SetName(report.DesignName()); attempt.ship->SetAccessPolicy(ACCESS_Enemy);
				if(!attempt.orbital)
				{
					const auto* construction = catalogue->ConstructionProcessForIndustry(IndustryId(87));
					const double distance = (construction ? construction->buildRadius * FeetPerMeter() : 0)
						+ 59.5 / .7071067690849304;
					AuMatrix4x4 factory;
					world.BuildingConstructionTransform(world.LocOfLL(development->location, development->altitudeMSL),
						development->facingRadians, nullptr, AuPoint(), factory);
					int qualifying = 0; attempt.failure = -11;
					for(int sample = 0; sample < 8; ++sample)
					{
						const AuPoint proposed = factory.Project(ProductionSpawnOffset(sample) * distance);
						AuGlobeLLF ll; if(!world.LLOfLoc(ll, proposed)) continue;
						const auto slab = std::find_if(state.developments.cbegin(), state.developments.cend(),
							[&world, &ll](const Development& value) {
								return value.industryId == 2 && value.roadSlab && value.completedLevels && !value.demolished
									&& ProductionRoadContains(world, value, ll);
							});
						if(slab == state.developments.cend()) continue;
						attempt.failure = -12; if(++qualifying > 4) break;
						const AuPoint ground = world.GetSurfaceLocation(ll);
						Data::Candidate candidate; candidate.slab = slab->id;
						if(world.IsSpawnAreaObstructed(ground, report.DesignId(), report.Quality(), report.RoughHullRadius(),
							&candidate.replaced)) continue;
						const AuPoint up = -world.GravityDir(ground);
						candidate.placement.Translate(ground);
						candidate.placement *= AuPoint(0, 0, 1).GetAlignmentTransform(up);
						candidate.placement.Translate(0, 0, report.LegacyStats()->GroundHeightAboveCG());
						candidate.placement.RotateZ(g_Au.Dice().Roll(1, 360, -1) * DEG2RAD);
						attempt.candidates.append(candidate);
					}
				}
				else
				{
					DaAtomList atoms; world.ListNormalSubatoms(atoms);
					QVector<DaVessel*> stations; DaVessel* replacement = nullptr;
					int newShips = 0, maximumStations = 1;
					bool pendingDesign = false;
					for(DaAtom* atom : atoms)
					{
						auto* vessel = dynamic_cast<DaVessel*>(atom);
						if(!vessel || (vessel->IsPrivatelyOwned() && vessel->HasHumanOfficer())) continue;
						if(!vessel->IsDesignDataCacheValid()) { pendingDesign = true; break; }
						if(vessel->IsInAtmosphere() || vessel->EmpireId() != state.controllingEmpire) continue;
						if(vessel->IsSpaceStation())
						{
							stations.append(vessel); if(vessel->HasFirstOfficer()) ++maximumStations;
							if(attempt.station && !vessel->FindPersonOnLineRecursive() && NewerSpacecraftDesign(report, *vessel)) replacement = vessel;
						}
						else if(vessel->IsNew())
						{
							++newShips;
							if(!attempt.station && !vessel->FindPersonRecursive() && NewerSpacecraftDesign(report, *vessel)) replacement = vessel;
						}
					}
					attempt.failure = pendingDesign ? -21 : attempt.station ? -22 : stations.isEmpty() ? -25 : -23;
					const bool capacity = !pendingDesign && (buyer || (attempt.station
						? stations.size() < maximumStations || (stations.size() == maximumStations && replacement)
						: !stations.isEmpty() && (newShips < 4 || (newShips == 4 && replacement))));
					if(capacity)
					{
						AuPoint origin = buyer || attempt.station ? city.Location() : stations.first()->Location();
						if(!buyer && attempt.station && !stations.isEmpty()) origin = -stations.first()->Location();
						const double altitude = world.AtmosphereRadius() + ParkAltAboveAtm();
						const AuPoint location = world.FindOrbitSpawnLoc(origin, attempt.ship->Id(), report.RoughHullRadius(), altitude, true);
						if(!location.IsZero() && !location.IsNaN())
						{
							Data::Candidate candidate;
							if(!buyer && ((attempt.station && stations.size() == maximumStations) || (!attempt.station && newShips == 4)))
								candidate.replaced = replacement;
							candidate.placement.Translate(location);
							candidate.placement *= AuPoint(0, 0, 1).GetAlignmentTransform(-world.GravityDir(location));
							attempt.candidates.append(candidate);
						}
						else attempt.failure = -24;
					}
				}
				for(const AuScPart* part : design.Parts())
				{
					const QStringList fields = part->GroupName().split('/');
					if(fields.size() != 6 || fields[0] != QStringLiteral("legacy") || fields[1] != QStringLiteral("parking")
						|| fields[4] != QStringLiteral("commodity")) continue;
					Data::Parked parked; parked.floor = part->PositionMatrix();
					parked.vehicle.reset(new DaVehicle(COMMODITYID(fields[5].toUInt()), result.quality)); parked.vehicle->Create();
					if(parked.vehicle->Id().IsNull()) { attempt.ship.reset(); attempt.failure = -5; break; }
					attempt.parked.push_back(std::move(parked));
				}
			}
		}
		batch.m_Data->attempts.push_back(std::move(attempt));
	}
	output = batch; return true;
}
}
