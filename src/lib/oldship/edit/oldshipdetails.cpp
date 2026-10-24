#include "oldshipedit.h"

#include "model.h"
#include "oldbuildingedit.h"
#include "oldbuildingspots.h"
#include "oldshiptypes.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace OldShip
{

namespace
{

const double Pi = 3.14159265358979323846;
const double BerthSpacing = 2.74509379;

bool Inside(const Room& r, double x, double y)
{
	return WhereInRoom(r, x, y) == 2;
}

bool IsCorner(const Room& r, int x, int y)
{
	const std::vector<FootPoint> o = RoomOutline(r);
	for(size_t i = 0; i < o.size(); i++)
		if(fabs(o[i].x - x) < 1e-6 && fabs(o[i].y - y) < 1e-6)
			return true;
	return false;
}

bool IsHullOpening(int t)
{
	return t == 3 || t == 9 || t == 50 || t == 52 || (t >= 91 && t <= 97);
}

bool IsMarker(int t)
{
	return t == 76 || t == 98 || t == 99;
}

bool IsAnyTurret(int t)
{
	return t == 21 || t == 22 || (t >= 33 && t <= 40);
}

double Spacing(int t)
{
	if(t == 74) return 5.0;
	if(t == 75) return 7.071;
	if(t == 1 || t == 2 || t == 4 || t == 6 || t == 7 || t == 11 || t == 12 || t == 13 || t == 15 || t == 17 || t == 19
			|| (t >= 25 && t <= 31) || (t >= 65 && t <= 73))
		return BerthSpacing;
	if(t == 10 || t == 14 || t == 16 || t == 18 || (t >= 20 && t <= 24) || (t >= 32 && t <= 40) || (t >= 100 && t <= 142))
		return 1.0;
	return 0.0;
}

void Spacing(const Detail& d, double& sx, double& sy)
{
	sx = sy = Spacing(d.type);
	if(DetailType::IsVehicleLoc(d.type))
	{
		double halfLength, halfWidth;
		VehicleHalfSize(d.extra, halfLength, halfWidth);
		const double a = DetailType::Rotation(d.type);
		sx = fabs(halfLength * cos(a)) + fabs(halfWidth * sin(a));
		sy = fabs(halfLength * sin(a)) + fabs(halfWidth * cos(a));
	}
}

std::vector<FootPoint> Placed(const std::vector<FootPoint>& footprint, const Detail& d)
{
	std::vector<FootPoint> out(footprint);
	for(size_t i = 0; i < out.size(); i++)
	{
		out[i].x += d.x;
		out[i].y += d.y;
	}
	return out;
}

bool FootprintsOverlap(const std::vector<FootPoint>& a, const std::vector<FootPoint>& b)
{
	if(a.size() < 3 || b.size() < 3)
		return false;
	double x0 = a[0].x, x1 = a[0].x, y0 = a[0].y, y1 = a[0].y;
	for(size_t i = 1; i < a.size(); i++)
	{
		x0 = std::min(x0, a[i].x); x1 = std::max(x1, a[i].x);
		y0 = std::min(y0, a[i].y); y1 = std::max(y1, a[i].y);
	}
	for(double x = x0 + 0.0613; x < x1; x += 0.125)
		for(double y = y0 + 0.0371; y < y1; y += 0.125)
			if(PointInFootprint(a, x, y, false) && PointInFootprint(b, x, y, false))
				return true;
	return false;
}

bool FillsArea(int t)
{
	return t == 10 || t == 14 || t == 16 || t == 18 || t == 23;
}

bool MayBeOnWall(int t)
{
	return FillsArea(t) || t == 99;
}

}

bool Editor::CanPlace(int t)
{
	const DetailTypeInfo* info = DetailTypeInfoOf(t);
	if(!info)
		return false;
	return info->placement != PLACE_OutsideDeck0;
}

int Editor::NextDetailId() const
{
	int32_t top = 0;
	for(size_t i = 0; i < m.details.size(); i++)
		top = std::max(top, m.details[i].id);
	return top + 1;
}

bool Editor::IsDetailValid(const Detail& d, int self, int& type, int& room, std::string& why) const
{
	type = d.type;
	room = d.room;
	const DetailTypeInfo* info = DetailTypeInfoOf(d.type);
	if(d.deck < -DeckLimit || d.deck > DeckLimit)
	{
		why = "There is no such deck.";
		return false;
	}
	if(d.x < -GridLimit || d.x > GridLimit || d.y < -GridLimit || d.y > GridLimit)
	{
		why = "That is beyond the edge of the design grid.";
		return false;
	}
	if(IsSpot(d.type))
		return IsSpotValid(d, self, room, why);
	if(!info)
	{
		why = "There is no such kind of equipment.";
		return false;
	}
	const int place = info->placement;
	const std::string name = DetailName(d.type);
	const char* const outsideWall = b ? "an outside wall" : "a hull wall";

	if(place == PLACE_Extents)
	{
		if(m.rooms.empty())
		{
			why = "The design has no rooms yet.";
			return false;
		}
		int minX = 0, maxX = 0, minY = 0, maxY = 0, minDeck = 0, maxDeck = 0;
		bool first = true;
		for(size_t i = 0; i < m.rooms.size(); i++)
			for(size_t k = 0; k < m.rooms[i].points.size(); k++)
			{
				const GridPoint& p = m.rooms[i].points[k];
				if(first)
				{
					minX = maxX = p.x; minY = maxY = p.y; minDeck = maxDeck = m.rooms[i].deck;
					first = false;
				}
				minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
				minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
				minDeck = std::min(minDeck, m.rooms[i].deck); maxDeck = std::max(maxDeck, m.rooms[i].deck);
			}
		if(first || d.x < minX || d.x > maxX || d.y < minY || d.y > maxY || d.deck < minDeck || d.deck > maxDeck)
		{
			why = "The " + name + " must be within the bounds of the rooms.";
			return false;
		}
		room = 0;
		return true;
	}

	if(place == PLACE_Stairs)
	{
		const std::vector<FootPoint> points = StairTestPoints(d.type);
		if(points.size() != 7)
		{
			why = "There is no such kind of stairs.";
			return false;
		}
		std::vector<size_t> rooms;
		for(size_t i = 0; i < m.rooms.size(); i++)
		{
			const Room& r = m.rooms[i];
			if(r.deck != d.deck)
				continue;
			const int where = WhereInRoom(r, d.x + points[0].x, d.y + points[0].y);
			const bool inside = where == 2;
			if(!where)
				continue;
			const bool closed = r.type == 9 || (r.type >= 22 && r.type <= 24);
			if(inside && closed)
			{
				why = b ? "Stairs cannot be in liquid storage, nor in a roof." : "Stairs cannot be in a fuel cell.";
				return false;
			}
			if(!closed)
				rooms.push_back(i);
		}
		if(rooms.empty())
		{
			why = "The stairs must be in a room. Turn them with the left and right arrow keys.";
			return false;
		}
		for(size_t k = 1; k < points.size(); k++)
		{
			bool found = false;
			for(size_t i = 0; i < rooms.size() && !found; i++)
				found = WhereInRoom(m.rooms[rooms[i]], d.x + points[k].x, d.y + points[k].y) != 0;
			if(!found)
			{
				why = "The stairs would reach through a wall. Turn them with the left and right arrow keys.";
				return false;
			}
		}
		return true;
	}

	int count = 0;
	for(size_t i = 0; i < m.rooms.size(); i++)
	{
		const Room& r = m.rooms[i];
		if(r.deck != d.deck)
			continue;
		const int where = WhereInRoom(r, d.x, d.y);
		const bool inside = where == 2;
		const bool onWall = where == 1;
		if(!inside && !onWall)
			continue;

		if(r.type == 2 && !info->okOnBridge)
		{
			why = "The " + name + (b ? " cannot be in the command center." : " cannot be on the bridge.");
			return false;
		}

		if(place == PLACE_Berth)
		{
			if(count)
			{
				why = "The " + name + " cannot be in two rooms at once.";
				return false;
			}
			if(r.type != 1 && r.type != 4 && r.type != 7 && r.type != 19)
			{
				why = "The " + name + " must be in living quarters: a barracks, cabin or stateroom.";
				return false;
			}
			if(onWall)
			{
				why = "The " + name + " must not be on a wall line.";
				return false;
			}
			if(DetailType::IsBerth(d.type))
			{
				static const double h = 0.70710678118654752;
				static const double step[8][2] = { { 1, 0 }, { h, h }, { 0, 1 }, { -h, h }, { -1, 0 }, { -h, -h }, { 0, -1 }, { h, -h } };
				const int turn = (int)floor(DetailType::Rotation(d.type) / (Pi / 4) + 0.5) & 7;
				if(!Inside(r, d.x + step[turn][0], d.y + step[turn][1]))
				{
					why = "The foot of the bunk would be outside the room. Turn it with the left and right arrow keys.";
					return false;
				}
			}
		}
		else if(place == PLACE_HullWall)
		{
			if(r.type == 9)
			{
				why = "The " + name + " must not be in a fuel cell.";
				return false;
			}
			if(count)
			{
				why = "The " + name + " must be on " + outsideWall + ", not a wall between rooms.";
				return false;
			}
			if(!onWall)
			{
				why = "The " + name + " must be on " + outsideWall + " line.";
				return false;
			}
			if(IsCorner(r, d.x, d.y))
			{
				why = "The " + name + " must not be at the end of a wall line.";
				return false;
			}
			if(DetailType::IsWallTurret(d.type))
			{
				double nx = 0, ny = 0;
				const std::vector<FootPoint> o = RoomOutline(r);
				const size_t n = o.size();
				for(size_t k = 0; k < n; k++)
				{
					const FootPoint& p = o[k];
					const FootPoint& q = o[(k + 1) % n];
					const double cross = (q.x - p.x) * (d.y - p.y) - (q.y - p.y) * (d.x - p.x);
					if(fabs(cross) < 1e-6 && d.x >= std::min(p.x, q.x) - 1e-6 && d.x <= std::max(p.x, q.x) + 1e-6
							&& d.y >= std::min(p.y, q.y) - 1e-6 && d.y <= std::max(p.y, q.y) + 1e-6)
					{
						const double len = sqrt((q.x - p.x) * (q.x - p.x) + (q.y - p.y) * (q.y - p.y));
						nx = -(q.y - p.y) / len;
						ny = (q.x - p.x) / len;
						break;
					}
				}
				if(Inside(r, d.x + nx * 0.25, d.y + ny * 0.25))
				{
					nx = -nx;
					ny = -ny;
				}
				double angle = atan2(ny, nx);
				if(angle < 0)
					angle += 2 * Pi;
				static const int variant[8] = { 21, 34, 35, 36, 37, 38, 39, 40 };
				int sector = 0;
				while(sector < 8 && angle >= Pi / 8 + sector * Pi / 4)
					sector++;
				type = variant[sector & 7];
			}
		}
		else if(place == PLACE_InteriorWall)
		{
			if(r.type == 9)
			{
				why = "The " + name + " must not be in a fuel cell.";
				return false;
			}
			if(count > 1)
			{
				why = "The " + name + " must not be where more than two rooms meet.";
				return false;
			}
			if(!onWall)
			{
				why = "The " + name + " must be on a wall line between two rooms.";
				return false;
			}
			if(IsCorner(r, d.x, d.y))
			{
				why = "The " + name + " must not be at the end of a wall line.";
				return false;
			}
		}
		else if(place == PLACE_Floor || place == PLACE_FloorOrOutside || place == PLACE_HullTopBottom)
		{
			if(r.type == 9)
			{
				why = "The " + name + " must not be in a fuel cell.";
				return false;
			}
			if(count)
			{
				why = "The " + name + " cannot be in two rooms at once.";
				return false;
			}
			if(onWall)
			{
				why = "The " + name + " must not be on a wall line.";
				return false;
			}
			if(place == PLACE_HullTopBottom && IsAnyTurret(d.type))
				type = 33;
		}
		else if(place == PLACE_Anywhere)
		{
			for(size_t k = 0; k < m.details.size(); k++)
			{
				const Detail& o = m.details[k];
				if((int)k != self && o.type == d.type && o.deck == d.deck && o.x == d.x && o.y == d.y)
				{
					why = "There is a " + name + " there already.";
					return false;
				}
			}
		}
		else if(place == PLACE_OutsideDeck0)
		{
			why = "The " + name + " cannot be in a room.";
			return false;
		}
		else
		{
			if(count)
			{
				why = "The " + name + " cannot be in two rooms at once.";
				return false;
			}
			const bool gameTable = d.type == 74 || d.type == 75;
			const bool rightRoom = r.type == place || (gameTable ? r.type == 10 : (r.type == 2 && info->okOnBridge));
			if(!rightRoom)
			{
				const RoomTypeInfo* want = RoomTypeInfoOf(place);
				why = "The " + name + " belongs in a " + std::string(want ? want->name : "different room") + ".";
				return false;
			}
			if(onWall && !MayBeOnWall(d.type))
			{
				why = "The " + name + " must not be on a wall line.";
				return false;
			}
			if(d.type == 99)
			{
				for(size_t k = 0; k < m.details.size(); k++)
				{
					const Detail& o = m.details[k];
					if((int)k == self || o.type != 99)
						continue;
					for(size_t q = 0; q < m.rooms.size(); q++)
						if((m.rooms[q].id & 0xffff) == (o.room & 0xffff))
						{
							if(m.rooms[q].style == r.style)
							{
								why = "The weapon bays with this room number have a weapon emitter already.";
								return false;
							}
							break;
						}
				}
			}
			else if(DetailType::IsLimitOnePerRoom(d.type))
			{
				for(size_t k = 0; k < m.details.size(); k++)
				{
					const Detail& o = m.details[k];
					if((int)k != self && o.type == d.type && o.deck == d.deck && (o.room & 0xffff) == (r.id & 0xffff))
					{
						why = "A room can have only one " + name + ".";
						return false;
					}
				}
			}
			if(gameTable)
			{
				const double h = 5.0 / GridUnit, k = 7.0710678 / GridUnit;
				const double corner[2][4][2] = { { { -h, -h }, { -h, h }, { h, h }, { h, -h } },
						{ { -k, 0 }, { 0, k }, { k, 0 }, { 0, -k } } };
				for(int c = 0; c < 4; c++)
					if(!Inside(r, d.x + corner[d.type - 74][c][0], d.y + corner[d.type - 74][c][1]))
					{
						why = "The game table would overlap a wall.";
						return false;
					}
			}
			if(DetailType::IsVehicleLoc(d.type))
			{
				const std::vector<FootPoint> corners = VehicleFootprint(d.type, d.extra);
				for(size_t c = 0; c < corners.size(); c++)
					if(!Inside(r, d.x + corners[c].x, d.y + corners[c].y))
					{
						why = "The vehicle's parking spot would overlap a wall. Turn it with the left and right arrow keys.";
						return false;
					}
			}
		}

		double ownX, ownY;
		Spacing(d, ownX, ownY);
		if(ownX != 0.0 && ownY != 0.0)
		{
			for(size_t k = 0; k < m.details.size(); k++)
			{
				const Detail& o = m.details[k];
				if((int)k == self || o.deck != d.deck || (o.room & 0xffff) != (r.id & 0xffff))
					continue;
				if(DetailType::IsStairs(o.type))
				{
					why = "The " + name + " would be too close to the stairs.";
					return false;
				}
				double otherX, otherY;
				Spacing(o, otherX, otherY);
				if(otherX == 0.0 && otherY == 0.0)
					continue;
				if(fabs((double)(o.x - d.x)) * GridUnit < ownX + otherX && fabs((double)(o.y - d.y)) * GridUnit < ownY + otherY)
				{
					why = "The " + name + " would be too close to other equipment.";
					return false;
				}
			}
		}

		room = r.id;
		count++;
	}

	if(place == PLACE_Floor || place == PLACE_HullTopBottom)
	{
		int below = 0;
		for(size_t i = 0; i < m.rooms.size(); i++)
		{
			const Room& r = m.rooms[i];
			if(r.deck != d.deck - 1)
				continue;
			const int where = WhereInRoom(r, d.x, d.y);
			if(where == 1)
			{
				why = "The " + name + " must not be above a wall line.";
				return false;
			}
			if(where != 2)
				continue;
			if(r.type == 9)
			{
				why = "The " + name + " must not be above a fuel cell.";
				return false;
			}
			if(below)
			{
				why = "The " + name + " must not be above more than one room.";
				return false;
			}
			if(count == 0)
			{
				if(IsAnyTurret(d.type))
					type = 22;
				room = r.id;
			}
			below++;
			count++;
		}
		if(count == 0)
		{
			why = "The " + name + " must be in or above a room.";
			return false;
		}
		if(b && place == PLACE_HullTopBottom && (d.deck < -1 || (d.deck < 1 && below == 0)))
		{
			why = "The " + name + " must be above the ground.";
			return false;
		}
		if(place == PLACE_Floor && count != 2)
		{
			why = "The " + name + " goes in a floor: it needs a room above and a room below.";
			return false;
		}
		if(place == PLACE_HullTopBottom && count != 1)
		{
			why = "The " + name + (b ? " goes through the outside of the building: it needs a room on one side only."
					: " goes in the hull: it needs a room on one side only.");
			return false;
		}
		return true;
	}

	if(place == PLACE_OutsideDeck0 && d.deck != 0)
	{
		why = "The " + name + " stands on the ground: on storey 0.";
		return false;
	}
	if(count == 0)
	{
		if(!b || (place != PLACE_FloorOrOutside && place != PLACE_OutsideDeck0))
		{
			why = "The " + name + (b ? " cannot be outside the building." : " cannot be outside the ship.");
			return false;
		}
		if(d.deck != 0)
		{
			why = "Outside, the " + name + " stands on the ground: on storey 0.";
			return false;
		}
		room = 0;
	}
	if(b && d.deck < 0 && IsHullOpening(d.type))
	{
		why = "The " + name + " cannot be below the ground.";
		return false;
	}
	if(count == 0)
		return true;
	if(place == PLACE_InteriorWall && count != 2)
	{
		why = "The " + name + " must be on a wall line between two rooms.";
		return false;
	}
	if(place == PLACE_HullWall && count != 1)
	{
		why = "The " + name + " must be on " + outsideWall + " line.";
		return false;
	}
	return true;
}

bool Editor::IsPlacementValid(const Detail& d, int& type, int& room, std::string& why) const
{
	if(DetailType::IsStairs(d.type) && !StairOnTiles(d.type, d.x, d.y))
	{
		type = d.type;
		room = d.room;
		why = "Stairs go on whole tiles, not between tiles.";
		return false;
	}
	return IsDetailValid(d, -1, type, room, why);
}

int Editor::PlaceDetail(Detail d, std::string& why)
{
	const int wanted = d.type;
	std::vector<size_t> replaced;
	if(DetailType::IsStairs(wanted))
	{
		const std::vector<FootPoint> floor = Placed(StairFootprint(wanted), d);
		for(size_t i = 0; i < m.details.size(); i++)
		{
			const Detail& o = m.details[i];
			if(o.deck != d.deck || IsMarker(o.type))
				continue;
			bool goes = false;
			if(DetailType::IsStairs(o.type))
			{
				if(o.type == wanted && o.x == d.x && o.y == d.y)
				{
					why = "There are stairs there already.";
					return -1;
				}
				goes = FootprintsOverlap(floor, Placed(StairFootprint(o.type), o));
			}
			else
				goes = PointInFootprint(floor, o.x, o.y, !DetailType::CanBeOnStairEdge(o.type));
			if(goes)
				replaced.push_back(i);
		}
	}
	else if(!IsMarker(wanted))
	{
		for(size_t i = 0; i < m.details.size(); i++)
		{
			const Detail& o = m.details[i];
			if(o.deck != d.deck || IsMarker(o.type))
				continue;
			if(DetailType::IsStairs(o.type))
			{
				if(PointInFootprint(Placed(StairFootprint(o.type), o), d.x, d.y, !DetailType::CanBeOnStairEdge(wanted)))
				{
					why = "There are stairs there.";
					return -1;
				}
			}
			else if(o.x == d.x && o.y == d.y)
			{
				if(o.type == wanted)
				{
					const DetailTypeInfo* info = DetailTypeInfoOf(wanted);
					why = "There is a " + std::string(info ? info->name : "piece of equipment") + " there already.";
					return -1;
				}
				if(replaced.empty())
					replaced.push_back(i);
			}
		}
	}

	const std::vector<Detail> before(m.details);
	for(size_t i = replaced.size(); i > 0; i--)
		m.details.erase(m.details.begin() + replaced[i - 1]);
	int type, room;
	if(!IsPlacementValid(d, type, room, why))
	{
		m.details = before;
		return -1;
	}
	if(wanted == 76 || (b && (wanted == 100 || wanted == 142)))
	{
		for(size_t i = m.details.size(); i > 0; i--)
			if(m.details[i - 1].type == wanted)
				m.details.erase(m.details.begin() + (i - 1));
	}
	d.type = type;
	d.room = room;
	d.id = NextDetailId();
	m.details.push_back(d);
	return (int)m.details.size() - 1;
}

bool Editor::AddDetail(int deck, double gx, double gy, int detailType, int brush, std::string& why, int commodity)
{
	if(!CanPlaceHere(detailType))
	{
		why = b ? "Buildings do not have this kind of equipment." : "This kind of equipment is not for spacecraft.";
		return false;
	}
	Detail d;
	d.deck = deck;
	d.type = detailType;
	d.flag = false;
	d.design = m.id;
	d.id = 0;
	d.x = (int)floor(gx + 0.5);
	d.y = (int)floor(gy + 0.5);
	if(DetailType::IsStairs(detailType))
		StairSnap(detailType, gx, gy, d.x, d.y);
	d.room = 0;
	d.extra = commodity;
	if(b && !IsSpot(detailType) && !b->ValidGrid(d.x, d.y, deck))
	{
		why = "That is beyond the edge of the building site.";
		return false;
	}

	if(detailType == 98)
	{
		int emitters = 0;
		for(size_t i = 0; i < m.details.size(); i++)
			if(m.details[i].type == 98)
				emitters++;
		if(emitters > 9)
		{
			why = "The design has all the contrail emitters it can have.";
			return false;
		}
	}

	const int at = PlaceDetail(d, why);
	if(at < 0)
		return false;
	const int room = m.details[at].room;

	if(brush > 0 && FillsArea(detailType))
	{
		if(brush > BrushMax)
			brush = BrushMax;
		int roomAt = -1;
		for(size_t i = 0; i < m.rooms.size(); i++)
			if(m.rooms[i].deck == deck && (m.rooms[i].id & 0xffff) == (room & 0xffff))
				roomAt = (int)i;
		if(roomAt >= 0)
		{
			const std::vector<GridPoint> outline = m.rooms[roomAt].points;
			for(int i = -brush; i <= brush; i++)
				for(int j = -brush; j <= brush; j++)
				{
					if((i == 0 && j == 0) || !PointInOutline(outline, d.x + i, d.y + j, true))
						continue;
					Detail more(d);
					more.x = d.x + i;
					more.y = d.y + j;
					std::string ignored;
					PlaceDetail(more, ignored);
				}
		}
	}
	return true;
}

bool Editor::EraseDetail(int deck, double gx, double gy, int brush, std::string& why)
{
	const int x = (int)floor(gx + 0.5), y = (int)floor(gy + 0.5);
	int type = -1;
	for(size_t i = 0; i < m.details.size(); i++)
		if(m.details[i].deck == deck && m.details[i].x == x && m.details[i].y == y)
		{
			type = m.details[i].type;
			m.details.erase(m.details.begin() + i);
			break;
		}
	if(type < 0)
	{
		for(size_t i = 0; i < m.details.size() && type < 0; i++)
		{
			const Detail& o = m.details[i];
			if(o.deck == deck && DetailType::IsStairs(o.type) && PointInFootprint(Placed(StairFootprint(o.type), o), gx, gy, true))
			{
				type = o.type;
				m.details.erase(m.details.begin() + i);
				brush = 0;
			}
		}
	}
	if(type < 0)
	{
		for(size_t i = 0; i < m.details.size() && type < 0; i++)
		{
			const Detail& o = m.details[i];
			if(o.deck == deck && IsSpot(o.type) && PointInFootprint(Placed(SpotFootprint(o.type), o), gx, gy, true))
			{
				type = o.type;
				m.details.erase(m.details.begin() + i);
				brush = 0;
			}
		}
	}
	if(type < 0)
	{
		why = "There is no equipment there to erase.";
		return false;
	}
	if(brush > 0)
	{
		if(brush > BrushMax)
			brush = BrushMax;
		for(size_t i = m.details.size(); i > 0; i--)
		{
			const Detail& o = m.details[i - 1];
			if(o.deck == deck && o.type == type && abs(o.x - x) <= brush && abs(o.y - y) <= brush)
				m.details.erase(m.details.begin() + (i - 1));
		}
	}
	return true;
}

int Editor::VerifyDetails()
{
	int removed = 0;
	for(size_t i = 0; i < m.details.size(); )
	{
		int type, room;
		std::string why;
		if(IsDetailValid(m.details[i], (int)i, type, room, why))
		{
			m.details[i].type = type;
			m.details[i].room = room;
			i++;
		}
		else
		{
			m.details.erase(m.details.begin() + i);
			removed++;
		}
	}
	return removed;
}

bool Editor::IsSpotValid(const Detail& d, int self, int& room, std::string& why) const
{
	room = 0;
	if(!b)
	{
		why = "Parking spots are for buildings.";
		return false;
	}
	const SpotKind kind = SpotKindOf(d.type);
	const std::vector<FootPoint> spot = Placed(SpotFootprint(d.type), d);

	bool inGarage = false;
	if(kind == Spot_Ground)
		for(size_t i = 0; i < m.rooms.size() && !inGarage; i++)
		{
			const Room& r = m.rooms[i];
			if(r.deck != d.deck || r.type != 13)
				continue;
			const std::vector<FootPoint> o = RoomOutline(r);
			bool inside = true;
			for(size_t k = 0; k < spot.size() && inside; k++)
				inside = PointInFootprint(o, spot[k].x, spot[k].y, true);
			if(inside)
			{
				inGarage = true;
				room = r.id;
			}
		}

	if(!inGarage)
	{
		if(d.deck != 0)
		{
			why = kind == Spot_Ground ? "A ground vehicle parks outside on the ground storey, or in a garage."
					: "Parking spots are outside, on the ground storey.";
			return false;
		}
		for(size_t i = 0; i < m.rooms.size(); i++)
		{
			const Room& r = m.rooms[i];
			if(r.deck != 0 || IsRoofType(r.type))
				continue;
			const std::vector<FootPoint> o = RoomOutline(r);
			bool covers = FootprintsOverlap(spot, o);
			for(size_t k = 0; k < o.size() && !covers; k++)
				covers = PointInFootprint(spot, o[k].x, o[k].y, false);
			for(size_t k = 0; k < spot.size() && !covers; k++)
				covers = PointInFootprint(o, spot[k].x, spot[k].y, false);
			if(covers)
			{
				why = kind == Spot_Ground ? "The spot would cover part of the building. A ground vehicle can park in a garage."
						: "The spot would cover part of the building: it stands outside, clear of it.";
				return false;
			}
		}
		const bool coastal = BuildingIsCoastal(b->industry);
		if(kind == Spot_Water && !coastal)
		{
			why = "Only a building by the sea has water vehicle spots.";
			return false;
		}
		for(size_t k = 0; k < spot.size(); k++)
		{
			if(kind == Spot_Water && spot[k].y <= 0)
			{
				why = "A water vehicle spot lies on the water side of the site: north of its middle line, all of it.";
				return false;
			}
			if(kind != Spot_Water && coastal && spot[k].y > 0)
			{
				why = "That is the water side of the site; this kind of spot stands on land.";
				return false;
			}
		}
	}

	for(size_t i = 0; i < m.details.size(); i++)
	{
		const Detail& o = m.details[i];
		if((int)i == self || o.deck != d.deck || !IsSpot(o.type))
			continue;
		const std::vector<FootPoint> other = Placed(SpotFootprint(o.type), o);
		bool overlap = FootprintsOverlap(spot, other);
		for(size_t k = 0; k < other.size() && !overlap; k++)
			overlap = PointInFootprint(spot, other[k].x, other[k].y, false);
		for(size_t k = 0; k < spot.size() && !overlap; k++)
			overlap = PointInFootprint(other, spot[k].x, spot[k].y, false);
		if(overlap)
		{
			why = "It would overlap another parking spot.";
			return false;
		}
	}
	return true;
}

}
