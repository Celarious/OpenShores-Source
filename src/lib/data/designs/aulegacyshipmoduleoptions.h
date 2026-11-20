#ifndef AuLegacyShipModuleOptions_H
#define AuLegacyShipModuleOptions_H

#include "dbcommodity.h"

namespace AuLegacyShip
{
// DqDesignAnalysis::ModuleSystem, AuData13.dll 0x180074bb0.
inline SYSTEM DesignerModuleSystem(quint16 commodity)
{
	switch(commodity)
	{
	case 0xea: return SYS_CommandAndControl;
	case 0xeb: case 0xec: return SYS_ManeuverDrive;
	case 0xed: case 0x118: case 0x119: return SYS_Hull;
	case 0xee: return SYS_LifeSupport;
	case 0xef: return SYS_PowerPlant;
	case 0xf0: case 0xf1: case 0xf2: return SYS_Shield;
	case 0xf3: case 0xf4: case 0xf5: return SYS_SurgeryUnit;
	case 0xf6: case 0xf7: return SYS_Transporter;
	case 0xf8: case 0xfa: case 0x106: return SYS_Turret;
	case 0xfc: case 0xfe: case 0x100: case 0x101: case 0x102: case 0x105:
	case 0x11b: case 0x121: case 0x123: case 0x13f: return SYS_Weapon;
	case 0x103: case 0x104: return SYS_FTLDrive;
	case 0x107: return SYS_Sensor;
	default: return SYS_Count;
	}
}

inline bool IsDesignerModule(SYSTEM system, quint16 commodity, bool building)
{
	if(system == SYS_Count || DesignerModuleSystem(commodity) != system) return false;
	// DqDesignAnalysis::IsBuildingModule excludes the five orbital harvesting bays.
	if(building)
		switch(commodity)
		{
		case 0x100: case 0x101: case 0x11b: case 0x121: case 0x123: return false;
		}
	return true;
}

// DaAtom::DesignNew initializes these six industry modules before allocating
// the new working design.
inline quint16 DefaultBuildingModule(quint8 industry)
{
	switch(industry)
	{
	case 11: return 0x105;
	case 12: return 0xf0;
	case 111: return 0x107;
	case 121: return 0xf3;
	case 123: return 0xea;
	case 128: return 0x106;
	default: return 0;
	}
}

// AuDesignBuilding's module button, Shores of Hazeron.exe 0x14017b5c0.
inline SYSTEM BuildingModuleSystem(quint8 industry)
{
	return DesignerModuleSystem(DefaultBuildingModule(industry));
}

inline bool IsBuildingModule(quint8 industry, quint16 commodity)
{
	return IsDesignerModule(BuildingModuleSystem(industry), commodity, true);
}
}

#endif
