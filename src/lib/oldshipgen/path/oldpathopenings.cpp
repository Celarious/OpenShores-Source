#include "oldpath.h"
#include "oldpathmath.h"

#include <cmath>

namespace OldPath
{

namespace
{

using namespace AuMath;

const double ShrinkLargeDoor = 0.9166666666666666;
const double DoorHalfWidth = 2.0;

struct RoomF
{
	uint32_t			design;
	int16_t				id;
	int8_t				deck;
	uint8_t				type;
	std::vector<Point>	outline;
};

struct DetailF
{
	int8_t		deck;
	uint8_t		type;
	int32_t		id;
	Point		loc;
	int16_t		room;
};

bool IsDoor(uint32_t type)
{
	switch(type)
	{
	case 3: case 5: case 8: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36:
	case 0x4d: case 0x4e: case 0x4f: case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56:
	case 0x57: case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f: case 0x60:
	case 0x61:
		return true;
	default:
		return false;
	}
}

std::vector<Opening>& ListFor(Openings& out, const Opening& o)
{
	if(o.type == 0x38)
		return out.railings;
	return IsDoor(o.type) ? out.doors : out.holes;
}

void AddOpening(Openings& out, const Opening& o)
{
	ListFor(out, o).push_back(o);
}

void AddOpeningIfUnique(Openings& out, const Opening& o)
{
	std::vector<Opening>& list = ListFor(out, o);
	const Point a(o.a[0], o.a[1], o.a[2]), b(o.b[0], o.b[1], o.b[2]);
	for(size_t i = 0; i < list.size(); i++)
	{
		const Point la(list[i].a[0], list[i].a[1], list[i].a[2]), lb(list[i].b[0], list[i].b[1], list[i].b[2]);
		if(FuzzyEquals(a, la) && FuzzyEquals(b, lb))
			return;
		if(FuzzyEquals(a, lb) && FuzzyEquals(b, la))
			return;
	}
	list.push_back(o);
}

void Set(double* d, const Point& p)
{
	d[0] = p.x;
	d[1] = p.y;
	d[2] = p.z;
}

Opening FromDetail(const DetailF& d)
{
	Opening o;
	o.id = d.id;
	o.type = d.type;
	o.deck = d.deck;
	o.deck2 = d.deck;
	Set(o.loc, d.loc);
	return o;
}

void TrimDoorSegment(const std::vector<RoomF>& rooms, const Point& loc, int16_t room, uint8_t deck, Point& a, Point& b)
{
	for(size_t r = 0; r < rooms.size(); r++)
	{
		if((int)rooms[r].deck != (int)deck || rooms[r].id == room)
			continue;
		const std::vector<Point>& pts = rooms[r].outline;
		for(size_t i = 0; i < pts.size(); i++)
		{
			if(FuzzyEquals(a, pts[i]) || FuzzyEquals(b, pts[i]))
				continue;
			const Point on = PointOnSegment2D(pts[i], a, b);
			if(!FuzzyEquals2D(on, pts[i]))
				continue;
			if(Between2D(pts[i], a, loc))
				a = pts[i];
			else
				b = pts[i];
		}
	}
}

void LargeDoorSegment(const std::vector<RoomF>& rooms, const Point& loc, int16_t room, Point& a, Point& b)
{
	for(size_t r = 0; r < rooms.size(); r++)
		if(rooms[r].id == room)
		{
			ClosestBoundarySegment2D(rooms[r].outline, loc, a, b);
			TrimDoorSegment(rooms, loc, room, (uint8_t)rooms[r].deck, a, b);
			return;
		}
	a = loc;
	b = loc;
}

bool IsWallDetail(uint8_t type)
{
	return type == 0x15 || (uint8_t)(type - 0x22) < 7 || type == 0x32;
}

void LargeSegment(const std::vector<RoomF>& rooms, const std::vector<DetailF>& details, const DetailF& d, Point& a,
		Point& b)
{
	LargeDoorSegment(rooms, d.loc, d.room, a, b);
	Point dir(b.x - a.x, b.y - a.y, b.z - a.z);
	Unit(dir);
	for(size_t k = 0; k < details.size(); k++)
	{
		const DetailF& w = details[k];
		if(w.deck != d.deck || !IsWallDetail(w.type))
			continue;
		if(FuzzyEquals2D(a, w.loc))
		{
			const Point m = Mul(dir, DoorHalfWidth);
			a = Point(a.x + m.x, a.y + m.y, a.z + m.z);
		}
		else if(FuzzyEquals2D(b, w.loc))
		{
			const Point m = Mul(dir, DoorHalfWidth);
			b = Point(b.x - m.x, b.y - m.y, b.z - m.z);
		}
		else
		{
			Point on = PointOnSegment2D(w.loc, a, b);
			if(Distance2D(w.loc, on) <= Fuzz)
			{
				on = PointOnSegment2D(d.loc, a, w.loc);
				const double away = Distance2D(d.loc, on);
				const Point m = Mul(dir, DoorHalfWidth);
				if(Fuzz < away)
					a = Point(m.x + w.loc.x, m.y + w.loc.y, m.z + w.loc.z);
				else
					b = Point(w.loc.x - m.x, w.loc.y - m.y, w.loc.z - m.z);
			}
		}
	}
	if(d.type == 3)
	{
		dir.x = dir.x * ShrinkLargeDoor;
		dir.y = dir.y * ShrinkLargeDoor;
		dir.z = dir.z * ShrinkLargeDoor;
		a = Point(a.x + dir.x, a.y + dir.y, a.z + dir.z);
		b = Point(b.x - dir.x, b.y - dir.y, b.z - dir.z);
	}
}

void SharedWall(const std::vector<RoomF>& rooms, const DetailF& d, Point& a, Point& b, int16_t& other)
{
	Point a1, b1;
	for(size_t r = 0; r < rooms.size(); r++)
		if(rooms[r].id == d.room)
		{
			ClosestBoundarySegment2D(rooms[r].outline, d.loc, a1, b1);
			break;
		}
	Point a2, b2;
	other = 0;
	for(size_t r = 0; r < rooms.size(); r++)
		if(rooms[r].deck == d.deck && rooms[r].id != d.room && PointOnBoundary2D(rooms[r].outline, d.loc))
		{
			ClosestBoundarySegment2D(rooms[r].outline, d.loc, a2, b2);
			other = rooms[r].id;
			break;
		}
	double dx = b1.x - a1.x, dy = b1.y - a1.y, dz = b1.z - a1.z;
	const double len = std::sqrt(dx * dx + dy * dy + dz * dz);
	if(len != 0.0)
	{
		dx = dx / len;
		dy = dy / len;
		dz = dz / len;
	}
	const double t1 = (a2.x - a1.x) * dx + dy * (a2.y - a1.y) + dz * (a2.z - a1.z);
	const double t2 = (b2.x - a1.x) * dx + dy * (b2.y - a1.y) + dz * (b2.z - a1.z);
	Point start = a1, end = b1;
	if(t1 <= t2)
	{
		if(0.0 < t1)
			start = a2;
		if(t2 < len)
			end = b2;
	}
	else
	{
		if(0.0 < t2)
			start = b2;
		if(t1 < len)
			end = a2;
	}
	if(d.type == 0x35 || d.type == 0x36)
	{
		a = Point(start.x + dx * ShrinkLargeDoor, start.y + dy * ShrinkLargeDoor, start.z + dz * ShrinkLargeDoor);
		b = Point(end.x - dx * ShrinkLargeDoor, end.y - dy * ShrinkLargeDoor, end.z - dz * ShrinkLargeDoor);
	}
	else
	{
		a = start;
		b = end;
	}
}

}

bool FillRoom(FloorBuffer82& buf, const std::vector<Point>& outline, int& minX, int& minY, int& maxX, int& maxY)
{
	if(outline.empty())
		return false;
	buf.Clear();
	uint8_t* m = buf.Raw();
	const Point& last = outline.back();
	int x = (int)std::round(last.x / GridUnit) + 0x80;
	int y = (int)std::round(last.y / GridUnit) + 0x80;
	minX = maxX = x;
	minY = maxY = y;
	std::vector<std::pair<int, int> > edge;
	for(size_t i = 0; i < outline.size(); i++)
	{
		const int tx = (int)std::round(outline[i].x / GridUnit) + 0x80;
		const int ty = (int)std::round(outline[i].y / GridUnit) + 0x80;
		if(tx < minX) minX = tx;
		if(maxX < tx) maxX = tx;
		if(ty < minY) minY = ty;
		if(maxY < ty) maxY = ty;
		const int sx = tx < x ? -1 : (x < tx ? 1 : 0);
		const int sy = ty < y ? -1 : (y < ty ? 1 : 0);
		while(x != tx || y != ty)
		{
			m[(uint16_t)(x * 0x100 + y)] = 1;
			edge.push_back(std::make_pair(x, y));
			x += sx;
			y += sy;
			if(x < minX || maxX < x || y < minY || maxY < y)
				return false;
		}
	}
	if(edge.size() < 2)
		return true;
	std::pair<int, int> pp = edge[edge.size() - 2], p = edge.back();
	for(size_t i = 0; i < edge.size(); i++)
	{
		const std::pair<int, int>& q = edge[i];
		const int inX = p.first - pp.first, inY = p.second - pp.second;
		const int outX = q.first - p.first, outY = q.second - p.second;
		bool fill = false;
		if(outX == 1)
		{
			if(outY == 1)
				fill = inX == -1 ? !((unsigned)(inY + 1) < 2) : true;
		}
		else if(outX == 0)
		{
			if(outY == 1)
			{
				if(inX == -1)
					fill = !((unsigned)(inY + 1) < 2);
				else
					fill = inX != 0 || inY != -1;
			}
			else if(inX == -1)
				fill = inY == 1;
		}
		else if(outY == 1)
			fill = inY == 1 || (inX == 1 && inY == 0);
		else if(q.second == p.second || inX != 1)
			fill = inY == 1;
		if(fill)
			for(int fx = p.first + 1; fx < maxX; fx++)
			{
				uint8_t& c = m[(uint16_t)(fx * 0x100 + p.second)];
				if(c != 0)
					break;
				c = 2;
			}
		pp = p;
		p = q;
	}
	return true;
}

namespace
{

void MarkCell(FloorBuffer82& buf, const Point& loc)
{
	int x, y;
	HalfGridCoord(loc, x, y);
	buf.Raw()[(uint16_t)(x * 0x100 + y)] = 3;
}

}

void AnalyzeOpenings(const Design& design, Openings& out)
{
	out = Openings();

	std::vector<RoomF> rooms(design.rooms.size());
	for(size_t i = 0; i < design.rooms.size(); i++)
	{
		const Room& r = design.rooms[i];
		RoomF& f = rooms[i];
		f.design = r.design;
		f.id = (int16_t)r.id;
		f.deck = (int8_t)r.deck;
		f.type = (uint8_t)r.type;
		for(size_t k = 0; k < r.points.size(); k++)
			f.outline.push_back(Point(r.points[k].x * GridUnit, r.points[k].y * GridUnit, f.deck * DeckHeight));
	}
	std::vector<DetailF> details(design.details.size());
	for(size_t i = 0; i < design.details.size(); i++)
	{
		const Detail& d = design.details[i];
		DetailF& f = details[i];
		f.deck = (int8_t)d.deck;
		f.type = (uint8_t)d.type;
		f.id = d.id;
		f.loc = Point(d.x * GridUnit, d.y * GridUnit, f.deck * DeckHeight);
		f.room = (int16_t)d.room;
	}

	bool any = false;
	for(size_t i = 0; i < rooms.size(); i++)
	{
		const RoomF& r = rooms[i];
		if(r.type == 0x16 || (unsigned)(r.type - 0x17) < 2 || r.outline.empty())
			continue;
		if(!any)
		{
			any = true;
			out.lowDeck = out.highDeck = r.deck;
		}
		else if(r.deck < out.lowDeck)
			out.lowDeck = r.deck;
		else if(out.highDeck < r.deck)
			out.highDeck = r.deck;
	}

	for(size_t i = 0; i < details.size(); i++)
	{
		const DetailF& d = details[i];
		if(d.type > 0x8e)
			continue;
		switch(d.type)
		{
		case 3:
		case 9:
		{
			Point a, b;
			LargeSegment(rooms, details, d, a, b);
			Opening o = FromDetail(d);
			Set(o.a, a);
			Set(o.b, b);
			o.room1 = d.room;
			AddOpeningIfUnique(out, o);
			break;
		}
		case 5:
		case 0x31:
		{
			int16_t room1 = 0, room2 = 0;
			Point u;
			for(size_t r = 0; r < rooms.size(); r++)
			{
				const RoomF& room = rooms[r];
				if(room.id == d.room)
				{
					Point a, b;
					ClosestBoundarySegment2D(room.outline, d.loc, a, b);
					u = Point(b.x - a.x, b.y - a.y, b.z - a.z);
					Unit(u, DoorHalfWidth);
					room1 = room.id;
				}
				else if(room2 == 0)
				{
					if(room.deck != d.deck)
						continue;
					if(PointOnBoundary2D(room.outline, d.loc))
						room2 = room.id;
				}
				if(room1 != 0 && room2 != 0)
					break;
			}
			Opening o = FromDetail(d);
			o.a[0] = u.x + d.loc.x;
			o.a[1] = u.y + d.loc.y;
			o.a[2] = u.z + d.loc.z;
			o.b[0] = d.loc.x - u.x;
			o.b[1] = d.loc.y - u.y;
			o.b[2] = d.loc.z - u.z;
			o.room1 = room1;
			o.room2 = room2;
			AddOpening(out, o);
			break;
		}
		case 8: case 0x33:
		case 0x4d: case 0x4e: case 0x4f: case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56:
		case 0x57: case 0x58: case 0x59: case 0x5a:
		{
			int16_t below = 0;
			for(size_t r = 0; r < rooms.size(); r++)
				if((int)rooms[r].deck == (int)d.deck - 1 && PointInBoundary(rooms[r].outline, d.loc))
				{
					below = rooms[r].id;
					break;
				}
			Opening o = FromDetail(d);
			o.deck2 = (int8_t)(d.deck - 1);
			Set(o.a, d.loc);
			o.room1 = d.room;
			o.room2 = below;
			AddOpening(out, o);
			break;
		}
		case 0x32:
		{
			Point u;
			for(size_t r = 0; r < rooms.size(); r++)
				if(rooms[r].id == d.room)
				{
					Point a, b;
					ClosestBoundarySegment2D(rooms[r].outline, d.loc, a, b);
					u = Point(b.x - a.x, b.y - a.y, b.z - a.z);
					Unit(u, DoorHalfWidth);
					break;
				}
			Opening o = FromDetail(d);
			o.a[0] = u.x + d.loc.x;
			o.a[1] = u.y + d.loc.y;
			o.a[2] = u.z + d.loc.z;
			o.b[0] = d.loc.x - u.x;
			o.b[1] = d.loc.y - u.y;
			o.b[2] = d.loc.z - u.z;
			o.room1 = d.room;
			AddOpening(out, o);
			break;
		}
		case 0x34:
		case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f: case 0x60: case 0x61:
		{
			Opening o = FromDetail(d);
			o.deck2 = 0;
			for(size_t r = 0; r < rooms.size(); r++)
				if(rooms[r].id == d.room)
				{
					o.deck2 = d.deck;
					if(rooms[r].deck == d.deck)
						o.deck2 = (int8_t)(d.deck - 1);
					break;
				}
			Set(o.a, d.loc);
			o.room1 = d.room;
			AddOpening(out, o);
			break;
		}
		case 0x35: case 0x36: case 0x37: case 0x38:
		{
			Point a, b;
			int16_t other;
			SharedWall(rooms, d, a, b, other);
			Opening o = FromDetail(d);
			Set(o.a, a);
			Set(o.b, b);
			o.room1 = d.room;
			o.room2 = other;
			AddOpeningIfUnique(out, o);
			break;
		}
		default:
			break;
		}
	}

	FloorBuffer82 upper, lower;
	int uMinX = 0, uMinY = 0, uMaxX = 0, uMaxY = 0, lMinX = 0, lMinY = 0, lMaxX = 0, lMaxY = 0;
	for(size_t i = 0; i < rooms.size(); i++)
	{
		const RoomF& up = rooms[i];
		if(up.type != 0xb && up.type != 0xf)
			continue;
		if((int)up.deck == out.lowDeck)
			continue;
		if(!FillRoom(upper, up.outline, uMinX, uMinY, uMaxX, uMaxY))
			continue;
		if(up.type == 0xf)
			for(size_t k = 0; k < details.size(); k++)
			{
				if(details[k].room != up.id)
					continue;
				switch(details[k].type)
				{
				case 8: case 0x21: case 0x33: case 0x34:
				case 0x4d: case 0x4e: case 0x4f: case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55:
				case 0x56: case 0x57: case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e:
				case 0x5f: case 0x60: case 0x61:
					MarkCell(upper, details[k].loc);
					break;
				default:
					break;
				}
			}
		for(size_t j = 0; j < rooms.size(); j++)
		{
			const RoomF& lo = rooms[j];
			if((int)lo.deck != (int)up.deck - 1)
				continue;
			if(!FillRoom(lower, lo.outline, lMinX, lMinY, lMaxX, lMaxY))
				continue;
			for(size_t k = 0; k < details.size(); k++)
			{
				if(details[k].room != lo.id)
					continue;
				switch(details[k].type)
				{
				case 10: case 0xb: case 0xe: case 0x10: case 0x12: case 0x17: case 0x18: case 0x20: case 0x4a: case 0x4b:
					MarkCell(lower, details[k].loc);
					break;
				default:
					break;
				}
			}
			const int x0 = lMinX < uMinX ? uMinX : lMinX;
			const int y0 = lMinY < uMinY ? uMinY : lMinY;
			const int x1 = uMaxX < lMaxX ? uMaxX : lMaxX;
			const int y1 = uMaxY < lMaxY ? uMaxY : lMaxY;
			if(!(x0 < x1 && y0 < y1))
				continue;
			for(int x = x0; x <= x1; x++)
				for(int y = y0; y <= y1; y++)
				{
					const unsigned k = (unsigned)(x * 0x100 + y);
					if(upper.Raw()[(uint16_t)k] != 2 || lower.Raw()[(uint16_t)k] != 2)
						continue;
					const Point p((double)(x - 0x80) * GridUnit, (double)(y - 0x80) * GridUnit,
							(double)(int)up.deck * DeckHeight);
					Opening o;
					o.type = up.type == 0xb ? 10 : 0;
					o.deck = up.deck;
					o.deck2 = (int8_t)(up.deck - 1);
					o.room1 = up.id;
					o.room2 = lo.id;
					Set(o.a, p);
					Set(o.loc, p);
					AddOpening(out, o);
				}
		}
	}
}

}
