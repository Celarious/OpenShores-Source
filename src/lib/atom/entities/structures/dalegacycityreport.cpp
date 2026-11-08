#include "dalegacycity.h"
#include "aulegacycitylifecycle.h"
#include "aucityreport.h"
#include "aucommpacket.h"
#include "auitem.h"
#include "aulegacycityinfrastructure.h"
#include "aulegacycitypopulationruntime.h"
#include "aulegacycityproduction.h"
#include "aulegacycityinventory.h"
#include "aulegacycitycommerce.h"
#include "auglobal.h"
#include "auqueryenvironment.h"
#include "auspecies.h"
#include "dagalaxy.h"
#include "dasector.h"
#include "dasolarsystem.h"
#include "daworldll.h"
#include "daperson.h"
#include "davessel.h"
#include "dgdata.h"
#include "dhdna.h"
#include <algorithm>
#include <limits>
#include <QWriteLocker>

using namespace AuLegacyCity;

bool DaLegacyCity::RxStatusChat(const AuCommPacket* message)
{
	if(!message || message->Type() != AuCommPacket::RequestInventory) return false;
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	const auto* sector = FindParentSector();
	auto* system = FindEnvironmentAtom();
	auto* person = system ? dynamic_cast<DaPerson*>(system->FindIdRecursive(message->FromId())) : nullptr;
	if(!g_Au.IsLegacyGeneration() || !catalogue || !world || !sector || !person || person->IsDead()) return true;
	if(person->IsInsideSpacecraft())
	{
		const auto* ship = person->FindParentVesselInEnv();
		if(!ship || !CanAccessSpacecraft(*ship)) return true;
	}
	else
	{
		const auto* environment = person->LockEnvironmentQuery();
		const bool inCity = environment->LegacyCityId() == Id();
		person->UnlockEnvironmentQuery();
		if(!inCity || person->FindParentPlanetInEnv() != world) return true;
	}
	const State state = Snapshot();
	AuIdList places; places.append(Id()); ChainOfPlaces(places);
	const DgData& authorityData = g_Au.LockDataForRead();
	const bool authorized = state.controllingEmpire && person->EmpireId() == state.controllingEmpire
		&& authorityData.IsEmpirePlaceInDomain(state.controllingEmpire, places, person->Id());
	g_Au.UnlockData();
	const auto range = AuCommStdChannel::RANGE(qMin(int(message->ReceptionRange()), int(AuCommStdChannel::SolarSystem)));
	if(!authorized && !person->HasAgentPower())
	{
		AuCommCityMessage response(AuCommCityMessage::NoChainNoInventory, message->ChannelName(), Id(), Name(),
			sector->Id(), Galaxy(), range, message->Scope());
		g_Au.SendAtomChat(response);
		return true;
	}
	// Transient server-side cooldown, per city and requester. Silently ignore
	// repeated clicks so neither mail nor Trade chat can be flooded.
	const qint64 now = AuTime::CurrentMSecsSinceEpoch();
	{
		QWriteLocker lock(&m_CityLock);
		for(auto request = m_StatusRequestTimes.begin(); request != m_StatusRequestTimes.end();)
			if(now >= request.value() && now - request.value() >= 30000) request = m_StatusRequestTimes.erase(request);
			else ++request;
		if(m_StatusRequestTimes.contains(person->Id())) return true;
		m_StatusRequestTimes.insert(person->Id(), now);
	}
	// ReportCurrent decodes stock only after Legacy metadata is active. It
	// does not run a population cycle or replace the saved periodic report.
	AuCityReport report;
	if(!ReportCurrent(report)) return true;
	QString flagsHtml = tr("<br><b>FLAG STATUS</b><div style=\"margin-left: 10px;\">");
	for(int index = 0; index < report.m_Flags.size(); ++index)
	{
		const auto& flag = report.m_Flags[index];
		QString empire;
		const DgData& data = g_Au.LockDataForRead();
		if(flag.m_Empire) data.EmpireName(flag.m_Empire, empire);
		g_Au.UnlockData();
		if(empire.isEmpty()) empire = flag.m_Empire ? tr("Empire %1").arg(flag.m_Empire) : tr("Unclaimed");
		const QString flagName = index == 0 ? tr("Town square flag")
			: tr("Military flag %1 (%2)").arg(index).arg(flag.m_DevLL.ToString());
		flagsHtml += tr("%1: %2, raised %3%.<br>").arg(flagName.toHtmlEscaped()).arg(empire.toHtmlEscaped())
			.arg(qRound(100.0 * flag.m_Level / MaxFlagLevel));
	}
	flagsHtml += QStringLiteral("</div>");
	if(!MailReport(report, report, person->Id(), flagsHtml, tr("Manual City Report"))) return true;
	AuCommCityChat response(tr("Current city status report sent to %1 by mail.").arg(person->Name()),
		AuCommStdChannel::TradeChannelName(), Id(), Name(), sector->Id(), Galaxy(), range, AuCommStdChannel::Everyone);
	g_Au.SendAtomChat(response);
	return true;
}

