#ifndef OLDSHIP_MODEL_H
#define OLDSHIP_MODEL_H

#include "oldship.h"

namespace OldBuilding
{
class Builder;
}

namespace OldShip
{

enum ShipTex
{
	ST_Berth, ST_BerthBottom,
	ST_LargeDoorClosed, ST_LargeDoorHalfOpen, ST_LargeDoorOpen,
	ST_LargeCrewDoorClosed, ST_LargeCrewDoorHalfOpen, ST_LargeCrewDoorOpen,
	ST_LargeHullDoorClosed, ST_LargeHullDoorHalfOpen, ST_LargeHullDoorOpen,
	ST_CrewDoorClosed, ST_CrewDoorHalfOpen, ST_CrewDoorOpen,
	ST_CrewHatchClosed, ST_CrewHatchHalfOpen, ST_CrewHatchOpen,
	ST_GravityDrive, ST_HullBottom,
	ST_HullDoorClosed, ST_HullDoorHalfOpen, ST_HullDoorOpen,
	ST_HullHatchClosed, ST_HullHatchHalfOpen, ST_HullHatchOpen,
	ST_HullTop, ST_HullTopBottomWindow, ST_HullWall, ST_HullWallWindow,
	ST_IntCeiling, ST_IntDoorClosed, ST_IntDoorHalfOpen, ST_IntDoorOpen, ST_IntFloor,
	ST_IntHatchClosed, ST_IntHatchHalfOpen, ST_IntHatchOpen,
	ST_IntWallEndCap, ST_IntWallFinished, ST_IntWallFill, ST_IntWallCargo, ST_IntWallFillCargo,
	ST_IntWallEquipment, ST_IntWallFillEquipment,
	ST_Ladder, ST_LadderPole, ST_LifeSupport, ST_PowerPlant, ST_Railing, ST_RailingPost, ST_Seat,
	ST_Sensor, ST_SensorTop, ST_Shield, ST_Surgery, ST_TransporterPad, ST_Turret, ST_WeaponBay, ST_FTLDrive,
	ST_StairCasing, ST_StairRiser, ST_StairTread,
	ST_Count
};

enum WallType
{
	WT_None = 0,
	WT_LargeDoor = 1,
	WT_Door = 2,
	WT_Turret = 3,
	WT_Plate = 4,
	WT_Window = 5,
};

void ShipTexture(ShipTex tx, float& s, float& t, float& u, float& v);
typedef bool (*TextureSource)(ShipTex tx, float& s, float& t, float& u, float& v);
TextureSource SetTextureSource(TextureSource source);
TextureSource CurrentTextureSource();
void EffectTexture(int tx, float& s, float& t, float& u, float& v);

void WallProfile(int roomType, int style, std::vector<Vec3>& points, std::vector<int>& walls);

#include "tables_decl.inc"

Vec3 RotateZ(const Vec3& p, double radians);
bool FuzzyEquals(const Vec3& a, const Vec3& b);
bool IsOnSegment(const Vec3& p, const Vec3& a, const Vec3& b);
double Distance(const Vec3& a, const Vec3& b);
double Distance2D(const Vec3& a, const Vec3& b);
Vec3 Unit(const Vec3& v);
Vec3 Cross(const Vec3& a, const Vec3& b);
inline Vec3 DeckCoord(int x, int y, double z) { return Vec3(FloorBuffer::Feet(x), FloorBuffer::Feet(y), z); }
int ClosestDeck(double z);

void ModelAdd(TriList& out, const Vec3& normal, const Vec3& origin, float rot,
		const Vec3& p0, float u0, float v0, const Vec3& p1, float u1, float v1, const Vec3& p2, float u2, float v2);
void ModelAdd(TriList& out, const Vec3& normal, const Vec3& origin, float rot,
		const Vec3& p0, float u0, float v0, const Vec3& p1, float u1, float v1, const Vec3& p2, float u2, float v2,
		const Vec3& p3, float u3, float v3);
void ModelAdd(TriList& out, const Vec3& normal, const int* ints, float shine, bool flip,
		float u0, float v0, float eu0, float ev0, const Vec3& p0,
		float u1, float v1, float eu1, float ev1, const Vec3& p1,
		float u2, float v2, float eu2, float ev2, const Vec3& p2);
void ModelAdd(TriList& out, const Vec3& normal, const int* ints, float shine, bool flip,
		float u0, float v0, float eu0, float ev0, const Vec3& p0,
		float u1, float v1, float eu1, float ev1, const Vec3& p1,
		float u2, float v2, float eu2, float ev2, const Vec3& p2,
		float u3, float v3, float eu3, float ev3, const Vec3& p3);
void ModelAdd(TriList& out, const Vec3& normal, const int* ints, float shine, bool flip, const Vec3& origin, float rot,
		float u0, float v0, float eu0, float ev0, const Vec3& p0,
		float u1, float v1, float eu1, float ev1, const Vec3& p1,
		float u2, float v2, float eu2, float ev2, const Vec3& p2);
void ModelAdd(TriList& out, const Vec3& normal, const int* ints, float shine, bool flip, const Vec3& origin, float rot,
		float u0, float v0, float eu0, float ev0, const Vec3& p0,
		float u1, float v1, float eu1, float ev1, const Vec3& p1,
		float u2, float v2, float eu2, float ev2, const Vec3& p2,
		float u3, float v3, float eu3, float ev3, const Vec3& p3);

struct Outline
{
	std::vector<Vec3>		points;
	std::vector<uint16_t>	segs;
	std::vector<uint32_t>	ids;
};

class Builder
{
public:
	explicit Builder(const Design& d) : m(d), animatedDoors(0) {}

