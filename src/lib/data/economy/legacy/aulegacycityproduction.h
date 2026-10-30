#ifndef AuLegacyCityProduction_H
#define AuLegacyCityProduction_H

#include "aulegacycitycatalogue.h"
#include "auitem.h"
#include "auspecies.h"
#include "aulegacycitystate.h"
#include <functional>

class AuZoneResource;
namespace AuLegacyShip { struct Design; }

namespace AuLegacyCity
{
struct ProductionComponent
{
	quint16 commodityId = 0; // Historical GD namespace.
	quint8 effect = 0, quality = 0, technology = 31;
	quint32 required = 0, applied = 0;
	bool IsSatisfied() const { return applied >= (required ? required : 1); }
};

struct ProductionProcess
{
	quint32 developmentId = 0;
	quint16 slot = 0, processId = 0;
	bool enabled = true;
	bool bulkManufacturing = true;
	quint8 minimumTechnology = 0, buildingTechnology = 0;
	quint32 laborNeeded = 0;
	qint64 completeAt = 0, lastWorked = 0; // UTC milliseconds, zero when idle.
	qint8 productionBoost = 0;
	qint32 lastQuantity = 0;
	quint8 lastQuality = 0, lastTechnology = 0;
	QVector<ProductionComponent> components;
	AuSpecies species;
	bool sentient = false;
	// A factory design uses the recovered room/detail bill instead of GD's
	// research recipe. The runtime blueprint is the identity carried on media.
	quint32 spacecraftDesign = 0;
	quint8 spacecraftMinimumTechnology = 0;
	QString spacecraftName;
	float spacecraftEstimatedCost = 0;
	qint64 spacecraftRetryAt = 0;
};

struct ProductionState
{
	QVector<ProductionProcess> processes;
	// Tools are withdrawn one at a time and retain their condition between jobs.
	AuItemHash tools;
	QList<quint16> vehicles; // Historical GD commodity IDs.
};

struct ProductionOutput
{
	quint32 spacecraftDesign = 0;
	quint16 commodityId = 0;
	quint32 quantity = 0;
	quint8 quality = 0, technology = 0;
	bool research = false, unit = false;
	AuSpecies species;
	bool sentient = false;
	bool viableDNA = false; // Derived from consumed process DNA before any livestock override.
};

// Original fixed industry-to-technology table, independent of modern patents.
AUDATA_DLL quint16 IndustryTechnologyCommodity(quint8 industry);
AUDATA_DLL quint8 TechnologyProductIndustry(quint16 commodity);
AUDATA_DLL quint8 TechnologyOfProduct(quint8 producingIndustry, quint8 buildingTechnology,
	quint16 commodity, const QVector<Development>& developments);
// The callback is the original site's geometric rock-resource query. Quality
// anywhere else in a resource zone cannot select a mine or well's recipe.
AUDATA_DLL quint16 DefaultProductionRecipe(const AuLegacyCityCatalogue& catalogue, quint8 industry,
	const std::function<bool(quint16)>& resourceAtSite);
// Original F10 recipe choices in GD order. Mine/well resources and water
// presence must come from the selected site's world geometry queries.
AUDATA_DLL QVector<ManufacturingProcessId> ProductionRecipeChoices(const AuLegacyCityCatalogue& catalogue,
	quint8 industry, const std::function<bool(quint16)>& resourceAtSite, bool waterPresent, quint8 orbitZone);
AUDATA_DLL bool IsProductionTechnology(quint16 commodity);
AUDATA_DLL bool IsProductionUnit(quint16 commodity);
AUDATA_DLL bool ProductionDNAIsViable(quint16 commodity, const AuSpecies& species);
// Original farm output must call its world livestock service before adding
// cargo. Zero means no livestock dependency. Native DNA alone is insufficient.
AUDATA_DLL quint8 ProductionFarmAnimalLimit(quint8 industry, quint16 commodity);
// World service status: 0 spawned one animal, 1 already at the animal limit,
// 2 no suitable fauna. Returns false for blocked/impossible output.
AUDATA_DLL bool ApplyFarmAnimalResult(ProductionOutput& output, quint8 status, const AuSpecies& species);
AUDATA_DLL bool ProductionIsCriticalToSurvival(const AuLegacyCityCatalogue& catalogue, quint16 commodity, bool planetCanHaveFauna);
AUDATA_DLL bool HasProductionTool(const AuLegacyCityCatalogue& catalogue, quint16 commodity,
	const ProductionState& production, const AuItemHash& inventory);
AUDATA_DLL bool UseProductionTool(const AuLegacyCityCatalogue& catalogue, quint16 commodity,
	ProductionState& production, AuItemHash& inventory);

// Collect RefundProductionMaterials before replacing a recipe or its minimum
// technology. The caller restores cargo up to capacity and refunds R&D money.
AUDATA_DLL QVector<ProductionOutput> RefundProductionMaterials(const AuLegacyCityCatalogue& catalogue,
	const ProductionProcess& process, quint32& researchMoney);
AUDATA_DLL bool InitializeProductionProcess(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process,
	quint16 recipe, quint8 buildingTechnology, bool bulkManufacturing = true);
AUDATA_DLL bool InitializeSpacecraftProduction(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process,
	quint32 designId, const AuLegacyShip::Design& design, quint8 minimumTechnology, quint8 buildingTechnology);
AUDATA_DLL float ActualProductionCost(const AuLegacyCityCatalogue& catalogue, const ProductionProcess& process);
AUDATA_DLL void SetProductionTechnology(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process,
	quint8 technology);
// DaCity::TechLevelChanged also resets funded non-research work, with the
// original spacecraft-factory exception, and updates university research costs.
AUDATA_DLL void ApplyBuildingTechnologyChange(const AuLegacyCityCatalogue& catalogue, State& city,
	ProductionState& production, quint32 developmentId, quint8 technology);
AUDATA_DLL void UpgradeBuildingsFromInventory(const AuLegacyCityCatalogue& catalogue, State& city,
	AuItemHash& inventory, ProductionState& production, quint32 priorityDevelopmentId = 0);

// Money is returned separately so the city can debit government R&D and credit
// citizen salaries. Natural procurement is an original one-item probability
// trial; modern worker-multiplied AuZoneResource::Fetch is deliberately unused.
AUDATA_DLL bool FetchProductionMaterials(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process,
	AuItemHash& inventory, ProductionState& state, const AuZoneResource& resources, double moneyAvailable,
	quint32& moneyTaken, bool ringworld = false, quint8 electricityForSunlight = 0, quint8 bottledWater = 0);
// Construction consumes the worst TL/quality stock while retaining the best
// reusable tools. It has no treasury funding or manufacturing substitutions.
AUDATA_DLL bool FetchConstructionMaterials(const AuLegacyCityCatalogue& catalogue, ProductionProcess& components,
	AuItemHash& inventory, ProductionState& state, const AuZoneResource& resources, bool ringworld = false);
AUDATA_DLL bool ApplyProductionMaterial(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process,
	AuItem& item, quint8 electricityForSunlight = 0, quint8 bottledWater = 0);
AUDATA_DLL bool CanRunProduction(const ProductionProcess& process);
AUDATA_DLL bool RunProduction(ProductionProcess& process, qint64 now);

// Finishing leaves inputs applied until the caller successfully places the
// output, increases building technology, or accepts the original discard rule.
// Retaining them permits original blocked-output retries without buying twice.
AUDATA_DLL bool FinishProduction(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process,
	const AuZoneResource& resources, qint64 now, ProductionOutput& output);
AUDATA_DLL void ConsumeProductionComponents(ProductionProcess& process);
AUDATA_DLL void ResetProduction(const AuLegacyCityCatalogue& catalogue, ProductionProcess& process);
AUDATA_DLL quint32 CurrencyProductionValue(const ProductionOutput& output);
AUDATA_DLL void ScalePowerProduction(ProductionOutput& output, quint8 industry, quint8 orbitZone);

// Bounded, atomic city-owned persistence. An empty group means no processes.
AUDATA_DLL bool EncodeProductionState(const AuLegacyCityCatalogue& catalogue, const ProductionState& state,
	QByteArray& bytes, QString& error);
AUDATA_DLL bool DecodeProductionState(const AuLegacyCityCatalogue& catalogue, const QByteArray& bytes,
	ProductionState& state, QString& error);
}
#endif
