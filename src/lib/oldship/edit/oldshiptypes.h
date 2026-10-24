#ifndef OLDSHIPTYPES_H
#define OLDSHIPTYPES_H

namespace OldShip
{

struct RoomTypeInfo
{
	const char*	name;
	const char*	desc;
};

struct DetailTypeInfo
{
	const char*	name;
	const char*	desc;
	int			okOnBridge;
	int			crew;
	int			placement;
	int			techLevel;
	int			mass;
};

enum PLACEMENT
{
	PLACE_Berth = 65535,
	PLACE_HullWall = 65534,
	PLACE_InteriorWall = 65533,
	PLACE_Floor = 65532,
	PLACE_HullTopBottom = 65531,
	PLACE_Extents = 65530,
	PLACE_Anywhere = 65529,
	PLACE_OutsideDeck0 = 65528,
	PLACE_Stairs = 65527,
	PLACE_FloorOrOutside = 65526
};

const RoomTypeInfo* RoomTypeInfoOf(int type);
const DetailTypeInfo* DetailTypeInfoOf(int type);

const char* RoomTypeBuildingName(int type);
const char* RoomTypeBuildingDesc(int type);
const char* DetailTypeBuildingName(int type);
const char* DetailTypeBuildingDesc(int type);

struct BuildingIndustryInfo
{
	int			id;
	const char*	name;
	int			buildRadius;
	int			stackLimit;
	int			hasProcess;
};
int BuildingIndustryCount();
const BuildingIndustryInfo& BuildingIndustryAt(int index);
const BuildingIndustryInfo* BuildingIndustryInfoOf(int industry);

}

#endif
