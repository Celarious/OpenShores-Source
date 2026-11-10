#include "dalegacycity.h"
#include "aucommpacket.h"
#include "aueventlog.h"
#include "auglobal.h"
#include "aulegacycitypopulation.h"
#include "auplayerstats.h"
#include "auqueryenvironment.h"
#include "daatomlist.h"
#include "dacreature.h"
#include "dagalaxy.h"
#include "danpc.h"
#include "daperson.h"
#include "dasector.h"
#include "dasolarsystem.h"
#include "davessel.h"
#include "davehicle.h"
#include "daworldll.h"
#include "dgdata.h"
#include "dgempire.h"
#include <QDateTime>
#include <QReadLocker>
#include <QWriteLocker>

namespace
{
int FlagAt(const AuLegacyCity::State& state, const AuGlobeLLF& location)
{
	for(int index = 0; index < state.flags.size(); ++index)
		if(state.flags.at(index).location == location) return index;
	return -1;
}

bool FriendlyFlag(POLITICAL_STANCE stance)
{
	return stance == POL_Friend || stance == POL_Empire || stance == POL_Vassal;
}

POLITICAL_STANCE FlagStance(quint32 empire, quint32 otherEmpire)
{
	if(empire == otherEmpire) return POL_Empire;
	if(!empire || !otherEmpire) return POL_Neutral;
	return PoliticalStance(empire, otherEmpire);
}

bool ChangeFlagInfluence(AuLegacyCity::Flag& flag, POLITICAL_STANCE stance, bool& attack, bool& capture)
{
	attack = capture = false;
	if(stance == POL_Neutral) return false;
	if(FriendlyFlag(stance) || !flag.level)
	{
		if(flag.level >= 30) return false;
		capture = !flag.level && stance == POL_Enemy;
		++flag.level;
	}
	else
	{
		--flag.level;
		attack = true;
	}
	return true;
}

void PoliticalChat(const DaLegacyCity& city, quint32 empire, const QString& text,
	AuCommStdChannel::RANGE range)
{
	const auto* sector = city.FindParentSector();
	if(!sector) return;
	AuCommCityChat message(text, AuCommStdChannel::FriendChannelName(), city.Id(), city.Name(),
		sector->Id(), city.Galaxy(), range, AuCommStdChannel::Friends);
	message.SetScopeData(empire);
	g_Au.SendAtomChat(message);
}
}

bool DaLegacyCity::IsOccupied() const
{
	QReadLocker lock(&m_CityLock);
	return m_City.allegianceEmpire != m_City.controllingEmpire;
}

