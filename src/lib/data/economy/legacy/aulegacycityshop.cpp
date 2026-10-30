#include "aulegacycityshop.h"
#include "aulegacycityproduction.h"
#include "aulegacycitystate.h"
#include "augear.h"
#include <cmath>
#include <limits>

namespace AuLegacyCity
{
bool PackPersonalItem(AuGear& gear, const AuItem& item)
{
	if(!item.CanBeCarried()) return false;
	for(const AuGearSlot::SLOTTYPE slot : {AuGearSlot::Body, AuGearSlot::Hand, AuGearSlot::Waist})
		for(auto& stored : gear)
			if(stored.SlotType() == slot && stored.Item().FitItemIntoContainer(item)) return true;
	AuGearSlot::SLOTTYPE slot = AuGearSlot::Invalid;
	quint8 which = 0;
	if(!gear.ItemsInSlot(AuGearSlot::Body, &which)) slot = AuGearSlot::Body;
	else if(gear.ItemsInSlot(AuGearSlot::Hand, &which) < 2) slot = AuGearSlot::Hand;
	else if(item.PackSize().width() * item.PackSize().height() <= 15
		&& gear.ItemsInSlot(AuGearSlot::Waist, &which) < 4) slot = AuGearSlot::Waist;
	if(slot == AuGearSlot::Invalid) return false;
	gear.Insert(slot, which, item);
	return true;
}

quint32 SaleStock(const AuItem& item, const AuLegacyCityCatalogue& catalogue,
	const State& state, bool fauna, bool flora)
{
	const CommodityRecord* commodity = catalogue.CommodityForRuntimeId(item.CommodityId());
	if(!commodity) return 0;
	// TxForSaleChat, 0x2d6de8: retain life support and a food reserve.
	quint64 reserve = 0;
	if(!fauna && (commodity->id.value == 5 || commodity->id.value == 83)) reserve = quint64(qMax(0, state.population)) * 13;
	else if(item.IsFood() && item.FoodNutritionValue()) reserve = quint64(qMax(0, state.population)) * (flora ? 1 : 10);
	return item.Count() > reserve ? quint32(item.Count() - reserve) : 0;
}

bool SalesFilter(AuCommPacket::TYPE type, COMMODITYFILTER& filter)
{
	switch(type)
	{
	case AuCommPacket::SolicitAllSales: case AuCommPacket::SolicitItemSales: filter = FILTER_All; break;
	case AuCommPacket::SolicitAmmoSales: filter = FILTER_Ammo; break;
	case AuCommPacket::SolicitArmorSales: filter = FILTER_ArmorAndClothes; break;
	case AuCommPacket::SolicitConstructionMaterialSales: filter = FILTER_ConstructionMaterials; break;
	case AuCommPacket::SolicitFoodSales: filter = FILTER_Food; break;
	case AuCommPacket::SolicitFuelSales: filter = FILTER_Fuel; break;
	case AuCommPacket::SolicitMedSales: filter = FILTER_MedicalSupplies; break;
	case AuCommPacket::SolicitMiscSales: filter = FILTER_Misc; break;
	case AuCommPacket::SolicitNaturalResourceSales: filter = FILTER_NaturalResources; break;
	case AuCommPacket::SolicitPartsSales: filter = FILTER_SpareParts; break;
	case AuCommPacket::SolicitPatentSales: filter = FILTER_Patents; break;
	case AuCommPacket::SolicitToolSales: filter = FILTER_Tools; break;
	case AuCommPacket::SolicitWeaponSales: filter = FILTER_Weapons; break;
	default: return false;
	}
	return true;
}

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

quint32 ShopDemandCost(const AuLegacyCityCatalogue& catalogue, const State& city,
	const AuItemHash& inventory, const ProductionState& production, const AuItem& item)
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
	return quint32(cost < 1.f ? 1 : cost >= double(maximumPrice) ? maximumPrice : quint64(cost));
}

quint32 ShopUnitPrice(const AuLegacyCityCatalogue& catalogue, const State& city,
	const AuItemHash& inventory, const ProductionState& production, const AuItem& item, quint8 industry)
{
	const quint64 demandCost = ShopDemandCost(catalogue, city, inventory, production, item);
	if(!demandCost) return 0;
	const quint64 maximumPrice = std::numeric_limits<quint32>::max();
	quint64 price = demandCost * 120 / 100;
	if(price == demandCost) ++price;
	if(industry == 105) price *= 2;
	return quint32(qMin(maximumPrice, price));
}
}
