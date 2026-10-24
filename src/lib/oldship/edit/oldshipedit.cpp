#include "oldshipedit.h"

#include "oldbuildingedit.h"
#include "oldshiptypes.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <set>

namespace OldShip
{

namespace
{

GridPoint Pt(int x, int y)
{
	GridPoint p;
	p.x = x;
	p.y = y;
	return p;
}

int FloorDiv2(int v)
{
	return v >= 0 ? v / 2 : -((1 - v) / 2);
}

bool IsRoof(int roomType)
{
	return roomType >= 22 && roomType <= 24;
}

int Sign(int v)
{
	return v > 0 ? 1 : v < 0 ? -1 : 0;
}

long Area2(const std::vector<GridPoint>& o)
{
	long sum = 0;
	for(size_t i = 0; i < o.size(); i++)
	{
		const GridPoint& a = o[i];
		const GridPoint& b = o[(i + 1) % o.size()];
		sum += (long)a.x * b.y - (long)b.x * a.y;
	}
	return sum;
}

}

std::vector<GridPoint> FillShapeAt(double gx, double gy, int gridLines)
{
	std::vector<GridPoint> shape;
	gridLines &= GRID_All;
	const int i = (int)floor(gx / 2.0), j = (int)floor(gy / 2.0);
	const int x = i * 2, y = j * 2;
	const double u = gx - x, v = gy - y;
	const bool below45 = v < u;
	const bool below135 = u + v < 2.0;
	const int quarter = below45 ? (below135 ? 0 : 3) : (below135 ? 1 : 2);

#define P(dx, dy) shape.push_back(Pt(x + (dx), y + (dy)))
	switch(gridLines)
	{
	case GRID_Lateral | GRID_Longitudinal:
		P(0, 0); P(0, 2); P(2, 2); P(2, 0);
		break;
	case GRID_Lateral | GRID_Longitudinal | GRID_Diagonal45:
		if(below45) { P(0, 0); P(2, 2); P(2, 0); }
		else { P(0, 0); P(0, 2); P(2, 2); }
		break;
	case GRID_Lateral | GRID_Longitudinal | GRID_Diagonal135:
		if(below135) { P(2, 0); P(0, 0); P(0, 2); }
		else { P(2, 0); P(0, 2); P(2, 2); }
		break;
	case GRID_All:
		if(quarter == 0) { P(1, 1); P(2, 0); P(0, 0); }
		else if(quarter == 1) { P(1, 1); P(0, 0); P(0, 2); }
		else if(quarter == 2) { P(1, 1); P(0, 2); P(2, 2); }
		else { P(1, 1); P(2, 2); P(2, 0); }
		break;
	case GRID_Lateral | GRID_Diagonal45:
		if(below45) { P(0, 0); P(2, 2); P(4, 2); P(2, 0); }
		else { P(0, 0); P(-2, 0); P(0, 2); P(2, 2); }
		break;
	case GRID_Longitudinal | GRID_Diagonal45:
		if(below45) { P(0, 0); P(2, 2); P(2, 0); P(0, -2); }
		else { P(0, 0); P(0, 2); P(2, 4); P(2, 2); }
		break;
	case GRID_Lateral | GRID_Diagonal135:
		if(below135) { P(0, 2); P(2, 0); P(0, 0); P(-2, 2); }
		else { P(0, 2); P(2, 2); P(4, 0); P(2, 0); }
		break;
	case GRID_Longitudinal | GRID_Diagonal135:
		if(below135) { P(0, 2); P(2, 0); P(2, -2); P(0, 0); }
		else { P(0, 2); P(0, 4); P(2, 2); P(2, 0); }
		break;
	case GRID_Diagonal45 | GRID_Diagonal135:
		if(quarter == 0) { P(1, 1); P(2, 0); P(1, -1); P(0, 0); }
		else if(quarter == 1) { P(1, 1); P(0, 0); P(-1, 1); P(0, 2); }
		else if(quarter == 2) { P(1, 1); P(0, 2); P(1, 3); P(2, 2); }
		else { P(1, 1); P(2, 2); P(3, 1); P(2, 0); }
		break;
	case GRID_Lateral | GRID_Diagonal45 | GRID_Diagonal135:
		if(quarter == 0) { P(1, 1); P(2, 0); P(0, 0); }
		else if(quarter == 1) { P(1, 1); P(0, 0); P(-1, 1); P(0, 2); }
		else if(quarter == 2) { P(1, 1); P(0, 2); P(2, 2); }
		else { P(1, 1); P(2, 2); P(3, 1); P(2, 0); }
		break;
	case GRID_Longitudinal | GRID_Diagonal45 | GRID_Diagonal135:
		if(quarter == 0) { P(1, 1); P(2, 0); P(1, -1); P(0, 0); }
		else if(quarter == 1) { P(1, 1); P(0, 0); P(0, 2); }
		else if(quarter == 2) { P(1, 1); P(0, 2); P(1, 3); P(2, 2); }
		else { P(1, 1); P(2, 2); P(2, 0); }
		break;
	default:
		break;
	}
#undef P
	return shape;
}

bool PointInOutline(const std::vector<GridPoint>& o, double px, double py, bool orOnEdge)
{
	const size_t n = o.size();
	if(n < 3)
		return false;
	bool inside = false;
	for(size_t i = 0; i < n; i++)
	{
		const double ax = o[i].x, ay = o[i].y;
		const double bx = o[(i + 1) % n].x, by = o[(i + 1) % n].y;
		const double cross = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
		if(fabs(cross) < 1e-9 && px >= std::min(ax, bx) - 1e-9 && px <= std::max(ax, bx) + 1e-9
				&& py >= std::min(ay, by) - 1e-9 && py <= std::max(ay, by) + 1e-9)
			return orOnEdge;
		if((ay > py) != (by > py) && px < ax + (py - ay) * (bx - ax) / (by - ay))
			inside = !inside;
	}
	return inside;
}

DesignCounts Count(const Design& design)
{
	DesignCounts c;
	std::set<int> decks;
	c.rooms = (int)design.rooms.size();
	c.details = (int)design.details.size();
	for(size_t i = 0; i < design.rooms.size(); i++)
	{
		c.roomsByType[design.rooms[i].type]++;
		decks.insert(design.rooms[i].deck);
	}
	for(size_t i = 0; i < design.details.size(); i++)
		c.detailsByType[design.details[i].type]++;
	c.decks = (int)decks.size();
	return c;
}

//

void CellSet::Neighbours(int key, int out[3])
{
	const int q = key & 3;
	const int j = ((key >> 2) & 1023) - 256;
	const int i = (key >> 12) - 256;
	out[0] = Key(i, j, (q + 3) & 3);
	if(q == 0) out[1] = Key(i, j - 1, 2);
	else if(q == 1) out[1] = Key(i - 1, j, 3);
	else if(q == 2) out[1] = Key(i, j + 1, 0);
	else out[1] = Key(i + 1, j, 1);
	out[2] = Key(i, j, (q + 1) & 3);
}

bool CellSet::Has(int key) const
{
	return std::binary_search(m.begin(), m.end(), key);
}

bool CellSet::FromOutline(const std::vector<GridPoint>& o)
{
	m.clear();
	if(o.size() < 3)
		return false;
	int minX = o[0].x, maxX = o[0].x, minY = o[0].y, maxY = o[0].y;
	for(size_t k = 1; k < o.size(); k++)
	{
		minX = std::min(minX, o[k].x); maxX = std::max(maxX, o[k].x);
		minY = std::min(minY, o[k].y); maxY = std::max(maxY, o[k].y);
	}
	static const double mid[4][2] = { { 1.0, 1.0 / 3.0 }, { 1.0 / 3.0, 1.0 }, { 1.0, 5.0 / 3.0 }, { 5.0 / 3.0, 1.0 } };
	for(int i = FloorDiv2(minX); i <= FloorDiv2(maxX); i++)
		for(int j = FloorDiv2(minY); j <= FloorDiv2(maxY); j++)
			for(int q = 0; q < 4; q++)
				if(PointInOutline(o, i * 2 + mid[q][0], j * 2 + mid[q][1], false))
					m.push_back(Key(i, j, q));
	std::sort(m.begin(), m.end());
	long area2 = Area2(o);
	if(area2 < 0)
		area2 = -area2;
	return !m.empty() && area2 == 2 * (long)m.size();
}

bool CellSet::ToOutline(std::vector<GridPoint>& outline) const
{
	outline.clear();
	if(m.empty())
		return false;
	typedef std::pair<int, int> XY;
	std::map<XY, XY> next;
	for(size_t k = 0; k < m.size(); k++)
	{
		const int key = m[k];
		const int q = key & 3;
		const int j = ((key >> 2) & 1023) - 256;
		const int i = (key >> 12) - 256;
		const XY centre(i * 2 + 1, j * 2 + 1);
		const XY corner[4] = { XY(i * 2, j * 2), XY(i * 2, j * 2 + 2), XY(i * 2 + 2, j * 2 + 2), XY(i * 2 + 2, j * 2) };
		const XY p[3] = { centre, corner[(q + 3) & 3], corner[q] };
		int neighbour[3];
		Neighbours(key, neighbour);
		for(int e = 0; e < 3; e++)
		{
			if(Has(neighbour[e]))
				continue;
			if(!next.insert(std::make_pair(p[e], p[(e + 1) % 3])).second)
				return false;
		}
	}
	if(next.empty())
		return false;
	std::vector<XY> loop;
	const XY start = next.begin()->first;
	XY at = start;
	do
	{
		loop.push_back(at);
		const std::map<XY, XY>::const_iterator it = next.find(at);
		if(it == next.end() || loop.size() > next.size())
			return false;
		at = it->second;
	}
	while(at != start);
	if(loop.size() != next.size())
		return false;
	std::vector<XY> corners;
	const size_t n = loop.size();
	for(size_t k = 0; k < n; k++)
	{
		const XY& a = loop[(k + n - 1) % n];
		const XY& b = loop[k];
		const XY& c = loop[(k + 1) % n];
		if(Sign(b.first - a.first) != Sign(c.first - b.first) || Sign(b.second - a.second) != Sign(c.second - b.second))
			corners.push_back(b);
	}
	if(corners.size() < 3)
		return false;
	size_t first = 0;
	for(size_t k = 1; k < corners.size(); k++)
		if(corners[k].first < corners[first].first
				|| (corners[k].first == corners[first].first && corners[k].second > corners[first].second))
			first = k;
	for(size_t k = 0; k < corners.size(); k++)
	{
		const XY& c = corners[(first + k) % corners.size()];
		outline.push_back(Pt(c.first, c.second));
	}
	return true;
}

bool CellSet::Overlaps(const CellSet& o) const
{
	size_t a = 0, b = 0;
	while(a < m.size() && b < o.m.size())
	{
		if(m[a] == o.m[b])
			return true;
		if(m[a] < o.m[b]) a++; else b++;
	}
	return false;
}

bool CellSet::Contains(const CellSet& o) const
{
	return std::includes(m.begin(), m.end(), o.m.begin(), o.m.end());
}

bool CellSet::SharesEdgeWith(const CellSet& o) const
{
	for(size_t k = 0; k < o.m.size(); k++)
	{
		int neighbour[3];
		Neighbours(o.m[k], neighbour);
		for(int e = 0; e < 3; e++)
			if(Has(neighbour[e]))
				return true;
	}
	return false;
}

bool CellSet::IsOnePiece() const
{
	if(m.empty())
		return false;
	std::set<int> seen;
	std::vector<int> todo(1, m[0]);
	seen.insert(m[0]);
	while(!todo.empty())
	{
		const int key = todo.back();
		todo.pop_back();
		int neighbour[3];
		Neighbours(key, neighbour);
		for(int e = 0; e < 3; e++)
			if(Has(neighbour[e]) && seen.insert(neighbour[e]).second)
				todo.push_back(neighbour[e]);
	}
	return seen.size() == m.size();
}

void CellSet::Holes(std::vector<int>& holes) const
{
	holes.clear();
	if(m.empty())
		return;
	int minI = 1 << 20, maxI = -(1 << 20), minJ = 1 << 20, maxJ = -(1 << 20);
	for(size_t k = 0; k < m.size(); k++)
	{
		const int j = ((m[k] >> 2) & 1023) - 256;
		const int i = (m[k] >> 12) - 256;
		minI = std::min(minI, i); maxI = std::max(maxI, i);
		minJ = std::min(minJ, j); maxJ = std::max(maxJ, j);
	}
	minI--; maxI++; minJ--; maxJ++;
	std::set<int> outside;
	std::vector<int> todo(1, Key(minI, minJ, 0));
	outside.insert(todo[0]);
	while(!todo.empty())
	{
		const int key = todo.back();
		todo.pop_back();
		int neighbour[3];
		Neighbours(key, neighbour);
		for(int e = 0; e < 3; e++)
		{
			const int nk = neighbour[e];
			const int j = ((nk >> 2) & 1023) - 256;
			const int i = (nk >> 12) - 256;
			if(i < minI || i > maxI || j < minJ || j > maxJ || Has(nk))
				continue;
			if(outside.insert(nk).second)
				todo.push_back(nk);
		}
	}
	for(int i = minI; i <= maxI; i++)
		for(int j = minJ; j <= maxJ; j++)
			for(int q = 0; q < 4; q++)
			{
				const int key = Key(i, j, q);
				if(!Has(key) && !outside.count(key))
					holes.push_back(key);
			}
}

bool CellSet::HasHoles() const
{
	std::vector<int> holes;
	Holes(holes);
	return !holes.empty();
}

void CellSet::FillHoles()
{
	std::vector<int> holes;
	Holes(holes);
	if(holes.empty())
		return;
	m.insert(m.end(), holes.begin(), holes.end());
	std::sort(m.begin(), m.end());
}

void CellSet::Add(const CellSet& o)
{
	std::vector<int> all;
	std::set_union(m.begin(), m.end(), o.m.begin(), o.m.end(), std::back_inserter(all));
	m.swap(all);
}

void CellSet::Remove(const CellSet& o)
{
	std::vector<int> rest;
	std::set_difference(m.begin(), m.end(), o.m.begin(), o.m.end(), std::back_inserter(rest));
	m.swap(rest);
}

bool CellSet::MiddleInside(const std::vector<GridPoint>& outline) const
{
	static const double mid[4][2] = { { 1.0, 1.0 / 3.0 }, { 1.0 / 3.0, 1.0 }, { 1.0, 5.0 / 3.0 }, { 5.0 / 3.0, 1.0 } };
	for(size_t k = 0; k < m.size(); k++)
	{
		const int q = m[k] & 3;
		const int j = ((m[k] >> 2) & 1023) - 256;
		const int i = (m[k] >> 12) - 256;
		if(PointInOutline(outline, i * 2 + mid[q][0], j * 2 + mid[q][1], false))
			return true;
	}
	return false;
}

bool Editor::ArmorIncrease(std::string& why)
{
	if(m.armor >= ArmorMax)
	{
		why = "The hull armor is at its highest level.";
		return false;
	}
	m.armor = m.armor < 0 ? 0 : m.armor + 1;
	return true;
}

bool Editor::ArmorDecrease(std::string& why)
{
	if(m.armor <= 0)
	{
		why = "The hull has no armor to take away.";
		return false;
	}
	m.armor = m.armor > ArmorMax ? ArmorMax : m.armor - 1;
	return true;
}

std::string Editor::RoomLabel(const Room& r) const
{
	const RoomTypeInfo* info = RoomTypeInfoOf(r.type);
	const char* inBuilding = b ? RoomTypeBuildingName(r.type) : 0;
	char number[32];
	snprintf(number, sizeof(number), " %d", r.style);
	return std::string(inBuilding ? inBuilding : info ? info->name : "room") + number;
}

int Editor::RoomAt(int deck, double gx, double gy) const
{
	for(int roofs = 0; roofs < 2; roofs++)
		for(size_t i = 0; i < m.rooms.size(); i++)
			if(m.rooms[i].deck == deck && IsRoof(m.rooms[i].type) == (roofs == 1) && WhereInRoom(m.rooms[i], gx, gy) == 2)
				return (int)i;
	return -1;
}

int Editor::FindRoom(int deck, int roomType, int roomNumber) const
{
	for(size_t i = 0; i < m.rooms.size(); i++)
		if(m.rooms[i].deck == deck && m.rooms[i].type == roomType && m.rooms[i].style == roomNumber)
			return (int)i;
	return -1;
}

int Editor::NextRoomId() const
{
	std::set<int> used;
	for(size_t i = 0; i < m.rooms.size(); i++)
		used.insert(m.rooms[i].id & 0xffff);
	int id = 1;
	while(used.count(id))
		id++;
	return id;
}

namespace
{

bool CellInLimits(const std::vector<GridPoint>& cell, std::string& why)
{
	if(cell.size() < 3)
	{
		why = "Enable at least two kinds of grid lines to make cells.";
		return false;
	}
	for(size_t i = 0; i < cell.size(); i++)
		if(cell[i].x < -GridLimit || cell[i].x > GridLimit || cell[i].y < -GridLimit || cell[i].y > GridLimit)
		{
			why = "That is beyond the edge of the design grid.";
			return false;
		}
	return true;
}

}

bool Editor::PlanSubtract(int deck, const CellSet& cells, int keep, bool skipRoofs, std::vector<RoomChange>& changes,
		std::string& why) const
{
	for(size_t i = 0; i < m.rooms.size(); i++)
	{
		const Room& r = m.rooms[i];
		if(r.deck != deck || (int)i == keep || (skipRoofs && IsRoof(r.type)))
			continue;
		CellSet rest;
		if(!rest.FromOutline(r.points))
		{
			if(cells.MiddleInside(r.points))
			{
				why = RoomLabel(r) + " has an outline off the grid lines and cannot be changed. Erase the whole room instead.";
				return false;
			}
			continue;
		}
		if(!rest.Overlaps(cells))
			continue;
		rest.Remove(cells);
		RoomChange change;
		change.room = (int)i;
		if(!rest.Empty())
		{
			if(!rest.IsOnePiece())
			{
				why = "That would cut " + RoomLabel(r) + " in two.";
				return false;
			}
			if(rest.HasHoles() || !rest.ToOutline(change.outline))
			{
				why = "That would leave a hole in " + RoomLabel(r) + ".";
				return false;
			}
		}
		changes.push_back(change);
	}
	return true;
}

void Editor::ApplyChanges(const std::vector<RoomChange>& changes)
{
	std::vector<int> gone;
	for(size_t i = 0; i < changes.size(); i++)
	{
		if(changes[i].outline.empty())
			gone.push_back(changes[i].room);
		else
			m.rooms[changes[i].room].points = changes[i].outline;
	}
	std::sort(gone.begin(), gone.end());
	for(size_t i = gone.size(); i > 0; i--)
		m.rooms.erase(m.rooms.begin() + gone[i - 1]);
	m.UpdateDeckRange();
}

bool Editor::PaintCell(int deck, const std::vector<GridPoint>& cell, int roomType, int roomNumber, std::string& why)
{
	if(!CellInLimits(cell, why))
		return false;
	if(b)
	{
		for(size_t i = 0; i < cell.size(); i++)
			if(!b->ValidGrid(cell[i].x, cell[i].y, deck))
			{
				why = "That is beyond the edge of the building site.";
				return false;
			}
		if(deck < b->MinDeck() || deck > b->MaxDeck())
		{
			char range[96];
			if(b->MinDeck() == b->MaxDeck())
				snprintf(range, sizeof(range), "Rooms of this kind of building can only be on storey %d.", b->MinDeck());
			else
				snprintf(range, sizeof(range), "Rooms of this kind of building can only be on storeys %d to %d.", b->MinDeck(), b->MaxDeck());
			why = range;
			return false;
		}
	}
	else if(deck < -DeckAddLimit || deck > DeckAddLimit)
	{
		why = "Rooms can only be added on decks -28 to 28.";
		return false;
	}
	if(IsRoof(roomType))
	{
		why = "A roof is made with the roof tools, by three clicks.";
		return false;
	}
	if(!CanPaint(roomType) || roomNumber < 0 || roomNumber > 255)
	{
		why = "There is no such kind of room.";
		return false;
	}
	CellSet cells;
	if(!cells.FromOutline(cell))
	{
		why = "That is not a cell of the design grid.";
		return false;
	}

	const int own = FindRoom(deck, roomType, roomNumber);
	CellSet grown(cells);
	if(own >= 0)
	{
		CellSet room;
		if(!room.FromOutline(m.rooms[own].points))
		{
			why = RoomLabel(m.rooms[own]) + " has an outline off the grid lines and cannot be changed. Erase the whole room instead.";
			return false;
		}
		if(room.Contains(cells))
		{
			why = "That cell is already part of " + RoomLabel(m.rooms[own]) + ".";
			return false;
		}
		if(!room.Overlaps(cells) && !room.SharesEdgeWith(cells))
		{
			why = "A new cell must share a side with " + RoomLabel(m.rooms[own])
					+ ". For a separate room of the same kind on this deck, change the room number.";
			return false;
		}
		grown.Add(room);
		grown.FillHoles();
	}
	std::vector<GridPoint> outline;
	if(!grown.ToOutline(outline))
	{
		why = "That would make the room touch itself at a corner.";
		return false;
	}

	std::vector<RoomChange> changes;
	if(!PlanSubtract(deck, grown, own, true, changes, why))
		return false;

	if(own >= 0)
		m.rooms[own].points = outline;
	else
	{
		Room room;
		room.design = m.id;
		room.id = NextRoomId();
		room.deck = deck;
		room.type = roomType;
		room.style = roomNumber;
		room.points = outline;
		m.rooms.push_back(room);
	}
	ApplyChanges(changes);
	VerifyDetails();
	return true;
}

bool Editor::EraseCells(int deck, const std::vector<GridPoint>& cell, std::string& why)
{
	if(!CellInLimits(cell, why))
		return false;
	CellSet cells;
	if(!cells.FromOutline(cell))
	{
		why = "That is not a cell of the design grid.";
		return false;
	}
	std::vector<RoomChange> changes;
	if(!PlanSubtract(deck, cells, -1, b != 0, changes, why))
		return false;
	if(changes.empty())
	{
		why = "There is no room there to erase.";
		return false;
	}
	ApplyChanges(changes);
	VerifyDetails();
	return true;
}

bool Editor::EraseRoom(int deck, const std::vector<GridPoint>& cell, std::string& why)
{
	if(cell.size() < 3)
	{
		why = "Enable at least two kinds of grid lines to pick a cell.";
		return false;
	}
	CellSet cells;
	cells.FromOutline(cell);
	std::vector<RoomChange> changes;
	for(size_t i = 0; i < m.rooms.size(); i++)
		if(m.rooms[i].deck == deck && cells.MiddleInRoom(m.rooms[i]))
		{
			RoomChange change;
			change.room = (int)i;
			changes.push_back(change);
		}
	if(changes.empty())
	{
		why = "There is no room there to erase.";
		return false;
	}
	ApplyChanges(changes);
	VerifyDetails();
	return true;
}

bool Editor::ChangeRoomType(int deck, double gx, double gy, int roomType, std::string& why)
{
	const int at = RoomAt(deck, gx, gy);
	if(at < 0)
	{
		why = "There is no room there.";
		return false;
	}
	Room& r = m.rooms[at];
	if(!CanPaint(roomType))
	{
		why = "There is no such kind of room.";
		return false;
	}
	if(IsRoof(r.type))
	{
		why = "A roof cannot be made into a room.";
		return false;
	}
	if(r.type == roomType)
	{
		why = "The room is of that kind already.";
		return false;
	}
	const int other = FindRoom(deck, roomType, r.style);
	if(other >= 0)
	{
		why = "This deck already has " + RoomLabel(m.rooms[other]) + ". Change this room's number first.";
		return false;
	}
	Room changed(r);
	m.rooms.erase(m.rooms.begin() + at);
	changed.type = roomType;
	changed.id = NextRoomId();
	m.rooms.push_back(changed);
	VerifyDetails();
	return true;
}

bool Editor::ChangeRoomNumber(int deck, double gx, double gy, int roomNumber, std::string& why)
{
	const int at = RoomAt(deck, gx, gy);
	if(at < 0)
	{
		why = "There is no room there.";
		return false;
	}
	Room& r = m.rooms[at];
	if(roomNumber < 0 || roomNumber > 255)
	{
		why = "Room numbers go from 0 to 255.";
		return false;
	}
	if(r.style == roomNumber)
	{
		why = "The room has that number already.";
		return false;
	}
	const int other = FindRoom(deck, r.type, roomNumber);
	if(other >= 0)
	{
		why = "This deck already has " + RoomLabel(m.rooms[other]) + ".";
		return false;
	}
	Room changed(r);
	m.rooms.erase(m.rooms.begin() + at);
	changed.style = roomNumber;
	changed.id = NextRoomId();
	m.rooms.push_back(changed);
	VerifyDetails();
	return true;
}

bool Editor::EraseDeck(int deck, std::string& why)
{
	bool any = false;
	for(size_t i = m.details.size(); i > 0; i--)
		if(m.details[i - 1].deck == deck)
		{
			m.details.erase(m.details.begin() + (i - 1));
			any = true;
		}
	for(size_t i = m.rooms.size(); i > 0; i--)
		if(m.rooms[i - 1].deck == deck)
		{
			m.rooms.erase(m.rooms.begin() + (i - 1));
			any = true;
		}
	if(!any)
	{
		why = "This deck is empty.";
		return false;
	}
	m.UpdateDeckRange();
	return true;
}

bool Editor::MoveFits(int deck, bool wholeDesign, int dx, int dy, int dDeck, std::string& why) const
{
	const int gx = dx * 2, gy = dy * 2;
	bool any = false;
	for(size_t i = 0; i < m.details.size(); i++)
	{
		const Detail& d = m.details[i];
		if(!wholeDesign && dDeck != 0 && d.deck == deck + dDeck)
		{
			why = "The deck to go to has equipment on it. It must be empty.";
			return false;
		}
		if(!wholeDesign && d.deck != deck)
			continue;
		any = true;
		if(d.deck + dDeck < -DeckLimit || d.deck + dDeck > DeckLimit)
		{
			why = "That is beyond the last deck there can be.";
			return false;
		}
		if(!InGrid(d.x + gx, d.y + gy, d.deck + dDeck))
		{
			why = b ? "That would take equipment beyond the edge of the building site."
					: "That would take equipment beyond the edge of the design grid.";
			return false;
		}
	}
	for(size_t i = 0; i < m.rooms.size(); i++)
	{
		const Room& r = m.rooms[i];
		if(!wholeDesign && dDeck != 0 && r.deck == deck + dDeck)
		{
			why = "The deck to go to has rooms on it. It must be empty.";
			return false;
		}
		if(!wholeDesign && r.deck != deck)
			continue;
		any = true;
		if(r.deck + dDeck < -DeckLimit || r.deck + dDeck > DeckLimit)
		{
			why = "That is beyond the last deck there can be.";
			return false;
		}
		if(b && IsRoof(r.type) && r.deck + dDeck < 0)
		{
			why = "That would take a roof below the ground.";
			return false;
		}
		if(b && !IsRoof(r.type) && dDeck != 0 && (r.deck + dDeck < b->MinDeck() || r.deck + dDeck > b->MaxDeck()))
		{
			why = "That would take rooms to a storey this kind of building cannot have.";
			return false;
		}
		for(size_t k = 0; k < r.points.size(); k++)
			if(!InGrid(r.points[k].x + gx, r.points[k].y + gy, r.deck + dDeck))
			{
				why = b ? "That would take a room beyond the edge of the building site."
						: "That would take a room beyond the edge of the design grid.";
				return false;
			}
		if(b && r.type == 24)
		{
			const std::vector<FootPoint> rim = RoomOutline(r);
			for(size_t k = 0; k < rim.size(); k++)
				if(!InGrid(rim[k].x + gx, rim[k].y + gy, r.deck + dDeck))
				{
					why = "That would take a roof beyond the edge of the building site.";
					return false;
				}
		}
	}
	if(!any)
	{
		why = wholeDesign ? "The design is empty." : "This deck is empty.";
		return false;
	}
	return true;
}

bool Editor::Move(bool wholeDesign, int deck, int dx, int dy, int dDeck, std::string& why)
{
	if(dx == 0 && dy == 0 && dDeck == 0)
	{
		why = "Give a distance to move by.";
		return false;
	}
	if(!MoveFits(deck, wholeDesign, dx, dy, dDeck, why))
		return false;
	for(size_t i = 0; i < m.details.size(); i++)
	{
		Detail& d = m.details[i];
		if(!wholeDesign && d.deck != deck)
			continue;
		d.x += dx * 2;
		d.y += dy * 2;
		d.deck += dDeck;
	}
	std::vector<Room> moved;
	for(size_t i = 0; i < m.rooms.size(); )
	{
		Room& r = m.rooms[i];
		if(!wholeDesign && r.deck != deck)
		{
			i++;
			continue;
		}
		for(size_t k = 0; k < r.points.size(); k++)
		{
			r.points[k].x += dx * 2;
			r.points[k].y += dy * 2;
		}
		r.deck += dDeck;
		if(wholeDesign)
			i++;
		else
		{
			moved.push_back(r);
			m.rooms.erase(m.rooms.begin() + i);
		}
	}
	for(size_t i = 0; i < moved.size(); i++)
	{
		moved[i].id = NextRoomId();
		m.rooms.push_back(moved[i]);
	}
	m.UpdateDeckRange();
	VerifyDetails();
	return true;
}

int Editor::Copy(int deck, int dx, int dy, int dDeck, int repeat, std::string& why)
{
	if(dx == 0 && dy == 0 && dDeck == 0)
	{
		why = "Give a distance to copy by.";
		return 0;
	}
	if(dDeck == 0)
	{
		why = "A deck is copied to another deck: give a number of decks up or down.";
		return 0;
	}
	int copies = 0;
	const std::vector<Detail> details(m.details);
	const std::vector<Room> rooms(m.rooms);
	for(int n = 1; n <= repeat; n++)
	{
		std::string reason;
		if(!MoveFits(deck, false, dx * n, dy * n, dDeck * n, reason))
		{
			if(!copies)
				why = reason;
			break;
		}
		for(size_t i = 0; i < details.size(); i++)
		{
			if(details[i].deck != deck)
				continue;
			Detail d(details[i]);
			d.id = NextDetailId();
			d.x += dx * 2 * n;
			d.y += dy * 2 * n;
			d.deck += dDeck * n;
			m.details.push_back(d);
		}
		for(size_t i = 0; i < rooms.size(); i++)
		{
			if(rooms[i].deck != deck)
				continue;
			Room r(rooms[i]);
			r.id = NextRoomId();
			for(size_t k = 0; k < r.points.size(); k++)
			{
				r.points[k].x += dx * 2 * n;
				r.points[k].y += dy * 2 * n;
			}
			r.deck += dDeck * n;
			m.rooms.push_back(r);
		}
		copies++;
	}
	if(copies)
	{
		m.UpdateDeckRange();
		VerifyDetails();
	}
	return copies;
}

}
