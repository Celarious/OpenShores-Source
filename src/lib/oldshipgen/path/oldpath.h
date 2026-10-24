#ifndef OLDPATH_H
#define OLDPATH_H

#include "oldship.h"

#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace OldPath
{

using OldShip::Design;
using OldShip::Room;
using OldShip::Detail;

const double GridUnit = 2.8284271245;
const double HalfGridUnit = 1.41421356225;
const double DeckHeight = 8.0;

struct Point
{
	double x, y, z;
	Point() : x(0), y(0), z(0) {}
	Point(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
};

inline void HalfGridCoord(const Point& p, int& x, int& y)
{
	x = (int)std::round(p.x / GridUnit) + 0x80;
	y = (int)std::round(p.y / GridUnit) + 0x80;
}

inline int OnDeck(double z)
{
	int d = (int)std::floor((z + 0.05) * 0.125);
	if(d < -0x7f)
		return -0x7f;
	return d > 0x7f ? 0x7f : d;
}

inline uint32_t MakeKey(int x, int y, int deck)
{
	return (uint32_t)(uint8_t)y | ((uint32_t)(uint8_t)x << 8) | ((uint32_t)(int32_t)(int8_t)deck << 16);
}

inline uint32_t MakeKey(const Point& p)
{
	int x, y;
	HalfGridCoord(p, x, y);
	return MakeKey(x, y, OnDeck(p.z));
}

inline int KeyX(uint32_t k) { return (k >> 8) & 0xff; }
inline int KeyY(uint32_t k) { return k & 0xff; }
inline int KeyDeck(uint32_t k) { return (int8_t)(k >> 16); }

inline Point CellCenter(int x, int y, int deck)
{
	return Point((x - 0x80) * GridUnit, (y - 0x80) * GridUnit, deck * DeckHeight);
}

struct Opening
{
	int32_t		id;
	uint32_t	type;
	int8_t		deck;
	int8_t		deck2;
	int16_t		room1;
	int16_t		room2;
	double		a[3];
	double		b[3];
	double		loc[3];
	Opening() : id(0), type(0), deck(0), deck2(0), room1(0), room2(0)
	{
		for(int i=0; i<3; i++)
			a[i] = b[i] = loc[i] = 0;
	}
	void Bytes(std::vector<uint8_t>& out) const;
};

struct Openings
{
	std::vector<Opening>	doors, holes, railings;
	int						lowDeck, highDeck;
	Openings() : lowDeck(0), highDeck(0) {}
};

class FloorBuffer82
{
public:
	FloorBuffer82(uint8_t fill = 0) : m(0x10000, fill) {}
	void Clear() { std::fill(m.begin(), m.end(), 0); }
	uint8_t Data(unsigned x, unsigned y) const { return m[(uint16_t)(x * 0x100 + y)]; }
	void SetData(unsigned x, unsigned y, uint8_t v) { m[(uint16_t)(x * 0x100 + y)] = v; }
	uint8_t* Raw() { return &m[0]; }
	const uint8_t* Raw() const { return &m[0]; }
private:
	std::vector<uint8_t> m;
};

struct RoomArray;
struct Path;

struct ExtantCell
{
	uint8_t		type;
	uint16_t	rooms[8];
	ExtantCell() : type(0) { for(int i=0; i<8; i++) rooms[i] = 0; }
	void Bytes(std::vector<uint8_t>& out) const;
	void FromBytes(const uint8_t* b);

	uint16_t InsideRoom(const Point& p, const RoomArray& rooms) const;
	int InsideRoomType(const Point& p, const RoomArray& rooms) const;
};

struct PathableCell
{
	uint32_t	dirs;
	uint8_t		type;
	int16_t		door;
	int32_t		hole;
	uint16_t	flags;
	int32_t		railing;
	uint16_t	room1;
	uint16_t	room2;
	uint8_t		stairDir;
	PathableCell() : dirs(0x200), type(0), door(0), hole(0), flags(0), railing(0), room1(0), room2(0), stairDir(0) {}
	void Bytes(std::vector<uint8_t>& out) const;
	void FromBytes(const uint8_t* b);
};

extern const uint32_t DirBit[27];

struct Direction { int8_t dx, dy; uint32_t bit; };
extern const Direction Directions[8];

struct PathCell
{
	uint8_t							type;
	std::map<uint32_t, uint64_t>	links;
	int16_t							door;
	int32_t							hole;
	uint16_t						stairFlags;
	int32_t							railing;
	uint16_t						room1;
	uint16_t						room2;
	std::set<uint32_t>				via1;
	std::set<uint32_t>				via2;
	PathCell() : type(0), door(0), hole(0), stairFlags(0), railing(0), room1(0), room2(0) {}
	void Bytes(std::vector<uint8_t>& out) const;
};

struct RoomOutline
{
	uint16_t			id;
	int8_t				deck;
	uint8_t				type;
	std::vector<Point>	points;
	RoomOutline() : id(0), deck(0), type(0) {}
};

struct RoomArray
{
	std::vector<RoomOutline>	rooms;

	void FromDesign(const Design& design);
	const RoomOutline* Find(uint16_t id) const;
	bool DoesSegmentIntersectRoom(const Point& a, const Point& b, uint16_t r1, uint16_t r2, const Point& ref,
		Point& normal) const;
	bool DoesSegmentIntersectRoom(const Point& a, const Point& b, uint16_t r1, uint16_t r2, const Point& ref,
		Point& normal, Point& hit) const;
};

class DoorStates
{
public:
	DoorStates() {}
	void SetDoorCount(unsigned count) { open.resize((count + 3) / 4 * 4, false); }
	void OpenDoor(unsigned i) { if(i / 4 < open.size() / 4) open[i] = true; }
	void CloseDoor(unsigned i) { if(i / 4 < open.size() / 4) open[i] = false; }
	bool IsDoorOpen(unsigned i) const { return i / 4 < open.size() / 4 && open[i]; }
	bool operator==(const DoorStates& o) const { return open == o.open; }
	bool operator!=(const DoorStates& o) const { return !(open == o.open); }
private:
	std::vector<bool>	open;
};

struct RouteLoc
{
	Point		pt;
	uint32_t	cell;
	int32_t		index;
	uint8_t		kind;
	RouteLoc() : cell(0), index(-1), kind(0) {}
	RouteLoc(const Point& p, int door, int hole, int railing, uint32_t key);
	bool operator==(const RouteLoc& o) const;
};

struct Route
{
	std::vector<RouteLoc>	locs;
	bool					crew;
	DoorStates				doors;
	bool					skipOpen;
	std::vector<uint32_t>	hubs;
	bool					ignoreDoors;
	uint32_t				target;
	Route() : crew(false), skipOpen(false), ignoreDoors(false), target(0) {}
	bool ContainsCellHash(uint32_t key) const;
	void ClearDestination() { locs.clear(); hubs.clear(); }
};

struct AccessibleCell
{
	uint32_t	cost;
	uint16_t	flags;
	uint8_t		stairDir;
	AccessibleCell() : cost(0), flags(0), stairDir(0) {}
};

struct Dice
{
	uint32_t	seed;
	Dice(uint32_t s = 0) : seed(s) {}
	int Roll(unsigned n, unsigned sides, int add);
};

struct DesignBounds
{
	double	minX, minY;
	double	maxX, maxY;
	int		lowDeck;
	int		highDeck;
	DesignBounds() : minX(0), minY(0), maxX(0), maxY(0), lowDeck(0), highDeck(0) {}
};

enum PathTest
{
	PathOk = 0,
	PathRefused = 1,
	PathFloorHit = 2,
	PathCeilingHit = 3,
	PathWallHit = 4,
	PathRailingHit = 0xc,
	PathObstacleHit = 0x14,
	PathDoorHit = 0x1c
};

bool FillRoom(FloorBuffer82& buf, const std::vector<Point>& outline, int& minX, int& minY, int& maxX, int& maxY);

struct StageObserver
{
	virtual ~StageObserver() {}
	virtual bool Stage(const char* stage, int deck, const Path& path, const FloorBuffer82* lattice,
			const FloorBuffer82* scratch) = 0;
};

extern uint8_t StairCellMap[80];
void UseIdentityStairCellMap();

struct Path
{
	std::map<uint32_t, ExtantCell>		extant;
	uint8_t								flag;
	std::map<uint32_t, PathableCell>	pathable;
	std::map<uint32_t, PathCell>		cells;

	bool								endKnown;
	PathableCell						endCell;

	Path() : flag(0), endKnown(false) {}

	void CreateSpacecraft(const Design& design, const Openings& openings, StageObserver* observer = 0);

	std::string Dump() const;

	PathTest TestSpacecraftPath(const Point& a, const Point& b, bool crew, const RoomArray& rooms,
		const DoorStates& doors, std::vector<uint16_t>& openList, bool noRailings, bool& onStairs) const;
	PathTest TestSpacecraftPath(const Point& a, const Point& b, bool crew, const RoomArray& rooms,
		const DoorStates& doors, std::vector<uint16_t>& openList, bool noRailings, bool& onStairs, Point& hit) const;

	int CellTypeAt(const Point& p, int8_t deck) const;
	bool IsPathable(const Point& p) const;
	int8_t DeckBelow(const Point& p, int8_t deck, bool& gravity) const;
	int8_t DeckBelow(const Point& p, int8_t deck, bool& gravity, const RoomArray& rooms) const;
	bool InsideRoom(const Point& p, int8_t deck, const RoomArray& rooms, uint16_t& room) const;
	uint16_t InsideRoom(const Point& p, const PathableCell& cell, const RoomArray& rooms) const;
	int HatchAbove(const Point& p) const;
	int HatchDoorIndex(const Point& p) const;
	int HatchDoorIndex(const Point& p, bool& crew) const;
	uint32_t FindBottomStep(uint32_t cellKey) const;

	static PathTest WallHit(const Point& normal);
	static PathTest DoorHit(const Point& normal);
	static PathTest RailingHit(const Point& normal);
	PathTest WallHit(uint32_t key, const Point& p) const;
	PathTest DoorHit(uint32_t key, const Point& p) const;
	PathTest RailingHit(uint32_t key, const Point& p) const;
	static PathTest ObstacleHit(uint32_t key, const Point& p);
	PathTest WallHit(uint32_t key, const Point& a, const Point& b, Point& hit) const;
	PathTest DoorHit(uint32_t key, const Point& a, const Point& b, Point& hit) const;
	PathTest RailingHit(uint32_t key, const Point& a, const Point& b, Point& hit) const;
	static PathTest ObstacleHit(uint32_t key, const Point& a, const Point& b, Point& hit);
	static PathTest FloorHit(const Point& a, const Point& b, int8_t deck, Point& hit);
	static PathTest CeilingHit(const Point& a, const Point& b, int8_t deck, Point& hit);
	PathTest PathBlocked(const Point& p, int8_t deckFrom, int8_t deckTo, uint32_t key, int kind) const;
	PathTest TestWallCellPath(const Point& a, bool aInCell, const Point& b, bool bInCell, uint32_t key, int8_t deck,
		const RoomArray& rooms) const;
	PathTest TestWallCellPath(const Point& a, bool aInCell, const Point& b, bool bInCell, uint32_t key, int8_t deck,
		const RoomArray& rooms, Point& hit) const;
	PathTest TestClippedCellPath(int x, int y, int deck, const Point& a, const Point& b, const RoomArray& rooms) const;
	PathTest TestClippedCellPath(int x, int y, int deck, const Point& a, const Point& b, const RoomArray& rooms,
		Point& hit) const;
	PathTest TestDiagonalCornerClip(const Point& a, const Point& b, int x, int y, int8_t deck, uint32_t cellDirs,
		uint32_t stepBit, const RoomArray& rooms) const;
	PathTest TestDiagonalCornerClip(const Point& a, const Point& b, int x, int y, int8_t deck, uint32_t cellDirs,
		uint32_t stepBit, const RoomArray& rooms, Point& hit) const;
	static bool TestStepOk(uint16_t from, uint16_t to, int8_t ddeck);

	const ExtantCell* FindExtant(uint32_t key) const;
	const PathableCell* FindPathable(uint32_t key) const;
	const PathableCell& PathableOrEnd(uint32_t key) const;
	PathableCell EndCell() const;

	//
	static const uint32_t NoCell = 0xffffffffu;

	bool SpacecraftRoute(Route& route, const Point& from, const Point& to, const RoomArray& rooms, bool landed,
		bool skipOpen, const DoorStates& doors, bool crew, bool ignoreDoors, bool railings) const;
	void ClosestHullEntrance(const Point& p, bool landed, bool ignoreDoors, const DoorStates& doors, Point& out,
		int& door, int& hole) const;
	bool RandomFloorLocation(Point& out, uint16_t room, Dice& dice) const;

	uint32_t ClosestHullEntranceKey(const Point& p, bool landed, bool ignoreDoors, const DoorStates& doors) const;
	uint32_t SpacecraftPathTerminalInfo(const Point& p, bool landed, bool ignoreDoors, const DoorStates& doors,
		bool& outside) const;
	uint32_t ClosestPathableSpacecraft(const Point& p, const RoomArray& rooms, const DoorStates& doors, bool crew)
		const;
	bool StepToPathableSpacecraft(Route& route, const Point& p, const RoomArray& rooms, const DoorStates& doors,
		bool crew) const;
	bool RouteAcrossSpacecraftRoom(Route& route, const Point& from, uint32_t a, const Point& to, uint32_t b,
		bool crew, bool skipOpen, const DoorStates& doors, const RoomArray& rooms) const;
	void RouteOnStairs(Route& route, const Point& from, uint32_t a, int8_t deckA, int8_t deckB) const;
	void RouteBacktrack(Route& route, uint16_t room, const RoomArray& rooms,
		std::map<uint32_t, AccessibleCell>& acc, uint32_t a, int8_t deckA, uint32_t b, int8_t deckB) const;
	uint32_t NextStep(uint32_t cur, uint32_t prev, const std::map<uint32_t, AccessibleCell>& acc, bool axesFirst,
		bool flat) const;
	int PathCostSpacecraft(uint32_t from, uint32_t to, bool crew, bool ignoreDoors, const DoorStates& doors,
		bool skipOpen, std::vector<uint32_t>& hubPath) const;
	void DistillBestSpacecraftPath(std::map<uint32_t, uint32_t>& hubs, uint32_t to, bool skipOpen,
		const DoorStates& doors, bool crew, bool ignoreDoors, std::vector<uint32_t>& hubPath) const;
	void AccessibleSpacecraftDoors(std::map<uint32_t, uint32_t>& hubs, uint32_t key, const DoorStates& doors,
		bool crew, bool ignoreDoors, bool railings) const;
	void DiscardInaccessibleDoors(std::map<uint32_t, uint32_t>& hubs, const DoorStates& doors, bool crew,
		bool ignoreDoors, bool railings) const;
	static uint32_t MoveCost(uint8_t type);
};

inline bool DqSpacecraftRoute(const Path& path, Route& route, const Point& from, const Point& to, bool landed,
	const RoomArray& rooms, bool skipOpen, const DoorStates& doors, bool crew, bool ignoreDoors, bool railings)
{
	return path.SpacecraftRoute(route, from, to, rooms, landed, skipOpen, doors, crew, ignoreDoors, railings);
}
inline void DqClosestHullEntrance(const Path& path, const Point& p, bool landed, bool ignoreDoors,
	const DoorStates& doors, Point& out, int& door, int& hole)
{
	path.ClosestHullEntrance(p, landed, ignoreDoors, doors, out, door, hole);
}
inline bool DqRandomFloorLocation(const Path& path, Point& out, uint16_t room, Dice& dice)
{
	return path.RandomFloorLocation(out, room, dice);
}

bool BoxClipAllowed(Point& a, Point& b);

int DqPathCellTypeAt(const Path& path, const DesignBounds& bounds, const Point& p);
int8_t DqDeckBelow(const Path& path, const Point& p, bool& gravity);
int8_t DqDeckBelow(const Path& path, const Point& p, bool& gravity, const RoomArray& rooms);
bool DqInsideRoom(const Path& path, const DesignBounds& bounds, const Point& p, const RoomArray& rooms,
	uint16_t& room);
PathTest DqTestSpacecraftPath(const Path& path, const Point& a, const Point& b, bool crew, const RoomArray& rooms,
	const DoorStates& doors, std::vector<uint16_t>& openList, bool noRailings);
PathTest DqTestSpacecraftPath(const Path& path, const Point& a, const Point& b, bool crew, const RoomArray& rooms,
	const DoorStates& doors, std::vector<uint16_t>& openList, bool noRailings, Point& hit);

void AnalyzeOpenings(const Design& design, Openings& out);

}

#endif
