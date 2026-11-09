#include "dalegacycity.h"
#include "aucommpacket.h"
#include "aucondition.h"
#include "augear.h"
#include "auglobal.h"
#include "audice.h"
#include "audnabase.h"
#include "auether.h"
#include "auitem.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityinventory.h"
#include "aulegacycitypopulation.h"
#include "aulegacycitypopulationruntime.h"
#include "auplayerstats.h"
#include "auqueryenvironment.h"
#include "auskill.h"
#include "auspecies.h"
#include "daatomlist.h"
#include "danpc.h"
#include "daperson.h"
#include "dagalaxy.h"
#include "dasector.h"
#include "dasolarsystem.h"
#include "davessel.h"
#include "daworldll.h"
#include "dgdata.h"
#include "dgempire.h"
#include <QDateTime>
#include <QMutexLocker>
#include <QReadLocker>
#include <QWriteLocker>
#include <algorithm>
#include <array>

namespace
{
// Boarding may enter city/world queries while creating actors. Serialize the
// personnel exchange without holding either city's state lock across creation.
QMutex personnelLock(QMutex::Recursive);

bool MetadataReady()
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !AuLegacyItemTechnologyActive() || !catalogue) return false;
	for(const quint16 commodity : catalogue->RuntimeCommodityIds())
		if(!g_Au.HasLegacyCommodity(COMMODITYID(commodity))) return false;
	return true;
}

std::array<quint32, 256> Levels(const AuLegacyCity::State& state)
{
	std::array<quint32, 256> levels = {};
	for(const auto& development : state.developments)
		if(!development.demolished) levels[development.industryId] += development.completedLevels;
	return levels;
}

bool Occupied(const AuLegacyCity::State& state)
{
	return state.population && state.loyalPopulation && state.allegianceEmpire != state.controllingEmpire;
}

bool UnderAttack(const AuLegacyCity::State& state)
{
	return state.lastAttack && double((QDateTime::currentMSecsSinceEpoch() - state.lastAttack) / 1000) <= 771.4285714285714;
}

bool CanDraft(const DaLegacyCity& city, const DaPerson* loader, quint32 empire)
{
	if(!loader || !empire || loader->EmpireId() != empire) return false;
	AuIdList places, governors;
	places.append(city.Id()); city.ChainOfPlaces(places);
	const auto& data = g_Au.LockDataForRead();
	data.EmpireDomainGovernors(empire, places, governors);
	g_Au.UnlockData();
	return governors.contains(loader->Id());
}

bool CanLoadOfficer(const DaLegacyCity& city, const DaPerson* loader, quint32 empire)
{
	if(!loader || !empire || loader->EmpireId() != empire) return false;
	AuIdList places;
	places.append(city.Id()); city.ChainOfPlaces(places);
	const auto& data = g_Au.LockDataForRead();
	const bool allowed = data.EmpireCitizenCanLoadOfficer(empire, loader->Id(), places);
	g_Au.UnlockData();
	return allowed;
}

void DecreasePopulation(AuLegacyCity::State& state, int count)
{
	count = qBound(0, count, state.population);
	state.population -= count;
	state.loyalPopulation -= qMin(state.loyalPopulation, quint32(count));
}

QList<DaNpc*> GroundTroops(const DaLegacyCity& city, quint32 empire, bool includeStations = false)
{
	QList<DaNpc*> troops;
	const auto* world = dynamic_cast<const DaWorldLL*>(city.Parent());
	if(!world) return troops;
	DaAtomList atoms; world->ListNormalSubatoms(atoms);
	for(auto* atom : atoms)
	{
		auto* troop = dynamic_cast<DaNpc*>(atom);
		if(troop && troop->Role() == ROLE_Troop && troop->EmpireId() == empire && troop->CurrentHitPoints() > 0
			&& troop->GarrisonBd() == city.Id() && (includeStations || !troop->IsTroopStationPose())) troops.append(troop);
	}
	return troops;
}
}

bool DaLegacyCity::HasOfficerAcademy() const
{
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!g_Au.IsLegacyGeneration() || !world || !world->IsHabitable(HABITABLE_System)) return false;
	const auto levels = Levels(Snapshot());
	// DaCity::HasStarFleetAcademy 0xf2240. An academy is the combination
	// of university, airport terminal and spacecraft factory in one city.
	return levels[125] && levels[111] && levels[87];
}

