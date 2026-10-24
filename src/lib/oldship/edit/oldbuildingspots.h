#ifndef OldBuildingSpots_H
#define OldBuildingSpots_H

#include "oldshipedit.h"
#include <vector>

namespace OldShip
{

enum SpotKind
{
	Spot_None = -1,
	Spot_Ground,
	Spot_Space,
	Spot_Rocket,
	Spot_Water,
	Spot_Spacecraft,
	Spot_Count
};

const int SpotGroundBase = 200;
const int SpotSpaceBase = 208;
const int SpotRocketBase = 216;
const int SpotWaterBase = 224;
const int SpotSpacecraftType = 232;

inline bool IsSpot(int type)
{
	return type >= SpotGroundBase && type <= SpotSpacecraftType;
}

SpotKind SpotKindOf(int type);
int SpotBase(SpotKind kind);
double SpotTurn(int type);
void SpotHalfSize(SpotKind kind, double& halfLength, double& halfWidth);
std::vector<FootPoint> SpotFootprint(int type);
const char* SpotName(int type);
const char* SpotDesc(int type);
const char* SpotIcon(SpotKind kind);

}

#endif
