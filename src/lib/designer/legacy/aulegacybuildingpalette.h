#ifndef AuLegacyBuildingPalette_H
#define AuLegacyBuildingPalette_H

#include <cstddef>

namespace AuLegacyBuildingPalette
{
// AuDesignBuilding's independent room and equipment palette. Room/detail IDs
// are shared by both designers; their displayed GD names and positions differ.
struct BuildingTool
{
	int type, row, column;
	const char* name;
	const char* tip;
};

const BuildingTool BuildingRooms[] = {
	{1, 0, 1, "Barracks", "Quarters for troops."},
	{2, 0, 2, "Command Center", "Command and control center."},
	{3, 0, 3, "Cell", "Confinement facility for prisoners and animals."},
	{8, 0, 4, "Shop", "General purpose work area."},
	{13, 0, 5, "Garage", "Parking area for vehicles that fit inside."},
	{9, 0, 6, "Liquid Storage", "Containment for storing liquids."},
	{10, 1, 1, "Kitchen", "Food preparation and consumption area."},
	{11, 1, 2, "Gravity Well", "Vertical column with no gravity for movement between floors."},
	{6, 1, 3, "Hall", "Access between rooms."},
	{5, 1, 4, "Storage", "Area for storing commodities."},
	{15, 1, 5, "Open to Story Below", "Extends height of room below."},
	{14, 1, 6, "Living Room", "Area for rest and relaxation."},
	{16, 2, 1, "Radar Room", "Space for remote sensing equipment and controls."},
	{17, 2, 2, "Shield Room", "Space for shield generators and controls."},
	{18, 2, 3, "Doctor's Office", "Medical treatment room."},
	{19, 2, 4, "Bedroom", "Home for people."},
	{20, 2, 5, "Transporter Room", "Matter transmission facility."},
	{21, 2, 6, "Weapon Room", "Integrated weapon system and fire control facility."},
	{22, 3, 10, "Roof, Gable", "Roof section with gable ends."},
	{23, 3, 11, "Roof, Hip", "Roof section with hip ends."},
	{24, 3, 12, "Roof, Round", "Roof section that is round or oval shaped."},
};

template<std::size_t N>
const BuildingTool* FindBuildingTool(const BuildingTool (&list)[N], int type)
{
	for(const BuildingTool& tool : list)
		if(tool.type == type) return &tool;
	return nullptr;
}
}

#endif