void DaLegacyCity::TockGarrison()
{
	if(!MetadataReady() || IsDead()) return;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	AuEther* ether = world ? world->Ether() : nullptr;
	const auto before = Snapshot();
	if(!ether || !before.controllingEmpire || !before.garrisonTroops) return;
	std::unique_ptr<AuDNABase> dna;
	if(!AuLegacyCity::ReadSpeciesDNA(before.citizenSpecies, dna)) return;
	QMutexLocker operation(&personnelLock);
	QHash<quint32, int> defenders;
	QList<DaNpc*> troops = GroundTroops(*this, before.controllingEmpire, true);
	QSet<AuId> assigned;
	for(DaNpc* troop : troops)
	{
		const auto* query = troop->LockEnvironmentQuery();
		if(query->LegacyCityId() == Id()) ++defenders[query->LegacyDevelopmentId()];
		troop->UnlockEnvironmentQuery();
	}
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	QVector<DefensePost> posts = DefensePosts();
	// Flag defenders and authored equipment operators draw from the same pool.
	for(const auto& development : before.developments)
		if((development.industryId == 1 || development.industryId == 10)
			&& !development.demolished && development.completedLevels && !defenders.value(development.id))
		{
			const auto flag = std::find_if(before.flags.cbegin(), before.flags.cend(),
				[&development](const AuLegacyCity::Flag& value) { return value.location == development.location; });
			if(flag != before.flags.cend() && flag->empire == before.controllingEmpire)
			{ DefensePost post; post.development = development; posts.prepend(post); }
		}
	for(const auto& post : posts)
	{
		const auto& development = post.development;
		if(post.stationId)
		{
			bool occupied = false;
			for(DaNpc* troop : troops)
				if(!assigned.contains(troop->Id()) && troop->Location().Distance2(post.station.OpM().Translation()) < 144)
				{ assigned.insert(troop->Id()); occupied = true; break; }
			if(occupied) continue;
			if(DefensePostPlayer(post)) continue;
		}
		const auto* recipe = catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(development.constructionProcessId));
		if(!recipe) recipe = catalogue->ConstructionProcessForIndustry(AuLegacyCity::IndustryId(development.industryId));
		if(!recipe) continue;
		// MaxWorkerSpawnRadius 0xf7f40 and GenRandomLocation 0x519130.
		// Preserve the latter's positive one-metre offset on BOTH axes:
		// Roll[-(max-min), max-min] / 100 + min, in hundredths of a foot.
		const double minimum = FeetPerMeter();
		const double maximum = double(float(recipe->buildRadius * FeetPerMeter())) - FeetPerMeter();
		const int extent = qMax(0, int((maximum - minimum) * 100));
		AuMatrix4x4 site;
		world->BuildingConstructionTransform(world->LocOfLL(development.location, development.altitudeMSL),
			development.facingRadians, nullptr, AuPoint(), site);
		AuGlobeLLF location;
		const double dx = g_Au.Dice().Roll(1, 2 * extent + 1, -extent - 1) / 100.0 + minimum;
		const double dy = g_Au.Dice().Roll(1, 2 * extent + 1, -extent - 1) / 100.0 + minimum;
		if(!world->LLOfLoc(location, site.Project(AuPoint(dx, dy, 0)))) continue;
		{
			QWriteLocker lock(&m_CityLock);
			if(m_City.controllingEmpire != before.controllingEmpire || !m_City.garrisonTroops) break;
			const auto live = std::find_if(m_City.developments.cbegin(), m_City.developments.cend(),
				[&development](const AuLegacyCity::Development& value) { return value.id == development.id
					&& value.location == development.location && value.completedLevels && !value.demolished; });
			const auto liveFlag = std::find_if(m_City.flags.cbegin(), m_City.flags.cend(),
				[&development](const AuLegacyCity::Flag& value) { return value.location == development.location; });
			if(live == m_City.developments.cend() || (!post.stationId && (liveFlag == m_City.flags.cend()
				|| liveFlag->empire != m_City.controllingEmpire)) || m_City.citizenSpecies != before.citizenSpecies) continue;
			--m_City.garrisonTroops; ++m_CityRevision;
		}
		DaNpc* troop = new DaNpc(ROLE_Troop, *dna,
			AuDNABase::CHROMATID(g_Au.Dice().Roll(1, 2, -1)), HANDEDNESS(g_Au.Dice().Roll(1, 2, -1)),
			g_Au.Dice().RandomPersonName());
		troop->ConnectSignals(); troop->Create(); troop->SetHomeWorld(world); troop->Stand();
		const AuPoint spawn = post.stationId
			? post.station.OpM().Translation()
			: world->GetSurfaceLocation(location, troop->GroundHeightAboveCG());
		AuMatrix4x4 placement;
		placement.Translate(spawn);
		placement *= AuPoint(0, 0, 1).GetAlignmentTransform(-world->GravityDir(spawn));
		placement.RotateZ(g_Au.Dice().Roll(1, 360, -1) * DEG2RAD);
		troop->SetPosAndOrient(AuAtomPos(world->Id(), placement));
		ether->AddAtom(troop);
		troop->ResetHitPoints(); troop->ResetHunger(); troop->ResetStamina();
		troop->SetEmpireId(before.controllingEmpire); troop->SetGarrisonBd(Id());
		troop->InitTraining(); troop->SetGarrisonPosture(); troop->UpdateEnvironment();
		EquipSpacecraftPersonnel(*troop);
		InflictCitizenCrises(*troop);
		troop->SetOrder(ORDER_Stay, world->Id(), spawn);
		// SetOrder's fallback Stay always targets the parent. Keep a city-owned
		// station order above it, as hospital operators already do.
		if(post.stationId) troop->AmendOrder(ORDER_Stay, Id(), spawn, post.stationId);
		if(post.stationId) ManBuildingStation(*troop, development, post.stationId);
		troop->SetInScene();
		g_Au.AtomAdded(troop->Id()); troop->SetNormal(); troop->TickTock();
		troops.append(troop); assigned.insert(troop->Id());
		SetModified();
	}
}

