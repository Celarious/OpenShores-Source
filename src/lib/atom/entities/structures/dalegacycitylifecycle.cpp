#include "dalegacycity.h"
#include "aulegacycitylifecycle.h"
#include "aulegacycitypopulationruntime.h"
#include "aulegacycityinfrastructure.h"
#include "aulegacycityproduction.h"
#include "aulegacycitycatalogue.h"
#include "aucityreport.h"
#include "aucondition.h"
#include "aucommunicationevent.h"
#include "aucommpacket.h"
#include "audice.h"
#include "audnabase.h"
#include "aueventlog.h"
#include "auglobal.h"
#include "auitem.h"
#include "auspecies.h"
#include "dacreature.h"
#include "daperson.h"
#include "davessel.h"
#include "dhdna.h"
#include "daworldll.h"
#include <QDateTime>
#include <QReadLocker>
#include <QWriteLocker>
#include <algorithm>
#include <cstdlib>
#include <limits>

namespace AuLegacyCity
{
namespace
{
bool HasCrisis(const State& state, quint8 type)
{
	for(const Crisis& crisis : state.crises) if(crisis.type == type) return true;
	return false;
}

QString CrisisName(const Crisis& crisis)
{
	if(crisis.type == 1) return QObject::tr("Poison %1").arg(crisis.quality + 1);
	if(crisis.type == 2) return QObject::tr("Disease %1").arg(crisis.quality + 1);
	return QObject::tr("%1 vermin %2").arg(AuSpecies(crisis.species).Description()).arg(crisis.quality + 1);
}

void LogCrisis(State& state, quint8 type, const QString& text, qint64 now)
{
	AppendCityEvent(state, quint8(type == 1 ? AuEventLogEntry::Poison
		: type == 2 ? AuEventLogEntry::Disease : AuEventLogEntry::Infestation), text, now);
}

// One item per untreated person/vermin per report, with Q+1 treatment units.
// Low-quality medicine remains useful; item technology does not gate cures.
quint32 TreatCrisis(AuItemHash& inventory, COMMODITYID medicine, quint32 population,
	Crisis& crisis, quint64& needed)
{
	const quint32 untreated = population > crisis.treatedUnits / (quint32(crisis.quality) + 1)
		? population - crisis.treatedUnits / (quint32(crisis.quality) + 1) : 0;
	const quint64 totalNeed = quint64(population) * (quint32(crisis.quality) + 1);
	needed = totalNeed > crisis.treatedUnits ? totalNeed - crisis.treatedUnits : 0;
	quint32 itemsUsed = 0;
	for(auto item = inventory.find(medicine); item != inventory.end() && item->first == medicine && needed && itemsUsed < untreated;)
	{
		AuItem* stock = item->second;
		const quint32 potency = quint32(stock->Quality()) + 1;
		const quint32 count = qMin(qMin(stock->Count(), untreated - itemsUsed),
			quint32(qMin(quint64(std::numeric_limits<quint32>::max()), (needed + potency - 1) / potency)));
		const quint64 units = qMin(needed, quint64(count) * potency);
		crisis.treatedUnits = quint32(qMin(quint64(std::numeric_limits<quint32>::max()), quint64(crisis.treatedUnits) + units));
		needed -= units; itemsUsed += count;
		if(count >= stock->Count()) item = inventory.Erase(item);
		else { stock->ScaleCount(stock->Count() - count); ++item; }
	}
	return itemsUsed;
}

void KillCitizens(State& state, quint32 deaths, const PopulationRuntimeFacts& facts, LifecycleResult& result)
{
	const quint32 original = quint32(state.population);
	deaths = qMin(deaths, original);
	state.population -= qint32(deaths);
	if(original <= state.loyalPopulation)
		state.loyalPopulation = qMax(quint32(state.population), state.loyalPopulation - qMin(state.loyalPopulation, deaths));
	if(!state.population && facts.context.indigenousWorld && !facts.nativeSpecies.isEmpty()
		&& state.citizenSpecies != facts.nativeSpecies)
	{
		state.citizenSpecies = facts.nativeSpecies;
		result.citizenSpeciesChanged = true;
	}
	result.deaths += deaths;
}

std::array<quint32, 256> Levels(const State& state)
{
	std::array<quint32, 256> levels = {};
	for(const Development& development : state.developments)
		if(!development.demolished) levels[development.industryId] += development.completedLevels;
	return levels;
}

bool DecayOne(const AuLegacyCityCatalogue& catalogue, State& state, ProductionState& production,
	AuCityReport& report, int mode, int reportSlot, LifecycleResult& result)
{
	QVector<int> eligible;
	int active = 0, townIndex = -1;
	for(int index = 0; index < state.developments.size(); ++index)
	{
		const Development& development = state.developments[index];
		if(development.demolished) continue;
		++active;
		if(development.industryId == 1) townIndex = index;
		const bool housing = development.industryId == 20 || development.industryId == 21
			|| development.industryId == 22 || development.industryId == 30 || development.industryId == 32;
		const bool business = development.industryId != 1 && development.industryId != 2
			&& development.industryId != 114 && !housing;
		if((mode == 0 && development.industryId != 1) || (mode == 1 && housing) || (mode == 2 && business))
			eligible.append(index);
	}
	// CanDeleteTownSquareBuildingsLocked: the town square is the final site.
	if(active == 1 && townIndex >= 0) eligible = QVector<int>(1, townIndex);
	if(eligible.isEmpty()) return false;
	const int index = eligible[g_Au.Dice().Roll(1, eligible.size(), -1)];
	Development& development = state.developments[index];
	const quint32 id = development.id;
	report.m_Decayed[reportSlot] = INDUSTRYID(catalogue.RuntimeIndustryId(IndustryId(development.industryId)));
	++result.decayed; result.changed = true;
	if(development.industryId == 2 || development.completedLevels <= 1)
	{
		result.removed.append(development);
		if(development.industryId == 1) result.finalDecay = true;
		if(development.industryId == 1 || development.industryId == 10)
			for(int flag = state.flags.size() - 1; flag >= 0; --flag)
				if(state.flags[flag].location == development.location) state.flags.removeAt(flag);
		state.developments.removeAt(index);
	}
	else
	{
		// RemoveConstructedLevel keeps an unfinished extension and its bill of
		// materials. The existing geometry helper refreshes its deck bounds.
		--development.completedLevels;
		if(development.components.isEmpty()) development.targetLevels = development.completedLevels;
		result.reduced.append(id);
	}
	const auto remaining = std::find_if(state.developments.cbegin(), state.developments.cend(), [id](const Development& site) { return site.id == id; });
	const quint16 remainingProcessSlots = remaining == state.developments.cend() ? 0 : ManufacturingSlots(remaining->industryId, remaining->completedLevels);
	production.processes.erase(std::remove_if(production.processes.begin(), production.processes.end(), [id, remainingProcessSlots](const ProductionProcess& process) {
		return process.developmentId == id && process.slot >= remainingProcessSlots;
	}), production.processes.end());
	state.lastEmployment = 0;
	return result.finalDecay;
}
}

void AppendCityEvent(State& state, quint8 eventType, const QString& text, qint64 now)
{
	if(eventType < 1 || eventType > quint8(AuEventLogEntry::AsteroidDepartedDeepSpace) || now < 0) return;
	AuEventLog log;
	for(const CityEvent& pending : state.pendingEvents)
	{
		AuEventLogEntry event(AuEventLogEntry::TYPE(pending.type), pending.text);
		event.m_Time = AuTime(pending.when); log.append(event);
	}
	AuEventLogEntry event(AuEventLogEntry::TYPE(eventType), text.left(4096));
	event.m_Time = AuTime(now);
	// Reuse original repeat detection and three-day retention. The separate
	// count bound protects saved/network records from an unbounded chat log.
	log.AddEventTrim(event);
	while(log.size() > 256) log.removeFirst();
	state.pendingEvents.clear();
	for(const AuEventLogEntry& stored : log)
	{
		CityEvent pending; pending.type = quint8(stored.m_Type);
		pending.text = stored.m_Text; pending.when = stored.m_Time.MSecsSinceEpoch();
		state.pendingEvents.append(pending);
	}
}

void CopyCityEvents(const State& state, AuCityReport& report)
{
	for(const CityEvent& pending : state.pendingEvents)
	{
		AuEventLogEntry event(AuEventLogEntry::TYPE(pending.type), pending.text);
		event.m_Time = AuTime(pending.when); report.m_EventLog->AddEventTrim(event);
	}
	if(!state.pendingEvents.isEmpty())
	{
		report.m_MessageType = AuMailMsg::MSG_CityDistressReport;
		report.m_DistressRpt += report.m_EventLog->ReportAlertsHtml();
	}
}

void DrainCityEvents(State& state, AuCityReport& report)
{
	CopyCityEvents(state, report);
	state.pendingEvents.clear();
}

void ApplyCityCrises(const AuLegacyCityCatalogue& catalogue, State& state, AuItemHash& inventory,
	const PopulationRuntimeFacts& facts, AuCityReport& report, qint64 now, LifecycleResult& result)
{
	// AuSitDisease/Poison/Rats::Report, AuAtom13 1b4950/1b5260/1b5b70.
	for(int index = 0; index < state.crises.size();)
	{
		Crisis& crisis = state.crises[index];
		std::unique_ptr<AuDNABase> verminDNA;
		if(crisis.type == 3 && !ReadSpeciesDNA(crisis.species, verminDNA))
		{ state.crises.removeAt(index); result.changed = true; continue; }
		const QString name = CrisisName(crisis);
		const qint64 duration = (quint32(crisis.strength) + 1) * (quint32(crisis.quality) + 1) * qint64(86400) / 28;
		const qint64 elapsed = qMax(qint64(0), (now - crisis.createdAt) / 1000);
		bool retain = elapsed < duration;
		QString text;
		if(!retain) text = crisis.type == 1 ? QObject::tr("Food poisoning abated. %1 has passed.").arg(name)
			: crisis.type == 2 ? QObject::tr("Epidemic abated. %1 has run its course.").arg(name)
			: QObject::tr("Infestation abated. %1 have gone away.").arg(name);
		else
		{
			quint32 affected = quint32(state.population);
			bool noVerminFood = false;
			ECOROLE diet = ECOROLE(0);
			if(crisis.type == 3)
			{
				diet = verminDNA->EcoRole();
				const int food = inventory.TotalFoodUnits(diet);
				noVerminFood = food <= 0;
				affected = food > 0 ? quint32(quint64(food) * (quint32(crisis.strength) + 1) / 256) : 0;
			}
			const quint16 historical = crisis.type == 1 ? 119 : crisis.type == 2 ? 120 : 62;
			const bool alreadyTreated = affected <= crisis.treatedUnits / (quint32(crisis.quality) + 1);
			quint64 needed = 0;
			const quint32 used = TreatCrisis(inventory, COMMODITYID(catalogue.RuntimeCommodityId(CommodityId(historical))), affected, crisis, needed);
			retain = needed != 0;
			if(noVerminFood) text = QObject::tr("Infestation starved. City has no food for %1.").arg(name);
			else if(!retain && alreadyTreated) text = crisis.type == 1 ? QObject::tr("Food poisoning cured. All remaining citizens received antidotes for %1.").arg(name)
				: crisis.type == 2 ? QObject::tr("Epidemic cured. All remaining citizens received antitoxins for %1.").arg(name)
				: QObject::tr("Infestation eliminated. Pharmaceuticals poisoned the remaining %1.").arg(name);
			else if(!retain) text = crisis.type == 1 ? QObject::tr("Food poisoning cured. Gave %2 antidotes to citizens for %1.").arg(name).arg(used)
				: crisis.type == 2 ? QObject::tr("Epidemic cured. Gave %2 antitoxins to citizens for %1.").arg(name).arg(used)
				: QObject::tr("Infestation eliminated. Poisoned %1 with %2 pharmaceuticals.").arg(name).arg(used);
			else if(crisis.type == 3)
			{
				const quint32 eaten = quint32(inventory.EatFood(g_Au.Dice().Roll(1, int(qMin(needed, quint64(std::numeric_limits<int>::max())))), diet));
				text = QObject::tr("Infestation. %1 pharmaceuticals still needed for %2. %3 food eaten.")
					.arg((needed + crisis.quality) / (quint32(crisis.quality) + 1)).arg(name).arg(eaten);
			}
			else
			{
				const quint32 sick = quint32((needed + crisis.quality) / (quint32(crisis.quality) + 1));
				qint64 severity = 0;
				if(crisis.type == 1) severity = (crisis.strength * (duration - elapsed) / duration + 9) / 10;
				else
				{
					const qint64 peak = duration / 4;
					severity = elapsed <= peak && peak ? (crisis.strength * elapsed / peak + 9) / 10
						: (crisis.strength - crisis.strength * (elapsed - peak) / (duration - peak) + 9) / 10;
				}
				const quint32 deaths = quint32(g_Au.Dice().Roll(1, int(quint64(sick) * severity / 255) + 1, -1));
				KillCitizens(state, qMin(deaths, sick), facts, result);
				text = crisis.type == 1 ? QObject::tr("Food poisoning. %1 antidotes still needed to cure %2. %3 citizens died.").arg(sick).arg(name).arg(deaths)
					: QObject::tr("Epidemic. %1 antitoxins still needed to cure %2. %3 citizens died.").arg(sick).arg(name).arg(deaths);
			}
		}
		LogCrisis(state, crisis.type, text, now);
		result.changed = true;
		if(retain) ++index; else state.crises.removeAt(index);
	}
	report.m_LegacyCrisisDeaths = result.deaths;
}

void ApplyCityDecay(const AuLegacyCityCatalogue& catalogue, State& state, ProductionState& production,
	AuCityReport& report, qint64 createdAt, qint64 now, LifecycleResult& result)
{
	// CityCycle 0x2749d0: one real week of grace; population <2 loses up
	// to three sites/levels. Populated cities shed excessive homes/businesses.
	if(now < createdAt || (now - createdAt) / 1000 <= 604800) return;
	if(state.population < 2)
	{
		for(int slot = 0; slot < 3 && !result.finalDecay; ++slot)
			DecayOne(catalogue, state, production, report, 0, slot, result);
	}
	else
	{
		const int homes = Homes(catalogue, Levels(state));
		quint32 jobs = 0;
		QHash<quint32, quint32> enabled;
		for(const ProductionProcess& process : production.processes) if(process.enabled) ++enabled[process.developmentId];
		for(const Development& development : state.developments)
			jobs += DevelopmentNonConstructionJobs(catalogue, development, enabled.value(development.id));
		const float capacityRatio = qMax(1.05f, float(report.m_MaxPop) / float(state.population));
		if(homes > state.population && capacityRatio < float(homes - 25) / float(state.population))
			DecayOne(catalogue, state, production, report, 1, 0, result);
		if(!result.finalDecay && jobs > quint32(state.population)
			&& capacityRatio < float(qint64(jobs) - 25) / float(state.population))
			DecayOne(catalogue, state, production, report, 2, 1, result);
	}
	if(result.decayed)
	{
		report.m_MessageType = result.finalDecay ? AuMailMsg::MSG_CityFinalDecayReport : AuMailMsg::MSG_CityDistressReport;
		report.m_DistressRpt += QObject::tr("City is decaying.<br>");
	}
}

void GenerateCityCrisis(State& state, const PopulationRuntimeReport& population, int decayed, qint64 now)
{
	// CityCycle 0x27ebd9: 1/112 reports try one of the three crises.
	if(g_Au.Dice().Roll(1, 112) != 1) return;
	const quint8 crisisTypes[3] = {2, 1, 3}; // Original CityCycle byte table.
	const quint8 type = crisisTypes[g_Au.Dice().Roll(1, 3, -1)];
	if(HasCrisis(state, type)) return;
	int foodPenalty = 0, crowding = 0, homeless = 0;
	for(const MoraleFactor& factor : population.change.moraleFactors)
	{
		if(factor.type == MoraleFactorType::Starvation) foodPenalty = 5;
		else if(factor.type == MoraleFactorType::Hunger) foodPenalty = 2;
		else if(factor.type == MoraleFactorType::ResourceZoneOverpopulation) crowding = -factor.modifier;
		else if(factor.type == MoraleFactorType::Homelessness) homeless = -factor.modifier;
	}
	const int hospital = population.change.hospitalShortage;
	const int risk = type == 1 ? decayed + foodPenalty : type == 2 ? crowding + homeless + hospital : hospital + 1;
	if((type != 3 && !state.population) || (type == 3 && (!population.ateEnough || !population.foodTypes) && !hospital)
		|| risk <= 0 || g_Au.Dice().Roll(1, 100) > risk) return;
	Crisis crisis; crisis.type = type; crisis.createdAt = now;
	if(type == 3)
	{
		DhDNA vermin; vermin.RandomVermin(g_Au.Dice());
		crisis.species = vermin.Species().Key();
	}
	crisis.strength = quint8(std::abs(g_Au.Dice().Roll(2, 256) - 257));
	if(type == 2) crisis.contagion = quint8(std::abs(g_Au.Dice().Roll(2, 256) - 257));
	crisis.quality = quint8(std::abs(g_Au.Dice().Roll(2, 32) - 33));
	state.crises.append(crisis);
	if(type == 2)
	{
		const QString title = g_Au.Dice().Roll(1, 2) == 1 ? QStringLiteral("Ms.") : QStringLiteral("Mr.");
		const QString vector = QObject::tr("%1 %2").arg(title, g_Au.Dice().RandomPersonName());
		const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
		QVector<quint8> industries;
		for(const Development& development : state.developments)
			if(!development.demolished && development.completedLevels && !industries.contains(development.industryId))
				industries.append(development.industryId);
		std::sort(industries.begin(), industries.end());
		QString source = QObject::tr("Unknown");
		if(catalogue && !industries.isEmpty())
		{
			const auto* industry = catalogue->Industry(IndustryId(industries[g_Au.Dice().Roll(1, industries.size(), -1)]));
			if(industry) source = industry->name;
		}
		LogCrisis(state, type, QObject::tr("%1 Outbreak! Vector: %2. Source: %3.").arg(CrisisName(crisis), vector, source), now);
	}
}
}