void DaLegacyCity::InfluenceFlagTakeover(const DaCreature& actor)
{
	// DaPerson/DaCitTroop::Think call this once per second. Town squares
	// require a person; troops may influence military flags only (0x2997c0).
	if(!g_Au.IsLegacyGeneration() || IsDead() || !actor.IsNormal() || actor.CurrentHitPoints() <= 0
		|| actor.IsInsideSpacecraft() || actor.FindParentPlanetInEnv() != Parent()) return;
	const auto* person = dynamic_cast<const DaPerson*>(&actor);
	const auto* troop = dynamic_cast<const DaNpc*>(&actor);
	if((!person && (!troop || !troop->IsNpcTroop())) || (person && person->IsAgentPowerOn())) return;
	const auto* query = actor.LockEnvironmentQuery();
	const bool inCity = query->LegacyCityId() == Id();
	const AuGlobeLLF location = query->m_InIndustryLL;
	actor.UnlockEnvironmentQuery();
	if(!inCity) return;
	const quint32 empire = actor.EmpireId();
	if(!empire) return;
	const auto before = Snapshot();
	const int index = FlagAt(before, location);
	if(index < 0 || (!index && !person)) return;
	const quint32 previousEmpire = before.flags.at(index).empire;
	const POLITICAL_STANCE stance = actor.DaAtom::PoliticalStanceMakeContact(empire, this, previousEmpire);
	if(stance == POL_Neutral) return;
	// Every hostile base must be taken before the town-square flag can move.
	if(!index)
		for(int base = 1; base < before.flags.size(); ++base)
			if(FlagStance(before.flags.at(base).empire, empire) == POL_Enemy) return;
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	bool distress = false, flagLoweredEvent = false;
	{
		QWriteLocker lock(&m_CityLock);
		const int currentIndex = FlagAt(m_City, location);
		if(currentIndex != index || m_City.flags.at(index).empire != previousEmpire) return;
		auto& flag = m_City.flags[index];
		bool attack = false, captured = false;
		if(!ChangeFlagInfluence(flag, stance, attack, captured)) return;
		if(captured)
		{
			PendingFlagCapture capture;
			capture.newEmpire = empire; capture.oldEmpire = previousEmpire; capture.location = location;
			m_PendingFlagCaptures.append(capture);
		}
		if(attack)
		{
			m_City.lastAttack = now;
			flagLoweredEvent = !index || previousEmpire == m_City.controllingEmpire;
			distress = !m_LastFlagDistress || (now - m_LastFlagDistress) / 1000 >= 30;
			if(distress) m_LastFlagDistress = now;
		}
		++m_CityRevision;
	}
	SetModified();
	if(auto* system = FindParentSolarSystem()) system->SetLastFlagInfluence();
	if(distress || flagLoweredEvent)
	{
		QString attacker, defender;
		const DgData& data = g_Au.LockDataForRead();
		data.EmpireName(empire, attacker); data.EmpireName(previousEmpire, defender);
		g_Au.UnlockData();
		// InfluenceFlagTakeover 0x298aa0 logs only the city's own flag
		// being lowered; native insurgent influence never enters this path.
		if(flagLoweredEvent) LogEvent(AuEventLogEntry::FlagLowered, attacker);
		if(distress)
			PoliticalChat(*this, previousEmpire,
				(index ? tr("HELP! %1 is capturing our %2 base in %3.") : tr("HELP! %1 is capturing our %2 city %3."))
					.arg(attacker, defender, Name()), AuCommStdChannel::Galaxy);
	}
}

void DaLegacyCity::TockPolitics(qint64 now)
{
	if(!g_Au.IsLegacyGeneration() || IsDead()) return;
	QVector<PendingFlagCapture> pending;
	{
		QWriteLocker lock(&m_CityLock);
		pending.swap(m_PendingFlagCaptures);
	}
	// Ownership changes, registry notifications and rewards happen after the
	// actor's influence transaction, as in CaptureFlagLater/DaCity::Tick.
	for(const auto& capture : pending) CaptureFlag(capture, now);
	const auto before = Snapshot();
	if(auto* system = FindParentSolarSystem())
		for(const auto& flag : before.flags) system->SetEmpirePresent(flag.empire);
	// DaCity::Tock 0x2d3100 gates native base influence on !IsInAnyScene
	// (the byte at +0x10; IsInAnyScene 0x1aebe0). Visible flags are influenced
	// by the actual actors at their sites, without an automatic defender.
	if(IsInAnyScene()) return;
	{
		QWriteLocker lock(&m_CityLock);
		if(m_LastAutonomousFlagInfluence && now / 1000 <= m_LastAutonomousFlagInfluence / 1000) return;
		m_LastAutonomousFlagInfluence = now;
	}
	// DaCity::Tock 0x2d3100 lets the native allegiance influence base
	// flags, independently of the town-square occupier. Insurgent influence
	// does not refresh attack time or solar-system activity timestamps.
	QVector<POLITICAL_STANCE> stances;
	for(const auto& flag : before.flags) stances.append(FlagStance(before.allegianceEmpire, flag.empire));
	QWriteLocker lock(&m_CityLock);
	if(m_City.allegianceEmpire != before.allegianceEmpire || m_City.flags.size() != before.flags.size()) return;
	bool changed = false;
	for(int index = 1; index < m_City.flags.size(); ++index)
	{
		auto& flag = m_City.flags[index];
		if(flag.location != before.flags.at(index).location || flag.empire != before.flags.at(index).empire) continue;
		const auto stance = stances.at(index);
		if(stance != POL_Enemy && (flag.level >= 30 || stance == POL_Neutral)) continue;
		bool attack = false, captured = false;
		if(!ChangeFlagInfluence(flag, stance, attack, captured)) continue;
		changed = true;
		if(captured)
		{
			PendingFlagCapture capture;
			capture.newEmpire = before.allegianceEmpire; capture.oldEmpire = flag.empire;
			capture.location = flag.location; capture.insurgent = true;
			m_PendingFlagCaptures.append(capture);
		}
	}
	if(changed) ++m_CityRevision;
	lock.unlock();
	if(changed) SetModified();
}

