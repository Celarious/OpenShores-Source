#include "oldbuilding.h"

namespace OldBuilding
{

namespace
{

thread_local int topTexture = BT_HullTop;

const int TblShipToBuilding[OldShip::ST_Count] =
{
	BT_Berth, BT_BerthBottom,
	BT_LargeDoor, BT_LargeDoor, BT_LargeDoor,
	BT_LargeCrewDoor, BT_LargeCrewDoor, BT_LargeCrewDoor,
	BT_LargeHullDoor, BT_LargeHullDoor, BT_LargeHullDoor,
	BT_CrewDoor, BT_CrewDoor, BT_CrewDoor,
	BT_CrewHatch, BT_CrewHatch, BT_CrewHatch,
	-1, BT_HullBottom,
	BT_HullDoor, BT_HullDoor, BT_HullDoor,
	BT_HullHatch, BT_HullHatch, BT_HullHatch,
	BT_HullTop, BT_HullTopBottomWindow, BT_HullWall, BT_HullWallWindow,
	BT_IntCeiling, BT_IntDoor, BT_IntDoor, BT_IntDoor, BT_IntFloor,
	BT_IntHatch, BT_IntHatch, BT_IntHatch,
	BT_IntWallEndCap, BT_IntWallFinished, BT_IntWallFill, BT_IntWallCargo, BT_IntWallFillCargo,
	BT_IntWallEquipment, BT_IntWallFillEquipment,
	BT_Ladder, BT_LadderPole, -1, -1, BT_Railing, BT_RailingPost, -1,
	-1, -1, -1, BT_Surgery, BT_TransporterPad, BT_Turret, -1, -1,
	BT_StairCasing, BT_StairRiser, BT_StairTread,
};

bool ShipTextureOfBuilding(OldShip::ShipTex tx, float& s, float& t, float& u, float& v)
{
	if(tx < 0 || tx >= OldShip::ST_Count || TblShipToBuilding[tx] < 0)
		return false;
	int b = TblShipToBuilding[tx];
	if(b == BT_HullTop)
		b = topTexture;
	BuildingTexture(b, s, t, u, v);
	return true;
}

}

void BuildingTexture(int tx, float& s, float& t, float& u, float& v)
{
	if(tx >= 0x2d)
		tx = 0x23;
	if(tx < 0 || tx >= 44)
		tx = 0;
	const int* r = TblBuildingTexture[tx];
	if(tx == 0x10)
	{
		s = 0.99609375f;
		u = 0.00390625f;
	}
	else
	{
		s = (float)(((double)r[0] + 0.5) * 0.0078125);
		u = (float)(((double)(r[2] + r[0]) - 0.5) * 0.0078125);
	}
	t = (float)(((double)r[1] + 0.5) * 0.000244140625);
	v = (float)(((double)(r[3] + r[1]) - 0.5) * 0.000244140625);
}

const TblProfile& WallProfile(bool exterior, int roomType, int materialCommodity, bool belowGround)
{
	int cls = 0;
	switch(materialCommodity)
	{
	case 48: cls = 1; break;
	case 51: cls = 2; break;
	case 76: cls = 3; break;
	case 86: cls = 4; break;
	case 146: cls = 5; break;
	}
	return TblProfiles[TblProfileIndex[exterior ? 0 : 1][belowGround ? 1 : 0][roomType & 0x1f][cls]];
}

int MaterialCommodity(int buildMaterial)
{
	switch(buildMaterial)
	{
	case 0: return 0x56;
	case 0x100000: return 0x30;
	case 0x300000: return 0x33;
	case 0x400000: return 0x92;
	case 0x500000: return 0x91;
	case 0x600000: return 0x262;
	}
	return 0x4c;
}

bool IsCandleLit(int industry)
{
	switch(industry)
	{
	case 0x14: case 0x15: case 0x16: case 0x1e: case 0x20: case 0x66: case 0x69: case 0x72: case 0x78:
		return true;
	}
	return false;
}

BuildingTextures::BuildingTextures() : before(OldShip::SetTextureSource(ShipTextureOfBuilding)) {}
BuildingTextures::~BuildingTextures() { OldShip::SetTextureSource(before); }

void SetTopTexture(int buildingTex) { topTexture = buildingTex; }

}
