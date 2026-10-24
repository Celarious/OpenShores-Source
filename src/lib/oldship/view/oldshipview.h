#ifndef OLDSHIPVIEW_H
#define OLDSHIPVIEW_H

#include "oldship.h"

#include <string>
#include <vector>

namespace OldShip
{

const double CellSize = 2 * GridUnit;
const int GridCells = 40;
const double GridRadius = GridCells * CellSize;
const double CageHeight = 224.0;
const double IconLift = 0.1;
const int IconSlots = 64;

struct Rgb
{
	float r, g, b;
};

Rgb RoomColor(int roomType);

struct Batch
{
	float				r, g, b, a;
	std::vector<Vec3>	points;
};

struct IconQuad
{
	Vec3	corner[4];
	float	u0, u1;
};

struct Label
{
	Vec3		at;
	std::string	text;
	Rgb			color;
};

struct Drawing
{
	std::vector<Batch>		floorsBelow;
	std::vector<Batch>		lines;
	std::vector<IconQuad>	icons;
	std::vector<Vec3>		stairs;
	std::vector<Batch>		floors;
	std::vector<Label>		labels;
};

void BuildDrawing(const Design& design, int deck, Drawing& out, bool building = false);

bool DetailIcon(int detailType, float& u0, float& u1, double& size);

bool DetailIconQuad(int detailType, const Vec3& at, IconQuad& quad, int commodity = 0);

void ModelLines(const Design& design, const Detail& detail, std::vector<Vec3>& lines);

void GridLines(int gridLines, std::vector<Vec3>& minor, std::vector<Vec3>& major);

void CageLines(double planeZ, std::vector<Vec3>& lines);

struct CompassLabel
{
	const char*	text;
	double		x, y;
};
const CompassLabel* CompassLabels(int& count);

void BuildingGridLines(int gridLines, double radius, bool landSide, std::vector<Vec3>& minor, std::vector<Vec3>& major);
void BuildingSiteLines(double radius, bool landSide, double planeZ, int lowestDeck, int highestDeck, std::vector<Vec3>& lines);
int BuildingCompassLabels(double radius, bool landSide, CompassLabel labels[8]);

void RoofLines(const Room& roof, std::vector<Vec3>& lines);
void RoofGhost(int roofType, int deck, int clicks, const GridPoint& p0, const GridPoint& p1, double gx, double gy,
		std::vector<Vec3>& lines);

enum GHOST
{
	GHOST_None,
	GHOST_Square,
	GHOST_Footprint
};

GHOST ToolGhost(int detailType, bool area, int brush, std::vector<Vec3>& lines, int commodity = 0);

void StairFigure(int detailType, std::vector<Vec3>& lines);

bool UsesBrush(int detailType);

bool RayToDeck(const Vec3& origin, const Vec3& direction, int deck, double& gx, double& gy);

}

#endif
