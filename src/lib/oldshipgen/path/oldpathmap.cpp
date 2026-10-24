#include "oldpath.h"
#include "oldpathmath.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <set>

namespace OldPath
{

uint8_t StairCellMap[80] = {
	0x00, 0x13, 0x1b, 0x23, 0x2b, 0x33, 0x0f, 0x3b, 0x43, 0x00,
	0x00, 0x14, 0x1c, 0x24, 0x2c, 0x34, 0x0f, 0x3c, 0x44, 0x00,
	0x00, 0x15, 0x1d, 0x25, 0x2d, 0x35, 0x0f, 0x3d, 0x45, 0x00,
	0x00, 0x16, 0x1e, 0x26, 0x2e, 0x36, 0x0f, 0x3e, 0x46, 0x00,
	0x00, 0x17, 0x1f, 0x27, 0x2f, 0x37, 0x0f, 0x3f, 0x47, 0x00,
	0x00, 0x18, 0x20, 0x28, 0x30, 0x38, 0x0f, 0x40, 0x48, 0x00,
	0x00, 0x19, 0x21, 0x29, 0x31, 0x39, 0x0f, 0x41, 0x49, 0x00,
	0x00, 0x1a, 0x22, 0x2a, 0x32, 0x3a, 0x0f, 0x42, 0x4a, 0x00 };

void UseIdentityStairCellMap()
{
	for(int i=0; i<80; i++)
		StairCellMap[i] = (uint8_t)i;
}

namespace
{

using namespace AuMath;

struct Dir
{
	int8_t		dx, dy, dz;
	uint32_t	bit, back, clearBack, cost;
};

const Dir Dirs[26] = {
	{ 1, 0, 0, 0x400, 0x4000, 0xffffbfff, 0xa },
	{ 1, 1, 0, 0x800, 0x8000, 0xffff7fff, 0xe },
	{ 0, 1, 0, 0x1000, 0x10000, 0xfffeffff, 0xa },
	{ -1, 1, 0, 0x2000, 0x20000, 0xfffdffff, 0xe },
	{ -1, 0, 0, 0x4000, 0x400, 0xfffffbff, 0xa },
	{ -1, -1, 0, 0x8000, 0x800, 0xfffff7ff, 0xe },
	{ 0, -1, 0, 0x10000, 0x1000, 0xffffefff, 0xa },
	{ 1, -1, 0, 0x20000, 0x2000, 0xffffdfff, 0xe },
	{ 0, 0, 1, 0x40000, 0x1, 0xfffffffe, 0xc },
	{ 1, 0, 1, 0x80000, 0x20, 0xffffffdf, 0x1c },
	{ 1, 1, 1, 0x100000, 0x40, 0xffffffbf, 0x22 },
	{ 0, 1, 1, 0x200000, 0x80, 0xffffff7f, 0x1c },
	{ -1, 1, 1, 0x400000, 0x100, 0xfffffeff, 0x22 },
	{ -1, 0, 1, 0x800000, 0x2, 0xfffffffd, 0x1c },
	{ -1, -1, 1, 0x1000000, 0x4, 0xfffffffb, 0x22 },
	{ 0, -1, 1, 0x2000000, 0x8, 0xfffffff7, 0x1c },
	{ 1, -1, 1, 0x4000000, 0x10, 0xffffffef, 0x22 },
	{ 0, 0, -1, 0x1, 0x40000, 0xfffbffff, 0xb },
	{ 1, 0, -1, 0x2, 0x800000, 0xff7fffff, 0x1c },
	{ 1, 1, -1, 0x4, 0x1000000, 0xfeffffff, 0x22 },
	{ 0, 1, -1, 0x8, 0x2000000, 0xfdffffff, 0x1c },
	{ -1, 1, -1, 0x10, 0x4000000, 0xfbffffff, 0x22 },
	{ -1, 0, -1, 0x20, 0x80000, 0xfff7ffff, 0x1c },
	{ -1, -1, -1, 0x40, 0x100000, 0xffefffff, 0x22 },
	{ 0, -1, -1, 0x80, 0x200000, 0xffdfffff, 0x1c },
	{ 1, -1, -1, 0x100, 0x400000, 0xffbfffff, 0x22 },
};

inline uint32_t Neighbour(uint32_t key, const Dir& d)
{
	return MakeKey(KeyX(key) + d.dx, KeyY(key) + d.dy, KeyDeck(key) + d.dz);
}

struct RoomF
{
	int16_t				id;
	int8_t				deck;
	uint8_t				type;
	std::vector<Point>	outline;
};

struct DetailF
{
	int8_t		deck;
	uint8_t		type;
	Point		loc;
	int16_t		room;
};

inline bool SkippedRoomType(uint8_t t)
{
	return t == 0x16 || t == 0x17 || t == 0x18;
}

inline uint8_t LatAt(const FloorBuffer82& b, int index)
{
	return (unsigned)index < 0x10000u ? b.Raw()[index] : 0;
}

inline void LatSet(FloorBuffer82& b, int index, uint8_t v)
{
	if((unsigned)index < 0x10000u)
		b.Raw()[index] = v;
}

bool IsAutomaticDoor(uint32_t t)
{
	return t == 5 || t == 8 || t == 0x31 || t == 0x33 || (t >= 0x4d && t <= 0x5a);
}

bool IsCrewDoor(uint32_t t)
{
	return t == 0x31 || t == 0x33 || t == 0x36 || (t >= 0x54 && t <= 0x5a);
}

bool IsWallOpening(uint32_t t)
{
	return t == 9 || t == 0x37;
}

bool CanTurretConnect(uint8_t t)
{
	switch(t)
	{
	case 2: case 3: case 8: case 9: case 0xa: case 0xc: case 0xe: case 0x4b: case 0x4c:
		return true;
	default:
		return false;
	}
}

Point OpeningA(const Opening& o) { return Point(o.a[0], o.a[1], o.a[2]); }
Point OpeningB(const Opening& o) { return Point(o.b[0], o.b[1], o.b[2]); }
Point OpeningLoc(const Opening& o) { return Point(o.loc[0], o.loc[1], o.loc[2]); }

double DistToBoundary2D(const std::vector<Point>& pts, const Point& p)
{
	if(pts.size() < 2)
		return -1.0;
	double best = -1.0;
	bool found = false;
	for(size_t i = 1; i < pts.size(); i++)
	{
		const Point q = PointOnSegment2D(p, pts[i - 1], pts[i]);
		const double dx = p.x - q.x, dy = p.y - q.y;
		const double d = std::sqrt(dx * dx + dy * dy);
		if(!found || best > d)
		{
			best = d;
			found = true;
		}
	}
	if(pts.size() > 2)
	{
		const Point q = PointOnSegment2D(p, pts.back(), pts.front());
		const double dx = p.x - q.x, dy = p.y - q.y;
		const double d = std::sqrt(dy * dy + dx * dx);
		if(!found || best > d)
			best = d;
	}
	return best;
}

struct Accessible
{
	uint32_t	cost;
	uint16_t	flags;
	uint8_t		stairDir;
};

typedef std::multimap<uint32_t, std::pair<uint32_t, uint32_t> > CellToDoMap;

void AddCell(CellToDoMap& todo, uint32_t cost, uint32_t key, uint32_t connect)
{
	for(CellToDoMap::iterator it = todo.begin(); it != todo.end(); ++it)
		if(it->second.second == key)
		{
			if(it->first <= cost)
				return;
			todo.erase(it);
			break;
		}
	todo.insert(todo.lower_bound(cost), std::make_pair(cost, std::make_pair(connect, key)));
}

uint32_t MoveCost(uint8_t type)
{
	static const int32_t T[18] = { -1, -1, 0, 30, 30, 0, 0, 0, 20, 20, 20, 50, 40, 20, 150, -1, 10, 100 };
	if(type < 18)
		return (uint32_t)T[type];
	return type == 0x4b || type == 0x4c ? 150 : 0;
}

bool TestStepOk(uint16_t c, uint16_t n, int dz)
{
	const uint16_t S = 0xff00;
	if(dz == 0)
	{
		if(!(c & S) && ((n & 0x1800) || !(n & S)))
			return true;
		if(c & 0x8000) { if(n & 0xc000) return true; }
		else if(c & 0x4000) { if(n & 0xe000) return true; }
		else if(c & 0x2000) { if(n & 0x7000) return true; }
		else if(c & 0x1000) { if((n & 0x3800) || !(n & S)) return true; }
		if(c & 0x800) return (n & 0x1c00) || !(n & S);
		if(c & 0x400) return (n & 0xe00) != 0;
		if(c & 0x200) return (n & 0x700) != 0;
		if(c & 0x100) return (n & 0x300) != 0;
		return false;
	}
	if(dz > 0)
	{
		if(!(c & S) && ((n & 0x1000) || !(n & S)))
			return true;
		if(c & 0x8000) return (n & 0x1c00) || !(n & S);
		if(c & 0x4000) return (n & 0xe00) != 0;
		if(c & 0x2000) return (n & 0x700) != 0;
		if(c & 0x1000) return (n & 0x300) != 0;
		return false;
	}
	if(!(c & S) && ((n & 0x8000) || !(n & S)))
		return true;
	if((c & 0x1000) && ((n & 0x8000) || !(n & S)))
		return true;
	if(c & 0x800) return (n & 0xc000) != 0;
	if(c & 0x400) return (n & 0xe000) != 0;
	if(c & 0x200) return (n & 0x7000) != 0;
	if(c & 0x100) return (n & 0x3800) || !(n & S);
	return false;
}

struct StairTable
{
	int		rows, cols, dx, dy;
	int		cells[49];
};

#include "oldpathstairs.inc"

class QHashOrder
{
public:
	QHashOrder() : numBits(0), size(0) {}
	void Insert(uint32_t key)
	{
		if(!buckets.empty() && Find(key))
			return;
		if(size >= (int)buckets.size())
			Rehash(numBits + 1);
		buckets[key % buckets.size()].push_back(key);
		size++;
	}
	void Erase(uint32_t key)
	{
		if(Unlink(key))
			size--;
	}
	void Remove(uint32_t key)
	{
		if(!Unlink(key))
			return;
		size--;
		if(size <= (int)(buckets.size() >> 3) && numBits > 4)
			Rehash(numBits - 2 > 4 ? numBits - 2 : 4);
	}
	void Keys(std::vector<uint32_t>& out) const
	{
		out.clear();
		for(size_t b = 0; b < buckets.size(); b++)
			out.insert(out.end(), buckets[b].begin(), buckets[b].end());
	}

private:
	std::vector<std::vector<uint32_t> > buckets;
	int numBits, size;

