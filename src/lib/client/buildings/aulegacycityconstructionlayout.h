#ifndef AuLegacyCityConstructionLayout_H
#define AuLegacyCityConstructionLayout_H

#include <QtGlobal>

// Exact AuTabBuild (2016 executable 0x2b9890) placement and icon identifiers.
// All rows/columns/spans below are original QGridLayout arguments. The
// category rail order differs from the original stacked page order.
namespace AuLegacyCityConstructionLayout
{
struct Button
{
	quint8 industry, process, page;
	int row, column, rowSpan, columnSpan;
	const char* name;
	const char* icon;
};

static const Button Buttons[] =
{
	// Page 0: Food & Drink; source layout ends at manual export line 327335.
	{30, 0, 0, 0, 3, 1, 2, "Farm", ":data/bFarm.png"},
	{32, 0, 0, 0, 5, 1, 2, "Orchard", ":data/bOrchard.png"},
	{34, 0, 0, 0, 7, 1, 2, "Wharf", ":data/bWharf.png"},
	{43, 0, 0, 2, 3, 1, 2, "Grist Mill", ":data/bGristMill.png"},
	{46, 0, 0, 2, 5, 1, 2, "Sugar Mill", ":data/bSugarMill.png"},
	{45, 0, 0, 2, 7, 1, 2, "Meat Processing Plant", ":data/bMeatProcessingPlant.png"},
	{40, 0, 0, 4, 4, 1, 2, "Bakery", ":data/bBakery.png"},
	{35, 0, 0, 4, 6, 1, 2, "Dairy", ":data/bDairy.png"},
	{41, 0, 0, 6, 3, 1, 2, "Brewery", ":data/bBrewery.png"},
	{42, 0, 0, 6, 5, 1, 2, "Distillery", ":data/bDistillery.png"},
	{48, 0, 0, 6, 7, 1, 2, "Winery", ":data/bWinery.png"},
	// Page 1: Housing; source layout ends at manual export line 327412.
	{20, 0, 1, 0, 1, 1, 1, "House", ":data/bHouse.png"},
	{22, 0, 1, 2, 2, 1, 1, "Apartment", ":data/bApartment.png"},
	{21, 0, 1, 4, 3, 1, 1, "Condominium", ":data/bHotel.png"},
	// Page 2: Power Plants; source layout ends at manual export line 327576.
	{62, 0, 2, 0, 3, 1, 2, "Hydroelectric Power Plant", ":data/bHydroelectricPowerPlant.png"},
	{66, 0, 2, 0, 5, 1, 2, "Solar Power Plant", ":data/bSolarPowerPlant.png"},
	{67, 0, 2, 0, 7, 1, 2, "Wind Power Plant", ":data/bWindPowerPlant.png"},
	{60, 0, 2, 2, 3, 1, 2, "Coal Power Plant", ":data/bCoalPowerPlant.png"},
	{64, 0, 2, 2, 5, 1, 2, "Natural Gas Power Plant", ":data/bNaturalGasPowerPlant.png"},
	{65, 0, 2, 2, 7, 1, 2, "Nuclear Power Plant", ":data/bNuclearPowerPlant.png"},
	{61, 0, 2, 4, 4, 1, 2, "Gasoline Power Plant", ":data/bGasolinePowerPlant.png"},
	{63, 0, 2, 4, 6, 1, 2, "Hydrogen Power Plant", ":data/bHydrogenPowerPlant.png"},
	// Page 3: Manufacture; source layout ends at manual export line 327773.
	{50, 0, 3, 0, 3, 1, 2, "Mine", ":data/bMine.png"},
	{89, 0, 3, 0, 5, 1, 2, "Treatment Plant", ":data/bTreatmentPlant.png"},
	{51, 0, 3, 0, 7, 1, 2, "Well", ":data/bWell.png"},
	{31, 0, 3, 1, 5, 1, 2, "Logging Camp", ":data/bLoggingCamp.png"},
	{86, 0, 3, 3, 4, 1, 2, "Smelter", ":data/bSmelter.png"},
	{83, 0, 3, 3, 6, 1, 2, "Refinery", ":data/bRefinery.png"},
	{44, 0, 3, 4, 4, 1, 2, "Lumber Mill", ":data/bLumberMill.png"},
	{47, 0, 3, 4, 6, 1, 2, "Textile Mill", ":data/bTextileMill.png"},
	{76, 0, 3, 6, 4, 1, 2, "Explosives Factory", ":data/bExplosivesFactory.png"},
	{84, 0, 3, 6, 6, 1, 2, "Rocket Motor Factory", ":data/bRocketMotorFactory.png"},
	// Page 4: Tools & Weapons; source layout ends at manual export line 327984.
	{72, 0, 4, 0, 3, 1, 2, "Carpenter", ":data/bCarpenter.png"},
	{79, 0, 4, 0, 5, 1, 2, "Machine Shop", ":data/bMachineShop.png"},
	{82, 0, 4, 0, 7, 1, 2, "Plastics Factory", ":data/bPlasticsFactory.png"},
	{75, 0, 4, 2, 4, 1, 2, "Electronics Factory", ":data/bElectronicsFactory.png"},
	{74, 0, 4, 2, 6, 1, 2, "Computer Factory", ":data/bComputerFactory.png"},
	{81, 0, 4, 3, 5, 1, 2, "Pharmaceutical Lab", ":data/bPharmaceuticalLab.png"},
	{73, 0, 4, 5, 4, 1, 2, "Clothing Factory", ":data/bClothingFactory.png"},
	{78, 0, 4, 5, 6, 1, 2, "Jeweler", ":data/bJeweler.png"},
	{71, 0, 4, 7, 3, 1, 2, "Armorer", ":data/bArmorer.png"},
	{80, 0, 4, 7, 5, 1, 2, "Munitions Factory", ":data/bMunitionsFactory.png"},
	{77, 0, 4, 7, 7, 1, 2, "Weapon Smith", ":data/bWeaponSmith.png"},
	// Page 5: Transportation; source layout ends at manual export line 328161.
	{111, 0, 5, 0, 4, 1, 2, "Airport Terminal", ":data/bAirportTerminal.png"},
	{110, 0, 5, 0, 6, 1, 2, "Airport Repair Shop", ":data/bAirportRepairShop.png"},
	{70, 0, 5, 2, 3, 1, 2, "Aircraft Factory", ":data/bAircraftFactory.png"},
	{85, 0, 5, 2, 5, 1, 2, "Ship Yard", ":data/bShipYard.png"},
	{88, 0, 5, 2, 7, 1, 2, "Vehicle Factory", ":data/bVehicleFactory.png"},
	{87, 0, 5, 3, 4, 1, 2, "Spacecraft Factory", ":data/bSpacecraftFactory.png"},
	{126, 0, 5, 3, 6, 1, 2, "Hangar", ":data/bHangar.png"},
	{3, 0, 5, 5, 5, 1, 2, "Design Studio", ":data/bDesignStudio.png"},
	{127, 0, 5, 6, 5, 1, 2, "Observatory", ":data/bObservatory.png"},
	// Page 6: Government & Military; source layout ends at manual export line 328381.
	{1, 0, 6, 1, 4, 1, 2, "Town Square", ":data/bTownSquare.png"},
	{10, 0, 6, 3, 2, 1, 2, "Military Flag", ":data/bBaseFlag.png"},
	{123, 0, 6, 3, 6, 1, 2, "Capitol", ":data/bCapital.png"},
	{128, 0, 6, 4, 3, 1, 2, "Military Guard Tower", ":data/bGuardTower.png"},
	{12, 0, 6, 5, 3, 1, 2, "Military Shield Generator", ":data/bShieldGeneratorBase.png"},
	{11, 0, 6, 6, 3, 1, 2, "Military Weapon System", ":data/bLaserTowerBase.png"},
	// Page 7: Commercial; source layout ends at manual export line 328649.
	{105, 0, 7, 0, 1, 1, 1, "Cantina", ":data/bTavern.png"},
	{103, 0, 7, 0, 2, 1, 1, "Grocery", ":data/bGrocery.png"},
	{104, 0, 7, 0, 3, 1, 1, "Retail Store", ":data/bRetailStore.png"},
	{101, 0, 7, 1, 1, 1, 1, "Broker", ":data/bBroker.png"},
	{100, 0, 7, 1, 2, 1, 1, "Bank", ":data/bBank.png"},
	{4, 0, 7, 1, 3, 1, 1, "Warehouse", ":data/bWarehouse.png"},
	{122, 0, 7, 3, 1, 1, 1, "Fire Station", ":data/bFireDepartment.png"},
	{124, 0, 7, 3, 2, 1, 1, "Police Station", ":data/bPoliceDepartment.png"},
	{121, 0, 7, 3, 3, 1, 1, "Hospital", ":data/bHospital.png"},
	{120, 0, 7, 5, 1, 1, 1, "Church", ":data/bChurch.png"},
	{114, 0, 7, 5, 2, 1, 1, "Park", ":data/bPark.png"},
	{125, 0, 7, 5, 3, 1, 1, "University", ":data/bSchool.png"},
	{112, 0, 7, 6, 1, 1, 1, "Arena", ":data/bArena.png"},
	{102, 0, 7, 6, 2, 1, 1, "Casino", ":data/bCasino.png"},
	{113, 0, 7, 6, 3, 1, 1, "Zoo", ":data/bZoo.png"},
	// Page 8: Roads & Landscape; source layout ends at manual export line 329218.
	{2, 80, 8, 0, 1, 1, 2, "Defoliate Area", ":data/bp_RoadDefoliate.bmp"},
	{2, 81, 8, 0, 3, 1, 2, "Clear Area", ":data/bp_RoadClearing.bmp"},
	{2, 74, 8, 0, 5, 1, 2, "Irrigate Area", ":data/bp_RoadIrrigate.bmp"},
	{2, 5, 8, 2, 1, 2, 2, "Grade Area", ":data/bp_RoadGrade.bmp"},
	{2, 2, 8, 3, 3, 2, 2, "Dirt Road", ":data/bp_RoadDirt.bmp"},
	{2, 3, 8, 3, 5, 2, 2, "Asphalt Road", ":data/bp_RoadAsphalt.bmp"},
	{2, 4, 8, 5, 3, 2, 2, "Concrete Road", ":data/bp_RoadConcrete.bmp"},
	{2, 7, 8, 5, 5, 2, 2, "Pressurized Road", ":data/bp_RoadPressurized.bmp"},
	{129, 79, 8, 10, 3, 1, 2, "Biodome", ":data/bBiodome.png"},
};

inline const char* IndustryIcon(int industry)
{
	for(const auto& button : Buttons)
		if(button.industry == industry) return button.icon;
	return "";
}

struct Category
{
	quint8 page;
	int row;
	const char* name;
	const char* icon;
};
static const Category Categories[] =
{
	{6, 0, "Government & Military", ":data/CommEmpire.png"},
	{8, 1, "Roads & Landscape", ":data/bp_RoadDirt.bmp"},
	{0, 2, "Food & Drink", ":data/c_Vegetable.png"},
	{1, 3, "Housing", ":data/Construction.png"},
	{2, 4, "Power Plants", ":data/c_Electricity.png"},
	{3, 5, "Manufacture", ":data/c_MineMachinery.png"},
	{4, 6, "Tools & Weapons", ":data/c_Hammer.png"},
	{7, 7, "Commercial", ":data/MsgBuyOffer.png"},
	{5, 8, "Transportation", ":data/c_Vehicle.png"},
};

struct PageGap { quint8 page; int row, height; };
static const PageGap PageGaps[] =
{
	{0, 1, 8},
	{0, 3, 8},
	{0, 5, 8},
	{1, 1, 8},
	{1, 3, 8},
	{2, 1, 8},
	{2, 3, 8},
	{3, 2, 8},
	{3, 5, 8},
	{4, 1, 8},
	{4, 4, 8},
	{4, 6, 8},
	{5, 1, 8},
	{5, 4, 8},
	{6, 0, 8},
	{6, 2, 8},
	{7, 2, 8},
	{7, 4, 8},
	{8, 1, 8},
	{8, 7, 8},
	{8, 9, 12},
};

struct PageStretch { quint8 page; int row, leftColumn, rightColumn; };
static const PageStretch PageStretches[] =
{
	{0, 7, 0, 10},
	{1, 5, 0, 10},
	{2, 5, 0, 10},
	{3, 7, 0, 10},
	{4, 8, 0, 10},
	{5, 7, 0, 10},
	{6, 7, 0, 10},
	{7, 7, 0, 10},
	{8, 11, 0, 7},
};

constexpr int BuildingButtonSize = 60;
constexpr int BuildingIconInset = 4;
constexpr int CategoryButtonSize = 30;
constexpr int PageMargin = 0;
constexpr int Spacing = 2;
constexpr int OuterMargin = 2;
constexpr int CategoryColumn = 1;
constexpr int CategoryGapRow = 9;
constexpr int CategoryGapHeight = 4;
constexpr int ClearRow = 10;
constexpr int StopRow = 11;
constexpr int StackRowSpan = 12;
constexpr int ConstraintAngleRow = 12;
constexpr int DetailRow = 13;
constexpr int DefaultPage = 6;

// The original roads page embeds these controls among its image buttons.
constexpr int GradingRow = 8;
constexpr int GradingColumn = 1;
constexpr int GradingColumnSpan = 4;
constexpr int RoadWidthRow = 8;
constexpr int RoadWidthColumn = 5;
constexpr int RoadWidthColumnSpan = 2;
}

#endif
