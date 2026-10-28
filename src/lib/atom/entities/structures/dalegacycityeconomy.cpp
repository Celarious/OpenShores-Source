#include "dalegacycity.h"
#include "aulegacycityinfrastructure.h"
#include "aulegacycityinventory.h"
#include "aulegacycitypopulation.h"
#include "aulegacycitypopulationruntime.h"
#include "aulegacycityproduction.h"
#include "aulegacycitycommerce.h"
#include "aulegacycitizenconstruction.h"
#include "aulegacycitysupply.h"
#include "aulegacycityfarm.h"
#include "aulegacycityvehicles.h"
#include "aulegacyconstructionmaterials.h"
#include "aucityreport.h"
#include "aucommpacket.h"
#include "audice.h"
#include "audnabase.h"
#include "auether.h"
#include "auglobal.h"
#include "augear.h"
#include "auitem.h"
#include "auqueryenvironment.h"
#include "auterraininfo.h"
#include "auzoneresource.h"
#include "daatomlist.h"
#include "daitem.h"
#include "danpc.h"
#include "daperson.h"
#include "dasolarsystem.h"
#include "daworldll.h"
#include "dgdata.h"
#include "dpstationsurgery.h"
#include <QSet>
#include <QDateTime>
#include <algorithm>
#include <memory>

using namespace AuLegacyCity;

namespace
{
bool AtHospitalMedic(const DaCreature& doctor, const AuStation& station, quint16 stationId)
{
	if(!doctor.IsNormal() || doctor.IsBodyDyingOrDead() || !doctor.IsMedicStationPose()
		|| doctor.StationId() != stationId) return false;
	AuStation expected(station);
	expected.OffsetOpM(doctor.ConsolePoseOffset(expected.Type(), expected.OccPos(),
		doctor.CGHeightStanding(), doctor.TorsoThickness(), expected.SeatIndex()));
	return doctor.Location().Distance2(expected.OpM().Translation()) < 4;
}

quint16 HospitalCommodity(int procedure)
{
	switch(procedure)
	{
	case DpStationSurgery::CI_Heal: return 73;
	case DpStationSurgery::CI_Antidote: return 119;
	case DpStationSurgery::CI_Antitoxin: return 120;
	case DpStationSurgery::CI_Decontaminate: return 83;
	default: return 0;
	}
}
}

void DaLegacyCity::TockHospitalPatients()
{
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || IsDead()) return;
	DaWorldLL* world = dynamic_cast<DaWorldLL*>(Parent());
	AuEther* ether = Ether();
	if(!world || !ether) return;
	const State state = Snapshot();
	DaAtomList occupants;
	world->ListNormalSubatoms(occupants);
	QList<DaNpc*> medics;
	for(DaAtom* atom : occupants)
	{
		DaNpc* npc = dynamic_cast<DaNpc*>(atom);
		if(npc && npc->IsNpcWorker() && npc->Order() == ORDER_Stay && npc->OrderTarget() == Id()) medics.append(npc);
	}
	QSet<DaNpc*> needed;
	const bool powered = StationsPowered();
	const DaAtomList patients(occupants);
	for(DaAtom* atom : patients)
	{
		if(!powered) break;
		DaPerson* patient = dynamic_cast<DaPerson*>(atom);
		if(!patient || !patient->IsNormal() || !patient->IsOnLine() || !patient->IsPatientStationPose()) continue;
		quint32 developmentId = 0;
		quint16 medicStationId = 0;
		AuStation station;
		if(!HospitalPatient(*patient, developmentId, station, medicStationId)) continue;
		DaCreature* doctor = nullptr;
		for(DaAtom* candidate : occupants)
		{
			DaCreature* creature = dynamic_cast<DaCreature*>(candidate);
			if(creature && AtHospitalMedic(*creature, station, medicStationId))
			{ doctor = creature; break; }
		}
		if(!doctor)
		{
			std::unique_ptr<AuDNABase> dna;
			if(!ReadSpeciesDNA(state.citizenSpecies, dna)) continue;
			DaNpc* civilian = new DaNpc(ROLE_Worker, *dna,
				AuDNABase::CHROMATID(g_Au.Dice().Roll(1, 2, -1)),
				HANDEDNESS(g_Au.Dice().Roll(1, 2, -1)), g_Au.Dice().RandomPersonName());
			civilian->ConnectSignals();
			civilian->Create();
			station.OffsetOpM(civilian->ConsolePoseOffset(station.Type(), station.OccPos(),
				civilian->CGHeightStanding(), civilian->TorsoThickness(), station.SeatIndex()));
			civilian->SetPosAndOrient(AuAtomPos(world->Id(), station.OpM()));
			ether->AddAtom(civilian);
			civilian->InitTraining();
			civilian->SetHomeWorld(world);
			civilian->SetEmpireId(state.controllingEmpire);
			civilian->ResetHitPoints();
			civilian->ResetStamina();
			civilian->ResetHunger();
			civilian->SitAtConsole(medicStationId, station.Type(), station.OccPos(), station.SeatIndex(), medicStationId);
			// City tock owns treatment; the worker keeps this station without
			// requiring a modern building/capsule parent or ship AI orders.
			civilian->SetOrder(ORDER_Stay, world->Id(), AuPoint());
			// SetOrder's fallback Stay always targets ParentId. Append the
			// service order so its city target survives save/load and cleanup.
			civilian->AmendOrder(ORDER_Stay, Id(), AuPoint(), medicStationId);
			civilian->InvalidateEnvironment();
			civilian->SetInScene();
			g_Au.AtomAdded(civilian->Id());
			civilian->SetNormal();
			civilian->TickTock();
			medics.append(civilian);
			occupants.append(civilian);
			doctor = civilian;
		}
		DaNpc* civilian = dynamic_cast<DaNpc*>(doctor);
		if(civilian && medics.contains(civilian))
		{
			needed.insert(civilian);
			TreatHospitalPatient(*civilian, *patient);
		}
	}
	// Includes patients who stood up and stations whose site was demolished.
	for(DaNpc* civilian : medics)
		if(!needed.contains(civilian)) civilian->Kill();
}