bool DaLegacyCity::ReportCurrent(AuCityReport& report)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	if(!g_Au.IsLegacyGeneration() || !catalogue || !world || !IsNormal()) return false;
	const auto state = Snapshot();
	AuItemHash inventory; ProductionState production; AccountingState accounting; QString error;
	if(!ReadCityInventory(state.inventory, *catalogue, inventory, error)
		|| !DecodeProductionState(*catalogue, state.manufacturing, production, error)
		|| !DecodeAccounting(state.accounting, accounting, error))
	{ qWarning() << "Legacy current city report:" << Id() << error; return false; }
	auto facts = GatherPopulationFacts(*this, *world, state);
	facts.context.isolated = QueryCityConnections(*this, *world, state).isEmpty();
	report = GatherCityReport(state, facts);
	CopyCityEvents(state, report);
	PopulationRuntimeReport population;
	population.occupied = state.controllingEmpire != state.allegianceEmpire;
	population.allegianceEmpire = state.allegianceEmpire;
	population.loyalPopulation = state.loyalPopulation;
	population.reportedLastTribute = accounting.lastTribute;
	population.troopsAtCity = quint16(std::min(65535, int(state.garrisonTroops) + facts.assignedTroops));
	QHash<quint32, quint32> enabled;
	for(const auto& process : production.processes) if(process.enabled) ++enabled[process.developmentId];
	auto developments = state.developments;
	const auto employment = EmployCitizens(*catalogue, developments, enabled,
		quint32(std::max(0, state.population)), facts.context.needsEnclosure, false);
	// A snapshot represents no population or morale change. The report codec
	// checks the starting population against the reported result; leaving the
	// result at its default zero makes every populated city's snapshot invalid.
	population.reportedChange.population = state.population;
	population.reportedChange.unemployed = int(employment.unemployed);
	population.reportedChange.satisfaction = state.population ? state.satisfaction : 1;
	population.change = population.reportedChange;
	// Complete the normal report envelope from copies. Do not run meals,
	// migration, salaries or advance lastPopulationCycle just to inspect stock.
	if(!CompleteCityReport(report, state, inventory, inventory, production, accounting,
		population, employment, AuTime::CurrentMSecsSinceEpoch(), error))
	{ qWarning() << "Legacy current city report:" << Id() << error; return false; }
	return true;
}

