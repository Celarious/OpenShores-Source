#include "oldbuildingspots.h"

#include <cmath>

namespace OldShip
{

namespace
{

const double FeetPerMetre = 3.2808399;
const double SpacecraftHalf = 119.0;

}

SpotKind SpotKindOf(int type)
{
	if(type >= SpotGroundBase && type < SpotGroundBase + 8)
		return Spot_Ground;
	if(type >= SpotSpaceBase && type < SpotSpaceBase + 8)
		return Spot_Space;
	if(type >= SpotRocketBase && type < SpotRocketBase + 8)
		return Spot_Rocket;
	if(type >= SpotWaterBase && type < SpotWaterBase + 8)
		return Spot_Water;
	if(type == SpotSpacecraftType)
		return Spot_Spacecraft;
	return Spot_None;
}

int SpotBase(SpotKind kind)
{
	switch(kind)
	{
	case Spot_Ground: return SpotGroundBase;
	case Spot_Space: return SpotSpaceBase;
	case Spot_Rocket: return SpotRocketBase;
	case Spot_Water: return SpotWaterBase;
	case Spot_Spacecraft: return SpotSpacecraftType;
	default: return -1;
	}
}

double SpotTurn(int type)
{
	const SpotKind kind = SpotKindOf(type);
	if(kind == Spot_None || kind == Spot_Spacecraft)
		return 0.0;
	return (type - SpotBase(kind)) * (3.14159265358979323846 / 4.0);
}

void SpotHalfSize(SpotKind kind, double& halfLength, double& halfWidth)
{
	switch(kind)
	{
	case Spot_Ground: halfLength = 6 * FeetPerMetre; halfWidth = 3 * FeetPerMetre; return;
	case Spot_Space: halfLength = 8.5 * FeetPerMetre; halfWidth = 6 * FeetPerMetre; return;
	case Spot_Rocket: halfLength = 5 * FeetPerMetre; halfWidth = 5 * FeetPerMetre; return;
	case Spot_Water: halfLength = 12 * FeetPerMetre; halfWidth = 6 * FeetPerMetre; return;
	case Spot_Spacecraft: halfLength = SpacecraftHalf; halfWidth = SpacecraftHalf; return;
	default: halfLength = halfWidth = 0; return;
	}
}

std::vector<FootPoint> SpotFootprint(int type)
{
	double halfLength, halfWidth;
	SpotHalfSize(SpotKindOf(type), halfLength, halfWidth);
	const double a = SpotTurn(type), c = cos(a), s = sin(a);
	const double side[4] = { -halfWidth, halfWidth, halfWidth, -halfWidth }, fore[4] = { -halfLength, -halfLength, halfLength, halfLength };
	std::vector<FootPoint> out;
	for(int i = 0; i < 4; i++)
	{
		FootPoint p;
		p.x = (side[i] * c - fore[i] * s) / GridUnit;
		p.y = (side[i] * s + fore[i] * c) / GridUnit;
		out.push_back(p);
	}
	return out;
}

const char* SpotName(int type)
{
	switch(SpotKindOf(type))
	{
	case Spot_Ground: return "Ground Vehicle Parking Spot";
	case Spot_Space: return "Space Vehicle Parking Spot";
	case Spot_Rocket: return "Space Rocket Parking Spot";
	case Spot_Water: return "Water Vehicle Parking Spot";
	case Spot_Spacecraft: return "Spacecraft Pad";
	default: return 0;
	}
}

const char* SpotDesc(int type)
{
	switch(SpotKindOf(type))
	{
	case Spot_Ground: return "Where a ground vehicle parks, or a vehicle factory delivers one. Outside on the ground, or in a garage.";
	case Spot_Space: return "Where a space vehicle (aircraft) parks, or an aircraft factory delivers one. Outside on the ground.";
	case Spot_Rocket: return "Where a space rocket parks. Outside on the ground.";
	case Spot_Water: return "Where a water vehicle parks, or a ship yard delivers one. Outside, on the water side of a building by the sea.";
	case Spot_Spacecraft: return "Where a spacecraft lands, or a spacecraft factory delivers one: a pad clear of the building, big enough "
			"for the largest ship that lands on the ground (238 ft square).";
	default: return 0;
	}
}

const char* SpotIcon(SpotKind kind)
{
	switch(kind)
	{
	case Spot_Ground: return ":data/ScParkGroundVehicle.png";
	case Spot_Space: return ":data/ScParkSpaceVehicle.png";
	case Spot_Rocket: return ":data/ScParkRocket.png";
	case Spot_Water: return ":data/ScParkWaterVehicle.png";
	case Spot_Spacecraft: return ":data/ScParkSpacecraft.png";
	default: return 0;
	}
}

}
