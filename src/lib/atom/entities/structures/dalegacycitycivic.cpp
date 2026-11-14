#include "dalegacycity.h"
#include "aucityreport.h"
#include "aucommpacket.h"
#include "aueventlog.h"
#include "audice.h"
#include "auglobal.h"
#include "auitem.h"
#include "aujob.h"
#include "aujoboffer.h"
#include "ausensorblip.h"
#include "aulegacycitypopulation.h"
#include "aulegacycityinventory.h"
#include "aulegacycityproduction.h"
#include "auzoneresource.h"
#include "auspecies.h"
#include "daperson.h"
#include "dagalaxy.h"
#include "dasector.h"
#include "dasolarsystem.h"
#include "davessel.h"
#include "daworldll.h"
#include "dgdata.h"
#include <QReadLocker>
#include <QSet>
#include <QWriteLocker>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace AuLegacyCity;

namespace
{
// Re-enable once capture/recapture offers have a normal-player client UI.
constexpr bool LegacyCityCaptureJobsEnabled = false;

quint32 JobPrice(double funds, quint32 quantity, quint32 maximum)
{
	if(!quantity || !std::isfinite(funds) || funds <= 0) return 0;
	return quint32(std::min(double(maximum), std::floor(funds / quantity)));
}
}

void DaLegacyCity::AddBounty(const AuId&, const QString&, const QString& reason, double reward, int)
{
	AddBounty(reason, reward);
}

void DaLegacyCity::AddBounty(const QString& reason, double reward)
{
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || IsDead()
		|| reason.isEmpty() || reason.size() > 4096 || !std::isfinite(reward) || reward <= 0) return;
	QWriteLocker lock(&m_CityLock);
	State next = m_City;
	double total = reward;
	for(const auto& entry : next.bounties) total += entry.reward;
	if(!std::isfinite(total)) return;
	AuLegacyCity::Bounty bounty;
	bounty.reason = reason; bounty.reward = reward; bounty.createdAt = AuTime::CurrentMSecsSinceEpoch();
	next.bounties.append(bounty);
	QByteArray checked; QString error;
	if(!EncodeState(next, checked, error)) return;
	m_City.bounties = next.bounties; ++m_CityRevision;
	lock.unlock(); SetModified();
}

double DaLegacyCity::Bounty() const
{
	if(!g_Au.IsLegacyGeneration()) return 0;
	QReadLocker lock(&m_CityLock);
	double total = 0;
	for(const auto& bounty : m_City.bounties) total += bounty.reward;
	return total;
}

void DaLegacyCity::BountyReasons(QStringList& reasons) const
{
	reasons.clear();
	if(!g_Au.IsLegacyGeneration()) return;
	QReadLocker lock(&m_CityLock);
	for(const auto& bounty : m_City.bounties) reasons.append(bounty.reason);
}

double DaLegacyCity::TakeBounty()
{
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker()) return 0;
	QWriteLocker lock(&m_CityLock);
	double total = 0;
	for(const auto& bounty : m_City.bounties) total += bounty.reward;
	if(m_City.bounties.isEmpty()) return 0;
	m_City.bounties.clear(); ++m_CityRevision;
	lock.unlock(); SetModified();
	return total;
}

double DaLegacyCity::PayCivicReward(double requested, bool government)
{
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !std::isfinite(requested) || requested <= 0) return 0;
	QWriteLocker lock(&m_CityLock);
	AccountingState accounting; QString error;
	if(!DecodeAccounting(m_City.accounting, accounting, error)) return 0;
	const double paid = government ? accounting.accounts.Debit(GovernmentExpense::Rewards, requested)
		: accounting.accounts.Debit(CitizenExpense::Rewards, requested);
	QByteArray encoded;
	if(!paid || !EncodeAccounting(accounting, encoded, error)) return 0;
	m_City.accounting = encoded; ++m_CityRevision;
	lock.unlock(); SetModified();
	return paid;
}

