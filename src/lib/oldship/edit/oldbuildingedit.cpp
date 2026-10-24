#include "oldbuildingedit.h"
#include "oldbuildingspots.h"

#include "oldshiptypes.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <set>

namespace OldShip
{

namespace
{

const double Pi = 3.14159265358979323846;

GridPoint Pt(int x, int y)
{
	GridPoint p;
	p.x = x;
	p.y = y;
	return p;
}

std::string Feet(double feet)
{
	char text[32];
	snprintf(text, sizeof(text), "%.0f", feet);
	return text;
}

std::string Number(int n)
{
	char text[32];
	snprintf(text, sizeof(text), "%d", n);
	return text;
}

std::string IndustryName(int industry)
{
	const BuildingIndustryInfo* info = BuildingIndustryInfoOf(industry);
	return info ? info->name : "building of industry " + Number(industry);
}

}

double BuildingTerrainLevelRadius(int industry)
{
	switch(industry)
	{
	case 20:
		return 26.246719360351562;
	case 30:
	case 32:
		return 42.650917053222656;
	case 31:
	case 50:
	case 51:
		return 32.80839920043945;
	case 66:
	case 67:
		return 49.21260070800781;
	case 113:
		return 65.6167984008789;
	case 114:
		return 9.842519760131836;
	}
	return 0.0;
}

double BuildingSiteRadius(int industry, int deck)
{
	const BuildingIndustryInfo* info = BuildingIndustryInfoOf(industry);
	const double buildRadius = info && info->hasProcess ? info->buildRadius * MetreInFeet : 0.0;
	const double level = BuildingTerrainLevelRadius(industry);
	if(industry == 50)
		return deck < -1 ? buildRadius : level;
	return level != 0.0 ? level : buildRadius;
}

bool BuildingIsCoastal(int industry)
{
	return industry == 34 || industry == 62 || industry == 85;
}

bool BuildingValidGridCoord(double x, double y, double radius, int industry)
{
	if(fabs(x) > radius || fabs(y) > radius)
		return false;
	return !BuildingIsCoastal(industry) || y <= 0.0;
}

int BuildingMinDeck(int industry)
{
	if(industry == 11 || industry == 12)
		return -5;
	if(industry == 50)
		return -8;
	if(industry == 51)
		return -1;
	return 0;
}

int BuildingMaxDeck(int industry, int material)
{
	switch(industry)
	{
	case 0: case 1: case 2: case 10: case 18: case 19: case 27: case 28: case 29: case 51: case 52: case 68: case 90:
	case 109: case 114: case 129: case 131: case 132: case 133: case 134: case 135: case 136: case 139: case 140:
		return 0;
	case 11: case 12:
		return 2;
	case 70: case 85: case 88: case 113: case 127:
		return 1;
	case 87: case 126:
		return 5;
	case 111:
		return 4;
	case 112: case 128:
		return 3;
	}
	const BuildingIndustryInfo* info = BuildingIndustryInfoOf(industry);
	const int stack = info && info->hasProcess ? info->stackLimit : 99;
	const bool logs = material == 86, lumber = material == 48, stone = material == 76, metal = material == 51, magmium = material == 146;
	switch(stack)
	{
	case 1:
		return logs ? 0 : 1;
	case 3:
		return logs ? 0 : 3;
	case 7:
		return logs ? 0 : lumber ? 3 : 7;
	case 15:
		return logs ? 0 : lumber ? 3 : stone ? 7 : 15;
	case 31:
		return logs ? 0 : lumber ? 3 : stone ? 7 : metal ? 15 : magmium ? 23 : 31;
	case 34:
		return 1;
	}
	return 0;
}

std::string BuildingFileExtension(int industry)
{
	const BuildingIndustryInfo* info = BuildingIndustryInfoOf(industry);
	std::string ext = info ? info->name : "";
	for(size_t i = 0; i < ext.size(); i++)
		if(ext[i] == ' ' || ext[i] == '/' || ext[i] == '\\')
			ext[i] = '_';
	return ext;
}

int BuildingIndustryOfExtension(const std::string& extension)
{
	for(int i = 0; i < BuildingIndustryCount(); i++)
	{
		const std::string ext = BuildingFileExtension(BuildingIndustryAt(i).id);
		if(ext.size() != extension.size())
			continue;
		bool same = true;
		for(size_t k = 0; k < ext.size() && same; k++)
			same = tolower((unsigned char)ext[k]) == tolower((unsigned char)extension[k]);
		if(same)
			return BuildingIndustryAt(i).id;
	}
	return -1;
}

void BuildingToEdit(const OldBuilding::Design& building, Design& out)
{
	out = Design();
	out.id = building.id;
	out.name = building.name;
	out.architect = building.architect;
	out.architectId = building.architectId;
	out.state = building.state;
	out.nullName = building.nullName;
	out.nullArchitect = building.nullArchitect;
	for(size_t i = 0; i < building.rooms.size(); i++)
	{
		const OldBuilding::Room& from = building.rooms[i];
		Room r;
		r.design = from.design;
		r.id = from.id;
		r.deck = from.deck;
		r.type = from.type;
		r.style = from.number & 0xff;
		r.points = from.points;
		out.rooms.push_back(r);
	}
	out.details = building.details;
	out.UpdateDeckRange();
}

void EditToBuilding(const Design& design, OldBuilding::Design& building)
{
	building.name = design.name;
	if(!design.name.empty())
		building.nullName = false;
	building.rooms.clear();
	for(size_t i = 0; i < design.rooms.size(); i++)
	{
		const Room& from = design.rooms[i];
		OldBuilding::Room r;
		r.design = from.design;
		r.id = from.id;
		r.deck = from.deck;
		r.type = from.type;
		r.number = (signed char)(from.style & 0xff);
		r.points = from.points;
		r.UpdateOutline();
		building.rooms.push_back(r);
	}
	building.details = design.details;
}

std::vector<FootPoint> RoomOutline(const Room& room)
{
	std::vector<FootPoint> outline;
	if(room.type == 24 && room.points.size() == 3)
	{
		OldBuilding::Room roof;
		roof.deck = room.deck;
		roof.type = room.type;
		roof.points = room.points;
		roof.UpdateOutline();
		for(size_t i = 0; i < roof.outline.size(); i++)
		{
			FootPoint p;
			p.x = roof.outline[i].x / GridUnit;
			p.y = roof.outline[i].y / GridUnit;
			outline.push_back(p);
		}
		return outline;
	}
	for(size_t i = 0; i < room.points.size(); i++)
	{
		FootPoint p;
		p.x = room.points[i].x;
		p.y = room.points[i].y;
		outline.push_back(p);
	}
	return outline;
}

int WhereInRoom(const Room& room, double gx, double gy)
{
	if(room.type == 24 && room.points.size() == 3)
	{
		const std::vector<FootPoint> o = RoomOutline(room);
		const size_t n = o.size();
		bool inside = false;
		for(size_t i = 0; i < n; i++)
		{
			const double ax = o[i].x, ay = o[i].y;
			const double bx = o[(i + 1) % n].x, by = o[(i + 1) % n].y;
			const double ex = bx - ax, ey = by - ay;
			const double len2 = ex * ex + ey * ey;
			double t = len2 > 0 ? ((gx - ax) * ex + (gy - ay) * ey) / len2 : 0;
			t = t < 0 ? 0 : t > 1 ? 1 : t;
			const double dx = gx - (ax + ex * t), dy = gy - (ay + ey * t);
			if(dx * dx + dy * dy < 1e-10)
				return 1;
			if((ay > gy) != (by > gy) && gx < ax + (gy - ay) * (bx - ax) / (by - ay))
				inside = !inside;
		}
		return inside ? 2 : 0;
	}
	if(PointInOutline(room.points, gx, gy, false))
		return 2;
	return PointInOutline(room.points, gx, gy, true) ? 1 : 0;
}

bool RoofSecondPoint(const GridPoint& p0, double gx, double gy, GridPoint& p1)
{
	const double vx = gx - p0.x, vy = gy - p0.y;
	if(vx == 0.0 && vy == 0.0)
		return false;
	const double turn = floor(atan2(vy, vx) / (Pi / 4) + 0.5) * (Pi / 4);
	const double ux = cos(turn), uy = sin(turn);
	const double along = vx * ux + vy * uy;
	p1 = Pt((int)floor(p0.x + ux * along + 0.5), (int)floor(p0.y + uy * along + 0.5));
	return p1.x != p0.x || p1.y != p0.y;
}

bool RoofThirdPoint(const GridPoint& p0, const GridPoint& p1, double gx, double gy, GridPoint& p2)
{
	const double ex = p1.x - p0.x, ey = p1.y - p0.y;
	const double len = sqrt(ex * ex + ey * ey);
	if(len == 0.0)
		return false;
	const double nx = -ey / len, ny = ex / len;
	const double out = (gx - p0.x) * nx + (gy - p0.y) * ny;
	p2 = Pt((int)floor(p0.x + nx * out + 0.5), (int)floor(p0.y + ny * out + 0.5));
	return (p2.x != p0.x || p2.y != p0.y) && (p2.x != p1.x || p2.y != p1.y);
}

bool RoofPoints(int roofType, const GridPoint& p0, const GridPoint& p1, const GridPoint& p2, std::vector<GridPoint>& points)
{
	points.clear();
	const long turn = (long)(p1.y - p2.y) * (p0.x - p2.x) - (long)(p1.x - p2.x) * (p0.y - p2.y);
	if(turn == 0 || !IsRoofType(roofType))
		return false;
	if(roofType == 24)
	{
		points.push_back(p0);
		points.push_back(p1);
		points.push_back(turn > 0 ? p2 : Pt(2 * p0.x - p2.x, 2 * p0.y - p2.y));
		return true;
	}
	const GridPoint p3 = Pt(p2.x + p1.x - p0.x, p2.y + p1.y - p0.y);
	if(turn > 0)
	{
		points.push_back(p1);
		points.push_back(p0);
		points.push_back(p2);
		points.push_back(p3);
	}
	else
	{
		points.push_back(p0);
		points.push_back(p1);
		points.push_back(p3);
		points.push_back(p2);
	}
	return true;
}

bool CellSet::MiddleInRoom(const Room& room) const
{
	static const double mid[4][2] = { { 1.0, 1.0 / 3.0 }, { 1.0 / 3.0, 1.0 }, { 1.0, 5.0 / 3.0 }, { 5.0 / 3.0, 1.0 } };
	for(size_t k = 0; k < m.size(); k++)
	{
		const int q = m[k] & 3;
		const int j = ((m[k] >> 2) & 1023) - 256;
		const int i = (m[k] >> 12) - 256;
		if(WhereInRoom(room, i * 2 + mid[q][0], j * 2 + mid[q][1]) == 2)
			return true;
	}
	return false;
}

bool Editor::InGrid(double gx, double gy, int deck) const
{
	if(gx < -GridLimit || gx > GridLimit || gy < -GridLimit || gy > GridLimit)
		return false;
	return !b || b->ValidGrid(gx, gy, deck);
}

std::string Editor::DetailName(int detailType) const
{
	const char* inBuilding = b ? DetailTypeBuildingName(detailType) : 0;
	if(inBuilding)
		return inBuilding;
	const DetailTypeInfo* info = DetailTypeInfoOf(detailType);
	return info ? info->name : "equipment";
}

bool Editor::CanPlaceHere(int detailType) const
{
	if(IsSpot(detailType))
		return b != 0;
	if(!b)
		return CanPlace(detailType);
	return DetailTypeInfoOf(detailType) && DetailTypeBuildingName(detailType);
}

bool Editor::CanPaint(int roomType) const
{
	if(!RoomTypeInfoOf(roomType) || IsRoofType(roomType))
		return false;
	return !b || RoomTypeBuildingName(roomType);
}

int Editor::NextRoomNumber(int deck, int roomType) const
{
	std::set<int> used;
	for(size_t i = 0; i < m.rooms.size(); i++)
		if(m.rooms[i].deck == deck && m.rooms[i].type == roomType)
			used.insert(m.rooms[i].style & 0xff);
	for(int number = 1; number < 256; number++)
		if(!used.count(number))
			return number;
	return 0;
}

bool Editor::AddRoof(int deck, const GridPoint& p0, const GridPoint& p1, const GridPoint& p2, int roofType, std::string& why)
{
	if(!b)
	{
		why = "Only a building has roofs.";
		return false;
	}
	Room roof;
	roof.design = m.id;
	roof.deck = deck;
	roof.type = roofType;
	if(!RoofPoints(roofType, p0, p1, p2, roof.points))
	{
		why = "The three points of a roof must not lie in one line.";
		return false;
	}
	if(deck < 0)
	{
		why = "A roof cannot be below the ground.";
		return false;
	}
	if(deck > DeckLimit)
	{
		why = "There is no such storey.";
		return false;
	}
	bool onSite = true;
	for(size_t i = 0; i < roof.points.size() && onSite; i++)
		onSite = InGrid(roof.points[i].x, roof.points[i].y, deck);
	const std::vector<FootPoint> rim = RoomOutline(roof);
	for(size_t i = 0; i < rim.size() && onSite; i++)
		onSite = InGrid(rim[i].x, rim[i].y, deck);
	if(!onSite)
	{
		why = "The roof would reach beyond the building site.";
		return false;
	}
	for(size_t i = 0; i < m.rooms.size(); i++)
	{
		const Room& r = m.rooms[i];
		if(r.deck != deck || r.type != roofType || r.points.size() != roof.points.size())
			continue;
		bool same = true;
		for(size_t k = 0; k < r.points.size() && same; k++)
			same = r.points[k].x == roof.points[k].x && r.points[k].y == roof.points[k].y;
		if(same)
		{
			why = "There is such a roof there already.";
			return false;
		}
	}
	roof.style = NextRoomNumber(deck, roofType);
	roof.id = NextRoomId();
	m.rooms.push_back(roof);
	m.UpdateDeckRange();
	VerifyDetails();
	return true;
}

void Editor::BuildingFaults(std::vector<std::string>& faults) const
{
	if(!b)
		return;
	const std::string industry = IndustryName(b->industry);
	const BuildingIndustryInfo* info = BuildingIndustryInfoOf(b->industry);
	if(!info || BuildingSiteRadius(b->industry, 0) <= 0.0)
		faults.push_back("The 2018 game had no building site for the industry " + industry + ": nothing could be built for it.");
	const int lo = b->MinDeck(), hi = b->MaxDeck();
	int beyond = 0, tooHigh = 0, tooLow = 0, roofLow = 0, equipment = 0;
	for(size_t i = 0; i < m.rooms.size(); i++)
	{
		const Room& r = m.rooms[i];
		const std::vector<FootPoint> o = RoomOutline(r);
		bool onSite = true;
		for(size_t k = 0; k < o.size() && onSite; k++)
			onSite = InGrid(o[k].x, o[k].y, r.deck);
		if(!onSite)
			beyond++;
		if(IsRoofType(r.type))
		{
			if(r.deck < 0)
				roofLow++;
		}
		else if(r.deck > hi)
			tooHigh++;
		else if(r.deck < lo)
			tooLow++;
	}
	for(size_t i = 0; i < m.details.size(); i++)
		if(!IsSpot(m.details[i].type) && !InGrid(m.details[i].x, m.details[i].y, m.details[i].deck))
			equipment++;
	const double radius = BuildingSiteRadius(b->industry, 0);
	if(beyond)
		faults.push_back(Number(beyond) + (beyond == 1 ? " room or roof reaches" : " rooms or roofs reach") + " beyond the building site of a "
				+ industry + ", which is " + Feet(radius) + " ft each way from its middle"
				+ (BuildingIsCoastal(b->industry) ? ", on the land side (south) of the shore line only." : "."));
	if(equipment)
		faults.push_back(Number(equipment) + " pieces of equipment stand beyond the building site of a " + industry + ".");
	if(tooHigh)
		faults.push_back(Number(tooHigh) + (tooHigh == 1 ? " room is" : " rooms are") + " above storey " + Number(hi)
				+ ", the highest a " + industry + " can have.");
	if(tooLow)
		faults.push_back(Number(tooLow) + (tooLow == 1 ? " room is" : " rooms are") + " below storey " + Number(lo)
				+ ", the lowest a " + industry + " can have" + (lo == 0 ? " (it has no basement)." : "."));
	if(roofLow)
		faults.push_back(Number(roofLow) + (roofLow == 1 ? " roof is" : " roofs are") + " below the ground.");
}

}
