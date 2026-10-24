#include "oldshipconverter.h"

#include <memory>

#include <cmath>

namespace OldShip
{

namespace
{

bool DoorTile(const Tri& t, ShipTex& half)
{
	static const ShipTex tiles[9] =
	{
		ST_IntDoorHalfOpen, ST_CrewDoorHalfOpen, ST_HullDoorHalfOpen, ST_LargeDoorHalfOpen, ST_LargeCrewDoorHalfOpen,
		ST_LargeHullDoorHalfOpen, ST_IntHatchHalfOpen, ST_CrewHatchHalfOpen, ST_HullHatchHalfOpen,
	};
	const float u = (t.v[0].u + t.v[1].u + t.v[2].u) / 3, v = (t.v[0].v + t.v[1].v + t.v[2].v) / 3;
	for(int i = 0; i < 9; i++)
	{
		float s0, t0, s1, t1;
		ShipTexture(tiles[i], s0, t0, s1, t1);
		if(u >= s0 && u <= s1 && v >= t0 && v <= t1)
		{
			half = tiles[i];
			return true;
		}
	}
	return false;
}

}

int BuildDoorStates(const Converter& c, AuScDoor& door, const DoorData& dd)
{
	QVector<Tri> tris = dd.outside;
	tris += dd.inside;
	int count = 0;
	for(int state = AuScDoor::Closed; state < AuScDoor::STATES_Count; state++)
	{
		int picture;
		if(state == AuScDoor::Closed || state == AuScDoor::StuckClosed || state == AuScDoor::LockedClosed || state <= AuScDoor::Open30)
			picture = -1;
		else if(state >= AuScDoor::Open40 && state <= AuScDoor::Open70)
			picture = 0;
		else
			picture = 1;
		AuScFaceList faces;
		c.DoorPicture(faces, tris, picture);
		if(faces.isEmpty())
			return 0;
		if(state != AuScDoor::Closed)
			door.AddState((quint8)state);
		door.AddFaces(faces);
		count += faces.size();
	}
	door.SetState(AuScDoor::Closed);
	door.SetDoorProperties(Qt::Unchecked, Qt::Unchecked, Qt::Unchecked, Qt::Unchecked);
	return count;
}

void Converter::DoorPicture(AuScFaceList& faces, const QVector<Tri>& tris, int picture) const
{
	for(int i = 0; i < tris.size(); i++)
	{
		ShipTex half;
		if(DoorTile(tris[i], half))
			faces.append(Face(tris[i], half, (ShipTex)(half + picture)));
		else
			faces.append(Face(tris[i]));
	}
}

double Converter::HullOpeningReach(int roomType) const
{
	double reach = 0;
	std::vector<Vec3> profile;
	std::vector<int> runs;
	WallProfile(0x1d, m_Ship.armor, profile, runs);
	for(size_t k = 0; k < profile.size(); k++)
		reach = qMax(reach, profile[k].x);
	WallProfile(roomType, m_Ship.armor, profile, runs);
	for(size_t k = 0; k < profile.size(); k++)
		reach = qMax(reach, profile[k].x);
	return reach;
}

void Converter::BuildDoors()
{
	int made = 0;
	for(int di = 0; di < m_Doors.size(); di++)
	{
		const DoorData& dd = m_Doors[di];
		const Detail& d = m_Ship.details[dd.detail];
		if(DetailType::IsAnyHatch(d.type))
			continue;
		const QString typeName = d.type == 3 ? QString("large hull door") : d.type == 5 ? QString("door")
				: d.type == 49 ? QString("crew door") : d.type == 50 ? QString("hull door") : d.type == 53 ? QString("large door")
				: QString("large crew door");
		if(dd.outside.isEmpty() && dd.inside.isEmpty())
		{
			Skip(QString("The %1 %2 is not drawn by the old model (it is not in a wall it fits): no door is made.")
					.arg(typeName).arg(Where(d)));
			continue;
		}

		const Vec3 loc = d.Loc();
		P2 m(loc.x, loc.y);
		const double floorZ = d.deck * DeckHeight;
		QVector<Tri> tris = dd.outside;
		tris += dd.inside;
		double lowest = 1e30;
		for(int i = 0; i < tris.size(); i++)
			for(int k = 0; k < 3; k++)
				lowest = qMin(lowest, tris[i].v[k].pos.z);
		QVector<P2> foot;
		for(int i = 0; i < tris.size(); i++)
			for(int k = 0; k < 3; k++)
				if(tris[i].v[k].pos.z <= lowest + 0.01)
					foot.append(P2(tris[i].v[k].pos.x, tris[i].v[k].pos.y));
		P2 a = foot[0], b = foot[0];
		for(int i = 0; i < foot.size(); i++)
			for(int k = i + 1; k < foot.size(); k++)
				if((foot[i] - foot[k]).Length() > (a - b).Length())
				{
					a = foot[i];
					b = foot[k];
				}
		if((a - b).Length() < 1.0)
		{
			Skip(QString("The %1 %2 has no width in the old model: no door is made.").arg(typeName).arg(Where(d)));
			continue;
		}
		const P2 u = (b - a).Unit();
		P2 n = u.Left();
		const double half = (b - a).Length() / 2;
		const double offCentre = ((a + b) * 0.5 - m).Dot(u);
		if((d.type == 3 || d.type == 53 || d.type == 54) && qAbs(offCentre) > 0.05)
			m = m + u * offCentre;
		int roomA = RoomAt(d.deck, m - n * 1.0), roomB = RoomAt(d.deck, m + n * 1.0);
		if(roomA < 0 && roomB >= 0)
		{
			n = n * -1.0;
			qSwap(roomA, roomB);
		}
		if(roomA < 0)
		{
			Skip(QString("The %1 %2 has no room on either side: no door is made.").arg(typeName).arg(Where(d)));
			continue;
		}
		const bool hull = roomB < 0;
		double reach = 0;
		for(int i = 0; i < tris.size(); i++)
			for(int k = 0; k < 3; k++)
				reach = qMax(reach, (P2(tris[i].v[k].pos.x, tris[i].v[k].pos.y) - m).Dot(n));

		const bool opening = DoorsAreOpenings();
		quint16 id = 0;
		if(!opening)
		{
			AuScDoor door;
			const int faces = BuildDoorStates(*this, door, dd);
			door.SetHullDoor(hull);
			const double outer = hull ? reach + 0.4 : 0.4;
			door.SetDoorOpLocs(At(m - n * 0.4, floorZ + SwitchHeight), At(m + n * outer, floorZ + SwitchHeight));
			door.SetGroupName(m_Rooms[roomA].name);
			id = Add(door, QString("%1 %2").arg(typeName).arg(Where(d)));
			if(!id)
				continue;
			m_Info.doorTriangles += faces;
		}
		made++;
		m_DoorMade[di] = true;
		m_DoorPart[di] = id;

		ConvertInfo::DoorInfo info;
		if(opening)
			info.kind = hull ? ConvertInfo::DoorInfo::HullOpening : ConvertInfo::DoorInfo::Opening;
		else
			info.kind = hull ? ConvertInfo::DoorInfo::HullDoor : ConvertInfo::DoorInfo::Door;
		info.detailType = d.type;
		info.detailId = d.id;
		info.part = id;
		info.roomA = RoomName(roomA);
		info.roomB = RoomName(roomB);
		info.ladder = 0;
		info.mid = At(m + n * (hull ? reach / 2 : 0.0), floorZ + 1.0);

		const double way = half > 2.6 ? half - 0.75 : qMin(DoorwayHalfWidth, half - 0.4);
		const double z0 = floorZ + VoidFloor - DoorwayDip, z1 = floorZ + DoorwayHeight;
		const double into = VoidInset + VoidOverlap;
		const bool wayA = Walkable(roomA), wayB = Walkable(roomB);
		if(!wayA || (!hull && !wayB))
			Log(QString("The %1 %2 leads into %3, which nobody can enter: it opens but goes nowhere.").arg(typeName).arg(Where(d))
					.arg(RoomName(wayA ? roomB : roomA)));
		if(opening && !hull)
		{
			AuScFaceList boxA, boxB;
			AddWallBox(boxA, m, u, n, wayA ? -into : -0.1, 0.15, way, z0, z1);
			AddRoomVoid(boxA, roomA, 0, QString("doorway void of the %1 %2").arg(typeName).arg(Where(d)));
			AddWallBox(boxB, m, u, n, -0.15, wayB ? into : 0.1, way, z0, z1);
			AddRoomVoid(boxB, roomB, 0, QString("doorway void of the %1 %2").arg(typeName).arg(Where(d)));
		}
		else
		{
			AuScFaceList box;
			AddWallBox(box, m, u, n, wayA ? -into : -0.1, hull ? DoorwayReach(reach) : (wayB ? into : 0.1), way, z0, z1);
			AddRoomVoid(box, roomA, id, QString("doorway void of the %1 %2").arg(typeName).arg(Where(d)));
		}
		if(opening)
		{
			Span span = { d.deck, m, u, way };
			m_OpenSpans.append(span);
		}
		double outside = 2.0;
		if(hull)
		{
			if(!opening)
			{
				HullDoorSpan span = { d.deck, m, u, n, way, id };
				m_HullDoorSpans.append(span);
			}
			AuScFaceList through, beyond;
			const double from = HullVoidFrom(reach);
			AddWallBox(through, m, u, n, from, qMax(reach + 0.3, from + 0.3), way, z0, z1);
			AddHullVoid(through, id, QString("hull void in the %1 %2").arg(typeName).arg(Where(d)));
			AddWallBox(beyond, m, u, n, qMax(reach + 0.1, from), reach + HullMargin + 1.5, way + 1.0, z0, z1);
			outside = reach + HullMargin + 1.0;
			AddHullVoid(beyond, id, QString("hull void outside the %1 %2").arg(typeName).arg(Where(d)));
			HullWay hw = { d.deck, m, u, n, way + 1.0, reach + HullMargin + 1.5, id, QString("%1 %2").arg(typeName).arg(Where(d)) };
			m_HullWays.append(hw);
			if(HullVoidDrop(d.deck) > 0)
			{
				AuScFaceList pit;
				AddGroundPit(pit, d.deck, m, u, n, from, reach + HullMargin + 1.5, way, z0 - HullVoidDrop(d.deck), z0 + 0.1);
				if(!pit.isEmpty())
					AddHullVoid(pit, id, QString("hull void in the ground outside the %1 %2").arg(typeName).arg(Where(d)));
			}
		}

		const double inside = PathReach(roomA, m, n * -1.0);
		if(!hull)
			outside = PathReach(roomB, m, n);
		const AuPoint endA = At(m - n * inside, floorZ + PathHeight), endB = At(m + n * outside, floorZ + PathHeight);
		info.sideA = At(m - n * inside, floorZ + 1.0);
		info.sideB = At(m + n * outside, floorZ + 1.0);
		if(wayA && (hull || wayB))
		{
			AddWalkPath(endA, endB, id);
			DoorSide sa = { roomA, endA, m - n * inside };
			m_DoorSides.append(sa);
			if(!hull)
			{
				DoorSide sb = { roomB, endB, m + n * outside };
				m_DoorSides.append(sb);
			}
		}
		m_Info.doors.append(info);
	}
	if(DoorsAreOpenings())
		Log(QString("Doors in walls: %1, each an opening without a door, as the old buildings had.").arg(made));
	else
		Log(QString("Doors in walls: %1, each a manual door opened by clicking it, with every door state.").arg(made));
}

void Converter::AddGroundPit(AuScFaceList& pit, int deck, const P2& m, const P2& u, const P2& n, double from, double to, double half,
		double zLow, double zHigh)
{
	const int strips = qMax(1, (int)ceil(2 * half / 1.0)), steps = qMax(1, (int)ceil((to - from) / 0.5));
	const double width = 2 * half / strips, step = (to - from) / steps;
	QVector<bool> closeBy(strips * steps, false);
	int cut = 0;
	for(int s = 0; s < strips; s++)
		for(int i = 0; i < steps; i++)
		{
			const P2 middle = m + u * (-half + (s + 0.5) * width) + n * (from + (i + 0.5) * step);
			const P2 at[5] = { middle, middle - u * (width / 2) - n * (step / 2), middle + u * (width / 2) - n * (step / 2),
					middle + u * (width / 2) + n * (step / 2), middle - u * (width / 2) + n * (step / 2) };
			bool close = false;
			for(int r = 0; r < m_Rooms.size() && !close; r++)
			{
				if(m_Rooms[r].deck != deck - 1 || m_Rooms[r].inner.isEmpty())
					continue;
				for(int k = 0; k < 5 && !close; k++)
					close = DistanceTo(m_Rooms[r].inner, at[k]) < 1.02;
			}
			closeBy[s * steps + i] = close;
			if(close)
				cut++;
		}
	if(!cut)
	{
		AddWallBox(pit, m, u, n, from, to, half, zLow, zHigh);
		return;
	}
	for(int s = 0; s < strips; s++)
	{
		const P2 middle = m + u * (-half + (s + 0.5) * width);
		for(int i = 0; i < steps; )
		{
			if(closeBy[s * steps + i])
			{
				i++;
				continue;
			}
			int j = i;
			while(j < steps && !closeBy[s * steps + j])
				j++;
			AddWallBox(pit, middle, u, n, from + i * step, from + j * step, width / 2, zLow, zHigh);
			i = j;
		}
	}
	if(cut)
		Log(QString("The hull void in the ground outside the doorway at (%1, %2) on deck %3 leaves out %4 of its %5 parts: a room "
				"of the deck below lies under them, whose void it must not come near.").arg(m.x, 0, 'f', 1).arg(m.y, 0, 'f', 1).arg(deck)
				.arg(cut).arg(strips * steps));
}

void Converter::BuildOpenings()
{
	int made = 0;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const Detail& d = m_Ship.details[i];
		if(d.type != 55 && d.type != 9)
			continue;
		const QString typeName = d.type == 55 ? QString("wall opening") : QString("hull opening");
		const Vec3 loc = d.Loc();
		const P2 m(loc.x, loc.y);
		const double floorZ = d.deck * DeckHeight;
		bool found = false;
		P2 u;
		for(int r = 0; r < m_Rooms.size() && !found; r++)
		{
			const RoomData& rd = m_Rooms[r];
			if(rd.deck != d.deck)
				continue;
			for(int k = 0; k < rd.poly.size() && !found; k++)
			{
				const P2& a = rd.poly[k];
				const P2& b = rd.poly[(k + 1) % rd.poly.size()];
				const P2 e = (b - a).Unit();
				const double along = (m - a).Dot(e);
				if(qAbs((m - a).Cross(e)) < 1e-3 && along > 1e-3 && along < (b - a).Length() - 1e-3)
				{
					u = e;
					found = true;
				}
			}
		}
		if(!found)
		{
			Skip(QString("The %1 %2 is not on a wall: nothing is made for it.").arg(typeName).arg(Where(d)));
			continue;
		}
		P2 n = u.Left();
		int roomA = RoomAt(d.deck, m - n * 1.0), roomB = RoomAt(d.deck, m + n * 1.0);
		if(roomA < 0 && roomB >= 0)
		{
			n = n * -1.0;
			qSwap(roomA, roomB);
		}
		const bool hull = roomB < 0;
		if(roomA < 0 || !Walkable(roomA) || (!hull && !Walkable(roomB)) || (hull != (d.type == 9)))
		{
			Skip(QString("The %1 %2 does not join two rooms people can enter%3: no way through is made.").arg(typeName).arg(Where(d))
					.arg(d.type == 9 ? QString(" (or a room and the outside)") : QString()));
			continue;
		}
		double back, ahead;
		WallStretch(d.deck, m, u, n, back, ahead);
		const P2 mid = m + u * ((ahead - back) / 2);
		bool twice = false;
		for(int o = 0; o < m_OpenSpans.size(); o++)
			twice = twice || (m_OpenSpans[o].deck == d.deck && (m_OpenSpans[o].mid - mid).Length() < 0.1);
		if(twice)
		{
			Skip(QString("The %1 %2 is in a stretch of wall another opening has already opened: nothing more is made.")
					.arg(typeName).arg(Where(d)));
			continue;
		}
		const double half = (ahead + back) / 2 - (hull ? 1.0 : 0.0) - (VoidInset + 0.1);
		if(half < 0.75)
		{
			Skip(QString("The %1 %2 is too narrow to walk through.").arg(typeName).arg(Where(d)));
			continue;
		}
		const double z0 = floorZ + VoidFloor - DoorwayDip, z1 = floorZ + DoorwayHeight;
		const double into = VoidInset + VoidOverlap;

		ConvertInfo::DoorInfo info;
		info.kind = hull ? ConvertInfo::DoorInfo::HullOpening : ConvertInfo::DoorInfo::Opening;
		info.detailType = d.type;
		info.detailId = d.id;
		info.part = 0;
		info.ladder = 0;
		info.roomA = RoomName(roomA);
		info.roomB = RoomName(roomB);
		double outside = 2.0;
		if(!hull)
		{
			AuScFaceList boxA, boxB;
			AddWallBox(boxA, mid, u, n, -into, 0.15, half, z0, z1);
			AddRoomVoid(boxA, roomA, 0, QString("void of the %1 %2").arg(typeName).arg(Where(d)));
			AddWallBox(boxB, mid, u, n, -0.15, into, half, z0 - DoorwayDip, z1);
			AddRoomVoid(boxB, roomB, 0, QString("void of the %1 %2").arg(typeName).arg(Where(d)));
			info.mid = At(mid, floorZ + 1.0);
		}
		else
		{
			const double reach = HullOpeningReach(m_Rooms[roomA].type);
			AuScFaceList box, through, beyond;
			AddWallBox(box, mid, u, n, -into, DoorwayReach(reach), half, z0, z1);
			AddRoomVoid(box, roomA, 0, QString("void of the %1 %2").arg(typeName).arg(Where(d)));
			const double from = HullVoidFrom(reach);
			AddWallBox(through, mid, u, n, from, qMax(reach + 0.3, from + 0.3), half, z0, z1);
			AddHullVoid(through, 0, QString("hull void in the %1 %2").arg(typeName).arg(Where(d)));
			AddWallBox(beyond, mid, u, n, qMax(reach + 0.1, from), reach + HullMargin + 1.5, half, z0, z1);
			outside = reach + HullMargin + 1.0;
			AddHullVoid(beyond, 0, QString("hull void outside the %1 %2").arg(typeName).arg(Where(d)));
			HullWay hw = { d.deck, mid, u, n, half, reach + HullMargin + 1.5, 0, QString("%1 %2").arg(typeName).arg(Where(d)) };
			m_HullWays.append(hw);
			if(HullVoidDrop(d.deck) > 0)
			{
				AuScFaceList pit;
				AddGroundPit(pit, d.deck, mid, u, n, from, reach + HullMargin + 1.5, half, z0 - HullVoidDrop(d.deck), z0 + 0.1);
				if(!pit.isEmpty())
					AddHullVoid(pit, 0, QString("hull void in the ground outside the %1 %2").arg(typeName).arg(Where(d)));
			}
			info.mid = At(mid + n * (reach / 2), floorZ + 1.0);
		}
		const double inside = PathReach(roomA, mid, n * -1.0);
		if(!hull)
			outside = PathReach(roomB, mid, n);
		const AuPoint endA = At(mid - n * inside, floorZ + PathHeight), endB = At(mid + n * outside, floorZ + PathHeight);
		info.sideA = At(mid - n * inside, floorZ + 1.0);
		info.sideB = At(mid + n * outside, floorZ + 1.0);
		AddWalkPath(endA, endB, 0);
		DoorSide sa = { roomA, endA, mid - n * inside };
		m_DoorSides.append(sa);
		if(!hull)
		{
			DoorSide sb = { roomB, endB, mid + n * outside };
			m_DoorSides.append(sb);
		}
		Span span = { d.deck, mid, u, (ahead + back) / 2 };
		m_OpenSpans.append(span);
		m_Info.doors.append(info);
		made++;
	}
	if(made)
		Log(QString("Openings without a door: %1, always open.").arg(made));
}