void DaLegacyCity::SurrenderFriendlyFlags(qint64 now)
{
	if(!g_Au.IsLegacyGeneration() || IsDead()) return;
	// Original SurrenderFriendlyFlags transfers friendly/vassal bases to the
	// town-square owner without changing their current pole height.
	const auto before = Snapshot();
	QVector<int> friendly;
	for(int index = 1; index < before.flags.size(); ++index)
	{
		const auto& flag = before.flags.at(index);
		if(flag.empire == before.controllingEmpire) continue;
		const auto stance = FlagStance(flag.empire, before.controllingEmpire);
		if(stance == POL_Friend || stance == POL_Vassal) friendly.append(index);
	}
	if(friendly.isEmpty()) return;
	QWriteLocker lock(&m_CityLock);
	if(m_City.controllingEmpire != before.controllingEmpire || m_City.flags.size() != before.flags.size()) return;
	bool changed = false;
	for(const int index : friendly)
		if(m_City.flags.at(index).location == before.flags.at(index).location
			&& m_City.flags.at(index).empire == before.flags.at(index).empire)
		{
			m_City.flags[index].empire = before.controllingEmpire;
			m_City.flags[index].capturedAt = now;
			changed = true;
		}
	if(changed) ++m_CityRevision;
	lock.unlock();
	if(changed) SetModified();
}