int DaLegacyCity::SpacecraftPersonnelAvailable(const DaVessel& ship, NPC_ROLE role, const DaPerson* loader) const
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!MetadataReady() || !catalogue || !world || IsDead() || ship.InDesignEnvironment()
		|| !CanAccessSpacecraft(ship) || (loader && !ship.InChainOfCommand(loader))) return 0;
	const auto state = Snapshot();
	if(!state.controllingEmpire || AuSpecies(state.citizenSpecies).IsNull()) return 0;
	if(role == ROLE_Officer)
	{
		if(ship.EmpireId() != state.controllingEmpire || !world->CaptId().IsNull() || !HasOfficerAcademy()
			|| Occupied(state) || UnderAttack(state) || (loader && !CanLoadOfficer(*this, loader, state.controllingEmpire))) return 0;
		return ship.OfficerBerthsAvailable() ? 1 : 0;
	}
	if(role == ROLE_Troop)
	{
		if(ship.EmpireId() != state.controllingEmpire) return 0;
		return qMin(int(ship.TroopBerthsAvailable()), int(state.garrisonTroops)
			+ (loader ? GroundTroops(*this, state.controllingEmpire).size() : 0));
	}
	if(role != ROLE_Crew && role != ROLE_Passenger) return 0;
	const auto& data = g_Au.LockDataForRead();
	const POLITICAL_STANCE stance = data.EmpirePoliticalStance(state.controllingEmpire, ship.EmpireId());
	const bool restricted = data.EmpireCommerceRestricted(state.controllingEmpire, CID_Invalid, stance);
	g_Au.UnlockData();
	if(restricted || stance == POL_Enemy) return 0;
	const int retained = loader && !Occupied(state) && CanDraft(*this, loader, state.controllingEmpire)
		? 2 : int((qint64(AuLegacyCity::Homes(*catalogue, Levels(state))) * 9) / 10);
	const int berths = role == ROLE_Crew ? int(ship.CrewBerthsAvailable()) : int(ship.PassengerBerthsAvailable());
	return qMax(0, qMin(berths, state.population - retained));
}

