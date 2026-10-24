#ifndef OLDPATH_SHIP_H
#define OLDPATH_SHIP_H

#include "oldpath.h"

#include <vector>

namespace OldPath
{

struct StairDetail
{
	int8_t		deck;
	uint8_t		type;
	Point		loc;
	StairDetail() : deck(0), type(0) {}
};

struct RoomInfo
{
	uint16_t	id;
	int8_t		deck;
	uint8_t		type;
	uint8_t		style;
	RoomInfo() : id(0), deck(0), type(0), style(0) {}
};

struct ShipFrame
{
	Point		min, max;
	Point		cg;
	int			lowDeck;
	int			highDeck;
	std::vector<StairDetail>	stairs;
	std::vector<RoomInfo>		rooms;
	ShipFrame() : lowDeck(0), highDeck(0) {}

	DesignBounds Bounds() const;
	const RoomInfo* FindRoom(uint16_t id) const;
};

void AnalyzeFrame(const Design& design, ShipFrame& out);

struct Tri
{
	Point		v[3];
};

void StairSurfaces(uint8_t type, const Point& loc, std::vector<Tri>& out);
bool TriIntersectZ(const Tri& t, const Point& p, Point& hit, Point& normal);
bool TestStairs(uint8_t type, const Point& loc, const Point& p, double& height, Point& normal);

struct Walker
{
	enum Kind
	{
		Person,
		Animal,
		Vehicle,
		Vessel,
		Other
	};
	Kind		kind;
	bool		ghost;
	bool		crewOrTroop;
	bool		inChainOfCommand;
	bool		sameFrame;
	bool		onGround;
	bool		sameGrandparent;
	bool		renderSolid;
	std::vector<Point>	box;
	Walker() : kind(Person), ghost(false), crewOrTroop(false), inChainOfCommand(false), sameFrame(true),
		onGround(true), sameGrandparent(true), renderSolid(true) {}
};

typedef bool (*HullRay)(void* ctx, const Point& from, const Point& to, double& hitZ);

class ShipWalk
{
public:
	ShipWalk(const Path& path, const RoomArray& rooms, const Openings& openings, const ShipFrame& frame,
		const DoorStates& doors);

	bool		designEnv;
	bool		valid;
	bool		crewDoorsLocked;
	bool		hullDoorsLocked;
	HullRay		hullRay;
	void*		hullRayCtx;

	Point ToDesign(const Point& p) const;
	Point FromDesign(const Point& p) const;

	int IsInOrOnHull(const Point& p) const;

	bool IsPathBlocked(const Point& old, const Point& now, const Walker& who, Point& normal,
		std::vector<uint16_t>& openDoors, bool& askParent, PathTest& code) const;

	bool CheckPeerJoin(const Point& old, const Point& now, const Walker& who, std::vector<uint16_t>& openDoors) const;
	bool CheckChildExit(const Point& old, const Point& now, const Walker& who) const;

	double FloorHt(const Point& p, bool& gravity, bool& onStairs, Point& normal, bool plusDeck) const;

	int OnDesignDeck(const Point& p) const;
	bool InsideRoom(const Point& p, uint16_t& room) const;
	uint16_t InsideRoomType(const Point& p, int& type, uint16_t& style) const;
	bool InsideRoomType(const Point& p, uint16_t& room, int& type, uint16_t& style, int8_t& deck, double& height,
		Point& normal) const;

	int HatchAbove(const Point& p) const;
	int HatchBelow(const Point& p) const;
	int HatchDoorIndex(const Point& p) const;
	int HatchDoorIndex(const Point& p, bool& crew) const;
	int HatchAt(const Point& p) const;
	bool IsHatchAt(const Point& p, uint32_t& type) const;
	static bool IsTopOrBottomOpeningAt(const Point& p, const Design& design);

	int HullDoorIndex(const Point& p) const;
	int HullDoorIndex(const Point& p, uint16_t room) const;
	bool IsHullDoor(int i) const;
	bool IsDoorHullHatch(unsigned i) const;
	bool IsDoorLocked(unsigned i) const;
	int DoorIndex(int32_t detailId) const;
	Point HatchLadderRotation(unsigned i) const;

	Point FloorLocInsideDoor(const Point& p) const;
	bool TestTransportLocation(const Point& p) const;
	Point TargetLocation(const Point& helm) const;
	float GroundHeightAboveCG() const;

private:
	const Path&			path;
	const RoomArray&	rooms;
	const Openings&		openings;
	const ShipFrame&	frame;
	const DoorStates&	doors;
	DesignBounds		bounds;

	PathTest StepTest(const Point& a, const Point& b, const Walker& who, std::vector<uint16_t>& openDoors) const;
};

int ClosestDeck(double z);
bool IsValidGridCoord(const Point& p);
bool OpeningIsOnWall(uint32_t type);

}

#endif