bool DaLegacyCity::RxCivicChat(const AuCommPacket* message)
{
	if(!message || (message->Type() != AuCommPacket::HailRequest
		&& message->Type() != AuCommPacket::DiplomaticMessage)) return false;
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || IsDead()) return true;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	auto* system = world ? dynamic_cast<DaSolarSystem*>(world->FindEnvironmentAtom()) : nullptr;
	const auto* sector = FindParentSector();
	const auto* galaxy = FindParentGalaxy();
	auto* person = system ? dynamic_cast<DaPerson*>(system->FindIdRecursive(message->FromId())) : nullptr;
	if(!world || !system || !sector || !galaxy || !person || person->IsDead()) return true;
	const State state = Snapshot();
	const auto range = AuCommStdChannel::RANGE(std::min(int(message->ReceptionRange()), int(AuCommStdChannel::SolarSystem)));
	const auto response = [&](AuCommCityMessage::MESSAGE kind) {
		AuCommCityMessage reply(kind, message->ChannelName(), Id(), state.name, sector->Id(), Galaxy(), range, message->Scope());
		g_Au.SendAtomChat(reply);
	};
	AuIdList places, governors;
	places.append(Id()); ChainOfPlaces(places);
	QString empireName;
	const auto& data = g_Au.LockDataForRead();
	data.EmpireName(state.controllingEmpire, empireName);
	data.EmpireDomainGovernors(state.controllingEmpire, places, governors);
	const bool commander = state.controllingEmpire == person->EmpireId()
		&& data.IsEmpirePlaceInDomain(state.controllingEmpire, places, person->Id());
	const bool pirate = data.EmpireRole(state.controllingEmpire) == EMPIRE_Pirate;
	const POLITICAL_STANCE stance = data.EmpirePoliticalStance(state.controllingEmpire, person->EmpireId());
	const quint32 player = data.PlayerId(), confirmation = data.ConfirmationNumber();
	g_Au.UnlockData();
	if(message->Type() == AuCommPacket::HailRequest)
	{
		if(stance == POL_Enemy) return true;
		if(state.controllingEmpire && state.controllingEmpire == person->EmpireId())
			person->LegacySystemVisited(system->Id(), *this);
		const QString channel = QStringLiteral("%1-%2").arg(state.name, person->Id().StringId());
		if(channel != message->ChannelName())
		{
			g_Au.OpenChannel(channel, AuCommStdChannel::Everyone, Id(), state.name, sector->Id(), Galaxy(), person->Id(), false);
			response(AuCommCityMessage::ReportOnPrivateChannel);
		}
		QString needs;
		AuCityReport report;
		if(ReportCurrent(report))
		{
			if(report.m_PopOrig && !report.m_HasBreathableAtmo && !report.HasAir()) needs += tr(" We need air.");
			if(report.m_PopOrig && !report.m_FoodUnits) needs += tr(" We need food.");
		}
		for(const Crisis& crisis : state.crises)
			needs += crisis.type == 1 ? tr(" We need Q%1 antidotes; lower quality antidotes can also help.").arg(crisis.quality)
				: crisis.type == 2 ? tr(" We need Q%1 antitoxins; lower quality antitoxins can also help.").arg(crisis.quality)
				: tr(" We need Q%1 pharmaceuticals to exterminate vermin; lower quality pharmaceuticals can also help.").arg(crisis.quality);
		QString service = state.population == 0 ? tr("This is an automated reply.")
			: state.population == 1 ? tr("I am the only one here.") : tr("Can we be of service?");
		bool repairShop = false; quint8 repairTechnology = 0;
		for(const Development& development : state.developments)
			if(!development.demolished && development.completedLevels && development.industryId == 110)
			{
				repairShop = true; repairTechnology = std::max(repairTechnology, development.technology);
			}
		if(repairShop) service += repairTechnology ? tr(" A TL%1 airport repair shop is available here.").arg(repairTechnology + 1)
			: tr(" An airport repair shop is available here.");
		if(!needs.isEmpty()) service += tr(" Help us!%1").arg(needs);
		AuGlobeLLF coordinates; world->LLOfLoc(coordinates, Location());
		const QString text = commander
			? tr("Hail commander %1. %2 city on %3 at %4 here. %5").arg(person->Name(), empireName, world->Name(), coordinates.ToString(), service)
			: tr("Hail %1. %2 city on %3 at %4 here. %5").arg(person->Name(), empireName, world->Name(), coordinates.ToString(), service);
		AuCommCityChat reply(text, channel, Id(), state.name, sector->Id(), Galaxy(), range, AuCommStdChannel::Everyone);
		g_Au.SendAtomChat(reply);
		return true;
	}
	if(InArena() || InDesignPreview()) return true;
	const auto* diplomatic = dynamic_cast<const AuCommDiplomaticMessage*>(message);
	if(!diplomatic) return true;
	const qint64 now = AuTime::CurrentMSecsSinceEpoch();
	{
		QReadLocker lock(&m_CityLock);
		if(m_LastDiplomaticMessage && now - m_LastDiplomaticMessage < 5 * d_MSecPerMin)
		{
			lock.unlock();
			if(stance != POL_Enemy) response(AuCommCityMessage::DiplomaticMessageWait);
			return true;
		}
	}
	if(pirate) { response(AuCommCityMessage::DiplomaticMessageIgnoredByPirate); return true; }
	if(governors.isEmpty()) { response(AuCommCityMessage::DiplomaticMessageIgnored); return true; }
	AuPoint galaxyLocation;
	system->GetLocationRelativeToParent(galaxyLocation, galaxy);
	const auto* ship = person->IsInsideSpacecraft() ? person->FindParentVesselInEnv() : nullptr;
	const QString sender = ship ? tr("%1 (%2) aboard %3").arg(person->Name().toHtmlEscaped(),
		person->Id().StringId(), ship->NameWithEmpire().toHtmlEscaped())
		: tr("%1 (%2)").arg(person->Name().toHtmlEscaped(), person->Id().StringId());
	const QString body = tr("<p>Excellency,</p><p>%1 asked %2 on %3 in %4 to deliver this diplomatic message to its leaders.</p>"
		"<blockquote>%5</blockquote><p>You received this because you are in the city's chain of command. "
		"Reply to continue the conversation with the sender. The delivery receipt keeps your identity anonymous.</p>")
		.arg(sender, state.name.toHtmlEscaped(), world->Name().toHtmlEscaped(), system->Name().toHtmlEscaped(),
			diplomatic->DiplomaticMessage().toHtmlEscaped());
	// The 2016 courier used mail, whose delivery receipt preserves governor anonymity.
	g_Au.SendBulkMail(player, confirmation, person->Id(), person->Name(), governors,
		AuMailMsg::MSG_CityDiplomaticMessage, tr("Diplomatic Message from %1").arg(person->Name()), body,
		system->Id(), system->Name(), world->Id(), world->Name(), Galaxy(), galaxyLocation);
	{
		QWriteLocker lock(&m_CityLock);
		m_LastDiplomaticMessage = now;
	}
	if(stance != POL_Enemy) response(AuCommCityMessage::DiplomaticMessageSent);
	return true;
}