void DaLegacyCity::EquipSpacecraftPersonnel(DaNpc& npc)
{
	if(!MetadataReady()) return;
	QMutexLocker operation(&personnelLock);
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	for(;;)
	{
		const auto before = Snapshot();
		if(IsDead()) return;
		const auto* gear = npc.LockGearForRead();
		if(!gear) { npc.UnlockGear(); return; }
		const AuGear previous(*gear);
		npc.UnlockGear();
		AuItemHash inventory, taken; QString error;
		if(!AuLegacyCity::ReadCityInventory(before.inventory, *catalogue, inventory, error)) return;
		// Gear changes refresh weight and can rebuild the NPC environment,
		// including this city's terrain. Never hold the city lock across them.
		npc.InitGear(inventory, taken, true);
		QByteArray bytes;
		if(!AuLegacyCity::WriteCityInventory(inventory, bytes, error))
		{ npc.SetGear(previous); return; }
		QWriteLocker lock(&m_CityLock);
		const bool ownerChanged = m_City.controllingEmpire != before.controllingEmpire;
		if(IsDead() || ownerChanged || m_City.inventory != before.inventory)
		{
			lock.unlock();
			// Rollback can also refresh the environment. Restore outside the
			// city lock, then retry against current stock after a competing sale.
			npc.SetGear(previous);
			if(IsDead() || ownerChanged) return;
			continue;
		}
		m_City.inventory = bytes; ++m_CityRevision;
		lock.unlock(); SetModified();
		return;
	}
}

void DaLegacyCity::InflictCitizenCrises(DaNpc& citizen) const
{
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker()) return;
	// AuSitPoison/Disease::InflictCitizen applies to new citizens emerging
	// from the city. Equipment resupply alone does not create an exposure.
	for(const auto& crisis : Snapshot().crises)
		if(crisis.type == 1)
			citizen.AddCondition(new AuConPoison(crisis.strength, crisis.quality, AuTime::Now()));
		else if(crisis.type == 2)
			citizen.AddCondition(new AuConDisease(crisis.strength, crisis.contagion, crisis.quality, AuTime::Now()));
}

int DaLegacyCity::BoardSpacecraftPersonnel(DaVessel& ship, NPC_ROLE role, int requested,
	const DaPerson* loader, const DaSentient* commander)
{
	if(requested <= 0 || !MetadataReady()) return 0;
	QMutexLocker operation(&personnelLock);
	requested = qMin(requested, SpacecraftPersonnelAvailable(ship, role, loader));
	if(!requested) return 0;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	const auto before = Snapshot();
	QList<AuSpecies> species; species.append(AuSpecies(before.citizenSpecies));
	const quint8 bodyQ = world->NaturalResourceQuality(CID_Animal, world->ResourceZone(Location()));
	const auto groundTroops = role == ROLE_Troop && loader ? GroundTroops(*this, before.controllingEmpire) : QList<DaNpc*>();
	int reservedTroops = 0;
	{
		QWriteLocker lock(&m_CityLock);
		if(m_City.controllingEmpire != before.controllingEmpire || m_City.citizenSpecies != before.citizenSpecies) return 0;
		if(role == ROLE_Troop)
		{
			reservedTroops = qMin(requested, int(m_City.garrisonTroops));
			requested = qMin(requested, reservedTroops + groundTroops.size());
			m_City.garrisonTroops -= quint16(reservedTroops);
		}
		else if(role != ROLE_Officer)
		{ requested = qMin(requested, m_City.population); DecreasePopulation(m_City, requested); }
		++m_CityRevision;
	}
	int boarded = 0;
	AuItemHash emptyInventory;
	for(; boarded < requested; ++boarded)
	{
		DaNpc* npc = nullptr;
		switch(role)
		{
		case ROLE_Officer: npc = ship.BoardOfficer(*world, before.controllingEmpire, species.first(), emptyInventory, bodyQ); break;
		case ROLE_Crew: npc = ship.BoardCrew(before.controllingEmpire, species, emptyInventory, bodyQ); break;
		case ROLE_Passenger: npc = ship.BoardColonist(Id(), world, before.controllingEmpire, species, emptyInventory, bodyQ); break;
		case ROLE_Troop:
			if(boarded < reservedTroops) npc = ship.BoardTroop(before.controllingEmpire, species, emptyInventory, bodyQ);
			else
			{
				npc = groundTroops[boarded - reservedTroops];
				if(!npc->IsNormal() || npc->CurrentHitPoints() <= 0 || npc->IsTroopStationPose()) { npc = nullptr; break; }
				ship.MoveBerthOccupantAboard(*npc);
			}
			break;
		default: break;
		}
		if(!npc) break;
		if(role != ROLE_Troop || boarded < reservedTroops)
		{
			EquipSpacecraftPersonnel(*npc);
			InflictCitizenCrises(*npc);
		}
		if(commander && (role == ROLE_Crew || role == ROLE_Troop)) npc->SetPosture(commander->Posture());
		if(role == ROLE_Officer)
		{ world->SetCapt(npc->Id(), npc->Name()); world->SetModified(NOTIFY_Database); }
	}
	{
		QWriteLocker lock(&m_CityLock);
		if(boarded < requested)
		{
			if(role == ROLE_Troop) m_City.garrisonTroops += quint16(qMax(0, reservedTroops - boarded));
			else if(role != ROLE_Officer)
			{
				m_City.population += requested - boarded;
				m_City.loyalPopulation += qMin(before.loyalPopulation, quint32(requested))
					- qMin(before.loyalPopulation, quint32(boarded));
			}
		}
		if(role == ROLE_Passenger && boarded)
		{
			AuLegacyCity::AccountingState accounting; QString error;
			if(AuLegacyCity::DecodeAccounting(m_City.accounting, accounting, error))
			{
				accounting.accounts.Debit(AuLegacyCity::CitizenExpense::Emigrate, boarded * 1000.0);
				AuLegacyCity::EncodeAccounting(accounting, m_City.accounting, error);
			}
		}
		++m_CityRevision;
	}
	SetModified();
	return boarded;
}

