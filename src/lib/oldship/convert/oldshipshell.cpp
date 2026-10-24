#include "oldshipconverter.h"

#include <cmath>

namespace OldShip
{

namespace
{

Vec3 Middle(const Tri& t)
{
	return Vec3((t.v[0].pos.x + t.v[1].pos.x + t.v[2].pos.x) / 3, (t.v[0].pos.y + t.v[1].pos.y + t.v[2].pos.y) / 3,
			(t.v[0].pos.z + t.v[1].pos.z + t.v[2].pos.z) / 3);
}

QString DetailName(int t)
{
	if(t == 10) return QString("maneuver drive");
	if(t == 14) return QString("power plant");
	if(t == 16) return QString("sensors");
	if(t == 18) return QString("shields");
	if(t == 23) return QString("weapon system");
	if(t == 24) return QString("FTL drive");
	if(t == 32) return QString("life support");
	if(t == 11) return QString("surgery unit");
	if(t == 20) return QString("transporter pad");
	if(DetailType::Bunks(t) == 2) return QString("double berth");
	if(DetailType::Bunks(t) == 1) return QString("single berth");
	if(DetailType::IsAnyHatch(t)) return QString("ladder");
	if(DetailType::IsStairs(t)) return QString("stairs");
	return QString("detail type %1").arg(t);
}

void AddKeel(const Converter& c, AuScFaceList& faces, const QVector<Tri>& tris)
{
	for(int i = 0; i < tris.size(); i++)
	{
		const Tri& t = tris[i];
		const Vec3 n = Cross(t.v[1].pos - t.v[0].pos, t.v[2].pos - t.v[0].pos);
		const double length = sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
		if(length < 1e-8 || fabs(n.z) < 0.999 * length || t.v[0].normal.z >= 0)
			continue;
		const AuPoint down(0, 0, -1), drop(0, 0, -KeelDepth);
		const AuPoint a = c.At(t.v[0].pos), b = c.At(t.v[1].pos), d = c.At(t.v[2].pos);
		AddTri(faces, a, b, d, down);
		AddTri(faces, a + drop, b + drop, d + drop, down);
	}
}

void AddRoof(const Converter& c, AuScFaceList& faces, const QVector<Tri>& tris)
{
	for(int i = 0; i < tris.size(); i++)
	{
		const Tri& t = tris[i];
		const Vec3 n = Cross(t.v[1].pos - t.v[0].pos, t.v[2].pos - t.v[0].pos);
		const double length = sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
		if(length < 1e-8 || fabs(n.z) < 0.999 * length || t.v[0].normal.z <= 0)
			continue;
		const AuPoint up(0, 0, 1), under(0, 0, -(DeckHeight - VoidCeiling) + SolidSkin), over(0, 0, c.RoofSolidRise());
		const AuPoint a = c.At(t.v[0].pos), b = c.At(t.v[1].pos), d = c.At(t.v[2].pos);
		AddTri(faces, a + under, b + under, d + under, up);
		AddTri(faces, a + over, b + over, d + over, up);
	}
}

}

void Converter::BuildHull()
{
	QMap<int, QVector<Tri> > byDeck;
	int keels = 0, roofs = 0;
	for(int i = 0; i < m_Hull.size(); i++)
	{
		const Tri& t = m_Hull[i];
		const Vec3 mid = Middle(t);
		int deck;
		const double z0 = t.v[0].pos.z;
		if(fabs(t.v[1].pos.z - z0) < 1e-6 && fabs(t.v[2].pos.z - z0) < 1e-6)
		{
			const int level = ClosestDeck(z0);
			deck = t.v[0].normal.z > 0 ? level - 1 : level;
		}
		else
			deck = (int)floor(mid.z / DeckHeight);
		deck = qBound(m_Ship.minDeck, deck, m_Ship.maxDeck);
		byDeck[deck].append(t);
	}
	for(QMap<int, QVector<Tri> >::const_iterator di = byDeck.constBegin(); di != byDeck.constEnd(); di++)
	{
		AuScFaceList faces;
		AddFaces(faces, di.value());
		if(faces.isEmpty())
			continue;
		AuScHull hull;
		hull.AddFaces(faces);
		if(Add(hull, QString("hull of deck %1").arg(di.key())))
			m_Info.hullTriangles += faces.size();

		AuScFaceList keel;
		AddKeel(*this, keel, di.value());
		if(!keel.isEmpty())
		{
			AuScBarrier barrier;
			barrier.AddFaces(keel);
			barrier.SetFaceOpacity(VoidOpacity);
			if(Add(barrier, QString("solid under the bottom of deck %1").arg(di.key())))
				keels++;
		}
		AuScFaceList roof;
		AddRoof(*this, roof, di.value());
		if(!roof.isEmpty())
		{
			AuScBarrier barrier;
			barrier.AddFaces(roof);
			barrier.SetFaceOpacity(VoidOpacity);
			if(Add(barrier, QString("solid over the top of deck %1").arg(di.key())))
				roofs++;
		}
	}
	for(int i = 0; i < m_Doors.size(); i++)
	{
		AuScFaceList plug;
		if(m_DoorMade[i] && m_DoorPart[i])
			AddKeel(*this, plug, m_Doors[i].outside);
		if(plug.isEmpty())
			continue;
		AuScBarrier barrier;
		barrier.AddFaces(plug);
		barrier.SetFaceOpacity(VoidOpacity);
		barrier.SetAssociatedPart(m_DoorPart[i], AuScDoor::Closed);
		if(Add(barrier, QString("solid under the hull hatch %1").arg(Where(m_Ship.details[m_Doors[i].detail]))))
			keels++;
	}
	for(int i = 0; i < m_Doors.size(); i++)
	{
		AuScFaceList plug;
		if(m_DoorMade[i] && m_DoorPart[i])
			AddRoof(*this, plug, m_Doors[i].outside);
		if(plug.isEmpty())
			continue;
		AuScBarrier barrier;
		barrier.AddFaces(plug);
		barrier.SetFaceOpacity(VoidOpacity);
		barrier.SetAssociatedPart(m_DoorPart[i], AuScDoor::Closed);
		if(Add(barrier, QString("solid over the hull hatch %1").arg(Where(m_Ship.details[m_Doors[i].detail]))))
			roofs++;
	}
	if(roofs)
		Log(QString("Solid over the hull's top: %1 barriers (not drawn) from the ceiling of the rooms' voids to %2 ft over the "
				"drawn top, so nobody passes up through the roof.").arg(roofs).arg(RoofSolidRise()));
	BuildWallSolids();
	if(keels)
		Log(QString("Solid under the hull's bottom: %1 barriers (not drawn) %2 ft deep, so nobody passes down through the "
				"lowest floors.").arg(keels).arg(KeelDepth));
}

void Converter::BuildWallSolids()
{
	const double wallDepth = WallSolidDepth();
	int made = 0;
	QMap<int, AuScFaceList*> byDeck;
	for(int r = 0; r < m_Rooms.size(); r++)
	{
		const RoomData& rd = m_Rooms[r];
		const int n = rd.poly.size();
		Polygon inside;
		if(rd.inner.size() != n || n < 3 || !Inset(rd.inner, -SolidSkin, inside))
			continue;
		const double z0 = rd.floorZ - (DeckHeight - VoidCeiling) + 0.3 + OffGrid, z1 = rd.floorZ + VoidCeiling + 0.3 + OffGrid;
		for(int k = 0; k < n; k++)
		{
			const P2& a = rd.poly[k];
			const P2& b = rd.poly[(k + 1) % n];
			const double length = (b - a).Length();
			const P2 u = (b - a).Unit(), out = u.Left() * -1.0;
			const int count = qMax(1, (int)floor(length / 0.25 + 0.5));
			QVector<int> solid(count);
			for(int i = 0; i < count; i++)
			{
				const P2 q = a + u * (length * (i + 0.5) / count);
				int is = RoomAt(rd.deck, q + out * 0.3) < 0 ? 1 : 0;
				for(int o = 0; o < m_OpenSpans.size() && is; o++)
					if(m_OpenSpans[o].deck == rd.deck && qAbs(u.Cross(m_OpenSpans[o].u)) < 0.1
							&& qAbs((q - m_OpenSpans[o].mid).Dot(m_OpenSpans[o].u)) <= m_OpenSpans[o].half + 0.15
							&& qAbs((q - m_OpenSpans[o].mid).Cross(m_OpenSpans[o].u)) < 0.5)
						is = 0;
				for(int o = 0; o < m_HullDoorSpans.size() && is; o++)
				{
					const HullDoorSpan& span = m_HullDoorSpans[o];
					if(span.deck != rd.deck || qAbs(u.Cross(span.u)) > 0.1 || qAbs((q - span.mid).Cross(span.u)) > 0.5)
						continue;
					const double off = qAbs((q - span.mid).Dot(span.u));
					if(off <= span.half + 0.15)
						is = 0;
					else if(off <= span.half + 1.3)
						is = 2;
				}
				for(int o = 0; o < m_WallClear.size() && is; o++)
					if(m_WallClear[o].deck == rd.deck && (q - m_WallClear[o].at).Length() < m_WallClear[o].radius)
						is = 0;
				solid[i] = is;
			}
			const P2& before = rd.poly[(k + n - 1) % n];
			const P2& after = rd.poly[(k + 2) % n];
			const P2 ub = (a - before).Unit(), ua = (after - b).Unit();
			const bool turnsAtStart = RoomAt(rd.deck, a - ub * 0.2 + ub.Left() * -0.3) < 0;
			const bool turnsAtEnd = RoomAt(rd.deck, b + ua * 0.2 + ua.Left() * -0.3) < 0;
			const double pastStart = turnsAtStart ? (ub.Cross(u) > 0 ? wallDepth + OffGrid : -OffGrid) : OffGrid;
			const double pastEnd = turnsAtEnd ? (u.Cross(ua) > 0 ? wallDepth + OffGrid : -OffGrid) : OffGrid;
			const double skinIn = (inside[k] - a).Dot(out) * -1.0;
			for(int i = 0; i < count; )
			{
				if(!solid[i])
				{
					i++;
					continue;
				}
				int j = i;
				while(j < count && solid[j] == solid[i])
					j++;
				const bool thin = solid[i] == 2;
				const double s0 = length * i / count + OffGrid, s1 = length * j / count + OffGrid, depth = thin ? 0.05 : wallDepth;
				P2 in0 = a + u * s0 - out * skinIn, out0 = a + u * s0 + out * depth;
				P2 in1 = a + u * s1 - out * skinIn, out1 = a + u * s1 + out * depth;
				if(i == 0)
				{
					in0 = turnsAtStart ? inside[k] : a - u * pastStart - out * skinIn;
					out0 = a - u * pastStart + out * depth;
				}
				if(j == count)
				{
					in1 = turnsAtEnd ? inside[(k + 1) % n] : b + u * pastEnd - out * skinIn;
					out1 = b + u * pastEnd + out * depth;
				}
				Polygon plan;
				plan << out0 << out1 << in1 << in0;
				if(SignedArea(plan) > 0.01)
				{
					QVector<AuPoint> bottom, top;
					for(int c = 0; c < 4; c++)
					{
						bottom.append(At(plan[c], z0));
						top.append(At(plan[c], z1));
					}
					if(!byDeck.contains(rd.deck))
						byDeck.insert(rd.deck, new AuScFaceList);
					AddPrism(*byDeck[rd.deck], bottom, top);
				}
				i = j;
			}
		}
	}
	for(QMap<int, AuScFaceList*>::iterator di = byDeck.begin(); di != byDeck.end(); di++)
	{
		AuScBarrier barrier;
		barrier.AddFaces(**di);
		barrier.SetFaceOpacity(VoidOpacity);
		if(Add(barrier, QString("solid in the hull walls of deck %1").arg(di.key())))
			made++;
		delete *di;
	}
	for(int i = 0; i < m_HullDoorSpans.size(); i++)
	{
		const HullDoorSpan& s = m_HullDoorSpans[i];
		const int room = RoomAt(s.deck, s.mid - s.n * 1.0);
		if(!Walkable(room))
			continue;
		const double floorZ = s.deck * DeckHeight;
		const double skinIn = DistanceTo(m_Rooms[room].inner, s.mid) - SolidSkin;
		if(skinIn <= 0)
			continue;
		AuScFaceList plug;
		AddWallBox(plug, s.mid, s.u, s.n, -skinIn, wallDepth, s.half + 0.15, floorZ - (DeckHeight - VoidCeiling) + 0.3, floorZ + VoidCeiling + 0.3);
		AuScBarrier barrier;
		barrier.AddFaces(plug);
		barrier.SetFaceOpacity(VoidOpacity);
		barrier.SetAssociatedPart(s.part, AuScDoor::Closed);
		if(Add(barrier, QString("solid in the hull door of part %1").arg(s.part)))
			made++;
	}
	if(made)
		Log(QString("Solid in the hull's walls: %1 barriers (not drawn), from the face of each room's void to %2 ft past the "
				"wall line, so nobody passes out through a wall.").arg(made).arg(wallDepth));
}

void Converter::BuildRoomSurfaces()
{
	QVector<QVector<Tri> > byRoom(m_Rooms.size() + 1);
	for(int i = 0; i < m_Inside.size(); i++)
	{
		const Tri& t = m_Inside[i];
		const Vec3 mid = Middle(t);
		const Vec3& n = t.v[0].normal;
		const Vec3 in(mid.x + n.x * 0.25, mid.y + n.y * 0.25, mid.z + n.z * 0.25);
		const int deck = qBound(m_Ship.minDeck, (int)floor(in.z / DeckHeight), m_Ship.maxDeck);
		const int room = RoomAt(deck, P2(in.x, in.y), 1e9);
		byRoom[room < 0 ? m_Rooms.size() : room].append(t);
	}
	for(int r = 0; r <= m_Rooms.size(); r++)
	{
		AuScFaceList faces;
		AddFaces(faces, byRoom[r]);
		if(faces.isEmpty())
			continue;
		AuScRoom room;
		room.AddFaces(faces);
		if(r < m_Rooms.size())
			room.SetGroupName(m_Rooms[r].name);
		room.SetInteriorLighting(false, INTLIGHT_Bright);
		if(Add(room, r < m_Rooms.size() ? QString("surfaces of %1").arg(m_Rooms[r].name) : QString("surfaces of no room")))
			m_Info.roomTriangles += faces.size();
	}
}

void Converter::BuildFittings()
{
	QMap<QString, int> made;
	QMap<int, int> noModel;
	QSet<QString> seen;
	int throughHull = 0;
	m_Design.Parts().RefreshObstructionsCache();
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const Detail& d = m_Ship.details[i];
		const int t = d.type;
		TriList tris;
		if(!RenderFitting(tris, d) || tris.empty())
		{
			noModel[t]++;
			continue;
		}
		double sx = 0, sy = 0, sz = 0, su = 0;
		for(size_t k = 0; k < tris.size(); k++)
			for(int v = 0; v < 3; v++)
			{
				sx += tris[k].v[v].pos.x;
				sy += tris[k].v[v].pos.y;
				sz += tris[k].v[v].pos.z;
				su += tris[k].v[v].u + tris[k].v[v].v;
			}
		const QString key = QString("%1/%2/%3/%4/%5").arg(tris.size()).arg(sx, 0, 'f', 2).arg(sy, 0, 'f', 2).arg(sz, 0, 'f', 2).arg(su, 0, 'f', 3);
		if(seen.contains(key))
		{
			Skip(QString("A second %1 %2 is left out: the game refuses two identical parts.").arg(DetailName(t)).arg(Where(d)));
			continue;
		}
		seen.insert(key);

		const bool ladder = DetailType::IsAnyHatch(t);
		const int deck = ladder ? d.deck - 1 : d.deck;
		const Vec3 loc = d.Loc();
		const int room = RoomAt(deck, P2(loc.x, loc.y), 0.05);
		if(room < 0)
		{
			Skip(QString("The %1 %2 has no room around it: its model is left out.").arg(DetailName(t)).arg(Where(d)));
			continue;
		}
		QVector<Tri> list;
		bool outside = false, inside = false;
		for(size_t k = 0; k < tris.size(); k++)
		{
			list.append(tris[k]);
			for(int v = 0; v < 3 && !(outside && inside); v++)
			{
				const bool in = m_Design.Parts().IsInsideObstructionModel(At(tris[k].v[v].pos));
				outside = outside || !in;
				inside = inside || in;
			}
		}
		if(!inside)
		{
			Skip(QString("The %1 %2 is wholly outside the hull: its model is left out (the game refuses it).")
					.arg(DetailName(t)).arg(Where(d)));
			continue;
		}
		AuScFaceList faces;
		AddFaces(faces, list);
		if(faces.isEmpty())
			continue;
		AuScMesh plain;
		AuScRoom poking;
		poking.SetInteriorLighting(false, INTLIGHT_Bright);
		AuScMeshBase& mesh = outside ? (AuScMeshBase&)poking : (AuScMeshBase&)plain;
		if(outside)
			throughHull++;
		mesh.AddFaces(faces);
		mesh.SetGroupName(m_Rooms[room].name);
		if(Add(mesh, QString("%1 %2").arg(DetailName(t)).arg(Where(d))))
		{
			m_Info.fittingTriangles += faces.size();
			made[DetailName(t)]++;
		}
	}
	QStringList list;
	for(QMap<QString, int>::const_iterator mi = made.constBegin(); mi != made.constEnd(); mi++)
		list.append(QString("%1 x %2").arg(mi.value()).arg(mi.key()));
	Log(QString("Equipment models kept as mesh parts: %1.").arg(list.isEmpty() ? QString("none") : list.join(", ")));
	if(throughHull)
		Log(QString("%1 of them reach out through the hull's solid (they stand on their room's edge); the game refuses such a "
				"mesh part, so those are room parts instead, which may.").arg(throughHull));
	static const int visualOnly[7] = { 10, 14, 16, 18, 23, 24, 32 };
	int engineering = 0;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
		for(int k = 0; k < 7; k++)
			if(m_Ship.details[i].type == visualOnly[k])
				engineering++;
	if(engineering)
		Log(QString("The %1 pieces of engineering equipment (drives, power plants, life support, shields, sensors, weapon "
				"systems) are decoration only: they give the design no systems.").arg(engineering));

}

}