void DaLegacyCity::CaptureFlag(const PendingFlagCapture& capture, qint64 now)
{
	using namespace AuLegacyCity;
	if(!capture.newEmpire || capture.newEmpire == capture.oldEmpire) return;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	if(!world) return;
	const auto before = Snapshot();
	const int index = FlagAt(before, capture.location);
	if(index < 0 || before.flags.at(index).empire != capture.oldEmpire) return;
	if(!index)
		for(int base = 1; base < before.flags.size(); ++base)
			if(FlagStance(before.flags.at(base).empire, capture.newEmpire) == POL_Enemy) return;
	const qint64 lastCapture = before.flags.at(index).capturedAt
		? before.flags.at(index).capturedAt : CreatedTime().MSecsSinceEpoch();
	AuId oldCapital;
	AuIdList places, governors;
	places.append(Id()); ChainOfPlaces(places);
	const DgData& data = g_Au.LockDataForRead();
	data.EmpireCapital(capture.oldEmpire, oldCapital);
	const bool empireCapital = !index && oldCapital == Id();
	const bool sectorCapital = !index && data.IsEmpireSectorCapital(capture.oldEmpire, Id());
	if(!index && data.EmpireRole(capture.oldEmpire) != EMPIRE_Pirate)
		data.EmpireDomainGovernors(capture.oldEmpire, places, governors);
	const quint32 player = data.PlayerId(), confirmation = data.ConfirmationNumber();
	QString attacker, defender;
	data.EmpireName(capture.newEmpire, attacker); data.EmpireName(capture.oldEmpire, defender);
	g_Au.UnlockData();
	QList<DaPerson*> conquerors, flagConquerors, captureWitnesses;
	// The five-minute reward gate is strictly greater than 300 elapsed
	// whole seconds. Pillage includes all hostile on-foot people in the city;
	// capture rewards and mission credit require the captured flag's site.
	const bool rewardEligible = (now - lastCapture) / 1000 > 300;
	if(!capture.insurgent)
	{
		DaAtomList people; world->FindPeopleOnLineRecursive(people);
		for(auto* atom : people)
		{
			auto* person = dynamic_cast<DaPerson*>(atom);
			if(!person || !person->IsNormal() || person->IsAgentPowerOn() || person->IsInsideSpacecraft()
				|| person->FindParentPlanetInEnv() != world) continue;
			const auto* query = person->LockEnvironmentQuery();
			const bool inCity = query->LegacyCityId() == Id();
			const bool atFlag = query->m_InIndustryLL == capture.location;
			person->UnlockEnvironmentQuery();
			if(!inCity || person->DaAtom::PoliticalStanceMakeContact(person->EmpireId(), this, capture.oldEmpire) != POL_Enemy) continue;
			if(rewardEligible) conquerors.append(person);
			if(atFlag)
			{
				captureWitnesses.append(person);
				if(rewardEligible) flagConquerors.append(person);
			}
		}
	}
	double pillage = 0;
	{
		QWriteLocker lock(&m_CityLock);
		if(FlagAt(m_City, capture.location) != index || m_City.flags.at(index).empire != capture.oldEmpire) return;
		// Recheck base ownership when applying a queued town capture.
		if(!index)
		{
			if(m_City.flags.size() != before.flags.size()) return;
			for(int base = 1; base < m_City.flags.size(); ++base)
				if(m_City.flags.at(base).empire != before.flags.at(base).empire) return;
		}
		State candidate = m_City;
		if(!index && !conquerors.isEmpty())
		{
			AccountingState accounting; QString error;
			if(!DecodeAccounting(candidate.accounting, accounting, error)) return;
			const double share = (accounting.accounts.government + accounting.accounts.citizens) / conquerors.size();
			if(share >= 1)
			{
				pillage = share;
				accounting.accounts.government = accounting.accounts.citizens = 0;
				accounting.accounts.ResetPeriod();
				if(!EncodeAccounting(accounting, candidate.accounting, error)) return;
			}
		}
		candidate.flags[index].empire = capture.newEmpire;
		candidate.flags[index].capturedAt = now;
		if(!index)
		{
			candidate.controllingEmpire = capture.newEmpire;
			candidate.garrisonTroops = 0;
			// SetEmpireNoLock adopts the victor only for an empty city.
			if(!candidate.population && candidate.allegianceEmpire != capture.newEmpire)
			{
				candidate.allegianceEmpire = capture.newEmpire;
				candidate.loyalPopulation = 0;
			}
		}
		m_City = candidate; ++m_CityRevision;
	}
	SetModified();
	if(!index) ApplyPoliticalControlChange(capture.oldEmpire, capture.newEmpire, false);
	else world->InvalidateEnvironmentRecursive();
	if(!index && !governors.isEmpty())
	{
		auto* system = FindParentSolarSystem();
		auto* galaxy = FindParentGalaxy();
		if(system && galaxy)
		{
			AuPoint galaxyLocation; system->GetLocationRelativeToParent(galaxyLocation, galaxy);
			const QString lostPlace = empireCapital ? tr("empire capital") : sectorCapital ? tr("sector capital") : tr("city");
			const QString text = capture.insurgent
				? tr("%1 insurgents overran our %2 %3. Governance was turned over to %1.")
					.arg(attacker.toHtmlEscaped(), lostPlace, Name().toHtmlEscaped())
				: tr("%1 has overcome our defenses. We have lost our %2 %3.")
					.arg(attacker.toHtmlEscaped(), lostPlace, Name().toHtmlEscaped());
			g_Au.SendBulkMail(player, confirmation, Id(), Name(), governors, AuMailMsg::MSG_CityDistressReport,
				tr("City Lost"), text, system->Id(), system->Name(), world->Id(), world->Name(), galaxy->Galaxy(), galaxyLocation);
		}
	}
	PoliticalChat(*this, capture.oldEmpire,
		(index ? (capture.insurgent ? tr("ALERT! %1 partisans captured our %2 flag in %3.") : tr("ALERT! %1 captured our %2 flag in %3."))
			: tr("ALERT! %1 captured our %2 city %3."))
			.arg(attacker, defender, Name()), AuCommStdChannel::Sector);
	for(auto* person : flagConquerors)
	{
		DgEmpire empire; empire.m_id = person->EmpireId();
		const DgData& gd = g_Au.LockDataForRead(); gd.Empire(empire); g_Au.UnlockData();
		if(index)
		{
			person->Reward(empire.m_RewCapBase, REW_CaptureBase);
			person->Stats().CapturedBase();
			person->LegacyBaseCaptured(*this);
		}
		else
		{
			if(empireCapital)
			{
				person->Reward(empire.m_RewCapCapitalEmpire, REW_CaptureCapitalEmpire);
				person->Stats().CapturedEmpireCapital();
			}
			else if(sectorCapital)
			{
				person->Reward(empire.m_RewCapCapitalSector, REW_CaptureCapitalSector);
				person->Stats().CapturedSectorCapital();
			}
			else
			{
				person->Reward(empire.m_RewCapCity, REW_CaptureCity);
				person->Stats().CapturedCity();
			}
			person->LegacyCityCaptured(*this);
		}
		person->SetModified();
	}
	// Confirm the completed transfer even when a recent capture prevents a
	// reward. Feedback must not depend on the five-minute anti-farming gate.
	for(auto* person : captureWitnesses)
		person->Thought(index ? tr("%1 defense base captured in city %2.").arg(defender, Name())
			: tr("%1 city %2 captured.").arg(defender, Name()));
	if(pillage >= 1)
		for(auto* person : conquerors)
		{
			person->CreditAccount(pillage);
			person->Thought(tr("%L1%2 was pillaged from city banks. The loot was split between %3 conquerors.")
				.arg(pillage).arg(AuGlobal::CurrencySymbol()).arg(conquerors.size()));
			person->SetModified();
		}
}