void Converter::BuildPaths()
{
	int made = 0;
	for(int i = 0; i < m_DoorSides.size(); i++)
	{
		const DoorSide& s = m_DoorSides[i];
		if(!Walkable(s.room) || !Contains(m_Rooms[s.room].inner, s.plan))
			continue;
		const RoomData& rd = m_Rooms[s.room];
		const double floorZ = rd.openBelow ? rd.floorZ - DeckHeight : rd.floorZ;
		if(rd.openBelow)
			continue;
		if(AddWalkPath(At(rd.centre, floorZ + PathHeight), s.at, 0))
			made++;
	}
	Log(QString("Walk paths: one through every door, hatch and stair, and %1 from doorways to the middles of their rooms.").arg(made));
}

void Converter::ClearHullWays()
{
	if(m_HullWays.isEmpty())
		return;
	std::unique_ptr<AuDesign> preview(m_Design.Clone());
	preview->FinalizePreview();
	const AuScParts& parts = preview->Parts();
	const double Step = 0.5, Clear = 3.0, Longest = 500.0;
	int lengthened = 0;
	for(int i = 0; i < m_HullWays.size(); i++)
	{
		const HullWay& w = m_HullWays[i];
		const double floorZ = w.deck * DeckHeight;
		const double z0 = floorZ + VoidFloor - DoorwayDip, z1 = qMin(floorZ + DoorwayHeight, floorZ + DeckHeight - KeelDepth - 0.5);
		double d = w.from, open = -1;
		for(; d < Longest; d += Step)
		{
			bool solid = false;
			for(int a = -1; a <= 1 && !solid; a++)
				for(int h = 0; h < 3 && !solid; h++)
				{
					const P2 p = w.mid + w.n * d + w.u * (a * (w.half - 0.1));
					const double z = h == 0 ? z0 + 0.3 : (h == 1 ? (z0 + z1) / 2 : z1 - 0.3);
					solid = parts.IsInsideObstructionModel(At(p, z));
				}
			if(solid)
				open = -1;
			else if(open < 0)
				open = d;
			if(open >= 0 && d - open >= Clear)
				break;
		}
		if(open < 0 || d >= Longest)
		{
			Note(QString("The %1 leads out under or between parts of the hull for more than %2 ft: nobody outside can walk to it.")
					.arg(w.what).arg(Longest, 0, 'f', 0));
			continue;
		}
		if(open <= w.from)
			continue;
		const double to = open + Clear;
		int crossed = -1;
		for(double s = w.from; s <= to && crossed < 0; s += Step)
			for(int a = -1; a <= 1 && crossed < 0; a++)
				crossed = RoomAt(w.deck, w.mid + w.n * s + w.u * (a * (w.half - 0.1)), 0.0);
		if(crossed >= 0)
		{
			Note(QString("The %1 leads out to %2 a few feet off: its way to open air would run through that room's wall, and is "
					"left as it is.").arg(w.what).arg(m_Rooms[crossed].name));
			continue;
		}
		AuScFaceList beyond;
		AddWallBox(beyond, w.mid, w.u, w.n, w.from - 0.5, to, w.half, z0, z1);
		if(AddHullVoid(beyond, w.part, QString("hull void out to open air from the %1").arg(w.what)))
		{
			lengthened++;
			Log(QString("The %1 is under or between parts of the hull that stand out past it: its hull void reaches %2 ft out, to "
					"open air.").arg(w.what).arg(to, 0, 'f', 1));
		}
	}
	if(lengthened)
		Log(QString("Ways out lengthened to open air: %1.").arg(lengthened));
}

}
