#include "oldshiptypes.h"
#include "oldbuildingspots.h"

namespace OldShip
{

namespace
{

const RoomTypeInfo s_BuildingRooms[25] =
{
	{ 0, 0 },
	{ "Barracks", "Quarters for troops." },
	{ "Command Center", "Command and control center." },
	{ "Cell", "Confinement facility for prisoners and animals." },
	{ 0, 0 },
	{ "Storage", "Area for storing commodities." },
	{ "Hall", "Access between rooms." },
	{ 0, 0 },
	{ "Shop", "General purpose work area." },
	{ "Liquid Storage", "Containment for storing liquids." },
	{ "Kitchen", "Food preparation and consumption area." },
	{ "Gravity Well", "Vertical column with no gravity for movement between floors." },
	{ 0, 0 },
	{ "Garage", "Parking area for vehicles that fit inside." },
	{ "Living Room", "Area for rest and relaxation." },
	{ "Open to Story Below", "Extends height of room below." },
	{ "Radar Room", "Space for remote sensing equipment and controls." },
	{ "Shield Room", "Space for shield projectors and controls." },
	{ "Doctor's Office", "Medical treatment room." },
	{ "Bedroom", "Home for people." },
	{ "Transporter Room", "Matter transmission facility." },
	{ "Weapon Room", "Integrated weapon system and fire control facility." },
	{ "Roof, Gable", "Roof section with gable ends." },
	{ "Roof, Hip", "Roof section with hip ends." },
	{ "Roof, Round", "Roof section that is round or oval shaped." },
};

struct BuildingDetailName
{
	const char*	name;
	const char*	desc;
};

const BuildingDetailName s_BuildingDetails[143] =
{
	{ 0, 0 },
	{ "Telescope Station", "Displays solar system and wormhole information." },
	{ "Bunk Bed", "Personal respawn location for two people." },
	{ "Large Wall Door", "Large wall-length doorway through exterior wall. Locked to prevent use by unauthorized personnel." },
	{ "Doctor's Station", "Required in each doctor's office to operate the surgical unit." },
	{ "Door", "Doorway through interior wall." },
	{ 0, 0 },
	{ "Fire Control Station", "Required in each weapon room to operate the weapon." },
	{ "Hatch", "Doorway through interior floors. Includes a ladder." },
	{ "Exterior Wall Opening", "Large wall-length opening through exterior wall." },
	{ 0, 0 },
	{ "Surgery Unit", "Performs complex anatomical operations. Requires a Doctor's Station in the same room." },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ "Radar Station", "Operates radar." },
	{ 0, 0 },
	{ "Shield Station", "Operates energy shields." },
	{ 0, 0 },
	{ "Transporter Station", "Operates matter transmission equipment." },
	{ "Transporter Pad", "One pad is required per person transported simultaneously. Requires a Transporter Station in the same room." },
	{ "Turret (Wall)", "Provides a standard wall mount for various weapons." },
	{ "Turret (Ceiling/Floor)", "Provides a standard floor or ceiling mount for various weapons." },
	{ 0, 0 },
	{ 0, 0 },
	{ "Bunk Bed", "Personal respawn location for two people." },
	{ "Bunk Bed", "Personal respawn location for two people." },
	{ "Bunk Bed", "Personal respawn location for two people." },
	{ "Bunk Bed", "Personal respawn location for two people." },
	{ "Bunk Bed", "Personal respawn location for two people." },
	{ "Bunk Bed", "Personal respawn location for two people." },
	{ "Bunk Bed", "Personal respawn location for two people." },
	{ 0, 0 },
	{ "Turret (Ceiling/Floor)", "Provides a standard floor or ceiling mount for various weapons." },
	{ "Turret (Wall)", "Provides a standard wall mount for various weapons." },
	{ "Turret (Wall)", "Provides a standard wall mount for various weapons." },
	{ "Turret (Wall)", "Provides a standard wall mount for various weapons." },
	{ "Turret (Wall)", "Provides a standard wall mount for various weapons." },
	{ "Turret (Wall)", "Provides a standard wall mount for various weapons." },
	{ "Turret (Wall)", "Provides a standard wall mount for various weapons." },
	{ "Turret (Wall)", "Provides a standard wall mount for various weapons." },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ "Restricted Door", "Doorway through interior wall. Locked to prevent use by unauthorized personnel." },
	{ "Exterior Door", "Doorway through exterior wall. Locked to prevent access by unauthorized personnel." },
	{ "Restricted Hatch", "Doorway through interior floors. Locked to prevent use by unauthorized personnel." },
	{ "Exterior Hatch", "Doorway through exterior floors. Locked to prevent use by unauthorized personnel." },
	{ "Large Door", "Large wall-length doorway through interior wall." },
	{ "Large Restricted Door", "Large wall-length doorway through interior wall. Locked to prevent use by unauthorized personnel." },
	{ "Wall Opening", "Large wall-length opening through interior wall." },
	{ "Railing", "Changes interior wall into a railing." },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ "Bed", "Personal respawn location for one person." },
	{ "Bed", "Personal respawn location for one person." },
	{ "Bed", "Personal respawn location for one person." },
	{ "Bed", "Personal respawn location for one person." },
	{ "Bed", "Personal respawn location for one person." },
	{ "Bed", "Personal respawn location for one person." },
	{ "Bed", "Personal respawn location for one person." },
	{ "Bed", "Personal respawn location for one person." },
	{ 0, 0 },
	{ "Game Table", "Provides interactive games for up to four people." },
	{ "Game Table", "Provides interactive games for up to four people." },
	{ 0, 0 },
	{ "Hatch", "Doorway through interior floors. Includes a ladder." },
	{ "Hatch", "Doorway through interior floors. Includes a ladder." },
	{ "Hatch", "Doorway through interior floors. Includes a ladder." },
	{ "Hatch", "Doorway through interior floors. Includes a ladder." },
	{ "Hatch", "Doorway through interior floors. Includes a ladder." },
	{ "Hatch", "Doorway through interior floors. Includes a ladder." },
	{ "Hatch", "Doorway through interior floors. Includes a ladder." },
	{ "Restricted Hatch", "Doorway through interior floors. Locked to prevent use by unauthorized personnel." },
	{ "Restricted Hatch", "Doorway through interior floors. Locked to prevent use by unauthorized personnel." },
	{ "Restricted Hatch", "Doorway through interior floors. Locked to prevent use by unauthorized personnel." },
	{ "Restricted Hatch", "Doorway through interior floors. Locked to prevent use by unauthorized personnel." },
	{ "Restricted Hatch", "Doorway through interior floors. Locked to prevent use by unauthorized personnel." },
	{ "Restricted Hatch", "Doorway through interior floors. Locked to prevent use by unauthorized personnel." },
	{ "Restricted Hatch", "Doorway through interior floors. Locked to prevent use by unauthorized personnel." },
	{ "Exterior Hatch", "Doorway through exterior floors. Locked to prevent use by unauthorized personnel." },
	{ "Exterior Hatch", "Doorway through exterior floors. Locked to prevent use by unauthorized personnel." },
	{ "Exterior Hatch", "Doorway through exterior floors. Locked to prevent use by unauthorized personnel." },
	{ "Exterior Hatch", "Doorway through exterior floors. Locked to prevent use by unauthorized personnel." },
	{ "Exterior Hatch", "Doorway through exterior floors. Locked to prevent use by unauthorized personnel." },
	{ "Exterior Hatch", "Doorway through exterior floors. Locked to prevent use by unauthorized personnel." },
	{ "Exterior Hatch", "Doorway through exterior floors. Locked to prevent use by unauthorized personnel." },
	{ 0, 0 },
	{ "Weapon Emitter", "Locates a weapon effect emission point of a weapon system. Weapon emitter is optional. Weapon effect emanates from the base of the fire control console when the weapon effect emitter is omitted." },
	{ "Flag Pole", "Displays the flag of the empire that controls the building." },
	{ "Stairs, Straight", "Stairs up to next story above, straight with no turns. Next story above must already exist." },
	{ "Stairs, Straight", "Stairs up to next story above, straight with no turns. Next story above must already exist." },
	{ "Stairs, Straight", "Stairs up to next story above, straight with no turns. Next story above must already exist." },
	{ "Stairs, Straight", "Stairs up to next story above, straight with no turns. Next story above must already exist." },
	{ "Stairs, Straight", "Stairs up to next story above, straight with no turns. Next story above must already exist." },
	{ "Stairs, Straight", "Stairs up to next story above, straight with no turns. Next story above must already exist." },
	{ "Stairs, Straight", "Stairs up to next story above, straight with no turns. Next story above must already exist." },
	{ "Stairs, Straight", "Stairs up to next story above, straight with no turns. Next story above must already exist." },
	{ "Stairs, Turn Left", "Stairs up to next story above, with 90 degree turn to the left. Next story above must already exist." },
	{ "Stairs, Turn Left", "Stairs up to next story above, with 90 degree turn to the left. Next story above must already exist." },
	{ "Stairs, Turn Left", "Stairs up to next story above, with 90 degree turn to the left. Next story above must already exist." },
	{ "Stairs, Turn Left", "Stairs up to next story above, with 90 degree turn to the left. Next story above must already exist." },
	{ "Stairs, Turn Left", "Stairs up to next story above, with 90 degree turn to the left. Next story above must already exist." },
	{ "Stairs, Turn Left", "Stairs up to next story above, with 90 degree turn to the left. Next story above must already exist." },
	{ "Stairs, Turn Left", "Stairs up to next story above, with 90 degree turn to the left. Next story above must already exist." },
	{ "Stairs, Turn Left", "Stairs up to next story above, with 90 degree turn to the left. Next story above must already exist." },
	{ "Stairs, Turn Right", "Stairs up to next story above, with 90 degree turn to the right. Next story above must already exist." },
	{ "Stairs, Turn Right", "Stairs up to next story above, with 90 degree turn to the right. Next story above must already exist." },
	{ "Stairs, Turn Right", "Stairs up to next story above, with 90 degree turn to the right. Next story above must already exist." },
	{ "Stairs, Turn Right", "Stairs up to next story above, with 90 degree turn to the right. Next story above must already exist." },
	{ "Stairs, Turn Right", "Stairs up to next story above, with 90 degree turn to the right. Next story above must already exist." },
	{ "Stairs, Turn Right", "Stairs up to next story above, with 90 degree turn to the right. Next story above must already exist." },
	{ "Stairs, Turn Right", "Stairs up to next story above, with 90 degree turn to the right. Next story above must already exist." },
	{ "Stairs, Turn Right", "Stairs up to next story above, with 90 degree turn to the right. Next story above must already exist." },
	{ "Stairs, U-turn Left", "Stairs up to next story above, with 180 degree turn to the left. Next story above must already exist." },
	{ "Stairs, U-turn Left", "Stairs up to next story above, with 180 degree turn to the left. Next story above must already exist." },
	{ "Stairs, U-turn Left", "Stairs up to next story above, with 180 degree turn to the left. Next story above must already exist." },
	{ "Stairs, U-turn Left", "Stairs up to next story above, with 180 degree turn to the left. Next story above must already exist." },
	{ "Stairs, U-turn Left", "Stairs up to next story above, with 180 degree turn to the left. Next story above must already exist." },
	{ "Stairs, U-turn Left", "Stairs up to next story above, with 180 degree turn to the left. Next story above must already exist." },
	{ "Stairs, U-turn Left", "Stairs up to next story above, with 180 degree turn to the left. Next story above must already exist." },
	{ "Stairs, U-turn Left", "Stairs up to next story above, with 180 degree turn to the left. Next story above must already exist." },
	{ "Stairs, U-turn Right", "Stairs up to next story above, with 180 degree turn to the right. Next story above must already exist." },
	{ "Stairs, U-turn Right", "Stairs up to next story above, with 180 degree turn to the right. Next story above must already exist." },
	{ "Stairs, U-turn Right", "Stairs up to next story above, with 180 degree turn to the right. Next story above must already exist." },
	{ "Stairs, U-turn Right", "Stairs up to next story above, with 180 degree turn to the right. Next story above must already exist." },
	{ "Stairs, U-turn Right", "Stairs up to next story above, with 180 degree turn to the right. Next story above must already exist." },
	{ "Stairs, U-turn Right", "Stairs up to next story above, with 180 degree turn to the right. Next story above must already exist." },
	{ "Stairs, U-turn Right", "Stairs up to next story above, with 180 degree turn to the right. Next story above must already exist." },
	{ "Stairs, U-turn Right", "Stairs up to next story above, with 180 degree turn to the right. Next story above must already exist." },
	{ "Troop Station", "Location for a troop to stand while on duty. Troop stations are secondary to the fire control station when troops go to stations." },
	{ "Light Pole", "Illuminates an area." },
};