void DaLegacyCity::LogEvent(quint8 eventType, const QString& text)
{
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || IsDead()
		|| eventType < 1 || eventType > quint8(AuEventLogEntry::AsteroidDepartedDeepSpace)) return;
	QWriteLocker lock(&m_CityLock);
	AuLegacyCity::AppendCityEvent(m_City, eventType, text, AuTime::CurrentMSecsSinceEpoch());
	++m_CityRevision;
	lock.unlock(); SetModified();
}

void DaLegacyCity::LogReceivedCommunication(const AuCommPacket* message)
{
	if(!message || !g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || IsDead()) return;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	auto* system = world ? world->FindEnvironmentAtom() : nullptr;
	if(!world || !system) return;
	const AuLegacyCity::State state = Snapshot();
	DaAtom* sender = world->FindIdRecursive(message->FromId());
	if(!sender)
	{
		const bool airport = std::any_of(state.developments.cbegin(), state.developments.cend(),
			[](const AuLegacyCity::Development& development) { return development.industryId == 111 && development.completedLevels && !development.demolished; });
		if(!airport) return;
		sender = system->FindIdRecursive(message->FromId());
	}
	const auto* person = dynamic_cast<const DaPerson*>(sender);
	const auto* directShip = dynamic_cast<const DaVessel*>(sender);
	if(!person && !directShip) return;
	const auto* aboardShip = person && person->IsInsideSpacecraft() ? person->FindParentVesselInEnv() : nullptr;
	const quint32 senderEmpire = aboardShip ? aboardShip->EmpireId() : sender->EmpireId();
	const DaAtom* contact = aboardShip ? static_cast<const DaAtom*>(aboardShip) : sender;
	const POLITICAL_STANCE stance = DaAtom::PoliticalStanceMakeContact(state.controllingEmpire, contact, senderEmpire);
	// RxChatMessage 0x2bfef0 logs all enemy packets and plain Chat packets.
	if(stance != POL_Enemy && message->Type() != AuCommPacket::Chat) return;
	const AuEventLogEntry event = AuReceivedCommunicationEvent(*message, stance, person, directShip, aboardShip);
	if(event.m_Type != AuEventLogEntry::Invalid) LogEvent(quint8(event.m_Type), event.m_Text);
}

