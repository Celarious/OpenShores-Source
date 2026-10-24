#ifndef OLDSHIPEDIT_H
#define OLDSHIPEDIT_H

#include "oldship.h"

#include <map>
#include <string>
#include <vector>

namespace OldShip
{

const int ArmorMax = 10;
const int DeckLimit = 42;
const int DeckAddLimit = 28;
const int GridLimit = 120;
const int BrushMax = 25;

enum GRIDLINES
{
	GRID_Lateral = 1,
	GRID_Diagonal45 = 2,
	GRID_Longitudinal = 4,
	GRID_Diagonal135 = 8,
	GRID_All = 15
};

std::vector<GridPoint> FillShapeAt(double gx, double gy, int gridLines);

bool PointInOutline(const std::vector<GridPoint>& outline, double gx, double gy, bool orOnEdge);

struct FootPoint
{
	double x, y;
};

std::vector<FootPoint> StairFootprint(int detailType);
std::vector<FootPoint> StairTestPoints(int detailType);

bool StairOnTiles(int detailType, int x, int y);
void StairSnap(int detailType, double gx, double gy, int& x, int& y);

enum VEHICLE
{
	VEHICLE_SpaceFighter = 8,
	VEHICLE_SpaceTransport = 9,
	VEHICLE_Suv = 82,
	VEHICLE_Apc = 104,
	VEHICLE_MotorBike = 133,
	VEHICLE_SpaceFighter2 = 231
};

bool VehicleHalfSize(int commodity, double& halfLength, double& halfWidth);
std::vector<FootPoint> VehicleFootprint(int detailType, int commodity);

bool PointInFootprint(const std::vector<FootPoint>& outline, double gx, double gy, bool orOnEdge);

int WhereInRoom(const Room& room, double gx, double gy);

struct BuildingRules;

struct DesignCounts
{
	int					rooms, details, decks;
	std::map<int, int>	roomsByType;
	std::map<int, int>	detailsByType;
	DesignCounts() : rooms(0), details(0), decks(0) {}
};
DesignCounts Count(const Design& design);

class CellSet
{
public:
	bool FromOutline(const std::vector<GridPoint>& outline);
	bool MiddleInRoom(const Room& room) const;
	bool ToOutline(std::vector<GridPoint>& outline) const;

	bool Empty() const { return m.empty(); }
	size_t Size() const { return m.size(); }
	bool Overlaps(const CellSet& o) const;
	bool Contains(const CellSet& o) const;
	bool SharesEdgeWith(const CellSet& o) const;
	bool IsOnePiece() const;
	bool HasHoles() const;
	void Add(const CellSet& o);
	void Remove(const CellSet& o);
	void FillHoles();
	bool MiddleInside(const std::vector<GridPoint>& outline) const;

private:
	std::vector<int> m;
	static int Key(int i, int j, int q) { return (((i + 256) << 10) | (j + 256)) << 2 | q; }
	static void Neighbours(int key, int out[3]);
	bool Has(int key) const;
	void Holes(std::vector<int>& holes) const;
};

class Editor
{
public:
	explicit Editor(Design& design, const BuildingRules* building = 0) : m(design), b(building) {}
	bool IsBuilding() const { return b != 0; }

	bool ArmorIncrease(std::string& why);
	bool ArmorDecrease(std::string& why);
	void SetStreamline(bool on) { m.streamline = on ? 1 : 0; }
	void SetMinQuality(int q) { m.minQuality = q < 1 ? 1 : q > 255 ? 255 : q; }

	int RoomAt(int deck, double gx, double gy) const;
	int FindRoom(int deck, int roomType, int roomNumber) const;

	bool PaintCell(int deck, const std::vector<GridPoint>& cell, int roomType, int roomNumber, std::string& why);
	bool EraseCells(int deck, const std::vector<GridPoint>& cell, std::string& why);
	bool EraseRoom(int deck, const std::vector<GridPoint>& cell, std::string& why);
	bool ChangeRoomType(int deck, double gx, double gy, int roomType, std::string& why);
	bool ChangeRoomNumber(int deck, double gx, double gy, int roomNumber, std::string& why);
	bool EraseDeck(int deck, std::string& why);

	bool AddRoof(int deck, const GridPoint& p0, const GridPoint& p1, const GridPoint& p2, int roofType, std::string& why);
	int NextRoomNumber(int deck, int roomType) const;
	void BuildingFaults(std::vector<std::string>& faults) const;

	bool Move(bool wholeDesign, int deck, int dx, int dy, int dDeck, std::string& why);
	int Copy(int deck, int dx, int dy, int dDeck, int repeat, std::string& why);

	bool AddDetail(int deck, double gx, double gy, int detailType, int brush, std::string& why, int commodity = 0);
	bool EraseDetail(int deck, double gx, double gy, int brush, std::string& why);

	int VerifyDetails();

	bool IsDetailValid(const Detail& d, int self, int& type, int& room, std::string& why) const;

	bool IsPlacementValid(const Detail& d, int& type, int& room, std::string& why) const;

	static bool CanPlace(int detailType);
	bool CanPlaceHere(int detailType) const;
	bool CanPaint(int roomType) const;

	std::string RoomLabel(const Room& r) const;

private:
	struct RoomChange
	{
		int						room;
		std::vector<GridPoint>	outline;
	};

	Design& m;
	const BuildingRules* b;
	bool InGrid(double gx, double gy, int deck) const;
	std::string DetailName(int detailType) const;
	bool IsSpotValid(const Detail& d, int self, int& room, std::string& why) const;
	int NextRoomId() const;
	int NextDetailId() const;
	int PlaceDetail(Detail d, std::string& why);
	bool MoveFits(int deck, bool wholeDesign, int dx, int dy, int dDeck, std::string& why) const;
	bool PlanSubtract(int deck, const CellSet& cells, int keep, bool skipRoofs, std::vector<RoomChange>& changes,
			std::string& why) const;
	void ApplyChanges(const std::vector<RoomChange>& changes);
};

}

#endif
