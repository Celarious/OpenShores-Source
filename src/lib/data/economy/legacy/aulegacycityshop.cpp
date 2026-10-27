#include "aulegacycityshop.h"
#include "aulegacycityproduction.h"
#include "aulegacycitystate.h"
#include <cmath>
#include <limits>

namespace AuLegacyCity
{
namespace
{
// Keep the original float calculation while preventing the unsigned supply
// sum in AuItem::Demand from wrapping for unusually large saved inventories.
const quint64 MaximumTally = std::numeric_limits<quint32>::max() / 2;

void AddTally(quint64& total, quint64 amount)
{
	total = qMin(MaximumTally, total + amount);
}

quint32 ProductionQuantity(const AuLegacyCityCatalogue& catalogue, const ProductionProcess& process,
	const ManufacturingProcessRecord& recipe)
{
	const CommodityRecord* commodity = catalogue.Commodity(recipe.commodity);
	const bool bulk = recipe.commodity.value == 3 || recipe.commodity.value == 157
		|| (commodity && commodity->descriptor.IsBrokerItem() && !commodity->IsTechnology());
	return quint32(recipe.quantity) * (process.bulkManufacturing && bulk ? 10U : 1U);
}
}

quint32 ShopUnitPrice(const AuLegacyCityCatalogue& catalogue, const State& city,
	const AuItemHash& inventory, const ProductionState& production, const AuItem& item, quint8 industry)
{
	CommodityId historical;
	if(!AuLegacyItemTechnologyActive()
		|| !catalogue.HistoricalCommodityId(quint16(item.CommodityId()), historical)) return 0;

	// DaCity::ProcessDemandTally 0x2a52e0 / AuItemHash::BetterCount
	// 0x270e50 count higher TL, or equal TL with equal-or-higher quality.
	quint64 stock = 0, needed = 0, produced = 0;
	for(auto stored = inventory.find(item.CommodityId()); stored != inventory.cend()
		&& stored->first == item.CommodityId(); ++stored)
		if(stored->second->IsBetter(item)) AddTally(stock, stored->second->Count());

	// DeManufacturingProcess::DemandTally 0x1260f0 includes enabled
	// recipes even while idle. Consumed inputs recur; tools have modest demand.
	for(const ProductionProcess& process : production.processes)
	{
		if(!process.enabled) continue;
		const ManufacturingProcessRecord* recipe = catalogue.ManufacturingProcess(ManufacturingProcessId(process.processId));
		if(!recipe) continue;
		for(const ProductionComponent& component : process.components)
			if(component.commodityId == historical.value)
			{
				if(!component.required) AddTally(needed, component.effect == 5 ? 2 : 1);
				else if(item.TechLevel() >= process.minimumTechnology)
					AddTally(needed, quint64(component.required) * 10);
			}
		if(recipe->commodity == historical && item.TechLevel() <= process.buildingTechnology)
			AddTally(produced, quint64(ProductionQuantity(catalogue, process, *recipe)) * 10);
	}
	if(item.EmpireOfOrigin() != city.controllingEmpire) produced = 0;

	if(IsProductionTechnology(historical.value))
	{
		// Patents are demanded for industry upgrades, not construction. A TL
		// above the highest building TL gets four times the intermediate demand.
		const quint8 technologyIndustry = TechnologyProductIndustry(historical.value);
		quint64 levels = 0;
		quint8 highest = 0, lowest = 31;
		for(const Development& development : city.developments)
			if(!development.demolished && development.completedLevels
				&& development.industryId == technologyIndustry)
			{
				levels += development.completedLevels;
				highest = qMax(highest, development.technology);
				lowest = qMin(lowest, development.technology);
			}
		if(levels && item.TechLevel() > lowest)
			AddTally(needed, qMax(quint64(1), levels / (item.TechLevel() > highest ? 2 : 8)));
	}
	else
	{
		// DeConstructionProcess::DemandTally 0x117a50 ignores halted work;
		// 0x129270 adds one plus the full consumed requirement for each match.
		for(const Development& development : city.developments)
			if(!development.demolished && !development.halted)
				for(const ConstructionComponent& component : development.components)
					if(component.commodityId == historical.value)
						AddTally(needed, quint64(component.quantity) + 1);
	}

	// DaCity::DemandCost 0x28baa0 rounds up before PriceOffer's integer
	// 120-percent margin (0x2a4f20). TxForSaleChat doubles cantina prices.
	const float demand = item.Demand(quint32(qMax(0, city.population)), quint32(stock), quint32(needed), quint32(produced));
	const float cost = std::ceil(item.UnitCost() * demand);
	if(!std::isfinite(cost)) return 0;
	const quint64 maximumPrice = std::numeric_limits<quint32>::max();
	const quint64 demandCost = cost < 1.f ? 1 : cost >= double(maximumPrice) ? maximumPrice : quint64(cost);
	quint64 price = demandCost * 120 / 100;
	if(price == demandCost) ++price;
	if(industry == 105) price *= 2;
	return quint32(qMin(maximumPrice, price));
}
}