int DaLegacyCity::DebarkSpacecraftPassengers(DaVessel& ship, int requested, quint32* transportFee, bool requireHomes)
{
	if(transportFee) *transportFee = 0;
	if(requested <= 0 || !MetadataReady() || !CanAccessSpacecraft(ship)) return 0;
	QMutexLocker operation(&personnelLock);
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	const auto before = Snapshot();
	if(PoliticalStance(before.controllingEmpire, ship.EmpireId()) == POL_Enemy) return 0;
	if(requireHomes) requested = qMin(requested, qMax(0, AuLegacyCity::Homes(*catalogue, Levels(before)) - before.population));
	if(!requested) return 0;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	auto* system = FindEnvironmentAtom();
	const int fareMultiplier = ship.IsFirstClass() ? 3 : ship.IsSecondClass() ? 2 : 1;
	int debarked = 0; quint32 fare = 0;
	for(; debarked < requested; ++debarked)
	{
		auto* passenger = ship.FindColonist();
		if(!passenger) break;
		const AuId origin = passenger->PortBd();
		const AuSpecies species = passenger->Species();
		const auto* originCity = system ? dynamic_cast<const DaLegacyCity*>(system->FindIdRecursive(origin)) : nullptr;
		const quint32 fee = origin == Id() ? 0 : fareMultiplier * (originCity && originCity->Parent() == world ? 3 : originCity ? 30 : 300);
		{
			QWriteLocker lock(&m_CityLock);
			AuLegacyCity::AccountingState accounting; QString error;
			if(!AuLegacyCity::DecodeAccounting(m_City.accounting, accounting, error)) break;
			accounting.accounts.Credit(AuLegacyCity::CitizenIncome::Immigrate, 1000 - fee, accounting.taxes);
			QByteArray bytes;
			if(!AuLegacyCity::EncodeAccounting(accounting, bytes, error)) break;
			m_City.accounting = bytes;
			if(!m_City.population || m_City.citizenSpecies.isEmpty()) m_City.citizenSpecies = species.Key();
			++m_City.population; ++m_CityRevision;
		}
		SpreadDisease(passenger, ship.Name());
		ship.RemoveBerthOccupant(passenger->Id());
		ship.KillColonist();
		fare += fee;
	}
	if(debarked) { ship.CreditAccountSales(fare); SetModified(); }
	if(transportFee) *transportFee = fare;
	return debarked;
}