	static int PrimeForNumBits(int bits)
	{
		static const int deltas[] = { 0, 0, 1, 3, 1, 5, 3, 3, 1, 9, 7, 5, 3, 9, 25, 3, 1, 21, 3, 41, 9, 25, 15, 41, 3,
			21, 3, 15, 39, 15, 39, 17 };
		return (1 << bits) + deltas[bits];
	}
	bool Find(uint32_t key) const
	{
		const std::vector<uint32_t>& c = buckets[key % buckets.size()];
		return std::find(c.begin(), c.end(), key) != c.end();
	}
	bool Unlink(uint32_t key)
	{
		if(buckets.empty())
			return false;
		std::vector<uint32_t>& c = buckets[key % buckets.size()];
		std::vector<uint32_t>::iterator it = std::find(c.begin(), c.end(), key);
		if(it == c.end())
			return false;
		c.erase(it);
		return true;
	}
	void Rehash(int hint)
	{
		if(hint < 4)
			hint = 4;
		if(hint == numBits)
			return;
		std::vector<std::vector<uint32_t> > old;
		old.swap(buckets);
		numBits = hint;
		buckets.assign(PrimeForNumBits(hint), std::vector<uint32_t>());
		for(size_t b = 0; b < old.size(); b++)
			for(size_t k = 0; k < old[b].size(); k++)
				buckets[old[b][k] % buckets.size()].push_back(old[b][k]);
	}
};

class Builder
{
public:
	Builder(Path& path, const Design& design, const Openings& openings, StageObserver* observer);
	void Run();

private:
	Path&					p;
	const Openings&			o;
	StageObserver*			observer;
	std::vector<RoomF>		rooms;
	std::vector<DetailF>	details;
	FloorBuffer82			lattice, scratch;

	bool Stage(const char* name, int deck, bool buffers);