void DaLegacyCity::TockLifecycle(qint64 previous, qint64 now)
{
	Q_UNUSED(previous);
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !IsNormal() || !Parent()) return;
	const AuLegacyCity::State state = Snapshot();
	for(const AuLegacyCity::Development& development : state.developments)
		if(development.industryId == 1 && development.completedLevels && !development.demolished) return;
	// Tock 0x2d3100 gives a new city four minutes before missing-square
	// cleanup. Removing the final site also removes the empire registry entry.
	if(now - CreatedTime().MSecsSinceEpoch() < 4 * 60000) return;
	g_Au.StopMonitoringChannels(Id());
	emit CityDecayed(state.controllingEmpire, Id());
	if(DaWorldLL* world = dynamic_cast<DaWorldLL*>(Parent())) world->InvalidateEnvironmentRecursive();
	Kill();
}

void DaLegacyCity::AddCrisis(quint8 type, quint8 strength, quint8 contagion, quint8 quality, const QByteArray& species)
{
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || type < 1 || type > 3 || species.size() > 4096) return;
	if(type == 3)
	{
		std::unique_ptr<AuDNABase> dna;
		if(!AuLegacyCity::ReadSpeciesDNA(species, dna)) return;
	}
	AuLegacyCity::Crisis crisis;
	crisis.type = type; crisis.strength = strength; crisis.contagion = contagion;
	crisis.quality = quality; crisis.createdAt = QDateTime::currentMSecsSinceEpoch(); crisis.species = species;
	QWriteLocker lock(&m_CityLock);
	for(const AuLegacyCity::Crisis& existing : m_City.crises) if(existing.type == type) return;
	m_City.crises.append(crisis); ++m_CityRevision;
	lock.unlock(); SetModified();
}

