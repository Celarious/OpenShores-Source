#include "dalegacycity.h"
#include "aucityreport.h"
#include "auitem.h"
#include "aulegacycityinfrastructure.h"
#include "aulegacycitypopulationruntime.h"
#include "aulegacycityproduction.h"
#include "auglobal.h"
#include "auspecies.h"
#include "dagalaxy.h"
#include "dasector.h"
#include "dasolarsystem.h"
#include "daworldll.h"
#include "dgdata.h"
#include "dhdna.h"
#include <algorithm>
#include <limits>

using namespace AuLegacyCity;

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
		flag.m_IID = index == 0 ? IID_TownSquare : IID_BaseHeadquarters;
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
		report.m_DistressRpt = QStringLiteral("<b style=\"color: rgb(255, 255, 0);\">DISTRESS</b><div style=\"margin-left: 10px;\"><span style=\"color: rgb(255, 255, 0);\">")
			+ distress + QStringLiteral("</span></div>");
	}
	return true;
}

void DaLegacyCity::MailLastReport(const AuId& toId) const
{
	// Original DaCity::MailLastReport 0x2a23d0 is an explicit request. Ordinary
	// report cycles do not mail a status message to every governor.
	if(InArena() || InDesignPreview() || toId.IsNull()) return;
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	const auto* system = world ? world->FindParentSolarSystem() : nullptr;
	const auto* galaxy = system ? system->FindParentGalaxy() : nullptr;
	if(!world || !system || !galaxy) return;
	AuCityReportHistory history;
	if(!ReportHistory(history) || history.IsEmpty()) return;
	const auto& report = history.Last();
	const auto& previous = history.Count() > 1 ? history.NextToLast() : report;
	const auto& data = g_Au.LockDataForRead();
	const quint32 player = data.PlayerId(), confirmation = data.ConfirmationNumber();
	g_Au.UnlockData();
	AuPoint galaxyLocation;
	system->GetLocationRelativeToParent(galaxyLocation, galaxy);
	g_Au.SendMail(player, confirmation, Id(), Name(), toId, AuMailMsg::MSGTYPE(report.m_MessageType),
		report.Title(), report.ReportHtml(previous, true), system->Id(), system->Name(), world->Id(), world->Name(),
		galaxy->Galaxy(), galaxyLocation);
}

void DaLegacyCity::SendCityReport(const AuCityReport& report, const AuCityReport* previous)
{
	// Called after publishing the transaction, so observers and mail rendering
	// may safely query this city and its environment again.
	const int satisfaction = report.m_PopOrig ? std::max(-20, std::min(20,
		int(report.m_SatOrig) + int(report.m_LegacyMoraleChange))) : 1;
	emit CityStatusChanged(Id(), report.AlertCondition(), qint8(satisfaction));
	if(!previous || previous->m_Industries != report.m_Industries)
		emit CityFacilitiesChanged(Id(), report.m_Industries);
	if(InArena() || InDesignPreview()
		|| (report.m_MessageType != AuMailMsg::MSG_CityDistressReport
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
		report.Title(), report.LegacyAlertMailHtml(), system->Id(), system->Name(),
		world->Id(), world->Name(), galaxy->Galaxy(), galaxyLocation);
}
