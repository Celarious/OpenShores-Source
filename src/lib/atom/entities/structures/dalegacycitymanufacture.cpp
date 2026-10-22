#include "dalegacycity.h"
#include "aulegacycityinfrastructure.h"
#include "aulegacycityinventory.h"
#include "aulegacycitypopulation.h"
#include "aulegacycityproduction.h"
#include "aulegacyfinancetransfer.h"
#include "augear.h"
#include "auglobal.h"
#include "auitem.h"
#include "auqueryenvironment.h"
#include "auzoneresource.h"
#include "daperson.h"
#include "daworldll.h"
#include "dgdata.h"
#include <QDateTime>
#include <algorithm>

using namespace AuLegacyCity;

bool DaLegacyCity::OperateIndustryProcess(DaPerson& person, quint32 developmentId,
	quint16 processSlot, bool fetch, bool run, QString& error)
{
	AuLegacyFinanceTransfer transfer(&person);
	error.clear();
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !catalogue || !world
		|| person.IsDead() || person.FindParentPlanetInEnv() != world)
	{ error = QStringLiteral("Move onto this Legacy city's world to operate its industry."); return false; }
	// Cached environment queries can enter city geometry. Resolve them before
	// the city write lock, including validation against the actual occupied site.
	const auto* actorEnvironment = person.LockEnvironmentQuery();
	const bool atSite = actorEnvironment->LegacyCityId() == Id()
		&& actorEnvironment->LegacyDevelopmentId() == developmentId;
	person.UnlockEnvironmentQuery();
	if(!atSite) { error = QStringLiteral("Stand inside this industry to fetch materials or start production."); return false; }
	State before; quint64 revision;
	{ QReadLocker lock(&m_CityLock); before = m_City; revision = m_CityRevision; }
	const auto original = std::find_if(before.developments.cbegin(), before.developments.cend(),
		[developmentId](const Development& development) { return development.id == developmentId; });
	if(original == before.developments.cend() || original->demolished || !original->completedLevels
		|| processSlot >= ManufacturingSlots(original->industryId, original->completedLevels))
	{ error = QStringLiteral("There is no completed industry process here."); return false; }
	ProductionState originalProduction;
	if(!DecodeProductionState(*catalogue, before.manufacturing, originalProduction, error)) return false;
	const auto originalProcess = std::find_if(originalProduction.processes.cbegin(), originalProduction.processes.cend(),
		[developmentId, processSlot](const ProductionProcess& process) {
			return process.developmentId == developmentId && process.slot == processSlot;
		});
	if(originalProcess == originalProduction.processes.cend())
	{ error = QStringLiteral("This process is still being initialized."); return false; }
	const quint32 actorEmpire = person.EmpireId();
	if(!originalProcess->enabled && actorEmpire != before.controllingEmpire)
	{ error = QStringLiteral("Only the city's empire may operate a process disabled for citizen work."); return false; }
	if(run && originalProcess->completeAt)
		return false; // A duplicate start is harmless; the reply carries the running snapshot.
	const bool ringworld = OnRingworld();
	AuGlobeLL cityLL;
	if(!world->LLOfLoc(cityLL, Location())) return false;
	AuZoneResource natural;
	world->QueryNaturalResources(natural, world->ResourceZone(cityLL.Longitude()));
	const quint8 sunlight = original->industryId == 66 ? 0 : natural.m_SunlightQ;
	const quint8 water = original->industryId == 34 || original->industryId == 51
		|| original->industryId == 62 || original->industryId == 89 ? 0 : natural.m_WaterInEnvironmentQ;
	if(world->Hydrographics() && world->AltitudeMSL(Location()) <= -32)
	{ natural.m_Atmosphere = 0; natural.m_Sunlight = 0; }
	else if(world->QuerySunlight(world->LocOfLL(original->location, original->altitudeMSL)) < .3f)
		natural.m_Sunlight = 0;
	QHash<quint16, AuGearPos> ready;
	if(run)
		for(const auto& component : originalProcess->components)
			if(component.commodityId != 3 && (component.effect == 2 || component.effect == 6
				|| (component.effect == 5 && !component.required)))
				ready.insert(component.commodityId,
					person.FindReadyItem(COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(component.commodityId)))));
	QVector<AuGearPos> wornGear;
	double salary = 0, salaryReward = 0;
	bool started = false;
	QWriteLocker lock(&m_CityLock);
	if(revision != m_CityRevision)
	{ error = QStringLiteral("The industry changed while its environment was being checked. Try again."); return false; }
	State state = m_City;
	ProductionState production; AccountingState accounting; AuItemHash inventory;
	if(!ReadCityInventory(state.inventory, *catalogue, inventory, error)
		|| !DecodeProductionState(*catalogue, state.manufacturing, production, error)
		|| !DecodeAccounting(state.accounting, accounting, error)) return false;
	TaxRates taxes = accounting.taxes;
	auto process = std::find_if(production.processes.begin(), production.processes.end(),
		[developmentId, processSlot](const ProductionProcess& item) {
			return item.developmentId == developmentId && item.slot == processSlot;
		});
	if(process == production.processes.end()) return false;
	const auto* recipe = catalogue->ManufacturingProcess(ManufacturingProcessId(process->processId));
	if(!recipe) return false;
	SetProductionTechnology(*catalogue, *process, TechnologyOfProduct(original->industryId,
		original->technology, recipe->commodity.value, state.developments));
	if(fetch || run)
	{
		quint32 money = 0;
		FetchProductionMaterials(*catalogue, *process, inventory, production, natural,
			accounting.accounts.government, money, ringworld, sunlight, water);
		if(money)
		{
			accounting.accounts.Debit(GovernmentExpense::Research, money);
			accounting.accounts.Credit(CitizenIncome::Salary, money, taxes);
		}
	}
	if(run && CanRunProduction(*process))
	{
		const bool hasPower = ringworld || inventory.RawCount(COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(3))));
		for(auto& component : process->components)
		{
			if(component.applied || (component.effect != 2 && component.effect != 6
				&& !(component.effect == 5 && !component.required))) continue;
			if(component.commodityId == 3) { if(hasPower) component.applied = 1; }
			else if(ready.value(component.commodityId).IsValid())
			{ component.applied = 1; wornGear.append(ready.value(component.commodityId)); }
			else if(UseProductionTool(*catalogue, component.commodityId, production, inventory)) component.applied = 1;
		}
		started = RunProduction(*process, QDateTime::currentMSecsSinceEpoch());
		if(started)
		{
			// DpBuilding::MakeOne 0x5a2e60: one crono salary, with the
			// treasury-independent reward channel covering a citizen-bank shortfall.
			// Original CVTSD2SS/DIVSS/MULSS/SUBSS at0x5a325f..0x5a32dd:
			// the ledger is double, but this individual wage is calculated in float.
			const float gross=qMin(1.0f,float(accounting.accounts.citizens));
			salaryReward=double(1.0f-gross);
			accounting.accounts.Debit(CitizenExpense::Salary,double(gross));
			const float incomeTax=(taxes.income/100.0f)*gross;
			accounting.accounts.Credit(GovernmentIncome::IncomeTax,double(incomeTax),taxes.hasCapital);
			salary=double(gross-incomeTax);
		}
	}
	if(!WriteCityInventory(inventory, state.inventory, error)
		|| !EncodeProductionState(*catalogue, production, state.manufacturing, error)
		|| !EncodeAccounting(accounting, state.accounting, error)) return false;
	QByteArray checked;
	if(!EncodeState(state, checked, error)) return false;
	const bool changed = state.inventory != before.inventory || state.manufacturing != before.manufacturing
		|| state.accounting != before.accounting;
	if(changed) { state.lastVisit = qMax(state.lastVisit, m_City.lastVisit); m_City = state; ++m_CityRevision; }
	lock.unlock();
	// Gear notifications, salary rewards and account notifications run after
	// publishing the city's transaction; they can query the environment again.
	for(const auto& gear : wornGear) person.UseGearItem(gear, IPS_Item);
	if(salaryReward > 0) person.Reward(salaryReward, REW_Salary);
	if(salary > 0) person.CreditAccount(salary);
	if(changed) SetModified();
	if(run && !started)
	{ error = QStringLiteral("Supply the required materials and technology before starting this process."); return false; }
	// A valid Fetch is accepted even when no additional materials are found.
	return fetch || changed || started;
}
