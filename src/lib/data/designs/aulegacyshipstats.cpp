#include "aulegacyshipstats.h"

#include "aulegacyshipdetails.h"
#include "aupoint.h"

#include <QHash>
#include <algorithm>
#include <cmath>
#include <limits>

namespace AuLegacyShip
{
namespace
{
struct EquipmentType
{
	quint16 massKg;
	quint8 techLevel;
	quint8 crew;
};

// GD 0x3a detail records, preserved in research/legacy-spacecraft-2016/detail-types.json.
constexpr EquipmentType EquipmentTypes[] = {
	{0, 0, 0}, // 0: None
	{80, 0, 1}, // 1: Navigator Station
	{120, 0, 0}, // 2: Double Berth
	{120, 0, 0}, // 3: Large Hull Door
	{80, 2, 1}, // 4: Medic Station
	{10, 0, 0}, // 5: Door
	{80, 0, 1}, // 6: Engineer Station
	{80, 1, 1}, // 7: Fire Control Station
	{40, 0, 0}, // 8: Hatch
	{0, 6, 0}, // 9: Hull Opening
	{2250, 0, 0}, // 10: Maneuver Drive
	{200, 2, 0}, // 11: Surgery Unit
	{80, 0, 1}, // 12: Helm
	{80, 0, 1}, // 13: Power Relay Station
	{800, 0, 0}, // 14: Power Plant
	{80, 1, 1}, // 15: Sensor Station
	{450, 1, 0}, // 16: Sensors
	{80, 3, 1}, // 17: Shield Station
	{1350, 3, 0}, // 18: Shields
	{80, 6, 1}, // 19: Transporter Station
	{20, 6, 0}, // 20: Transporter Pad
	{1000, 0, 1}, // 21: Turret (Side)
	{1000, 0, 1}, // 22: Turret (Top/Bottom)
	{1800, 1, 0}, // 23: Weapon System
	{700, 0, 0}, // 24: FTL Drive
	{120, 0, 0}, // 25: Double Berth
	{120, 0, 0}, // 26: Double Berth
	{120, 0, 0}, // 27: Double Berth
	{120, 0, 0}, // 28: Double Berth
	{120, 0, 0}, // 29: Double Berth
	{120, 0, 0}, // 30: Double Berth
	{120, 0, 0}, // 31: Double Berth
	{250, 0, 0}, // 32: Life Support
	{1000, 0, 1}, // 33: Turret (Top/Bottom)
	{1000, 0, 1}, // 34: Turret (Side)
	{1000, 0, 1}, // 35: Turret (Side)
	{1000, 0, 1}, // 36: Turret (Side)
	{1000, 0, 1}, // 37: Turret (Side)
	{1000, 0, 1}, // 38: Turret (Side)
	{1000, 0, 1}, // 39: Turret (Side)
	{1000, 0, 1}, // 40: Turret (Side)
	{0, 0, 0}, // 41: Space Fighter Location
	{0, 0, 0}, // 42: Space Fighter Location
	{0, 0, 0}, // 43: Space Fighter Location
	{0, 0, 0}, // 44: Space Fighter Location
	{0, 0, 0}, // 45: Space Fighter Location
	{0, 0, 0}, // 46: Space Fighter Location
	{0, 0, 0}, // 47: Space Fighter Location
	{0, 0, 0}, // 48: Space Fighter Location
	{20, 0, 0}, // 49: Crew Door
	{30, 0, 0}, // 50: Hull Door
	{50, 0, 0}, // 51: Crew Hatch
	{60, 0, 0}, // 52: Hull Hatch
	{70, 0, 0}, // 53: Large Door
	{90, 0, 0}, // 54: Large Crew Door
	{0, 0, 0}, // 55: Wall Opening
	{0, 0, 0}, // 56: Railing
	{0, 0, 1}, // 57: Vehicle Location
	{0, 0, 1}, // 58: Vehicle Location
	{0, 0, 1}, // 59: Vehicle Location
	{0, 0, 1}, // 60: Vehicle Location
	{0, 0, 1}, // 61: Vehicle Location
	{0, 0, 1}, // 62: Vehicle Location
	{0, 0, 1}, // 63: Vehicle Location
	{0, 0, 1}, // 64: Vehicle Location
	{70, 0, 0}, // 65: Single Berth
	{70, 0, 0}, // 66: Single Berth
	{70, 0, 0}, // 67: Single Berth
	{70, 0, 0}, // 68: Single Berth
	{70, 0, 0}, // 69: Single Berth
	{70, 0, 0}, // 70: Single Berth
	{70, 0, 0}, // 71: Single Berth
	{70, 0, 0}, // 72: Single Berth
	{40, 0, 0}, // 73: Captain's Chair
	{90, 0, 0}, // 74: Game Table
	{90, 0, 0}, // 75: Game Table
	{0, 0, 0}, // 76: Pivot Point
	{40, 0, 0}, // 77: Hatch
	{40, 0, 0}, // 78: Hatch
	{40, 0, 0}, // 79: Hatch
	{40, 0, 0}, // 80: Hatch
	{40, 0, 0}, // 81: Hatch
	{40, 0, 0}, // 82: Hatch
	{40, 0, 0}, // 83: Hatch
	{50, 0, 0}, // 84: Crew Hatch
	{50, 0, 0}, // 85: Crew Hatch
	{50, 0, 0}, // 86: Crew Hatch
	{50, 0, 0}, // 87: Crew Hatch
	{50, 0, 0}, // 88: Crew Hatch
	{50, 0, 0}, // 89: Crew Hatch
	{50, 0, 0}, // 90: Crew Hatch
	{60, 0, 0}, // 91: Hull Hatch
	{60, 0, 0}, // 92: Hull Hatch
	{60, 0, 0}, // 93: Hull Hatch
	{60, 0, 0}, // 94: Hull Hatch
	{60, 0, 0}, // 95: Hull Hatch
	{60, 0, 0}, // 96: Hull Hatch
	{60, 0, 0}, // 97: Hull Hatch
	{0, 0, 0}, // 98: Contrail Emitter
	{0, 0, 0}, // 99: Weapon Emitter
	{0, 0, 0}, // 100: Flag Pole
	{100, 0, 0}, // 101: Stairs, Straight
	{100, 0, 0}, // 102: Stairs, Straight
	{100, 0, 0}, // 103: Stairs, Straight
	{100, 0, 0}, // 104: Stairs, Straight
	{100, 0, 0}, // 105: Stairs, Straight
	{100, 0, 0}, // 106: Stairs, Straight
	{100, 0, 0}, // 107: Stairs, Straight
	{100, 0, 0}, // 108: Stairs, Straight
	{100, 0, 0}, // 109: Stairs, Turn Left
	{100, 0, 0}, // 110: Stairs, Turn Left
	{100, 0, 0}, // 111: Stairs, Turn Left
	{100, 0, 0}, // 112: Stairs, Turn Left
	{100, 0, 0}, // 113: Stairs, Turn Left
	{100, 0, 0}, // 114: Stairs, Turn Left
	{100, 0, 0}, // 115: Stairs, Turn Left
	{100, 0, 0}, // 116: Stairs, Turn Left
	{100, 0, 0}, // 117: Stairs, Turn Right
	{100, 0, 0}, // 118: Stairs, Turn Right
	{100, 0, 0}, // 119: Stairs, Turn Right
	{100, 0, 0}, // 120: Stairs, Turn Right
	{100, 0, 0}, // 121: Stairs, Turn Right
	{100, 0, 0}, // 122: Stairs, Turn Right
	{100, 0, 0}, // 123: Stairs, Turn Right
	{100, 0, 0}, // 124: Stairs, Turn Right
	{100, 0, 0}, // 125: Stairs, U-turn Left
	{100, 0, 0}, // 126: Stairs, U-turn Left
	{100, 0, 0}, // 127: Stairs, U-turn Left
	{100, 0, 0}, // 128: Stairs, U-turn Left
	{100, 0, 0}, // 129: Stairs, U-turn Left
	{100, 0, 0}, // 130: Stairs, U-turn Left
	{100, 0, 0}, // 131: Stairs, U-turn Left
	{100, 0, 0}, // 132: Stairs, U-turn Left
	{100, 0, 0}, // 133: Stairs, U-turn Right
	{100, 0, 0}, // 134: Stairs, U-turn Right
	{100, 0, 0}, // 135: Stairs, U-turn Right
	{100, 0, 0}, // 136: Stairs, U-turn Right
	{100, 0, 0}, // 137: Stairs, U-turn Right
	{100, 0, 0}, // 138: Stairs, U-turn Right
	{100, 0, 0}, // 139: Stairs, U-turn Right
	{100, 0, 0}, // 140: Stairs, U-turn Right
	{0, 0, 0}, // 141: Troop Station
	{0, 0, 0}, // 142: Light Pole
};

struct RoomGeometry
{
	AuPointArray boundary;
	QRectF bounds;
	int deck = 0;
};

bool BoundariesTouch(const RoomGeometry& a, const RoomGeometry& b)
{
	constexpr double fuzz = 0.00001;
	if(a.bounds.left() > b.bounds.right() + fuzz || b.bounds.left() > a.bounds.right() + fuzz
		|| a.bounds.top() > b.bounds.bottom() + fuzz || b.bounds.top() > a.bounds.bottom() + fuzz)
		return false;
	for(const AuPoint& point : a.boundary)
		if(b.boundary.PointInBoundary(point) || b.boundary.PointOnBoundary_2D(point))
			return true;
	for(const AuPoint& point : b.boundary)
		if(a.boundary.PointInBoundary(point) || a.boundary.PointOnBoundary_2D(point))
			return true;
	return false;
}

bool HullIsContiguous(const QVector<RoomGeometry>& rooms)
{
	if(rooms.isEmpty())
		return false;
	QMap<int, QVector<int>> decks;
	for(int i = 0; i < rooms.size(); ++i)
		decks[rooms[i].deck].append(i);
	for(auto it = decks.begin(); it != decks.end(); ++it)
		std::sort(it.value().begin(), it.value().end(), [&rooms](int a, int b) {
			return rooms[a].bounds.left() < rooms[b].bounds.left();
		});
	QVector<bool> visited(rooms.size(), false);
	QVector<int> pending;
	pending.append(0);
	visited[0] = true;
	for(int next = 0; next < pending.size(); ++next)
	{
		const RoomGeometry& room = rooms[pending[next]];
		for(int deck = room.deck - 1; deck <= room.deck + 1; ++deck)
		{
			const auto found = decks.constFind(deck);
			if(found == decks.constEnd())
				continue;
			for(int candidate : found.value())
			{
				if(rooms[candidate].bounds.left() > room.bounds.right() + 0.00001)
					break;
				if(!visited[candidate] && BoundariesTouch(room, rooms[candidate]))
				{
					visited[candidate] = true;
					pending.append(candidate);
				}
			}
		}
	}
	return pending.size() == rooms.size();
}

quint64 VehicleMass(quint16 commodity, bool& known)
{
	// DaVehicle::Mass, AuAtom13.dll 1800f70b0, then AnalyzeDesign's integer cast.
	known = true;
	switch(commodity)
	{
	case 0: return 0;
	case 6: return 5511;
	case 7: case 8: case 77: return 22046;
	case 9: return 55115;
	case 28: case 82: return 6400;
	case 104: return 66138;
	case 132: return 320;
	case 133: return 230;
	case 231: return 26455;
	default: known = false; return 0;
	}
}

bool IsConsole(int type)
{
	return type == 1 || type == 4 || type == 6 || type == 7 || type == 12 || type == 13
		|| type == 15 || type == 17 || type == 19 || type == 73;
}
}

QVector<AuPoint> ContrailEmitterLocations(const Design& design, const Stats& stats, bool station)
{
	QVector<AuPoint> positions;
	for(const auto& detail : design.details)
	{
		if(detail.detailType != 98) continue;
		const QPointF xy = DecodePoint(detail.packedXY);
		positions.append(AuPoint(xy.x(), xy.y() - (station ? 0 : stats.width / 10),
			detail.deck * AuLegacyShip::DeckHeight + (station ? -4 : 4)));
		if(positions.size() == 10) break;
	}
	if(positions.isEmpty() && !station)
	{
		// Original fallback: center, port and starboard aft emitters.
		const double y = stats.hullCenter.y() - stats.length / 2 - stats.width / 10;
		const double z = (stats.minimumDeck + stats.maximumDeck) * AuLegacyShip::DeckHeight / 2;
		for(double x : {0.0, -stats.width / 3, stats.width / 3})
			positions.append(AuPoint(stats.hullCenter.x() + x, y, z));
	}
	return positions;
}

Stats CalculateStats(const Design& design)
{
	Stats stats;
	QHash<quint16, const Room*> roomsById;
	QVector<RoomGeometry> geometry;
	geometry.reserve(design.rooms.size());
	QRectF hullBounds;
	int invalidRooms = 0;
	int duplicateRooms = 0;
	for(const Room& room : design.rooms)
	{
		// Empty records are retained by the inspector; the original array reader dropped them.
		const QPolygonF polygon = RoomBoundary(room);
		if(polygon.isEmpty())
			continue;
		if(roomsById.contains(room.roomId))
			++duplicateRooms;
		else
			roomsById.insert(room.roomId, &room);
		++stats.roomCounts[room.roomType];
		RoomGeometry item;
		item.deck = room.deck;
		item.bounds = polygon.boundingRect();
		for(const QPointF& point : polygon)
			item.boundary.append(AuPoint(point.x(), point.y(), 0.0));
		const double volume = std::round(item.boundary.Area() * DeckHeight);
		if(!std::isfinite(volume) || volume < 0.0
			|| volume >= double(std::numeric_limits<quint64>::max() / MaximumRecords))
		{
			++invalidRooms;
			item.boundary.clear();
		}
		else
		{
			stats.roomVolumes[room.roomType] += quint64(volume);
			stats.hullVolume += quint64(volume);
			if(polygon.size() < 3 || volume == 0.0)
				++invalidRooms;
			if(room.roomType == 21)
				stats.weaponBays[room.roomSubtype].volume += quint64(volume);
		}
		if(room.roomType == 10)
			stats.hasGalley = true;
		if(room.roomType == 14)
			stats.hasLounge = true;
		// The original excludes building-only room types 22..24 from ship extents.
		if(!(room.roomType >= 22 && room.roomType <= 24) && !item.boundary.isEmpty())
		{
			if(!stats.hasHull)
			{
				hullBounds = item.bounds;
				stats.minimumDeck = stats.maximumDeck = room.deck;
				stats.hasHull = true;
			}
			else
			{
				hullBounds = hullBounds.united(item.bounds);
				stats.minimumDeck = std::min(stats.minimumDeck, int(room.deck));
				stats.maximumDeck = std::max(stats.maximumDeck, int(room.deck));
			}
		}
		geometry.append(item);
	}
	stats.hullContiguous = HullIsContiguous(geometry);
	if(stats.hasHull)
	{
		stats.width = hullBounds.width();
		stats.length = hullBounds.height();
		stats.hullCenter = hullBounds.center();
		// AnalyzeDesign extends the extrema one full deck above and below.
		stats.height = (stats.maximumDeck - stats.minimumDeck + 2) * DeckHeight;
	}

	int orphanDetails = 0;
	for(const Detail& detail : design.details)
	{
		const int type = detail.detailType;
		++stats.detailCounts[type];
		if(type >= int(sizeof(EquipmentTypes) / sizeof(EquipmentTypes[0])))
		{
			++stats.unknownDetailTypes;
			continue;
		}
		const EquipmentType& equipment = EquipmentTypes[type];
		stats.equipmentMass += quint64(int(float(equipment.massKg) * 2.2046225f));
		if(equipment.techLevel > stats.equipmentTechLevel
			|| (equipment.techLevel != 0 && equipment.techLevel == stats.equipmentTechLevel
				&& type < stats.equipmentTechSource))
		{
			stats.equipmentTechLevel = equipment.techLevel;
			stats.equipmentTechSource = type;
		}
		const Room* room = roomsById.value(detail.roomId, nullptr);
		if(!room && type != 76 && type != 98 && type != 100 && type != 142)
			++orphanDetails;
		const int canonical = CanonicalDetailType(type);
		if(canonical == 57)
		{
			bool known = false;
			stats.equipmentMass += VehicleMass(detail.commodityIdNormalized, known);
			if(!known)
				++stats.unknownVehicleMasses;
			++stats.parkingSpots;
			++stats.vehicleComplement[detail.commodityIdNormalized];
			if((detail.commodityIdNormalized >= 6 && detail.commodityIdNormalized <= 9)
				|| detail.commodityIdNormalized == 231)
				++stats.pilots;
			else
				++stats.drivers;
		}
		if(room && (canonical == 2 || canonical == 65))
		{
			const int berths = canonical == 2 ? 2 : 1;
			switch(room->roomType)
			{
			case 1: stats.troopBerths += berths; break;
			case 4: stats.officerBerths += berths; break;
			case 7: stats.crewBerths += berths; break;
			case 19: stats.passengerBerths += berths; break;
			default: break;
			}
		}
		if(room && IsConsole(type))
		{
			++stats.consoles;
			if(type == 73)
				++stats.officerStations;
			else
				++stats.crewStations;
		}
		if(type == 12)
			stats.hasHelm = true;
		if(room && (canonical == 21 || canonical == 22))
			++stats.turrets;
		if(room && type == 11)
			++stats.surgeries;
		if(room && canonical == 74)
			++stats.gameTables;
		if(type == 141)
			++stats.troopStations;
		switch(canonical)
		{
		case 10: ++stats.maneuverUnits; break;
		case 14: ++stats.powerPlantUnits; break;
		case 15: case 16: ++stats.sensorUnits; break;
		case 17: case 18: ++stats.shieldUnits; break;
		case 23:
			++stats.weaponUnits;
			if(room)
				++stats.weaponBays[room->roomSubtype].units;
			break;
		case 24: ++stats.ftlUnits; break;
		case 32: ++stats.lifeSupportUnits; break;
		case 20:
			++stats.transporterPads;
			if(room)
				++stats.transporterRooms[quint16(quint16(quint8(room->deck)) << 8) | room->roomSubtype];
			break;
		case 99:
			if(room)
				++stats.weaponBays[room->roomSubtype].emitters;
			break;
		default: break;
		}
		if(canonical == 3 || canonical == 5 || canonical == 8 || canonical == 49
			|| canonical == 50 || canonical == 51 || canonical == 52
			|| canonical == 53 || canonical == 54)
			++stats.operableDoors;
		if(canonical == 3 || canonical == 9 || canonical == 50 || canonical == 52)
			++stats.hullOpenings;
	}
	stats.crewRequired = stats.officerStations + stats.crewStations + stats.drivers
		+ stats.pilots + stats.turrets + stats.troopStations;
	if(stats.passengerBerths)
		stats.passengerClass = stats.hasGalley ? (stats.hasLounge ? 1 : 2) : 3;
	const int berthTechLevel = std::max(0, stats.crewBerths + stats.passengerBerths + stats.troopBerths - 8);
	if(berthTechLevel > stats.equipmentTechLevel
		|| (berthTechLevel == stats.equipmentTechLevel && stats.equipmentTechSource > 2))
	{
		stats.equipmentTechLevel = std::min(255, berthTechLevel);
		stats.equipmentTechSource = 2;
	}
	if(invalidRooms)
		stats.diagnostics.append(QStringLiteral("%1 room boundaries have no usable volume.").arg(invalidRooms));
	if(duplicateRooms)
		stats.diagnostics.append(QStringLiteral("%1 duplicate room IDs use the first matching room for equipment.").arg(duplicateRooms));
	if(orphanDetails)
		stats.diagnostics.append(QStringLiteral("%1 equipment records have no matching room.").arg(orphanDetails));
	if(stats.unknownDetailTypes)
		stats.diagnostics.append(QStringLiteral("%1 equipment types are absent from the recovered 2016 table.").arg(stats.unknownDetailTypes));
	if(stats.unknownVehicleMasses)
		stats.diagnostics.append(QStringLiteral("%1 vehicle markers use the original zero-mass fallback.").arg(stats.unknownVehicleMasses));
	return stats;
}
}
