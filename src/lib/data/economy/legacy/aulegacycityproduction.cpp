#include "aulegacycityproduction.h"
#include "aulegacycityinventory.h"
#include "aulegacyshipperformance.h"
#include "aupowerproduction.h"
#include "audice.h"
#include "auglobal.h"
#include "auzoneresource.h"
#include <QBuffer>
#include <QDataStream>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>

namespace AuLegacyCity
{
namespace
{
constexpr int MaximumProductionBytes = 4 * 1024 * 1024;
constexpr quint32 MaximumProcesses = 65535;
constexpr quint32 MaximumTools = 65535;

bool Bulk(const AuLegacyCityCatalogue& catalogue, const ManufacturingProcessRecord& recipe)
{
	const auto* commodity = catalogue.Commodity(recipe.commodity);
	return recipe.commodity.value == 3 || recipe.commodity.value == 157
		|| (commodity && (commodity->descriptor.m_combits & CB_Cargo)
			&& !IsProductionTechnology(recipe.commodity.value));
}

quint32 BaseLabor(const AuLegacyCityCatalogue& catalogue, const ProductionProcess& process)
{
	const auto* recipe = catalogue.ManufacturingProcess(ManufacturingProcessId(process.processId));
	if(process.spacecraftDesign) return 20; // InitSpacecraft 0x126fb0, before fetched-material labor.
	return recipe ? recipe->labor * (process.bulkManufacturing && Bulk(catalogue, *recipe) ? 10U : 1U) : 0;
}

quint8 NaturalQuality(const AuLegacyCityCatalogue& catalogue, const AuZoneResource& resources, quint16 commodity)
{
	// The original switch did not include the modern patent aliases or resources.
	switch(commodity)
	{
	case 5: case 14: case 15: case 20: case 22: case 23: case 24: case 26:
	case 29: case 30: case 33: case 35: case 37: case 38: case 39: case 42:
	case 43: case 44: case 45: case 52: case 53: case 57: case 58: case 59:
	case 66: case 74: case 76: case 81: case 83: case 86: case 87: case 88:
	case 89: case 94: case 111: case 130: case 134: case 137: case 138:
	case 139: case 140: case 141: case 142: case 143: case 144: case 147:
	case 148: case 271: case 272:
		return resources.Quality(COMMODITYID(catalogue.RuntimeCommodityId(CommodityId(commodity))));
	default: return 0;
	}
}

int NaturalFetchProbability(const AuZoneResource& resources, quint16 commodity)
{
	// AuZoneResource::Fetch 0x1132b0 permits only these eight environmental
	// components. Ore, wood and other products instead use GD output recipes.
	switch(commodity)
	{
	case 87: return resources.m_Atmosphere;
	case 88: return int(resources.m_Zone) >= 1 && int(resources.m_Zone) <= 3 ? resources.m_WaterInEnvironment : 0;
	case 89: return resources.m_Vegetation;
	case 94: return resources.m_Sunlight;
	case 147: return int(resources.m_Zone) == 0 ? resources.m_WaterInEnvironment : 0;
	case 148: return int(resources.m_Zone) == 4 ? resources.m_WaterInEnvironment : 0;
	case 149: return int(resources.m_Zone) == 0 ? resources.m_Atmosphere * int(resources.m_AtmType) / 75 : 0;
	case 150: return int(resources.m_Zone) == 4 ? resources.m_Atmosphere * int(resources.m_AtmType) / 75 : 0;
	default: return 0;
	}
}

quint32 ApplyMaterial(ProductionComponent& component, AuItem& item, quint8 quality, quint8 technology)
{
	if(component.IsSatisfied() || !item.Count()) return 0;
	if(!component.required)
	{
		// Reuse modern item condition and quality tests; both retain the original
		// 3500+2Q out of 4010 probability and condition decrement on failure.
		if(!item.UseItem()) { item.Consume(1); return 1; }
		component.applied = 1;
		return 0;
	}
	quint32 good = 0, used = 0;
	if(item.Count() == 1)
	{
		good = DbCommodity::TestQuality(quality) ? 1 : 0;
		used = 1;
	}
	else
	{
		good = component.required - component.applied;
		used = good * 2 - DbCommodity::TestFailure(quality, good);
		if(used > item.Count())
		{
			// 2016 truncates this proportion. Modern DeProcessComponent rounds up.
			good = quint32(quint64(item.Count()) * good / used);
			used = item.Count();
			if(!good) good = 1;
		}
	}
	if(good)
	{
		if(!component.quality || !component.applied || quality < component.quality) component.quality = quality;
		component.technology = qMin(component.technology, technology);
		component.applied += good;
	}
	item.Consume(used);
	return used;
}

bool ApplyItem(ProductionProcess& process, ProductionComponent& component, AuItem& item,
	quint8 electricityForSunlight, quint8 bottledWater)
{
	if(component.IsSatisfied() || !item.Count()) return false;
	if(component.required && item.TechLevel() < process.minimumTechnology) return false;
	quint8 quality = item.Quality(), technology = item.TechLevel();
	if(electricityForSunlight && component.commodityId == 94)
	{ quality = electricityForSunlight; technology = 31; }
	else if(bottledWater && component.commodityId == 88) quality = bottledWater;
	const quint32 previousCount = item.Count(), previousApplied = component.applied;
	const quint8 previousCondition = item.HitPoints();
	const quint32 labor = ApplyMaterial(component, item, quality, technology);
	process.laborNeeded += labor;
	// Applied inventory is clamped to requested TL on both sides of the call.
	if(!(electricityForSunlight && component.commodityId == 94)
		&& !(bottledWater && component.commodityId == 88))
		component.technology = qMax(component.technology, process.minimumTechnology);
	if(previousCount != item.Count())
	{
		const AuSpecies species = item.DNASpecies();
		if(!species.IsNull()) { process.species = species; process.sentient = item.DNAIsSentient(); }
	}
	return previousCount != item.Count() || previousApplied != component.applied || previousCondition != item.HitPoints();
}

bool ReadBytes(QDataStream& in, QByteArray& bytes, quint32 maximum)
{
	quint32 count = 0;
	in >> count;
	if(count == 0xffffffffU && in.status() == QDataStream::Ok) { bytes.clear(); return true; }
	if(in.status() != QDataStream::Ok || count > maximum || count > quint64(in.device()->bytesAvailable())) return false;
	bytes.resize(int(count));
	return !count || in.readRawData(bytes.data(), int(count)) == int(count);
}

bool Valid(const AuLegacyCityCatalogue& catalogue, const ProductionState& state, QString& error)
{
	if(state.processes.size() > int(MaximumProcesses) || state.tools.size() > MaximumTools || state.vehicles.size() > 256)
	{ error = QStringLiteral("Legacy production collection exceeds its limit."); return false; }
	QSet<quint64> identities;
	for(const auto& process : state.processes)
	{
		const auto* recipe = catalogue.ManufacturingProcess(ManufacturingProcessId(process.processId));
		const quint64 identity = (quint64(process.developmentId) << 16) | process.slot;
		if(!process.developmentId || !recipe || identities.contains(identity)
			|| process.minimumTechnology < recipe->minimumTechnology || process.minimumTechnology > 31
			|| process.buildingTechnology > 31 || process.lastTechnology > 31 || process.productionBoost < 0
			|| process.components.size() > 128 || process.species.Key().size() > 4096
			|| process.laborNeeded > 1000000000U || process.completeAt < 0 || process.lastWorked < 0)
		{ error = QStringLiteral("Invalid legacy production process."); return false; }
		identities.insert(identity);
		ProductionProcess expected;
		InitializeProductionProcess(catalogue, expected, process.processId, process.buildingTechnology, process.bulkManufacturing);
		if(process.spacecraftDesign)
		{
			if(process.processId != 373 || process.slot || process.spacecraftName.size() > 128
				|| process.components.isEmpty()
				|| process.spacecraftMinimumTechnology > process.minimumTechnology
				|| !std::isfinite(process.spacecraftEstimatedCost) || process.spacecraftEstimatedCost < 0
				|| process.spacecraftRetryAt < 0 || (process.spacecraftRetryAt && process.completeAt))
			{ error = QStringLiteral("Invalid legacy spacecraft manufacturing state."); return false; }
		}
		else if(process.spacecraftRetryAt || process.spacecraftMinimumTechnology
			|| !process.spacecraftName.isEmpty() || process.spacecraftEstimatedCost)
		{ error = QStringLiteral("Unexpected legacy spacecraft manufacturing state."); return false; }
		if(!process.spacecraftDesign && expected.components.size() != process.components.size())
		{ error = QStringLiteral("Legacy production component count disagrees with GD."); return false; }
		for(int i = 0; i < process.components.size(); ++i)
		{
			const auto& component = process.components[i];
			const auto& original = process.spacecraftDesign ? component : expected.components[i];
			const quint32 maximumApplied = component.commodityId == 157 && IsProductionTechnology(recipe->commodity.value)
				? 32000U : qMax(1U, component.required);
			if(!catalogue.Commodity(CommodityId(component.commodityId))
				|| (process.spacecraftDesign && (component.effect != 5 || component.required > 1000000000U))
				|| component.commodityId != original.commodityId || component.effect != original.effect
				|| component.required != original.required || component.technology > 31
				|| component.applied > maximumApplied)
			{ error = QStringLiteral("Invalid legacy production component."); return false; }
			if(process.spacecraftDesign)
				for(int previous = 0; previous < i; ++previous)
					if(process.components[previous].commodityId == component.commodityId)
					{ error = QStringLiteral("Duplicate legacy spacecraft material."); return false; }
		}
		if(process.completeAt)
		{
			for(const auto& component : process.components)
				if(component.effect == 5 && !component.IsSatisfied())
				{ error = QStringLiteral("Running legacy process has incomplete materials."); return false; }
		}
	}
	for(const auto& pair : state.tools)
		if(!pair.second || !catalogue.CommodityForRuntimeId(quint16(pair.first)) || !pair.second->Count()
			|| pair.second->Count() > 1000000000U || !pair.second->Quality() || pair.second->Content())
		{ error = QStringLiteral("Invalid legacy production tool."); return false; }
	for(quint16 vehicle : state.vehicles)
		if(!IsProductionUnit(vehicle)) { error = QStringLiteral("Invalid legacy city vehicle type."); return false; }
	return true;
}
}

bool IsProductionTechnology(quint16 commodity)
{
	return (commodity >= 158 && commodity <= 225) || commodity == 286 || commodity == 348;
}

bool IsProductionUnit(quint16 commodity)
{
	switch(commodity)
	{
	case 6: case 7: case 8: case 9: case 28: case 54: case 70: case 71:
	case 77: case 82: case 103: case 104: case 132: case 133: case 231: return true;
	default: return false;
	}
}

bool ProductionDNAIsViable(quint16 commodity, const AuSpecies& species)
{
	if(species.IsNull()) return false;
	// AuData13 DeManufacturingProcess::DNAIsViable, 0x2ead0.
	switch(commodity)
	{
	case 14: case 20: case 26: case 33: case 49: case 52: case 100: case 233: return true;
	default: return false;
	}
}

quint8 ProductionFarmAnimalLimit(quint8 industry, quint16 commodity)
{
	// DpBuilding::Tick 0x5f22c0. Cheese and fertilizer need livestock only
	// when made directly at a farm, unlike processing their cargo inputs.
	if(commodity == 14) return 2;
	if(commodity == 26 || commodity == 52 || (industry == 30 && (commodity == 20 || commodity == 29))) return 1;
	return 0;
}

bool ApplyFarmAnimalResult(ProductionOutput& output, quint8 status, const AuSpecies& species)
{
	if(status > 1 || species.IsNull()) return false;
	output.species = species;
	output.sentient = false;
	if(status == 0 && output.quantity) --output.quantity;
	// Deliberately retain viability derived from process input DNA. The
	// original did not recompute it from the selected livestock's DNA.
	return true;
}

bool ProductionIsCriticalToSurvival(const AuLegacyCityCatalogue& catalogue, quint16 commodity, bool planetCanHaveFauna)
{
	// DeManufacturingProcess::IsCriticalToSurvival 0x127760.
	if(!planetCanHaveFauna && (commodity == 3 || commodity == 5 || commodity == 83)) return true;
	const auto* record = catalogue.Commodity(CommodityId(commodity));
	return record && record->descriptor.m_food && (record->descriptor.m_combits & CB_Food);
}

bool HasProductionTool(const AuLegacyCityCatalogue& catalogue, quint16 commodity,
	const ProductionState& production, const AuItemHash& inventory)
{
	if(production.vehicles.contains(commodity)) return true;
	const auto runtime = COMMODITYID(catalogue.RuntimeCommodityId(CommodityId(commodity)));
	return production.tools.FindBest(runtime) != production.tools.cend() || inventory.FindBest(runtime) != inventory.cend();
}

bool UseProductionTool(const AuLegacyCityCatalogue& catalogue, quint16 commodity,
	ProductionState& production, AuItemHash& inventory)
{
	if(production.vehicles.contains(commodity)) return true;
	const auto runtime = COMMODITYID(catalogue.RuntimeCommodityId(CommodityId(commodity)));
	auto tool = production.tools.FindBest(runtime);
	if(tool == production.tools.end())
	{
		auto stored = inventory.FindBest(runtime);
		if(stored == inventory.end()) return false;
		production.tools.AddItemCount(*stored->second, 1);
		stored->second->Consume(1);
		if(!stored->second->Count()) inventory.Erase(stored);
		tool = production.tools.FindBest(runtime);
	}
	if(tool == production.tools.end()) return false;
	// ApplyToolOrVehicle 0x26ff00 grants this effort even on its final use.
	if(!tool->second->UseItem()) tool->second->Consume(1);
	if(!tool->second->Count()) production.tools.Erase(tool);
	return true;
}

quint16 IndustryTechnologyCommodity(quint8 industry)
{
	// AuAtom13.dll 0x1807cdeb0, 130 original little-endian DWORDs.
	static const quint16 table[130] = {
		0,0,0,177,219,0,0,0,0,0,0,186,175,0,0,0,0,0,0,0,
		189,188,162,0,0,0,0,0,0,0,181,193,201,0,222,348,0,0,0,0,
		165,167,178,184,194,196,214,216,224,0,197,221,0,0,0,0,0,0,0,0,
		174,183,190,191,199,200,212,223,0,0,158,164,170,173,176,179,180,220,192,195,
		198,203,204,206,208,210,211,213,218,217,0,0,0,0,0,0,0,0,0,0,
		166,168,171,185,207,215,0,0,0,0,160,161,163,225,202,0,0,0,0,0,
		172,187,182,169,205,209,0,286,159,0
	};
	return industry < 130 ? table[industry] : 0;
}

quint8 TechnologyProductIndustry(quint16 commodity)
{
	// AuAtom13.dll 0x1807cd930 was checked against the complete inverse table.
	for(quint8 industry = 1; industry < 130; ++industry)
		if(IndustryTechnologyCommodity(industry) == commodity) return industry;
	return 0;
}

quint8 TechnologyOfProduct(quint8 producingIndustry, quint8 buildingTechnology,
	quint16 commodity, const QVector<Development>& developments)
{
	// University technology itself (209) is deliberately outside this switch.
	if(producingIndustry != 125 || !((commodity >= 158 && commodity <= 208)
		|| (commodity >= 210 && commodity <= 225) || commodity == 286 || commodity == 348)) return buildingTechnology;
	const quint8 industry = TechnologyProductIndustry(commodity);
	quint8 highest = 0;
	bool exists = false;
	for(const auto& development : developments)
		if(development.industryId == industry && !development.demolished && development.completedLevels)
		{ highest = qMax(highest, development.technology); exists = true; }
	return exists ? qMin(buildingTechnology, highest) : 0;
}

quint16 DefaultProductionRecipe(const AuLegacyCityCatalogue& catalogue, quint8 industry,
	const std::function<bool(quint16)>& resourceAtSite)
{
	const quint16 defaultRecipe = catalogue.DefaultManufacturingProcess(IndustryId(industry)).value;
	if(industry != 50 && industry != 51) return defaultRecipe;
	for(const auto& recipe : catalogue.ManufacturingProcesses())
		if(recipe.industry.value == industry && recipe.commodity.value != (industry == 50 ? 76 : 83)
			&& resourceAtSite && resourceAtSite(recipe.commodity.value)) return recipe.id.value;
	return defaultRecipe;
}

QVector<ManufacturingProcessId> ProductionRecipeChoices(const AuLegacyCityCatalogue& catalogue,
	quint8 industry, const std::function<bool(quint16)>& resourceAtSite, bool waterPresent, quint8 orbitZone)
{
	// Shores of Hazeron.exe FUN_1401d2570: resource restrictions apply only
	// to the selected mine, well and water treatment plant's F10 choices.
	QVector<ManufacturingProcessId> choices;
	for(const auto& recipe : catalogue.ManufacturingProcesses())
	{
		if(recipe.industry.value != industry) continue;
		if(industry == 50 || industry == 51)
		{
			if(!IsProductionTechnology(recipe.commodity.value)
				&& (!resourceAtSite || !resourceAtSite(recipe.commodity.value))) continue;
		}
		else if(industry == 89)
		{
			const quint16 environmentRecipe = orbitZone == 0 ? 252 : orbitZone == 4 ? 253 : 170;
			if(!IsProductionTechnology(recipe.commodity.value) && recipe.id.value != 171
				&& (!waterPresent || recipe.id.value != environmentRecipe)) continue;
		}
		choices.append(recipe.id);
	}
	return choices;
}

QVector<ProductionOutput> RefundProductionMaterials(const AuLegacyCityCatalogue& catalogue,
	const ProductionProcess& process, quint32& researchMoney)
{
	researchMoney = 0;
	QVector<ProductionOutput> output;
	for(const auto& component : process.components)
	{
		if(!component.required || !component.applied) continue;
		if(component.commodityId == 157) researchMoney += component.applied;
		else
		{
			const auto* commodity = catalogue.Commodity(CommodityId(component.commodityId));
			if(!commodity || !commodity->descriptor.CanBeCargo()) continue;
			ProductionOutput refund;
			refund.commodityId = component.commodityId;
			refund.quantity = component.applied;
			refund.quality = component.quality;
			refund.technology = component.technology;
			output.append(refund);
		}
	}
	return output;
}

bool InitializeProductionProcess(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process,
	quint16 recipeId, quint8 buildingTechnology, bool bulkManufacturing)
{
	const auto* recipe = catalogue.ManufacturingProcess(ManufacturingProcessId(recipeId));
	if(!recipe || buildingTechnology > 31) return false;
	ProductionProcess replacement;
	replacement.developmentId = process.developmentId;
	replacement.slot = process.slot;
	replacement.enabled = process.enabled;
	replacement.lastWorked = process.lastWorked;
	replacement.processId = recipeId;
	replacement.minimumTechnology = recipe->minimumTechnology;
	replacement.buildingTechnology = buildingTechnology;
	replacement.bulkManufacturing = bulkManufacturing;
	const bool bulk = bulkManufacturing && Bulk(catalogue, *recipe);
	for(const auto& requirement : catalogue.ComponentsForProcess(ManufacturingProcessId(recipeId)))
	{
		quint32 quantity = requirement.quantity;
		if(requirement.commodity.value == 157) quantity *= 100;
		else if(bulk && requirement.effect == 5 && quantity) quantity *= 10;
		bool merged = false;
		for(auto& component : replacement.components)
			if(component.commodityId == requirement.commodity.value && component.effect == requirement.effect)
			{ component.required += quantity; merged = true; break; }
		if(!merged)
		{
			ProductionComponent component;
			component.commodityId = requirement.commodity.value;
			component.effect = requirement.effect;
			component.required = quantity;
			replacement.components.append(component);
		}
	}
	replacement.laborNeeded = BaseLabor(catalogue, replacement);
	SetProductionTechnology(catalogue, replacement, buildingTechnology);
	process = replacement;
	return true;
}

bool InitializeSpacecraftProduction(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process,
	quint32 designId, const AuLegacyShip::Design& design, quint8 minimumTechnology, quint8 buildingTechnology)
{
	if(!designId || minimumTechnology > 31 || buildingTechnology > 31 || process.slot) return false;
	// The design's armor/mass/equipment floor also constrains production; a
	// materials-only floor can otherwise admit a hull too advanced for TL32.
	const auto performance = AuLegacyShip::CalculatePerformance(design, AuLegacyShip::CalculateStats(design), 0);
	const auto bill = AuLegacyShip::CalculateManufacturing(design, qMax(int(minimumTechnology), performance.minimumTechLevel));
	if(!bill.valid || bill.techLevel > 31 || bill.materials.isEmpty()) return false;
	ProductionProcess replacement;
	replacement.developmentId = process.developmentId; replacement.slot = process.slot;
	replacement.enabled = process.enabled; replacement.lastWorked = process.lastWorked;
	replacement.processId = 373; replacement.bulkManufacturing = false;
	replacement.spacecraftDesign = designId; replacement.spacecraftName = design.header.name.left(128);
	replacement.spacecraftMinimumTechnology = quint8(performance.minimumTechLevel);
	replacement.spacecraftEstimatedCost = bill.estimatedCost;
	replacement.minimumTechnology = quint8(bill.techLevel); replacement.buildingTechnology = buildingTechnology;
	replacement.laborNeeded = 20;
	for(const auto& material : bill.materials)
	{
		if(material.quantity > 1000000000U || !catalogue.Commodity(CommodityId(material.commodity))) return false;
		ProductionComponent component;
		component.commodityId = material.commodity; component.effect = 5;
		component.required = quint32(material.quantity); replacement.components.append(component);
	}
	process = replacement;
	return true;
}

float ActualProductionCost(const AuLegacyCityCatalogue& catalogue, const ProductionProcess& process)
{
	// DeManufacturingProcess::ActualCost 0x125ba0 prices the applied inputs,
	// including their actual quality/TL, then rounds to the nearest thousand.
	float cost = 0;
	for(const auto& component : process.components)
		if(component.required && component.applied)
		{
			const auto runtime = COMMODITYID(catalogue.RuntimeCommodityId(CommodityId(component.commodityId)));
			const float unit = g_Au.CommodityQualityCost(runtime, component.quality, component.technology);
			cost += (unit + 1.f) * component.applied;
		}
	return std::round(cost / 1000.f) * 1000.f;
}

void SetProductionTechnology(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process, quint8 technology)
{
	if(technology > 31) return;
	process.buildingTechnology = technology;
	const auto* recipe = catalogue.ManufacturingProcess(ManufacturingProcessId(process.processId));
	if(recipe && !process.spacecraftDesign && IsProductionTechnology(recipe->commodity.value))
		for(auto& component : process.components)
			if(component.commodityId == 157) { component.required = (quint32(technology) + 1) * 1000; break; }
}

void ApplyBuildingTechnologyChange(const AuLegacyCityCatalogue& catalogue, State& city,
	ProductionState& production, quint32 developmentId, quint8 technology)
{
	auto development = std::find_if(city.developments.begin(), city.developments.end(), [developmentId](const Development& value) {
		return value.id == developmentId;
	});
	if(development == city.developments.end() || technology > 31 || development->technology == technology) return;
	development->technology = technology;
	const quint8 industry = development->industryId;
	// 0x2d1ef0 updates every process at this building, even where its capped
	// product TL did not change. Universities elsewhere update only on a change.
	for(auto& process : production.processes)
	{
		const auto site = std::find_if(city.developments.cbegin(), city.developments.cend(), [&process](const Development& value) {
			return value.id == process.developmentId;
		});
		if(site == city.developments.cend()) continue;
		const bool sameSite = site->id == developmentId;
		if(!sameSite && (industry == 125 || site->industryId != 125 || site->demolished)) continue;
		const auto* recipe = catalogue.ManufacturingProcess(ManufacturingProcessId(process.processId));
		if(!recipe) continue;
		const auto productTechnology = TechnologyOfProduct(site->industryId, site->technology, recipe->commodity.value, city.developments);
		if(!sameSite && process.buildingTechnology == productTechnology) continue;
		SetProductionTechnology(catalogue, process, productTechnology);
		if(!IsProductionTechnology(recipe->commodity.value) && (!sameSite || industry != 87))
			ResetProduction(catalogue, process);
	}
	city.lastEmployment = 0;
}

void UpgradeBuildingsFromInventory(const AuLegacyCityCatalogue& catalogue, State& city,
	AuItemHash& inventory, ProductionState& production, quint32 priorityDevelopmentId)
{
	// 0x2d24e0 visits the deposit's building first, then the complete LL map
	// (including that building again). This order decides who receives patents.
	QVector<int> sites;
	for(int index = 0; index < city.developments.size(); ++index) sites.append(index);
	std::stable_sort(sites.begin(), sites.end(), [&city](int a, int b) { return city.developments[a].location < city.developments[b].location; });
	if(priorityDevelopmentId)
		for(int index = 0; index < city.developments.size(); ++index)
			if(city.developments[index].id == priorityDevelopmentId) { sites.prepend(index); break; }
	for(const int index : sites)
	{
		const auto& development = city.developments[index];
		if(development.demolished || !development.completedLevels || development.technology == 31) continue;
		const auto commodity = IndustryTechnologyCommodity(development.industryId);
		if(!commodity) continue;
		const auto runtime = COMMODITYID(catalogue.RuntimeCommodityId(CommodityId(commodity)));
		auto patent = inventory.FindBest(runtime, city.controllingEmpire);
		if(patent != inventory.end() && patent->second->TechLevel() > development.technology)
		{
			const auto technology = patent->second->TechLevel();
			patent->second->Consume(1); if(!patent->second->Count()) inventory.Erase(patent);
			ApplyBuildingTechnologyChange(catalogue, city, production, development.id, technology);
		}
		patent = inventory.FindBest(runtime);
		if(patent != inventory.end() && patent->second->TechLevel() > development.technology)
		{
			const auto levels = qMin(patent->second->Count(), quint32(patent->second->TechLevel() - development.technology));
			const auto technology = quint8(development.technology + levels);
			patent->second->Consume(levels); if(!patent->second->Count()) inventory.Erase(patent);
			ApplyBuildingTechnologyChange(catalogue, city, production, development.id, technology);
		}
	}
}

bool ApplyProductionMaterial(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process,
	AuItem& item, quint8 electricityForSunlight, quint8 bottledWater)
{
	CommodityId historical;
	if(process.completeAt || !catalogue.HistoricalCommodityId(quint16(item.CommodityId()), historical)) return false;
	for(auto& component : process.components)
		if(!component.IsSatisfied() && (component.commodityId == historical.value
			|| (component.commodityId == 94 && historical.value == 3 && electricityForSunlight)
			|| (component.commodityId == 88 && historical.value == 83 && bottledWater)))
			return ApplyItem(process, component, item, electricityForSunlight, bottledWater);
	return false;
}

namespace
{
enum class ProcurementContext { Manufacturing, Construction };

bool FetchMaterials(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process,
	AuItemHash& inventory, ProductionState& state, const AuZoneResource& resources, double moneyAvailable,
	quint32& moneyTaken, bool ringworld, quint8 electricityForSunlight, quint8 bottledWater, ProcurementContext context)
{
	moneyTaken = 0;
	if(process.spacecraftRetryAt) return false;
	bool changed = false;
	bool ringworldSupplied = false;
	for(auto& component : process.components)
	{
		if(component.IsSatisfied()) continue;
		const int chance = NaturalFetchProbability(resources, component.commodityId);
		if(chance > 0 && g_Au.Dice().Roll(1, 100) <= chance)
		{
			const quint16 runtime = catalogue.RuntimeCommodityId(CommodityId(component.commodityId));
			const quint8 quality = NaturalQuality(catalogue, resources, component.commodityId);
			AuItem item(COMMODITYID(runtime), quality, 0, 1);
			// Natural resources have one condition point in the recovered code.
			item.SetHitPoints(1);
			const quint8 technology = qMin(quint8(quality >> 3), process.buildingTechnology);
			const quint32 applied = component.applied;
			process.laborNeeded += ApplyMaterial(component, item, quality, technology);
			changed |= applied != component.applied || !item.Count();
		}
		if(component.commodityId == 157 && component.required > component.applied
			&& std::isfinite(moneyAvailable) && moneyAvailable > 0)
		{
			const quint32 taken = quint32(qMin(double(component.required - component.applied), std::floor(moneyAvailable)));
			component.quality = 255;
			component.technology = 31;
			component.applied += taken;
			process.laborNeeded += taken * 4;
			moneyAvailable -= taken;
			moneyTaken += taken;
			changed |= taken != 0;
		}
		if(ringworld && !ringworldSupplied && !component.IsSatisfied()
			&& (component.commodityId == 3 || (component.commodityId == 94 && electricityForSunlight)))
		{
			component.quality = component.commodityId == 94 ? electricityForSunlight : 255;
			component.technology = 31;
			component.applied = qMax(1U, component.required);
			ringworldSupplied = true;
			changed = true;
		}
		// Both original FetchVehicles implementations write the Q192/TL0
		// pair, and only apply a vehicle when this component has no units yet.
		if(!component.applied && state.vehicles.contains(component.commodityId))
		{ component.quality = 192; component.technology = 0; component.applied = 1; changed = true; }
	}
	// FetchMaterials repeats until one full pass makes no progress, including
	// retrying tools that break and selecting another stack after exhaustion.
	bool progress;
	do
	{
		progress = false;
		for(auto& component : process.components)
		{
			if(component.IsSatisfied() || component.commodityId == 157) continue;
			quint16 historical = component.commodityId;
			if(historical == 94 && electricityForSunlight) historical = 3;
			else if(historical == 88 && bottledWater) historical = 83;
			const COMMODITYID runtime = COMMODITYID(catalogue.RuntimeCommodityId(CommodityId(historical)));
			AuItemHash& source = component.required ? inventory : state.tools;
			// DeConstructionProcess::FetchMaterials 0x117a70 calls 0xc6a60
			// for consumed stock (lowest TL, then lowest Q), preserving good
			// materials for manufacturing. Both paths use the best tools.
			auto item = context == ProcurementContext::Construction && component.required
				? source.FindWorst(runtime) : source.FindBest(runtime);
			if(item == source.end() && !component.required)
			{
				auto stock = inventory.FindBest(runtime);
				if(stock != inventory.end())
				{
					source.AddItemCount(*stock->second, 1);
					stock->second->Consume(1);
					if(!stock->second->Count()) inventory.Erase(stock);
					item = source.FindBest(runtime);
					progress = true;
				}
			}
			if(item == source.end()) continue;
			if(ApplyItem(process, component, *item->second, electricityForSunlight, bottledWater)) progress = true;
			if(!item->second->Count()) source.Erase(item);
		}
		changed |= progress;
	} while(progress);
	return changed;
}
}

bool FetchProductionMaterials(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process,
	AuItemHash& inventory, ProductionState& state, const AuZoneResource& resources, double moneyAvailable,
	quint32& moneyTaken, bool ringworld, quint8 electricityForSunlight, quint8 bottledWater)
{
	return FetchMaterials(catalogue, process, inventory, state, resources, moneyAvailable, moneyTaken,
		ringworld, electricityForSunlight, bottledWater, ProcurementContext::Manufacturing);
}

bool FetchConstructionMaterials(const AuLegacyCityCatalogue& catalogue, ProductionProcess& components,
	AuItemHash& inventory, ProductionState& state, const AuZoneResource& resources, bool ringworld)
{
	quint32 unusedMoney = 0;
	return FetchMaterials(catalogue, components, inventory, state, resources, 0, unusedMoney,
		ringworld, 0, 0, ProcurementContext::Construction);
}

bool CanRunProduction(const ProductionProcess& process)
{
	if(!process.processId || process.completeAt || process.spacecraftRetryAt
		|| process.minimumTechnology > process.buildingTechnology) return false;
	for(const auto& component : process.components)
		if(component.effect == 5 && !component.IsSatisfied()) return false;
	return true;
}

bool RunProduction(ProductionProcess& process, qint64 now)
{
	if(!CanRunProduction(process) || now <= 0) return false;
	quint32 laborBoost = 0;
	int productionBoost = 0;
	for(const auto& component : process.components)
		if(component.applied)
		{
			if(component.effect == 2) laborBoost += 5;
			else if(component.effect == 6) ++productionBoost;
		}
	process.productionBoost = qint8(qMin(127, productionBoost));
	const quint64 seconds = (quint64(process.laborNeeded) + laborBoost + 3) / (laborBoost + 4);
	process.completeAt = now + qint64(seconds * 1000);
	return true;
}

bool FinishProduction(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process,
	const AuZoneResource& resources, qint64 now, ProductionOutput& output)
{
	const auto* recipe = catalogue.ManufacturingProcess(ManufacturingProcessId(process.processId));
	if(!recipe) return false;
	if(process.spacecraftRetryAt)
	{
		if(now < process.spacecraftRetryAt) return false;
		output = ProductionOutput(); output.spacecraftDesign = process.spacecraftDesign;
		output.unit = true; output.quantity = 1; output.quality = process.lastQuality; output.technology = process.lastTechnology;
		return true;
	}
	if(!process.completeAt || now < process.completeAt) return false;
	process.completeAt = 0;
	process.laborNeeded = BaseLabor(catalogue, process);
	ProductionOutput result;
	result.spacecraftDesign = process.spacecraftDesign;
	result.commodityId = process.spacecraftDesign ? 0 : recipe->commodity.value;
	result.research = IsProductionTechnology(result.commodityId);
	result.unit = process.spacecraftDesign || IsProductionUnit(result.commodityId);
	result.species = process.species;
	result.sentient = process.sentient;
	result.viableDNA = ProductionDNAIsViable(result.commodityId, process.species);
	result.technology = process.buildingTechnology;
	if(result.commodityId == 3) { result.quality = 255; result.technology = 31; }
	else
	{
		quint64 qualitySum = 0, quantitySum = 0;
		for(const auto& component : process.components)
			if(component.effect == 5 && component.required)
			{
				if(component.quality && component.applied)
				{ qualitySum += quint64(component.quality) * component.applied; quantitySum += component.applied; }
				result.technology = qMin(result.technology, component.technology);
			}
		const quint8 natural = NaturalQuality(catalogue, resources, result.commodityId);
		if(natural && recipe->naturalResourceQuality)
		{ qualitySum += natural; ++quantitySum; result.technology = qMin(result.technology, quint8(natural >> 3)); }
		if(quantitySum > 1) qualitySum /= quantitySum;
		result.quality = quint8(qMax(quint64(1), qualitySum));
		if(result.research && result.technology < 31 && g_Au.Dice().Roll(1, 100) == 1) ++result.technology;
	}
	result.quantity = process.spacecraftDesign ? 1 : recipe->quantity * (process.bulkManufacturing && Bulk(catalogue, *recipe) ? 10U : 1U);
	if(process.productionBoost > 0)
		result.quantity += qMax(quint32(process.productionBoost), result.quantity * quint32(process.productionBoost) / 10);
	process.lastQuantity = qint32(result.quantity);
	process.lastQuality = result.quality;
	process.lastTechnology = result.technology;
	output = result;
	return true;
}

void ConsumeProductionComponents(ProductionProcess& process)
{
	process.spacecraftRetryAt = 0;
	for(auto& component : process.components) { component.applied = 0; component.technology = 31; }
}

void ResetProduction(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process)
{
	ConsumeProductionComponents(process);
	process.completeAt = 0;
	process.laborNeeded = BaseLabor(catalogue, process);
}

quint32 CurrencyProductionValue(const ProductionOutput& output)
{
	const float value = std::ceil(float(output.quantity) * (float(output.technology) / 10.f + 1.f)
		* float(output.quality) * (1.f / 128.f) * 1.5f);
	return quint32(qBound(1.0, double(value), double(std::numeric_limits<quint32>::max())));
}

void ScalePowerProduction(ProductionOutput& output, quint8 industry, quint8 orbitZone)
{
	if(output.commodityId != 3) return;
	// Power plant industry IDs 66/67 are identical in both catalogues.
	output.quantity = quint32(AuPowerProduction::ScaleForOrbit(qint32(output.quantity), INDUSTRYID(industry), ORBITZONE(orbitZone)));
}

bool EncodeProductionState(const AuLegacyCityCatalogue& catalogue, const ProductionState& state,
	QByteArray& bytes, QString& error)
{
	error.clear();
	if(!Valid(catalogue, state, error)) return false;
	QByteArray encoded;
	QDataStream out(&encoded, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_4_1);
	out << quint16(0x4c50) << quint8(2) << quint32(state.processes.size());
	for(const auto& process : state.processes)
	{
		out << process.developmentId << process.slot << process.processId << process.enabled << process.bulkManufacturing
			<< process.minimumTechnology << process.buildingTechnology << process.laborNeeded << process.completeAt
			<< process.lastWorked << process.productionBoost << process.lastQuantity << process.lastQuality
			<< process.lastTechnology << process.species.Key() << process.sentient << quint16(process.components.size());
		for(const auto& component : process.components)
			out << component.commodityId << component.effect << component.quality << component.technology
				<< component.required << component.applied;
		out << process.spacecraftDesign << process.spacecraftMinimumTechnology << process.spacecraftName
			<< process.spacecraftEstimatedCost << process.spacecraftRetryAt;
	}
	QByteArray tools;
	if(!WriteCityInventory(state.tools, tools, error)) return false;
	out << tools;
	out << quint16(state.vehicles.size());
	for(quint16 vehicle : state.vehicles) out << vehicle;
	if(out.status() != QDataStream::Ok || encoded.size() > MaximumProductionBytes)
	{ error = QStringLiteral("Legacy production data exceeds its byte limit."); return false; }
	bytes = encoded;
	return true;
}

bool DecodeProductionState(const AuLegacyCityCatalogue& catalogue, const QByteArray& bytes,
	ProductionState& state, QString& error)
{
	error.clear();
	if(bytes.isEmpty()) { state = ProductionState(); return true; }
	if(bytes.size() > MaximumProductionBytes) { error = QStringLiteral("Legacy production data exceeds its byte limit."); return false; }
	QDataStream in(bytes);
	in.setVersion(QDataStream::Qt_4_1);
	quint16 magic = 0; quint8 version = 0; quint32 count = 0;
	in >> magic >> version >> count;
	if(in.status() != QDataStream::Ok || magic != 0x4c50 || (version != 1 && version != 2) || count > MaximumProcesses
		|| quint64(count) * 44 > quint64(in.device()->bytesAvailable()))
	{ error = QStringLiteral("Invalid legacy production header."); return false; }
	ProductionState decoded;
	for(quint32 i = 0; i < count; ++i)
	{
		ProductionProcess process;
		quint8 enabled = 0, bulk = 0, sentient = 0; quint16 componentCount = 0;
		QByteArray species;
		in >> process.developmentId >> process.slot >> process.processId >> enabled >> bulk
			>> process.minimumTechnology >> process.buildingTechnology >> process.laborNeeded >> process.completeAt
			>> process.lastWorked >> process.productionBoost >> process.lastQuantity >> process.lastQuality >> process.lastTechnology;
		if(!ReadBytes(in, species, 4096)) { error = QStringLiteral("Invalid legacy production species."); return false; }
		in >> sentient >> componentCount;
		if(in.status() != QDataStream::Ok || enabled > 1 || bulk > 1 || sentient > 1 || componentCount > 128
			|| quint64(componentCount) * 13 > quint64(in.device()->bytesAvailable()))
		{ error = QStringLiteral("Invalid legacy production component count."); return false; }
		process.enabled = enabled;
		process.bulkManufacturing = bulk;
		process.sentient = sentient;
		process.species = AuSpecies(species);
		for(quint16 j = 0; j < componentCount; ++j)
		{
			ProductionComponent component;
			in >> component.commodityId >> component.effect >> component.quality >> component.technology
				>> component.required >> component.applied;
			process.components.append(component);
		}
		if(version >= 2)
			in >> process.spacecraftDesign >> process.spacecraftMinimumTechnology >> process.spacecraftName
				>> process.spacecraftEstimatedCost >> process.spacecraftRetryAt;
		decoded.processes.append(process);
	}
	QByteArray tools;
	if(!ReadBytes(in, tools, MaximumProductionBytes)
		|| !ReadCityInventory(tools, catalogue, decoded.tools, error))
	{ if(error.isEmpty()) error = QStringLiteral("Invalid legacy production tools."); return false; }
	quint16 vehicles = 0;
	in >> vehicles;
	if(in.status() != QDataStream::Ok || vehicles > 256 || quint64(vehicles) * 2 > quint64(in.device()->bytesAvailable()))
	{ error = QStringLiteral("Invalid legacy vehicle count."); return false; }
	for(quint16 i = 0; i < vehicles; ++i) { quint16 vehicle = 0; in >> vehicle; decoded.vehicles.append(vehicle); }
	if(in.status() != QDataStream::Ok || !in.atEnd() || !Valid(catalogue, decoded, error))
	{ if(error.isEmpty()) error = QStringLiteral("Malformed legacy production data."); return false; }
	state = decoded;
	return true;
}
}
