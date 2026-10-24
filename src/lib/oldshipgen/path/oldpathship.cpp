#include "oldpathship.h"
#include "oldpathmathstep.h"

#include <cmath>
#include <cstring>

namespace OldPath
{

using namespace AuMath;

namespace
{

#include "oldpathstairsurf.inc"

double Bits(uint64_t u)
{
	double d;
	std::memcpy(&d, &u, 8);
	return d;
}

Point MinPt(const Point& self, const Point& o)
{
	return Point(o.x < self.x ? o.x : self.x, o.y < self.y ? o.y : self.y, o.z < self.z ? o.z : self.z);
}

Point MaxPt(const Point& self, const Point& o)
{
	return Point(o.x > self.x ? o.x : self.x, o.y > self.y ? o.y : self.y, o.z > self.z ? o.z : self.z);
}

double Distance2(const Point& self, const Point& o)
{
	double dx = self.x - o.x, dy = self.y - o.y, dz = self.z - o.z;
	return dy * dy + dx * dx + dz * dz;
}

bool InTriangle(const Point& p, const Point& a, const Point& b, const Point& c, bool strict)
{
	double d = (a.x - p.x) * (b.y - p.y) - (b.x - p.x) * (a.y - p.y);
	int s1 = 0.0 <= d ? (0.0 < d) : -1;
	d = (c.y - p.y) * (b.x - p.x) - (c.x - p.x) * (b.y - p.y);
	int s2 = 0.0 <= d ? (0.0 < d) : -1;
	d = (c.x - p.x) * (a.y - p.y) - (c.y - p.y) * (a.x - p.x);
	int s3 = 0.0 <= d ? (0.0 < d) : -1;
	if((0 < s1 || 0 < s2 || 0 < s3) && (strict || s1 < 0 || s2 < 0 || s3 < 0))
		return false;
	return true;
}

const uint64_t StairCos[8] = { 0x3ff0000000000000ull, 0x3fe6a09e667f3bcdull, 0x3c91a62633145c07ull,
	0xbfe6a09e667f3bccull, 0xbff0000000000000ull, 0xbfe6a09e667f3bceull, 0xbcaa79394c9e8a0aull, 0x3fe6a09e667f3bcbull };
const uint64_t StairSin[8] = { 0x0000000000000000ull, 0x3fe6a09e667f3bcdull, 0x3ff0000000000000ull,
	0x3fe6a09e667f3bcdull, 0x3ca1a62633145c07ull, 0xbfe6a09e667f3bccull, 0xbff0000000000000ull, 0xbfe6a09e667f3bceull };

}

DesignBounds ShipFrame::Bounds() const
{
	DesignBounds b;
	b.minX = min.x;
	b.minY = min.y;
	b.maxX = max.x;
	b.maxY = max.y;
	b.lowDeck = lowDeck;
	b.highDeck = highDeck;
	return b;
}

const RoomInfo* ShipFrame::FindRoom(uint16_t id) const
{
	for(size_t i=0; i<rooms.size(); i++)
		if(rooms[i].id == id)
			return &rooms[i];
	return 0;
}

void AnalyzeFrame(const Design& design, ShipFrame& out)
{
	out = ShipFrame();
	bool any = false;
	for(size_t i=0; i<design.rooms.size(); i++)
	{
		const Room& r = design.rooms[i];
		RoomInfo info;
		info.id = (uint16_t)r.id;
		info.deck = (int8_t)r.deck;
		info.type = (uint8_t)r.type;
		info.style = (uint8_t)r.style;
		out.rooms.push_back(info);
		uint8_t type = (uint8_t)r.type;
		if(type == 0x16 || (unsigned)(type - 0x17) < 2 || r.points.empty())
			continue;
		std::vector<Point> pts;
		for(size_t j=0; j<r.points.size(); j++)
			pts.push_back(Point((double)r.points[j].x * GridUnit, (double)r.points[j].y * GridUnit,
				(double)(int8_t)r.deck * DeckHeight));
		int8_t deck = (int8_t)r.deck;
		if(!any)
		{
			Extents(pts, out.min, out.max);
			any = true;
			out.lowDeck = out.highDeck = deck;
		}
		else
		{
			Point mn, mx;
			Extents(pts, mn, mx);
			out.min = MinPt(out.min, mn);
			out.max = MaxPt(out.max, mx);
			if(deck < out.lowDeck)
				out.lowDeck = deck;
			else if(out.highDeck < deck)
				out.highDeck = deck;
		}
	}
	if(any)
	{
		out.max.z = out.max.z + 8.0;
		out.min.z = out.min.z - 8.0;
	}
	Point sum(out.max.x + out.min.x, out.min.y + out.max.y, out.min.z + out.max.z);
	out.cg = Point(sum.x / 2.0, sum.y / 2.0, sum.z / 2.0);
	for(size_t i=0; i<design.details.size(); i++)
	{
		const Detail& d = design.details[i];
		if(d.type > 0x8e)
			continue;
		int8_t deck = (int8_t)d.deck;
		Point loc((double)d.x * GridUnit, (double)d.y * GridUnit, (double)deck * DeckHeight);
		if(d.type == 0x4c)
		{
			if(out.min.x <= loc.x && out.min.y <= loc.y && out.min.z <= loc.z && loc.x <= out.max.x &&
					loc.y <= out.max.y && loc.z <= out.max.z)
				out.cg = loc;
		}
		else if(d.type >= 0x65 && d.type <= 0x8c)
		{
			StairDetail s;
			s.deck = deck;
			s.type = (uint8_t)d.type;
			s.loc = loc;
			out.stairs.push_back(s);
		}
	}
}

void StairSurfaces(uint8_t type, const Point& loc, std::vector<Tri>& out)
{
	if(type < 0x65 || type > 0x8c)
		return;
	int i = type - 0x65, shape = i / 8, k = i % 8;
	const SurfTemplate& t = SurfTemplates[shape][k & 1];
	double c = Bits(StairCos[k]), s = Bits(StairSin[k]);
	for(int n=0; n<t.count; n++)
	{
		Tri tri;
		for(int v=0; v<3; v++)
		{
			double x = Bits(t.tris[n][3 * v]), y = Bits(t.tris[n][3 * v + 1]), z = Bits(t.tris[n][3 * v + 2]);
			tri.v[v].x = (x * c - y * s) + loc.x;
			tri.v[v].z = z + loc.z;
			tri.v[v].y = y * c + x * s + loc.y;
		}
		out.push_back(tri);
	}
}

bool TriIntersectZ(const Tri& t, const Point& p, Point& hit, Point& normal)
{
	const Point& a = t.v[0];
	const Point& b = t.v[1];
	const Point& c = t.v[2];
	if(!InTriangle(p, a, b, c, false))
		return false;
	Point n((b.z - a.z) * (c.y - a.y) - (c.z - a.z) * (b.y - a.y),
		(c.z - a.z) * (b.x - a.x) - (b.z - a.z) * (c.x - a.x),
		(c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x));
	Unit(n);
	const double dx = 0.0, dy = 0.0, dz = 1.0;
	double den = dy * n.y + dx * n.x + dz * n.z;
	if(den == 0.0)
		return false;
	double tt = ((a.y * n.y + a.x * n.x + a.z * n.z) - (p.y * n.y + p.x * n.x + p.z * n.z)) / den;
	double ox, oy, oz;
	if(tt == 0.0)
		ox = oy = oz = 0.0;
	else
	{
		ox = dx * tt;
		oy = dy * tt;
		oz = dz * tt;
	}
	hit = Point(p.x + ox, p.y + oy, p.z + oz);
	normal = n;
	return true;
}

bool TestStairs(uint8_t type, const Point& loc, const Point& p, double& height, Point& normal)
{
	std::vector<Tri> tris;
	StairSurfaces(type, loc, tris);
	for(size_t i=0; i<tris.size(); i++)
	{
		Point hit;
		if(TriIntersectZ(tris[i], p, hit, normal))
		{
			height = p.z - hit.z;
			return true;
		}
	}
	height = 0.0;
	return false;
}

int ClosestDeck(double z)
{
	float f = std::round((float)(z * 0.125));
	int d = (int)f;
	if(d < -0x7f)
		return -0x7f;
	return d > 0x7f ? 0x7f : d;
}

bool IsValidGridCoord(const Point& p)
{
	double x = std::round(p.x / GridUnit);
	if(!(-120.0 <= x && x <= 120.0))
		return false;
	double y = std::round(p.y / GridUnit);
	return -120.0 <= y && y <= 120.0;
}

bool OpeningIsOnWall(uint32_t type)
{
	switch(type)
	{
	case 3: case 5: case 9: case 0x31: case 0x32: case 0x35: case 0x36: case 0x37: case 0x38:
		return true;
	default:
		return false;
	}
}

namespace
{

uint16_t EncodeGridCoord(const Point& p)
{
	double x = std::round(p.x / GridUnit);
	double y = std::round(p.y / GridUnit);
	return (uint16_t)(((int)y & 0xff) | (uint16_t)((int16_t)(int8_t)(int)x << 8));
}

Point DecodeGridCoord(uint16_t code, int8_t deck)
{
	return Point((double)(int)(int8_t)(code >> 8) * GridUnit, (double)(int)(int8_t)(code & 0xff) * GridUnit,
		(double)(int)deck * DeckHeight);
}

uint16_t RoomArrayInsideRoom(const RoomArray& rooms, const Point& p)
{
	int deck = OnDeck(p.z);
	for(size_t i=0; i<rooms.rooms.size(); i++)
		if(rooms.rooms[i].deck == deck && PointInBoundary(rooms.rooms[i].points, p))
			return rooms.rooms[i].id;
	return 0;
}

}

ShipWalk::ShipWalk(const Path& path_, const RoomArray& rooms_, const Openings& openings_, const ShipFrame& frame_,
		const DoorStates& doors_)
	: designEnv(false), valid(true), crewDoorsLocked(false), hullDoorsLocked(false), hullRay(0), hullRayCtx(0),
	path(path_), rooms(rooms_), openings(openings_), frame(frame_), doors(doors_), bounds(frame_.Bounds())
{
}

Point ShipWalk::ToDesign(const Point& p) const
{
	if(designEnv)
		return p;
	return Point(p.x + frame.cg.x, p.y + frame.cg.y, p.z + frame.cg.z);
}

Point ShipWalk::FromDesign(const Point& p) const
{
	if(designEnv)
		return p;
	return Point(p.x - frame.cg.x, p.y - frame.cg.y, p.z - frame.cg.z);
}

int ShipWalk::IsInOrOnHull(const Point& p) const
{
	if(!valid)
		return 0;
	int t = DqPathCellTypeAt(path, bounds, ToDesign(p));
	if(t == 0)
		return 0;
	if(t == 0x11)
		return 3;
	if(t == 0x10 || t == 7)
		return 2;
	return 1;
}

PathTest ShipWalk::StepTest(const Point& a, const Point& b, const Walker& who, std::vector<uint16_t>& openDoors) const
{
	bool lockedOut = crewDoorsLocked && !who.crewOrTroop && !(who.kind == Walker::Person && who.inChainOfCommand);
	bool crew = !lockedOut;
	bool noRailings = who.sameFrame && !who.onGround;
	bool onStairs = false;
	if(who.kind == Walker::Person || who.kind == Walker::Animal)
		return path.TestSpacecraftPath(a, b, crew, rooms, doors, openDoors, noRailings, onStairs);
	if(!who.sameFrame && !who.sameGrandparent)
		return DqTestSpacecraftPath(path, a, b, crew, rooms, doors, openDoors, noRailings);
	if(who.kind == Walker::Vessel)
	{
		double dz = b.z - a.z, dy = b.y - a.y, dx = b.x - a.x;
		for(size_t i=0; i<who.box.size(); i++)
		{
			Point c = ToDesign(who.box[i]);
			Point e(dx + c.x, dy + c.y, dz + c.z);
			bool st = false;
			PathTest r = path.TestSpacecraftPath(c, e, crew, rooms, doors, openDoors, false, st);
			if(r != PathOk)
				return r;
		}
		return DqTestSpacecraftPath(path, a, b, crew, rooms, doors, openDoors, false);
	}
	if(who.kind == Walker::Vehicle)
	{
		Point d = Sub(b, a);
		for(size_t i=0; i<who.box.size(); i++)
		{
			Point c = ToDesign(who.box[i]);
			PathTest r = DqTestSpacecraftPath(path, c, Add(c, d), crew, rooms, doors, openDoors, false);
			if(r != PathOk)
				return r;
		}
		return DqTestSpacecraftPath(path, a, b, crew, rooms, doors, openDoors, false);
	}
	return DqTestSpacecraftPath(path, a, b, crew, rooms, doors, openDoors, noRailings);
}

bool ShipWalk::IsPathBlocked(const Point& old, const Point& now, const Walker& who, Point& normal,
		std::vector<uint16_t>& openDoors, bool& askParent, PathTest& code) const
{
	askParent = false;
	code = PathOk;
	if(!valid)
	{
		normal = Point(0, 0, 0);
		code = PathRefused;
		return true;
	}
	if(who.kind == Walker::Person && who.ghost)
		return false;
	if(designEnv && !who.renderSolid)
		return false;
	Point a = ToDesign(old), b = ToDesign(now);
	code = StepTest(a, b, who, openDoors);
	const double h = Bits(0x3fe6a0902de00d1bull);
	static const double side[8][2] = { { 1, 0 }, { h, h }, { 0, 1 }, { -h, h }, { -1, 0 }, { -h, -h }, { 0, -1 },
		{ h, -h } };
	switch((int)code)
	{
	case PathRefused:
		normal = Point(0, 0, 0);
		return true;
	case PathFloorHit:
		normal = Point(0, 0, 1);
		return true;
	case PathCeilingHit:
		normal = Point(0, 0, -1);
		return true;
	default:
		if(code >= PathWallHit && code <= PathDoorHit + 7)
		{
			int k = (code - PathWallHit) % 8;
			normal = Point(side[k][0], side[k][1], 0);
			return true;
		}
		askParent = true;
		return false;
	}
}

bool ShipWalk::CheckPeerJoin(const Point& old, const Point& now, const Walker& who,
		std::vector<uint16_t>& openDoors) const
{
	if(!valid)
		return false;
	Point normal;
	bool askParent;
	PathTest code;
	if(designEnv)
	{
		if(!who.renderSolid)
			return false;
		if(IsInOrOnHull(now) == 0)
			return false;
		if(IsInOrOnHull(old) == 0 && IsPathBlocked(old, now, who, normal, openDoors, askParent, code))
			return false;
		return true;
	}
	if(IsInOrOnHull(now) == 0)
		return false;
	return !IsPathBlocked(old, now, who, normal, openDoors, askParent, code);
}

bool ShipWalk::CheckChildExit(const Point&, const Point& now, const Walker& who) const
{
	if(designEnv && !who.renderSolid)
		return true;
	return IsInOrOnHull(now) == 0;
}

double ShipWalk::FloorHt(const Point& p, bool& gravity, bool& onStairs, Point& normal, bool plusDeck) const
{
	if(!valid)
	{
		gravity = false;
		onStairs = false;
		return 0.0;
	}
	Point q = ToDesign(p);
	if(DqPathCellTypeAt(path, bounds, q) == 0x11 && hullRay)
	{
		double hitZ = 0.0;
		if(hullRay(hullRayCtx, p, Point(p.x, p.y, p.z - 8.0), hitZ))
		{
			gravity = false;
			onStairs = false;
			return p.z - hitZ;
		}
	}
	int8_t below = DqDeckBelow(path, q, gravity, rooms);
	if(!frame.stairs.empty())
	{
		int top = OnDeck(q.z);
		for(int d = top; ; d--)
		{
			if(d <= below)
				d = below;
			for(size_t i=0; i<frame.stairs.size(); i++)
			{
				const StairDetail& s = frame.stairs[i];
				if(s.deck != d)
					continue;
				double hgt;
				if(TestStairs(s.type, s.loc, q, hgt, normal) && 0.0 <= hgt)
				{
					onStairs = true;
					return hgt;
				}
			}
			if(d == below)
				break;
		}
	}
	onStairs = false;
	double z = q.z - (double)(int)below * 8.0;
	if(plusDeck)
		z = z + 8.0;
	return z;
}

int ShipWalk::OnDesignDeck(const Point& p) const
{
	if(!valid)
		return 0;
	return OnDeck(designEnv ? p.z : p.z + frame.cg.z);
}

bool ShipWalk::InsideRoom(const Point& p, uint16_t& room) const
{
	room = 0;
	if(!valid)
	{
		room = RoomArrayInsideRoom(rooms, p);
		return room != 0;
	}
	if(designEnv)
	{
		const double m = 1.41421356225;
		if(p.x < frame.min.x - m || p.y < frame.min.y - m || frame.max.x + m < p.x || frame.max.y + m < p.y)
			return false;
		int deck = OnDeck(p.z);
		if(deck < frame.lowDeck || frame.highDeck < deck)
			return false;
		room = RoomArrayInsideRoom(rooms, p);
		return room != 0;
	}
	Point q(p.x + frame.cg.x, p.y + frame.cg.y, p.z + frame.cg.z);
	return DqInsideRoom(path, bounds, q, rooms, room);
}

uint16_t ShipWalk::InsideRoomType(const Point& p, int& type, uint16_t& style) const
{
	if(!valid)
	{
		type = 0;
		style = 0;
		return 0;
	}
	uint16_t room;
	InsideRoom(p, room);
	if(room == 0)
	{
		type = 0;
		style = 0;
		return 0;
	}
	const RoomInfo* r = frame.FindRoom(room);
	if(r)
	{
		type = r->type;
		style = r->style;
	}
	return room;
}

bool ShipWalk::InsideRoomType(const Point& p, uint16_t& room, int& type, uint16_t& style, int8_t& deck,
		double& height, Point& normal) const
{
	normal = Point(0, 0, 1);
	if(!valid)
	{
		room = 0;
		type = 0;
		style = 0;
		deck = 0;
		height = 0.0;
		return false;
	}
	bool in = InsideRoom(p, room);
	Point q = ToDesign(p);
	if(room == 0)
	{
		type = 0;
		style = 0;
		deck = (int8_t)OnDesignDeck(p);
		height = q.z - (double)(int)deck * 8.0;
		return in;
	}
	const RoomInfo* r = frame.FindRoom(room);
	if(r)
	{
		type = r->type;
		style = r->style;
		deck = r->deck;
		height = q.z - (double)(int)r->deck * 8.0;
	}
	return in;
}

int ShipWalk::HatchAbove(const Point& p) const
{
	if(!designEnv)
	{
		Point q = Add(p, frame.cg);
		if(IsValidGridCoord(q))
		{
			int i = path.HatchAbove(q);
			if(-1 < i)
				return doors.IsDoorOpen((unsigned)i) + 1;
		}
		return 0;
	}
	if(!IsValidGridCoord(p))
		return 0;
	int deck = OnDeck(p.z);
	Point q = DecodeGridCoord(EncodeGridCoord(p), (int8_t)(deck + 1));
	for(size_t i=0; i<openings.doors.size(); i++)
	{
		const Opening& o = openings.doors[i];
		if((unsigned)(int)o.deck == (unsigned)(deck + 1) && !OpeningIsOnWall(o.type) &&
				FuzzyEquals2D(Point(o.a[0], o.a[1], o.a[2]), q))
		{
			int k = DoorIndex(o.id);
			return doors.IsDoorOpen((unsigned)k) + 1;
		}
	}
	return 0;
}

int ShipWalk::HatchBelow(const Point& p) const
{
	if(!designEnv)
	{
		Point q = Add(p, frame.cg);
		if(IsValidGridCoord(q))
		{
			int i = path.HatchDoorIndex(q);
			if(-1 < i)
				return doors.IsDoorOpen((unsigned)i) + 1;
		}
		return 0;
	}
	if(!IsValidGridCoord(p))
		return 0;
	int deck = OnDeck(p.z);
	Point q = DecodeGridCoord(EncodeGridCoord(p), (int8_t)deck);
	for(size_t i=0; i<openings.doors.size(); i++)
	{
		const Opening& o = openings.doors[i];
		if(o.deck == deck && !OpeningIsOnWall(o.type) && FuzzyEquals2D(Point(o.a[0], o.a[1], o.a[2]), q))
		{
			int k = DoorIndex(o.id);
			return doors.IsDoorOpen((unsigned)k) + 1;
		}
	}
	return 0;
}

int ShipWalk::HatchDoorIndex(const Point& p) const
{
	return path.HatchDoorIndex(Add(p, frame.cg));
}

int ShipWalk::HatchDoorIndex(const Point& p, bool& crew) const
{
	return path.HatchDoorIndex(Add(p, frame.cg), crew);
}

int ShipWalk::HatchAt(const Point& p) const
{
	int deck = ClosestDeck(p.z);
	for(size_t i=0; i<openings.doors.size(); i++)
	{
		const Opening& o = openings.doors[i];
		if(o.deck == deck && !OpeningIsOnWall(o.type) && FuzzyEquals2D(Point(o.a[0], o.a[1], o.a[2]), p))
			return o.id;
	}
	return 0;
}

bool ShipWalk::IsHatchAt(const Point& p, uint32_t& type) const
{
	int deck = ClosestDeck(p.z);
	for(size_t i=0; i<openings.doors.size(); i++)
	{
		const Opening& o = openings.doors[i];
		if(o.deck == deck && !OpeningIsOnWall(o.type) && FuzzyEquals2D(Point(o.a[0], o.a[1], o.a[2]), p))
		{
			type = o.type;
			return true;
		}
	}
	type = 0;
	return false;
}

bool ShipWalk::IsTopOrBottomOpeningAt(const Point& p, const Design& design)
{
	int deck = ClosestDeck(p.z);
	for(size_t i=0; i<design.details.size(); i++)
	{
		const Detail& d = design.details[i];
		uint8_t t = (uint8_t)d.type;
		if((int8_t)d.deck != deck)
			continue;
		if(!(t == 8 || (uint8_t)(t - 0x33) < 2 || (uint8_t)(t - 0x4d) < 0x15 || t == 0x21 || t == 0x16))
			continue;
		Point loc((double)d.x * GridUnit, (double)d.y * GridUnit, (double)(int8_t)d.deck * DeckHeight);
		if(FuzzyEquals(loc, p))
			return true;
	}
	return false;
}

int ShipWalk::HullDoorIndex(const Point& p) const
{
	if(!valid)
		return -1;
	Point q = ToDesign(p);
	int best = -1;
	double bestD = 0.0;
	for(size_t i=0; i<openings.doors.size(); i++)
	{
		const Opening& o = openings.doors[i];
		if(o.type != 3)
			continue;
		double d = Distance2(q, Point(o.loc[0], o.loc[1], o.loc[2]));
		if(best != -1 && bestD <= d)
			continue;
		best = (int)i;
		bestD = d;
	}
	return best;
}

int ShipWalk::HullDoorIndex(const Point& p, uint16_t room) const
{
	if(!valid)
		return -1;
	Point q = ToDesign(p);
	int best = -1;
	double bestD = 0.0;
	for(size_t i=0; i<openings.doors.size(); i++)
	{
		const Opening& o = openings.doors[i];
		if(o.type != 3 || ((uint16_t)o.room1 != room && (uint16_t)o.room2 != room))
			continue;
		double d = Distance2(q, Point(o.loc[0], o.loc[1], o.loc[2]));
		if(best != -1 && bestD <= d)
			continue;
		best = (int)i;
		bestD = d;
	}
	return best;
}

bool ShipWalk::IsHullDoor(int i) const
{
	if(i < 0 || (size_t)i >= openings.doors.size())
		return false;
	switch(openings.doors[i].type)
	{
	case 3: case 9: case 0x32: case 0x34: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f: case 0x60: case 0x61:
		return true;
	default:
		return false;
	}
}

bool ShipWalk::IsDoorHullHatch(unsigned i) const
{
	if(i >= openings.doors.size())
		return false;
	uint32_t t = openings.doors[i].type;
	return t == 0x34 || t - 0x5b < 7;
}

bool ShipWalk::IsDoorLocked(unsigned i) const
{
	if(i >= openings.doors.size())
		return false;
	uint32_t t = openings.doors[i].type;
	uint32_t u = t - 0x31;
	if(u < 0x2a && ((0x3f800000025ull >> (u & 0x3f)) & 1))
		return crewDoorsLocked;
	switch(t)
	{
	case 3: case 9: case 0x32: case 0x34: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f: case 0x60: case 0x61:
		return hullDoorsLocked;
	default:
		return false;
	}
}

int ShipWalk::DoorIndex(int32_t detailId) const
{
	for(size_t i=0; i<openings.doors.size(); i++)
		if(openings.doors[i].id == detailId)
			return (int)i;
	return -1;
}

Point ShipWalk::HatchLadderRotation(unsigned i) const
{
	float a = 0.0f;
	if(i < openings.doors.size())
	{
		switch(openings.doors[i].type)
		{
		case 0x19: case 0x22: case 0x3a: case 0x42: case 0x4b: case 0x4d: case 0x54: case 0x5b: case 0x66: case 0x6e:
		case 0x76: case 0x7e: case 0x86:
			a = 0.7853981852531433f;
			break;
		case 0x1a: case 0x23: case 0x3b: case 0x43: case 0x4e: case 0x55: case 0x5c: case 0x67: case 0x6f: case 0x77:
		case 0x7f: case 0x87:
			a = 1.5707963705062866f;
			break;
		case 0x1b: case 0x24: case 0x3c: case 0x44: case 0x4f: case 0x56: case 0x5d: case 0x68: case 0x70: case 0x78:
		case 0x80: case 0x88:
			a = 2.356194496154785f;
			break;
		case 0x1c: case 0x25: case 0x3d: case 0x45: case 0x50: case 0x57: case 0x5e: case 0x69: case 0x71: case 0x79:
		case 0x81: case 0x89:
			a = 3.1415927410125732f;
			break;
		case 0x1d: case 0x26: case 0x3e: case 0x46: case 0x51: case 0x58: case 0x5f: case 0x6a: case 0x72: case 0x7a:
		case 0x82: case 0x8a:
			a = 3.9269907474517822f;
			break;
		case 0x1e: case 0x27: case 0x3f: case 0x47: case 0x52: case 0x59: case 0x60: case 0x6b: case 0x73: case 0x7b:
		case 0x83: case 0x8b:
			a = 4.71238899230957f;
			break;
		case 0x1f: case 0x28: case 0x40: case 0x48: case 0x53: case 0x5a: case 0x61: case 0x6c: case 0x74: case 0x7c:
		case 0x84: case 0x8c:
			a = 5.497786998748779f;
			break;
		default:
			break;
		}
	}
	return Point(0, 0, (double)a);
}

Point ShipWalk::FloorLocInsideDoor(const Point& p) const
{
	if(!valid)
		return p;
	Point q = Add(p, frame.cg);
	const double step = 2.8284271245;
	for(int i = -1; i < 2; i++)
	{
		double x = q.x;
		if(0 < i)
			x = q.x + step;
		else if(i < 0)
			x = q.x - step;
		for(int j = -1; j < 2; j++)
		{
			if(i == 0 && j == 0)
				continue;
			double y = q.y;
			if(0 < j)
				y = q.y + step;
			else if(j < 0)
				y = q.y - step;
			int t = DqPathCellTypeAt(path, bounds, Point(x, y, q.z));
			if(t == 2 || t == 8 || t == 0x4b)
				return Point(x - frame.cg.x, y - frame.cg.y, q.z - frame.cg.z);
		}
	}
	return p;
}

bool ShipWalk::TestTransportLocation(const Point& p) const
{
	unsigned t = (unsigned)DqPathCellTypeAt(path, bounds, Add(p, frame.cg));
	return !(t < 0x10 && ((0x801bu >> (t & 0x1f)) & 1));
}

Point ShipWalk::TargetLocation(const Point& helm) const
{
	Point o(0, 0, 0);
	if(!designEnv)
		o = Point(0.0 + frame.cg.x, 0.0 + frame.cg.y, 0.0 + frame.cg.z);
	if(DqPathCellTypeAt(path, bounds, o) == 0)
		return Point(helm.x + 0.0, helm.y + 0.0, helm.z + 4.0);
	return Point(0, 0, 0);
}

float ShipWalk::GroundHeightAboveCG() const
{
	double d = designEnv ? frame.min.z : frame.min.z - frame.cg.z;
	double h = -d;
	double r = 4.0;
	if(0.0 <= h && h != 0.0)
		r = h + -8.0;
	return (float)(r + 1.25);
}

}