void DaLegacyCity::SpreadDisease(const DaCreature* creature, const QString& sourceName)
{
	if(!creature || !g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker()) return;
	const AuConDisease* disease = dynamic_cast<const AuConDisease*>(creature->FindConditionLock(AuCondition::Disease));
	const bool infectious = disease && disease->IsContagious();
	const quint8 strength = infectious ? disease->Strength() : 0;
	const quint8 contagion = infectious ? disease->Contagion() : 0;
	const quint8 quality = infectious ? disease->Quality() : 0;
	creature->UnlockConditions();
	if(!infectious) return;
	AuLegacyCity::Crisis crisis;
	crisis.type = 2; crisis.strength = strength; crisis.contagion = contagion; crisis.quality = quality;
	crisis.createdAt = AuTime::CurrentMSecsSinceEpoch();
	const QString source = sourceName.isEmpty() ? tr("Unknown") : sourceName;
	const QString text = tr("%1 Outbreak! Vector: %2. Source: %3.")
		.arg(tr("Disease %1").arg(quality + 1), creature->Name(), source);
	QWriteLocker lock(&m_CityLock);
	for(const auto& existing : m_City.crises) if(existing.type == 2) return;
	m_City.crises.append(crisis);
	AuLegacyCity::AppendCityEvent(m_City, quint8(AuEventLogEntry::Disease), text, crisis.createdAt);
	++m_CityRevision;
	lock.unlock(); SetModified();
}