DaCreature* DaLegacyCity::FindPatient(const DaCreature& doctor) const
{
	DaWorldLL* world = dynamic_cast<DaWorldLL*>(Parent());
	if(!world || doctor.ParentId() != world->Id() || !doctor.IsMedicStationPose() || !StationsPowered()) return nullptr;
	DaAtomList occupants;
	world->ListNormalSubatoms(occupants);
	for(DaAtom* atom : occupants)
	{
		DaCreature* patient = dynamic_cast<DaCreature*>(atom);
		if(!patient) continue;
		quint32 developmentId = 0;
		quint16 medicStationId = 0;
		AuStation medic;
		if(HospitalPatient(*patient, developmentId, medic, medicStationId)
			&& AtHospitalMedic(doctor, medic, medicStationId)) return patient;
	}
	return nullptr;
}

void DaLegacyCity::TreatHospitalPatient(DaCreature& doctor, DaCreature& patient, int procedure,
	const AuCondition* selectedCondition)
{
	// StationsPowered validates the active catalogue and every installed
	// commodity descriptor before it reads the city inventory.
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !AuLegacyItemTechnologyActive() || !StationsPowered()) return;
	const AuLegacyCityCatalogue* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue) return;
	const bool automatic = procedure < 0;
	AuConditionList conditions;
	if(automatic) patient.CloneConditions(conditions);
	std::unique_ptr<AuCondition> condition(selectedCondition ? selectedCondition->Clone() : nullptr);
	std::unique_ptr<AuItem> treatment;
	m_CityLock.lockForWrite();
	State changed = m_City;
	AuItemHash inventory;
	QString error;
	if(!ReadCityInventory(changed.inventory, *catalogue, inventory, error))
	{ m_CityLock.unlock(); conditions.Clear(); return; }
	if(automatic)
	{
		if(patient.IsBodyDyingOrDead()) procedure = DpStationSurgery::CI_Defibrillate;
		else if(patient.CurrentHitPoints() < patient.MaxHitPoints() && !conditions.Condition(AuCondition::Bandage)
			&& inventory.FindBest(COMMODITYID(catalogue->RuntimeCommodityId(
				CommodityId(HospitalCommodity(DpStationSurgery::CI_Heal))))) != inventory.end())
			procedure = DpStationSurgery::CI_Heal;
		for(const AuCondition* candidate : conditions)
		{
			if(procedure >= 0) break;
			int proposed = -1;
			switch(candidate->CType())
			{
			case AuCondition::Poison: proposed = DpStationSurgery::CI_Antidote; break;
			case AuCondition::Disease: proposed = DpStationSurgery::CI_Antitoxin; break;
			case AuCondition::Acid: case AuCondition::Fire: proposed = DpStationSurgery::CI_Decontaminate; break;
			case AuCondition::Paralysis: proposed = DpStationSurgery::CI_PhysicalTherapy; break;
			case AuCondition::ImplantDNA: case AuCondition::ImplantSpecies: proposed = DpStationSurgery::CI_Surgery; break;
			default: break;
			}
			if(proposed < 0) continue;
			const quint16 required = HospitalCommodity(proposed);
			if(required)
			{
				const auto available = inventory.FindBest(COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(required))));
				if(available == inventory.end()) continue;
				// Immunization handles disease over time; avoid repeated doses.
				const AuConAntitoxin* immunization = dynamic_cast<const AuConAntitoxin*>(conditions.Condition(AuCondition::Antitoxin));
				if(required == 120 && immunization && immunization->EffectiveQuality() >= available->second->Quality()) continue;
			}
			procedure = proposed;
			condition.reset(candidate->Clone());
			break;
		}
	}
	const quint16 commodity = HospitalCommodity(procedure);
	AuItemHash::iterator item = commodity
		? inventory.FindBest(COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(commodity)))) : inventory.end();
	bool consumed = false;
	if(item != inventory.end())
	{
		treatment.reset(item->second->Clone());
		consumed = commodity == 83 || !treatment->TestQuality();
		if(consumed)
		{
			item->second->Consume(1);
			if(!item->second->Count()) inventory.Erase(item);
			if(!WriteCityInventory(inventory, changed.inventory, error))
			{ m_CityLock.unlock(); conditions.Clear(); return; }
			m_City = changed;
			++m_CityRevision;
		}
	}
	m_CityLock.unlock();
	conditions.Clear();
	if(consumed) SetModified();
	if(!automatic && commodity && !treatment)
		doctor.Thought(tr("Hospital's pharmacy has no %1.")
			.arg(g_Au.CommodityName(COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(commodity))))));
	if(consumed)
	{
		AuCommThought message(tr("%1 consumed from hospital's pharmacy.").arg(treatment->SayName()),
			patient.Id(), patient.Name());
		g_Au.SendAtomChat(message);
	}
	if(commodity && !treatment) return;
	switch(procedure)
	{
	case DpStationSurgery::CI_Defibrillate:
		DpStationSurgery::ApplyDefibrillate(&doctor, &patient, patient.CurrentHitPoints() <= 0); break;
	case DpStationSurgery::CI_Heal: DpStationSurgery::ApplyHeal(&doctor, &patient, *treatment); break;
	case DpStationSurgery::CI_Antidote: DpStationSurgery::ApplyAntidote(&doctor, &patient, condition.get(), *treatment); break;
	case DpStationSurgery::CI_Antitoxin: DpStationSurgery::ApplyAntitoxin(&doctor, &patient, *treatment); break;
	case DpStationSurgery::CI_Decontaminate:
		if(condition && condition->CType() == AuCondition::Fire)
			DpStationSurgery::ApplyExtinguish(&doctor, &patient, condition.get(), *treatment);
		else DpStationSurgery::ApplyDecontaminate(&doctor, &patient, condition.get(), *treatment);
		break;
	case DpStationSurgery::CI_PhysicalTherapy: DpStationSurgery::DoPhysicalTherapy(&doctor, &patient, condition.get()); break;
	case DpStationSurgery::CI_Surgery: DpStationSurgery::DoSurgery(&doctor, &patient, condition.get()); break;
	}
}