void DaLegacyCity::ReportEnemySensorContact(const DaSolarSystem* system, const AuSensorBlipList& blips, float sensorRange)
{
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !system || IsDead()) return;
	const auto* sector = system->FindParentSector();
	const auto* galaxy = system->FindParentGalaxy();
	if(!sector || !galaxy) return;
	const AuPoint cityLocation = EnvLocation();
	const double rangeSquared = double(sensorRange) * sensorRange;
	const QString address = sector->Name() + QStringLiteral(" ") + sector->Description() + QStringLiteral(", ") + galaxy->Name();
	for(auto blip = blips.begin(); blip != blips.end(); ++blip)
	{
		const AuSensorBlip* contact = *blip;
		if(!contact->IsSensorConsoleBlip() || contact->Range2(cityLocation) > rangeSquared) continue;
		const AuEventLogEntry::TYPE kind = contact->OfficerEventLogType();
		QSharedPointer<AuJob> job;
		AuId jobKey = contact->AtomId();
		switch(kind)
		{
		case AuEventLogEntry::EnemyCapital:
		case AuEventLogEntry::EnemyCity:
			LogEvent(quint8(kind), contact->HtmlDesc(system));
			if(LegacyCityCaptureJobsEnabled)
				job.reset(new AuJobCaptureLegacyCity(contact->AtomId(), contact->ScreenName(), system->Id(), system->Name(), address,
					contact->IsEmpireCapitol() ? AuJobCaptureLegacyCity::EmpireCapital
						: contact->IsSectorCapitol() ? AuJobCaptureLegacyCity::SectorCapital : AuJobCaptureLegacyCity::Enemy));
			break;
		case AuEventLogEntry::EnemyStation:
		case AuEventLogEntry::EnemyStationS:
			if(contact->IsNewSpacecraft()) continue;
			LogEvent(quint8(kind), contact->HtmlDesc(system));
			job.reset(new AuJobDestroyEnemyStation(contact->AtomId(), contact->ScreenName(), system->Id(), system->Name(), address));
			break;
		case AuEventLogEntry::EnemySpaceship:
		case AuEventLogEntry::EnemySpaceshipS:
		case AuEventLogEntry::EnemyStarship:
		case AuEventLogEntry::EnemyStarshipS:
			if(contact->IsNewSpacecraft()) continue;
			LogEvent(quint8(kind), contact->HtmlDesc(system));
			job.reset(new AuJobInterdictSystem(contact->EmpireId(), system->Id(), system->Name(), address));
			jobKey = system->Id(); // One system interdiction request per report.
			break;
		case AuEventLogEntry::EnemyAircraft:
		case AuEventLogEntry::EnemyBoat:
		case AuEventLogEntry::EnemyVehicle:
			if(!contact->IsNewVehicle()) LogEvent(quint8(kind), contact->HtmlDesc(system));
			break;
		default: break;
		}
		if(job)
		{
			QWriteLocker lock(&m_CityLock);
			if(m_EnemyJobs.size() < 4096 || m_EnemyJobs.contains(jobKey)) m_EnemyJobs.insert(jobKey, job);
		}
	}
}