AuCityReport DaLegacyCity::GatherCityReport(const State& state, const PopulationRuntimeFacts& facts) const
{
	// DaCity::CityCycle 0x2749d0 captures these values before breathing,
	// eating, migration and salary circulation. World/global reads stay here.
	AuCityReport report;
	report.m_CityType = CITY_City_Obsolete;
	report.m_Name = HtmlName(state.name);
	report.m_PopOrig = quint32(std::max(0, state.population));
	report.m_SatOrig = state.satisfaction;
	report.m_LoyaltyOrig = quint32(std::max(0, state.population)) <= state.loyalPopulation ? 1.0f
		: float(state.loyalPopulation) / float(state.population);
	report.m_IsIsolated = facts.context.isolated;
	report.m_IsOnRingworld = facts.onRingworld;
	report.m_HasBreathableAtmo = !facts.context.needsEnclosure;
	report.m_IsHabitablePlanet = facts.context.indigenousWorld;
	report.m_HasDestroyedBuildings = facts.context.destroyedBuildings ? 1 : 0;
	report.m_MaxPop = quint32(std::max(0, facts.context.environmentCapacity));
	report.m_ZonePop = quint32(std::max(0, facts.context.environmentPopulation));
	report.m_LastVisit = facts.context.reportExemptFromVisitPenalty ? 0
		: quint16(std::min(65535, std::max(0, facts.context.daysSinceSystemVisit)));
	report.m_LastAvatarAccountActive = quint16(std::min(65535, std::max(0, facts.daysSinceCityVisit)));
	const QByteArray citizenKey = state.citizenSpecies.isEmpty() ? facts.citizenSpeciesFallback : state.citizenSpecies;
	std::unique_ptr<AuDNABase> citizenDNA, nativeDNA;
	const AuSpecies citizens = ReadSpeciesDNA(citizenKey, citizenDNA) ? AuSpecies(citizenKey) : AuSpecies();
	const AuSpecies native = ReadSpeciesDNA(facts.nativeSpecies, nativeDNA) ? AuSpecies(facts.nativeSpecies) : AuSpecies();
	bool alien = false;
	if(report.m_IsHabitablePlanet && citizenDNA && nativeDNA)
	{
		const auto* originalCitizen = dynamic_cast<const DhDNA*>(citizenDNA.get());
		const auto* originalNative = dynamic_cast<const DhDNA*>(nativeDNA.get());
		// Original CityCycle compares these two fields. DNA4 follows the same
		// compatibility boundary as ApplyCityPopulation's existing adapter.
		alien = originalCitizen && originalNative
			? originalCitizen->SkinType != originalNative->SkinType
				|| originalCitizen->EcologicalRole != originalNative->EcologicalRole
			: !citizenDNA->IsSexuallyCompatible(*nativeDNA);
	}
	if(!citizens.IsNull()) report.AddResidentSpecies(citizens, alien, report.m_PopOrig);
	if(!native.IsNull())
	{
		report.m_NativeSpecies = native.Name();
		report.m_NativeSpeciesDesc = native.Description();
	}
	AccountingState accounting; QString error;
	if(DecodeAccounting(state.accounting, accounting, error)) report.m_LastTribute = accounting.lastTribute;
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world) return report;
	// CityCycle 0x278e1d reports the world's named, undeployed cadet only
	// at an eligible academy, outside occupation and recent attacks.
	const bool occupied = state.population && state.loyalPopulation
		&& state.allegianceEmpire != state.controllingEmpire;
	if(!occupied && !facts.context.underAttack && HasOfficerAcademy() && world->CaptId().IsNull())
		report.m_CaptName = world->CaptName();
	report.m_PlanetName = world->HtmlName();
	report.m_PlanetDesc = world->Description();
	world->LLOfLoc(report.m_LL, Location());
	report.m_Zone = quint8(world->ResourceZone(report.m_LL.Longitude()));
	if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
	{
		const auto capacities = CityStorage(*catalogue, state.developments, quint8(world->OrbitZone()));
		report.m_iStorage = capacities.cargo;
		report.m_iCapacitor = capacities.electricity;
	}
	if(const auto* system = FindParentSolarSystem())
	{
		report.m_SystemName = system->HtmlNameWithSystem();
		report.m_SystemDesc = system->Description();
	}
	const auto* sector = FindParentSector();
	if(sector)
	{
		report.m_SectorName = sector->HtmlName();
		report.m_SectorDesc = sector->Description();
	}
	const auto* galaxy = FindParentGalaxy();
	if(galaxy) report.m_Galaxy = quint8(galaxy->Galaxy());
	AuId capital;
	const auto& data = g_Au.LockDataForRead();
	if(data.EmpireCapital(state.controllingEmpire, capital))
	{
		report.m_IsCapitalOfEmpire = capital == Id();
		if(sector) report.m_IsCapitalOfSector = data.EmpireSectorCapital(state.controllingEmpire, sector->Id()) == Id();
	}
	g_Au.UnlockData();
	for(int index = 0; index < state.flags.size(); ++index)
	{
		const auto& source = state.flags[index];
		AuFlag flag;
		flag.m_Empire = source.empire;
		flag.m_Level = source.level;
		flag.m_Capture = AuTime(source.capturedAt);
		flag.m_DevLL = source.location;
		flag.m_Terrain = world->Id();
		flag.m_IID = index == 0 ? IID_TownSquare : IID_BaseFlag;
		report.m_Flags.append(flag);
	}
	return report;
}