namespace
{
struct UnlockWrite
{
	QReadWriteLock* lock;
	~UnlockWrite() { if(lock) lock->unlock(); }
	void Release() { if(lock) lock->unlock(); lock = nullptr; }
};

TaxRates CityTaxes(quint32 empire, bool bank)
{
	TaxRates taxes;
	const auto& data = g_Au.LockDataForRead();
	AuId capital;
	taxes.hasCapital = data.EmpireCapital(empire, capital) && !capital.IsNull();
	if(bank && taxes.hasCapital)
	{
		// Both empire getters and the original city cache use percentages.
		taxes.income = data.EmpireIncomeTax(empire);
		taxes.sales = data.EmpireSalesTax(empire);
		taxes.subsidy = data.EmpireSubsidyTax(empire);
	}
	g_Au.UnlockData();
	return taxes;
}

bool HasBank(const State& state)
{
	for(const auto& dev : state.developments)
		if(dev.industryId == 100 && dev.completedLevels && !dev.demolished) return true;
	return false;
}

float Loyalty(const State& state)
{
	return state.population <= qint64(state.loyalPopulation) ? 1.f
		: state.population ? float(state.loyalPopulation) / state.population : 0.f;
}

float Motivation(const State& state, const TaxRates& taxes)
{
	// DaCity::MotivationToWork 0xf8460 uses income tax and occupation.
	float motivation = taxes.income >= 100 ? 0 : 1.f - taxes.income / 100;
	if(state.allegianceEmpire != state.controllingEmpire) motivation *= 1.f - Loyalty(state);
	return motivation;
}

bool Motivated(float motivation)
{
	return motivation >= 1 || g_Au.Dice().Roll(1, 100) <= motivation * 100;
}

bool WorkerAvailable(const AuLegacyCityCatalogue& catalogue, const Development& development, quint32 enabled)
{
	const auto jobs = DevelopmentJobs(catalogue, development, enabled);
	return development.employees && jobs && (development.employees == jobs
		|| g_Au.Dice().Roll(1, jobs) <= qint32(development.employees));
}

struct ProcessEnvironment
{
	AuZoneResource procurement, output;
	quint8 sunlightSubstitution = 0, waterSubstitution = 0;
	bool dark = false;
};

ProcessEnvironment QueryProcessEnvironment(DaWorldLL& world, const AuPoint& cityLocation,
	const Development& development)
{
	ProcessEnvironment environment;
	AuGlobeLL cityLL;
	if(!world.LLOfLoc(cityLL, cityLocation)) return environment;
	world.QueryNaturalResources(environment.procurement, world.ResourceZone(cityLL.Longitude()));
	world.QueryNaturalResources(environment.output, world.ResourceZone(development.location.Longitude()));
	environment.dark = world.QuerySunlight(world.LocOfLL(development.location, development.altitudeMSL)) < .3f;
	if(development.industryId == 2)
	{
		// DpGlobeRoad::IsDark 0x653810 checks the end, then the start.
		AuMatrix4x4 placement;
		world.BuildingConstructionTransform(world.LocOfLL(development.location, development.altitudeMSL),
			development.facingRadians, nullptr, AuPoint(), placement);
		const double halfLength = development.roadLengthMeters * FeetPerMeter() / 2;
		AuGlobeLLF start, end;
		if(world.LLOfLoc(end, placement.Project(AuPoint(0, halfLength, 0)))
			&& world.LLOfLoc(start, placement.Project(AuPoint(0, -halfLength, 0))))
			environment.dark = world.QuerySunlight(world.LocOfLL(end, development.roadEndAltitudeMSL)) < .3f
				|| world.QuerySunlight(world.LocOfLL(start, development.roadStartAltitudeMSL)) < .3f;
	}
	environment.sunlightSubstitution = development.industryId == 66 ? 0 : environment.procurement.m_SunlightQ;
	environment.waterSubstitution = development.industryId == 34 || development.industryId == 51
		|| development.industryId == 62 || development.industryId == 89 ? 0 : environment.procurement.m_WaterInEnvironmentQ;
	if(world.Hydrographics() && world.AltitudeMSL(cityLocation) <= -32)
	{
		environment.procurement.m_Atmosphere = 0;
		environment.procurement.m_Sunlight = 0;
	}
	else if(environment.dark) environment.procurement.m_Sunlight = 0;
	return environment;
}

void FetchCityProcess(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process,
	ProductionState& production, AuItemHash& inventory, AccountingState& accounting,
	const TaxRates& taxes, const ProcessEnvironment& environment, bool ringworld)
{
	quint32 money = 0;
	FetchProductionMaterials(catalogue, process, inventory, production, environment.procurement,
		accounting.accounts.government, money, ringworld, environment.sunlightSubstitution, environment.waterSubstitution);
	if(money)
	{
		accounting.accounts.Debit(GovernmentExpense::Research, money);
		accounting.accounts.Credit(CitizenIncome::Salary, money, taxes);
	}
}

bool ReadEconomy(const AuLegacyCityCatalogue& catalogue, const State& state,
	AuItemHash& inventory, ProductionState& production, AccountingState& accounting, QString& error)
{
	return ReadCityInventory(state.inventory, catalogue, inventory, error)
		&& DecodeProductionState(catalogue, state.manufacturing, production, error)
		&& DecodeAccounting(state.accounting, accounting, error);
}

bool WriteEconomy(const AuLegacyCityCatalogue& catalogue, State& state,
	const AuItemHash& inventory, const ProductionState& production, const AccountingState& accounting, QString& error)
{
	QByteArray checked;
	return WriteCityInventory(inventory, state.inventory, error)
		&& EncodeProductionState(catalogue, production, state.manufacturing, error)
		&& EncodeAccounting(accounting, state.accounting, error)
		&& EncodeState(state, checked, error);
}

bool EconomyChanged(const State& before, const State& after)
{
	if(before.inventory != after.inventory || before.manufacturing != after.manufacturing
		|| before.accounting != after.accounting || before.reportHistory != after.reportHistory
		|| before.population != after.population || before.satisfaction != after.satisfaction
		|| before.loyalPopulation != after.loyalPopulation || before.capacitor != after.capacitor
		|| before.lastEmployment != after.lastEmployment || before.lastPopulationCycle != after.lastPopulationCycle
		|| before.citizenSpecies != after.citizenSpecies || before.developments.size() != after.developments.size()) return true;
	for(int index = 0; index < before.developments.size(); ++index)
		if(before.developments[index].technology != after.developments[index].technology
			|| before.developments[index].employees != after.developments[index].employees) return true;
	return false;
}

void RefundProcess(const AuLegacyCityCatalogue& catalogue, const ProductionProcess& process,
	const State& state, AuItemHash& inventory, AccountingState& accounting, const TaxRates& taxes,
	quint8 orbitZone, bool breathable)
{
	quint32 money = 0;
	const auto materials = RefundProductionMaterials(catalogue, process, money);
	const auto capacity = CityStorage(catalogue, state.developments, orbitZone);
	const AuSpecies species(state.citizenSpecies);
	const quint8 ecoRole = species.IsNull() ? 0 : quint8(species.EcoRole());
	for(const auto& material : materials)
	{
		const auto runtime = COMMODITYID(catalogue.RuntimeCommodityId(CommodityId(material.commodityId)));
		std::unique_ptr<AuItem> item(AuItem::Create(runtime, material.quality, state.controllingEmpire, material.quantity));
		if(item && item->SetTechLevel(material.technology))
		{
			item->SetCount(TestFitCityCommodity(catalogue, inventory, *item, capacity, breathable, ecoRole));
			if(item->Count()) AddCityCommodity(catalogue, inventory, *item, capacity, breathable, ecoRole);
		}
	}
	accounting.accounts.Credit(GovernmentIncome::Research, money, taxes.hasCapital);
}
}

