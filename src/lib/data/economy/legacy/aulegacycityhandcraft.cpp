#include "aulegacycityhandcraft.h"
#include "auzoneresource.h"

namespace AuLegacyCity
{
bool HandcraftAllowed(const ManufacturingProcessRecord& recipe, const HandcraftContext& context)
{
	return recipe.craft == 1 || (recipe.craft == 2 && context.atIndustry && recipe.industry.value == context.industry);
}

bool PrepareHandcraft(const AuLegacyCityCatalogue& catalogue, quint16 recipeId,
	const AuGear& gear, const HandcraftContext& context, ProductionProcess& process)
{
	// DaPerson::HandcraftItem 0x331090 and DeManufacturingProcess's default
	// constructor 0x1fab0: outside the matching industry, product TL is zero.
	const auto* recipe = catalogue.ManufacturingProcess(ManufacturingProcessId(recipeId));
	if(!recipe || !HandcraftAllowed(*recipe, context)) return false;
	const quint8 technology = context.atIndustry && context.industry == recipe->industry.value ? context.technology : 0;
	if(!InitializeProductionProcess(catalogue, process, recipeId, technology, false)) return false;
	for(auto& component : process.components)
	{
		if(component.effect != 5) continue;
		if(component.commodityId == 3)
		{
			if(context.hasPower)
			{ component.applied = qMax(1U, component.required); component.quality = 255; component.technology = 31; }
			continue;
		}
		const auto runtime = COMMODITYID(catalogue.RuntimeCommodityId(CommodityId(component.commodityId)));
		const quint32 needed = qMax(1U, component.required);
		quint32 found = 0; quint8 quality = 0, materialTechnology = 0;
		auto inspect = [&](const AuItem& item)
		{
			if(found >= needed || item.CommodityId() != runtime || !item.Count()) return;
			found += qMin(needed - found, item.Count());
			if(!quality || item.Quality() < quality) quality = item.Quality();
			// Original zero TL is also the accumulator's unset sentinel.
			if(!materialTechnology || item.TechLevel() < materialTechnology) materialTechnology = item.TechLevel();
			if(component.required && process.species.IsNull() && !item.DNASpecies().IsNull())
			{ process.species = item.DNASpecies(); process.sentient = item.DNAIsSentient(); }
		};
		for(const auto& slot : gear)
		{
			if(found >= needed) break;
			inspect(slot.Item());
			if(const auto* contents = slot.Item().Content())
				for(const auto& entry : *contents) { if(found >= needed) break; inspect(*entry.second); }
		}
		if(found) { component.applied = found; component.quality = quality; component.technology = materialTechnology; }
	}
	return true;
}

void ConsumeHandcraft(const AuLegacyCityCatalogue& catalogue, const ProductionProcess& process, AuGear& gear)
{
	for(const auto& component : process.components)
	{
		if(component.effect != 5 || !component.required || component.commodityId == 3) continue;
		const auto runtime = COMMODITYID(catalogue.RuntimeCommodityId(CommodityId(component.commodityId)));
		quint32 remaining = component.required;
		for(auto slot = gear.begin(); slot != gear.end() && remaining;)
		{
			if(slot->Item().CommodityId() == runtime)
			{
				remaining -= slot->Item().Consume(remaining);
				if(!slot->Item().Count()) { slot = gear.erase(slot); continue; }
			}
			if(auto* contents = slot->Item().Content())
				for(auto entry = contents->begin(); entry != contents->end() && remaining;)
				{
					if(entry->second->CommodityId() == runtime) remaining -= entry->second->Consume(remaining);
					if(!entry->second->Count()) entry = contents->Erase(entry); else ++entry;
				}
			++slot;
		}
	}
}

bool FinishHandcraft(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process, ProductionOutput& output)
{
	if(!CanRunProduction(process)) return false;
	// DeManufacturingProcess::RunHandCraft 0x44c70 finishes immediately and
	// caps historical quantities to one, even for factory batch recipes. The
	// added arrows retain their authored batch, matching the modern weapon recipe.
	AuZoneResource resources;
	process.completeAt = 1;
	if(!FinishProduction(catalogue, process, resources, 1, output)) return false;
	if(catalogue.RuntimeCommodityId(CommodityId(output.commodityId)) != CID_Arrows)
		output.quantity = qMin(1U, output.quantity);
	process.lastQuantity = qint32(output.quantity);
	return true;
}
}