const BuildingIndustryInfo s_Industries[96] =
{
	{ 1, "Town Square", 16, 0, 1 },
	{ 2, "Road", 0, 0, 1 },
	{ 3, "Design Studio", 15, 15, 1 },
	{ 4, "Warehouse", 7, 0, 1 },
	{ 5, "Landscape", 0, 99, 0 },
	{ 10, "Military Flag", 12, 0, 1 },
	{ 11, "Military Weapon System", 15, 0, 1 },
	{ 12, "Military Shield Projector", 15, 0, 1 },
	{ 18, "Space Marine Barracks", 0, 99, 0 },
	{ 19, "Military Guard Post", 0, 99, 0 },
	{ 20, "House", 3, 3, 1 },
	{ 21, "Condominium", 15, 31, 1 },
	{ 22, "Apartment", 15, 31, 1 },
	{ 27, "Military Barracks", 0, 99, 0 },
	{ 28, "Military Headquarters", 0, 99, 0 },
	{ 29, "Military Radar", 0, 99, 0 },
	{ 30, "Farm", 3, 1, 1 },
	{ 31, "Logging Camp", 1, 34, 1 },
	{ 32, "Orchard", 7, 1, 1 },
	{ 34, "Wharf", 7, 0, 1 },
	{ 35, "Dairy", 15, 7, 1 },
	{ 40, "Bakery", 15, 7, 1 },
	{ 41, "Brewery", 15, 7, 1 },
	{ 42, "Distillery", 15, 7, 1 },
	{ 43, "Grist Mill", 15, 7, 1 },
	{ 44, "Lumber Mill", 15, 7, 1 },
	{ 45, "Meat Processing Plant", 15, 7, 1 },
	{ 46, "Sugar Mill", 15, 7, 1 },
	{ 47, "Textile Mill", 15, 7, 1 },
	{ 48, "Winery", 15, 7, 1 },
	{ 50, "Mine", 38, 32, 1 },
	{ 51, "Well", 37, 1, 1 },
	{ 52, "Drilling Rig", 0, 99, 0 },
	{ 60, "Coal Power Plant", 15, 3, 1 },
	{ 61, "Gasoline Power Plant", 15, 3, 1 },
	{ 62, "Hydroelectric Power Plant", 39, 3, 1 },
	{ 63, "Hydrogen Power Plant", 15, 3, 1 },
	{ 64, "Natural Gas Power Plant", 15, 3, 1 },
	{ 65, "Nuclear Power Plant", 15, 3, 1 },
	{ 66, "Solar Power Plant", 5, 3, 1 },
	{ 67, "Wind Power Plant", 5, 3, 1 },
	{ 68, "Nuclear Power Plant", 0, 99, 0 },
	{ 70, "Aircraft Factory", 15, 0, 1 },
	{ 71, "Armorer", 15, 7, 1 },
	{ 72, "Carpenter", 15, 7, 1 },
	{ 73, "Clothing Factory", 15, 7, 1 },
	{ 74, "Computer Factory", 15, 7, 1 },
	{ 75, "Electronics Factory", 15, 7, 1 },
	{ 76, "Explosives Factory", 15, 7, 1 },
	{ 77, "Weapon Smith", 15, 7, 1 },
	{ 78, "Jeweler", 15, 7, 1 },
	{ 79, "Machine Shop", 15, 7, 1 },
	{ 80, "Munitions Factory", 15, 7, 1 },
	{ 81, "Pharmaceutical Lab", 15, 7, 1 },
	{ 82, "Plastics Factory", 15, 7, 1 },
	{ 83, "Refinery", 15, 7, 1 },
	{ 84, "Rocket Motor Factory", 15, 7, 1 },
	{ 85, "Ship Yard", 7, 0, 1 },
	{ 86, "Smelter", 15, 7, 1 },
	{ 87, "Spacecraft Factory", 15, 0, 1 },
	{ 88, "Vehicle Factory", 15, 0, 1 },
	{ 89, "Treatment Plant", 7, 3, 1 },
	{ 90, "Nuclear Fuel Plant", 0, 99, 0 },
	{ 100, "Bank", 15, 31, 1 },
	{ 101, "Broker", 15, 31, 1 },
	{ 102, "Casino", 15, 31, 1 },
	{ 103, "Grocery", 15, 31, 1 },
	{ 104, "Retail Store", 15, 31, 1 },
	{ 105, "Cantina", 15, 31, 1 },
	{ 109, "Airport Radar", 0, 99, 0 },
	{ 110, "Airport Repair Shop", 15, 1, 1 },
	{ 111, "Airport Terminal", 15, 0, 1 },
	{ 112, "Arena", 7, 0, 1 },
	{ 113, "Zoo", 3, 0, 1 },
	{ 114, "Park", 0, 0, 1 },
	{ 119, "Private Security Contractor", 0, 99, 0 },
	{ 120, "Church", 15, 31, 1 },
	{ 121, "Hospital", 15, 31, 1 },
	{ 122, "Fire Station", 15, 3, 1 },
	{ 123, "Capitol", 15, 31, 1 },
	{ 124, "Police Station", 15, 3, 1 },
	{ 125, "University", 15, 7, 1 },
	{ 126, "Hangar", 15, 0, 1 },
	{ 127, "Observatory", 15, 0, 1 },
	{ 128, "Military Gun Tower", 15, 0, 1 },
	{ 129, "Biodome", 0, 0, 1 },
	{ 130, "Mass Media", 15, 31, 1 },
	{ 131, "Fence/Wall", 0, 99, 0 },
	{ 132, "Bridge", 0, 99, 0 },
	{ 133, "Tunnel", 0, 99, 0 },
	{ 134, "Military Airport Terminal", 0, 99, 0 },
	{ 135, "Military Motor Pool", 0, 99, 0 },
	{ 136, "Military Dock", 0, 99, 0 },
	{ 137, "Shed", 0, 99, 0 },
	{ 138, "Storage Tank", 0, 99, 0 },
	{ 139, "Star Fleet Academy", 0, 99, 0 },
};

}

const char* RoomTypeBuildingName(int type)
{
	return type >= 0 && type < 25 ? s_BuildingRooms[type].name : 0;
}

const char* RoomTypeBuildingDesc(int type)
{
	return type >= 0 && type < 25 ? s_BuildingRooms[type].desc : 0;
}

const char* DetailTypeBuildingName(int type)
{
	if(IsSpot(type))
		return SpotName(type);
	return type >= 0 && type < 143 ? s_BuildingDetails[type].name : 0;
}

const char* DetailTypeBuildingDesc(int type)
{
	if(IsSpot(type))
		return SpotDesc(type);
	return type >= 0 && type < 143 ? s_BuildingDetails[type].desc : 0;
}

int BuildingIndustryCount()
{
	return (int)(sizeof(s_Industries) / sizeof(s_Industries[0]));
}

const BuildingIndustryInfo& BuildingIndustryAt(int index)
{
	return s_Industries[index];
}

const BuildingIndustryInfo* BuildingIndustryInfoOf(int industry)
{
	for(int i = 0; i < BuildingIndustryCount(); i++)
		if(s_Industries[i].id == industry)
			return &s_Industries[i];
	return 0;
}

}
