#include "aulegacycitypopulationruntime.h"
#include "aulegacycitycatalogue.h"
#include "auglobal.h"
#include "auitem.h"
#include "auanimalia.h"
#include "audna.h"
#include "auspecies.h"
#include "daatomlist.h"
#include "dalegacycity.h"
#include "danpc.h"
#include "daworldll.h"
#include "daworldringsection.h"
#include "dasolarsystem.h"
#include "dasector.h"
#include "dagalaxy.h"
#include "dauniverse.h"
#include "dgdata.h"
#include "dgempire.h"
#include "dhdna.h"
#include "dpbody.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <QDateTime>

namespace AuLegacyCity
{
bool ReadSpeciesDNA(const QByteArray& key, std::unique_ptr<AuDNABase>& dna)
{
	dna.reset();
	// DhDNA::FromSpecies reads a QByteArray wrapper, and its historical reader
	// assumes the advertised inner length is correct. Validate it first.
	const quint32 size = quint32(sizeof(DhDNA_));
	if(key.size() == int(size) + 4)
	{
		const quint32 declared = (quint32(quint8(key[0])) << 24) | (quint32(quint8(key[1])) << 16)
			| (quint32(quint8(key[2])) << 8) | quint32(quint8(key[3]));
		if(declared == size)
		{
			std::unique_ptr<DhDNA> original(new DhDNA);
			if(!original->FromSpecies(AuSpecies(key))) return false;
			dna = std::move(original);
			return true;
		}
	}
	// AuDNA::FromSpecies does not inspect stream status. Use its chromosome
	// readers with bounded counts and verify the complete canonical species.
	if(key.size() < 6 || key.size() > 65536 || quint8(key[0]) != 0x29 || quint8(key[1]) != 1)
		return false;
	QDataStream in(key);
	in.setVersion(QDataStream::Qt_5_7);
	quint8 type = 0, version = 0;
	in >> type >> version;
	std::unique_ptr<AuDNA> current(new AuDNA);
	for(int chain = 0; chain < 2; ++chain)
	{
		quint16 count = 0;
		in >> count;
		if(in.status() != QDataStream::Ok || count > key.size()) return false;
		AuChromatid& chromatid = current->Chain(AuDNABase::CHROMATID(chain));
		while(count--)
		{
			in >> type;
			if(in.status() != QDataStream::Ok) return false;
			std::unique_ptr<AuChromosome> chromosome(AuChromosome::CreateChromosome(AuChromosome::CTYPE(type)));
			if(!chromosome) return false;
			chromosome->ReadSpeciesKey(in, version);
			if(in.status() != QDataStream::Ok) return false;
			chromatid.append(chromosome.release());
		}
	}
	if(!in.atEnd() || current->Species().Key() != key) return false;
	dna = std::move(current);
	return true;
}

namespace
{
bool CanSendTribute(DaLegacyCity& city, const AuId& destinationGalaxy)
{
	DaGalaxy* source = city.FindParentGalaxy();
	if(!source || destinationGalaxy.IsNull()) return false;
	if(source->Id() == destinationGalaxy) return true;
	DaUniverse* universe = source->FindParentUniverse();
	auto* destination = universe ? dynamic_cast<DaGalaxy*>(universe->FindSubatomId(destinationGalaxy)) : nullptr;
	if(!destination) return false;
	const auto protectedRealm = [](GALAXY galaxy)
	{
		return galaxy == VeilOfTargoss || galaxy == EdgeOfTheRift || galaxy == FallasEmbrace;
	};
	return protectedRealm(source->Galaxy()) == protectedRealm(destination->Galaxy());
}
}

PopulationRuntimeFacts GatherPopulationFacts(DaLegacyCity& city, DaWorldLL& world, const State& state)
{
	PopulationRuntimeFacts facts;
	PopulationContext& c = facts.context;
	const AuPoint location = city.Location();
	facts.onRingworld = dynamic_cast<DaWorldRingSection*>(&world) != nullptr;
	c.needsEnclosure = (world.Hydrographics() && world.AltitudeMSL(location) <= -32.0)
		|| !world.HasBreathableAtmosphere();
	// 2016 IsHabitable(1) used these two tests. Modern HomeWorld adds rules.
	c.indigenousWorld = facts.onRingworld || (world.IsHabitableOrbit() && world.HasBreathableAtmosphere());
	if(c.indigenousWorld && world.HasWorkerSpecies()) facts.nativeSpecies = world.WorkerSpecies().Key();
	c.environmentCapacity = world.PopulationMax();
	// The old radius byte was thousands of km; modern radii are hundreds of
	// km. PopulationMax already preserves old radius*100 as modern radius*10.
	const int zone = world.ResourceZone(location);
	c.environmentPopulation = state.population;
	DaAtomList atoms;
	world.ListNormalSubatoms(atoms);
	for(DaAtom* atom : atoms)
	{
		const auto* troop = dynamic_cast<const DaNpc*>(atom);
		if(troop && troop->Role() == ROLE_Troop && troop->CurrentHitPoints() > 0
			&& troop->EmpireId() == state.controllingEmpire && troop->GarrisonBd() == city.Id()) ++facts.assignedTroops;
		DaLegacyCity* other = dynamic_cast<DaLegacyCity*>(atom);
		if(!other || other == &city || world.ResourceZone(other->Location()) != zone) continue;
		c.environmentPopulation += other->Snapshot().population;
	}
	if(DaSolarSystem* system = city.FindParentSolarSystem())
	{
		const AuTime lastVisit = system->LastPersonVisited();
		if(!lastVisit.IsNull()) c.daysSinceSystemVisit = int(lastVisit.ElapsedDay());
	}
	if(state.lastVisit) facts.daysSinceCityVisit = int(AuTime(state.lastVisit).ElapsedDay());
	AuId empireCapital, empireCapitalGalaxy, sectorCapital;
	AuId overlordCapital, overlordGalaxy;
	quint8 overlordPercent = 0;
	const DgData& gd = g_Au.LockDataForRead();
	c.exemptFromVisitPenalty = gd.EmpireRole(state.controllingEmpire) == EMPIRE_Agent;
	facts.pirateEmpire = gd.EmpireRole(state.controllingEmpire) == EMPIRE_Pirate;
	const bool normalizeAllegiance = !state.loyalPopulation || !state.population;
	const bool occupied = !normalizeAllegiance && state.allegianceEmpire != state.controllingEmpire;
	const qint64 now = AuTime::CurrentMSecsSinceEpoch();
	if(state.lastPopulationCycle && now >= state.lastPopulationCycle
		&& double((now - state.lastPopulationCycle) / 1000) >= 771.4285714285714
		&& !occupied && facts.daysSinceCityVisit >= 15 && state.loyalPopulation < 2
		&& state.satisfaction < 0 && !facts.pirateEmpire)
	{
		facts.revoltDestinationKnown = true;
		facts.revoltEmpire = normalizeAllegiance || state.allegianceEmpire == state.controllingEmpire
			? gd.SelectLegacyPirateEmpire() : state.allegianceEmpire;
	}
	if(gd.EmpireCapital(state.controllingEmpire, empireCapital, empireCapitalGalaxy))
	{
		if(DaSector* sector = city.FindParentSector())
		{
			sectorCapital = gd.EmpireSectorCapital(state.controllingEmpire, sector->Id());
			c.exemptFromVisitPenalty |= empireCapital != city.Id() && !sectorCapital.IsNull();
		}
		if(empireCapital == city.Id())
		{
			DgEmpire empire;
			empire.m_id = state.controllingEmpire;
			if(gd.Empire(empire))
				for(const auto& stance : empire.m_Stance)
					if(stance.second.m_stance == POL_Vassal && stance.second.m_tribute)
					{
						if(gd.EmpireCapital(stance.second.m_id, overlordCapital, overlordGalaxy))
							overlordPercent = stance.second.m_tribute;
						break;
					}
		}
	}
	if(state.citizenSpecies.isEmpty())
		facts.citizenSpeciesFallback = gd.EmpireCapitolSpecies(state.controllingEmpire).Key();
	g_Au.UnlockData();
	// CityCycle leaves the report's system-visit days at zero when any of
	// these exemptions applies, so ReportHtml has no abandonment to report.
	c.reportExemptFromVisitPenalty = c.exemptFromVisitPenalty;
	if(!empireCapital.IsNull())
	{
		if(empireCapital == city.Id())
		{
			if(overlordPercent && !overlordCapital.IsNull() && CanSendTribute(city, overlordGalaxy))
			{
				facts.tributeDestination = overlordCapital.GetID();
				facts.tributePercent = overlordPercent;
				facts.tributeUsesPercentage = true;
			}
		}
		else if(!sectorCapital.IsNull() && sectorCapital != city.Id())
		{ facts.tributeDestination = sectorCapital.GetID(); facts.tributePercent = 50; }
		else if(CanSendTribute(city, empireCapitalGalaxy))
		{ facts.tributeDestination = empireCapital.GetID(); facts.tributePercent = 50; }
	}
	return facts;
}

bool ApplyCityPopulation(State& state, AuItemHash& inventory, AccountingState& accounting,
	const TaxRates& taxes, const PopulationRuntimeFacts& facts, qint64 now, int jobs,
	PopulationRuntimeReport& report, QString& error)
{
	error.clear();
	const AuLegacyCityCatalogue* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue) { error = QStringLiteral("The legacy city catalogue is unavailable."); return false; }
	if(!AuLegacyItemTechnologyActive())
	{ error = QStringLiteral("Legacy population requires the legacy item technology boundary."); return false; }
	const QByteArray citizenKey = state.citizenSpecies.isEmpty() ? facts.citizenSpeciesFallback : state.citizenSpecies;
	std::unique_ptr<AuDNABase> citizenDNA;
	if(!ReadSpeciesDNA(citizenKey, citizenDNA))
	{ error = QStringLiteral("Legacy city population requires its original citizen DNA; this species is missing or unsupported."); return false; }
	PopulationContext c = facts.context;
	c.daysSinceCityVisit = facts.daysSinceCityVisit;
	c.levels.fill(0);
	for(const Development& development : state.developments)
		if(!development.demolished) c.levels[development.industryId] += development.completedLevels;
	c.population = state.population;
	c.satisfaction = state.satisfaction;
	c.unemployed = int(accounting.unemployed);
	c.homes = Homes(*catalogue, c.levels, &c.apartmentHomes);
	c.loyalty = c.population <= int(state.loyalPopulation) ? 1.0f : float(state.loyalPopulation) / float(c.population);
	// CityCycle captures the report's loyalty first, then normalizes an empty
	// opposing allegiance before using IsOccupied in morale/migration.
	const bool normalizeAllegiance = !state.loyalPopulation || !state.population;
	c.occupied = !normalizeAllegiance && state.allegianceEmpire != state.controllingEmpire;
	quint32 loyalPopulation = normalizeAllegiance ? 0 : state.loyalPopulation;
	bool revoltRequested = false;
	if(c.occupied) loyalPopulation = loyalPopulation < 2 ? 0 : loyalPopulation - 1;
	else if(facts.daysSinceCityVisit < 15)
	{
		if(loyalPopulation < quint32(c.population)) ++loyalPopulation;
		else if(quint32(c.population) < loyalPopulation)
			loyalPopulation = c.population < 2 ? 0 : loyalPopulation - 1;
	}
	else
	{
		loyalPopulation = loyalPopulation < 2 ? 0 : loyalPopulation - 1;
		revoltRequested = !loyalPopulation && c.satisfaction < 0 && !facts.pirateEmpire;
	}
	if(revoltRequested)
	{
		if(!facts.revoltDestinationKnown)
		{ error = QStringLiteral("Legacy city neglect revolt requires its original destination empire."); return false; }
		PopulationRuntimeReport result;
		result.loyaltyAtStart = c.loyalty;
		result.occupied = c.occupied;
		result.allegianceEmpire = normalizeAllegiance ? state.controllingEmpire : state.allegianceEmpire;
		result.loyalPopulation = loyalPopulation;
		result.reportedLastTribute = accounting.lastTribute;
		result.revoltRequested = result.cycleInterrupted = true;
		result.revoltOldEmpire = state.controllingEmpire;
		result.revoltNewEmpire = facts.revoltEmpire;
		state.loyalPopulation = loyalPopulation;
		state.allegianceEmpire = facts.revoltEmpire;
		result.revoltCaptured = ApplyCitySurrender(state, accounting.accounts,
			result.revoltOldEmpire, result.revoltNewEmpire);
		state.lastVisit = now;
		// CityCycle returns immediately after Revolt, before meals, the tax
		// percentages, salaries, tribute and accounting-period resets. Tock
		// still appends the partial report and advances the cycle timestamp.
		accounting.taxes.hasCapital = taxes.hasCapital;
		accounting.lastPopulationCycle = state.lastPopulationCycle = now;
		result.change.population = state.population;
		result.change.satisfaction = state.satisfaction;
		result.change.unemployed = int(accounting.unemployed);
		report = result;
		return true;
	}
	c.ecoRole = quint8(citizenDNA->EcoRole());
	std::unique_ptr<AuDNABase> nativeDNA;
	if(c.indigenousWorld)
	{
		if(!ReadSpeciesDNA(facts.nativeSpecies, nativeDNA))
		{ error = QStringLiteral("Legacy city population requires the original indigenous DNA of this world."); return false; }
		const DhDNA* originalCitizen = dynamic_cast<const DhDNA*>(citizenDNA.get());
		const DhDNA* originalNative = dynamic_cast<const DhDNA*>(nativeDNA.get());
		if(originalCitizen && originalNative)
			c.differentNativeRace = originalCitizen->SkinType != originalNative->SkinType
				|| originalCitizen->EcologicalRole != originalNative->EcologicalRole;
		else
			// DNA4 has no original SkinType field. The existing species
			// compatibility rule supplies its native-race boundary.
			c.differentNativeRace = !citizenDNA->IsSexuallyCompatible(*nativeDNA);
	}
	else c.differentNativeRace = false;
	if((!c.indigenousWorld || c.differentNativeRace) && !facts.connectionsKnown)
	{ error = QStringLiteral("Legacy city immigration requires the recovered city connection state."); return false; }
	if(!facts.attackKnown)
	{ error = QStringLiteral("Legacy city population requires the city attack state."); return false; }
	if(state.population < 0 || accounting.unemployed > quint32(std::numeric_limits<int>::max())
		|| jobs < 0 || !std::isfinite(c.loyalty) || now < 0
		|| c.environmentCapacity < 0 || c.environmentPopulation < 0
		|| (c.environmentPopulation > c.environmentCapacity && c.environmentCapacity * 5 / 100 == 0)
		|| taxes.income < 0 || taxes.income > 100 || taxes.sales < 0 || taxes.sales > 100
		|| !std::isfinite(taxes.income) || !std::isfinite(taxes.sales))
	{ error = QStringLiteral("Invalid legacy population or account inputs."); return false; }
	quint16 foodPerMeal = accounting.foodPerMeal;
	if(c.population > 0 && foodPerMeal == 0)
	{
		if(const DhDNA* original = dynamic_cast<const DhDNA*>(citizenDNA.get()))
		{
			DpBody body('\1', true, *original);
			foodPerMeal = body.MaxHitPoints() / 5;
		}
		else
		{
			AuAnimalia body(*static_cast<const AuDNA*>(citizenDNA.get()), AuDNABase::Alpha, LeftHanded, Mature);
			foodPerMeal = body.MaxHitPoints() / 5;
		}
		if(!foodPerMeal) foodPerMeal = 10;
	}
	const quint64 requestedFood = quint64(c.population) * (foodPerMeal < 2 ? 10 : foodPerMeal);
	if(requestedFood > quint64(std::numeric_limits<quint32>::max()))
	{ error = QStringLiteral("Legacy city meal exceeds the original inventory quantity range."); return false; }
	const auto* town = catalogue->Industry(IndustryId(1));
	const auto* flag = catalogue->Industry(IndustryId(10));
	c.townSquareTroops = town ? std::abs(int(town->employees)) : 0;
	c.militaryFlagTroops = flag ? std::abs(int(flag->employees)) : 0;
	const COMMODITYID air = COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(5)));
	const COMMODITYID electricity = COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(3)));
	c.hasPower = facts.onRingworld || inventory.RawCount(electricity) != 0;
	c.hasAir = inventory.RawCount(air) != 0;
	c.airConsumed = 0;
	c.foodVariety = quint8(inventory.AvailableFoodTypes(ECOROLE(c.ecoRole)));
	PopulationRuntimeReport result;
	result.reportedLastTribute = accounting.lastTribute;
	result.loyaltyAtStart = c.loyalty;
	result.occupied = c.occupied;
	// CityCycle copies native allegiance/count after its one-person loyalty
	// adjustment and before meals, deaths and migration can change the count.
	result.allegianceEmpire = normalizeAllegiance ? state.controllingEmpire : state.allegianceEmpire;
	result.loyalPopulation = loyalPopulation;
	result.revoltRequested = revoltRequested;
	result.foodPerMeal = foodPerMeal;
	result.foodTypes = c.foodVariety;
	result.foodUnits = inventory.TotalFoodUnits(ECOROLE(c.ecoRole));
	if(c.population > 0)
	{
		if(c.needsEnclosure && c.hasAir) c.airConsumed = inventory.RawRemove(air, quint32(c.population));
		if(c.foodVariety)
			result.ateEnough = inventory.EatFood(quint32(requestedFood), ECOROLE(c.ecoRole), result.consumedLiquor)
				>= int(requestedFood);
		else result.ateEnough = false;
	}
	c.ateFullMeal = result.ateEnough;
	result.airConsumed = c.airConsumed;
	result.change = PopulationCycle(c);
	result.reportedChange = PopulationReport(c);
	// DecreasePopulation adjusts allegiance only when every inhabitant was
	// loyal before that individual decrease. Replay its original movement
	// order; IncreasePopulation does not increase the loyal count.
	int movingPopulation = c.population;
	const auto decreaseLoyalPopulation = [&movingPopulation, &loyalPopulation](int amount)
	{
		if(movingPopulation <= 0 || amount <= 0) return;
		amount = std::min(movingPopulation, amount);
		const int remaining = movingPopulation - amount;
		if(quint32(movingPopulation) <= loyalPopulation)
		{
			loyalPopulation -= std::min(loyalPopulation, quint32(amount));
			loyalPopulation = std::max(loyalPopulation, quint32(remaining));
		}
		movingPopulation = remaining;
	};
	decreaseLoyalPopulation(result.change.suffocated);
	decreaseLoyalPopulation(result.change.starved);
	movingPopulation += result.change.births;
	decreaseLoyalPopulation(result.change.emigrants);
	state.population = result.change.population;
	state.satisfaction = qint8(result.change.satisfaction);
	if(normalizeAllegiance)
	{
		state.allegianceEmpire = state.controllingEmpire;
	}
	state.loyalPopulation = loyalPopulation;
	state.citizenSpecies = citizenKey;
	// CityCycle 0x279708: one recruit per friendly base flag per cycle,
	// bounded by the town/flag requirement and one seventh of population.
	int troops = int(state.garrisonTroops) + facts.assignedTroops;
	const int requiredTroops = c.townSquareTroops + c.militaryFlagTroops * int(c.levels[10]);
	const int troopLimit = qMin(requiredTroops, state.population / 7);
	if(state.controllingEmpire && !c.occupied)
		for(const Flag& flag : state.flags)
		{
			if(troops >= troopLimit) break;
			if(flag.empire != state.controllingEmpire) continue;
			++state.garrisonTroops; ++troops; ++result.troopsDrafted;
			loyalPopulation -= qMin(loyalPopulation, quint32(1));
			--state.population;
		}
	state.loyalPopulation = loyalPopulation;
	result.troopsAtCity = quint16(qMin(troops, 65535));
	const bool emptied = c.population > 0 && (result.change.suffocated + result.change.starved >= c.population
		|| result.change.population == 0);
	if(emptied && c.indigenousWorld && citizenKey != facts.nativeSpecies)
	{
		state.citizenSpecies = facts.nativeSpecies;
		result.citizenSpeciesChanged = true;
	}
	accounting.unemployed = quint32(result.change.unemployed);
	accounting.foodPerMeal = foodPerMeal;
	accounting.lastPopulationCycle = state.lastPopulationCycle = now;
	accounting.taxes = taxes;
	if(!taxes.hasCapital || !c.levels[100])
		accounting.taxes.income = accounting.taxes.sales = accounting.taxes.subsidy = 0;
	if(c.levels[100]) accounting.accounts.CirculateSalaries(state.population, jobs, accounting.taxes);
	if(c.levels[100])
	{
		const int hour = QDateTime::fromMSecsSinceEpoch(now, Qt::UTC).time().hour();
		if(hour == 0)
		{
			if(!accounting.tributeSentToday)
			{
				accounting.lastTribute = 0;
				if(accounting.accounts.government > 0 && facts.tributeDestination && facts.tributePercent)
				{
					// CityCycle uses MULSD(percent), DIVSD(100) for a vassal,
					// including 50%; ordinary capital/sector tribute uses MULSD(.5).
					const double amount = facts.tributePercent == 50 && !facts.tributeUsesPercentage
						? accounting.accounts.government * 0.5
						: accounting.accounts.government * double(facts.tributePercent) / 100.0;
					accounting.lastTribute = amount;
					accounting.accounts.Debit(GovernmentExpense::Tribute, amount);
					result.tributeDestination = facts.tributeDestination;
					result.tributeAmount = float(amount);
				}
				accounting.tributeSentToday = true;
			}
		}
		else accounting.tributeSentToday = false;
	}
	report = result;
	return true;
}
}

void DaLegacyCity::RegisterEmpireVisit(quint32 empireId, qint64 now)
{
	if(!g_Au.IsLegacyGeneration() || !empireId || now <= 0) return;
	{
		QWriteLocker lock(&m_CityLock);
		if(m_City.controllingEmpire != empireId) return;
		m_City.lastVisit = now;
	}
	SetModified(NOTIFY_Database);
}
