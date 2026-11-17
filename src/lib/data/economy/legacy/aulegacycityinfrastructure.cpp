#include "aulegacycityinfrastructure.h"
#include "auitem.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace AuLegacyCity
{
quint16 ManufacturingSlots(quint8 industry, quint8 completedLevels)
{
	// AuAtom13.dll DpBuilding::ManufacturingProcesses, 0x5a3330.
	switch(industry)
	{
	case 30: case 31: case 32: case 34: case 35:
	case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47: case 48:
	case 50: case 51:
	case 60: case 61: case 62: case 63: case 64: case 65: case 66: case 67:
	case 70: case 71: case 72: case 73: case 74: case 75: case 76: case 77:
	case 78: case 79: case 80: case 81: case 82: case 83: case 84: case 85:
	case 86: case 87: case 88: case 89: case 110: case 125:
		return completedLevels;
	case 0: case 1: case 2: case 3: case 10: case 114: case 126: case 129: case 130:
		return 0;
	default:
		return completedLevels ? 1 : 0;
	}
}

StorageCapacities DevelopmentStorage(const AuLegacyCityCatalogue& catalogue,
	const Development& development, quint8 buildingTechnology, quint8 orbitZone)
{
	StorageCapacities capacity;
	if(development.demolished || development.IsDestroyed() || development.industryId == 2) return capacity;
	const auto* industry = catalogue.Industry(IndustryId(development.industryId));
	if(!industry) return capacity;
	// DpDevelopment::Storage 0x651890; original zones are 0..4.
	capacity.cargo = industry->StorageUnits();
	capacity.electricity = industry->CapacitorUnits();
	if((development.industryId == 67 && orbitZone == 4)
		|| (development.industryId == 66 && orbitZone == 0))
		capacity.electricity *= 4;
	else if((development.industryId == 67 && orbitZone == 3)
		|| (development.industryId == 66 && orbitZone == 1))
		capacity.electricity *= 2;
	// DpBuilding::Storage 0xfdeb0: multiply before integer TL uplift.
	capacity.cargo *= development.completedLevels;
	capacity.electricity *= development.completedLevels;
	capacity.cargo += quint32(buildingTechnology) * capacity.cargo * 5 / 100;
	capacity.electricity += quint32(buildingTechnology) * capacity.electricity * 5 / 100;
	capacity.cargo += quint32(ManufacturingSlots(development.industryId, development.completedLevels)) * 10;
	if(!development.components.isEmpty()) capacity.cargo += 10;
	return capacity;
}

StorageCapacities CityStorage(const AuLegacyCityCatalogue& catalogue,
	const QVector<Development>& developments, quint8 orbitZone)
{
	StorageCapacities total;
	for(int index = 0; index < developments.size(); ++index)
	{
		const auto capacity = DevelopmentStorage(catalogue, developments[index],
			developments[index].technology, orbitZone);
		total.cargo += capacity.cargo;
		total.electricity += capacity.electricity;
	}
	return total;
}

quint32 BuildingPowerDemand(const Development& development, bool isDark)
{
	// DpBuilding::PowerConsumption 0xf9bc0 uses the current top level,
	// including the level being constructed; daylight still consumes one.
	if(development.demolished || development.IsDestroyed() || development.industryId == 2) return 0;
	return isDark ? development.completedLevels + (development.components.isEmpty() ? 0U : 1U) : 1U;
}

quint32 RoadPowerDemand(const Development& development, bool isDark)
{
	// DpRoad::PowerConsumption 0x17e6b0: asphalt/concrete/pressurized
	// roads, or any construction site, consume one at night.
	if(development.demolished || development.industryId != 2 || !isDark) return 0;
	return !development.components.isEmpty() || development.constructionProcessId == 3
		|| development.constructionProcessId == 4 || development.constructionProcessId == 7 ? 1 : 0;
}

quint32 IndustryJobs(const AuLegacyCityCatalogue& catalogue, quint8 industry)
{
	// DgData::IndustryJobs 0x31660. Negative employees encode a fixed
	// building staff; they are not negative jobs or a multiplier per level.
	if(industry == 1 || industry == 10) return 0;
	if(industry == 11 || industry == 12) return 1;
	const auto* record = catalogue.Industry(IndustryId(industry));
	return record ? quint32(std::abs(int(record->employees))) : 0;
}

quint32 DevelopmentProductionJobs(const AuLegacyCityCatalogue& catalogue,
	const Development& development, quint32 enabledProcessCount)
{
	// DaCity::JobsAtDevLL 0x29c010. Per-level manufacturing industries
	// use enabled process jobs; other positive-employee industries add a
	// direct job per completed level even if their process is disabled.
	if(development.demolished || development.industryId == 2) return 0;
	if(ManufacturingSlots(development.industryId, 2) == 2) return enabledProcessCount;
	const auto* industry = catalogue.Industry(IndustryId(development.industryId));
	if(industry && industry->employees > 0) return development.completedLevels;
	return ManufacturingSlots(development.industryId, development.completedLevels) ? enabledProcessCount : 0;
}

quint32 DevelopmentNonConstructionJobs(const AuLegacyCityCatalogue& catalogue,
	const Development& development, quint32 enabledProcessCount)
{
	// DpBuilding::NonConstructionJobs 0x5a34f0 adds the baseline staff
	// exactly once when at least one level is completed.
	if(development.demolished || development.industryId == 2) return 0;
	return DevelopmentProductionJobs(catalogue, development, enabledProcessCount)
		+ (development.completedLevels ? IndustryJobs(catalogue, development.industryId) : 0);
}

quint32 DevelopmentJobs(const AuLegacyCityCatalogue& catalogue,
	const Development& development, quint32 enabledProcessCount)
{
	if(development.demolished) return 0;
	return DevelopmentNonConstructionJobs(catalogue, development, enabledProcessCount)
		+ (!development.components.isEmpty() && !development.halted ? 1U : 0U);
}

EmploymentTotals EmployCitizens(const AuLegacyCityCatalogue& catalogue,
	QVector<Development>& developments, const QHash<quint32, quint32>& enabledProcessCounts,
	quint32 population, bool airProductionNeeded, bool reverse,
	const QHash<quint32, WorkforceCondition>& conditions)
{
	// DaWorldLL::EmployCitizens 0x516070 and DaCity::Employ* 0x28e2c0-
	// 0x28eee0. The ratio is fixed before repair hiring. Each site rounds
	// upward independently, so map traversal direction affects shortages.
	EmploymentTotals totals;
	totals.unemployed = population;
	const auto conditionOf = [&conditions](const Development& development) {
		auto condition = conditions.value(development.id);
		condition.damaged |= development.damage != 0;
		condition.destroyed |= development.IsDestroyed();
		condition.burning |= development.onFire;
		return condition;
	};
	QVector<int> buildings, roads;
	QVector<quint32> jobs(developments.size());
	for(int index = 0; index < developments.size(); ++index)
	{
		auto& development = developments[index];
		if(development.demolished) continue;
		const auto condition = conditionOf(development);
		const auto enabled = enabledProcessCounts.value(development.id);
		if(development.industryId == 2) roads.append(index); else buildings.append(index);
		if(condition.damaged && development.industryId != 2) ++totals.repairJobs;
		if(condition.destroyed && development.industryId != 2) continue;
		jobs[index] = DevelopmentJobs(catalogue, development, enabled);
		totals.jobs += jobs[index];
		totals.nonConstructionJobs += DevelopmentNonConstructionJobs(catalogue, development, enabled);
	}
	if(!totals.jobs && !totals.repairJobs) return totals;
	auto order = [&](int a, int b) { return developments[a].location < developments[b].location; };
	std::stable_sort(buildings.begin(), buildings.end(), order);
	std::stable_sort(roads.begin(), roads.end(), order);
	if(reverse) { std::reverse(buildings.begin(), buildings.end()); std::reverse(roads.begin(), roads.end()); }
	const double ratio = totals.jobs ? std::min(1.0, double(population) / totals.jobs) : 0.0;
	for(const int index : buildings)
	{
		developments[index].employees = 0;
		const auto condition = conditionOf(developments[index]);
		if(totals.unemployed && condition.damaged && !condition.burning)
		{ ++developments[index].employees; --totals.unemployed; }
	}
	auto assign = [&](int index)
	{
		// EmployNonEssentialPeople retains a road's prior allocation when
		// hiring again, but clears it if no workers or jobs remain.
		if(developments[index].industryId == 2 && (!totals.unemployed || !jobs[index]))
		{ developments[index].employees = 0; return; }
		const auto count = std::min(totals.unemployed, quint32(std::ceil(double(jobs[index]) * ratio)));
		developments[index].employees += count;
		totals.unemployed -= count;
	};
	auto foodProducer = [](quint8 industry)
	{
		// DpDevelopment::IsFoodProducer 0xf3ca0 deliberately excludes
		// brewery, distillery and winery from essential food priority.
		switch(industry)
		{ case 30: case 32: case 34: case 35: case 40: case 43: case 45: case 46: return true;
		default: return false; }
	};
	auto pass = [&](int category)
	{
		for(const int index : buildings)
		{
			const auto industry = developments[index].industryId;
			const bool matches = category == 0 ? industry >= 60 && industry <= 67
				: category == 1 ? industry == 83 : foodProducer(industry);
			if(matches && !conditionOf(developments[index]).destroyed) assign(index);
		}
	};
	if(airProductionNeeded) { pass(0); pass(1); pass(2); }
	else { pass(2); pass(0); }
	if(reverse) for(const int index : roads) assign(index);
	for(const int index : buildings)
	{
		const auto industry = developments[index].industryId;
		if(!foodProducer(industry) && !(industry >= 60 && industry <= 67)
			&& !(airProductionNeeded && industry == 83)
			&& !conditionOf(developments[index]).destroyed) assign(index);
	}
	if(!reverse) for(const int index : roads) assign(index);
	return totals;
}

bool CommerceCycleDue(qint64 previousMilliseconds, qint64 nowMilliseconds, quint32 cityId)
{
	if(previousMilliseconds <= 0 || nowMilliseconds < previousMilliseconds) return false;
	const qint64 previousMinute = previousMilliseconds / 60000;
	return previousMinute != nowMilliseconds / 60000
		&& (previousMinute % 60 + cityId) % 6 == 0;
}

quint32 IndustryImportPercentage(quint8 industry, quint32 completedLevels)
{
	// DaCity::Tock 0x2d3100.
	switch(industry)
	{
	case 101: return std::min(completedLevels, 40U) * 2;
	case 103: case 104: return std::min(completedLevels, 40U);
	case 105: return std::min(completedLevels / 2 + completedLevels % 2, 20U);
	default: return 0;
	}
}

quint32 ConnectionImportPercentage(quint32 percentage, quint32 connectionFlags)
{
	// DaCity::ImportBrokerItems 0x296f60 and the matching service imports.
	if(!percentage || !connectionFlags) return 0;
	quint32 result = percentage;
	if(connectionFlags == 8) result /= 4; // Other world in the system.
	else if(connectionFlags & 0x30)
		result = connectionFlags & 0x10 ? (percentage + 2) / 3 : (percentage + 3) / 4;
	else if(!(connectionFlags & 1))
		result = connectionFlags & 2 ? percentage / 2 : percentage / 3;
	return std::max(1U, result);
}

PowerTransfer PlanPowerTransfer(quint32 exporterElectricity, quint32 demand,
	quint32 receiverFreeCapacity)
{
	// DaCity::ImportPower 0x297a10: five percent of the exporter's
	// current stored power, first direct demand and then capacitor charge.
	PowerTransfer transfer;
	const quint32 quota = exporterElectricity / 20;
	transfer.supplied = std::min(quota, demand);
	transfer.charged = std::min(quota - transfer.supplied, receiverFreeCapacity);
	return transfer;
}

PowerTransfer TransferPower(AuItemHash& exporter, AuItemHash& receiver,
	quint16 runtimeElectricity, quint32 demand, quint32 receiverCapacitor)
{
	if(&exporter == &receiver || !runtimeElectricity) return {};
	const auto commodity = static_cast<COMMODITYID>(runtimeElectricity);
	const quint32 stored = receiver.RawCount(commodity);
	const auto planned = PlanPowerTransfer(exporter.RawCount(commodity), demand,
		stored < receiverCapacitor ? receiverCapacitor - stored : 0);
	std::unique_ptr<AuItem> charge;
	if(planned.charged)
	{
		charge.reset(AuItem::Create(commodity, 255, 0, planned.charged));
		if(!charge || !charge->SetTechLevel(31) || !receiver.CanAddItem(*charge)) return {};
	}
	const quint32 consumed = quint32(exporter.ConsumeCommodity(commodity, planned.RemovedFromExporter()));
	PowerTransfer actual;
	actual.supplied = std::min(consumed, planned.supplied);
	actual.charged = consumed - actual.supplied;
	if(actual.charged) receiver.AddItemCount(*charge, actual.charged);
	return actual;
}
}