bool DaLegacyCity::CompleteCityReport(AuCityReport& report, const State& state,
	const AuItemHash& initialInventory, const AuItemHash& inventory, const ProductionState& production,
	const AccountingState& accounting, const PopulationRuntimeReport& population,
	const EmploymentTotals& employment, qint64 now, QString& error) const
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue || now < 0 || report.m_CityType != CITY_City_Obsolete)
	{ error = QStringLiteral("Invalid legacy city report context."); return false; }
	QByteArray ledger;
	if(!EncodeAccounting(accounting, ledger, error)) return false;
	// The normal report codec carries all common fields. Version 16 adds the
	// separate old accounts and actual cycle result rather than recomputing it
	// with the modern city's different morale and migration rules.
	report.m_LegacyAccounting = ledger;
	report.m_LegacyMoraleChange = qint8(std::max(-128, std::min(127, population.change.moraleChange)));
	report.m_LegacyPopulationAfter = state.population;
	report.m_TroopsAtCity = population.troopsAtCity;
	report.m_TroopsDrafted = population.troopsDrafted;
	report.m_HasLegacyDetails = true;
	report.m_LegacyReportedPopulation = population.reportedChange;
	report.m_LegacyOccupied = population.occupied;
	report.m_LegacyAllegianceEmpire = population.allegianceEmpire;
	report.m_LegacyLoyalPopulation = population.loyalPopulation;
	*report.m_Inventory = initialInventory;
	*report.m_Tools = production.tools;
	report.m_Vehicles.clear();
	for(const quint16 vehicle : production.vehicles)
		report.m_Vehicles.append(COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(vehicle))));
	report.m_AirSupply = initialInventory.RawCount(COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(5))));
	report.m_AirConsumed = qint32(population.airConsumed);
	report.m_FoodUnits = population.foodUnits;
	report.m_FoodTypes = qint8(population.foodTypes);
	report.m_FoodPerMeal = population.foodPerMeal;
	report.m_AteEnoughFood = population.ateEnough;
	report.m_ConsumedLiquor = population.consumedLiquor;
	report.m_LastTribute = population.reportedLastTribute;
	report.m_Power = inventory.RawCount(COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(3))));
	report.m_Jobs = qint32(std::min(quint32(std::numeric_limits<qint32>::max()), employment.jobs));
	report.m_JobsNonConst = qint32(std::min(quint32(std::numeric_limits<qint32>::max()), employment.nonConstructionJobs));
	report.m_JobsRepair = qint32(std::min(quint32(std::numeric_limits<qint32>::max()), employment.repairJobs));
	report.m_Unemployed = qint32(std::min(quint32(std::numeric_limits<qint32>::max()), employment.unemployed));
	report.m_Industries.clear();
	report.m_LegacyIndustryTechnology.clear();
	for(const auto& development : state.developments)
	{
		if(development.demolished || !development.completedLevels) continue;
		const auto runtime = INDUSTRYID(catalogue->RuntimeIndustryId(IndustryId(development.industryId)));
		// The 2016 industry record had levels and high/low quality, not the
		// later active-level counter. The shared old-format reader likewise
		// initializes LevelsInUse to the full level count.
		report.m_Industries.Add(runtime, AuCityIndustry(development.completedLevels, development.completedLevels));
		auto technology = report.m_LegacyIndustryTechnology.find(runtime);
		if(technology == report.m_LegacyIndustryTechnology.end())
			report.m_LegacyIndustryTechnology.insert(runtime, qMakePair(development.technology, development.technology));
		else
		{
			technology.value().first = std::min(technology.value().first, development.technology);
			technology.value().second = std::max(technology.value().second, development.technology);
		}
	}
	report.m_PatentRnD.clear();
	for(const auto& process : production.processes)
	{
		const auto* recipe = catalogue->ManufacturingProcess(ManufacturingProcessId(process.processId));
		if(recipe && process.enabled && IsProductionTechnology(recipe->commodity.value))
			++report.m_PatentRnD[recipe->name];
	}
	report.m_WhenCompiled = AuTime(now);
	QString distress;
	if(report.m_PopOrig && !report.m_HasBreathableAtmo && !report.HasPower())
		distress += tr("Powerless in harsh environment.<br>");
	if(population.change.suffocated) distress += tr("Citizens are suffocating.<br>");
	if(population.change.starved) distress += tr("Citizens are starving.<br>");
	if(!distress.isEmpty())
	{
		report.m_MessageType = AuMailMsg::MSG_CityDistressReport;
		report.m_DistressRpt += QStringLiteral("<b style=\"color: rgb(255, 255, 0);\">DISTRESS</b><div style=\"margin-left: 10px;\"><span style=\"color: rgb(255, 255, 0);\">")
			+ distress + QStringLiteral("</span></div>");
	}
	for(const auto& bounty : state.bounties)
		report.m_DistressRpt += tr("Capture Bounty! %1 posted. Reason: %2<br>")
			.arg(AuGlobal::FormatMoney(bounty.reward), bounty.reason.toHtmlEscaped());
	return true;
}

