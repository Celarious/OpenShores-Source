#ifndef AuLegacyShipStats_H
#define AuLegacyShipStats_H

#include "aulegacyship.h"
#include "aupoint.h"

#include <QMap>
#include <QStringList>
#include <array>

namespace AuLegacyShip
{
struct WeaponBayStats
{
	quint64 volume = 0;
	int units = 0;
	int emitters = 0;
};

// Original designer quantities use feet, cubic feet and pounds.
struct Stats
{
	std::array<quint64, 256> roomVolumes{};
	std::array<int, 256> roomCounts{};
	std::array<int, 256> detailCounts{};
	quint64 hullVolume = 0;
	quint64 equipmentMass = 0;
	int unknownVehicleMasses = 0;
	int unknownDetailTypes = 0;
	int crewBerths = 0;
	int officerBerths = 0;
	int passengerBerths = 0;
	int troopBerths = 0;
	int passengerClass = 0;
	int officerStations = 0;
	int crewStations = 0;
	int drivers = 0;
	int pilots = 0;
	int crewRequired = 0;
	int consoles = 0;
	int turrets = 0;
	int surgeries = 0;
	int gameTables = 0;
	int parkingSpots = 0;
	int troopStations = 0;
	int maneuverUnits = 0;
	int powerPlantUnits = 0;
	int sensorUnits = 0;
	int shieldUnits = 0;
	int weaponUnits = 0;
	int ftlUnits = 0;
	int lifeSupportUnits = 0;
	int transporterPads = 0;
	int operableDoors = 0;
	int hullOpenings = 0;
	bool hasHelm = false;
	bool hasGalley = false;
	bool hasLounge = false;
	bool hasHull = false;
	bool hullContiguous = false;
	int minimumDeck = 0;
	int maximumDeck = 0;
	double width = 0;
	double length = 0;
	double height = 0;
	QPointF hullCenter;
	int equipmentTechLevel = 0;
	int equipmentTechSource = 0;
	QMap<quint8, WeaponBayStats> weaponBays;
	QMap<quint16, int> transporterRooms;
	QMap<quint16, int> vehicleComplement;
	QStringList diagnostics;
};

Stats CalculateStats(const Design& design);
// AuAtom13:1803dc790, in author coordinates. Stations require authored emitters.
QVector<AuPoint> ContrailEmitterLocations(const Design& design, const Stats& stats, bool station);
}

#endif // AuLegacyShipStats_H
