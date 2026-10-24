#ifndef OLDBUILDINGEDIT_H
#define OLDBUILDINGEDIT_H

#include "oldshipedit.h"
#include "oldbuilding.h"

namespace OldShip
{

const double MetreInFeet = 3.28083989501312;

double BuildingTerrainLevelRadius(int industry);
double BuildingSiteRadius(int industry, int deck);
bool BuildingIsCoastal(int industry);
bool BuildingValidGridCoord(double xFeet, double yFeet, double radius, int industry);
int BuildingMinDeck(int industry);
int BuildingMaxDeck(int industry, int materialCommodity = 145);

std::string BuildingFileExtension(int industry);
int BuildingIndustryOfExtension(const std::string& extension);

struct BuildingRules
{
	int industry;

	explicit BuildingRules(int anIndustry = 0) : industry(anIndustry) {}
	bool ValidGrid(double gx, double gy, int deck) const
	{
		return BuildingValidGridCoord(gx * GridUnit, gy * GridUnit, BuildingSiteRadius(industry, deck), industry);
	}
	int MinDeck() const { return BuildingMinDeck(industry); }
	int MaxDeck() const { return BuildingMaxDeck(industry); }
};

inline bool IsRoofType(int roomType)
{
	return roomType >= 22 && roomType <= 24;
}

void BuildingToEdit(const OldBuilding::Design& building, Design& out);
void EditToBuilding(const Design& design, OldBuilding::Design& building);

std::vector<FootPoint> RoomOutline(const Room& room);

bool RoofSecondPoint(const GridPoint& p0, double gx, double gy, GridPoint& p1);
bool RoofThirdPoint(const GridPoint& p0, const GridPoint& p1, double gx, double gy, GridPoint& p2);
bool RoofPoints(int roofType, const GridPoint& p0, const GridPoint& p1, const GridPoint& p2, std::vector<GridPoint>& points);

}

#endif
