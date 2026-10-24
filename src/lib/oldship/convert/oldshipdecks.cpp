#include "oldshipconverter.h"

#include <cmath>

namespace OldShip
{

int BuildDoorStates(const Converter& c, AuScDoor& door, const DoorData& dd);

namespace
{

const double LadderReach = 1.834;
const double StairRise = 7.98958333333;
const double HalfRise = 3.99479166667;
const double AlongGrid = 5.656854249;
const double AlongGridRun = 11.313708498;
const double LowSteps = 3.0;

struct Flight
{
	P2		start, dir;
	double	run, half, z0, z1;
};

struct Landing
{
	double	x0, y0, x1, y1, z;
};

Flight MakeFlight(double sx, double sy, double dx, double dy, double run, double half, double z0, double z1)
{
	Flight f;
	f.start = P2(sx, sy);
	f.dir = P2(dx, dy);
	f.run = run;
	f.half = half;
	f.z0 = z0;
	f.z1 = z1;
	return f;
}

void StairLayout(int shape, bool diagonal, QVector<Flight>& flights, QVector<Landing>& landings)
{
	const double side = (shape == 2 || shape == 4) ? -1.0 : 1.0;
	if(shape == 0)
	{
		const double length = diagonal ? 12.0 : AlongGridRun, width = diagonal ? 4.0 : AlongGrid;
		flights.append(MakeFlight(0, width / 2, 1, 0, length, width / 2, 0, StairRise));
	}
	else if(shape == 1 || shape == 2)
	{
		const double width = diagonal ? 4.0 : AlongGrid, run = diagonal ? 8.0 : AlongGrid;
		flights.append(MakeFlight(0, side * width / 2, 1, 0, run, width / 2, 0, HalfRise));
		flights.append(MakeFlight(run + width / 2, side * width, 0, side, run, width / 2, HalfRise, StairRise));
		Landing l = { run, qMin(0.0, side * width), run + width, qMax(0.0, side * width), HalfRise };
		landings.append(l);
	}
	else
	{
		const double run = diagonal ? 8.0 : AlongGrid, width = diagonal ? 6.0 : AlongGrid, depth = diagonal ? 4.0 : AlongGrid;
		flights.append(MakeFlight(0, side * width / 2, 1, 0, run, width / 2, 0, HalfRise));
		flights.append(MakeFlight(run, side * width * 1.5, -1, 0, run, width / 2, HalfRise, StairRise));
		Landing l = { run, qMin(0.0, side * 2 * width), run + depth, qMax(0.0, side * 2 * width), HalfRise };
		landings.append(l);
	}
}

bool RectWithin(const Polygon& outline, const P2& start, const P2& dir, double length, double half)
{
	const P2 across = dir.Left();
	const double edge = 0.05;
	const int alongCount = qMax(2, (int)ceil((length - 2 * edge) / 0.33) + 1);
	const int acrossCount = qMax(2, (int)ceil((2 * half - 2 * edge) / 0.33) + 1);
	for(int i = 0; i < alongCount; i++)
		for(int j = 0; j < acrossCount; j++)
			if(!Contains(outline, start + dir * (edge + (length - 2 * edge) * i / (alongCount - 1))
					+ across * (-half + edge + (2 * half - 2 * edge) * j / (acrossCount - 1))))
				return false;
	return true;
}

bool SharesFloor(const Polygon& a, const Polygon& b)
{
	double x0 = 1e30, x1 = -1e30, y0 = 1e30, y1 = -1e30;
	for(int i = 0; i < a.size(); i++)
	{
		x0 = qMin(x0, a[i].x); x1 = qMax(x1, a[i].x);
		y0 = qMin(y0, a[i].y); y1 = qMax(y1, a[i].y);
	}
	for(double x = x0 + 0.171; x < x1; x += 0.33)
		for(double y = y0 + 0.113; y < y1; y += 0.33)
			if(Contains(a, P2(x, y)) && Contains(b, P2(x, y)))
				return true;
	return false;
}

void AddFlightSpace(const Converter& c, AuScFaceList& faces, const P2& start, const P2& dir, const Flight& fl, double from,
		double to, double z, bool capped)
{
	const P2 across = dir.Left();
	const double side = fl.half - VoidInset, slope = (fl.z1 - fl.z0) / fl.run;
	const P2 corner[4] = { start + dir * from - across * side, start + dir * to - across * side, start + dir * to + across * side,
			start + dir * from + across * side };
	const double rise[4] = { fl.z0 + slope * from, fl.z0 + slope * to, fl.z0 + slope * to, fl.z0 + slope * from };
	QVector<AuPoint> floorLine, roof;
	for(int k = 0; k < 4; k++)
	{
		floorLine.append(c.At(corner[k], z + rise[k] - 0.2));
		roof.append(c.At(corner[k], z + (capped ? VoidCeiling - DoorwayDip : rise[k] + DoorwayHeight)));
	}
	AddPrism(faces, floorLine, roof);
}

}

void Converter::BuildHatches()
{
	int made = 0;
	for(int di = 0; di < m_Doors.size(); di++)
	{
		const DoorData& dd = m_Doors[di];
		const Detail& d = m_Ship.details[dd.detail];
		if(!DetailType::IsAnyHatch(d.type))
			continue;
		const Vec3 loc = d.Loc();
		const P2 m(loc.x, loc.y);
		const double z = d.deck * DeckHeight;
		const int above = RoomAt(d.deck, m, 0.05), below = RoomAt(d.deck - 1, m, 0.05);
		if(dd.outside.isEmpty() && dd.inside.isEmpty())
		{
			Skip(QString("The hatch %1 has no cover in the old model (no whole cell of floor there): no hatch is made.").arg(Where(d)));
			continue;
		}
		if(above >= 0 && m_Rooms[above].openBelow)
		{
			Skip(QString("The hatch %1 is in %2, which has no floor: no hatch is made.").arg(Where(d)).arg(RoomName(above)));
			continue;
		}
		if(!Walkable(above) && !Walkable(below))
		{
			Skip(QString("The hatch %1 has no room people can enter over or under it: no hatch is made.").arg(Where(d)));
			continue;
		}
		ConvertInfo::DoorInfo info;
		info.kind = Walkable(above) && Walkable(below) ? ConvertInfo::DoorInfo::Hatch
				: Walkable(below) ? ConvertInfo::DoorInfo::HullHatchTop : ConvertInfo::DoorInfo::HullHatchBottom;
		if((info.kind == ConvertInfo::DoorInfo::HullHatchTop && above >= 0) || (info.kind == ConvertInfo::DoorInfo::HullHatchBottom && below >= 0))
		{
			Skip(QString("The hatch %1 leads into %2, which nobody can enter: no hatch is made.").arg(Where(d))
					.arg(RoomName(above >= 0 && !Walkable(above) ? above : below)));
			continue;
		}
		const bool hull = info.kind != ConvertInfo::DoorInfo::Hatch;
		const double turn = (double)DetailType::Rotation(d.type);
		const P2 toLadder(cos(turn), sin(turn));
		P2 body = m + toLadder * (LadderReach - ClimberBack);
		double shaftHalf = HatchHalf;
		bool fits = false;
		bool tall = false;
		for(int attempt = 0; attempt < 5 && !fits; attempt++)
		{
			if(attempt == 1 || attempt == 3)
				body = m;
			if(attempt == 2)
				shaftHalf = 0.9;
			if(attempt == 3)
			{
				shaftHalf = HatchHalf;
				tall = true;
			}
			if(attempt == 4)
				shaftHalf = 0.9;
			fits = true;
			for(int side = 0; side < 2; side++)
			{
				const int room = side ? above : below;
				if(!Walkable(room))
					continue;
				Polygon nearWall;
				if(tall && !Inset(m_Rooms[room].poly, HatchWallClear, nearWall))
					nearWall = m_Rooms[room].inner;
				const Polygon& in = tall ? nearWall : m_Rooms[room].inner;
				fits = fits && Contains(in, body) && Contains(in, m + P2(shaftHalf, shaftHalf)) && Contains(in, m + P2(-shaftHalf, shaftHalf))
						&& Contains(in, m + P2(shaftHalf, -shaftHalf)) && Contains(in, m + P2(-shaftHalf, -shaftHalf));
			}
		}
		if(!fits)
		{
			Skip(QString("The hatch %1 is against a wall of the room over or under it: a way through it would run into the wall. "
					"No hatch is made; its cover stays in the floor as it is drawn.").arg(Where(d)));
			continue;
		}
		const QString what = hull ? QString("hull hatch %1").arg(Where(d)) : QString("hatch %1").arg(Where(d));
		if(tall)
		{
			ClearPlace over = { d.deck, m, shaftHalf + LadderReach + 0.5 }, under = { d.deck - 1, m, shaftHalf + LadderReach + 0.5 };
			m_WallClear.append(over);
			m_WallClear.append(under);
		}

		AuScDoor door;
		const int faces = BuildDoorStates(*this, door, dd);
		door.SetHullDoor(hull);
		door.SetDoorOpLocs(At(m, z - 0.8), At(m, z + 0.8));
		const int own = Walkable(above) ? above : below;
		door.SetGroupName(m_Rooms[own].name);
		const quint16 id = Add(door, what);
		if(!id)
			continue;
		made++;
		m_DoorMade[di] = true;
		m_DoorPart[di] = id;
		m_Info.doorTriangles += faces;

		const double lowTop = z - DeckHeight + VoidCeiling, highBottom = z + VoidFloor;
		const bool top = info.kind == ConvertInfo::DoorInfo::HullHatchTop, bottom = info.kind == ConvertInfo::DoorInfo::HullHatchBottom;
		{
			AuScFaceList shaft;
			const double shaftLow = bottom ? z - 0.3 : (tall ? z - DeckHeight + VoidFloor - DoorwayDip : lowTop - 0.5);
			const double shaftHigh = top ? z + 0.3 : (tall ? z + VoidCeiling - DoorwayDip : highBottom + 0.5);
			AddWallBox(shaft, m, P2(1, 0), P2(0, 1), -shaftHalf, shaftHalf, shaftHalf, shaftLow, shaftHigh);
			AddRoomVoid(shaft, own, id, QString("void through the %1").arg(what));
		}
		if(hull)
		{
			AuScFaceList beyond;
			const double reach = HatchHalf + 1.5;
			if(top)
				AddWallBox(beyond, m, P2(1, 0), P2(0, 1), -reach, reach, reach, z - 0.05, z + HullMargin + 5.5);
			else
				AddWallBox(beyond, m, P2(1, 0), P2(0, 1), -reach, reach, reach, z - HullMargin - 5.5, z + 0.05);
			AddHullVoid(beyond, id, QString("hull void outside the %1").arg(what));
		}

		AuScPathLadder ladder;
		ladder.MakePath(At(body, z - DeckHeight + PathHeight), At(body, z + PathHeight));
		ladder.SetFacing(AuPoint(toLadder.x, toLadder.y, 0));
		ladder.SetAssociatedPart(id, AuScDoor::Open);
		info.ladder = Add(ladder, QString("ladder path of the %1").arg(what));
		info.ladderLow = At(body, z - DeckHeight + VoidFloor + 1.0);
		info.ladderHigh = At(body, z + VoidFloor + 1.0);

		info.detailType = d.type;
		info.detailId = d.id;
		info.part = id;
		info.roomA = RoomName(below);
		info.roomB = RoomName(above);
		info.mid = At(m, z - 0.1);
		info.sideA = info.ladderLow;
		info.sideB = info.ladderHigh;
		m_Info.doors.append(info);
		if(Walkable(below))
		{
			DoorSide s = { below, At(body, z - DeckHeight + PathHeight), body };
			m_DoorSides.append(s);
		}
		if(Walkable(above))
		{
			DoorSide s = { above, At(body, z + PathHeight), body };
			m_DoorSides.append(s);
		}
	}
	if(made)
		Log(QString("Hatches: %1, each a manual door in the floor with a ladder path beside the old ladder.").arg(made));
}

void Converter::BuildStairs()
{
	int made = 0, shut = 0;
	QVector<Polygon> taken;
	QVector<int> takenDeck;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const Detail& d = m_Ship.details[i];
		if(!DetailType::IsStairs(d.type) || d.flag)
			continue;
		const int shape = (d.type - 101) / 8;
		const bool diagonal = ((d.type - 101) & 1) != 0;
		const double turn = (double)DetailType::Rotation(d.type);
		const Vec3 loc = d.Loc();
		const double z = d.deck * DeckHeight;
		const P2 origin(loc.x, loc.y), ex(cos(turn), sin(turn)), ey(-sin(turn), cos(turn));
		QVector<Flight> flights;
		QVector<Landing> landings;
		StairLayout(shape, diagonal, flights, landings);

		QVector<Polygon> floor;
		for(int f = 0; f < flights.size(); f++)
		{
			const P2 start = origin + ex * flights[f].start.x + ey * flights[f].start.y;
			const P2 dir = ex * flights[f].dir.x + ey * flights[f].dir.y;
			const P2 across = dir.Left() * flights[f].half;
			Polygon outline;
			outline << start - across << start + dir * flights[f].run - across << start + dir * flights[f].run + across << start + across;
			floor.append(outline);
		}
		for(int l = 0; l < landings.size(); l++)
		{
			Polygon outline;
			outline << origin + ex * landings[l].x0 + ey * landings[l].y0 << origin + ex * landings[l].x1 + ey * landings[l].y0
					<< origin + ex * landings[l].x1 + ey * landings[l].y1 << origin + ex * landings[l].x0 + ey * landings[l].y1;
			floor.append(outline);
		}
		bool shared = false;
		for(int a = 0; a < floor.size() && !shared; a++)
			for(int b = 0; b < taken.size() && !shared; b++)
				shared = takenDeck[b] == d.deck && SharesFloor(floor[a], taken[b]);
		if(shared)
		{
			Note(QString("The stairs %1 stand on the floor of other stairs: they are drawn only, and the stairs before them in "
					"the design are the way up.").arg(Where(d)));
			continue;
		}

		const Flight& first = flights.first();
		const Flight& last = flights.last();
		const P2 foot = origin + ex * first.start.x + ey * first.start.y;
		const P2 firstDir = ex * first.dir.x + ey * first.dir.y;
		const P2 lastStart = origin + ex * last.start.x + ey * last.start.y;
		const P2 lastDir = ex * last.dir.x + ey * last.dir.y;
		const P2 head = lastStart + lastDir * last.run;
		const int low = RoomAt(d.deck, foot + firstDir * 1.0, 0.05), high = RoomAt(d.deck + 1, head + lastDir * 1.0, 0.05);
		if(!Walkable(low))
		{
			Skip(QString("The stairs %1 do not start in a room people can enter: they are decoration only.").arg(Where(d)));
			continue;
		}

		ConvertInfo::StairInfo info;
		info.detailType = d.type;
		info.detailId = d.id;
		info.roomLow = RoomName(low);

		const bool wallBehind = !Contains(m_Rooms[low].inner, foot - firstDir * 1.0);
		const P2 before = wallBehind ? foot + firstDir * (VoidInset + 0.3) : foot - firstDir * 1.0;
		if(!wallBehind)
			info.drawn.append(At(before, z + VoidFloor));
		for(int f = 0; f < flights.size(); f++)
		{
			const Flight& fl = flights[f];
			const P2 start = origin + ex * fl.start.x + ey * fl.start.y;
			const P2 dir = ex * fl.dir.x + ey * fl.dir.y;
			const P2 across = dir.Left();
			const double footIn = fl.z0 < 0.01 ? VoidInset : 0.0;
			for(int s = 0; s <= 4; s++)
			{
				const double along = s == 0 && footIn > 0 ? footIn + 0.3 : fl.run * s / 4.0;
				info.drawn.append(At(start + dir * along, z + fl.z0 + (fl.z1 - fl.z0) * along / fl.run + VoidFloor));
			}
			for(int s = 0; s < 5; s++)
				for(int w = -1; w <= 1; w++)
					info.plan.append(At(start + dir * (fl.run * (s + 0.5) / 5.0) + across * (w * (fl.half - 0.4)), z + VoidFloor));
		}
		for(int l = 0; l < landings.size(); l++)
			for(int s = 0; s < 3; s++)
				for(int w = 0; w < 3; w++)
					info.plan.append(At(origin + ex * (landings[l].x0 + (landings[l].x1 - landings[l].x0) * (s + 0.5) / 3.0)
							+ ey * (landings[l].y0 + (landings[l].y1 - landings[l].y0) * (w + 0.5) / 3.0), z + VoidFloor));
		const P2 after = head + lastDir * (Walkable(high) && Contains(m_Rooms[high].inner, head + lastDir * 1.5) ? 1.5 : VoidInset + 0.3);
		info.drawn.append(At(after, z + DeckHeight + VoidFloor));

		int well = -1;
		for(int r = 0; r < m_Rooms.size() && well < 0; r++)
		{
			const RoomData& rd = m_Rooms[r];
			if(rd.deck != d.deck + 1 || !rd.openBelow)
				continue;
			bool all = true;
			for(int f = 0; f < flights.size() && all; f++)
				all = RectWithin(rd.poly, origin + ex * flights[f].start.x + ey * flights[f].start.y,
						ex * flights[f].dir.x + ey * flights[f].dir.y, flights[f].run, flights[f].half);
			for(int l = 0; l < landings.size() && all; l++)
				all = RectWithin(rd.poly, origin + ex * landings[l].x0 + ey * ((landings[l].y0 + landings[l].y1) / 2), ex,
						landings[l].x1 - landings[l].x0, qAbs(landings[l].y1 - landings[l].y0) / 2);
			if(all)
				well = r;
		}
		const bool arrives = Walkable(high) && !m_Rooms[high].openBelow;
		if(well < 0 || !arrives)
		{
			const int over = RoomAt(d.deck + 1, lastStart + lastDir * (last.run / 2), 0.0);
			QString line;
			if(well < 0)
				line = QString("The stairs %1 lead nowhere: deck %2 is not open over them%3. They need an Open to Deck Below room "
						"above them on deck %2, over all of the floor they cover%4. As it is they are drawn only, and nothing "
						"passes through that floor.").arg(Where(d)).arg(d.deck + 1)
						.arg(over < 0 ? QString(" (it has no room there)") : m_Rooms[over].openBelow
							? QString(" all over (%1 is over a part of them only)").arg(m_Rooms[over].name)
							: QString(" (%1 has its floor there)").arg(m_Rooms[over].name))
						.arg(arrives ? QString() : QString(", and a room with a floor to arrive on at their top"));
			else
				line = QString("The stairs %1 lead nowhere: deck %2 is open over them (%3), but has no floor where they arrive. They "
						"need a room with a floor at their top. As it is they are drawn only.").arg(Where(d)).arg(d.deck + 1)
						.arg(m_Rooms[well].name);
			Note(line);
			NoteProblem(line, Problem::StairsShut);
			m_Info.stairsShut.append(line);
			m_Info.stairs.append(info);
			shut++;
			continue;
		}
		info.open = true;
		info.roomHigh = RoomName(high);
		for(int a = 0; a < floor.size(); a++)
		{
			taken.append(floor[a]);
			takenDeck.append(d.deck);
		}
		info.walk = info.drawn;

		for(int f = 0; f < flights.size(); f++)
		{
			const Flight& fl = flights[f];
			const P2 start = origin + ex * fl.start.x + ey * fl.start.y;
			const P2 dir = ex * fl.dir.x + ey * fl.dir.y;
			const P2 across = dir.Left();
			const double rise = (fl.z1 - fl.z0) / fl.run, back = OffGrid;
			const P2 c[4] = { start - dir * back - across * fl.half, start + dir * (fl.run + OffGrid) - across * fl.half,
					start + dir * (fl.run + OffGrid) + across * fl.half, start - dir * back + across * fl.half };
			const double h[4] = { fl.z0 - rise * back, fl.z1 + rise * OffGrid, fl.z1 + rise * OffGrid, fl.z0 - rise * back };
			QVector<AuPoint> bottom, slope;
			for(int k = 0; k < 4; k++)
			{
				bottom.append(At(c[k], z));
				slope.append(At(c[k], z + h[k] + VoidFloor));
			}
			AuScFaceList wedge;
			AddPrism(wedge, bottom, slope);
			AuScBarrier barrier;
			barrier.AddFaces(wedge);
			barrier.SetFaceOpacity(VoidOpacity);
			Add(barrier, QString("slope of the stairs %1, flight %2").arg(Where(d)).arg(f + 1));

			const double footIn = fl.z0 < 0.01 ? VoidInset : 0.0;
			const double split = fl.z0 >= LowSteps ? 0.0 : fl.z1 <= LowSteps ? fl.run : (LowSteps - fl.z0) * fl.run / (fl.z1 - fl.z0);
			if(split > footIn + 0.05)
			{
				AuScFaceList space;
				AddFlightSpace(*this, space, start, dir, fl, footIn, qMin(fl.run, split + VoidOverlap), z, true);
				AddRoomVoid(space, low, 0, QString("void over the foot of the stairs %1, flight %2").arg(Where(d)).arg(f + 1));
			}
			if(split < fl.run - 0.05)
			{
				const bool landingBefore = f > 0 && !landings.isEmpty(), landingAfter = f + 1 < flights.size() && !landings.isEmpty();
				AuScFaceList space;
				AddFlightSpace(*this, space, start, dir, fl, landingBefore && split <= 0.0 ? -VoidOverlap : qMax(split, footIn),
						landingAfter ? fl.run + VoidOverlap : fl.run, z, false);
				AddRoomVoid(space, well, 0, QString("void over the stairs %1, flight %2").arg(Where(d)).arg(f + 1));
			}
		}
		for(int l = 0; l < landings.size(); l++)
		{
			const Landing& ld = landings[l];
			const double seam = 0.05;
			const double bx = ld.x1 >= ld.x0 ? seam : -seam, by = ld.y1 >= ld.y0 ? seam : -seam;
			const P2 c[4] = { origin + ex * (ld.x0 - bx) + ey * (ld.y0 - by), origin + ex * (ld.x1 + bx) + ey * (ld.y0 - by),
					origin + ex * (ld.x1 + bx) + ey * (ld.y1 + by), origin + ex * (ld.x0 - bx) + ey * (ld.y1 + by) };
			double in[4] = { VoidInset, VoidInset, VoidInset, VoidInset };
			for(int f = 0; f < flights.size(); f++)
				for(int end = 0; end < 2; end++)
				{
					const P2 p = flights[f].start + flights[f].dir * (end ? flights[f].run : 0.0);
					const bool withinX = p.x > ld.x0 - 0.05 && p.x < ld.x1 + 0.05, withinY = p.y > ld.y0 - 0.05 && p.y < ld.y1 + 0.05;
					if(withinX && qAbs(p.y - ld.y0) < 0.05) in[0] = seam;
					if(withinY && qAbs(p.x - ld.x1) < 0.05) in[1] = seam;
					if(withinX && qAbs(p.y - ld.y1) < 0.05) in[2] = seam;
					if(withinY && qAbs(p.x - ld.x0) < 0.05) in[3] = seam;
				}
			const double sx = ld.x1 >= ld.x0 ? 1.0 : -1.0, sy = ld.y1 >= ld.y0 ? 1.0 : -1.0;
			const double vx0 = ld.x0 + sx * in[3], vx1 = ld.x1 - sx * in[1], vy0 = ld.y0 + sy * in[0], vy1 = ld.y1 - sy * in[2];
			const P2 v[4] = { origin + ex * vx0 + ey * vy0, origin + ex * vx1 + ey * vy0, origin + ex * vx1 + ey * vy1,
					origin + ex * vx0 + ey * vy1 };
			QVector<AuPoint> bottom, top, floorLine, roof;
			for(int k = 0; k < 4; k++)
			{
				bottom.append(At(c[k], z));
				top.append(At(c[k], z + ld.z + VoidFloor));
				floorLine.append(At(v[k], z + ld.z - 0.2));
				roof.append(At(v[k], z + ld.z + DoorwayHeight));
			}
			AuScFaceList block, space;
			AddPrism(block, bottom, top);
			AuScBarrier barrier;
			barrier.AddFaces(block);
			barrier.SetFaceOpacity(VoidOpacity);
			Add(barrier, QString("landing of the stairs %1").arg(Where(d)));
			AddPrism(space, floorLine, roof);
			AddRoomVoid(space, well, 0, QString("void over the landing of the stairs %1").arg(Where(d)));
		}
		{
			const P2 across = lastDir.Left();
			AuScFaceList sheet, onto;
			AddWallBox(sheet, head, across, lastDir, OffGrid / 2, 0.6, last.half, z + DeckHeight + VoidFloor - 0.3, z + DeckHeight + VoidFloor);
			AuScBarrier barrier;
			barrier.AddFaces(sheet);
			barrier.SetFaceOpacity(VoidOpacity);
			Add(barrier, QString("top of the stairs %1").arg(Where(d)));
			AddWallBox(onto, head, across, lastDir, -0.5, VoidInset + VoidOverlap + 0.6, last.half - VoidInset,
					z + DeckHeight + VoidFloor - DoorwayDip, z + DeckHeight + DoorwayHeight);
			AddRoomVoid(onto, high, 0, QString("void at the top of the stairs %1").arg(Where(d)));
			const int shaft = RoomAt(d.deck + 1, head - lastDir * 0.5, 0.0);
			bool opening = false;
			for(int o = 0; o < m_OpenSpans.size(); o++)
				opening = opening || (m_OpenSpans[o].deck == d.deck + 1 && qAbs((head - m_OpenSpans[o].mid).Dot(m_OpenSpans[o].u)) <= m_OpenSpans[o].half
						&& qAbs((head - m_OpenSpans[o].mid).Cross(m_OpenSpans[o].u)) < 0.3);
			if(shaft >= 0 && shaft != high && !opening)
			{
				Log(QString("WARNING: the stairs %1 arrive at the wall between %2 and %3, where the old design has no wall "
						"opening. The way is open all the same: people pass through what is drawn there.").arg(Where(d))
						.arg(m_Rooms[shaft].name).arg(m_Rooms[high].name));
				Span span = { d.deck + 1, head, across, last.half };
				m_OpenSpans.append(span);
			}
		}
		for(int k = 0; k + 1 < info.walk.size(); k++)
			AddWalkPath(info.walk[k] + AuPoint(0, 0, PathHeight), info.walk[k + 1] + AuPoint(0, 0, PathHeight), 0);
		DoorSide sl = { low, info.walk.first() + AuPoint(0, 0, PathHeight), before };
		m_DoorSides.append(sl);
		DoorSide sh = { high, info.walk.last() + AuPoint(0, 0, PathHeight), after };
		m_DoorSides.append(sh);
		m_Info.stairs.append(info);
		made++;
	}
	if(made)
		Log(QString("Stairs: %1 lead from deck to deck, each flight a ramp (a barrier under the line of its steps) with a void "
				"over it, under an opening in the deck above that is over all of them.").arg(made));
	if(shut)
		Log(QString("Stairs that lead nowhere: %1, drawn only, for want of an opening in the deck above or of a floor to arrive "
				"on (see the notes).").arg(shut));
}

}