bool DaLegacyCity::RxPersonnelChat(const AuCommPacket* message, quint32& countSupplied)
{
	if(!message) return false;
	NPC_ROLE role = ROLE_Count;
	bool debark = false, pending = false;
	switch(message->Type())
	{
	case AuCommPacket::BoardCrew: role = ROLE_Crew; pending = g_Au.HasPendingCrew(message); break;
	case AuCommPacket::BoardOfficers: role = ROLE_Officer; pending = g_Au.HasPendingOfficers(message); break;
	case AuCommPacket::BoardPassengers: role = ROLE_Passenger; pending = g_Au.HasPendingColonists(message); break;
	case AuCommPacket::BoardTroops: role = ROLE_Troop; pending = g_Au.HasPendingTroops(message); break;
	case AuCommPacket::DebarkPassengers: debark = true; pending = g_Au.HasPendingColonists(message); break;
	default: return false;
	}
	if(!MetadataReady() || !pending || countSupplied >= message->Count()) return true;
	auto* system = FindEnvironmentAtom();
	auto* person = system ? dynamic_cast<DaPerson*>(system->FindIdRecursive(message->FromId())) : nullptr;
	auto* ship = person ? person->FindParentVesselInEnv() : nullptr;
	const auto* sector = FindParentSector();
	if(!person || person->IsDead() || !ship || !sector) return true;
	const auto range = AuCommStdChannel::RANGE(qMin(int(message->ReceptionRange()), int(AuCommStdChannel::SolarSystem)));
	const auto say = [&](const QString& text) {
		AuCommCityChat response(text, message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), range, message->Scope());
		g_Au.SendAtomChat(response);
	};
	if(!ship->InChainOfCommand(person)) { say(tr("Only an officer in the ship's chain of command can transfer personnel.")); return true; }
	if(ship->InDesignEnvironment()) { say(tr("Personnel cannot be transferred in the design studio.")); return true; }
	if(role == ROLE_Officer)
	{
		const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
		if(!world) return true;
		if(!world->CaptId().IsNull()) { say(tr("An officer from this world is already deployed.")); return true; }
		if(!world->IsHabitable(HABITABLE_System)) { say(tr("This world cannot produce spacecraft officers.")); return true; }
		if(!HasSpacecraftService(111)) { say(tr("An airport terminal is required to board an officer.")); return true; }
		if(!HasSpacecraftService(125)) { say(tr("A university is required to train an officer.")); return true; }
		if(!HasSpacecraftService(87)) { say(tr("A spacecraft factory is required to train an officer.")); return true; }
		if(!CanLoadOfficer(*this, person, EmpireId())) { say(tr("You do not have authority to load an officer from this world.")); return true; }
	}
	if(!CanAccessSpacecraft(*ship)) { say(tr("Move onto a road slab, into transporter range, or close to a friendly station to transfer personnel.")); return true; }
	quint32 fare = 0;
	const int requested = int(message->Count() - countSupplied);
	const int count = debark ? DebarkSpacecraftPassengers(*ship, requested, &fare, false)
		: BoardSpacecraftPersonnel(*ship, role, requested, person);
	if(!count) { say(tr("No eligible personnel or berths are available for this transfer.")); return true; }
	countSupplied += quint32(count);
	if(debark)
	{
		person->Stats().DebarkedPassengers(count); person->PassengersDelivered(Id(), quint16(count));
		AuCommPassengersDebarked response(person->Id(), quint16(count), fare, false,
			message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), range, message->Scope()); g_Au.SendAtomChat(response);
	}
	else if(role == ROLE_Officer)
	{
		person->Stats().BoardedOfficer();
		const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
		AuCommOfficerBoarded response(person->Id(), world->CaptName(), false,
			message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), range, message->Scope()); g_Au.SendAtomChat(response);
	}
	else if(role == ROLE_Crew)
	{
		person->Stats().BoardedCrew(count);
		AuCommCrewBoarded response(person->Id(), quint8(count), false,
			message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), range, message->Scope()); g_Au.SendAtomChat(response);
	}
	else if(role == ROLE_Troop)
	{
		person->Stats().BoardedTroops(count);
		AuCommTroopsBoarded response(person->Id(), quint16(count), false,
			message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), range, message->Scope()); g_Au.SendAtomChat(response);
	}
	else
	{
		person->Stats().BoardedPassengers(count); person->PassengersPickedUp(Id(), Parent()->Id(), quint16(count));
		AuCommPassengersBoarded response(person->Id(), quint16(count), false,
			message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), range, message->Scope()); g_Au.SendAtomChat(response);
	}
	return true;
}

