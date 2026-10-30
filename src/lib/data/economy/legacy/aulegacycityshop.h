#ifndef AuLegacyCityShop_H
#define AuLegacyCityShop_H

#include "aulegacycitycatalogue.h"
#include "auitem.h"
#include "aucommpacket.h"

class AuGear;

namespace AuLegacyCity
{
struct State;
struct ProductionState;
AUDATA_DLL bool SalesFilter(AuCommPacket::TYPE type, COMMODITYFILTER& filter);
// Pack a private gear transaction without actor/environment callbacks.
AUDATA_DLL bool PackPersonalItem(AuGear& gear, const AuItem& item);
AUDATA_DLL quint32 SaleStock(const AuItem& item, const AuLegacyCityCatalogue& catalogue,
	const State& state, bool fauna, bool flora);

// Original on-world city pricing. The caller owns the twenty-minute offer
// cache and verifies Legacy metadata activation before reading inventories.
AUDATA_DLL quint32 ShopUnitPrice(const AuLegacyCityCatalogue& catalogue, const State& city,
	const AuItemHash& inventory, const ProductionState& production, const AuItem& item, quint8 industry);
AUDATA_DLL quint32 ShopDemandCost(const AuLegacyCityCatalogue& catalogue, const State& city,
	const AuItemHash& inventory, const ProductionState& production, const AuItem& item);

inline bool HasShop(const AuLegacyCityCatalogue& catalogue, quint8 industry)
{
	switch(industry)
	{
	case 3: case 101: case 103: case 104: case 105: case 112: return true;
	default: return !catalogue.ProcessesForIndustry(IndustryId(industry)).isEmpty();
	}
}

// DaCity::DoesIndustrySellItem, AuAtom13 0x28e000. The category bits
// belong to the GD commodity, independent of the modern industry catalogue.
inline bool IndustrySellsCommodity(const AuLegacyCityCatalogue& catalogue, quint8 industry,
	CommodityId commodity, ECOROLE cityDiet)
{
	const CommodityRecord* record = catalogue.Commodity(commodity);
	if(!record) return false;
	const DbCommodity* runtime = catalogue.RuntimeCommodity(catalogue.RuntimeCommodityId(commodity));
	if(!runtime) return false;
	const DbCommodity& descriptor = *runtime;
	switch(industry)
	{
	case 3:
		return commodity.value == 112 || commodity.value == 113 || commodity.value == 282
			|| commodity.value == 287 || commodity.value == 288;
	case 101: return descriptor.IsBrokerItem();
	case 103:
		return descriptor.IsGroceryItem() && (!descriptor.CanBeEaten()
			|| descriptor.IsIntoxicant() || descriptor.IsFood(cityDiet));
	case 104: return descriptor.IsRetailItem();
	case 105:
		return descriptor.IsCantinaItem() && (!descriptor.CanBeEaten()
			|| descriptor.IsIntoxicant() || descriptor.IsFood(cityDiet));
	case 112: return commodity.value == 113;
	default:
		for(const ManufacturingProcessId processId : catalogue.ProcessesForIndustry(IndustryId(industry)))
		{
			const ManufacturingProcessRecord* process = catalogue.ManufacturingProcess(processId);
			if(process && process->commodity == commodity) return true;
		}
		return false;
	}
}
}

#endif
