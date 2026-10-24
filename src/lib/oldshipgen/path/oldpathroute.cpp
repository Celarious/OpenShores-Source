#include "oldpath.h"

#include <algorithm>
#include <cstring>

namespace OldPath
{

namespace
{

struct RDir
{
	int8_t		dx, dy, dz;
	uint32_t	bit;
	uint32_t	cost;
};

const RDir RD[26] = {
	{ 1, 0, 0, 0x400, 0xa }, { 1, 1, 0, 0x800, 0xe }, { 0, 1, 0, 0x1000, 0xa }, { -1, 1, 0, 0x2000, 0xe },
	{ -1, 0, 0, 0x4000, 0xa }, { -1, -1, 0, 0x8000, 0xe }, { 0, -1, 0, 0x10000, 0xa }, { 1, -1, 0, 0x20000, 0xe },
	{ 0, 0, 1, 0x40000, 0xc },
	{ 1, 0, 1, 0x80000, 0x1c }, { 1, 1, 1, 0x100000, 0x22 }, { 0, 1, 1, 0x200000, 0x1c }, { -1, 1, 1, 0x400000, 0x22 },
	{ -1, 0, 1, 0x800000, 0x1c }, { -1, -1, 1, 0x1000000, 0x22 }, { 0, -1, 1, 0x2000000, 0x1c },
	{ 1, -1, 1, 0x4000000, 0x22 },
	{ 0, 0, -1, 0x1, 0xb },
	{ 1, 0, -1, 0x2, 0x1c }, { 1, 1, -1, 0x4, 0x22 }, { 0, 1, -1, 0x8, 0x1c }, { -1, 1, -1, 0x10, 0x22 },
	{ -1, 0, -1, 0x20, 0x1c }, { -1, -1, -1, 0x40, 0x22 }, { 0, -1, -1, 0x80, 0x1c }, { 1, -1, -1, 0x100, 0x22 },
};

inline uint32_t Neighbour(uint32_t key, int k)
{
	return MakeKey(KeyX(key) + RD[k].dx, KeyY(key) + RD[k].dy, KeyDeck(key) + RD[k].dz);
}

inline bool IsOpening(const PathableCell& c)
{
	return c.door >= 0 || c.hole >= 0 || c.railing >= 0;
}

inline bool InRoom(const PathableCell& c, uint16_t room)
{
	if(!IsOpening(c))
		return c.room1 == room;
	return c.room1 == room || c.room2 == room;
}

inline Point LocPoint(uint32_t key)
{
	Point p = CellCenter(KeyX(key), KeyY(key), KeyDeck(key));
	p.x = p.x + 0.0;
	p.y = p.y + 0.0;
	p.z = p.z + 4.0;
	return p;
}

inline RouteLoc CellLoc(uint32_t key, const PathableCell& c)
{
	return RouteLoc(LocPoint(key), c.door, c.hole, c.railing, key);
}

bool DoesShareRoom(const PathableCell& a, const PathableCell& b)
{
	if(!IsOpening(a))
	{
		if(!IsOpening(b))
			return a.room1 == b.room1;
		return a.room1 == b.room1 || a.room1 == b.room2;
	}
	if(IsOpening(b))
		return a.room1 == b.room1 || a.room1 == b.room2 || a.room2 == b.room1 || a.room2 == b.room2;
	return a.room1 == b.room1 || a.room2 == b.room1;
}

bool DoesShareRoomNotOutside(const PathableCell& a, const PathableCell& b)
{
	if(!IsOpening(a))
	{
		if(a.room1 == 0)
			return false;
		if(!IsOpening(b))
			return a.room1 == b.room1;
		return a.room1 == b.room1 || a.room1 == b.room2;
	}
	if(!IsOpening(b))
	{
		if(a.room1 != 0 && a.room1 == b.room1)
			return true;
		return a.room2 != 0 && a.room2 == b.room1;
	}
	if(a.room1 != 0 && (a.room1 == b.room1 || a.room1 == b.room2))
		return true;
	return a.room2 != 0 && (a.room2 == b.room1 || a.room2 == b.room2);
}

uint16_t SharedRoomId(const PathableCell& a, const PathableCell& b)
{
	if(!IsOpening(a))
	{
		if(!IsOpening(b))
			return a.room1 == b.room1 ? a.room1 : 0xffff;
		return a.room1 == b.room1 || a.room1 == b.room2 ? a.room1 : 0xffff;
	}
	if(!IsOpening(b))
		return a.room1 == b.room1 || a.room2 == b.room1 ? b.room1 : 0xffff;
	if(a.room1 == b.room1 || a.room1 == b.room2)
		return a.room1;
	if(a.room2 == b.room1 || a.room2 == b.room2)
		return a.room2;
	return 0xffff;
}

uint16_t SharedRoomIdExcept(const PathableCell& a, const PathableCell& b, uint16_t except)
{
	if(!IsOpening(a))
	{
		if(a.room1 == except)
			return 0xffff;
		if(!IsOpening(b))
			return a.room1 == b.room1 ? a.room1 : 0xffff;
		return a.room1 == b.room1 || a.room1 == b.room2 ? a.room1 : 0xffff;
	}
	if(!IsOpening(b))
	{
		if(a.room1 == except)
			return 0xffff;
		return a.room1 == b.room1 || a.room2 == b.room1 ? b.room1 : 0xffff;
	}
	if(a.room1 != except && (a.room1 == b.room1 || a.room1 == b.room2))
		return a.room1;
	if(a.room2 != except && (a.room2 == b.room1 || a.room2 == b.room2))
		return a.room2;
	return 0xffff;
}

bool InVia(const std::set<uint32_t>& via, uint16_t room)
{
	for(std::set<uint32_t>::const_iterator it = via.begin(); it != via.end(); ++it)
		if((uint16_t)*it == room)
			return true;
	return false;
}

bool CanRouteViaRoom1(const PathCell& h, uint16_t room)
{
	return room == h.room1 || InVia(h.via1, room);
}

bool CanRouteViaRoom2(const PathCell& h, uint16_t room)
{
	return room == h.room2 || InVia(h.via2, room);
}

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

typedef std::multimap<uint32_t, std::vector<uint32_t> > PathCellToDoMap;

void AddPathCell(PathCellToDoMap& todo, uint32_t cost, uint32_t key, const std::vector<uint32_t>& path)
{
	for(PathCellToDoMap::iterator it = todo.begin(); it != todo.end(); ++it)
		if(it->second.back() == key)
		{
			if(it->first <= cost)
				return;
			todo.erase(it);
			break;
		}
	std::vector<uint32_t> p(path);
	p.push_back(key);
	todo.insert(todo.lower_bound(cost), std::make_pair(cost, p));
}

const uint32_t StairEntry[8] = { 0x31c00, 0x23c00, 0x7c00, 0xf800, 0x1f000, 0x3e000, 0x3c400, 0x38c00 };

int StairStepDir(int dir, uint32_t dirs)
{
	if(RD[dir].bit & dirs)
		return dir;
	int d = dir - 1;
	if(d < 0)
		d = 7;
	if(RD[d].bit & dirs)
		return d;
	d = dir + 1 < 8 ? dir + 1 : 0;
	return (RD[d].bit & dirs) ? d : dir;
}

typedef std::set<uint32_t> Visited;

}

RouteLoc::RouteLoc(const Point& p, int door, int hole, int railing, uint32_t key)
	: pt(p), cell(key), index(-1), kind(0)
{
	if(door >= 0)
	{
		index = door;
		kind = 1;
	}
	else if(hole >= 0)
	{
		index = hole;
		kind = 2;
	}
	else if(railing >= 0)
	{
		index = railing;
		kind = 3;
	}
	else if(door == -2)
		kind = 4;
	else if(door == -3)
		kind = 5;
}

bool RouteLoc::operator==(const RouteLoc& o) const
{
	return std::memcmp(&pt.x, &o.pt.x, 8) == 0 && std::memcmp(&pt.y, &o.pt.y, 8) == 0 &&
		std::memcmp(&pt.z, &o.pt.z, 8) == 0 && cell == o.cell && index == o.index && kind == o.kind;
}

bool Route::ContainsCellHash(uint32_t key) const
{
	for(size_t i=0; i<locs.size(); i++)
		if(locs[i].cell == key)
			return true;
	return false;
}

int Dice::Roll(unsigned n, unsigned sides, int add)
{
	if(n == 0)
		return 0;
	const double k = -2.3283064365386963e-10;
	unsigned sum = 0;
	for(unsigned i=0; i<n; i++)
	{
		seed = seed * 0x41c64e6du + 0x3039u;
		sum += 1 - (int)((double)seed * (double)sides * k);
	}
	unsigned r = 0;
	if(sides != 0)
		r = sides == 1 ? n : sum;
	return add == 0 ? (int)r : (int)(r + add);
}

uint32_t Path::MoveCost(uint8_t type)
{
	static const int32_t T[18] = { -1, -1, 0, 30, 30, 0, 0, 0, 20, 20, 20, 50, 40, 20, 150, -1, 10, 100 };
	if(type < 18)
		return (uint32_t)T[type];
	return type == 0x4b || type == 0x4c ? 150 : 0;
}

namespace
{

bool AccessibleCells(const Path& p, std::map<uint32_t, AccessibleCell>& acc, uint32_t key, uint32_t connect,
	uint16_t room, Visited& visited, CellToDoMap& todo, bool skipOpen, int mode, int8_t deck)
{
	const uint16_t curFlags = acc[key].flags;
	bool result = false;
	for(int k = 0; k < 26; k++)
	{
		if(!(connect & RD[k].bit))
			continue;
		const uint32_t nk = Neighbour(key, k);
		const int8_t nd = (int8_t)(KeyDeck(key) + RD[k].dz);
		if(visited.count(nk))
			continue;
		const PathableCell& pc = p.PathableOrEnd(nk);
		if(!InRoom(pc, room))
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
				if(RD[k].dz != 0)
					continue;
				result = true;
			}
		}
		else if(mode == 2)
		{
			if(RD[k].dz != 0 || (f & 0xc000))
				continue;
		}
		else if(mode == 3)
		{
			if(RD[k].dz != 0 || (f & 0x6000))
				continue;
		}
		else if(mode == 4)
		{
			if(nd != deck && (!(curFlags & 0xc000) || !(f & 0x800)))
				continue;
			if(!Path::TestStepOk(curFlags, f, RD[k].dz))
				continue;
		}
		else if(!Path::TestStepOk(curFlags, f, RD[k].dz))
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
		const uint32_t cost = Path::MoveCost(pc.type) + RD[k].cost + acc[key].cost;
		if(!acc.count(nk))
		{
			AccessibleCell a;
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

void AccessibleSpacecraftNeighbors(const Path& p, std::map<uint32_t, uint32_t>& hubs,
	std::map<uint32_t, AccessibleCell>& acc, uint32_t key, uint32_t connect, uint16_t room, Visited& visited,
	CellToDoMap& todo, const DoorStates& doors, bool crew, bool ignoreDoors, bool railings)
{
	const uint16_t curFlags = acc[key].flags;
	for(int k = 0; k < 26; k++)
	{
		if(!(connect & RD[k].bit))
			continue;
		const uint32_t nk = Neighbour(key, k);
		if(visited.count(nk))
			continue;
		const PathableCell& pc = p.PathableOrEnd(nk);
		if(!InRoom(pc, room))
			continue;
		if((pc.flags & 0xff10) == 0x10)
			continue;
		if(!Path::TestStepOk(curFlags, pc.flags, RD[k].dz))
			continue;
		bool hub;
		switch(pc.type)
		{
		case 2: case 0xc: case 0xd: case 0xe: case 0x10: case 0x11:
			hub = false;
			break;
		case 3: case 5:
			if(!crew && (pc.flags & 2) && !doors.IsDoorOpen((unsigned)(int)pc.door))
				continue;
			hub = true;
			break;
		case 4: case 6: case 7:
			if(!ignoreDoors && !doors.IsDoorOpen((unsigned)(int)pc.door))
				continue;
			hub = true;
			break;
		case 8: case 9: case 0xa: case 0xb: case 0x4b: case 0x4c:
			hub = true;
			break;
		case 0x12:
			if(!railings)
				continue;
			hub = true;
			break;
		default:
			continue;
		}
		const uint32_t cost = Path::MoveCost(pc.type) + RD[k].cost + acc[key].cost;
		if(!acc.count(nk))
		{
			AccessibleCell a;
			a.cost = cost;
			a.flags = pc.flags;
			a.stairDir = pc.stairDir & 0x77;
			acc[nk] = a;
		}
		if(hub && !hubs.count(nk) && p.cells.count(nk))
			hubs[nk] = cost;
		AddCell(todo, cost, nk, pc.dirs);
	}
	visited.insert(key);
}

void AccessibleSpacecraftDoorsInRoom(const Path& p, std::map<uint32_t, uint32_t>& hubs, uint32_t key, uint32_t connect,
	uint16_t flags, uint16_t room, const DoorStates& doors, bool crew, bool ignoreDoors, bool railings)
{
	std::map<uint32_t, AccessibleCell> acc;
	AccessibleCell start;
	start.flags = flags;
	acc[key] = start;
	Visited visited;
	CellToDoMap todo;
	AccessibleSpacecraftNeighbors(p, hubs, acc, key, connect, room, visited, todo, doors, crew, ignoreDoors, railings);
	while(!todo.empty())
	{
		CellToDoMap wave;
		wave.swap(todo);
		for(CellToDoMap::const_iterator it = wave.begin(); it != wave.end(); ++it)
			AccessibleSpacecraftNeighbors(p, hubs, acc, it->second.second, it->second.first, room, visited, todo, doors,
				crew, ignoreDoors, railings);
	}
}

void PathCellsSpacecraft(const Path& p, uint32_t fromKey, uint32_t toKey, Visited& visited, PathCellToDoMap& todo,
	bool crew, bool ignoreDoors, bool skipOpen, const DoorStates& doors, uint32_t base,
	const std::vector<uint32_t>& path)
{
	const PathCell& from = p.cells.find(fromKey)->second;
	const PathCell& to = p.cells.find(toKey)->second;
	for(std::map<uint32_t, uint64_t>::const_iterator l = from.links.begin(); l != from.links.end(); ++l)
	{
		if(visited.count(l->first))
			continue;
		const PathCell& h = p.cells.find(l->first)->second;
		if(l->first != toKey)
		{
			const bool has1 = from.room1 == h.room1 || from.room2 == h.room1;
			const bool has2 = from.room1 == h.room2 || from.room2 == h.room2;
			bool ok = true;
			if(has1 && has2)
				ok = CanRouteViaRoom1(h, to.room1) || CanRouteViaRoom2(h, to.room1);
			else if(has1)
				ok = CanRouteViaRoom2(h, to.room1);
			else if(has2)
				ok = CanRouteViaRoom1(h, to.room1);
			if(!ok)
				continue;
		}
		if(!crew && (h.stairFlags & 2) && !doors.IsDoorOpen((unsigned)(int)h.door))
			continue;
		if(!ignoreDoors && (uint8_t)(h.type - 4) < 4 && h.type != 5 && !doors.IsDoorOpen((unsigned)(int)h.door))
			continue;
		if(skipOpen)
		{
			const PathableCell& pc = p.PathableOrEnd(l->first);
			if((pc.flags & 0xff10) == 0x10 || pc.type == 0x12)
				continue;
		}
		AddPathCell(todo, base + (uint32_t)l->second, l->first, path);
	}
	visited.insert(fromKey);
}

}

void Path::AccessibleSpacecraftDoors(std::map<uint32_t, uint32_t>& hubs, uint32_t key, const DoorStates& doors,
	bool crew, bool ignoreDoors, bool railings) const
{
	const PathableCell& c = PathableOrEnd(key);
	AccessibleSpacecraftDoorsInRoom(*this, hubs, key, c.dirs, c.flags, c.room1, doors, crew, ignoreDoors, railings);
	if(IsOpening(c))
		AccessibleSpacecraftDoorsInRoom(*this, hubs, key, c.dirs, c.flags, c.room2, doors, crew, ignoreDoors,
			railings);
}

void Path::DiscardInaccessibleDoors(std::map<uint32_t, uint32_t>& hubs, const DoorStates& doors, bool crew,
	bool ignoreDoors, bool railings) const
{
	for(std::map<uint32_t, uint32_t>::iterator it = hubs.begin(); it != hubs.end(); )
	{
		std::map<uint32_t, PathCell>::const_iterator h = cells.find(it->first);
		bool drop = false;
		if(h != cells.end())
			switch(h->second.type)
			{
			case 3: case 5:
				drop = !crew && (h->second.stairFlags & 2) && !doors.IsDoorOpen((unsigned)(int)h->second.door);
				break;
			case 4: case 6: case 7:
				drop = !ignoreDoors && !doors.IsDoorOpen((unsigned)(int)h->second.door);
				break;
			case 0x12:
				drop = !railings;
				break;
			default:
				break;
			}
		if(drop)
			hubs.erase(it++);
		else
			++it;
	}
}

uint32_t Path::ClosestHullEntranceKey(const Point& p, bool landed, bool ignoreDoors, const DoorStates& doors) const
{
	int x, y;
	HalfGridCoord(p, x, y);
	const int deck = OnDeck(p.z);
	int best = -1;
	uint32_t bestKey = NoCell;
	const int8_t low = (int8_t)flag;
	for(std::map<uint32_t, PathCell>::const_iterator it = cells.begin(); it != cells.end(); ++it)
	{
		const PathCell& h = it->second;
		const int hd = KeyDeck(it->first);
		bool ok;
		if(landed)
		{
			if(h.type == 4 && hd == low)
				ok = ignoreDoors || doors.IsDoorOpen((unsigned)(int)h.door);
			else if(h.type == 6 && hd == low + 1)
				ok = ignoreDoors || doors.IsDoorOpen((unsigned)(int)h.door);
			else
				ok = h.type == 0xb && hd == low;
		}
		else
			ok = h.type < 0xc && ((0x8d0u >> h.type) & 1) &&
				(ignoreDoors || !(h.stairFlags & 0x20) || doors.IsDoorOpen((unsigned)(int)h.door));
		if(!ok)
			continue;
		const int dx = x - KeyX(it->first), dy = y - KeyY(it->first), dd = deck - hd;
		const int d = dd * dd + dy * dy + dx * dx;
		if(best < 0 || d < best)
		{
			bestKey = it->first;
			best = d;
			if(d == 0)
				return bestKey;
		}
	}
	return bestKey;
}

void Path::ClosestHullEntrance(const Point& p, bool landed, bool ignoreDoors, const DoorStates& doors, Point& out,
	int& door, int& hole) const
{
	const uint32_t k = ClosestHullEntranceKey(p, landed, ignoreDoors, doors);
	if(k == NoCell)
	{
		door = -1;
		hole = -1;
		return;
	}
	const PathCell& h = cells.find(k)->second;
	out = CellCenter(KeyX(k), KeyY(k), KeyDeck(k));
	door = h.door;
	hole = h.hole;
}

uint32_t Path::SpacecraftPathTerminalInfo(const Point& p, bool landed, bool ignoreDoors, const DoorStates& doors,
	bool& outside) const
{
	const uint32_t k = MakeKey(p);
	if(FindPathable(k))
	{
		outside = false;
		return k;
	}
	if(FindExtant(k))
	{
		outside = false;
		return NoCell;
	}
	outside = true;
	const uint32_t h = ClosestHullEntranceKey(p, landed, ignoreDoors, doors);
	if(h == NoCell || !FindPathable(h))
		return NoCell;
	return h;
}

uint32_t Path::ClosestPathableSpacecraft(const Point& p, const RoomArray& rooms, const DoorStates& doors, bool crew)
	const
{
	int x, y;
	HalfGridCoord(p, x, y);
	const int8_t deck = (int8_t)OnDeck(p.z);
	uint16_t room = 0;
	InsideRoom(p, deck, rooms, room);
	uint32_t best = NoCell;
	double bestD = 0.0;
	for(int k = 0; k < 8; k++)
	{
		const uint32_t nk = MakeKey(x + RD[k].dx, y + RD[k].dy, deck);
		const PathableCell* c = FindPathable(nk);
		if(!c || !InRoom(*c, room))
			continue;
		if(!((c->flags & 8) || !(c->flags & 0x20) || doors.IsDoorOpen((unsigned)(int)c->door) ||
				(c->type != 4 && (crew || !(c->flags & 2)))))
			continue;
		const Point q = CellCenter(KeyX(nk), KeyY(nk), KeyDeck(nk));
		const double dy = p.y - q.y, dx = p.x - q.x;
		const double d = dy * dy + dx * dx;
		if(best == NoCell || (d <= bestD && bestD != d))
		{
			best = nk;
			bestD = d;
		}
	}
	return best;
}

bool Path::StepToPathableSpacecraft(Route& route, const Point& p, const RoomArray& rooms, const DoorStates& doors,
	bool crew) const
{
	if(!FindExtant(MakeKey(p)))
		return false;
	const uint32_t c = ClosestPathableSpacecraft(p, rooms, doors, crew);
	if(c == NoCell)
		return false;
	const RouteLoc loc = CellLoc(c, *FindPathable(c));
	if(!route.locs.empty())
	{
		if(route.ContainsCellHash(c))
			return false;
		route.locs.insert(route.locs.begin(), loc);
		return true;
	}
	route.locs.push_back(loc);
	return true;
}

uint32_t Path::NextStep(uint32_t cur, uint32_t prev, const std::map<uint32_t, AccessibleCell>& acc, bool axesFirst,
	bool flat) const
{
	const PathableCell& c = PathableOrEnd(cur);
	int best = -1;
	uint32_t bestCost = 0;
	uint32_t bestKey = NoCell;

	struct Try
	{
		static void Dir(int k, uint32_t cur, uint32_t prev, const PathableCell& c,
			const std::map<uint32_t, AccessibleCell>& acc, bool flat, int& best, uint32_t& bestCost, uint32_t& bestKey)
		{
			if(!(RD[k].bit & c.dirs))
				return;
			const uint32_t nk = Neighbour(cur, k);
			if(prev != NoCell && prev == nk)
				return;
			std::map<uint32_t, AccessibleCell>::const_iterator a = acc.find(nk);
			if(a == acc.end())
				return;
			if(flat && !(c.flags & 0xff00) && (a->second.flags & 0xff00))
				return;
			const int8_t dz = RD[k].dz;
			if(dz == 0 && ((c.flags & 0xf000) || (a->second.flags & 0xf000)))
			{
				if(!Path::TestStepOk(c.flags, a->second.flags, 0))
					return;
				if(!(c.flags & 0xf000) && (a->second.flags & 0x1000) &&
						!(StairEntry[(a->second.stairDir >> 4) & 7] & RD[k].bit))
					return;
			}
			if(best < 0 || a->second.cost < bestCost || (a->second.cost == bestCost && dz != 0))
			{
				best = k;
				bestCost = a->second.cost;
				bestKey = nk;
			}
		}
		static void Plain(int k, uint32_t cur, uint32_t prev, const PathableCell& c,
			const std::map<uint32_t, AccessibleCell>& acc, int& best, uint32_t& bestCost, uint32_t& bestKey)
		{
			if(!(RD[k].bit & c.dirs))
				return;
			const uint32_t nk = Neighbour(cur, k);
			if(prev != NoCell && prev == nk)
				return;
			std::map<uint32_t, AccessibleCell>::const_iterator a = acc.find(nk);
			if(a == acc.end())
				return;
			if(best < 0 || a->second.cost < bestCost || (a->second.cost == bestCost && RD[k].dz != 0))
			{
				best = k;
				bestCost = a->second.cost;
				bestKey = nk;
			}
		}
	};

	if(!axesFirst)
	{
		for(int k = 0; k < 9; k++)
			Try::Dir(k, cur, prev, c, acc, flat, best, bestCost, bestKey);
	}
	else
	{
		for(int k = 0; k < 9; k += 2)
			Try::Dir(k, cur, prev, c, acc, flat, best, bestCost, bestKey);
		std::map<uint32_t, AccessibleCell>::const_iterator self = acc.find(cur);
		if(self != acc.end() && (best < 0 || self->second.cost <= bestCost))
			for(int k = 1; k < 9; k += 2)
				Try::Dir(k, cur, prev, c, acc, flat, best, bestCost, bestKey);
	}
	if(RD[17].bit & c.dirs)
	{
		const uint32_t nk = Neighbour(cur, 17);
		if(prev == NoCell || prev != nk)
		{
			std::map<uint32_t, AccessibleCell>::const_iterator a = acc.find(nk);
			if(a != acc.end() && (best < 0 || a->second.cost <= bestCost))
				return FindPathable(nk) ? nk : NoCell;
		}
	}
	if(best < 0)
	{
		for(int k = 9; k < 17; k++)
			Try::Plain(k, cur, prev, c, acc, best, bestCost, bestKey);
		if(best < 0)
		{
			for(int k = 18; k < 26; k++)
				Try::Plain(k, cur, prev, c, acc, best, bestCost, bestKey);
			if(best < 0)
				return NoCell;
		}
	}
	return FindPathable(bestKey) ? bestKey : NoCell;
}

void Path::RouteBacktrack(Route& route, uint16_t room, const RoomArray& rooms,
	std::map<uint32_t, AccessibleCell>& acc, uint32_t a, int8_t deckA, uint32_t b, int8_t deckB) const
{
	bool axesFirst = false;
	if(room != 0)
	{
		const RoomOutline* r = rooms.Find(room);
		if(r && (r->type == 8 || r->type == 0x10 || r->type == 0x11 || r->type == 0x15))
			axesFirst = true;
	}
	const PathableCell& B = PathableOrEnd(b);
	const bool flat = deckA == deckB && !(B.flags & 0xff00);
	const PathableCell& A = PathableOrEnd(a);
	bool started = false;
	uint32_t pdx = 0, pdy = 0;
	uint32_t lx = KeyX(a), ly = KeyY(a);
	int8_t ldeck = deckA;
	uint16_t lflags = A.flags;
	uint32_t prev = NoCell, cur = a;
	if(a == b)
		return;
	uint32_t next;
	do
	{
		if(route.locs.size() > 9)
			return;
		next = NextStep(cur, prev, acc, axesFirst, flat);
		if(next == NoCell)
			return;
		acc[next].cost += 1000;
		const PathableCell& N = *FindPathable(next);
		const Point pt = LocPoint(next);
		const uint32_t nx = KeyX(next), ny = KeyY(next);
		const uint32_t dx = nx - lx, dy = ny - ly;
		const int door = (deckB <= deckA || !(N.flags & 0xf000)) ? N.door : -3;
		const int8_t nd = (int8_t)KeyDeck(next);
		const RouteLoc loc(pt, door, N.hole, N.railing, next);
		if(started && dx == pdx && dy == pdy && nd == ldeck && (int16_t)N.flags == (int16_t)lflags)
			route.locs.back() = loc;
		else
			route.locs.push_back(loc);
		started = true;
		pdx = dx;
		pdy = dy;
		lx = nx;
		ly = ny;
		lflags = N.flags;
		prev = cur;
		cur = next;
		ldeck = nd;
	}
	while(next != b);
}

void Path::RouteOnStairs(Route& route, const Point& from, uint32_t a, int8_t deckA, int8_t deckB) const
{
	const PathableCell& A = PathableOrEnd(a);
	const uint16_t f = A.flags;
	if(f & 0xf000)
	{
		const double h = (from.z - (double)(int)deckA * 8.0) * 0.125;
		if(!(((int16_t)f >= 0 || h < 0.75) && (!(f & 0x4000) || h < 0.5) && (!(f & 0x2000) || h < 0.25) &&
				!(f & 0x1000)))
		{
			bool up = deckA < deckB;
			for(size_t i=0; i<route.locs.size(); i++)
				if((int8_t)(route.locs[i].cell >> 16) == deckA && !(PathableOrEnd(route.locs[i].cell).flags & 0xf000))
				{
					up = false;
					break;
				}
			const int dir = StairStepDir(up ? (A.stairDir >> 4) & 7 : A.stairDir & 7, A.dirs);
			const uint16_t nx = (uint16_t)(KeyX(a) + RD[dir].dx);
			const uint8_t ny = (uint8_t)(KeyY(a) + RD[dir].dy);
			route.locs.clear();
			RouteLoc loc;
			loc.cell = ((uint32_t)(int32_t)deckA << 16) | ny | (uint16_t)(nx << 8);
			loc.pt = LocPoint(loc.cell);
			loc.index = -1;
			loc.kind = up ? 5 : 0;
			route.locs.push_back(loc);
			return;
		}
	}
	if(!(f & 0xf00))
		return;
	bool down = deckB < deckA || route.locs.empty();
	for(size_t i=0; i<route.locs.size(); i++)
	{
		const int8_t d = (int8_t)(route.locs[i].cell >> 16);
		if(d < deckA)
		{
			down = true;
			break;
		}
		if(d == deckA && !(PathableOrEnd(route.locs[i].cell).flags & 0xf00))
			down = false;
	}
	const PathableCell& below = PathableOrEnd(MakeKey(KeyX(a), KeyY(a), (int8_t)(deckA - 1)));
	const int dir = StairStepDir(down ? below.stairDir & 7 : (below.stairDir >> 4) & 7, A.dirs);
	const uint16_t nx = (uint16_t)(KeyX(a) + RD[dir].dx);
	const uint8_t ny = (uint8_t)(KeyY(a) + RD[dir].dy);
	route.locs.clear();
	RouteLoc loc;
	loc.cell = ((uint32_t)(int32_t)deckA << 16) | ny | (uint16_t)(nx << 8);
	loc.pt = LocPoint(loc.cell);
	loc.index = -1;
	loc.kind = down ? 4 : 0;
	route.locs.push_back(loc);
}

bool Path::RouteAcrossSpacecraftRoom(Route& route, const Point& from, uint32_t a, const Point& to, uint32_t b,
	bool crew, bool skipOpen, const DoorStates& doors, const RoomArray& rooms) const
{
	if(a == b)
		return true;
	const int8_t deckA = (int8_t)(a >> 16), deckB = (int8_t)(b >> 16);
	const PathableCell& A = PathableOrEnd(a);
	const PathableCell& B = PathableOrEnd(b);
	if(deckA == deckB)
	{
		std::vector<uint16_t> openList;
		bool onStairs = false;
		const PathTest r = TestSpacecraftPath(from, to, crew, rooms, doors, openList, false, onStairs);
		if(r == PathOk && !onStairs)
		{
			route.locs.push_back(RouteLoc(to, B.door, B.hole, B.railing, b));
			return true;
		}
	}
	uint16_t room = SharedRoomId(A, B);
	bool retried = false;
	for(;;)
	{
		int mode = 0;
		if(deckA < deckB && (B.flags & 0xc00))
			mode = 1;
		else if(deckB == deckA && IsOpening(A) && IsOpening(B) &&
				(!(A.flags & 0xc00) || !(B.flags & 0xc00) || room != A.room2) && (int16_t)A.flags >= 0)
			mode = 2;
		std::map<uint32_t, AccessibleCell> acc;
		AccessibleCell start;
		start.flags = B.flags;
		start.stairDir = (uint8_t)((((B.stairDir >> 4) & 7) << 4) | (B.stairDir & 7));
		acc[b] = start;
		Visited visited;
		CellToDoMap todo;
		if(AccessibleCells(*this, acc, b, B.dirs, room, visited, todo, skipOpen, mode, deckA))
			mode = 4;
		if(acc.size() == 1)
			RouteBacktrack(route, room, rooms, acc, a, deckA, b, deckB);
		else
			while(!todo.empty())
			{
				CellToDoMap wave;
				wave.swap(todo);
				for(CellToDoMap::const_iterator it = wave.begin(); it != wave.end(); ++it)
				{
					if(it->second.second == a)
					{
						RouteBacktrack(route, room, rooms, acc, a, deckA, b, deckB);
						todo.clear();
						break;
					}
					if(AccessibleCells(*this, acc, it->second.second, it->second.first, room, visited, todo, skipOpen,
							mode, deckA))
						mode = 4;
				}
			}
		bool retry = false;
		if(route.locs.empty() && !retried && IsOpening(A) && IsOpening(B))
		{
			room = SharedRoomIdExcept(A, B, room);
			if((uint16_t)(room - 1) < 0xfffe)
			{
				retried = true;
				retry = true;
			}
		}
		if(!retry)
		{
			RouteOnStairs(route, from, a, deckA, deckB);
			return !route.locs.empty();
		}
	}
}

int Path::PathCostSpacecraft(uint32_t from, uint32_t to, bool crew, bool ignoreDoors, const DoorStates& doors,
	bool skipOpen, std::vector<uint32_t>& hubPath) const
{
	if(from == to)
	{
		hubPath.clear();
		return 0;
	}
	if(!cells.count(from) || !cells.count(to))
	{
		hubPath.clear();
		return -1;
	}
	Visited visited;
	PathCellToDoMap todo;
	std::vector<uint32_t> start(1, from);
	PathCellsSpacecraft(*this, from, to, visited, todo, crew, ignoreDoors, skipOpen, doors, 0, start);
	while(!todo.empty())
	{
		PathCellToDoMap wave;
		wave.swap(todo);
		for(PathCellToDoMap::const_iterator it = wave.begin(); it != wave.end(); ++it)
		{
			if(it->second.back() == to)
			{
				hubPath = it->second;
				return (int)it->first;
			}
			PathCellsSpacecraft(*this, it->second.back(), to, visited, todo, crew, ignoreDoors, skipOpen, doors,
				it->first, it->second);
		}
	}
	hubPath.clear();
	return -1;
}

void Path::DistillBestSpacecraftPath(std::map<uint32_t, uint32_t>& hubs, uint32_t to, bool skipOpen,
	const DoorStates& doors, bool crew, bool ignoreDoors, std::vector<uint32_t>& hubPath) const
{
	int best = -1;
	uint32_t bestKey = 0;
	for(std::map<uint32_t, uint32_t>::const_iterator it = hubs.begin(); it != hubs.end(); ++it)
	{
		std::vector<uint32_t> path;
		const int c = PathCostSpacecraft(it->first, to, crew, ignoreDoors, doors, skipOpen, path);
		if(c < 0)
			continue;
		const int total = (int)it->second + c;
		if(best < 0 || total < best)
		{
			bestKey = it->first;
			best = total;
			hubPath = path;
		}
	}
	hubs.clear();
	if(best >= 0)
		hubs[bestKey] = (uint32_t)best;
}

bool Path::SpacecraftRoute(Route& route, const Point& from, const Point& to, const RoomArray& rooms, bool landed,
	bool skipOpen, const DoorStates& doors, bool crew, bool ignoreDoors, bool railings) const
{
	route.locs.clear();
	bool sOut = false, tOut = false;
	const uint32_t s = SpacecraftPathTerminalInfo(from, landed, ignoreDoors, doors, sOut);
	if(s == NoCell)
		return StepToPathableSpacecraft(route, from, rooms, doors, crew);
	uint32_t t = SpacecraftPathTerminalInfo(to, landed, true, doors, tOut);
	if(t == NoCell)
	{
		const uint32_t c = ClosestPathableSpacecraft(to, rooms, doors, crew);
		if(c == NoCell)
			return false;
		return SpacecraftRoute(route, from, CellCenter(KeyX(c), KeyY(c), KeyDeck(c)), rooms, landed, skipOpen, doors,
			crew, ignoreDoors, railings);
	}
	if(ignoreDoors && FindPathable(t)->type == 6)
	{
		t = MakeKey(KeyX(t), KeyY(t), (int8_t)(KeyDeck(t) - 1));
		if(!FindPathable(t))
			return false;
	}
	const PathableCell& S = *FindPathable(s);
	const PathableCell& T = *FindPathable(t);
	if(!sOut)
	{
		if(s == t)
		{
			if(tOut)
				route.locs.push_back(RouteLoc(to, -1, -1, -1, 0));
			return true;
		}
	}
	else
	{
		if(tOut)
		{
			route.locs.push_back(RouteLoc(to, -1, -1, -1, 0));
			return true;
		}
		route.locs.push_back(CellLoc(s, S));
	}
	if(!(T.flags & 8) && (T.flags & 0x20))
	{
		if(!(T.flags & 2))
		{
			if(T.type < 0x12 && ((0x30890u >> T.type) & 1) && !ignoreDoors &&
					!doors.IsDoorOpen((unsigned)(int)S.door))
				return false;
		}
		else if(!crew && !doors.IsDoorOpen((unsigned)(int)S.door))
			return false;
	}
	if(DoesShareRoomNotOutside(S, T))
	{
		if(KeyDeck(s) == KeyDeck(t))
		{
			if((S.flags & 8) || !(S.flags & 0x20) || crew || doors.IsDoorOpen((unsigned)(int)S.door))
			{
				if(RouteAcrossSpacecraftRoom(route, from, s, to, t, crew, skipOpen, doors, rooms))
				{
					if(tOut)
						route.locs.push_back(RouteLoc(to, -1, -1, -1, 0));
					return true;
				}
			}
			else if(InsideRoom(from, S, rooms) == InsideRoom(to, T, rooms) &&
					RouteAcrossSpacecraftRoom(route, from, s, to, t, false, skipOpen, doors, rooms))
			{
				if(tOut)
					route.locs.push_back(RouteLoc(to, -1, -1, -1, 0));
				return true;
			}
		}
		else if(RouteAcrossSpacecraftRoom(route, from, s, to, t, crew, skipOpen, doors, rooms))
			return true;
	}

	std::map<uint32_t, uint32_t> startHubs;
	std::map<uint32_t, PathCell>::const_iterator sh = cells.find(s);
	if(sh != cells.end())
	{
		for(std::map<uint32_t, uint64_t>::const_iterator l = sh->second.links.begin(); l != sh->second.links.end(); ++l)
			startHubs[l->first] = (uint32_t)l->second;
		DiscardInaccessibleDoors(startHubs, doors, crew, ignoreDoors, railings);
	}
	else
		AccessibleSpacecraftDoors(startHubs, s, doors, crew, ignoreDoors, railings);
	if(startHubs.empty())
		return false;

	uint32_t hub = NoCell;
	if(!route.hubs.empty() && route.target == t && route.skipOpen == skipOpen && route.crew == crew &&
			route.ignoreDoors == ignoreDoors && (crew || route.doors == doors))
	{
		hub = route.hubs.front();
		if(DoesShareRoom(S, PathableOrEnd(hub)))
			return RouteAcrossSpacecraftRoom(route, from, s, LocPoint(hub), hub, crew, skipOpen, doors, rooms);
	}
	route.hubs.clear();
	route.target = t;
	route.skipOpen = skipOpen;
	route.doors = doors;
	route.crew = crew;
	route.ignoreDoors = ignoreDoors;
	if(IsOpening(T) && cells.count(t))
	{
		DistillBestSpacecraftPath(startHubs, t, skipOpen, doors, crew, ignoreDoors, route.hubs);
		if(startHubs.empty())
		{
			route.hubs.clear();
			return false;
		}
		hub = startHubs.begin()->first;
		return RouteAcrossSpacecraftRoom(route, from, s, LocPoint(hub), hub, crew, skipOpen, doors, rooms);
	}
	std::map<uint32_t, uint32_t> targetHubs;
	std::map<uint32_t, PathCell>::const_iterator th = cells.find(t);
	if(th != cells.end())
	{
		for(std::map<uint32_t, uint64_t>::const_iterator l = th->second.links.begin(); l != th->second.links.end(); ++l)
			targetHubs[l->first] = (uint32_t)l->second;
		DiscardInaccessibleDoors(targetHubs, doors, crew, ignoreDoors, railings);
	}
	else
		AccessibleSpacecraftDoors(targetHubs, t, doors, crew, ignoreDoors, railings);
	bool found = false;
	uint16_t bestTotal = 0;
	for(std::map<uint32_t, uint32_t>::const_iterator it = targetHubs.begin(); it != targetHubs.end(); ++it)
	{
		std::map<uint32_t, uint32_t> tmp(startHubs);
		std::vector<uint32_t> path;
		DistillBestSpacecraftPath(tmp, it->first, skipOpen, doors, crew, ignoreDoors, path);
		if(tmp.empty())
			continue;
		const uint16_t total = (uint16_t)((int16_t)tmp.begin()->second + (int16_t)it->second);
		if(!found || total < bestTotal)
		{
			hub = tmp.begin()->first;
			bestTotal = total;
			route.hubs = path;
			found = true;
		}
	}
	if(found)
		return RouteAcrossSpacecraftRoom(route, from, s, LocPoint(hub), hub, crew, skipOpen, doors, rooms);
	route.hubs.clear();
	return false;
}

bool Path::RandomFloorLocation(Point& out, uint16_t room, Dice& dice) const
{
	std::vector<uint32_t> list;
	for(std::map<uint32_t, PathableCell>::const_iterator it = pathable.begin(); it != pathable.end(); ++it)
	{
		const PathableCell& c = it->second;
		if(!InRoom(c, room))
			continue;
		if(((c.flags & 8) || !(c.flags & 0x20)) && (c.room2 != 0 || !(c.flags & 8)) && !(c.flags & 0xff00))
			list.push_back(it->first);
	}
	if(list.empty())
		return false;
	const int i = dice.Roll(1, (unsigned)list.size(), -1);
	const uint32_t k = list[i];
	out = CellCenter(KeyX(k), KeyY(k), KeyDeck(k));
	return true;
}

}