void DaWorldLL::ReportLegacyOfficerAvailable()
{
	if(!g_Au.IsLegacyGeneration() || !CaptId().IsNull() || !IsHabitable(HABITABLE_System)) return;
	DaAtomList atoms; ListNormalSubatoms(atoms);
	QHash<quint32, const DaLegacyCity*> academies;
	for(auto* atom : atoms)
	{
		const auto* city = dynamic_cast<const DaLegacyCity*>(atom);
		if(!city || !city->HasOfficerAcademy() || !city->EmpireId() || DgEmpire::IsPirate(city->EmpireId())) continue;
		const auto previous = academies.constFind(city->EmpireId());
		if(previous != academies.cend() && (*previous)->Population() > city->Population()) continue;
		academies.insert(city->EmpireId(), city);
	}
	AuIdList places; ChainOfPlaces(places);
	QHash<AuId, const DaLegacyCity*> recipients;
	const auto& data = g_Au.LockDataForRead();
	const quint32 player = data.PlayerId(), confirmation = data.ConfirmationNumber();
	for(auto academy = academies.cbegin(); academy != academies.cend(); ++academy)
	{
		AuIdList loaders;
		data.EmpireDomainOfficerLoaders(academy.key(), places, loaders);
		for(const auto& loader : loaders) recipients.insert(loader, academy.value());
	}
	g_Au.UnlockData();
	if(recipients.isEmpty()) return;
	{
		QWriteLocker lock(&m_LLLock);
		if(!m_CaptId.IsNull()) return;
		if(m_CaptName.isEmpty()) m_CaptName = g_Au.Dice().RandomPersonName();
	}
	SetModified(NOTIFY_Database);
	const QString captain = CaptName();
	const auto* system = FindParentSolarSystem();
	const auto* sector = FindParentSector();
	const auto* galaxy = FindParentGalaxy();
	if(!system || !sector || !galaxy) return;
	AuPoint galaxyLocation; system->GetLocationRelativeToParent(galaxyLocation, galaxy);
	for(auto recipient = recipients.cbegin(); recipient != recipients.cend(); ++recipient)
	{
		const auto* city = recipient.value();
		AuGlobeLL location; LLOfLoc(location, city->Location());
		QString text = g_Au.EncodeUTCString() + QStringLiteral("<br><br>") + HtmlName(tr("Cadet %1").arg(captain));
		text += QStringLiteral("<div style=\"margin-left: 10px;\">") + tr("Star Fleet Academy");
		text += QStringLiteral("<br>") + city->HtmlName() + tr(" at ") + location.ToString();
		text += QStringLiteral("<br>") + HtmlName() + QStringLiteral(", ") + Description();
		text += QStringLiteral("<br>") + system->HtmlNameWithSystem() + ' ' + system->Description();
		text += QStringLiteral("<br>") + sector->HtmlName() + ' ' + sector->Description();
		text += QStringLiteral("<br>") + galaxy->HtmlName() + QStringLiteral("</div>");
		text += tr("<br>Re: Assignment Request on %1<br><br>Commander,<br><br>"
			"I have graduated from spacecraft officer training at the top of my class.<br><br>"
			"Place me in command of a starship. I won't let you down.<br><br>"
			"Cordially,<br><i>%2</i>").arg(HtmlName()).arg(captain.toHtmlEscaped());
		g_Au.SendMail(player, confirmation, Id(), captain, recipient.key(), AuMailMsg::MSG_OfficerReady,
			tr("Assignment Request on %1").arg(Name()), text, system->Id(), system->Name(), Id(), Name(),
			galaxy->Galaxy(), galaxyLocation);
	}
}
