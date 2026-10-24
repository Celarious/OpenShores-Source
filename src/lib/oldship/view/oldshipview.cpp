#include "oldshipview.h"
#include "oldbuildingspots.h"

#include "model.h"
#include "oldbuildingedit.h"
#include "oldshipedit.h"
#include "oldshiptypes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>

namespace OldShip
{

namespace
{

const Rgb RoomColors[25] =
{
	{ 1.000f, 0.000f, 0.000f },
	{ 0.537f, 0.639f, 0.286f },
	{ 1.000f, 0.867f, 0.000f },
	{ 0.439f, 0.439f, 0.439f },
	{ 0.176f, 0.055f, 0.576f },
	{ 0.867f, 0.765f, 0.345f },
	{ 0.639f, 0.886f, 0.576f },
	{ 0.718f, 0.675f, 0.502f },
	{ 0.718f, 0.718f, 0.718f },
	{ 0.929f, 0.980f, 1.000f },
	{ 0.929f, 0.925f, 0.659f },
	{ 0.788f, 1.000f, 0.737f },
	{ 1.000f, 0.988f, 0.357f },
	{ 1.000f, 0.604f, 0.239f },
	{ 0.898f, 0.000f, 0.000f },
	{ 0.502f, 0.439f, 0.502f },
	{ 1.000f, 0.000f, 0.529f },
	{ 0.365f, 0.890f, 0.957f },
	{ 0.412f, 0.957f, 0.792f },
	{ 0.647f, 0.000f, 0.314f },
	{ 0.729f, 0.298f, 0.827f },
	{ 0.196f, 0.345f, 0.788f },
	{ 0.906f, 0.259f, 0.133f },
	{ 0.906f, 0.259f, 0.133f },
	{ 0.906f, 0.259f, 0.133f }
};

const signed char IconSlot[143] = { -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
	1, 1, 1, 1, 1, 1, 1, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
	40, 40, 40, 40, 40, 40, 40, 40, 24, 24, 24, 24, 24, 24, 24, 24, 25, 27, 27, 26, 7, 7, 7, 7, 7, 7, 7, 50, 50, 50, 50, 50,
	50, 50, 51, 51, 51, 51, 51, 51, 51, 28, 29, 30, 56, 56, 56, 56, 56, 56, 56, 56, 57, 57, 57, 57, 57, 57, 57, 57, 58, 58,
	58, 58, 58, 58, 58, 58, 59, 59, 59, 59, 59, 59, 59, 59, 60, 60, 60, 60, 60, 60, 60, 60, 61, 62 };
const float IconSize[143] = { 10, 4, 3.5f, 2, 4, 2, 4, 4, 4, 2, 2.8284271f, 3.5f, 4, 4, 2.8284271f, 4, 2.8284271f, 4,
	2.8284271f, 4, 2.8284271f, 2, 4, 2.8284271f, 2.8284271f, 3.5f, 3.5f, 3.5f, 3.5f, 3.5f, 3.5f, 3.5f, 2.8284271f, 4, 2, 2, 2,
	2, 2, 2, 2, 10, 10, 10, 10, 10, 10, 10, 10, 2, 2, 4, 4, 2, 2, 2, 2, 35, 35, 35, 35, 35, 35, 35, 35, 3.5f, 3.5f, 3.5f,
	3.5f, 3.5f, 3.5f, 3.5f, 3.5f, 4, 10, 10, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 6, 5, 3, 35, 35,
	35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
	35, 35, 35, 35, 35, 35, 35, 35, 4, 25 };

const double BerthHead = 1.41421356225, BerthFoot = 4.24264068675;
const double VehicleSpot = 10.0;

bool IsBerth(int t)
{
	return t == 2 || (t >= 25 && t <= 31) || (t >= 65 && t <= 72);
}

bool IsRoof(int roomType)
{
	return roomType >= 22 && roomType <= 24;
}

Batch& BatchOf(std::vector<Batch>& list, float r, float g, float b, float a)
{
	for(size_t i = 0; i < list.size(); i++)
		if(list[i].r == r && list[i].g == g && list[i].b == b && list[i].a == a)
			return list[i];
	Batch nu;
	nu.r = r;
	nu.g = g;
	nu.b = b;
	nu.a = a;
	list.push_back(nu);
	return list.back();
}

void AddLine(std::vector<Vec3>& lines, const Vec3& a, const Vec3& b)
{
	lines.push_back(a);
	lines.push_back(b);
}

void AddLoop(std::vector<Vec3>& lines, const std::vector<Vec3>& loop)
{
	for(size_t i = 0; i < loop.size(); i++)
		AddLine(lines, loop[i], loop[(i + 1) % loop.size()]);
}

double Area2(const std::vector<Vec3>& p)
{
	double a = 0;
	for(size_t i = 0; i < p.size(); i++)
	{
		const Vec3& u = p[i];
		const Vec3& v = p[(i + 1) % p.size()];
		a += u.x * v.y - v.x * u.y;
	}
	return a;
}

double Turn(const Vec3& a, const Vec3& b, const Vec3& c)
{
	return (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
}

void Triangulate(const std::vector<Vec3>& outline, std::vector<Vec3>& out)
{
	std::vector<Vec3> p(outline);
	if(p.size() < 3)
		return;
	if(Area2(p) < 0)
	{
		std::vector<Vec3> turned(p.rbegin(), p.rend());
		p.swap(turned);
	}
	int guard = (int)(p.size() * p.size()) + 8;
	while(p.size() > 3 && guard-- > 0)
	{
		bool cut = false;
		for(size_t i = 0; i < p.size() && !cut; i++)
		{
			const Vec3& a = p[(i + p.size() - 1) % p.size()];
			const Vec3& b = p[i];
			const Vec3& c = p[(i + 1) % p.size()];
			const double turn = Turn(a, b, c);
			if(turn < -1e-9)
				continue;
			bool empty = true;
			if(turn > 1e-9)
				for(size_t k = 0; k < p.size() && empty; k++)
				{
					const Vec3& q = p[k];
					if(&q == &a || &q == &b || &q == &c)
						continue;
					if(Turn(a, b, q) >= -1e-9 && Turn(b, c, q) >= -1e-9 && Turn(c, a, q) >= -1e-9
							&& !(q.x == a.x && q.y == a.y) && !(q.x == b.x && q.y == b.y) && !(q.x == c.x && q.y == c.y))
						empty = false;
				}
			if(!empty)
				continue;
			if(turn > 1e-9)
			{
				out.push_back(a);
				out.push_back(b);
				out.push_back(c);
			}
			p.erase(p.begin() + i);
			cut = true;
		}
		if(!cut)
			break;
	}
	if(p.size() == 3 && Turn(p[0], p[1], p[2]) > 1e-9)
	{
		out.push_back(p[0]);
		out.push_back(p[1]);
		out.push_back(p[2]);
	}
}

std::vector<Vec3> OutlineOf(const Room& r, double z)
{
	std::vector<Vec3> o;
	for(size_t i = 0; i < r.points.size(); i++)
		o.push_back(Vec3(r.points[i].x * GridUnit, r.points[i].y * GridUnit, z));
	return o;
}

void TurnedRect(double x0, double y0, double x1, double y1, double radians, const Vec3& at, Vec3 corner[4])
{
	const double cx[4] = { x0, x1, x1, x0 }, cy[4] = { y0, y0, y1, y1 };
	const double c = cos(radians), s = sin(radians);
	for(int i = 0; i < 4; i++)
		corner[i] = Vec3(at.x + cx[i] * c - cy[i] * s, at.y + cx[i] * s + cy[i] * c, at.z);
}

void DetailRect(int t, int commodity, double& x0, double& y0, double& x1, double& y1)
{
	double halfLength, halfWidth;
	if((DetailType::IsVehicleLoc(t) && VehicleHalfSize(commodity, halfLength, halfWidth))
			|| (t >= 41 && t <= 48 && VehicleHalfSize(VEHICLE_SpaceFighter, halfLength, halfWidth)))
	{
		x0 = -halfLength;
		x1 = halfLength;
		y0 = -halfWidth;
		y1 = halfWidth;
		return;
	}
	if(IsBerth(t))
	{
		x0 = -BerthHead;
		x1 = BerthFoot;
		y0 = -BerthHead;
		y1 = BerthHead;
		return;
	}
	double half = (t >= 0 && t < 143 ? IconSize[t] : 4.0f) / 2;
	if(DetailType::IsGameTable(t))
		half = 5.0;
	else if(DetailType::IsVehicleLoc(t))
		half = VehicleSpot / 2;
	x0 = y0 = -half;
	x1 = y1 = half;
}

}

Rgb RoomColor(int roomType)
{
	if(roomType < 0 || roomType > 24)
		roomType = 0;
	return RoomColors[roomType];
}

bool DetailIcon(int t, float& u0, float& u1, double& size)
{
	if(t < 0 || t >= 143 || IconSlot[t] < 0)
		return false;
	u0 = IconSlot[t] / (float)IconSlots;
	u1 = u0 + 15.0f / 1024.0f;
	size = IconSize[t];
	return true;
}

bool DetailIconQuad(int t, const Vec3& at, IconQuad& quad, int commodity)
{
	double size;
	if(DetailType::IsStairs(t) || !DetailIcon(t, quad.u0, quad.u1, size))
		return false;
	double x0, y0, x1, y1;
	DetailRect(t, commodity, x0, y0, x1, y1);
	TurnedRect(x0, y0, x1, y1, DetailType::Rotation(t), at, quad.corner);
	return true;
}

void ModelLines(const Design& design, const Detail& detail, std::vector<Vec3>& lines)
{
	Builder builder(design);
	TriList tris;
	Detail shown(detail);
	shown.flag = false;
	if(!builder.RenderFitting(tris, shown))
		return;
	std::set<std::pair<std::pair<long, std::pair<long, long> >, std::pair<long, std::pair<long, long> > > > seen;
	for(size_t i = 0; i < tris.size(); i++)
		for(int k = 0; k < 3; k++)
		{
			const Vec3& a = tris[i].v[k].pos;
			const Vec3& b = tris[i].v[(k + 1) % 3].pos;
			std::pair<long, std::pair<long, long> > ka(lround(a.x * 100), std::make_pair(lround(a.y * 100), lround(a.z * 100)));
			std::pair<long, std::pair<long, long> > kb(lround(b.x * 100), std::make_pair(lround(b.y * 100), lround(b.z * 100)));
			if(ka == kb)
				continue;
			if(kb < ka)
				std::swap(ka, kb);
			if(seen.insert(std::make_pair(ka, kb)).second)
				AddLine(lines, a, b);
		}
}

void StairFigure(int detailType, std::vector<Vec3>& lines)
{
	if(!DetailType::IsStairs(detailType))
		return;
	Design none;
	Detail stairs = Detail();
	stairs.type = detailType;
	ModelLines(none, stairs, lines);
}

void RoofLines(const Room& roof, std::vector<Vec3>& lines)
{
	if(!IsRoof(roof.type))
		return;
	OldBuilding::Design none;
	OldBuilding::Builder builder(none, 76, 0, 0, 0);
	OldBuilding::Room room;
	room.design = 0;
	room.id = 0;
	room.deck = roof.deck;
	room.type = roof.type;
	room.number = 0;
	room.points = roof.points;
	room.UpdateOutline();
	OldBuilding::BTriList tris;
	builder.RenderRoof(tris, room);
	std::set<std::pair<std::pair<long, std::pair<long, long> >, std::pair<long, std::pair<long, long> > > > seen;
	for(size_t i = 0; i < tris.size(); i++)
		for(int k = 0; k < 3; k++)
		{
			const Vec3& a = tris[i].v[k].pos;
			const Vec3& b = tris[i].v[(k + 1) % 3].pos;
			std::pair<long, std::pair<long, long> > ka(lround(a.x * 100), std::make_pair(lround(a.y * 100), lround(a.z * 100)));
			std::pair<long, std::pair<long, long> > kb(lround(b.x * 100), std::make_pair(lround(b.y * 100), lround(b.z * 100)));
			if(ka == kb)
				continue;
			if(kb < ka)
				std::swap(ka, kb);
			if(seen.insert(std::make_pair(ka, kb)).second)
				AddLine(lines, a, b);
		}
}

void RoofGhost(int roofType, int deck, int clicks, const GridPoint& p0, const GridPoint& p1, double gx, double gy,
		std::vector<Vec3>& lines)
{
	const double z = deck * DeckHeight + IconLift;
	GridPoint next;
	next.x = (int)floor(gx + 0.5);
	next.y = (int)floor(gy + 0.5);
	bool has = true;
	if(clicks == 1)
		has = RoofSecondPoint(p0, gx, gy, next);
	else if(clicks >= 2)
		has = RoofThirdPoint(p0, p1, gx, gy, next);
	const Vec3 a(p0.x * GridUnit, p0.y * GridUnit, z), b(p1.x * GridUnit, p1.y * GridUnit, z);
	if(clicks >= 1 && !has)
	{
		if(clicks >= 2)
			AddLine(lines, a, b);
		return;
	}
	const Vec3 at(next.x * GridUnit, next.y * GridUnit, z);
	const double arm = GridUnit / 2;
	AddLine(lines, Vec3(at.x - arm, at.y, z), Vec3(at.x + arm, at.y, z));
	AddLine(lines, Vec3(at.x, at.y - arm, z), Vec3(at.x, at.y + arm, z));
	if(clicks == 1)
		AddLine(lines, a, at);
	else if(clicks >= 2)
	{
		Room roof;
		roof.design = 0;
		roof.id = 0;
		roof.deck = deck;
		roof.type = roofType;
		roof.style = 0;
		if(RoofPoints(roofType, p0, p1, next, roof.points))
		{
			std::vector<Vec3> figure;
			RoofLines(roof, figure);
			for(size_t i = 0; i < figure.size(); i++)
				lines.push_back(Vec3(figure[i].x, figure[i].y, figure[i].z + IconLift));
		}
		else
			AddLine(lines, a, b);
	}
}

void BuildDrawing(const Design& design, int deck, Drawing& out, bool building)
{
	out = Drawing();
	for(size_t i = 0; i < design.rooms.size(); i++)
	{
		const Room& r = design.rooms[i];
		if(r.points.size() < 3)
			continue;
		const Rgb c = RoomColor(r.type);
		const double z = r.deck * DeckHeight;
		std::vector<Vec3> floor = OutlineOf(r, z), ceiling = OutlineOf(r, z + DeckHeight);
		const bool roof = building && IsRoof(r.type);
		if(roof)
		{
			const std::vector<FootPoint> rim = RoomOutline(r);
			floor.clear();
			for(size_t k = 0; k < rim.size(); k++)
				floor.push_back(Vec3(rim[k].x * GridUnit, rim[k].y * GridUnit, z));
		}

		const float dim = r.deck == deck ? 1.0f : 1.0f / 3.0f;
		Batch& wire = BatchOf(out.lines, c.r * dim, c.g * dim, c.b * dim, 1.0f);
		AddLoop(wire.points, floor);
		if(roof)
			RoofLines(r, wire.points);
		else if(!IsRoof(r.type))
		{
			AddLoop(wire.points, ceiling);
			for(size_t k = 0; k < floor.size(); k++)
				AddLine(wire.points, floor[k], ceiling[k]);
		}

		if(!roof && r.deck < deck)
			Triangulate(floor, BatchOf(out.floorsBelow, c.r * 0.25f, c.g * 0.25f, c.b * 0.25f, 1.0f).points);
		else if(!roof && r.deck == deck)
			Triangulate(floor, BatchOf(out.floors, c.r, c.g, c.b, 0.8f).points);
		else if(!roof && r.deck == deck + 1)
			Triangulate(floor, BatchOf(out.floors, c.r * 0.25f, c.g * 0.25f, c.b * 0.25f, 0.2f).points);

		if(r.deck == deck)
		{
			const RoomTypeInfo* info = RoomTypeInfoOf(r.type);
			const char* inBuilding = building ? RoomTypeBuildingName(r.type) : 0;
			char number[16];
			snprintf(number, sizeof(number), " %d", r.style + 1);
			Label label;
			label.at = floor[0];
			label.text = std::string(inBuilding ? inBuilding : info ? info->name : "Room") + number;
			label.color = c;
			out.labels.push_back(label);
		}
	}

	std::map<int, int> pads;
	for(size_t i = 0; i < design.details.size(); i++)
	{
		const Detail& d = design.details[i];
		if(d.deck != deck)
			continue;
		if(DetailType::IsStairs(d.type))
		{
			ModelLines(design, d, out.stairs);
			continue;
		}
		const Vec3 loc = d.Loc();
		if(IsSpot(d.type))
		{
			const std::vector<FootPoint> spot = SpotFootprint(d.type);
			for(size_t k = 0; k < spot.size(); k++)
			{
				const FootPoint& a = spot[k];
				const FootPoint& c = spot[(k + 1) % spot.size()];
				AddLine(out.stairs, Vec3(loc.x + a.x * GridUnit, loc.y + a.y * GridUnit, loc.z + IconLift),
						Vec3(loc.x + c.x * GridUnit, loc.y + c.y * GridUnit, loc.z + IconLift));
			}
			Label label;
			label.at = Vec3(loc.x, loc.y, loc.z + IconLift);
			label.text = SpotName(d.type);
			label.color = RoomColor(13);
			out.labels.push_back(label);
			continue;
		}
		IconQuad quad;
		const bool hasIcon = DetailIconQuad(d.type, Vec3(loc.x, loc.y, loc.z + IconLift), quad, d.extra);
		if(hasIcon)
			out.icons.push_back(quad);
		if(hasIcon && DetailType::IsVehicleLoc(d.type))
		{
			for(int k = 0; k < 4; k++)
				AddLine(out.stairs, quad.corner[k], quad.corner[(k + 1) % 4]);
		}
		if(d.type == 20)
		{
			char number[16];
			snprintf(number, sizeof(number), "%d", ++pads[d.room]);
			Label label;
			label.at = loc;
			label.text = number;
			label.color = RoomColor(20);
			out.labels.push_back(label);
		}
	}
}

void GridLines(int gridLines, std::vector<Vec3>& minor, std::vector<Vec3>& major)
{
	const double S = CellSize, R = GridRadius;
	for(int i = -GridCells; i <= GridCells; i++)
	{
		std::vector<Vec3>& lines = i % 10 ? minor : major;
		if(gridLines & GRID_Longitudinal)
			AddLine(lines, Vec3(i * S, R, 0), Vec3(i * S, -R, 0));
		if(gridLines & GRID_Lateral)
			AddLine(lines, Vec3(R, i * S, 0), Vec3(-R, i * S, 0));
		if(gridLines & GRID_Diagonal45)
		{
			if(i != GridCells)
				AddLine(lines, Vec3(i * S, -R, 0), Vec3(R, -i * S, 0));
			if(i != -GridCells && i != GridCells)
				AddLine(lines, Vec3(-R, i * S, 0), Vec3(-i * S, R, 0));
		}
		if(gridLines & GRID_Diagonal135)
		{
			if(i != GridCells)
				AddLine(lines, Vec3(i * S, R, 0), Vec3(R, i * S, 0));
			if(i != -GridCells && i != GridCells)
				AddLine(lines, Vec3(-R, -i * S, 0), Vec3(-i * S, -R, 0));
		}
	}
}

void CageLines(double planeZ, std::vector<Vec3>& lines)
{
	const double R = GridRadius;
	const double level[3] = { CageHeight - planeZ, -planeZ, -CageHeight - planeZ };
	const double cx[4] = { R, R, -R, -R }, cy[4] = { R, -R, -R, R };
	for(int l = 0; l < 3; l++)
		for(int k = 0; k < 4; k++)
			AddLine(lines, Vec3(cx[k], cy[k], level[l]), Vec3(cx[(k + 1) % 4], cy[(k + 1) % 4], level[l]));
	for(int k = 0; k < 4; k++)
		AddLine(lines, Vec3(cx[k], cy[k], level[0]), Vec3(cx[k], cy[k], level[2]));
}

const CompassLabel* CompassLabels(int& count)
{
	static const CompassLabel labels[8] =
	{
		{ "Fore", 0, GridRadius }, { "Aft", 0, -GridRadius }, { "Port", -GridRadius, 0 }, { "Starboard", GridRadius, 0 },
		{ "Port Quarter", -GridRadius, -GridRadius }, { "Starboard Quarter", GridRadius, -GridRadius },
		{ "Port Fore", -GridRadius, GridRadius }, { "Starboard Fore", GridRadius, GridRadius }
	};
	count = 8;
	return labels;
}

void BuildingGridLines(int gridLines, double radius, bool landSide, std::vector<Vec3>& minor, std::vector<Vec3>& major)
{
	const double S = CellSize, R = radius;
	if(R <= 0.0)
		return;
	const double top = landSide ? 0.0 : R;
	const int cells = (int)floor(R / S + 1e-9);
	for(int i = -cells; i <= cells; i++)
	{
		std::vector<Vec3>& lines = i % 10 ? minor : major;
		if(gridLines & GRID_Longitudinal)
			AddLine(lines, Vec3(i * S, top, 0), Vec3(i * S, -R, 0));
		if((gridLines & GRID_Lateral) && i * S <= top)
			AddLine(lines, Vec3(R, i * S, 0), Vec3(-R, i * S, 0));
	}
	const int diagonals = (int)floor((R + R) / S + 1e-9);
	for(int i = -diagonals; i <= diagonals; i++)
	{
		std::vector<Vec3>& lines = i % 10 ? minor : major;
		const double c = i * S;
		if(gridLines & GRID_Diagonal45)
		{
			const double x0 = std::max(-R, -R - c), x1 = std::min(R, top - c);
			if(x1 > x0)
				AddLine(lines, Vec3(x0, x0 + c, 0), Vec3(x1, x1 + c, 0));
		}
		if(gridLines & GRID_Diagonal135)
		{
			const double x0 = std::max(-R, c - top), x1 = std::min(R, c + R);
			if(x1 > x0)
				AddLine(lines, Vec3(x0, c - x0, 0), Vec3(x1, c - x1, 0));
		}
	}
}

void BuildingSiteLines(double radius, bool landSide, double planeZ, int lowestDeck, int highestDeck, std::vector<Vec3>& lines)
{
	const double R = radius;
	if(R <= 0.0)
		return;
	const double top = landSide ? 0.0 : R;
	const double cx[4] = { R, R, -R, -R }, cy[4] = { top, -R, -R, top };
	double level[4] = { 0.0, -planeZ, (highestDeck + 1) * DeckHeight - planeZ, lowestDeck * DeckHeight - planeZ };
	for(int l = 0; l < 4; l++)
	{
		bool again = false;
		for(int k = 0; k < l; k++)
			again = again || fabs(level[k] - level[l]) < 1e-6;
		if(again)
			continue;
		for(int k = 0; k < 4; k++)
			AddLine(lines, Vec3(cx[k], cy[k], level[l]), Vec3(cx[(k + 1) % 4], cy[(k + 1) % 4], level[l]));
	}
	for(int k = 0; k < 4; k++)
		AddLine(lines, Vec3(cx[k], cy[k], level[2]), Vec3(cx[k], cy[k], level[3]));
}

int BuildingCompassLabels(double radius, bool landSide, CompassLabel labels[8])
{
	const double R = radius + CellSize / 2;
	const double top = landSide ? CellSize / 2 : R;
	const CompassLabel all[4] =
	{
		{ landSide ? "Shore (North)" : "North", 0, top }, { "South", 0, -R }, { "West", -R, landSide ? -R / 2 : 0 },
		{ "East", R, landSide ? -R / 2 : 0 }
	};
	for(int i = 0; i < 4; i++)
		labels[i] = all[i];
	return 4;
}

bool UsesBrush(int t)
{
	return t == 10 || t == 14 || t == 16 || t == 18 || t == 23 || t == 24 || t == 32;
}

GHOST ToolGhost(int t, bool area, int brush, std::vector<Vec3>& lines, int commodity)
{
	const Vec3 here(0, 0, 0);
	Vec3 corner[4];
	if(IsSpot(t))
	{
		const std::vector<FootPoint> spot = SpotFootprint(t);
		for(size_t i = 0; i < spot.size(); i++)
		{
			const FootPoint& a = spot[i];
			const FootPoint& b = spot[(i + 1) % spot.size()];
			AddLine(lines, Vec3(a.x * GridUnit, a.y * GridUnit, 0), Vec3(b.x * GridUnit, b.y * GridUnit, 0));
		}
		double halfLength, halfWidth;
		SpotHalfSize(SpotKindOf(t), halfLength, halfWidth);
		const double a = SpotTurn(t);
		AddLine(lines, here, Vec3(-sin(a) * halfLength, cos(a) * halfLength, 0));
		return GHOST_Footprint;
	}
	if(t >= 0 && DetailType::IsStairs(t))
	{
		const std::vector<FootPoint> floor = StairFootprint(t);
		for(size_t i = 0; i < floor.size(); i++)
		{
			const FootPoint& a = floor[i];
			const FootPoint& b = floor[(i + 1) % floor.size()];
			AddLine(lines, Vec3(a.x * GridUnit, a.y * GridUnit, 0), Vec3(b.x * GridUnit, b.y * GridUnit, 0));
		}
		return GHOST_Footprint;
	}
	if(t >= 0 && (IsBerth(t) || DetailType::IsAnyHatch(t) || DetailType::IsGameTable(t) || DetailType::IsVehicleLoc(t)))
	{
		double x0, y0, x1, y1;
		DetailRect(t, commodity, x0, y0, x1, y1);
		const double turn = DetailType::Rotation(t);
		TurnedRect(x0, y0, x1, y1, turn, here, corner);
		for(int k = 0; k < 4; k++)
			AddLine(lines, corner[k], corner[(k + 1) % 4]);
		if(IsBerth(t))
		{
			Vec3 cross[4];
			TurnedRect(0, y0, 0, y1, turn, here, cross);
			AddLine(lines, cross[0], cross[3]);
		}
		return GHOST_Footprint;
	}
	const double half = BerthHead + (area ? brush * GridUnit : 0.0);
	TurnedRect(-half, -half, half, half, 0, here, corner);
	for(int k = 0; k < 4; k++)
		AddLine(lines, corner[k], corner[(k + 1) % 4]);
	return GHOST_Square;
}

bool RayToDeck(const Vec3& origin, const Vec3& direction, int deck, double& gx, double& gy)
{
	const double z = deck * DeckHeight;
	if(fabs(direction.z) < 1e-12)
		return false;
	const double t = (z - origin.z) / direction.z;
	if(t < 0)
		return false;
	gx = (origin.x + direction.x * t) / GridUnit;
	gy = (origin.y + direction.y * t) / GridUnit;
	return true;
}

}
