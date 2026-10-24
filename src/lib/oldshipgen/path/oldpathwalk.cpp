#include "oldpath.h"
#include "oldpathmathstep.h"

#include <cstdlib>
#include <cstring>

namespace OldPath
{

using namespace AuMath;

const uint32_t DirBit[27] =
{
	0x40, 0x8000, 0x1000000,
	0x20, 0x4000, 0x800000,
	0x10, 0x2000, 0x400000,
	0x80, 0x10000, 0x2000000,
	0x1, 0x200, 0x40000,
	0x8, 0x1000, 0x200000,
	0x100, 0x20000, 0x4000000,
	0x2, 0x400, 0x80000,
	0x4, 0x800, 0x100000,
};

const Direction Directions[8] =
{
	{ 1, 0, 0x400 }, { 1, 1, 0x800 }, { 0, 1, 0x1000 }, { -1, 1, 0x2000 },
	{ -1, 0, 0x4000 }, { -1, -1, 0x8000 }, { 0, -1, 0x10000 }, { 1, -1, 0x20000 },
};

namespace
{

const double HalfGrid = HalfGridUnit;
const double DoorDiag = 0.7071;
const double ClipBox = 229.1025970845;

inline int KeyDeck8(uint32_t k) { return (int8_t)(k >> 16); }

inline Point Decode(uint32_t k)
{
	return Point((double)(int)(KeyX(k) - 0x80) * GridUnit, (double)(int)(KeyY(k) - 0x80) * GridUnit, 0.0);
}

inline Point DoorVector(uint16_t flags)
{
	if((flags & 0xc0) == 0xc0)
		return Point(-DoorDiag, DoorDiag, 0.0);
	if(flags & 0x80)
		return Point(0.0, 1.0, 0.0);
	if(flags & 0x40)
		return Point(DoorDiag, DoorDiag, 0.0);
	return Point(1.0, 0.0, 0.0);
}

inline bool OpenEdgeType(uint8_t t)
{
	return t < 0x12 && ((0x30890u >> (t & 0x1f)) & 1);
}

inline double SideOf(const Point& c, const Point& v, const Point& p)
{
	double ey = c.y + v.y, ex = c.x + v.x;
	return (ey - p.y) * (c.x - p.x) - (ex - p.x) * (c.y - p.y);
}

inline int Sign3(double s)
{
	if(0.0 <= s)
		return 0.0 < s ? 1 : 0;
	return -1;
}

inline bool MayOpen(const PathableCell& c, bool crew)
{
	return (c.flags & 1) && (crew || !(c.flags & 2));
}

PointArray CellSquare(const Point& c, double z)
{
	PointArray sq;
	sq.push_back(Point(c.x - HalfGrid, c.y - HalfGrid, z));
	sq.push_back(Point(c.x - HalfGrid, c.y + HalfGrid, z));
	sq.push_back(Point(c.x + HalfGrid, c.y + HalfGrid, z));
	sq.push_back(Point(c.x + HalfGrid, c.y - HalfGrid, z));
	return sq;
}

int Sector(const Point& v)
{
	float f = (float)ZRotation(v);
	if(f < 0.0f)
		f = f + 6.2831854820251465f;
	if(f < 0.39269909262657166f || 5.890486240386963f <= f)
		return 0;
	if(f < 1.1780972480773926f)
		return 1;
	if(f < 1.9634954929351807f)
		return 2;
	if(f < 2.7488937377929688f)
		return 3;
	if(f < 3.5342917442321777f)
		return 4;
	if(f < 4.319690227508545f)
		return 5;
	return (5.105088233947754f <= f) + 6;
}

}

const ExtantCell* Path::FindExtant(uint32_t key) const
{
	std::map<uint32_t, ExtantCell>::const_iterator it = extant.find(key);
	return it == extant.end() ? 0 : &it->second;
}

const PathableCell* Path::FindPathable(uint32_t key) const
{
	std::map<uint32_t, PathableCell>::const_iterator it = pathable.find(key);
	return it == pathable.end() ? 0 : &it->second;
}

PathableCell Path::EndCell() const
{
	if(endKnown)
		return endCell;
	static const int delta[] = { 0, 0, 1, 3, 1, 5, 3, 3, 1, 9, 7, 5, 3, 9, 25, 3, 1, 21, 3, 41, 9, 25, 3, 35, 3, 3, 3,
		3, 3, 3, 3, 3 };
	int bits = 0, buckets = 0;
	for(size_t i=0; i<pathable.size(); i++)
		if((int)i >= buckets)
		{
			bits = bits + 1 < 4 ? 4 : bits + 1;
			buckets = (1 << bits) + delta[bits];
		}
	PathableCell e;
	uint32_t size = (uint32_t)pathable.size();
	e.dirs = 1;
	e.type = (uint8_t)size;
	e.door = (int16_t)(size >> 16);
	e.hole = 0x30;
	e.flags = 4;
	e.railing = buckets;
	e.room1 = 0;
	e.room2 = 0;
	e.stairDir = 1;
	return e;
}

const PathableCell& Path::PathableOrEnd(uint32_t key) const
{
	const PathableCell* c = FindPathable(key);
	if(c)
		return *c;
	static PathableCell e;
	e = EndCell();
	return e;
}

void RoomArray::FromDesign(const Design& design)
{
	rooms.clear();
	for(size_t i=0; i<design.rooms.size(); i++)
	{
		const Room& r = design.rooms[i];
		RoomOutline o;
		o.id = (uint16_t)r.id;
		o.deck = (int8_t)r.deck;
		o.type = (uint8_t)r.type;
		for(size_t j=0; j<r.points.size(); j++)
			o.points.push_back(Point((double)r.points[j].x * GridUnit, (double)r.points[j].y * GridUnit,
				(double)r.deck * DeckHeight));
		rooms.push_back(o);
	}
}

const RoomOutline* RoomArray::Find(uint16_t id) const
{
	for(size_t i=0; i<rooms.size(); i++)
		if(rooms[i].id == id)
			return &rooms[i];
	return 0;
}

namespace
{
const double WallSnap = 0.083;

bool FindRoomWall(const RoomArray& ra, const Point& a, const Point& b, uint16_t r1, uint16_t r2, Point& e1, Point& e2)
{
	const RoomOutline* room = ra.Find(r1);
	if(!room)
		return false;
	e1 = Point(0, 0, 0);
	e2 = Point(0, 0, 0);
	if(r1 == r2)
	{
		double d;
		if(!FuzzyEquals2D(a, b))
		{
			if(DoesIntersectBoundary2D(room->points, a, b, e1, e2))
				return true;
			d = ClosestBoundarySegment2D(room->points, b, e1, e2);
		}
		else
			d = ClosestBoundarySegment2D(room->points, b, e1, e2);
		if(0.0 <= d && d <= WallSnap)
		{
			Point q = PointOnSegment2D(a, e1, e2);
			if(d <= Distance2D(a, q))
				return true;
		}
		return false;
	}
	return 0.0 <= ClosestBoundarySegment2D(room->points, a, e1, e2);
}

void WallNormal(const Point& e1, const Point& e2, const Point& ref, Point& n)
{
	double rx = ref.x, ry = ref.y;
	n.z = 0.0;
	double s = (e1.x - rx) * (e2.y - ry) - (e2.x - rx) * (e1.y - ry);
	if(s < 0.0)
	{
		n.x = -(e1.y - e2.y);
		n.y = e1.x - e2.x;
	}
	else if(0.0 < s)
	{
		n.x = -(e2.y - e1.y);
		n.y = e2.x - e1.x;
	}
	else
	{
		n.x = 0.0;
		n.y = 0.0;
	}
}
}

bool RoomArray::DoesSegmentIntersectRoom(const Point& a, const Point& b, uint16_t r1, uint16_t r2, const Point& ref,
	Point& normal) const
{
	Point e1, e2;
	if(!FindRoomWall(*this, a, b, r1, r2, e1, e2))
		return false;
	if(e1.x == e2.x && e1.y == e2.y && e1.z == e2.z)
		normal = Point(0, 0, 0);
	else
		WallNormal(e1, e2, ref, normal);
	return true;
}

bool RoomArray::DoesSegmentIntersectRoom(const Point& a, const Point& b, uint16_t r1, uint16_t r2, const Point& ref,
	Point& normal, Point& hit) const
{
	Point e1, e2;
	if(!FindRoomWall(*this, a, b, r1, r2, e1, e2))
		return false;
	if(e1.x == e2.x && e1.y == e2.y && e1.z == e2.z)
	{
		normal = Point(0, 0, 0);
		hit = e1;
	}
	else
	{
		LineIntersection2D(hit, a, b, e1, e2);
		WallNormal(e1, e2, ref, normal);
	}
	return true;
}

uint16_t ExtantCell::InsideRoom(const Point& p, const RoomArray& ra) const
{
	for(int i=0; i<8; i++)
	{
		if(rooms[i] == 0)
			return 0;
		const RoomOutline* r = ra.Find(rooms[i]);
		if(r && (PointInBoundary(r->points, p) || PointOnBoundary2D(r->points, p)))
			return r->id;
	}
	return 0;
}

int ExtantCell::InsideRoomType(const Point& p, const RoomArray& ra) const
{
	for(int i=0; i<8; i++)
	{
		if(rooms[i] == 0)
			return 0;
		const RoomOutline* r = ra.Find(rooms[i]);
		if(r && (PointInBoundary(r->points, p) || PointOnBoundary2D(r->points, p)))
			return r->type;
	}
	return 0;
}

PathTest Path::WallHit(const Point& n) { return (PathTest)(PathWallHit + Sector(n)); }
PathTest Path::DoorHit(const Point& n) { return (PathTest)(PathDoorHit + Sector(n)); }
PathTest Path::RailingHit(const Point& n) { return (PathTest)(PathRailingHit + Sector(n)); }

namespace
{
PathTest SideHit(const Path& path, int base, uint32_t key, const Point& p)
{
	int x = KeyX(key), y = KeyY(key), deck = KeyDeck8(key);
	int s = Sector(Sub(p, Decode(key)));
	static const int ab[8][4] =
	{
		{ 1, -1, 1, 1 },
		{ 1, 0, 0, 1 },
		{ 1, 1, -1, 1 },
		{ 0, 1, -1, 0 },
		{ -1, 1, -1, -1 },
		{ -1, 0, 0, -1 },
		{ -1, -1, 1, -1 },
		{ 0, -1, 1, 0 },
	};
	bool aThere = path.FindExtant(MakeKey(x + ab[s][0], y + ab[s][1], deck)) != 0;
	bool bMissing = path.FindExtant(MakeKey(x + ab[s][2], y + ab[s][3], deck)) == 0;
	int r = aThere ? s + bMissing : s - 1 + bMissing;
	if(r < 0)
		r = 7;
	if(r > 7)
		r = 0;
	return (PathTest)(base + r);
}
}

PathTest Path::WallHit(uint32_t key, const Point& p) const { return SideHit(*this, PathWallHit, key, p); }
PathTest Path::DoorHit(uint32_t key, const Point& p) const { return SideHit(*this, PathDoorHit, key, p); }
PathTest Path::RailingHit(uint32_t key, const Point& p) const { return SideHit(*this, PathRailingHit, key, p); }

PathTest Path::ObstacleHit(uint32_t key, const Point& p)
{
	Point d = Sub(p, Decode(key));
	if(std::fabs(d.y) < std::fabs(d.x))
		return 0.0 < d.x ? (PathTest)0x14 : (PathTest)0x18;
	return 0.0 < d.y ? (PathTest)0x16 : (PathTest)0x1a;
}

PathTest Path::FloorHit(const Point& a, const Point& b, int8_t deck, Point& hit)
{
	Point d(b.x - a.x, b.y - a.y, b.z - a.z);
	Point s = Mul(d, ((double)(int)deck * DeckHeight - a.z) / d.z);
	double sy = s.y, sz = s.z;
	hit.x = a.x + s.x;
	hit.y = sy + a.y;
	hit.z = sz + a.z;
	return PathFloorHit;
}

PathTest Path::CeilingHit(const Point& a, const Point& b, int8_t deck, Point& hit)
{
	FloorHit(a, b, deck, hit);
	return PathCeilingHit;
}

PathTest Path::WallHit(uint32_t key, const Point& a, const Point& b, Point& hit) const
{
	int8_t d = (int8_t)OnDeck(a.z);
	int8_t kd = (int8_t)KeyDeck8(key);
	if(d < kd)
		return CeilingHit(a, b, kd, hit);
	if(kd < d)
		return FloorHit(a, b, d, hit);
	Point c = Decode(key);
	int x = KeyX(key), y = KeyY(key);
	for(int i=0; i<8; i++)
	{
		int dx = Directions[i].dx, dy = Directions[i].dy;
		const ExtantCell* e = FindExtant(MakeKey(x + dx, y + dy, kd));
		if(!e || e->type != 1)
			continue;
		Point w = c;
		if(dx != 0)
			w.x = (double)dx * HalfGrid + c.x;
		if(dy != 0)
			w.y = (double)dy * HalfGrid + c.y;
		if(LineIntersection2D(hit, a, b, c, w) && Between(hit, a, b, 1e-05) && Between2D(hit, c, w))
		{
			double ny = w.y - c.y;
			double nx = w.x - c.x;
			Point n(-ny, nx, 0.0);
			if(0.0 < (b.x - a.x) * n.x + (b.y - a.y) * n.y)
				n = Point(ny, -nx, -0.0);
			return WallHit(n);
		}
	}
	return ObstacleHit(key, a, b, hit);
}

namespace
{
Point LineHit(const Path& path, uint32_t key, const Point& a, const Point& b, Point& hit)
{
	Point c = Decode(key);
	const PathableCell& cell = path.PathableOrEnd(key);
	Point v = DoorVector(cell.flags);
	Point e(c.x + v.x, c.y + v.y, c.z + 0.0);
	LineIntersection2D(hit, a, b, c, e);
	double ny = e.y - c.y;
	double nx = e.x - c.x;
	Point n(-ny, nx, 0.0);
	if(0.0 < (b.x - a.x) * n.x + (b.y - a.y) * n.y)
		n = Point(ny, -nx, -0.0);
	return n;
}
}

PathTest Path::DoorHit(uint32_t key, const Point& a, const Point& b, Point& hit) const
{
	return DoorHit(LineHit(*this, key, a, b, hit));
}

PathTest Path::RailingHit(uint32_t key, const Point& a, const Point& b, Point& hit) const
{
	return RailingHit(LineHit(*this, key, a, b, hit));
}

PathTest Path::ObstacleHit(uint32_t key, const Point& a, const Point& b, Point& hit)
{
	int8_t d = (int8_t)OnDeck(a.z);
	int8_t kd = (int8_t)KeyDeck8(key);
	if(d < kd)
		return CeilingHit(a, b, kd, hit);
	if(kd < d)
		return FloorHit(a, b, d, hit);
	Point c = Decode(key);
	PointArray sq = CellSquare(c, c.z + 0.0);
	Point r(0, 0, 0);
	if(!IntersectBoundary2D(sq, a, b, r))
	{
		hit = a;
		return ObstacleHit(key, a);
	}
	double len = Distance2D(a, b);
	double part = Distance2D(a, r);
	Point s = Mul(Point(b.x - a.x, b.y - a.y, b.z - a.z), part / len);
	double sy = s.y, sz = s.z;
	hit.x = s.x + a.x;
	hit.y = sy + a.y;
	hit.z = sz + a.z;
	double dy = a.y - b.y;
	double dx = a.x - b.x;
	if(std::fabs(dx) < std::fabs(dy) || std::fabs(dx) == std::fabs(dy))
		return dy <= 0.0 ? (PathTest)0x1a : (PathTest)0x16;
	return 0.0 < dx ? (PathTest)0x14 : (PathTest)0x18;
}

PathTest Path::PathBlocked(const Point& p, int8_t deckFrom, int8_t deckTo, uint32_t key, int kind) const
{
	if(deckFrom < deckTo)
		return PathCeilingHit;
	if(deckTo < deckFrom)
		return PathFloorHit;
	if(kind == 1)
		return DoorHit(key, p);
	if(kind == 2)
		return RailingHit(key, p);
	if(kind == 3)
		return WallHit(key, p);
	return ObstacleHit(key, p);
}

namespace
{
PathTest WallCellPath(const Path& path, const Point& a, bool aInCell, const Point& b, bool bInCell, uint32_t key,
	int8_t deck, const RoomArray& rooms, Point* hit)
{
	Point c = Decode(key);
	PointArray sq = CellSquare(c, 0.0);
	PointArray cross;
	Point p0, p1;
	if(!IntersectBoundary2D(sq, a, b, cross))
	{
		p0 = a;
		p1 = b;
	}
	else if(cross.size() > 1)
	{
		p0 = cross.front();
		p1 = cross.back();
	}
	else if(aInCell)
	{
		p0 = a;
		p1 = cross.front();
	}
	else if(bInCell)
	{
		p0 = cross.front();
		p1 = b;
	}
	else
		return PathOk;
	uint16_t r1 = 0, r2 = 0;
	path.InsideRoom(p0, deck, rooms, r1);
	if(r1 == 0)
	{
		if(hit)
			*hit = a;
		return PathRefused;
	}
	path.InsideRoom(p1, deck, rooms, r2);
	Point n(0, 0, 0);
	bool crossed = hit ? rooms.DoesSegmentIntersectRoom(p0, p1, r1, r2, a, n, *hit) :
		rooms.DoesSegmentIntersectRoom(p0, p1, r1, r2, a, n);
	if(crossed)
		return Path::WallHit(n);
	return PathOk;
}
}

PathTest Path::TestWallCellPath(const Point& a, bool aInCell, const Point& b, bool bInCell, uint32_t key, int8_t deck,
	const RoomArray& rooms) const
{
	return WallCellPath(*this, a, aInCell, b, bInCell, key, deck, rooms, 0);
}

PathTest Path::TestWallCellPath(const Point& a, bool aInCell, const Point& b, bool bInCell, uint32_t key, int8_t deck,
	const RoomArray& rooms, Point& hit) const
{
	return WallCellPath(*this, a, aInCell, b, bInCell, key, deck, rooms, &hit);
}

PathTest Path::TestClippedCellPath(int x, int y, int deck, const Point& a, const Point& b, const RoomArray& rooms) const
{
	uint32_t k = MakeKey(x, y, (int8_t)deck);
	const ExtantCell* e = FindExtant(k);
	if(e && e->type == 1)
		return TestWallCellPath(a, false, b, false, k, (int8_t)deck, rooms);
	return ObstacleHit(k, a);
}

PathTest Path::TestClippedCellPath(int x, int y, int deck, const Point& a, const Point& b, const RoomArray& rooms,
	Point& hit) const
{
	uint32_t k = MakeKey(x, y, (int8_t)deck);
	const ExtantCell* e = FindExtant(k);
	if(!e)
		return ObstacleHit(k, a);
	if(e->type == 1)
		return TestWallCellPath(a, false, b, false, k, (int8_t)deck, rooms, hit);
	return ObstacleHit(k, a, b, hit);
}

namespace
{
PathTest CornerClip(const Path& path, const Point& a, const Point& b, int x, int y, int8_t deck, uint32_t dirs,
	uint32_t bit, const RoomArray& rooms, Point* hit)
{
	if((bit & 0x2a800) == 0)
		return PathOk;
	double cx, cy;
	int tx = x, ty = y;
	Point c = Decode(MakeKey(x, y, 0));
	if(bit & 0x800)
	{
		if((dirs & 0x1400) == 0x1400)
			return PathOk;
		cx = c.x + HalfGrid;
		cy = c.y + HalfGrid;
		if(0.0 <= (b.y - cy) * (a.x - cx) - (a.y - cy) * (b.x - cx))
		{
			if(dirs & 0x400)
				return PathOk;
			tx = x + 1;
		}
		else
		{
			if(dirs & 0x1000)
				return PathOk;
			ty = y + 1;
		}
	}
	else if(bit & 0x2000)
	{
		if((dirs & 0x5000) == 0x5000)
			return PathOk;
		cx = c.x + -HalfGrid;
		cy = c.y + HalfGrid;
		if((b.y - cy) * (a.x - cx) - (a.y - cy) * (b.x - cx) < 0.0)
		{
			if(dirs & 0x4000)
				return PathOk;
			tx = x - 1;
		}
		else
		{
			if(dirs & 0x1000)
				return PathOk;
			ty = y + 1;
		}
	}
	else if(bit & 0x8000)
	{
		if((dirs & 0x14000) == 0x14000)
			return PathOk;
		cx = c.x + -HalfGrid;
		cy = c.y + -HalfGrid;
		if(0.0 <= (b.y - cy) * (a.x - cx) - (a.y - cy) * (b.x - cx))
		{
			if(dirs & 0x4000)
				return PathOk;
			tx = x - 1;
		}
		else
		{
			if(dirs & 0x10000)
				return PathOk;
			ty = y - 1;
		}
	}
	else
	{
		if((dirs & 0x10400) == 0x10400)
			return PathOk;
		cx = c.x + HalfGrid;
		cy = c.y + -HalfGrid;
		if((b.y - cy) * (a.x - cx) - (a.y - cy) * (b.x - cx) < 0.0)
		{
			if(dirs & 0x400)
				return PathOk;
			tx = x + 1;
		}
		else
		{
			if(dirs & 0x10000)
				return PathOk;
			ty = y - 1;
		}
	}
	return hit ? path.TestClippedCellPath(tx, ty, deck, a, b, rooms, *hit) :
		path.TestClippedCellPath(tx, ty, deck, a, b, rooms);
}
}

PathTest Path::TestDiagonalCornerClip(const Point& a, const Point& b, int x, int y, int8_t deck, uint32_t dirs,
	uint32_t bit, const RoomArray& rooms) const
{
	return CornerClip(*this, a, b, x, y, deck, dirs, bit, rooms, 0);
}

PathTest Path::TestDiagonalCornerClip(const Point& a, const Point& b, int x, int y, int8_t deck, uint32_t dirs,
	uint32_t bit, const RoomArray& rooms, Point& hit) const
{
	return CornerClip(*this, a, b, x, y, deck, dirs, bit, rooms, &hit);
}

bool Path::TestStepOk(uint16_t p1, uint16_t p2, int8_t p3)
{
	uint16_t m;
	if(p3 == 0)
	{
		if((p1 & 0xff00) == 0)
		{
			if(p2 & 0x1800)
				return true;
			if((p2 & 0xff00) == 0)
				return true;
		}
		if((int16_t)p1 < 0)
		{
			if(p2 & 0xc000)
				return true;
		}
		else if((p1 & 0x4000) == 0)
		{
			if((p1 & 0x2000) == 0)
			{
				if(p1 & 0x1000)
				{
					if(p2 & 0x3800)
						return true;
					if((p2 & 0xff00) == 0)
						return true;
				}
			}
			else if(p2 & 0x7000)
				return true;
		}
		else if(p2 & 0xe000)
			return true;
		if((p1 & 0x800) == 0)
		{
			if((p1 & 0x400) == 0)
			{
				if((p1 & 0x200) == 0)
				{
					m = 0x100;
					goto low;
				}
				goto mid2;
			}
			goto mid1;
		}
	}
	else
	{
		if(p3 < 1)
		{
			if((p1 & 0xff00) == 0)
			{
				if((int16_t)p2 < 0)
					return true;
				if((p2 & 0xff00) == 0)
					return true;
			}
			if(p1 & 0x1000)
			{
				if((int16_t)p2 < 0)
					return true;
				if((p2 & 0xff00) == 0)
					return true;
			}
			if(p1 & 0x800)
				return (p2 & 0xc000) != 0;
			if(p1 & 0x400)
				return (p2 & 0xe000) != 0;
			if(p1 & 0x200)
				return (p2 & 0x7000) != 0;
			if((p1 & 0x100) == 0)
				return false;
			if((p2 & 0x3800) == 0)
				return (p2 & 0xff00) == 0;
			return true;
		}
		m = 0x1000;
		if((p1 & 0xff00) == 0)
		{
			if(p2 & 0x1000)
				return true;
			if((p2 & 0xff00) == 0)
				return true;
		}
		if((int16_t)p1 >= 0)
		{
			if((p1 & 0x4000) == 0)
			{
				if((p1 & 0x2000) == 0)
					goto low;
				goto mid2;
			}
			goto mid1;
		}
	}
	if(p2 & 0x1c00)
		return true;
	return (p2 & 0xff00) == 0;
mid1:
	return (p2 & 0xe00) != 0;
mid2:
	return (p2 & 0x700) != 0;
low:
	if((m & p1) == 0)
		return false;
	return (p2 & 0x300) != 0;
}

int Path::CellTypeAt(const Point& p, int8_t deck) const
{
	int x, y;
	HalfGridCoord(p, x, y);
	const ExtantCell* e = FindExtant(MakeKey(x, y, deck));
	return e ? e->type : 0;
}

bool Path::IsPathable(const Point& p) const
{
	return FindPathable(MakeKey(p)) != 0;
}

namespace
{
int8_t DeckBelowImpl(const Path& path, const Point& p, int8_t deck, bool& gravity, const RoomArray* rooms)
{
	int x, y;
	HalfGridCoord(p, x, y);
	const PathableCell* cell = path.FindPathable(MakeKey(x, y, deck));
	if(!cell)
	{
		const ExtantCell* e = path.FindExtant(MakeKey(x, y, deck));
		if(!e)
		{
			gravity = false;
			return -0x2b;
		}
		if(rooms && e->type == 1)
			gravity = e->InsideRoomType(p, *rooms) == 0xb;
		return deck;
	}
	gravity = (cell->flags >> 2) & 1;
	PathableCell c = *cell;
	int guard = 0;
	while((c.dirs & 1) && (c.flags & 0x14))
	{
		deck = (int8_t)(deck - 1);
		c = path.PathableOrEnd(MakeKey(x, y, deck));
		if(++guard > 0x200)
			break;
	}
	if(c.type == 0x11)
	{
		int8_t below = (int8_t)(deck - 1);
		const ExtantCell* e = path.FindExtant(MakeKey(x, y, below));
		if(!e)
			return below;
		uint8_t t = e->type;
		if(t == 0)
			return below;
		if(t < 0x12 && ((0x32080u >> (t & 0x1f)) & 1))
			return below;
	}
	return deck;
}
}

int8_t Path::DeckBelow(const Point& p, int8_t deck, bool& gravity) const
{
	return DeckBelowImpl(*this, p, deck, gravity, 0);
}

int8_t Path::DeckBelow(const Point& p, int8_t deck, bool& gravity, const RoomArray& rooms) const
{
	return DeckBelowImpl(*this, p, deck, gravity, &rooms);
}

uint16_t Path::InsideRoom(const Point& p, const PathableCell& cell, const RoomArray& rooms) const
{
	if((cell.door < 0 && cell.hole < 0 && cell.railing < 0) || (cell.flags & 0x14))
		return cell.room1;
	if(cell.flags & 8)
		return cell.type == 7 ? 0 : cell.room1;
	if(cell.room2 == 0)
	{
		const RoomOutline* r = rooms.Find(cell.room1);
		if(!r)
			return cell.room2;
		return PointInBoundary(r->points, p) ? cell.room1 : cell.room2;
	}
	const RoomOutline* r1 = rooms.Find(cell.room1);
	const RoomOutline* r2 = rooms.Find(cell.room2);
	if(!r1 || !r2)
		return 0;
	if(r1->points.size() < r2->points.size())
		return PointInBoundary(r1->points, p) ? cell.room1 : cell.room2;
	return PointInBoundary(r2->points, p) ? cell.room2 : cell.room1;
}

bool Path::InsideRoom(const Point& p, int8_t deck, const RoomArray& rooms, uint16_t& room) const
{
	room = 0;
	int x, y;
	HalfGridCoord(p, x, y);
	uint32_t k = MakeKey(x, y, deck);
	const PathableCell* cell = FindPathable(k);
	if(!cell)
	{
		const ExtantCell* e = FindExtant(k);
		if(e)
			room = e->InsideRoom(p, rooms);
		return false;
	}
	room = InsideRoom(p, *cell, rooms);
	return true;
}

int Path::HatchAbove(const Point& p) const
{
	int x, y;
	HalfGridCoord(p, x, y);
	const PathableCell* c = FindPathable(MakeKey(x, y, (int8_t)(OnDeck(p.z) + 1)));
	if(c && (c->flags & 8))
		return c->door;
	return -1;
}

int Path::HatchDoorIndex(const Point& p) const
{
	const PathableCell* c = FindPathable(MakeKey(p));
	if(c && (c->flags & 8))
		return c->door;
	return -1;
}

int Path::HatchDoorIndex(const Point& p, bool& crew) const
{
	const PathableCell* c = FindPathable(MakeKey(p));
	if(c && (c->flags & 8))
	{
		crew = (c->flags >> 1) & 1;
		return c->door;
	}
	return -1;
}

uint32_t Path::FindBottomStep(uint32_t key) const
{
	const PathableCell* cell = FindPathable(key);
	if(!cell)
		return key;
	uint32_t ck = key;
	bool seen = false;
	int deck = KeyDeck8(key);
	if(cell->flags & 0x1000)
		return ck;
	for(int guard = 0; ; guard++)
	{
		if(guard == 0x20000)
			return 0xffffffffu;
		const Direction& d = Directions[cell->stairDir & 7];
		if((cell->dirs & d.bit) == 0)
			break;
		uint32_t nk = MakeKey(KeyX(ck) + d.dx, KeyY(ck) + d.dy, deck);
		const PathableCell* next = FindPathable(nk);
		if(!next)
			break;
		if(next->flags & 0x2000)
		{
			if(seen)
				break;
			seen = true;
		}
		cell = next;
		ck = nk;
		if(next->flags & 0x1000)
			break;
	}
	return ck;
}

bool BoxClipAllowed(Point& a, Point& b)
{
	const double mx = ClipBox, mn = -ClipBox;
	if(a.x < mn && b.x < mn)
		return false;
	if((mx < a.x && mx < b.x) || (a.y < mn && b.y < mn) || (mx < a.y && mx < b.y) || (a.z < mn && b.z < mn) ||
		(mx < a.z && mx < b.z))
		return false;
	Point d = Sub(b, a);
	double tx = std::fabs(a.x) <= mx ? 0.0 : (std::fabs(a.x) - mx) / std::fabs(d.x);
	double ty = std::fabs(a.y) <= mx ? 0.0 : (std::fabs(a.y) - mx) / std::fabs(d.y);
	double tz = std::fabs(a.z) <= mx ? 0.0 : (std::fabs(a.z) - mx) / std::fabs(d.z);
	if(0.0 < tx || 0.0 < ty || 0.0 < tz)
	{
		double t = tx;
		if(tx <= ty || tx <= tz)
		{
			t = ty;
			if(ty <= tz)
				t = tz;
		}
		Point s = Mul(d, t);
		a.x = s.x + a.x;
		a.y = s.y + a.y;
		a.z = s.z + a.z;
	}
	Point e = Sub(a, b);
	tx = std::fabs(b.x) <= mx ? 0.0 : (std::fabs(b.x) - mx) / std::fabs(e.x);
	ty = std::fabs(b.y) <= mx ? 0.0 : (std::fabs(b.y) - mx) / std::fabs(e.y);
	tz = std::fabs(b.z) <= mx ? 0.0 : (std::fabs(b.z) - mx) / std::fabs(e.z);
	if(0.0 < tx || 0.0 < ty || 0.0 < tz)
	{
		double t = tx;
		if(tx <= ty || tx <= tz)
		{
			t = ty;
			if(ty <= tz)
				t = tz;
		}
		Point s = Mul(e, t);
		b.x = s.x + b.x;
		b.y = s.y + b.y;
		b.z = s.z + b.z;
	}
	Point f = Sub(b, a);
	return 0.0 < f.y * d.y + f.x * d.x + f.z * d.z;
}

namespace
{
struct StepSetup
{
	int ax, ay, bx, by;
	int8_t adeck;
	int dx, dy, dz;
	int n, offx, offy;
	uint32_t key0;
};

void Setup(const Point& a, const Point& b, StepSetup& s)
{
	HalfGridCoord(a, s.ax, s.ay);
	s.adeck = (int8_t)OnDeck(a.z);
	s.key0 = MakeKey(s.ax, s.ay, s.adeck);
	HalfGridCoord(b, s.bx, s.by);
	int8_t bdeck = (int8_t)OnDeck(b.z);
	s.dx = s.bx - s.ax;
	s.dy = s.by - s.ay;
	s.dz = (int)bdeck - (int)s.adeck;
	s.offx = 0;
	s.offy = 0;
	int ady = std::abs(s.dy), adx = std::abs(s.dx), adz = std::abs(s.dz);
	if(ady < adx && adz < adx)
	{
		s.n = adx;
		if(s.dy != 0)
			s.offy = (adx / ady) / 2;
	}
	else
	{
		s.n = adz;
		if(s.n < ady)
		{
			s.n = ady;
			if(s.dx != 0)
				s.offx = (ady / adx) / 2;
		}
	}
}

struct LineSides
{
	double s1, s2;
	Point c, e;
};

LineSides Sides(uint32_t cellKey, const PathableCell& cell, const Point& a, const Point& b)
{
	LineSides r;
	r.c = Decode(cellKey);
	Point v = DoorVector(cell.flags);
	r.e = Point(r.c.x + v.x, r.c.y + v.y, 0.0);
	r.s1 = SideOf(r.c, v, a);
	r.s2 = SideOf(r.c, v, b);
	return r;
}

bool MovesAway(const LineSides& l, const Point& a, const Point& b)
{
	double d1 = DistToLine(a, Point(l.c.x, l.c.y, a.z), Point(l.e.x, l.e.y, a.z));
	double d2 = DistToLine(b, Point(l.c.x, l.c.y, b.z), Point(l.e.x, l.e.y, b.z));
	return d2 < d1;
}

void HatchAboveToOpen(const Path& path, const PathableCell& cell, const StepSetup& s, const DoorStates& doors,
	bool crew, std::vector<uint16_t>& list)
{
	if(!(cell.dirs & 0x40000))
		return;
	const PathableCell& up = path.PathableOrEnd(MakeKey(s.ax, s.ay, (int8_t)(s.adeck + 1)));
	if((up.flags & 8) && !doors.IsDoorOpen((unsigned)(int)up.door) && MayOpen(up, crew))
		list.push_back((uint16_t)up.door);
}

inline bool DoorOpen(const DoorStates& doors, const PathableCell& c)
{
	return doors.IsDoorOpen((unsigned)(int)c.door);
}
}

PathTest Path::TestSpacecraftPath(const Point& a0, const Point& b0, bool crew, const RoomArray& rooms,
	const DoorStates& doors, std::vector<uint16_t>& list, bool noRail, bool& onStairs) const
{
	onStairs = false;
	Point a = a0, b = b0;
	if(!BoxClipAllowed(a, b))
		return PathOk;
	StepSetup s;
	Setup(a, b, s);
	const PathableCell* cur = FindPathable(s.key0);

	if(s.n < 1)
	{
		if(!cur)
		{
			const ExtantCell* e = FindExtant(s.key0);
			if(e)
			{
				if(e->type != 1)
					return ObstacleHit(s.key0, a);
				uint16_t r1 = e->InsideRoom(a, rooms);
				if(r1 == 0)
					return PathRefused;
				uint16_t r2 = e->InsideRoom(b, rooms);
				Point n(0, 0, 0);
				if(rooms.DoesSegmentIntersectRoom(a, b, r1, r2, a, n))
					return WallHit(n);
			}
			return PathOk;
		}
		if(cur->flags & 0xff00)
			onStairs = true;
		if(!(cur->flags & 0x20))
		{
			if(cur->type == 0x12 && !noRail)
			{
				LineSides l = Sides(s.key0, *cur, a, b);
				if(l.s1 == 0.0 || l.s2 == 0.0)
					return PathRefused;
				if(Sign3(l.s1) != Sign3(l.s2) || MovesAway(l, a, b))
					return RailingHit(s.key0, a);
			}
		}
		else if(!DoorOpen(doors, *cur))
		{
			if(!(cur->flags & 8))
			{
				if(MayOpen(*cur, crew))
					list.push_back((uint16_t)cur->door);
				else
				{
					LineSides l = Sides(s.key0, *cur, a, b);
					if(l.s1 == 0.0 || l.s2 == 0.0)
						return PathRefused;
					if(Sign3(l.s1) != Sign3(l.s2) || MovesAway(l, a, b))
						return DoorHit(s.key0, a);
				}
			}
			else if(MayOpen(*cur, crew))
				list.push_back((uint16_t)cur->door);
		}
		HatchAboveToOpen(*this, *cur, s, doors, crew, list);
		return PathOk;
	}

	int prevX = s.ax, prevY = s.ay;
	int8_t prevDeck = s.adeck;
	uint32_t curKey = s.key0;
	for(int i=1; i<=s.n; i++)
	{
		int x = s.ax + ((s.offx + i) * s.dx) / s.n;
		int y = ((s.offy + i) * s.dy) / s.n + s.ay;
		if(x < 0 || y < 0 || 0xff < x || 0xff < y)
			break;
		int8_t deck = (int8_t)((int8_t)((i * s.dz) / s.n) + s.adeck);
		if(0x2b < std::abs((int)deck))
			break;
		uint32_t key = MakeKey(x, y, deck);
		if(!cur)
		{
			const ExtantCell* e1 = FindExtant(curKey);
			if(e1)
			{
				if(e1->type != 1)
					return PathBlocked(a, prevDeck, deck, key, 0);
				PathTest r = TestWallCellPath(a, true, b, false, curKey, prevDeck, rooms);
				if(r != PathOk)
					return r;
			}
			const PathableCell* nc = FindPathable(key);
			if(!nc)
			{
				const ExtantCell* e2 = FindExtant(key);
				if(e2)
				{
					if(e2->type != 1)
						return PathBlocked(a, prevDeck, deck, key, 0);
					if(!e1)
						return PathBlocked(a, prevDeck, deck, key, 3);
					PathTest r = TestWallCellPath(a, false, b, true, key, deck, rooms);
					if(r != PathOk)
						return r;
				}
			}
			else if(!e1)
			{
				if(!OpenEdgeType(nc->type))
					return PathBlocked(a, prevDeck, deck, key, 3);
				if((nc->flags & 0x20) && nc->type != 7 && !DoorOpen(doors, *nc))
					return PathBlocked(a, prevDeck, deck, key, 1);
			}
			cur = nc;
		}
		else
		{
			if(cur->flags & 0xff00)
				onStairs = true;
			uint32_t bit = DirBit[(deck - prevDeck) + 1 + ((y - prevY + 1) + (x - prevX + 1) * 3) * 3];
			if((bit & cur->dirs) == 0)
			{
				if(OpenEdgeType(cur->type))
				{
					if(deck == prevDeck && i == 1)
					{
						if(!(cur->flags & 0x20))
						{
							if(cur->type == 0x12 && !noRail)
							{
								LineSides l = Sides(curKey, *cur, a, b);
								int s1 = Sign3(l.s1), s2 = Sign3(l.s2);
								if(s1 == 0 || s2 == 0)
									return PathRefused;
								if(s2 != s1)
									return RailingHit(curKey, a);
							}
						}
						else if(!DoorOpen(doors, *cur) && !(cur->flags & 8))
						{
							LineSides l = Sides(curKey, *cur, a, b);
							int s1 = Sign3(l.s1), s2 = Sign3(l.s2);
							if(s2 != s1 || s1 == 0 || s2 == 0)
							{
								if(!MayOpen(*cur, crew))
								{
									if(s1 == 0 || s2 == 0)
										return PathRefused;
									return DoorHit(curKey, a);
								}
								list.push_back((uint16_t)cur->door);
							}
						}
					}
					if(FindExtant(key))
						return PathBlocked(a, prevDeck, deck, key, 3);
					cur = 0;
				}
				else
				{
					const ExtantCell* e = FindExtant(key);
					if(!e || e->type != 1)
						return PathBlocked(a, prevDeck, deck, key, 0);
					PathTest r = TestDiagonalCornerClip(a, b, prevX, prevY, prevDeck, cur->dirs, bit, rooms);
					if(r != PathOk)
						return r;
					r = TestWallCellPath(a, false, b, true, key, deck, rooms);
					if(r != PathOk)
						return r;
					cur = 0;
				}
			}
			else
			{
				const PathableCell& nc = PathableOrEnd(key);
				const PathableCell* ncp = FindPathable(key);
				if(prevDeck < deck)
				{
					if((nc.flags & 0x20) && !DoorOpen(doors, nc))
					{
						if(!MayOpen(nc, crew))
							return PathCeilingHit;
						list.push_back((uint16_t)nc.door);
					}
				}
				else if(deck < prevDeck)
				{
					if((cur->flags & 8) && !DoorOpen(doors, *cur))
					{
						if((cur->room2 == 0 && (cur->flags & 8)) || (!crew && (cur->flags & 2)))
							return PathFloorHit;
						list.push_back((uint16_t)cur->door);
					}
					if((nc.flags & 0x20) && !DoorOpen(doors, nc) && !(nc.flags & 8))
					{
						if(!MayOpen(nc, crew))
							return PathFloorHit;
						list.push_back((uint16_t)nc.door);
					}
				}
				else
				{
					if(i == 1)
					{
						if(!(cur->flags & 0x20))
						{
							if(cur->type == 0x12 && !noRail)
							{
								LineSides l = Sides(curKey, *cur, a, b);
								int s1 = Sign3(l.s1), s2 = Sign3(l.s2);
								if(s1 == 0 || s2 == 0)
									return PathRefused;
								if(s2 != s1)
									return RailingHit(curKey, a);
							}
						}
						else if(!DoorOpen(doors, *cur) && !(cur->flags & 8))
						{
							LineSides l = Sides(curKey, *cur, a, b);
							int s1 = Sign3(l.s1), s2 = Sign3(l.s2);
							if(s2 != s1 || s1 == 0 || s2 == 0)
							{
								if(!MayOpen(*cur, crew))
								{
									if(s1 != 0 && s2 != 0)
										return DoorHit(curKey, a);
									return PathRefused;
								}
								list.push_back((uint16_t)cur->door);
							}
						}
					}
					PathTest r = TestDiagonalCornerClip(a, b, prevX, prevY, prevDeck, cur->dirs, bit, rooms);
					if(r != PathOk)
						return r;
					if(!(nc.flags & 0x20))
					{
						if(nc.type == 0x12 && !noRail)
							return RailingHit(key, a);
					}
					else if(!DoorOpen(doors, nc) && !(nc.flags & 8))
					{
						if(!MayOpen(nc, crew))
							return DoorHit(key, a);
						list.push_back((uint16_t)nc.door);
					}
				}
				cur = ncp;
			}
		}
		prevDeck = deck;
		prevX = x;
		prevY = y;
		curKey = key;
	}
	return PathOk;
}

PathTest Path::TestSpacecraftPath(const Point& a0, const Point& b0, bool crew, const RoomArray& rooms,
	const DoorStates& doors, std::vector<uint16_t>& list, bool noRail, bool& onStairs, Point& hit) const
{
	onStairs = false;
	Point a = a0, b = b0;
	if(!BoxClipAllowed(a, b))
		return PathOk;
	StepSetup s;
	Setup(a, b, s);
	const PathableCell* cur = FindPathable(s.key0);

	if(s.n < 1)
	{
		if(!cur)
		{
			const ExtantCell* e = FindExtant(s.key0);
			if(!e)
				return PathOk;
			if(e->type != 1)
			{
				hit = a;
				return ObstacleHit(s.key0, a);
			}
			uint16_t r1 = e->InsideRoom(a, rooms);
			if(r1 == 0)
			{
				hit = a;
				return PathRefused;
			}
			uint16_t r2 = e->InsideRoom(b, rooms);
			Point n(0, 0, 0);
			if(!rooms.DoesSegmentIntersectRoom(a, b, r1, r2, a, n, hit))
				return PathOk;
			return WallHit(n);
		}
		if(cur->flags & 0xff00)
			onStairs = true;
		if(!(cur->flags & 0x20))
		{
			if(cur->type == 0x12 && !noRail)
			{
				LineSides l = Sides(s.key0, *cur, a, b);
				if(l.s1 == 0.0)
				{
					hit = a;
					return PathRefused;
				}
				if(l.s2 == 0.0)
				{
					hit = b;
					return PathRefused;
				}
				if(Sign3(l.s1) != Sign3(l.s2) || MovesAway(l, a, b))
					return RailingHit(s.key0, a, b, hit);
			}
		}
		else if(!DoorOpen(doors, *cur))
		{
			if(!(cur->flags & 8))
			{
				if(!MayOpen(*cur, crew))
				{
					LineSides l = Sides(s.key0, *cur, a, b);
					if(l.s1 == 0.0)
					{
						hit = a;
						return PathRefused;
					}
					if(l.s2 == 0.0)
					{
						hit = b;
						return PathRefused;
					}
					if(Sign3(l.s1) != Sign3(l.s2) || MovesAway(l, a, b))
						return DoorHit(s.key0, a, b, hit);
				}
				else
					list.push_back((uint16_t)cur->door);
			}
			else if(MayOpen(*cur, crew))
				list.push_back((uint16_t)cur->door);
		}
		HatchAboveToOpen(*this, *cur, s, doors, crew, list);
		return PathOk;
	}

	int prevX = s.ax, prevY = s.ay;
	int8_t prevDeck = s.adeck;
	uint32_t curKey = s.key0;
	for(int i=1; i<=s.n; i++)
	{
		int x = s.ax + ((s.offx + i) * s.dx) / s.n;
		int y = ((s.offy + i) * s.dy) / s.n + s.ay;
		if(x < 0 || y < 0 || 0xff < x || 0xff < y)
			return PathOk;
		int8_t deck = (int8_t)((int8_t)((i * s.dz) / s.n) + s.adeck);
		if(0x2b < std::abs((int)deck))
			return PathOk;
		uint32_t key = MakeKey(x, y, deck);
		if(!cur)
		{
			const ExtantCell* e1 = FindExtant(curKey);
			if(e1)
			{
				if(e1->type != 1)
					return ObstacleHit(key, a, b, hit);
				PathTest r = TestWallCellPath(a, true, b, false, curKey, prevDeck, rooms, hit);
				if(r != PathOk)
					return r;
			}
			const PathableCell* nc = FindPathable(key);
			if(!nc)
			{
				const ExtantCell* e2 = FindExtant(key);
				if(e2)
				{
					if(e2->type != 1)
						return ObstacleHit(key, a, b, hit);
					if(!e1)
						return WallHit(key, a, b, hit);
					PathTest r = TestWallCellPath(a, false, b, true, key, deck, rooms, hit);
					if(r != PathOk)
						return r;
				}
			}
			else if(!e1)
			{
				if(!OpenEdgeType(nc->type))
					return WallHit(key, a, b, hit);
				if((nc->flags & 0x20) && nc->type != 7 && !DoorOpen(doors, *nc))
					return DoorHit(key, a, b, hit);
			}
			cur = nc;
		}
		else
		{
			if(cur->flags & 0xff00)
				onStairs = true;
			uint32_t bit = DirBit[(deck - prevDeck) + 1 + ((y - prevY + 1) + (x - prevX + 1) * 3) * 3];
			if((bit & cur->dirs) == 0)
			{
				if(!OpenEdgeType(cur->type))
				{
					const ExtantCell* e = FindExtant(key);
					if(!e || e->type != 1)
						return ObstacleHit(key, a, b, hit);
					PathTest r = TestDiagonalCornerClip(a, b, prevX, prevY, prevDeck, cur->dirs, bit, rooms, hit);
					if(r != PathOk)
						return r;
					r = TestWallCellPath(a, false, b, true, key, deck, rooms, hit);
					if(r != PathOk)
						return r;
					cur = 0;
				}
				else
				{
					if(deck == prevDeck && i == 1)
					{
						if(!(cur->flags & 0x20))
						{
							if(cur->type == 0x12 && !noRail)
							{
								LineSides l = Sides(curKey, *cur, a, b);
								if(l.s1 == 0.0)
								{
									hit = a;
									return PathRefused;
								}
								if(l.s2 == 0.0)
								{
									hit = b;
									return PathRefused;
								}
								if(Sign3(l.s2) != Sign3(l.s1))
									return RailingHit(curKey, a, b, hit);
							}
						}
						else if(!DoorOpen(doors, *cur) && !(cur->flags & 8))
						{
							LineSides l = Sides(curKey, *cur, a, b);
							int s1 = Sign3(l.s1), s2 = Sign3(l.s2);
							if(s2 != s1 || s1 == 0 || s2 == 0)
							{
								if(!MayOpen(*cur, crew))
								{
									if(s1 == 0)
									{
										hit = a;
										return PathRefused;
									}
									if(s2 != 0)
										return DoorHit(curKey, a, b, hit);
									hit = b;
									return PathRefused;
								}
								list.push_back((uint16_t)cur->door);
							}
						}
					}
					if(FindExtant(key))
						return WallHit(key, a, b, hit);
					cur = 0;
				}
			}
			else
			{
				const PathableCell& nc = PathableOrEnd(key);
				const PathableCell* ncp = FindPathable(key);
				if(prevDeck < deck)
				{
					if((nc.flags & 0x20) && !DoorOpen(doors, nc))
					{
						if(!MayOpen(nc, crew))
						{
							Point d = Sub(b, a);
							hit = Add(a, Mul(d, ((double)(int)deck * DeckHeight - a.z) / (b.z - a.z)));
							return PathCeilingHit;
						}
						list.push_back((uint16_t)nc.door);
					}
				}
				else if(deck < prevDeck)
				{
					bool floor = false;
					if((cur->flags & 8) && !DoorOpen(doors, *cur))
					{
						if((cur->room2 != 0 || !(cur->flags & 8)) && (crew || !(cur->flags & 2)))
							list.push_back((uint16_t)cur->door);
						else
							floor = true;
					}
					if(!floor && (nc.flags & 0x20) && !DoorOpen(doors, nc) && !(nc.flags & 8))
					{
						if(MayOpen(nc, crew))
							list.push_back((uint16_t)nc.door);
						else
							floor = true;
					}
					if(floor)
					{
						Point d = Sub(b, a);
						hit = Add(a, Mul(d, ((double)(int)prevDeck * DeckHeight - a.z) / (b.z - a.z)));
						return PathFloorHit;
					}
				}
				else
				{
					if(i == 1)
					{
						if(!(cur->flags & 0x20))
						{
							if(cur->type == 0x12 && !noRail)
							{
								LineSides l = Sides(curKey, *cur, a, b);
								if(l.s1 == 0.0)
								{
									hit = a;
									return PathRefused;
								}
								if(l.s2 == 0.0)
								{
									hit = b;
									return PathRefused;
								}
								if(Sign3(l.s2) != Sign3(l.s1))
									return RailingHit(curKey, a, b, hit);
							}
						}
						else if(!DoorOpen(doors, *cur) && !(cur->flags & 8))
						{
							LineSides l = Sides(curKey, *cur, a, b);
							int s1 = Sign3(l.s1), s2 = Sign3(l.s2);
							if(s2 != s1 || s1 == 0 || s2 == 0)
							{
								if(!MayOpen(*cur, crew))
								{
									if(s1 != 0)
									{
										if(s2 == 0)
										{
											hit = b;
											return PathRefused;
										}
										return DoorHit(curKey, a, b, hit);
									}
									hit = a;
									return PathRefused;
								}
								list.push_back((uint16_t)cur->door);
							}
						}
					}
					PathTest r = TestDiagonalCornerClip(a, b, prevX, prevY, prevDeck, cur->dirs, bit, rooms, hit);
					if(r != PathOk)
						return r;
					if(!(nc.flags & 0x20))
					{
						if(nc.type == 0x12 && !noRail)
							return RailingHit(key, a, b, hit);
					}
					else if(!DoorOpen(doors, nc) && !(nc.flags & 8))
					{
						if(!MayOpen(nc, crew))
							return DoorHit(key, a, b, hit);
						list.push_back((uint16_t)nc.door);
					}
				}
				cur = ncp;
			}
		}
		prevDeck = deck;
		prevX = x;
		prevY = y;
		curKey = key;
	}
	return PathOk;
}

namespace
{
inline bool InBox(const DesignBounds& d, const Point& p)
{
	return d.minX - HalfGrid <= p.x && d.minY - HalfGrid <= p.y && p.x <= d.maxX + HalfGrid && p.y <= d.maxY + HalfGrid;
}

bool CanEncodeGridCoord(const Point& p)
{
	double x = std::round(p.x / GridUnit);
	if(!(-128.0 <= x && x <= 127.0))
		return false;
	double y = std::round(p.y / GridUnit);
	return -128.0 <= y && y <= 127.0;
}

Point GridSnapped(const Point& p, int8_t deck)
{
	int gx = (int8_t)(int)std::round(p.x / GridUnit);
	int gy = (int8_t)(int)std::round(p.y / GridUnit);
	return Point((double)gx * GridUnit, (double)gy * GridUnit, (double)(int)deck * DeckHeight);
}
}

int DqPathCellTypeAt(const Path& path, const DesignBounds& d, const Point& p)
{
	if(!InBox(d, p))
		return 0;
	int8_t deck = (int8_t)OnDeck(p.z);
	if(!(d.lowDeck - 1 <= (int)deck && (int)deck <= d.highDeck + 1))
		return 0;
	int x, y;
	HalfGridCoord(p, x, y);
	const ExtantCell* e = path.FindExtant(MakeKey(x, y, deck));
	return e ? e->type : 0;
}

int8_t DqDeckBelow(const Path& path, const Point& p, bool& gravity)
{
	if(!CanEncodeGridCoord(p))
		return -0x2b;
	int8_t deck = (int8_t)OnDeck(p.z);
	return path.DeckBelow(GridSnapped(p, deck), deck, gravity);
}

int8_t DqDeckBelow(const Path& path, const Point& p, bool& gravity, const RoomArray& rooms)
{
	if(!CanEncodeGridCoord(p))
		return -0x2b;
	int8_t deck = (int8_t)OnDeck(p.z);
	return path.DeckBelow(GridSnapped(p, deck), deck, gravity, rooms);
}

bool DqInsideRoom(const Path& path, const DesignBounds& d, const Point& p, const RoomArray& rooms, uint16_t& room)
{
	room = 0;
	if(InBox(d, p))
	{
		int8_t deck = (int8_t)OnDeck(p.z);
		if(d.lowDeck <= (int)deck && (int)deck <= d.highDeck)
			return path.InsideRoom(p, deck, rooms, room);
	}
	return path.IsPathable(p);
}

PathTest DqTestSpacecraftPath(const Path& path, const Point& a, const Point& b, bool crew, const RoomArray& rooms,
	const DoorStates& doors, std::vector<uint16_t>& list, bool noRail)
{
	bool stairs = false;
	return path.TestSpacecraftPath(a, b, crew, rooms, doors, list, noRail, stairs);
}

PathTest DqTestSpacecraftPath(const Path& path, const Point& a, const Point& b, bool crew, const RoomArray& rooms,
	const DoorStates& doors, std::vector<uint16_t>& list, bool noRail, Point& hit)
{
	bool stairs = false;
	return path.TestSpacecraftPath(a, b, crew, rooms, doors, list, noRail, stairs, hit);
}

}