bool DaLegacyCity::SetIndustryProcess(DaPerson& person, quint32 developmentId, quint16 processSlot,
	quint16 processId, quint8 minimumTechnology, bool enabled, QString& error)
{
	error.clear();
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !catalogue
		|| !person.EmpireId() || person.EmpireId() != EmpireId())
	{ error = QStringLiteral("Only citizens of the controlling empire may configure this industry."); return false; }
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	if(!world) { error = QStringLiteral("The industry has no parent world."); return false; }
	const auto zone = quint8(world->OrbitZone());
	const bool openAir = world->HasBreathableAtmosphere()
		&& !(world->Hydrographics() && world->AltitudeMSL(Location()) <= -32);
	QWriteLocker lock(&m_CityLock);
	State state = m_City;
	if(person.EmpireId() != state.controllingEmpire)
	{ error = QStringLiteral("The city's government changed. Refresh the industry window."); return false; }
	auto dev = std::find_if(state.developments.begin(), state.developments.end(),
		[developmentId](const Development& item) { return item.id == developmentId; });
	const auto* recipe = catalogue->ManufacturingProcess(ManufacturingProcessId(processId));
	if(dev == state.developments.end() || dev->demolished || !recipe || recipe->industry.value != dev->industryId
		|| processSlot >= ManufacturingSlots(dev->industryId, dev->completedLevels)
		|| minimumTechnology < recipe->minimumTechnology || minimumTechnology > 31)
	{ error = QStringLiteral("Invalid legacy industry, recipe, process slot or technology level."); return false; }
	AuItemHash inventory; ProductionState production; AccountingState accounting;
	if(!ReadEconomy(*catalogue, state, inventory, production, accounting, error)) return false;
	const auto taxes = accounting.taxes;
	auto process = std::find_if(production.processes.begin(), production.processes.end(),
		[developmentId, processSlot](const ProductionProcess& item) { return item.developmentId == developmentId && item.slot == processSlot; });
	if(process == production.processes.end())
	{
		ProductionProcess added; added.developmentId = developmentId; added.slot = processSlot;
		production.processes.append(added); process = production.processes.end() - 1;
	}
	const bool replace = process->processId != processId
		|| (process->minimumTechnology != minimumTechnology && !IsProductionTechnology(recipe->commodity.value));
	if(replace)
	{
		if(process->processId) RefundProcess(*catalogue, *process, state, inventory, accounting, taxes, zone, openAir);
		if(!InitializeProductionProcess(*catalogue, *process, processId,
			TechnologyOfProduct(dev->industryId, dev->technology, recipe->commodity.value, state.developments)))
		{ error = QStringLiteral("Unable to initialize the original manufacturing recipe."); return false; }
	}
	process->enabled = enabled;
	if(!IsProductionTechnology(recipe->commodity.value)) process->minimumTechnology = minimumTechnology;
	state.lastEmployment = 0;
	if(!WriteEconomy(*catalogue, state, inventory, production, accounting, error)) return false;
	state.lastVisit = qMax(state.lastVisit, m_City.lastVisit);
	m_City = state; ++m_CityRevision;
	lock.unlock(); SetModified();
	return true;
}

void DaLegacyCity::ImportCityGoods(const QVector<CityConnection>& connections)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	if(!catalogue || !world) return;
	const quint8 zone = quint8(world->OrbitZone());
	// Original Tock imports each service in this order; each connection can
	// move citizen wholesale funds even when it has no eligible goods.
	for(const quint8 industry : {101, 103, 104, 105})
		for(const auto& connection : connections)
		{
			auto* exporter = connection.city;
			if(!exporter || exporter == this || exporter->IsDead()) continue;
			auto* exporterWorld = dynamic_cast<DaWorldLL*>(exporter->Parent());
			if(!exporterWorld) continue;
			const bool flora = exporterWorld->CanHaveFlora(), fauna = exporterWorld->CanHaveFauna();
			QWriteLocker lock(&m_CityLock);
			quint32 levels = 0;
			for(const auto& dev : m_City.developments)
				if(!dev.demolished && dev.industryId == industry) levels += dev.completedLevels;
			const auto base = IndustryImportPercentage(industry, levels);
			if(!base || !exporter->m_CityLock.tryLockForWrite()) continue;
			UnlockWrite exporterLock{&exporter->m_CityLock};
			const int percent = int(ConnectionImportPercentage(base, connection.flags));
			if(!percent) continue;
			State receiverState = m_City, exporterState = exporter->m_City;
			AuItemHash receiverInventory, exporterInventory;
			AccountingState receiverAccounts, exporterAccounts; QString error;
			if(!ReadCityInventory(receiverState.inventory, *catalogue, receiverInventory, error)
				|| !ReadCityInventory(exporterState.inventory, *catalogue, exporterInventory, error)
				|| !DecodeAccounting(receiverState.accounting, receiverAccounts, error)
				|| !DecodeAccounting(exporterState.accounting, exporterAccounts, error))
			{ qWarning() << "Legacy city commerce:" << error; continue; }
			const auto capacity = CityStorage(*catalogue, receiverState.developments, zone);
			const auto stance = exporter->PoliticalStanceMakeContact(exporterState.controllingEmpire, this,
				receiverState.controllingEmpire, &exporterState.name, &receiverState.name);
			const auto& data = g_Au.LockDataForRead();
			if(stance != POL_Enemy)
			{
				const double amount = receiverAccounts.accounts.citizens * double(percent) / 100;
				receiverAccounts.accounts.Debit(CitizenExpense::Wholesale, amount);
				exporterAccounts.accounts.Credit(CitizenIncome::Wholesale, amount, TaxRates());
			}
			switch(industry)
			{
			case 101:
				// AuData13 0x13ad20 refuses an unknown exporter empire. The
				// modern helper instead permits neutral exports in that case.
				if(data.HasEmpire(exporterState.controllingEmpire))
					data.ExportBrokerItems(exporterState.population, exporterState.controllingEmpire, exporterInventory,
						receiverState.controllingEmpire, receiverInventory, percent, capacity.cargo, flora, fauna);
				break;
			case 103: data.ExportGroceryItems(exporterState.population, exporterState.controllingEmpire, exporterInventory,
				receiverState.controllingEmpire, receiverInventory, percent, capacity.cargo, flora, fauna); break;
			case 104: data.ExportRetailItems(exporterState.population, exporterState.controllingEmpire, exporterInventory,
				receiverState.controllingEmpire, receiverInventory, percent, capacity.cargo, flora, fauna); break;
			case 105: data.ExportCantinaItems(exporterState.population, exporterState.controllingEmpire, exporterInventory,
				receiverState.controllingEmpire, receiverInventory, percent, capacity.cargo, flora, fauna); break;
			}
			g_Au.UnlockData();
			QByteArray receiverBytes, exporterBytes;
			if(!WriteCityInventory(receiverInventory, receiverState.inventory, error)
				|| !WriteCityInventory(exporterInventory, exporterState.inventory, error)
				|| !EncodeAccounting(receiverAccounts, receiverState.accounting, error)
				|| !EncodeAccounting(exporterAccounts, exporterState.accounting, error)
				|| !EncodeState(receiverState, receiverBytes, error) || !EncodeState(exporterState, exporterBytes, error))
			{ qWarning() << "Legacy city commerce:" << error; continue; }
			const bool receiverChanged = EconomyChanged(m_City, receiverState);
			const bool exporterChanged = EconomyChanged(exporter->m_City, exporterState);
			receiverState.lastVisit = qMax(receiverState.lastVisit, m_City.lastVisit);
			exporterState.lastVisit = qMax(exporterState.lastVisit, exporter->m_City.lastVisit);
			if(receiverChanged) { m_City = receiverState; ++m_CityRevision; }
			if(exporterChanged) { exporter->m_City = exporterState; ++exporter->m_CityRevision; }
			exporterLock.Release(); lock.unlock();
			if(receiverChanged) SetModified();
			if(exporterChanged) exporter->SetModified();
		}
}