	void CreateModels(TriList& exterior, TriList& interior) const;

	void RenderExterior(TriList& out, bool inside) const;
	void RenderInterior(TriList& out) const;
	void RenderInteriorDecks(TriList& out) const;
	bool RenderFitting(TriList& out, const Detail& d) const;

	void RenderExteriorTops(TriList& out, ShipTex tex, WallType wall, double z, int& minX, int& maxX, int& minY, int& maxY,
			FloorBuffer& fb, bool inside) const;
	void RenderExteriorBottoms(TriList& out, ShipTex tex, WallType wall, double z, int& minX, int& maxX, int& minY, int& maxY,
			FloorBuffer& fb, bool inside) const;
	void RenderExteriorWalls(TriList& out, ShipTex tex, WallType wall, const std::vector<Outline>& outlines, bool inside) const;

	const Design& Ship() const { return m; }

	struct Opening
	{
		int		id;
		int		type;
		int		deck;
		Vec3	loc;
	};
	void SetOpenings(const std::vector<Opening>& list, int animated = 0) { openings = list; animatedDoors = animated; }

	virtual ~Builder() {}
	void SetWeaponBays(const std::vector<int>& bays) { weaponBays = bays; }

protected:
	virtual int TransporterSystemIndex(uint16_t systemId) const { (void)systemId; return 0; }

private:
	friend class OldBuilding::Builder;
	int WeaponBayIndex(int bay) const;
	std::vector<int> weaponBays;
	void RenderHatch(TriList& out, const Vec3& at, bool diagonal, int side, bool inside, int detailType) const;
	void RenderTurret(TriList& out, const Vec3& at, double tilt, bool inside) const;
	bool IsHatchAt(const Vec3& at, int& detailType) const;
	bool IsTopOrBottomOpeningAt(const Vec3& at) const;
	int HatchAt(const Vec3& at) const;
	void RenderWallEndCap(TriList& out, const std::vector<Vec3>& profile, const Vec3& mitre, const Vec3& corner,
			bool start, bool inside) const;
	int AnimatedDoors() const { return animatedDoors; }
	int DoorIndexDesignLocked(int openingId) const;

	void RenderInteriorCeilings(TriList& out, double z, int& minX, int& maxX, int& minY, int& maxY,
			const FloorBuffer& fb) const;
	void RenderInteriorFloors(TriList& out, ShipTex tex, double z, bool hatches, int& minX, int& maxX, int& minY, int& maxY,
			const FloorBuffer& fb) const;
	bool IsExteriorWall(const Vec3& at, int heading, const FloorBuffer& fb) const;
	void AdjacentRoomsData(const Vec3& at, int heading, const FloorBuffer& fb, uint16_t& left, uint16_t& right) const;
	void RenderInteriorWalls(TriList& out, const Room& room, const FloorBuffer& fb) const;

	void RenderBerthDouble(TriList& out, const Vec3& at, double rot) const;
	void RenderBerthSingle(TriList& out, const Vec3& at, double rot) const;
	void RenderLadder(TriList& out, const Vec3& at, float rot) const;
	void RenderDriveUnit(TriList& out, ShipTex tex, const Vec3& at) const;
	void RenderSurgeryUnit(TriList& out, const Vec3& at) const;
	void RenderSensor(TriList& out, const Vec3& at) const;
	void RenderShield(TriList& out, const Vec3& at) const;
	void RenderLifeSupport(TriList& out, const Vec3& at) const;
	void RenderTransporterPad(TriList& out, const Vec3& at, uint16_t roomId, const std::vector<Room>& rooms) const;
	void RenderWeapon(TriList& out, const Vec3& at, uint16_t roomId, const std::vector<Room>& rooms) const;
	void RenderStairsStraight(TriList& out, const Vec3& at, double rot, bool diagonal) const;
	void RenderStairsTurnLeft(TriList& out, const Vec3& at, double rot, bool diagonal) const;
	void RenderStairsTurnRight(TriList& out, const Vec3& at, double rot, bool diagonal) const;
	void RenderStairsUTurnLeft(TriList& out, const Vec3& at, double rot, bool diagonal) const;
	void RenderStairsUTurnRight(TriList& out, const Vec3& at, double rot, bool diagonal) const;

	const Design& m;
	std::vector<Opening> openings;
	int animatedDoors;
};

}

#endif