void DaLegacyCity::MailLastReport(const AuId& toId) const
{
	// Original DaCity::MailLastReport 0x2a23d0 is an explicit request.
	if(InArena() || InDesignPreview() || toId.IsNull()) return;
	AuCityReportHistory history;
	if(!ReportHistory(history) || history.IsEmpty()) return;
	const auto& report = history.Last();
	const auto& previous = history.Count() > 1 ? history.NextToLast() : report;
	MailReport(report, previous, toId);
}

bool DaLegacyCity::MailReport(const AuCityReport& report, const AuCityReport& previous,
	const AuId& toId, const QString& extraHtml, const QString& subject) const
{
	if(InArena() || InDesignPreview() || toId.IsNull()) return false;
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	const auto* system = world ? world->FindParentSolarSystem() : nullptr;
	const auto* galaxy = system ? system->FindParentGalaxy() : nullptr;
	if(!world || !system || !galaxy) return false;
	const auto& data = g_Au.LockDataForRead();
	const quint32 player = data.PlayerId(), confirmation = data.ConfirmationNumber();
	g_Au.UnlockData();
	AuPoint galaxyLocation;
	system->GetLocationRelativeToParent(galaxyLocation, galaxy);
	g_Au.SendMail(player, confirmation, Id(), Name(), toId, AuMailMsg::MSGTYPE(report.m_MessageType),
		subject.isEmpty() ? report.Title() : subject, report.ReportHtml(previous, true) + extraHtml,
		system->Id(), system->Name(), world->Id(), world->Name(),
		galaxy->Galaxy(), galaxyLocation);
	return true;
}

void DaLegacyCity::SendCityReport(const AuCityReport& report, const AuCityReport* previous)
{
	PostCivicJobs(report);
	// Called after publishing the transaction, so observers and mail rendering
	// may safely query this city and its environment again.
	const int satisfaction = report.m_PopOrig ? std::max(-20, std::min(20,
		int(report.m_SatOrig) + int(report.m_LegacyMoraleChange))) : 1;
	emit CityStatusChanged(Id(), report.AlertCondition(), qint8(satisfaction));
	if(!previous || previous->m_Industries != report.m_Industries)
		emit CityFacilitiesChanged(Id(), report.m_Industries);
	const bool fullReport = g_Au.MailReports();
	if(InArena() || InDesignPreview()
		|| (!fullReport && report.m_MessageType != AuMailMsg::MSG_CityDistressReport
			&& report.m_MessageType != AuMailMsg::MSG_CityFinalDecayReport)) return;
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	const auto* system = world ? world->FindParentSolarSystem() : nullptr;
	const auto* galaxy = system ? system->FindParentGalaxy() : nullptr;
	if(!world || !system || !galaxy) return;
	AuIdList places, recipients;
	places.append(Id());
	ChainOfPlaces(places);
	const auto& data = g_Au.LockDataForRead();
	const quint32 empire = report.EmpireId(0);
	const quint32 player = data.PlayerId(), confirmation = data.ConfirmationNumber();
	if(data.EmpireRole(empire) != EMPIRE_Pirate) data.EmpireDomainGovernors(empire, places, recipients);
	g_Au.UnlockData();
	if(recipients.isEmpty()) return;
	AuPoint galaxyLocation;
	system->GetLocationRelativeToParent(galaxyLocation, galaxy);
	g_Au.SendBulkMail(player, confirmation, Id(), Name(), recipients, AuMailMsg::MSGTYPE(report.m_MessageType),
		report.Title(), fullReport ? report.ReportHtml(previous ? *previous : report, true) : report.LegacyAlertMailHtml(), system->Id(), system->Name(),
		world->Id(), world->Name(), galaxy->Galaxy(), galaxyLocation);
}