void DaLegacyCity::TockEconomy(qint64 previous, qint64 now)
{
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!world || !catalogue) return;
	State before; quint64 revision;
	{ QReadLocker lock(&m_CityLock); before = m_City; revision = m_CityRevision; }
	const auto connections = QueryCityConnections(*this, *world, before);
	if(CommerceCycleDue(previous, now, Id().GetID()))
	{
		ImportCityGoods(connections);
		QReadLocker lock(&m_CityLock); before = m_City; revision = m_CityRevision;
	}
	const bool ringworld = OnRingworld();
	if(before.controllingEmpire && previous > 0 && now / 60000 > previous / 60000)
	{
		// DaCity::Tock 0x2d3100 requires an observatory and powered radar.
		// Original radar is built into airport111 and military systems11/12.
		const bool observatory = std::any_of(before.developments.cbegin(), before.developments.cend(),
			[](const Development& dev) { return dev.industryId == 127 && dev.completedLevels && !dev.demolished; });
		const bool radar = std::any_of(before.developments.cbegin(), before.developments.cend(),
			[](const Development& dev) { return (dev.industryId == 111 || dev.industryId == 11 || dev.industryId == 12)
				&& dev.completedLevels && !dev.demolished; });
		auto* system = FindParentSolarSystem();
		if(observatory && radar && system && !system->IsScannedBy(before.controllingEmpire))
		{
			AuItemHash stock; QString error;
			const auto electricity = COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(3)));
			if(ringworld || (ReadCityInventory(before.inventory, *catalogue, stock, error) && stock.RawCount(electricity)))
				system->ScannedBy(before.controllingEmpire, system->GenHab(), system->GenSeed());
		}
	}
	const bool breathable = world->HasBreathableAtmosphere();
	const bool needsEnclosure = !breathable || (world->Hydrographics() && world->AltitudeMSL(Location()) <= -32);
	const quint8 zone = quint8(world->OrbitZone());
	const bool populationDue = before.lastPopulationCycle && now >= before.lastPopulationCycle
		&& double((now - before.lastPopulationCycle) / 1000) >= 771.4285714285714;
	const auto empireTaxes = CityTaxes(before.controllingEmpire, HasBank(before));
	PopulationRuntimeFacts populationFacts;
	QList<quint16> cycleVehicles;
	if(populationDue)
	{
		// CityCycle 0x2749d0 refreshes available vehicles from the world
		// before taking the report snapshot; temporary registrations expire.
		QList<COMMODITYID> vehicles;
		world->TallyVehicles(Id(), before.controllingEmpire, vehicles);
		for(const auto vehicle : vehicles)
		{
			CommodityId historical;
			if(catalogue->HistoricalCommodityId(quint16(vehicle), historical) && IsProductionUnit(historical.value))
				cycleVehicles.append(historical.value);
		}
		populationFacts = GatherPopulationFacts(*this, *world, before);
		populationFacts.connectionsKnown = true;
		populationFacts.context.isolated = connections.isEmpty();
		populationFacts.attackKnown = true;
		populationFacts.context.underAttack = before.lastAttack
			&& double((now - before.lastAttack) / 1000) <= 771.4285714285714;
	}
	std::unique_ptr<AuCityReport> cycleReport;
	std::unique_ptr<AuCityReport> previousReport;
	bool reportCompleted = false;
	bool revoltRequested = false;
	bool cycleInterrupted = false, revoltCaptured = false;
	quint32 revoltOldEmpire = 0, revoltNewEmpire = 0;
	bool citizenSpeciesChanged = false;
	quint32 tributeDestination = 0;
	float tributeAmount = 0;
	if(populationDue)
		cycleReport.reset(new AuCityReport(GatherCityReport(before, populationFacts)));
	const auto cityLocation = Location();
	QHash<quint32, ProcessEnvironment> environments;
	QHash<quint32, AuZoneResource> vehicleResources;
	QHash<quint32, CitizenConstructionFacts> constructionFacts;
	QHash<quint32, DevelopmentSupplyFacts> supplyFacts;
	QHash<quint32, quint16> defaultRecipes;
	ProductionState existing; QString existingError;
	if(!DecodeProductionState(*catalogue, before.manufacturing, existing, existingError))
	{ qWarning() << "Legacy city economy:" << Id() << existingError; return; }
	FarmProductionBatch farmProduction;
	if(!PrepareFarmProduction(*this, *world, before, existing, now, farmProduction, existingError))
	{ qWarning() << "Legacy city livestock:" << Id() << existingError; return; }
	for(const auto& dev : before.developments)
	{
		if(dev.demolished) continue;
		environments.insert(dev.id, QueryProcessEnvironment(*world, cityLocation, dev));
		vehicleResources.insert(dev.id, environments.value(dev.id).output);
		if(!dev.components.isEmpty()) constructionFacts.insert(dev.id, GatherCitizenConstructionFacts(before, dev, now));
		if(ManufacturingSlots(dev.industryId, dev.completedLevels))
			supplyFacts.insert(dev.id, GatherDevelopmentSupplyFacts(before, dev, now));
		bool missing = !dev.components.isEmpty();
		for(quint16 slot = 0; slot < ManufacturingSlots(dev.industryId, dev.completedLevels); ++slot)
			if(std::none_of(existing.processes.cbegin(), existing.processes.cend(), [&dev, slot](const ProductionProcess& process) {
				return process.developmentId == dev.id && process.slot == slot; })) { missing = true; break; }
		if(!missing) continue;
		AuMatrix4x4 placement;
		const auto center = world->LocOfLL(dev.location, dev.altitudeMSL);
		world->BuildingConstructionTransform(center, dev.facingRadians, nullptr, AuPoint(), placement);
		const auto* constructionRecipe = catalogue->ConstructionProcessForIndustry(IndustryId(dev.industryId));
		const double radius = constructionRecipe ? constructionRecipe->buildRadius * FeetPerMeter() : 0;
		defaultRecipes.insert(dev.id, DefaultProductionRecipe(*catalogue, dev.industryId, [&](quint16 historical) {
			return world->TestRockResource(placement, center, AuPoint(radius, radius, radius),
				COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(historical))));
		}));
	}
	VehicleProductionBatch vehicleProduction;
	if(!PrepareVehicleProduction(*this, *world, before, existing, vehicleResources, now, vehicleProduction, existingError))
	{ qWarning() << "Legacy city vehicles:" << Id() << existingError; return; }
	AuLegacyConstructionMaterials::Batch constructionMaterials;
	bool constructionChanged = false, physicalChanged = false;
	PowerImportBatch powerImports(*this);
	QWriteLocker lock(&m_CityLock);
	if(revision != m_CityRevision) return;
	State state = m_City;
	AuItemHash inventory; ProductionState production; AccountingState accounting; QString error;
	if(!ReadEconomy(*catalogue, state, inventory, production, accounting, error))
	{ qWarning() << "Legacy city economy:" << Id() << error; return; }
	// Treasury availability follows the authoritative capital designation as
	// soon as it arrives, including before the first population cycle. Tax
	// percentages retain their original population-cycle refresh below.
	accounting.taxes.hasCapital = empireTaxes.hasCapital;
	TaxRates taxes = accounting.taxes;
	bool jobsChanged = false;
	QHash<quint32, int> developmentIndexes;
	for(int index = 0; index < state.developments.size(); ++index) developmentIndexes.insert(state.developments[index].id, index);
	for(auto process = production.processes.begin(); process != production.processes.end();)
	{
		const int index = developmentIndexes.value(process->developmentId, -1);
		if(index < 0 || state.developments[index].demolished
			|| process->slot >= ManufacturingSlots(state.developments[index].industryId, state.developments[index].completedLevels))
		{
			// Removing a site/level destroys its processes. Only an explicit
			// recipe or minimum-TL change refunds applied production materials.
			process = production.processes.erase(process); jobsChanged = true;
		}
		else ++process;
	}
	const auto ensureProcesses = [&](const Development& dev) {
		if(dev.demolished) return;
		for(quint16 slot = 0; slot < ManufacturingSlots(dev.industryId, dev.completedLevels); ++slot)
		{
			const auto exists = std::find_if(production.processes.cbegin(), production.processes.cend(),
				[&dev, slot](const ProductionProcess& item) { return item.developmentId == dev.id && item.slot == slot; });
			if(exists != production.processes.cend()) continue;
			quint16 recipeId = defaultRecipes.value(dev.id);
			const auto first = std::find_if(production.processes.cbegin(), production.processes.cend(), [&dev](const ProductionProcess& process) {
				return process.developmentId == dev.id && process.slot == 0; });
			if(dev.selectedManufacturingRecipe) recipeId = dev.selectedManufacturingRecipe;
			else if(first != production.processes.cend()) recipeId = first->processId;
			const auto* recipe = catalogue->ManufacturingProcess(ManufacturingProcessId(recipeId));
			if(!recipe) continue;
			ProductionProcess process; process.developmentId = dev.id; process.slot = slot;
			process.enabled = dev.manufacturingCitizenEnabled;
			if(InitializeProductionProcess(*catalogue, process, recipeId,
				TechnologyOfProduct(dev.industryId, dev.technology, recipe->commodity.value, state.developments)))
			{ production.processes.append(process); jobsChanged = true; }
		}
	};
	for(const auto& dev : state.developments) ensureProcesses(dev);
	std::stable_sort(production.processes.begin(), production.processes.end(), [](const ProductionProcess& a, const ProductionProcess& b) {
		return a.developmentId < b.developmentId || (a.developmentId == b.developmentId && a.slot < b.slot);
	});
	QHash<quint32, quint32> enabled;
	for(const auto& process : production.processes) if(process.enabled) ++enabled[process.developmentId];
	const bool reverseEmployment = AuTime::CurrentMinCycle() > .5f;
	if(jobsChanged || !state.lastEmployment || now < state.lastEmployment || (now - state.lastEmployment) / 1000 > 29)
	{
		const auto employment = EmployCitizens(*catalogue, state.developments, enabled,
			quint32(state.population), !breathable, reverseEmployment);
		accounting.unemployed = employment.unemployed;
		state.lastEmployment = now;
		if(!state.lastPopulationCycle || now < state.lastPopulationCycle) state.lastPopulationCycle = now;
		else if(double((now - state.lastPopulationCycle) / 1000) >= 771.4285714285714)
		{
			production.vehicles = cycleVehicles;
			cycleReport->m_Vehicles.clear();
			for(const auto vehicle : cycleVehicles)
				cycleReport->m_Vehicles.append(COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(vehicle))));
			std::unique_ptr<AuDNABase> cycleCitizenDNA;
			if(!ReadSpeciesDNA(state.citizenSpecies.isEmpty() ? populationFacts.citizenSpeciesFallback : state.citizenSpecies,
				cycleCitizenDNA))
			{ qWarning() << "Legacy city population:" << Id() << "Invalid citizen species."; return; }
			// CityCycle 0x2749d0 discards obsolete stock before the report
			// snapshot and meals. Underwater cities retain their air and water.
			DiscardObsoleteCityItems(*catalogue, inventory, !needsEnclosure, quint8(cycleCitizenDNA->EcoRole()),
				CityStorage(*catalogue, state.developments, zone).cargo);
			const AuItemHash initialInventory(inventory);
			*cycleReport->m_Inventory = initialInventory;
			*cycleReport->m_Tools = production.tools;
			if(!populationFacts.revoltDestinationKnown) UpgradeBuildingsFromInventory(*catalogue, state, inventory, production);
			PopulationRuntimeReport report;
			if(!ApplyCityPopulation(state, inventory, accounting, empireTaxes, populationFacts, now,
				int(employment.jobs), report, error))
			{ qWarning() << "Legacy city population:" << Id() << error; return; }
			else
			{
				revoltRequested = report.revoltRequested;
				cycleInterrupted = report.cycleInterrupted;
				revoltCaptured = report.revoltCaptured;
				revoltOldEmpire = report.revoltOldEmpire;
				revoltNewEmpire = report.revoltNewEmpire;
				citizenSpeciesChanged = report.citizenSpeciesChanged;
				tributeDestination = report.tributeDestination;
				tributeAmount = report.tributeAmount;
				AuCityReportHistory history;
				if(!cycleReport || (!state.reportHistory.isEmpty() && !ReadReportHistory(state.reportHistory, history))
					|| (!cycleInterrupted && !CompleteCityReport(*cycleReport, state, initialInventory, inventory, production, accounting, report, employment, now, error)))
				{ qWarning() << "Legacy city report:" << Id() << error; return; }
				if(cycleInterrupted)
				{
					// Original CityCycle returns early after a neglect revolt. Its
					// caller still stores the partially filled report, without meals,
					// balance-sheet copying, activity resets or status notifications.
					if(!EncodeAccounting(AccountingState(), cycleReport->m_LegacyAccounting, error)) return;
					cycleReport->m_LegacyPopulationAfter = state.population;
					cycleReport->m_WhenCompiled = AuTime(now);
				}
				if(!history.IsEmpty()) previousReport.reset(new AuCityReport(history.Last()));
				history.Append(*cycleReport);
				QByteArray historyBytes;
				if(!WriteReportHistory(history, historyBytes, error))
				{ qWarning() << "Legacy city report:" << Id() << error; return; }
				state.reportHistory = historyBytes;
				reportCompleted = true;
				// CityCycle snapshots power every period. With a bank, it also
				// snapshots both ledgers before ResetAccountingPeriod (0x2b5160).
				if(!cycleInterrupted)
				{
					accounting.powerRequested = 0;
					if(HasBank(state)) accounting.accounts.ResetPeriod();
				}
			}
		}
	}
	taxes = accounting.taxes;
	const auto electricity = COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(3)));
	const bool hadPower = ringworld || inventory.RawCount(electricity) != 0;
	const float motivation = Motivation(state, taxes);
	const AuSpecies citizens(state.citizenSpecies);
	const quint8 ecoRole = citizens.IsNull() ? 0 : quint8(citizens.EcoRole());
	QVector<int> buildings, roads;
	for(int index = 0; index < state.developments.size(); ++index)
		(state.developments[index].industryId == 2 ? roads : buildings).append(index);
	const bool reverseWork = !reverseEmployment;
	const auto order = [&state](int a, int b) { return state.developments[a].location < state.developments[b].location; };
	std::stable_sort(buildings.begin(), buildings.end(), order); std::stable_sort(roads.begin(), roads.end(), order);
	if(reverseWork) { std::reverse(buildings.begin(), buildings.end()); std::reverse(roads.begin(), roads.end()); }
	QVector<int> sites = reverseWork ? roads : buildings;
	sites += reverseWork ? buildings : roads;
	for(const auto index : sites)
	{
		auto& dev = state.developments[index];
		if(dev.demolished) continue;
		const auto& environment = environments[dev.id];
		const auto demand = dev.industryId == 2 ? RoadPowerDemand(dev, environment.dark) : BuildingPowerDemand(dev, environment.dark);
		accounting.powerRequested += demand;
		if(!ringworld)
		{
			const auto consumed = quint32(inventory.ConsumeCommodity(electricity, demand));
			if(consumed < demand) powerImports.Import(state, inventory, connections, demand - consumed,
				CityStorage(*catalogue, state.developments, zone).electricity);
		}
		const bool canOperate = hadPower || needsEnclosure || !environment.dark;
		// TickDevelopment 0x2d2ef0 finishes the site's existing production
		// before OperatedByCitizens applies construction and starts new work.
		for(auto& process : production.processes)
		{
			if(process.developmentId != dev.id) continue;
			const auto* selectedRecipe = catalogue->ManufacturingProcess(ManufacturingProcessId(process.processId));
			if(!selectedRecipe) continue;
			SetProductionTechnology(*catalogue, process, TechnologyOfProduct(dev.industryId, dev.technology,
				selectedRecipe->commodity.value, state.developments));
			ProductionOutput output;
			if(FinishProduction(*catalogue, process, environment.output, now, output))
			{
				ScalePowerProduction(output, dev.industryId, zone);
				process.lastQuantity = qint32(output.quantity);
				bool farmReady = true;
				qint32 farmFailure = -99;
				if(ProductionFarmAnimalLimit(dev.industryId, output.commodityId))
				{
					const auto* livestock = farmProduction.Find(dev.id, process.slot, &state);
					farmReady = livestock != nullptr;
					if(livestock)
					{
						farmProduction.MarkApplied(dev.id, process.slot);
						farmReady = ApplyFarmAnimalResult(output, livestock->status, livestock->species);
						farmFailure = -2;
						// A new animal uses one output unit and consumes inputs even
						// when the remaining cargo cannot fit (DpBuilding::Tick).
						if(livestock->status == 0) ConsumeProductionComponents(process);
					}
				}
				if(!farmReady) process.lastQuantity = farmFailure;
				else if(!output.quantity)
				{
					// The single produced unit became livestock; no cargo remains.
				}
				else if(output.unit)
				{
					if(IsSpawnedProductionVehicle(output.commodityId))
					{
						if(vehicleProduction.Apply(dev.id, process.slot, output, state, inventory))
						{
							if(!production.vehicles.contains(output.commodityId)) production.vehicles.append(output.commodityId);
							process.lastQuantity = 1;
							ConsumeProductionComponents(process);
						}
						else process.lastQuantity = -1; // Keep applied materials for another spawn attempt.
					}
					else switch(output.commodityId)
					{
					case 54: // Mine machinery
					case 70: // Commercial ship
						// DpBuilding::Tick 0x5f22c0 registers these without
						// spawning an atom, then consumes their applied inputs.
						if(!production.vehicles.contains(output.commodityId)) production.vehicles.append(output.commodityId);
						ConsumeProductionComponents(process);
						break;
					case 71:  // Naval ship
					case 103: // Tank
						// The original registers these too, but retains inputs
						// and reports -99 instead of completing the output.
						if(!production.vehicles.contains(output.commodityId)) production.vehicles.append(output.commodityId);
						process.lastQuantity = -99;
						break;
					default:
						process.lastQuantity = -99;
						break;
					}
				}
				else if(output.commodityId == 157)
				{
					const auto amount = CurrencyProductionValue(output);
					accounting.accounts.Credit(CitizenIncome::Mint, amount, taxes);
					process.lastQuantity = qint32(amount); process.lastTechnology = 0; process.lastQuality = 255;
					ConsumeProductionComponents(process);
				}
				else if(output.research && (!output.technology || state.allegianceEmpire != state.controllingEmpire))
					ConsumeProductionComponents(process);
				else if(output.research && output.commodityId == IndustryTechnologyCommodity(dev.industryId)
					&& output.technology > dev.technology)
				{
					ApplyBuildingTechnologyChange(*catalogue, state, production, dev.id, dev.technology + 1);
					ConsumeProductionComponents(process);
				}
				else
				{
					const auto runtime = COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(output.commodityId)));
					std::unique_ptr<AuItem> item(AuItem::Create(runtime, output.quality, state.controllingEmpire, output.quantity));
					if(item && item->SetTechLevel(output.technology))
					{
						if(auto* biological = dynamic_cast<AuItemDNA*>(item.get()))
						{
							std::unique_ptr<AuDNABase> dna(output.species.CreateTypicalDNA());
							if(dna) biological->SetDNA(dna.get(), output.viableDNA, output.sentient, Mature);
						}
						const auto capacity = CityStorage(*catalogue, state.developments, zone);
						if(AddCityCommodity(*catalogue, inventory, *item, capacity, !needsEnclosure, ecoRole))
						{
							if(output.research) UpgradeBuildingsFromInventory(*catalogue, state, inventory, production, dev.id);
							else constructionChanged |= SupplyDevelopmentMaterials(state, dev, inventory, production, accounting,
								supplyFacts.value(dev.id), constructionMaterials);
							ConsumeProductionComponents(process);
						}
						else if(IsObsoleteCityItem(*catalogue, inventory, *item, breathable, ecoRole, capacity.cargo))
						{ ConsumeProductionComponents(process); process.lastQuantity = -4; }
						else process.lastQuantity = -3;
					}
				}
			}
		}
		if(canOperate && dev.citizenWorker && !dev.halted && !dev.components.isEmpty()
			&& WorkerAvailable(*catalogue, dev, enabled.value(dev.id)) && Motivated(motivation))
		{
			bool siteChanged = false;
			constructionChanged |= ApplyCitizenConstruction(state, dev, inventory, production,
				constructionFacts.value(dev.id), siteChanged, constructionMaterials);
			physicalChanged |= siteChanged;
			if(siteChanged)
			{
				// Completion can append slots; finish the old process iteration
				// before allowing its vector to reallocate.
				ensureProcesses(dev);
				enabled[dev.id] = 0;
				for(const auto& process : production.processes) if(process.developmentId == dev.id && process.enabled) ++enabled[dev.id];
			}
		}
		const bool available = WorkerAvailable(*catalogue, dev, enabled.value(dev.id));
		for(auto& process : production.processes)
		{
			if(process.developmentId != dev.id) continue;
			if(canOperate && available && process.enabled && !process.completeAt
				&& process.minimumTechnology <= dev.technology
				&& (!process.lastWorked || (now - process.lastWorked) / 1000 > 10))
			{
				process.lastWorked = now;
				const auto* recipe = catalogue->ManufacturingProcess(ManufacturingProcessId(process.processId));
				if(recipe && (ProductionIsCriticalToSurvival(*catalogue, recipe->commodity.value, false) || Motivated(motivation)))
				{
					FetchCityProcess(*catalogue, process, production, inventory, accounting, taxes, environment, ringworld);
					RunProduction(process, now);
				}
			}
		}
	}
	state.capacitor = float(inventory.RawCount(electricity));
	if(!WriteEconomy(*catalogue, state, inventory, production, accounting, error))
	{ qWarning() << "Legacy city economy:" << Id() << error; return; }
	if(!constructionMaterials.Commit(error))
	{ qWarning() << "Legacy city construction materials:" << Id() << error; return; }
	state.lastVisit = qMax(state.lastVisit, m_City.lastVisit);
	const bool changed = constructionChanged || EconomyChanged(m_City, state);
	if(changed) { m_City = state; ++m_CityRevision; }
	powerImports.Commit();
	lock.unlock();
	powerImports.Publish();
	if(changed) SetModified();
	farmProduction.Publish(state.controllingEmpire);
	vehicleProduction.Publish(state.controllingEmpire);
	constructionMaterials.Finalize();
	if(before.population != state.population) world->WorldPopulationChanged();
	if(physicalChanged) world->InvalidateEnvironmentRecursive();
	if(citizenSpeciesChanged) emit EmpireCityWorkerSpeciesChanged(state.controllingEmpire, Id(), AuSpecies(state.citizenSpecies));
	if(tributeDestination && tributeAmount > 0) CreditCityBank(tributeAmount, AuId(tributeDestination));
	if(revoltRequested)
	{
		QString empireName;
		const auto& data = g_Au.LockDataForRead();
		data.EmpireName(before.controllingEmpire, empireName);
		g_Au.UnlockData();
		NotifyRevolt(revoltOldEmpire, revoltNewEmpire, revoltCaptured,
			tr("Citizens are angry at the government for neglecting them. Riots and protests erupted into the streets. "
			"Effigies of the emperor were hanged and burned by enraged citizens. The flag of %1 was thrown to the ground, trampled and burned.<br><br>"
			"In a triumphant moment, police and military units sided with the protesters. The tide turned against the few remaining loyalists. "
			"Local politicians and school teachers were hanged and their homes burned.").arg(empireName.toHtmlEscaped()));
	}
	if(reportCompleted && !cycleInterrupted) SendCityReport(*cycleReport, previousReport.get());
}