void DaLegacyCity::PostCityBountyJob()
{
	if(!LegacyCityCaptureJobsEnabled) return;
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !g_Au.m_EnableJobPosting
		|| InArena() || InDesignPreview() || IsDead()) return;
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	const auto* system = world ? world->FindParentSolarSystem() : nullptr;
	const auto* sector = FindParentSector();
	const auto* galaxy = FindParentGalaxy();
	if(!world || !system || !sector || !galaxy) return;
	const State state = Snapshot();
	double bounty = 0; QStringList reasons;
	for(const auto& entry : state.bounties) { bounty += entry.reward; reasons.append(entry.reason); }
	if(bounty <= 0) return;
	AuJobBountyHunterLegacyCity job(Id(), state.name, state.controllingEmpire,
		tr("%1, %2, %3").arg(world->Name(), system->Name(), sector->Name()), bounty, reasons);
	emit PostJobOffer(AuJobOffer(state.name, 0, sector->Location(), galaxy->Galaxy(), &job));
}

void DaLegacyCity::PostCivicJobs(const AuCityReport& report)
{
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker()
		|| InArena() || InDesignPreview() || IsDead() || report.m_MessageType == AuMailMsg::MSG_CityFinalDecayReport) return;
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	const auto* system = world ? world->FindParentSolarSystem() : nullptr;
	const auto* sector = FindParentSector();
	const auto* galaxy = FindParentGalaxy();
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!world || !system || !sector || !galaxy || !catalogue) return;
	if(!AuLegacyItemTechnologyActive()) return;
	for(const quint16 commodity : catalogue->RuntimeCommodityIds())
		if(!g_Au.HasLegacyCommodity(COMMODITYID(commodity))) return;
	const State state = Snapshot();
	AccountingState accounting; ProductionState production; AuItemHash inventory; QString error;
	if(!DecodeAccounting(state.accounting, accounting, error)
		|| !DecodeProductionState(*catalogue, state.manufacturing, production, error)
		|| !ReadCityInventory(state.inventory, *catalogue, inventory, error)) return;
	const auto post = [&](const AuJob& job, quint32 empire) {
		if(g_Au.m_EnableJobPosting) emit PostJobOffer(AuJobOffer(state.name, empire, sector->Location(), galaxy->Galaxy(), &job));
	};
	QHash<AuId, QSharedPointer<AuJob>> enemyJobs;
	{
		QWriteLocker lock(&m_CityLock);
		enemyJobs.swap(m_EnemyJobs);
	}
	for(auto job = enemyJobs.cbegin(); job != enemyJobs.cend(); ++job)
		if(job.value()) post(*job.value(), state.controllingEmpire);
	// Original report gives foreign flag holders intelligence and an in-progress
	// capture mission. Occupied loyalists without a flag relay the same summary
	// only when their strictly-less-than loyalty roll succeeds.
	QSet<quint32> flagHolders;
	for(const Flag& flag : state.flags)
		if(flag.empire && flag.empire != state.controllingEmpire) flagHolders.insert(flag.empire);
	if(LegacyCityCaptureJobsEnabled && !flagHolders.isEmpty())
	{
		AuJobCaptureLegacyCity job(Id(), state.name, system->Id(), system->Name(), sector->Name(), AuJobCaptureLegacyCity::UnderAttack);
		post(job, state.allegianceEmpire);
	}
	AuIdList places; places.append(Id()); ChainOfPlaces(places);
	const auto intelligence = [&](quint32 empire, AuJobCaptureLegacyCity::Reason reason) {
		AuIdList governors;
		const auto& data = g_Au.LockDataForRead();
		const bool pirate = data.EmpireRole(empire) == EMPIRE_Pirate;
		if(!pirate) data.EmpireDomainGovernors(empire, places, governors);
		const quint32 player = data.PlayerId(), confirmation = data.ConfirmationNumber();
		g_Au.UnlockData();
		if(pirate || governors.isEmpty()) return;
		QString body = MailLocationHeader(report.m_WhenCompiled);
		body += tr("<p>Population: %1. Morale: %2.</p>").arg(state.population).arg(state.satisfaction);
		for(int index = 0; index < state.flags.size(); ++index)
		{
			const Flag& flag = state.flags.at(index);
			QString owner;
			const auto& flagData = g_Au.LockDataForRead(); flagData.EmpireName(flag.empire, owner); g_Au.UnlockData();
			if(owner.isEmpty()) owner = flag.empire ? tr("Empire %1").arg(flag.empire) : tr("Unclaimed");
			body += tr("<p>%1 flag at %2: %3, raised %4%.</p>")
				.arg(index ? tr("Military") : tr("Town square"), flag.location.ToString().toHtmlEscaped(), owner.toHtmlEscaped())
				.arg(qRound(100.0 * flag.level / MaxFlagLevel));
		}
		AuPoint galaxyLocation; system->GetLocationRelativeToParent(galaxyLocation, galaxy);
		if(!InArena() && !InDesignPreview())
			g_Au.SendBulkMail(player, confirmation, Id(), state.name, governors, AuMailMsg::MSG_CityIntelligenceReport,
				tr("City Intelligence Report"), body, system->Id(), system->Name(), world->Id(), world->Name(), Galaxy(), galaxyLocation);
		if(LegacyCityCaptureJobsEnabled)
		{
			AuJobCaptureLegacyCity job(Id(), state.name, system->Id(), system->Name(), sector->Name(), reason);
			post(job, empire);
		}
	};
	for(const quint32 holder : flagHolders) intelligence(holder, AuJobCaptureLegacyCity::InProgress);
	if(report.m_LegacyOccupied && state.population && state.allegianceEmpire
		&& !flagHolders.contains(state.allegianceEmpire))
	{
		const int loyalty = qBound(0, qRound(report.m_LoyaltyOrig * 100), 100);
		if(g_Au.Dice().Roll(1, 100) < loyalty) intelligence(state.allegianceEmpire, AuJobCaptureLegacyCity::Occupied);
	}
	const quint32 population = report.m_PopOrig;
	const quint32 quantity = quint32(std::min(quint64(std::numeric_limits<quint32>::max()), quint64(population) * 4));
	if(population && !report.m_HasBreathableAtmo && report.m_AirConsumed < population)
	{
		const COMMODITYID air = COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(5)));
		const quint32 price = JobPrice(accounting.accounts.citizens, quantity, report.HasAir() ? 1000 : 10000);
		if(report.HasAir())
		{
			AuJobDeliverCargoGasping job(Id(), state.name, system->Id(), system->Name(), sector->Name(), air, 1, quantity, price);
			post(job, state.controllingEmpire);
		}
		else
		{
			AuJobDeliverCargoSuffocating job(Id(), state.name, system->Id(), system->Name(), sector->Name(), air, 1, quantity, price);
			post(job, state.controllingEmpire);
		}
	}
	if(population && !report.m_AteEnoughFood)
	{
		const AuSpecies species(state.citizenSpecies);
		const quint32 price = JobPrice(accounting.accounts.citizens, quantity, report.m_FoodUnits ? 500 : 5000);
		if(report.m_FoodUnits)
		{
			AuJobDeliverFoodHungry job(Id(), state.name, system->Id(), system->Name(), sector->Name(), species.EcoRole(), 1, quantity, price);
			post(job, state.controllingEmpire);
		}
		else
		{
			AuJobDeliverFoodStarving job(Id(), state.name, system->Id(), system->Name(), sector->Name(), species.EcoRole(), 1, quantity, price);
			post(job, state.controllingEmpire);
		}
	}
	// CityCycle 0x27a5... posts at most one ordinary manufacturing and one
	// spacecraft-factory shortage. Any stock at the selected TL suppresses a
	// request, even when that stock cannot yet fill the entire recipe.
	bool offeredManufacturing = false, offeredSpacecraft = false;
	for(const ProductionProcess& process : production.processes)
	{
		if(offeredManufacturing && offeredSpacecraft) break;
		const bool spacecraft = process.spacecraftDesign != 0;
		if(!process.enabled || (spacecraft ? offeredSpacecraft : offeredManufacturing)) continue;
		const auto* recipe = spacecraft ? nullptr : catalogue->ManufacturingProcess(ManufacturingProcessId(process.processId));
		if(!spacecraft && !recipe) continue;
		for(const ProductionComponent& component : process.components)
		{
			if(component.effect != 5 || component.commodityId == 3 || component.commodityId == 157) continue;
			const COMMODITYID commodity = COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(component.commodityId)));
			if(AuZoneResource::IsFetchable(commodity)) continue;
			bool stocked = false;
			for(auto item = inventory.find(commodity); item != inventory.end() && item->first == commodity; ++item)
				if(item->second->Count() && item->second->TechLevel() >= process.minimumTechnology) { stocked = true; break; }
			if(stocked) continue;
			const quint32 required = quint32(std::min(quint64(std::numeric_limits<quint32>::max()),
				quint64(component.required ? component.required : 1) * 10));
			const quint8 technology = component.required ? process.minimumTechnology : 0;
			const quint8 quality = component.required ? quint8(technology << 3) : 0;
			const double cost = std::ceil(double(g_Au.CommodityQualityCost(commodity, std::max(quint8(128), quality), technology))) * 10;
			const quint32 price = JobPrice(accounting.accounts.citizens, required,
				quint32(std::min(double(std::numeric_limits<quint32>::max()), std::max(0.0, cost))));
			if(spacecraft)
			{
				AuJobDeliverCargoSpacecraftFactory job(Id(), state.name, system->Id(), system->Name(), sector->Name(),
					commodity, std::max(quint8(1), quality), required, price);
				job.SetMinimumTechnology(technology); post(job, state.controllingEmpire); offeredSpacecraft = true;
			}
			else
			{
				AuJobDeliverCargoManufacture job(Id(), state.name, system->Id(), system->Name(), sector->Name(),
					commodity, std::max(quint8(1), quality), required, price,
					COMMODITYID(catalogue->RuntimeCommodityId(recipe->commodity)));
				job.SetMinimumTechnology(technology); post(job, state.controllingEmpire); offeredManufacturing = true;
			}
			break;
		}
	}
	// Population recovery requests accompany the original seven-day decay grace.
	std::array<quint32, 256> levels = {};
	for(const auto& development : state.developments)
		if(!development.demolished) levels[development.industryId] += development.completedLevels;
	const int homes = Homes(*catalogue, levels);
	const bool overpopulated = population && report.m_ZonePop > report.m_MaxPop;
	bool evacuationOffered = !report.m_AteEnoughFood;
	if(overpopulated)
	{
		AuJobPickUpPassengersOverpopulated job(Id(), state.name, world->Id(), world->Name(), system->Id(), system->Name(), sector->Name(),
			JobPrice(accounting.accounts.citizens, 1, 10000));
		post(job, state.controllingEmpire); evacuationOffered = true;
	}
	if(!evacuationOffered && !report.m_IsOnRingworld && qint64(population) - homes >= 4)
	{
		AuJobPickUpPassengersHomeless job(Id(), state.name, world->Name(), system->Id(), system->Name(), sector->Name(),
			JobPrice(accounting.accounts.citizens, 1, 5000));
		post(job, state.controllingEmpire); evacuationOffered = true;
	}
	if(!evacuationOffered && report.m_LegacyReportedPopulation.emigrants > 0
		&& !world->IsHabitable(HABITABLE_System) && report.m_IsIsolated && !levels[111])
	{
		AuJobPickUpPassengersStranded job(Id(), state.name, world->Name(), system->Id(), system->Name(), sector->Name(),
			JobPrice(accounting.accounts.citizens, 1, 10000));
		post(job, state.controllingEmpire);
	}
	const bool old = CreatedTime().SecsTo(report.m_WhenCompiled) > 7 * 24 * 60 * 60;
	const quint32 residents = quint32(std::max(0, state.population));
	if(old && report.HasDecay() && residents < 2 && homes > 0)
	{
		AuJobDeliverPassengersDeserted job(Id(), state.name, system->Id(), system->Name(), sector->Name(), quint32(homes),
			JobPrice(accounting.accounts.citizens, quint32(homes), 10000));
		post(job, state.controllingEmpire);
	}
	else if(old && report.HasDecay() && residents < quint32(homes))
	{
		const quint32 missing = quint32(homes) - residents;
		AuJobDeliverPassengersColonists job(Id(), state.name, system->Id(), system->Name(), sector->Name(), missing,
			JobPrice(accounting.accounts.citizens, missing, 5000));
		post(job, state.controllingEmpire);
	}
	if(old && report.HasDecay() && residents >= 2 && report.m_JobsNonConst > qint32(residents))
	{
		const quint32 missing = quint32(report.m_JobsNonConst) - residents;
		AuJobDeliverPassengersWorkers job(Id(), state.name, system->Id(), system->Name(), sector->Name(), missing,
			JobPrice(accounting.accounts.citizens, missing, 5000));
		post(job, state.controllingEmpire);
	}
	if(!report.m_LegacyOccupied && report.m_LastVisit >= 14 && report.m_LastVisit <= 1000)
	{
		AuJobVisitSystem job(Id(), system->Id(), system->Name(), sector->Name(),
			JobPrice(accounting.accounts.citizens, 1, quint32((report.m_LastVisit - 7) / 7) * 10000));
		post(job, state.controllingEmpire);
	}
}