void DaLegacyCity::Surrender(quint32 oldEmpire, quint32 newEmpire)
{
	using namespace AuLegacyCity;
	if(!g_Au.IsLegacyGeneration() || oldEmpire == newEmpire || !newEmpire || IsDead()) return;
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	bool controlChanged = false, changed = false;
	{
		QWriteLocker lock(&m_CityLock);
		State candidate = m_City;
		AccountingState accounting; QString error;
		if(!DecodeAccounting(candidate.accounting, accounting, error)) return;
		controlChanged = ApplyCitySurrender(candidate, accounting.accounts, oldEmpire, newEmpire);
		for(int index = 0; index < candidate.flags.size(); ++index)
			if(candidate.flags.at(index).empire != m_City.flags.at(index).empire)
			{ candidate.flags[index].capturedAt = now; changed = true; }
		if(!changed || !EncodeAccounting(accounting, candidate.accounting, error)) return;
		m_City = candidate; ++m_CityRevision;
	}
	SetModified();
	if(controlChanged) ApplyPoliticalControlChange(oldEmpire, newEmpire, true);
	else if(auto* world = dynamic_cast<DaWorldLL*>(Parent()))
	{
		DaAtomList troops; world->FindTroopsRecursive(oldEmpire, troops);
		for(auto* atom : troops)
			if(auto* troop = dynamic_cast<DaNpc*>(atom))
				if(troop->AssignedShip().IsNull() && troop->GarrisonBd() == Id())
				{ troop->SetEmpireId(newEmpire); troop->SetModified(); }
		world->InvalidateEnvironmentRecursive();
	}
}

void DaLegacyCity::ApplyPoliticalControlChange(quint32 oldEmpire, quint32 newEmpire, bool surrenderTroops)
{
	if(!g_Au.IsLegacyGeneration() || oldEmpire == newEmpire) return;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	if(world)
	{
		if(surrenderTroops)
		{
			// CaptureNewShipsAndVehicles is a Surrender consequence in
			// 2016; physical flag capture does not transfer manufactured units.
			DaAtomList units; world->FindVesselsRecursive(units); world->FindVehiclesRecursive(units);
			for(auto* atom : units)
			{
				auto* ship = dynamic_cast<DaVessel*>(atom);
				auto* vehicle = dynamic_cast<DaVehicle*>(atom);
				if((!ship || !ship->IsNew()) && (!vehicle || !vehicle->IsNew())) continue;
				const auto* query = atom->LockEnvironmentQuery();
				const bool inCity = query->LegacyCityId() == Id();
				atom->UnlockEnvironmentQuery();
				if(!inCity || atom->EmpireId() == newEmpire) continue;
				if(ship) ship->SetEmpireId(newEmpire);
				else vehicle->SetEmpireId(newEmpire);
				atom->SetModified();
			}
			DaAtomList troops; world->FindTroopsRecursive(oldEmpire, troops);
			for(auto* atom : troops)
				if(auto* troop = dynamic_cast<DaNpc*>(atom))
					if(troop->AssignedShip().IsNull() && troop->GarrisonBd() == Id())
					{ troop->SetEmpireId(newEmpire); troop->SetModified(); }
		}
		world->InvalidateEnvironmentRecursive();
	}
	if(auto* system = FindParentSolarSystem())
		if(!system->IsExploredBy(newEmpire)) system->ExploredBy(newEmpire);
	emit CityCaptured(oldEmpire, newEmpire, Id());
}

