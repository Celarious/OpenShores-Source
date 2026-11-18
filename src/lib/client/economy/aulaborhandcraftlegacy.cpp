#include "aulaborhandcraft.h"
#include "auglobal.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityhandcraft.h"
#include "aulegacyhandcraft.h"
#include "daperson.h"

void AuHandCraftPanel::UpdateLegacyContents(DaPerson* person)
{
	// Recipe gathering is generation-specific; category/search rendering is shared.
	UpdateContents(person);
}

bool AuHandCraftPanel::LegacyPlanReady(DaPerson* person, const QList<int>& plan) const
{
	const AuLegacyCityCatalogue* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue || !person || plan.isEmpty() || !g_Au.IsLegacyGeneration()
		|| !g_Au.HasLegacyCommodity(CID_Flag)) return false;
	const AuLegacyCity::HandcraftContext context = AuLegacyCity::QueryHandcraftContext(*person);
	const AuGear* gear = person->LockGearForRead();
	AuGear preview(*gear);
	person->UnlockGear();
	for(int index = 0; index < plan.count(); ++index)
	{
		AuLegacyCity::ProductionProcess process;
		if(!AuLegacyCity::PrepareHandcraft(*catalogue, quint16(plan[index]), preview, context, process)
			|| !AuLegacyCity::CanRunProduction(process)) return false;
		if(index + 1 == plan.count()) return true;

		// Use the execution primitives on a copy, so prerequisite tools, quantities
		// and material technology are checked exactly as for an actual handcraft.
		const AuLegacyCity::ManufacturingProcessRecord* recipe = catalogue->ManufacturingProcess(
			AuLegacyCity::ManufacturingProcessId(quint16(plan[index])));
		const AuLegacyCity::CommodityRecord* commodity = recipe ? catalogue->Commodity(recipe->commodity) : nullptr;
		if(!commodity || commodity->IsTechnology() || AuLegacyCity::IsProductionUnit(recipe->commodity.value)
			|| recipe->commodity.value == 157 || recipe->commodity.value == 342 || recipe->commodity.value == 349) return false;
		AuLegacyCity::ProductionOutput output;
		if(!AuLegacyCity::FinishHandcraft(*catalogue, process, output) || !output.quantity) return false;
		AuLegacyCity::ConsumeHandcraft(*catalogue, process, preview);
		const COMMODITYID runtime = COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(output.commodityId)));
		AuItem item(runtime, output.quality, person->EmpireId(), output.quantity);
		if(!item.SetTechLevel(output.technology)) return false;
		// Preview storage is virtual; server inventory placement remains authoritative.
		preview.append(AuGearSlot(AuGearSlot::Hand, 0, item));
	}
	return false;
}