	void PaintFloorPlan(int8_t deck, int8_t highDeck);
	void PaintOpenings(int8_t deck, int8_t highDeck);
	void PaintOpening(int8_t deck, const Opening& op, int door, int hole, int railing, uint8_t type, uint16_t flags);
	void PaintLargeOpening(int8_t deck, const std::vector<Opening>& list, const Opening& op, int door, int hole,
			int railing, uint8_t type, uint16_t flags);
	bool IsWallEndEmbedded(const Point& p, int8_t deck, const std::vector<Opening>& list) const;
	int InsideRoomOrOnWall(const Point& p, int8_t deck, uint16_t& id) const;
	const RoomF* FindRoom(int16_t id) const;
	void AddPathableDoorCell(int x, int y, int8_t deck, uint8_t type, uint16_t flags, int door, int hole, int railing,
			uint16_t room1, uint16_t room2);
	void ConnectOpeningNeighbors(PathableCell& cell, int x, int y, int8_t deck);
	void ConnectBelowOpening(PathableCell& cell, int x, int y, int8_t deck);
	void PaintObstaclesAndTurrets(int8_t deck, int8_t highDeck);
	void RemovePathableCell(int x, int y, int8_t deck);
	void DisconnectCells(int8_t deck, int x1, int y1, uint32_t bits1, int x2, int y2, uint32_t bits2);
	void ConnectTurretNeighbors(int x, int y, int8_t deck, uint16_t room);
	void PaintDiagonalHullWalls(int8_t deck);
	void PaintStairs(const DetailF& d, int mode);
	void PaintStairCells(int ox, int oy, int8_t deck, const StairTable& t, int mode);
	void LatticeToCells(int8_t deck);
	void ConnectLedges();
	void TrimVerticalPathConnections();
	void DropOneSidedLinks();
	void AccessibleSpacecraftDoors(std::map<uint32_t, uint64_t>& links, uint32_t key, uint32_t connect,
			uint16_t flags, uint16_t room);
	void AccessibleSpacecraftNeighbors(std::map<uint32_t, uint64_t>& links, std::map<uint32_t, Accessible>& acc,
			uint32_t key, uint32_t connect, uint16_t room, std::set<uint32_t>& visited, CellToDoMap& todo);
	void HubLinks();
	void PathRoomCells(std::map<uint32_t, PathCell>::iterator hub, bool room1);
	void SetHub(uint32_t key, const PathCell& hub);
	void EraseHub(uint32_t key, bool remove);
	QHashOrder				hubOrder;
	void DisconnectInaccessibleConnections(std::map<uint32_t, PathCell>::iterator hub);
	bool TestConnection(uint32_t aKey, const PathableCell& a, uint32_t bKey, const PathCell& b);
	uint32_t FindBottomStep(uint32_t key);
	bool DoesStairlessPathExist(uint16_t room, uint32_t fromKey, uint32_t toKey, int mode);
	bool AccessibleCells(std::map<uint32_t, Accessible>& acc, uint32_t key, uint32_t connect, uint16_t room,
			std::set<uint32_t>& visited, CellToDoMap& todo, bool skipOpen, int mode, int8_t deck);
};

Builder::Builder(Path& path, const Design& design, const Openings& openings, StageObserver* obs)
	: p(path), o(openings), observer(obs)
{
	for(size_t i = 0; i < design.rooms.size(); i++)
	{
		const Room& r = design.rooms[i];
		RoomF f;
		f.id = (int16_t)r.id;
		f.deck = (int8_t)r.deck;
		f.type = (uint8_t)r.type;
		for(size_t k = 0; k < r.points.size(); k++)
			f.outline.push_back(Point(r.points[k].x * GridUnit, r.points[k].y * GridUnit, r.deck * DeckHeight));
		rooms.push_back(f);
	}
	for(size_t i = 0; i < design.details.size(); i++)
	{
		const Detail& d = design.details[i];
		DetailF f;
		f.deck = (int8_t)d.deck;
		f.type = (uint8_t)d.type;
		f.loc = Point(d.x * GridUnit, d.y * GridUnit, d.deck * DeckHeight);
		f.room = (int16_t)d.room;
		details.push_back(f);
	}
}

bool Builder::Stage(const char* name, int deck, bool buffers)
{
	if(!observer)
		return true;
	return observer->Stage(name, deck, p, buffers ? &lattice : 0, buffers ? &scratch : 0);
}

void Builder::Run()
{
	p.extant.clear();
	p.pathable.clear();
	p.cells.clear();
	p.flag = (uint8_t)o.lowDeck;
	const int8_t highDeck = (int8_t)o.highDeck;
	for(int deck = o.lowDeck; deck <= o.highDeck + 1; deck++)
	{
		const int8_t d = (int8_t)deck;
		PaintFloorPlan(d, highDeck);
		if(!Stage("floorplan", deck, true))
			return;
		PaintOpenings(d, highDeck);
		if(!Stage("openings", deck, true))
			return;
		PaintObstaclesAndTurrets(d, highDeck);
		if(!Stage("obstacles", deck, true))
			return;
		PaintDiagonalHullWalls(d);
		if(!Stage("diagonal", deck, true))
			return;
		LatticeToCells(d);
	}
	if(!Stage("painted", 0, false))
		return;
	ConnectLedges();
	if(!Stage("ledges", 0, false))
		return;
	for(size_t i = 0; i < details.size(); i++)
		PaintStairs(details[i], 2);
	if(!Stage("stairs", 0, false))
		return;
	TrimVerticalPathConnections();
	if(!Stage("trim", 0, false))
		return;
	DropOneSidedLinks();
	if(!Stage("bits", 0, false))
		return;
	HubLinks();
	if(!Stage("doors", 0, false))
		return;
	for(std::map<uint32_t, PathCell>::iterator h = p.cells.begin(); h != p.cells.end(); ++h)
		DisconnectInaccessibleConnections(h);
	if(!Stage("disconnect", 0, false))
		return;
	for(std::map<uint32_t, PathCell>::iterator h = p.cells.begin(); h != p.cells.end(); ++h)
	{
		PathRoomCells(h, true);
		PathRoomCells(h, false);
	}
}

void Builder::PathRoomCells(std::map<uint32_t, PathCell>::iterator hub, bool room1)
{
	const uint16_t room = room1 ? hub->second.room1 : hub->second.room2;
	std::set<uint32_t>& via = room1 ? hub->second.via1 : hub->second.via2;
	std::set<uint32_t> frontier, visited;
	for(std::map<uint32_t, uint64_t>::const_iterator l = hub->second.links.begin(); l != hub->second.links.end(); ++l)
	{
		std::map<uint32_t, PathCell>::const_iterator o = p.cells.find(l->first);
		if(o != p.cells.end() && (o->second.room1 == room || o->second.room2 == room))
			frontier.insert(l->first);
	}
	visited.insert(hub->first);
	via.clear();
	while(!frontier.empty())
	{
		std::set<uint32_t> next;
		for(std::set<uint32_t>::const_iterator f = frontier.begin(); f != frontier.end(); ++f)
		{
			std::map<uint32_t, PathCell>::const_iterator fh = p.cells.find(*f);
			if(fh == p.cells.end())
				continue;
			for(std::map<uint32_t, uint64_t>::const_iterator l = fh->second.links.begin();
					l != fh->second.links.end(); ++l)
				if(!visited.count(l->first))
					next.insert(l->first);
			visited.insert(*f);
			if(fh->second.room1 != room)
				via.insert(fh->second.room1);
			if(fh->second.room2 != room)
				via.insert(fh->second.room2);
		}
		frontier.swap(next);
	}
}

void Builder::HubLinks()
{
	for(std::map<uint32_t, PathCell>::iterator h = p.cells.begin(); h != p.cells.end(); ++h)
	{
		std::map<uint32_t, PathableCell>::const_iterator cit = p.pathable.find(h->first);
		if(cit == p.pathable.end())
			continue;
		const PathableCell& c = cit->second;
		AccessibleSpacecraftDoors(h->second.links, h->first, c.dirs, c.flags, c.room1);
		if(c.door >= 0 || c.hole >= 0 || c.railing >= 0)
			AccessibleSpacecraftDoors(h->second.links, h->first, c.dirs, c.flags, c.room2);
	}
}

void Builder::AccessibleSpacecraftDoors(std::map<uint32_t, uint64_t>& links, uint32_t key, uint32_t connect,
		uint16_t flags, uint16_t room)
{
	std::map<uint32_t, Accessible> acc;
	Accessible start;
	start.cost = 0;
	start.flags = flags;
	start.stairDir = 0;
	acc[key] = start;
	std::set<uint32_t> visited;
	CellToDoMap todo;
	AccessibleSpacecraftNeighbors(links, acc, key, connect, room, visited, todo);
	while(!todo.empty())
	{
		CellToDoMap wave;
		wave.swap(todo);
		for(CellToDoMap::const_iterator it = wave.begin(); it != wave.end(); ++it)
			AccessibleSpacecraftNeighbors(links, acc, it->second.second, it->second.first, room, visited, todo);
	}
}

void Builder::AccessibleSpacecraftNeighbors(std::map<uint32_t, uint64_t>& links, std::map<uint32_t, Accessible>& acc,
		uint32_t key, uint32_t connect, uint16_t room, std::set<uint32_t>& visited, CellToDoMap& todo)
{
	const uint16_t curFlags = acc[key].flags;
	for(int k = 0; k < 26; k++)
	{
		if(!(connect & Dirs[k].bit))
			continue;
		const uint32_t nk = Neighbour(key, Dirs[k]);
		if(visited.count(nk))
			continue;
		std::map<uint32_t, PathableCell>::const_iterator pit = p.pathable.find(nk);
		if(pit == p.pathable.end())
			continue;
		const PathableCell& pc = pit->second;
		if(pc.door < 0 && pc.hole < 0 && pc.railing < 0)
		{
			if(pc.room1 != room)
				continue;
		}
		else if(pc.room1 != room && pc.room2 != room)
			continue;
		if((pc.flags & 0xff10) == 0x10)
			continue;
		if(!TestStepOk(curFlags, pc.flags, Dirs[k].dz))
			continue;
		bool hub;
		switch(pc.type)
		{
		case 3: case 5: case 4: case 6: case 7: case 0x12:
		case 8: case 9: case 0xa: case 0xb: case 0x4b: case 0x4c:
			hub = true;
			break;
		case 2: case 0xc: case 0xd: case 0xe: case 0x10: case 0x11:
			hub = false;
			break;
		default:
			continue;
		}
		const uint32_t cost = MoveCost(pc.type) + Dirs[k].cost + acc[key].cost;
		if(!acc.count(nk))
		{
			Accessible a;
			a.cost = cost;
			a.flags = pc.flags;
			a.stairDir = pc.stairDir & 0x77;
			acc[nk] = a;
		}
		if(hub && !links.count(nk) && p.cells.count(nk))
			links[nk] = cost;
		AddCell(todo, cost, nk, pc.dirs);
	}
	visited.insert(key);
}

void Builder::ConnectLedges()
{
	for(std::map<uint32_t, PathableCell>::iterator it = p.pathable.begin(); it != p.pathable.end(); ++it)
	{
		PathableCell& c = it->second;
		if(!(c.dirs & 0x40000))
			continue;
		for(int k = 9; k <= 16; k++)
		{
			if(c.dirs & Dirs[k].bit)
				continue;
			std::map<uint32_t, PathableCell>::iterator n = p.pathable.find(Neighbour(it->first, Dirs[k]));
			if(n == p.pathable.end())
				continue;
			c.dirs |= Dirs[k].bit;
			n->second.dirs |= Dirs[k].back;
		}
	}
}

void Builder::DropOneSidedLinks()
{
	for(std::map<uint32_t, PathableCell>::iterator it = p.pathable.begin(); it != p.pathable.end(); ++it)
		for(int k = 0; k < 26; k++)
		{
			if(!(it->second.dirs & Dirs[k].bit))
				continue;
			std::map<uint32_t, PathableCell>::const_iterator n = p.pathable.find(Neighbour(it->first, Dirs[k]));
			if(n == p.pathable.end() || !(n->second.dirs & Dirs[k].back))
				it->second.dirs &= ~Dirs[k].bit;
		}
}

inline bool IsVerticalHub(uint8_t t)
{
	return t == 8 || t == 9 || t == 0x4b || t == 0x4c;
}

void Builder::TrimVerticalPathConnections()
{
	std::map<uint16_t, std::vector<uint16_t> > done;
	for(;;)
	{
		std::vector<uint32_t> order;
		hubOrder.Keys(order);
		size_t i = 0;
		for(; i < order.size(); i++)
		{
			const PathCell& c = p.cells[order[i]];
			if(!IsVerticalHub(c.type))
				continue;
			std::map<uint16_t, std::vector<uint16_t> >::const_iterator d = done.find(c.room1);
			if(d != done.end() && std::find(d->second.begin(), d->second.end(), c.room2) != d->second.end())
				continue;
			break;
		}
		if(i == order.size())
			return;
		const PathCell& h = p.cells[order[i]];
		const uint8_t t = h.type;
		const uint8_t t2 = t == 8 ? 0x4b : t == 0x4b ? 8 : t == 9 ? 0x4c : t == 0x4c ? 9 : 0;
		const uint16_t r1 = h.room1, r2 = h.room2;
		if(r1 != 0 && r2 != 0)
		{
			bool stairTop = false;
			Point sum;
			int n = 0;
			for(size_t g = 0; g < order.size(); g++)
			{
				const PathCell& c = p.cells[order[g]];
				if((c.type != t && c.type != t2) || c.room1 != r1 || c.room2 != r2)
					continue;
				if(c.stairFlags & 0x800)
				{
					stairTop = true;
					break;
				}
				const Point q = CellCenter(KeyX(order[g]), KeyY(order[g]), KeyDeck(order[g]));
				sum.x += q.x;
				sum.y += q.y;
				sum.z += q.z;
				n++;
			}
			if(stairTop)
			{
				for(size_t g = 0; g < order.size(); g++)
				{
					const uint32_t key = order[g];
					const PathCell& c = p.cells[key];
					if((c.type != t && c.type != t2) || c.room1 != r1 || c.room2 != r2)
						continue;
					if(!(c.stairFlags & 0x800))
					{
						EraseHub(key, false);
						continue;
					}
					if(c.type != 0x4b && c.type != 0x4c)
						continue;
					const PathableCell& pc = p.pathable[key];
					bool besideTop = false;
					for(int k = 0; k < 8 && !besideTop; k++)
					{
						if(!(pc.dirs & Dirs[k].bit))
							continue;
						std::map<uint32_t, PathCell>::const_iterator nb = p.cells.find(Neighbour(key, Dirs[k]));
						besideTop = nb != p.cells.end() && (nb->second.stairFlags & 0x800);
					}
					if(besideTop)
						EraseHub(key, false);
				}
			}
			else if(n > 1)
			{
				const Point c(sum.x / n, sum.y / n, sum.z / n);
				int cx, cy;
				HalfGridCoord(c, cx, cy);
				uint32_t keep = MakeKey(cx, cy, OnDeck(c.z));
				std::map<uint32_t, PathCell>::const_iterator at = p.cells.find(keep);
				if(at == p.cells.end() || (at->second.type != t && at->second.type != t2) || at->second.room1 != r1 ||
						at->second.room2 != r2)
				{
					keep = 0x800;
					double best = -1.0;
					for(size_t g = 0; g < order.size(); g++)
					{
						const PathCell& q = p.cells[order[g]];
						if((q.type != t && q.type != t2) || q.room1 != r1 || q.room2 != r2)
							continue;
						const Point e = CellCenter(KeyX(order[g]), KeyY(order[g]), KeyDeck(order[g]));
						const double dx = e.x - c.x, dy = e.y - c.y;
						const double d2 = dy * dy + dx * dx;
						if(0.0 > best || best > d2)
						{
							best = d2;
							keep = order[g];
						}
					}
				}
				for(size_t g = 0; g < order.size(); g++)
				{
					const PathCell& q = p.cells[order[g]];
					if((q.type == t || q.type == t2) && q.room1 == r1 && q.room2 == r2 && order[g] != keep)
						EraseHub(order[g], false);
				}
			}
		}
		done[r1].push_back(r2);
	}
}

void Builder::EraseHub(uint32_t key, bool remove)
{
	if(!p.cells.erase(key))
		return;
	if(remove)
		hubOrder.Remove(key);
	else
		hubOrder.Erase(key);
}

void Builder::SetHub(uint32_t key, const PathCell& hub)
{
	p.cells[key] = hub;
	hubOrder.Insert(key);
}

void Builder::LatticeToCells(int8_t deck)
{
	uint8_t* m = lattice.Raw();
	for(int x = 0; x < 0x100; x++)
		for(int y = 0; y < 0x100; y++)
		{
			uint8_t& c = m[x * 0x100 + y];
			if(c == 0)
				continue;
			const uint32_t key = MakeKey(x, y, deck);
			std::map<uint32_t, ExtantCell>::iterator it = p.extant.find(key);
			if(it == p.extant.end())
			{
				ExtantCell e;
				e.type = c;
				p.extant[key] = e;
			}
			else
				it->second.type = c;
			if(c == 0x10 || c == 7 || c == 0xd)
				c = 0;
			else
				c = 0x10;
		}
}

void AddRoom(ExtantCell& e, uint16_t room)
{
	for(int i = 0; i < 8; i++)
		if(e.rooms[i] == 0)
		{
			e.rooms[i] = room;
			return;
		}
}

ExtantCell& ExtantAt(Path& p, uint32_t key)
{
	std::map<uint32_t, ExtantCell>::iterator it = p.extant.find(key);
	if(it != p.extant.end())
		return it->second;
	ExtantCell e;
	e.type = 1;
	return p.extant[key] = e;
}

uint32_t NeighbourBits(const FloorBuffer82& b, int x, int y, uint8_t v)
{
	const int i = x * 0x100 + y;
	uint32_t bits = 0x200;
	if(LatAt(b, i + 0x100) == v) bits |= 0x400;
	if(LatAt(b, i + 0x101) == v) bits |= 0x800;
	if(LatAt(b, i + 1) == v) bits |= 0x1000;
	if(LatAt(b, i - 0x100 + 1) == v) bits |= 0x2000;
	if(LatAt(b, i - 0x100) == v) bits |= 0x4000;
	if(LatAt(b, i - 0x100 - 1) == v) bits |= 0x8000;
	if(LatAt(b, i - 1) == v) bits |= 0x10000;
	if(LatAt(b, i + 0xff) == v) bits |= 0x20000;
	return bits;
}

PathableCell MakePathable(uint32_t connect, uint8_t type, uint16_t room1)
{
	PathableCell c;
	c.dirs = connect;
	c.type = type;
	c.door = -1;
	c.hole = -1;
	c.flags = 0;
	c.railing = -1;
	c.room1 = room1;
	c.room2 = 0;
	c.stairDir = 0;
	return c;
}

void Builder::PaintFloorPlan(int8_t deck, int8_t highDeck)
{
	if(deck <= highDeck)
		for(size_t r = 0; r < rooms.size(); r++)
		{
			const RoomF& room = rooms[r];
			if(room.id == 0)
				continue;
			if(room.deck != deck || SkippedRoomType(room.type))
				continue;
			if(room.outline.empty())
				continue;
			int minX, minY, maxX, maxY;
			FillRoom(scratch, room.outline, minX, minY, maxX, maxY);
			for(int x = minX; x <= maxX; x++)
				for(int y = minY; y <= maxY; y++)
				{
					const int i = x * 0x100 + y;
					const uint8_t s = LatAt(scratch, i);
					if(s == 0)
						continue;
					AddRoom(ExtantAt(p, MakeKey(x, y, deck)), room.id);
					if(room.type != 9 && s == 2)
						p.pathable[MakeKey(x, y, deck)] = MakePathable(NeighbourBits(scratch, x, y, 2), 2, room.id);
					LatSet(lattice, i, s);
				}
		}
	for(int x = 0; x < 0x100; x++)
		for(int y = 0; y < 0x100; y++)
			if(lattice.Raw()[x * 0x100 + y] == 0x10)
				p.pathable[MakeKey(x, y, deck)] = MakePathable(NeighbourBits(lattice, x, y, 0x10), 0x10, 0);
}

void Builder::PaintOpenings(int8_t deck, int8_t highDeck)
{
	for(size_t i = 0; i < o.doors.size(); i++)
	{
		const Opening& d = o.doors[i];
		if(d.deck != deck)
			continue;
		if(d.room1 == 0)
			continue;
		uint16_t flags = IsAutomaticDoor(d.type) ? 0x21 : 0x20;
		if(IsCrewDoor(d.type))
			flags |= 2;
		const int idx = (int)i;
		switch(d.type)
		{
		case 3:
			if(deck <= highDeck)
				PaintLargeOpening(deck, o.holes, d, idx, -1, -1, 4, flags);
			break;
		case 5: case 0x31:
			if(deck <= highDeck)
				PaintOpening(deck, d, idx, -1, -1, 3, flags);
			break;
		case 8: case 0x33: case 0x4d: case 0x4e: case 0x4f: case 0x50: case 0x51: case 0x52: case 0x53: case 0x54:
		case 0x55: case 0x56: case 0x57: case 0x58: case 0x59: case 0x5a:
			PaintOpening(deck, d, idx, -1, -1, 5, flags | 8);
			break;
		case 0x32:
			if(deck <= highDeck)
				PaintOpening(deck, d, idx, -1, -1, 4, flags);
			break;
		case 0x34: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f: case 0x60: case 0x61:
		{
			int x, y;
			HalfGridCoord(OpeningLoc(d), x, y);
			std::map<uint32_t, PathableCell>::iterator below = p.pathable.find(MakeKey(x, y, (int8_t)(deck - 1)));
			const uint8_t type = (below == p.pathable.end() || below->second.type == 0x10) ? 6 : 7;
			PaintOpening(deck, d, idx, -1, -1, type, flags | 8);
			break;
		}
		case 0x35: case 0x36:
			if(deck <= highDeck)
				PaintLargeOpening(deck, o.holes, d, idx, -1, -1, 3, flags);
			break;
		default:
			break;
		}
	}
	if(deck > highDeck)
		return;
	for(size_t i = 0; i < o.holes.size(); i++)
	{
		const Opening& h = o.holes[i];
		if(h.deck != deck)
			continue;
		if(h.room1 == 0)
			continue;
		switch(h.type)
		{
		case 0: PaintOpening(deck, h, -1, (int)i, -1, 9, 0x10); break;
		case 9: PaintLargeOpening(deck, o.holes, h, -1, (int)i, -1, 0xb, 0); break;
		case 10: PaintOpening(deck, h, -1, (int)i, -1, 8, 4); break;
		case 0x37: PaintLargeOpening(deck, o.holes, h, -1, (int)i, -1, 0xa, 0); break;
		default: break;
		}
	}
	for(size_t i = 0; i < o.railings.size(); i++)
	{
		const Opening& r = o.railings[i];
		if(r.deck != deck)
			continue;
		if(r.room1 == 0)
			continue;
		PaintLargeOpening(deck, o.holes, r, -1, -1, (int)i, 0x12, 0);
	}
}

void Builder::PaintOpening(int8_t deck, const Opening& op, int door, int hole, int railing, uint8_t type,
		uint16_t flags)
{
	int x, y;
	HalfGridCoord(OpeningLoc(op), x, y);
	LatSet(lattice, x * 0x100 + y, type);
	bool diagonal = false;
	const double eps = 1e-05;
	if(std::fabs(op.b[1] - op.a[1]) > eps)
	{
		if(std::fabs(op.b[0] - op.a[0]) <= eps)
			flags |= 0x80;
		else
		{
			diagonal = true;
			if((op.b[0] > op.a[0] && op.b[1] > op.a[1]) || (op.a[0] > op.b[0] && op.a[1] > op.b[1]))
				flags |= 0x40;
			else
				flags |= 0xc0;
		}
	}
	AddPathableDoorCell(x, y, deck, type, flags, door, hole, railing, op.room1, op.room2);
	if(type == 4 && diagonal)
	{
		const int nx[4] = { x + 1, x - 1, x, x }, ny[4] = { y, y, y + 1, y - 1 };
		for(int k = 0; k < 4; k++)
			if(LatAt(lattice, nx[k] * 0x100 + ny[k]) == 0)
			{
				LatSet(lattice, nx[k] * 0x100 + ny[k], 4);
				AddPathableDoorCell(nx[k], ny[k], deck, 4, flags, door, hole, railing, op.room1, op.room2);
			}
	}
	PathCell hub;
	hub.type = type;
	hub.door = (int16_t)door;
	hub.hole = hole;
	hub.stairFlags = 0;
	hub.railing = (int16_t)railing;
	hub.room1 = op.room1;
	hub.room2 = op.room2;
	SetHub(MakeKey(x, y, deck), hub);
}

const RoomF* Builder::FindRoom(int16_t id) const
{
	for(size_t i = 0; i < rooms.size(); i++)
		if(rooms[i].id == id)
			return &rooms[i];
	return 0;
}

void Builder::PaintLargeOpening(int8_t deck, const std::vector<Opening>& list, const Opening& op, int door, int hole,
		int railing, uint8_t type, uint16_t flags)
{
	int x1, y1, x2, y2;
	HalfGridCoord(OpeningA(op), x1, y1);
	HalfGridCoord(OpeningB(op), x2, y2);
	const int dx = x2 - x1, dy = y2 - y1;
	const int n = std::abs(dy) > std::abs(dx) ? std::abs(dy) : std::abs(dx);
	if(n == 0)
		return;
	const int first = IsWallEndEmbedded(OpeningA(op), op.deck, list) ? 1 : 0;
	const int last = IsWallEndEmbedded(OpeningB(op), op.deck, list) ? n - 1 : n;
	if(op.room2 != 0)
	{
		const RoomF* r1 = FindRoom(op.room1);
		const RoomF* r2 = FindRoom(op.room2);
		if(r1 && r2 && (r1->type == 0xb || r1->type == 0xf) && (r2->type == 0xb || r2->type == 0xf))
			flags |= (r1->type == 0xb && r2->type == 0xb) ? 4 : 0x10;
		for(int i = first; i <= last; i++)
		{
			const int px = x1 + i * dx / n, py = y1 + i * dy / n;
			LatSet(lattice, px * 0x100 + py, type);
			AddPathableDoorCell(px, py, deck, type, flags, door, hole, railing, op.room1, op.room2);
		}
	}
	else
	{
		const RoomF* r1 = FindRoom(op.room1);
		if(r1 && (r1->type == 0xb || r1->type == 0xf))
			flags |= r1->type == 0xb ? 4 : 0x10;
		for(int i = first; i <= last; i++)
		{
			const int px = x1 + i * dx / n, py = y1 + i * dy / n;
			LatSet(lattice, px * 0x100 + py, type);
			AddPathableDoorCell(px, py, deck, type, flags, door, hole, railing, op.room1, op.room2);
			if(dx == 0 || dy == 0)
				continue;
			int sx, sy;
			if(dx > 0)
			{
				sx = dy > 0 ? px - 1 : px;
				sy = dy > 0 ? py : py + 1;
			}
			else
			{
				sx = dy < 0 ? px + 1 : px;
				sy = dy < 0 ? py : py - 1;
			}
			if(LatAt(lattice, sx * 0x100 + sy) == 0)
			{
				LatSet(lattice, sx * 0x100 + sy, type);
				AddPathableDoorCell(sx, sy, deck, type, flags, door, hole, railing, op.room1, op.room2);
			}
			if(i == last)
			{
				if(dx > 0)
				{
					sx = dy > 0 ? px : px + 1;
					sy = dy > 0 ? py + 1 : py;
				}
				else
				{
					sx = dy < 0 ? px : px - 1;
					sy = dy < 0 ? py - 1 : py;
				}
				if(LatAt(lattice, sx * 0x100 + sy) == 0)
				{
					LatSet(lattice, sx * 0x100 + sy, type);
					AddPathableDoorCell(sx, sy, deck, type, flags, door, hole, railing, op.room1, op.room2);
				}
			}
		}
	}
	int x, y;
	HalfGridCoord(OpeningLoc(op), x, y);
	const uint32_t key = MakeKey(x, y, deck);
	if(p.pathable.find(key) == p.pathable.end())
		return;
	PathCell hub;
	hub.type = type;
	hub.door = (int16_t)door;
	hub.hole = hole;
	hub.stairFlags = 0;
	hub.railing = (int16_t)railing;
	hub.room1 = op.room1;
	hub.room2 = op.room2;
	SetHub(key, hub);
}

int Builder::InsideRoomOrOnWall(const Point& pt, int8_t deck, uint16_t& id) const
{
	for(size_t i = 0; i < rooms.size(); i++)
	{
		const RoomF& r = rooms[i];
		if(r.deck != deck || SkippedRoomType(r.type))
			continue;
		if(PointInBoundary(r.outline, pt))
		{
			id = (uint16_t)r.id;
			return 1;
		}
		if(PointOnBoundary2D(r.outline, pt))
		{
			id = (uint16_t)r.id;
			return 2;
		}
	}
	return 0;
}

bool Builder::IsWallEndEmbedded(const Point& pt, int8_t deck, const std::vector<Opening>& list) const
{
	std::vector<const Opening*> ending;
	std::vector<uint16_t> ids;
	for(size_t i = 0; i < list.size(); i++)
	{
		const Opening& w = list[i];
		if(!IsWallOpening(w.type) || w.deck != deck)
			continue;
		if(!FuzzyEquals2D(OpeningA(w), pt) && !FuzzyEquals2D(OpeningB(w), pt))
			continue;
		ending.push_back(&w);
		const uint16_t r[2] = { (uint16_t)w.room1, (uint16_t)w.room2 };
		for(int k = 0; k < 2; k++)
			if(r[k] != 0 && std::find(ids.begin(), ids.end(), r[k]) == ids.end())
				ids.push_back(r[k]);
	}
	if(ending.size() <= 1)
		return true;
	for(size_t i = 0; i < rooms.size(); i++)
	{
		const RoomF& r = rooms[i];
		if(r.deck != deck || SkippedRoomType(r.type))
			continue;
		if(std::find(ids.begin(), ids.end(), (uint16_t)r.id) != ids.end())
			continue;
		if(1.0 > DistToBoundary2D(r.outline, pt))
			return true;
	}
	const double a = 2.1213203433750003;
	const double off[8][2] = { { a, 0 }, { a, a }, { 0, a }, { -a, a }, { -a, 0 }, { -a, -a }, { 0, -a }, { a, -a } };
	uint16_t id = 0;
	for(int k = 0; k < 8; k++)
	{
		const Point q(off[k][0] + pt.x, off[k][1] + pt.y, pt.z);
		const int where = InsideRoomOrOnWall(q, deck, id);
		if(where == 0)
			return true;
		if(where == 1)
		{
			if(std::find(ids.begin(), ids.end(), id) == ids.end())
				return true;
		}
		else if(where == 2)
		{
			if(std::find(ids.begin(), ids.end(), id) == ids.end())
				return true;
			bool on = false;
			for(size_t e = 0; e < ending.size() && !on; e++)
				on = 0.1 > Distance2D(q, PointOnSegment2D(q, OpeningA(*ending[e]), OpeningB(*ending[e])));
			if(!on)
				return true;
		}
	}
	return false;
}

void Builder::AddPathableDoorCell(int x, int y, int8_t deck, uint8_t type, uint16_t flags, int door, int hole,
		int railing, uint16_t room1, uint16_t room2)
{
	const uint32_t key = MakeKey(x, y, deck);
	std::map<uint32_t, PathableCell>::iterator it = p.pathable.find(key);
	if(it == p.pathable.end())
	{
		PathableCell c;
		c.dirs = 0x200;
		c.type = type;
		c.door = (int16_t)door;
		c.hole = hole;
		c.flags = flags;
		c.railing = (int16_t)railing;
		c.room1 = room1;
		c.room2 = room2;
		c.stairDir = 0;
		it = p.pathable.insert(std::make_pair(key, c)).first;
	}
	else
	{
		PathableCell& c = it->second;
		c.door = (int16_t)door;
		c.hole = hole;
		c.railing = (int16_t)railing;
		c.room1 = room1;
		c.room2 = room2;
		c.type = type;
		c.flags = flags;
	}
	PathableCell& cell = it->second;
	const uint32_t below = MakeKey(x, y, (int8_t)(deck - 1));
	switch(type)
	{
	case 3: case 4: case 0xa: case 0xb: case 0x12:
		ConnectOpeningNeighbors(cell, x, y, deck);
		if(flags & 0x14)
			ConnectBelowOpening(cell, x, y, deck);
		break;
	case 5: case 7:
	{
		std::map<uint32_t, PathableCell>::iterator b = p.pathable.find(below);
		if(b != p.pathable.end())
		{
			cell.dirs |= 1;
			b->second.dirs |= 0x40000;
		}
		break;
	}
	case 6:
	{
		std::map<uint32_t, PathableCell>::iterator b = p.pathable.find(below);
		if(b != p.pathable.end())
		{
			cell.dirs |= 1;
			b->second.dirs |= 0x40000;
		}
		else if(p.extant.find(below) == p.extant.end())
		{
			cell.dirs |= 1;
			PathableCell out = MakePathable(0x200, 0x11, 0);
			out.dirs |= 0x40000;
			p.pathable[below] = out;
			ExtantCell e;
			e.type = 0x11;
			p.extant[below] = e;
		}
		break;
	}
	case 8: case 9: case 0x4b: case 0x4c:
		ConnectBelowOpening(cell, x, y, deck);
		break;
	default:
		break;
	}
}

void Builder::ConnectOpeningNeighbors(PathableCell& cell, int x, int y, int8_t deck)
{
	for(int k = 0; k < 8; k++)
	{
		std::map<uint32_t, PathableCell>::iterator n = p.pathable.find(MakeKey(x + Dirs[k].dx, y + Dirs[k].dy, deck));
		if(n == p.pathable.end())
			continue;
		cell.dirs |= Dirs[k].bit;
		n->second.dirs |= Dirs[k].back;
	}
}

void Builder::ConnectBelowOpening(PathableCell& cell, int x, int y, int8_t deck)
{
	const int8_t down = (int8_t)(deck - 1);
	std::map<uint32_t, PathableCell>::iterator b = p.pathable.find(MakeKey(x, y, down));
	if(b == p.pathable.end())
	{
		cell.flags &= 0xffef;
		if(cell.type == 9)
			cell.type = 0x4c;
		return;
	}
	PathableCell& bc = b->second;
	cell.dirs |= 1;
	bc.dirs |= 0x40000;
	if(cell.flags & 4)
		bc.flags |= 4;
	const uint16_t bf = bc.flags;
	if(bf & 0xf000)
	{
		if(bf & 0x1000)
			cell.flags |= 0x100;
		else if(bf & 0x2000)
			cell.flags |= 0x200;
		else if(bf & 0x4000)
			cell.flags |= 0x400;
		else if(bf & 0x8000)
			cell.flags |= 0x800;
	}
	static const struct { int8_t dx, dy; uint32_t test, self, other; } T[8] = {
		{ 1, 0, 0x400, 2, 0x800000 }, { 1, 1, 0x800, 4, 0x1000000 }, { 0, 1, 0x1000, 8, 0x2000000 },
		{ -1, 1, 0x2000, 0x10, 0x4000000 }, { -1, 0, 0x4000, 0x20, 0x80000 }, { -1, -1, 0x8000, 0x40, 0x100000 },
		{ 0, -1, 0x10000, 0x80, 0x200000 }, { 1, -1, 0x20000, 0x100, 0x400000 } };
	for(int k = 0; k < 8; k++)
	{
		if(!(bc.dirs & T[k].test))
			continue;
		std::map<uint32_t, PathableCell>::iterator n = p.pathable.find(MakeKey(x + T[k].dx, y + T[k].dy, down));
		if(n == p.pathable.end())
			continue;
		cell.dirs |= T[k].self;
		n->second.dirs |= T[k].other;
	}
}

void Builder::PaintObstaclesAndTurrets(int8_t deck, int8_t highDeck)
{
	for(size_t i = 0; i < details.size(); i++)
	{
		const DetailF& d = details[i];
		if(d.deck != deck)
			continue;
		bool around = false;
		uint8_t v;
		switch(d.type)
		{
		case 0x15: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28:
			if(deck > highDeck) continue;
			v = 0xc;
			break;
		case 0x16:
			v = 0xd;
			break;
		case 0x1: case 0x2: case 0x4: case 0x6: case 0x7: case 0xb: case 0xc: case 0xd: case 0xf: case 0x11: case 0x13:
		case 0x14: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f: case 0x21: case 0x41:
		case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: case 0x48: case 0x49:
			if(deck > highDeck) continue;
			v = 0xe;
			break;
		case 0xa: case 0xe: case 0x10: case 0x12: case 0x17: case 0x18: case 0x20: case 0x64: case 0x8e:
			if(deck > highDeck) continue;
			v = 0xf;
			break;
		case 0x4a: case 0x4b:
			if(deck > highDeck) continue;
			v = 0xf;
			around = true;
			break;
		default:
			if(d.type >= 0x65 && d.type <= 0x8c)
				PaintStairs(d, 1);
			continue;
		}
		int x, y;
		HalfGridCoord(d.loc, x, y);
		LatSet(lattice, x * 0x100 + y, v);
		const uint32_t key = MakeKey(x, y, deck);
		if(v == 0xf)
		{
			RemovePathableCell(x, y, deck);
			if(LatAt(lattice, x * 0x100 + y + 0x101) == 0xf)
				DisconnectCells(deck, x + 1, y, 0x2000, x, y + 1, 0x20000);
			if(LatAt(lattice, (x - 1) * 0x100 + 1 + y) == 0xf)
				DisconnectCells(deck, x - 1, y, 0x800, x, y + 1, 0x8000);
			if(LatAt(lattice, (x - 1) * 0x100 - 1 + y) == 0xf)
				DisconnectCells(deck, x - 1, y, 0x20000, x, y - 1, 0x2000);
			if(LatAt(lattice, x * 0x100 + 0xff + y) == 0xf)
				DisconnectCells(deck, x + 1, y, 0x8000, x, y - 1, 0x800);
			if(around)
				for(int ax = -1; ax <= 1; ax++)
					for(int ay = -1; ay <= 1; ay++)
					{
						if(ax == 0 && ay == 0)
							continue;
						const int k = (x + ax) * 0x100 + y + ay;
						if(LatAt(lattice, k) != 2)
							continue;
						LatSet(lattice, k, 0xe);
						std::map<uint32_t, PathableCell>::iterator c = p.pathable.find(MakeKey(x + ax, y + ay, deck));
						if(c != p.pathable.end() && c->second.type == 2)
							c->second.type = 0xe;
					}
		}
		else if(v == 0xe)
		{
			std::map<uint32_t, PathableCell>::iterator c = p.pathable.find(key);
			if(c != p.pathable.end() && c->second.type == 2)
				c->second.type = 0xe;
		}
		else if(v == 0xd)
		{
			std::map<uint32_t, PathableCell>::iterator c = p.pathable.find(key);
			if(c != p.pathable.end())
				c->second.type = 0xd;
		}
		else
			ConnectTurretNeighbors(x, y, deck, (uint16_t)d.room);
	}
}

void Builder::RemovePathableCell(int x, int y, int8_t deck)
{
	const uint32_t key = MakeKey(x, y, deck);
	std::map<uint32_t, PathableCell>::iterator it = p.pathable.find(key);
	if(it == p.pathable.end())
		return;
	p.pathable.erase(it);
	for(int k = 0; k < 26; k++)
	{
		std::map<uint32_t, PathableCell>::iterator n = p.pathable.find(
				MakeKey(x + Dirs[k].dx, y + Dirs[k].dy, (int8_t)(deck + Dirs[k].dz)));
		if(n != p.pathable.end() && (n->second.dirs & Dirs[k].back))
			n->second.dirs &= Dirs[k].clearBack;
	}
	EraseHub(key, true);
}

void Builder::DisconnectCells(int8_t deck, int x1, int y1, uint32_t bits1, int x2, int y2, uint32_t bits2)
{
	std::map<uint32_t, PathableCell>::iterator a = p.pathable.find(MakeKey(x1, y1, deck));
	if(a != p.pathable.end())
		a->second.dirs &= ~bits1;
	std::map<uint32_t, PathableCell>::iterator b = p.pathable.find(MakeKey(x2, y2, deck));
	if(b != p.pathable.end())
		b->second.dirs &= ~bits2;
}

void Builder::ConnectTurretNeighbors(int x, int y, int8_t deck, uint16_t room)
{
	PathableCell& cell = p.pathable[MakeKey(x, y, deck)] = MakePathable(0x200, 0xc, room);
	for(int k = 0; k < 8; k++)
	{
		std::map<uint32_t, PathableCell>::iterator n = p.pathable.find(MakeKey(x + Dirs[k].dx, y + Dirs[k].dy, deck));
		if(n == p.pathable.end() || !CanTurretConnect(n->second.type))
			continue;
		cell.dirs |= Dirs[k].bit;
		n->second.dirs |= Dirs[k].back;
	}
}

void Builder::PaintDiagonalHullWalls(int8_t deck)
{
	uint8_t* m = lattice.Raw();
	for(int x = 0; x < 0x100; x++)
		for(int y = 0; y < 0x100; y++)
		{
			const int i = x * 0x100 + y;
			const uint8_t c = m[i];
			if(c != 0 && c != 0x10)
				continue;
			const bool alongX = (x < 0xff && m[i + 0x100] == 1) || (x > 0 && m[i - 0x100] == 1);
			const bool alongY = (y < 0xff && m[i + 1] == 1) || (y > 0 && m[i - 1] == 1);
			if(!alongX || !alongY)
				continue;
			m[i] = 0xf;
			if(c == 0x10)
				RemovePathableCell(x, y, deck);
		}
}

void Builder::PaintStairs(const DetailF& d, int mode)
{
	if(d.type < 0x65 || d.type > 0x8c)
		return;
	int x, y;
	HalfGridCoord(d.loc, x, y);
	PaintStairCells(x, y, d.deck, StairTables[d.type - 0x65], mode);
}

void Builder::PaintStairCells(int ox, int oy, int8_t deck, const StairTable& t, int mode)
{
	for(int i = 0; i < t.rows; i++)
		for(int j = 0; j < t.cols; j++)
		{
			const int x = ox - t.dx + j, y = oy - t.dy + i;
			const uint8_t code = (uint8_t)t.cells[(t.rows - i - 1) * t.cols + j];
			const uint8_t m = StairCellMap[code < 100 ? code : code - 100];
			if(m == 0)
				continue;
			if(mode == 2)
			{
				const uint32_t key = MakeKey(x, y, deck);
				std::map<uint32_t, PathableCell>::iterator c = p.pathable.find(key);
				if(c == p.pathable.end())
					continue;
				uint8_t dir, down;
				uint16_t step, fromBelow;
				if(m >= 0x13 && m <= 0x1a) { dir = m - 0x13; step = 0x1000; fromBelow = 0x100; down = (dir + 4) & 7; }
				else if(m >= 0x1b && m <= 0x22) { dir = m - 0x1b; step = 0x2000; fromBelow = 0x200; down = (dir + 4) & 7; }
				else if(m >= 0x23 && m <= 0x2a) { dir = m - 0x23; step = 0x4000; fromBelow = 0x400; down = (dir + 4) & 7; }
				else if(m >= 0x2b && m <= 0x32) { dir = m - 0x2b; step = 0x8000; fromBelow = 0x800; down = (dir + 4) & 7; }
				else if(m >= 0x3b && m <= 0x42) { dir = m - 0x3b; step = 0x4000; fromBelow = 0x400; down = (dir + 2) & 7; }
				else if(m >= 0x43 && m <= 0x4a) { dir = m - 0x43; step = 0x4000; fromBelow = 0x400; down = (dir + 6) & 7; }
				else
					continue;
				PathableCell& cell = c->second;
				cell.flags |= step;
				cell.stairDir = (uint8_t)(down << 4 | dir);
				std::map<uint32_t, PathCell>::iterator h = p.cells.find(key);
				if(h != p.cells.end())
					h->second.stairFlags |= step;
				const uint32_t upKey = MakeKey(x, y, (int8_t)(deck + 1));
				std::map<uint32_t, PathableCell>::iterator u = p.pathable.find(upKey);
				if(u == p.pathable.end() || !(u->second.flags & 0x14))
					continue;
				const bool carried = cell.type == 0xe || cell.type == 0x4b || cell.type == 0x4c;
				u->second.flags |= fromBelow;
				if(carried)
				{
					if(u->second.type == 8)
						u->second.type = 0x4b;
					else if(u->second.type == 9)
						u->second.type = 0x4c;
				}
				std::map<uint32_t, PathCell>::iterator uh = p.cells.find(upKey);
				if(uh == p.cells.end())
					continue;
				uh->second.stairFlags |= fromBelow;
				if(carried)
				{
					if(uh->second.type == 8)
						uh->second.type = 0x4b;
					else if(uh->second.type == 9)
						uh->second.type = 0x4c;
				}
			}
			else if(mode == 1)
			{
				if(m == 0xf)
				{
					LatSet(lattice, x * 0x100 + y, 0xf);
					RemovePathableCell(x, y, deck);
					if(LatAt(lattice, x * 0x100 + y + 0x101) == 0xf)
						DisconnectCells(deck, x + 1, y, 0x2000, x, y + 1, 0x20000);
					if(LatAt(lattice, (x - 1) * 0x100 + 1 + y) == 0xf)
						DisconnectCells(deck, x - 1, y, 0x800, x, y + 1, 0x8000);
					if(LatAt(lattice, (x - 1) * 0x100 - 1 + y) == 0xf)
						DisconnectCells(deck, x - 1, y, 0x20000, x, y - 1, 0x2000);
					if(LatAt(lattice, x * 0x100 + 0xff + y) == 0xf)
						DisconnectCells(deck, x + 1, y, 0x8000, x, y - 1, 0x800);
				}
				else if(code >= 100)
				{
					const uint32_t key = MakeKey(x, y, deck);
					std::map<uint32_t, PathableCell>::iterator c = p.pathable.find(key);
					if(c == p.pathable.end())
						continue;
					std::map<uint32_t, PathCell>::iterator h = p.cells.find(key);
					uint8_t to;
					if(c->second.type == 2) to = 0xe;
					else if(c->second.type == 8) to = 0x4b;
					else if(c->second.type == 9) to = 0x4c;
					else continue;
					LatSet(lattice, x * 0x100 + y, to);
					c->second.type = to;
					if(h != p.cells.end())
						h->second.type = to;
				}
			}
		}
}

void Builder::DisconnectInaccessibleConnections(std::map<uint32_t, PathCell>::iterator hub)
{
	std::map<uint32_t, PathableCell>::const_iterator a = p.pathable.find(hub->first);
	std::map<uint32_t, uint64_t>& links = hub->second.links;
	for(std::map<uint32_t, uint64_t>::iterator l = links.begin(); l != links.end(); )
	{
		std::map<uint32_t, PathCell>::iterator other = p.cells.find(l->first);
		if(other == p.cells.end())
		{
			links.erase(l++);
			continue;
		}
		if(a == p.pathable.end() || !TestConnection(a->first, a->second, other->first, other->second))
		{
			other->second.links.erase(hub->first);
			links.erase(l++);
			continue;
		}
		++l;
	}
}

uint16_t SharedRoomId(const PathableCell& a, const PathableCell& b)
{
	const bool pa = a.door < 0 && a.hole < 0 && a.railing < 0;
	const bool pb = b.door < 0 && b.hole < 0 && b.railing < 0;
	if(pa)
	{
		if(pb)
			return a.room1 == b.room1 ? a.room1 : 0xffff;
		return (a.room1 == b.room1 || a.room1 == b.room2) ? a.room1 : 0xffff;
	}
	if(pb)
		return (a.room1 == b.room1 || a.room2 == b.room1) ? b.room1 : 0xffff;
	if(a.room1 == b.room1 || a.room1 == b.room2)
		return a.room1;
	if(a.room2 == b.room1 || a.room2 == b.room2)
		return a.room2;
	return 0xffff;
}

uint16_t SharedRoomIdExcept(const PathableCell& a, const PathableCell& b, uint16_t except)
{
	const bool pa = a.door < 0 && a.hole < 0 && a.railing < 0;
	const bool pb = b.door < 0 && b.hole < 0 && b.railing < 0;
	if(pa)
	{
		if(a.room1 == except)
			return 0xffff;
		if(pb)
			return a.room1 == b.room1 ? a.room1 : 0xffff;
		return (a.room1 == b.room1 || a.room1 == b.room2) ? a.room1 : 0xffff;
	}
	if(pb)
	{
		if(a.room1 == except)
			return 0xffff;
		return (a.room1 == b.room1 || a.room2 == b.room1) ? b.room1 : 0xffff;
	}
	if(a.room1 != except && (a.room1 == b.room1 || a.room1 == b.room2))
		return a.room1;
	if(a.room2 == except)
		return 0xffff;
	if(a.room2 == b.room1 || a.room2 == b.room2)
		return a.room2;
	return 0xffff;
}

uint32_t Builder::FindBottomStep(uint32_t key)
{
	const int8_t deck = (int8_t)KeyDeck(key);
	bool seen = false;
	uint32_t cur = key;
	for(int guard = 0; ; guard++)
	{
		if(guard == 0x20000)
			return cur;
		const PathableCell& c = p.pathable[cur];
		if(c.flags & 0x1000)
			return cur;
		const Dir& d = Dirs[c.stairDir & 7];
		if(!(c.dirs & d.bit))
			return cur;
		const uint32_t nk = MakeKey(KeyX(cur) + d.dx, KeyY(cur) + d.dy, deck);
		std::map<uint32_t, PathableCell>::const_iterator n = p.pathable.find(nk);
		if(n == p.pathable.end())
			return cur;
		if(n->second.flags & 0x2000)
		{
			if(seen)
				return cur;
			seen = true;
		}
		cur = nk;
	}
}

bool Builder::TestConnection(uint32_t aKey, const PathableCell& a, uint32_t bKey, const PathCell& b)
{
	const int8_t da = (int8_t)KeyDeck(aKey), db = (int8_t)KeyDeck(bKey);
	if(db > da)
	{
		uint32_t c = MakeKey(KeyX(bKey), KeyY(bKey), da);
		std::map<uint32_t, PathableCell>::const_iterator ci = p.pathable.find(c);
		if(ci == p.pathable.end())
			return true;
		if(ci->second.flags & 0x8000)
			c = FindBottomStep(c);
		return DoesStairlessPathExist(b.room2, aKey, c, (a.flags & 0x8000) ? 3 : 2);
	}
	if(db < da)
	{
		uint32_t c = MakeKey(KeyX(aKey), KeyY(aKey), db);
		std::map<uint32_t, PathableCell>::const_iterator ci = p.pathable.find(c);
		if(ci == p.pathable.end())
			return true;
		if(ci->second.flags & 0x8000)
			c = FindBottomStep(c);
		const PathableCell& d = p.pathable[bKey];
		return DoesStairlessPathExist(a.room2, c, bKey, (d.flags & 0x8000) ? 3 : 2);
	}
	const PathableCell& bc = p.pathable[bKey];
	const uint16_t shared = SharedRoomId(a, bc);
	uint16_t except = SharedRoomIdExcept(a, bc, shared);
	bool below = false;
	if(except == 0xffff && (a.flags & 0x1c) && (bc.flags & 0x1c) && shared == a.room2 && shared == bc.room2)
	{
		except = shared;
		below = true;
	}
	else if(shared != 0xffff && DoesStairlessPathExist(shared, aKey, bKey, 3))
		return true;
	if(except == 0xffff)
		return false;
	if(!below)
		return DoesStairlessPathExist(except, aKey, bKey, 2);
	uint32_t e = MakeKey(KeyX(aKey), KeyY(aKey), (int8_t)(da - 1));
	uint32_t f = MakeKey(KeyX(bKey), KeyY(bKey), (int8_t)(da - 1));
	if(!p.pathable.count(e) || !p.pathable.count(f))
		return true;
	if(p.pathable[e].flags & 0x8000)
		e = FindBottomStep(e);
	if(p.pathable[f].flags & 0x8000)
		f = FindBottomStep(f);
	return DoesStairlessPathExist(except, e, f, 2);
}

bool Builder::DoesStairlessPathExist(uint16_t room, uint32_t fromKey, uint32_t toKey, int mode)
{
	if(fromKey == toKey)
		return true;
	const PathableCell& to = p.pathable[toKey];
	std::map<uint32_t, Accessible> acc;
	Accessible start;
	start.cost = 0;
	start.flags = to.flags;
	start.stairDir = to.stairDir & 0x77;
	acc[toKey] = start;
	std::set<uint32_t> visited;
	CellToDoMap todo;
	AccessibleCells(acc, toKey, to.dirs, room, visited, todo, true, mode, 0);
	if(acc.size() == 1)
		return true;
	while(!todo.empty())
	{
		CellToDoMap wave;
		wave.swap(todo);
		for(CellToDoMap::const_iterator it = wave.begin(); it != wave.end(); ++it)
		{
			if(it->second.second == fromKey)
				return true;
			AccessibleCells(acc, it->second.second, it->second.first, room, visited, todo, true, mode, 0);
		}
	}
	return false;
}

bool Builder::AccessibleCells(std::map<uint32_t, Accessible>& acc, uint32_t key, uint32_t connect, uint16_t room,
		std::set<uint32_t>& visited, CellToDoMap& todo, bool skipOpen, int mode, int8_t deck)
{
	const uint16_t curFlags = acc[key].flags;
	bool result = false;
	for(int k = 0; k < 26; k++)
	{
		if(!(connect & Dirs[k].bit))
			continue;
		const uint32_t nk = Neighbour(key, Dirs[k]);
		const int8_t nd = (int8_t)(KeyDeck(key) + Dirs[k].dz);
		if(visited.count(nk))
			continue;
		std::map<uint32_t, PathableCell>::const_iterator pit = p.pathable.find(nk);
		if(pit == p.pathable.end())
			continue;
		const PathableCell& pc = pit->second;
		if(pc.door < 0 && pc.hole < 0 && pc.railing < 0)
		{
			if(pc.room1 != room)
				continue;
		}
		else if(pc.room1 != room && pc.room2 != room)
			continue;
		const uint16_t f = pc.flags;
		if((f & 0xff10) == 0x10)
			continue;
		if(mode == 1)
		{
			if(!(f & 0xf000) || nd != deck)
				continue;
			if(f & 0x1000)
			{
				if(Dirs[k].dz != 0)
					continue;
				result = true;
			}
		}
		else if(mode == 2)
		{
			if(Dirs[k].dz != 0 || (f & 0xc000))
				continue;
		}
		else if(mode == 3)
		{
			if(Dirs[k].dz != 0 || (f & 0x6000))
				continue;
		}
		else if(mode == 4)
		{
			if(nd != deck && (!(curFlags & 0xc000) || !(f & 0x800)))
				continue;
			if(!TestStepOk(curFlags, f, Dirs[k].dz))
				continue;
		}
		else if(!TestStepOk(curFlags, f, Dirs[k].dz))
			continue;
		switch(pc.type)
		{
		case 0x12:
			if(skipOpen)
				continue;
			break;
		case 3: case 4: case 9: case 0xa: case 0xb: case 0xc: case 0xd: case 0xe: case 0x11: case 0x4c:
			if(skipOpen && (f & 0x10) && !(f & 0xff00))
				continue;
			break;
		case 2: case 5: case 6: case 7: case 8: case 0x10: case 0x4b:
			break;
		default:
			continue;
		}
		const uint32_t cost = MoveCost(pc.type) + Dirs[k].cost + acc[key].cost;
		if(!acc.count(nk))
		{
			Accessible a;
			a.cost = cost;
			a.flags = f;
			a.stairDir = pc.stairDir & 0x77;
			acc[nk] = a;
		}
		AddCell(todo, cost, nk, pc.dirs);
	}
	visited.insert(key);
	return result;
}

}

void Path::CreateSpacecraft(const Design& design, const Openings& openings, StageObserver* observer)
{
	Builder b(*this, design, openings, observer);
	b.Run();
}

}