bool DaLegacyCity::GarrisonTroop(DaNpc& troop, bool autoReturn)
{
	if(!g_Au.IsLegacyGeneration() || IsDead() || !troop.IsNpcTroop() || troop.IsBodyDyingOrDead()
		|| troop.IsInsideSpacecraft() || troop.IsLockedPose() || troop.FindParentPlanetInEnv() != Parent()) return false;
	if(autoReturn && (!troop.AssignedShip().IsNull() || troop.GarrisonBd() != Id()
		|| troop.IsInAnyScene() || troop.IsMounted() || troop.HasCompanion())) return false;
	const auto* query = troop.LockEnvironmentQuery();
	const bool inCity = query->LegacyCityId() == Id();
	const AuGlobeLLF location = query->m_InIndustryLL;
	const bool flagSite = query->m_InIndustry == IID_TownSquare || query->m_InIndustry == IID_BaseFlag;
	troop.UnlockEnvironmentQuery();
	if(!inCity || (autoReturn && !flagSite && !troop.IsTroopStationPose())) return false;
	AuLegacyCity::State before; quint64 revision = 0;
	QSet<AuId> returning;
	{ QReadLocker lock(&m_CityLock); before = m_City; revision = m_CityRevision; returning = m_ReturningGarrisonTroops; }
	if(returning.contains(troop.Id())) return false;
	if(FlagAt(before, location) >= 0)
	{
		// OkToReturnToGarrison (0x260080) retains the last physical defender.
		auto* world = dynamic_cast<DaWorldLL*>(Parent());
		if(!world) return false;
		DaAtomList troops; world->FindTroopsRecursive(before.controllingEmpire, troops);
		int defenders = 0;
		for(auto* atom : troops)
		{
			auto* defender = dynamic_cast<DaNpc*>(atom);
			if(!defender || returning.contains(defender->Id()) || !defender->IsNormal() || defender->CurrentHitPoints() <= 0
				|| defender->IsInsideSpacecraft() || defender->GarrisonBd() != Id()) continue;
			const auto* environment = defender->LockEnvironmentQuery();
			const bool sameFlag = environment->LegacyCityId() == Id() && environment->m_InIndustryLL == location;
			defender->UnlockEnvironmentQuery();
			if(sameFlag) ++defenders;
		}
		if(defenders < 2) return false;
	}
	const quint32 empire = troop.EmpireId();
	QWriteLocker lock(&m_CityLock);
	// Competing returns must not both consume the last pair of defenders.
	if(m_CityRevision != revision || m_ReturningGarrisonTroops.contains(troop.Id())
		|| m_City.controllingEmpire != before.controllingEmpire
		|| (!autoReturn && empire != m_City.controllingEmpire)
		|| m_City.garrisonTroops == 0xffff) return false;
	m_ReturningGarrisonTroops.insert(troop.Id());
	++m_City.garrisonTroops; ++m_CityRevision;
	lock.unlock();
	SetModified();
	return true;
}

void DaLegacyCity::FinishGarrisonTroop(const AuId& troop)
{
	// The caller has marked the physical troop dead. Until then, other
	// returns exclude this reserved actor when retaining the last defender.
	QWriteLocker lock(&m_CityLock);
	m_ReturningGarrisonTroops.remove(troop);
}
