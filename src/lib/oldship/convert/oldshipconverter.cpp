#include "oldshipconverter.h"

#include "audef.h"
#include "autexturelib.h"
#include <cmath>

namespace OldShip
{

bool Convert(const Design& ship, AuScDesign& out, QStringList& log, ConvertInfo* info, const ConvertOptions* options)
{
	ConvertInfo own;
	Converter c(ship, out, log, info ? *info : own, options ? *options : ConvertOptions());
	return c.Run();
}

Converter::Converter(const Design& ship, AuDesign& design, QStringList& log, ConvertInfo& info, const ConvertOptions& options)
	: m_Ship(ship), m_Design(design), m_Log(log), m_Info(info), m_Options(options), m_Builder(ship), m_Failed(0)
{
}

bool Converter::Run()
{
	if(m_Ship.rooms.empty())
	{
		Log("The old design has no rooms.");
		return false;
	}
	ReadRooms();
	RenderModels();
	if(m_Hull.isEmpty())
	{
		Log("The old design gives no hull.");
		return false;
	}
	Log(QString("Old design \"%1\": %2 rooms on decks %3 to %4, %5 details. %6").arg(QString::fromUtf8(m_Ship.name.c_str()))
			.arg(m_Ship.rooms.size()).arg(m_Ship.minDeck).arg(m_Ship.maxDeck).arg(m_Ship.details.size())
			.arg(m_Options.keepOrigin ? QString("The new design keeps the old design's origin.")
				: QString("The new design's origin is %4: old (%1, %2, %3) ft.")
					.arg(m_Offset.x, 0, 'f', 2).arg(m_Offset.y, 0, 'f', 2).arg(m_Offset.z, 0, 'f', 2)
					.arg(m_Info.pivot ? QString("the old design's pivot point") : QString("the middle of the hull"))));

	BuildRoomVoids();
	BuildDoors();
	BuildOpenings();
	BuildHatches();
	BuildStairs();
	for(int i = 0; i < m_Doors.size(); i++)
		if(!m_DoorMade[i])
		{
			m_Hull += m_Doors[i].outside;
			m_Inside += m_Doors[i].inside;
		}
	BuildHull();
	BuildSurroundings();
	BuildRoomSurfaces();
	BuildFittings();
	BuildBerths();
	BuildStations();
	BuildGameTables();
	BuildParking();
	BuildEmitters();
	ReportRailings();
	ReportTurrets();
	ReportTheRest();
	ClearHullWays();
	BuildPaths();
	BuildWallProbes();
	SetProperties();
	if(!m_Duplicates.isEmpty())
	{
		QStringList kinds;
		int count = 0;
		for(QMap<QString, int>::const_iterator di = m_Duplicates.constBegin(); di != m_Duplicates.constEnd(); di++)
		{
			kinds.append(QString("%1 x %2").arg(di.value()).arg(di.key()));
			count += di.value();
		}
		Note(QString("%1 parts were left out, each the exact duplicate of a part already made (%2): the game refuses a "
				"blueprint with two parts that are the same in every way.").arg(count).arg(kinds.join(", ")));
	}

	Log(QString("Made %1 parts: %2 hull triangles, %3 of rooms, %4 of doors and hatches (all their states), %5 of fittings.")
			.arg(m_Design.Parts().size()).arg(m_Info.hullTriangles).arg(m_Info.roomTriangles).arg(m_Info.doorTriangles)
			.arg(m_Info.fittingTriangles));
	if(m_Failed)
	{
		Log(QString("%1 parts were refused by the design.").arg(m_Failed));
		return false;
	}
	return CheckSize();
}

void Converter::ReadRooms()
{
	QMap<int, int> count;
	for(size_t i = 0; i < m_Ship.rooms.size(); i++)
	{
		const Room& r = m_Ship.rooms[i];
		RoomData rd;
		rd.index = (int)i;
		rd.deck = r.deck;
		rd.type = r.type;
		rd.floorZ = r.deck * DeckHeight;
		rd.openBelow = false;
		Polygon raw;
		for(size_t k = 0; k < r.points.size(); k++)
			raw.append(P2(r.points[k].x * GridUnit, r.points[k].y * GridUnit));
		rd.poly = Tidied(raw);

		QString typeName = RoomTypeName(r.type);
		if(typeName.isEmpty())
			typeName = QString("Room");
		rd.name = QString("%1 %2").arg(typeName).arg(++count[r.type]);

		const bool enterable = rd.poly.size() >= 3 && r.type != 0 && r.type != 9 && r.type != 12 && (r.type < 22 || r.type > 24);
		const bool narrow = enterable && !Inset(rd.poly, VoidInset, rd.inner) && !Inset(rd.poly, VoidInset / 2, rd.inner);
		if(narrow)
			rd.inner.clear();
		rd.centre = PointInside(rd.inner.isEmpty() ? rd.poly : rd.inner);
		if(narrow)
			SkipRoom(QString("%1 (deck %2) is too narrow for a room void: nobody can enter it.").arg(rd.name).arg(r.deck), rd);
		m_Rooms.append(rd);
	}

	for(int i = 0; i < m_Rooms.size(); i++)
	{
		RoomData& rd = m_Rooms[i];
		if((rd.type == 15 || rd.type == 11) && !rd.inner.isEmpty())
		{
			const int under = RoomAt(rd.deck - 1, rd.centre);
			rd.openBelow = Walkable(under) && Contains(m_Rooms[under].inner, rd.centre);
			for(int k = 0; k < rd.inner.size() && rd.openBelow; k++)
			{
				const P2 mid = (rd.inner[k] + rd.inner[(k + 1) % rd.inner.size()]) * 0.5;
				rd.openBelow = Walkable(RoomAt(rd.deck - 1, rd.inner[k])) && Walkable(RoomAt(rd.deck - 1, mid));
			}
			if(!rd.openBelow)
				Log(QString("%1 (deck %2) is not wholly over rooms of the deck below: it gets a floor people stand on, like any "
						"other room, though the old model draws none.").arg(rd.name).arg(rd.deck));
		}
	}

	for(int i = 0; i < m_Rooms.size(); i++)
		for(int j = 0; j < m_Rooms.size(); j++)
			if(i != j && m_Rooms[i].deck == m_Rooms[j].deck && !m_Rooms[j].inner.isEmpty() && !m_Rooms[i].inner.isEmpty()
					&& Contains(m_Rooms[j].inner, m_Rooms[i].centre))
				SkipRoom(QString("%1 lies inside the outline of %2 (deck %3): their voids overlap, the wall between them is not solid.")
						.arg(m_Rooms[i].name).arg(m_Rooms[j].name).arg(m_Rooms[i].deck), m_Rooms[i]);
}

QString Converter::RoomTypeName(int type) const
{
	return RoomTypeNameInSpacecraft((ROOMTYPE)type);
}

bool Converter::RenderFitting(TriList& out, const Detail& d) const
{
	return m_Builder.RenderFitting(out, d);
}

int Converter::RoomAt(int deck, const P2& p, double reach) const
{
	int best = -1;
	double bestDist = reach;
	for(int i = 0; i < m_Rooms.size(); i++)
	{
		if(m_Rooms[i].deck != deck || m_Rooms[i].poly.size() < 3)
			continue;
		const double d = DistanceTo(m_Rooms[i].poly, p);
		if(d == 0.0)
			return i;
		if(d <= bestDist)
		{
			bestDist = d;
			best = i;
		}
	}
	return best;
}

bool Converter::FloorPlace(const Detail& d, int& room, P2& at) const
{
	const Vec3 loc = d.Loc();
	at = P2(loc.x, loc.y);
	room = RoomAt(d.deck, at, 0.05);
	if(!Walkable(room))
		return false;
	const RoomData& rd = m_Rooms[room];
	if(Contains(rd.inner, at))
		return true;
	at = at + (rd.centre - at).Unit() * 1.0;
	return Contains(rd.inner, at);
}

QString Converter::Where(const Detail& d) const
{
	const QString where = QString("at grid %1,%2 on deck %3").arg(d.x).arg(d.y).arg(d.deck);
	m_WhereAsked.insert(where, m_WhereDetails.size());
	m_WhereDetails.append(d);
	return where;
}

static QString Brief(QString line, const QString& where)
{
	if(!where.isEmpty())
		line.remove(QString(" ") + where);
	const int colon = line.indexOf(':');
	if(colon > 0)
		line = line.left(colon);
	if(line.startsWith(QString("The ")))
		line = line.mid(4);
	if(!line.isEmpty())
		line[0] = line[0].toUpper();
	if(line.endsWith('.'))
		line.chop(1);
	if(line.size() > 72)
		line = line.left(69) + QString("...");
	return line;
}

void Converter::NoteProblem(const QString& line, Problem::KIND kind)
{
	int at = -1, found = -1;
	QString where;
	for(QHash<QString, int>::const_iterator wi = m_WhereAsked.constBegin(); wi != m_WhereAsked.constEnd(); wi++)
	{
		const int in = line.indexOf(wi.key());
		if(in >= 0 && (at < 0 || in < at))
		{
			at = in;
			found = wi.value();
			where = wi.key();
		}
	}
	if(found < 0)
		return;
	const Detail& d = m_WhereDetails[found];
	Problem p;
	p.kind = kind;
	p.line = line;
	p.text = Brief(line, where);
	p.deck = d.deck;
	p.detailId = d.id;
	p.detailType = d.type;
	const Vec3 loc = d.Loc();
	p.room = RoomName(RoomAt(d.deck, P2(loc.x, loc.y), 0.05));
	p.at = AuPoint(loc.x, loc.y, loc.z);
	m_Info.problems.append(p);
}

void Converter::SkipRoom(const QString& line, const RoomData& rd)
{
	m_Info.skipped.append(line);
	m_Log.append(QString("NOT DONE: ") + line);
	Problem p;
	p.kind = Problem::RoomShut;
	p.line = line;
	p.text = Brief(line, QString());
	p.deck = rd.deck;
	p.room = rd.name;
	p.at = AuPoint(rd.centre.x, rd.centre.y, rd.floorZ);
	if(rd.index >= 0 && rd.index < (int)m_Ship.rooms.size())
		p.outline = OutlineOnFloor(m_Ship.rooms[rd.index]);
	m_Info.problems.append(p);
}

void Converter::RenderModels()
{
	std::vector<Builder::Opening> openings;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const Detail& d = m_Ship.details[i];
		const int t = d.type;
		if(t == 3 || t == 5 || t == 49 || t == 50 || t == 53 || t == 54 || DetailType::IsAnyHatch(t))
		{
			Builder::Opening o;
			o.id = d.id;
			o.type = t;
			o.deck = d.deck;
			o.loc = d.Loc();
			bool repeated = false;
			for(size_t k = 0; k < openings.size(); k++)
				repeated = repeated || openings[k].id == o.id;
			if(repeated)
			{
				Skip(QString("Detail type %1 %2 has the id of another door: left as it is drawn, not a door.").arg(t).arg(Where(d)));
				continue;
			}
			openings.push_back(o);
			DoorData dd;
			dd.detail = (int)i;
			m_Doors.append(dd);
		}
	}
	m_Builder.SetOpenings(openings, (int)openings.size());
	m_DoorMade.fill(false, m_Doors.size());
	m_DoorPart.fill(0, m_Doors.size());

	TriList outside, inside;
	m_Builder.RenderExterior(outside, false);
	m_Builder.RenderInteriorDecks(inside);
	m_Builder.RenderExterior(inside, true);

	Vec3 lo(1e30, 1e30, 1e30), hi(-1e30, -1e30, -1e30);
	for(size_t i = 0; i < outside.size(); i++)
	{
		const Tri& t = outside[i];
		for(int k = 0; k < 3; k++)
		{
			const Vec3& p = t.v[k].pos;
			lo = Vec3(qMin(lo.x, p.x), qMin(lo.y, p.y), qMin(lo.z, p.z));
			hi = Vec3(qMax(hi.x, p.x), qMax(hi.y, p.y), qMax(hi.z, p.z));
		}
		const int door = t.v[0].ints[0] - 10;
		if(door >= 0 && door < m_Doors.size())
			m_Doors[door].outside.append(t);
		else
			m_Hull.append(t);
	}
	for(size_t i = 0; i < inside.size(); i++)
	{
		const Tri& t = inside[i];
		const int door = t.v[0].ints[0] - 10;
		if(door >= 0 && door < m_Doors.size())
			m_Doors[door].inside.append(t);
		else
			m_Inside.append(t);
	}
	if(!outside.empty() && !m_Options.keepOrigin)
	{
		m_Offset = Vec3((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, (lo.z + hi.z) / 2);
		for(size_t i = 0; i < m_Ship.details.size(); i++)
		{
			const Detail& d = m_Ship.details[i];
			const Vec3 at(d.x * GridUnit, d.y * GridUnit, d.deck * DeckHeight + DeckHeight / 2);
			if(d.type == 76 && at.x > lo.x && at.x < hi.x && at.y > lo.y && at.y < hi.y && at.z > lo.z && at.z < hi.z)
			{
				m_Offset = at;
				m_Info.pivot = true;
				break;
			}
		}
	}
	m_Info.offset = AuPoint(m_Offset.x, m_Offset.y, m_Offset.z);
}

AuScFace* Converter::Face(const Tri& t) const
{
	AuScPoint p[3];
	for(int k = 0; k < 3; k++)
	{
		const Vertex& v = t.v[k];
		p[k] = AuScPoint(At(v.pos), v.u, v.v, AuPoint(v.normal.x, v.normal.y, v.normal.z));
	}
	AuScFaceTri* face = new AuScFaceTri(p[0], p[1], p[2]);
	if(AtlasSlot)
		face->SetTextureIndex(AtlasSlot);
	Decorate(*face, t);
	return face;
}

AuScFace* Converter::Face(const Tri& t, ShipTex from, ShipTex to) const
{
	float fs, ft, fu, fv, ts, tt, tu, tv;
	ShipTexture(from, fs, ft, fu, fv);
	ShipTexture(to, ts, tt, tu, tv);
	Tri moved = t;
	for(int k = 0; k < 3; k++)
	{
		moved.v[k].u += ts - fs;
		moved.v[k].v += tt - ft;
	}
	return Face(moved);
}

void Converter::AddFaces(AuScFaceList& faces, const QVector<Tri>& tris) const
{
	for(int i = 0; i < tris.size(); i++)
	{
		const Tri& t = tris[i];
		const Vec3 a = t.v[1].pos - t.v[0].pos, b = t.v[2].pos - t.v[0].pos;
		const Vec3 n = Cross(a, b);
		if(n.x * n.x + n.y * n.y + n.z * n.z < 1e-16)
			continue;
		faces.append(Face(t));
	}
}

quint16 Converter::Add(AuScPart& part, const QString& what)
{
	AuPoint lo, hi;
	QString key = part.TypeName();
	if(part.ModelExtents(lo, hi))
		key += QString("/%1/%2/%3/%4/%5/%6").arg(lo.x, 0, 'f', 2).arg(lo.y, 0, 'f', 2).arg(lo.z, 0, 'f', 2).arg(hi.x, 0, 'f', 2)
				.arg(hi.y, 0, 'f', 2).arg(hi.z, 0, 'f', 2);
	const QList<quint16> alike = m_PartsByBox.value(key);
	for(int i = 0; i < alike.size(); i++)
	{
		AuScParts::const_iterator pi = m_Design.Parts().constFind(alike[i]);
		if(pi != m_Design.Parts().constEnd() && part == **pi)
		{
			m_Duplicates[part.PartName()]++;
			return alike[i];
		}
	}
	AuScParts added;
	const quint16 id = m_Design.PartAdd(part, added);
	if(!id)
	{
		Log(QString("FAILED to add %1: the part says it is not valid.").arg(what));
		m_Failed++;
	}
	else
		m_PartsByBox[key].append(id);
	return id;
}

void Converter::AddWallBox(AuScFaceList& faces, const P2& m, const P2& u, const P2& n, double from, double to, double halfWidth,
		double z0, double z1) const
{
	P2 c[4] = { m - u * halfWidth + n * from, m + u * halfWidth + n * from, m + u * halfWidth + n * to, m - u * halfWidth + n * to };
	Polygon plan;
	for(int i = 0; i < 4; i++)
		plan.append(c[i]);
	if(SignedArea(plan) < 0)
	{
		qSwap(c[0], c[3]);
		qSwap(c[1], c[2]);
	}
	QVector<AuPoint> bottom, top;
	for(int i = 0; i < 4; i++)
	{
		bottom.append(At(c[i], z0));
		top.append(At(c[i], z1));
	}
	AddPrism(faces, bottom, top);
}

HOLD_ACCESS ShipRoomHoldAccess(int oldType)
{
	static const HOLD_ACCESS access[25] =
	{
		HOLD_Inaccessible, HOLD_Armory , HOLD_Inaccessible, HOLD_Inaccessible, HOLD_Wardrobe ,
		HOLD_Hold , HOLD_Inaccessible, HOLD_Wardrobe , HOLD_FuelCoupling ,
		HOLD_Inaccessible, HOLD_Pantry , HOLD_Inaccessible, HOLD_Inaccessible, HOLD_VehicleBay ,
		HOLD_Bar , HOLD_Inaccessible, HOLD_Inaccessible, HOLD_Inaccessible, HOLD_Pharmacy ,
		HOLD_Wardrobe , HOLD_Inaccessible, HOLD_Armory , HOLD_Inaccessible, HOLD_Inaccessible,
		HOLD_Inaccessible,
	};
	return oldType >= 0 && oldType < 25 ? access[oldType] : HOLD_Inaccessible;
}

void Converter::VoidProperties(AuScRoomVoid& v, const RoomData& rd, bool doorway) const
{
	v.SetHoldAccess(doorway ? HOLD_Inaccessible : ShipRoomHoldAccess(rd.type));
	v.SetPersonnelAccess(PERSONNEL_Everyone);
	v.SetGravity(rd.type == 11 ? G_Zero : G_Artificial, AuPoint(0, 0, 1));
}

quint16 Converter::AddRoomVoid(AuScFaceList& faces, int room, quint16 door, const QString& what)
{
	AuScRoomVoid v;
	v.AddFaces(faces);
	v.SetFaceOpacity(VoidOpacity);
	const RoomData& rd = m_Rooms[room];
	v.SetGroupName(rd.name);
	VoidProperties(v, rd, door != 0);
	if(door)
		v.SetAssociatedPart(door, AuScDoor::Open);
	return Add(v, what);
}

quint16 Converter::AddHullVoid(AuScFaceList& faces, quint16 door, const QString& what)
{
	AuScHullVoid v;
	v.AddFaces(faces);
	v.SetFaceOpacity(VoidOpacity);
	if(door)
		v.SetAssociatedPart(door, AuScDoor::Open);
	return Add(v, what);
}

quint16 Converter::AddWalkPath(const AuPoint& from, const AuPoint& to, quint16 door)
{
	if(from.Distance(to) < 0.5)
		return 0;
	AuScPathWalk path;
	path.MakePath(from, to);
	if(door)
		path.SetAssociatedPart(door, AuScDoor::Open);
	return Add(path, "walk path");
}

double Converter::PathReach(int room, const P2& onWall, const P2& n) const
{
	static const double tries[4] = { 2.0, 1.5, 1.1, VoidInset + 0.2 };
	if(Walkable(room))
		for(int i = 0; i < 4; i++)
			if(Contains(m_Rooms[room].inner, onWall + n * tries[i]))
				return tries[i];
	return VoidInset + 0.1;
}

void Converter::WallStretch(int deck, const P2& p, const P2& u, const P2& n, double& back, double& ahead) const
{
	const int right = RoomAt(deck, p - n * 0.3), left = RoomAt(deck, p + n * 0.3);
	ahead = back = 0;
	for(int dir = 0; dir < 2; dir++)
	{
		double& reach = dir ? back : ahead;
		for(int k = 1; k < 400; k++)
		{
			const P2 q = p + u * ((dir ? -1.0 : 1.0) * (k - 0.5) * LatticeUnit);
			if(RoomAt(deck, q - n * 0.3) != right || RoomAt(deck, q + n * 0.3) != left)
				break;
			reach = k * LatticeUnit;
		}
	}
}

void Converter::SetProperties()
{
	AuScDesign& design = static_cast<AuScDesign&>(m_Design);
	QString name = QString::fromUtf8(m_Ship.name.c_str()).trimmed();
	if(name.isEmpty())
		name = QString("Old Ship");
	design.SetName(name);
	QString desc = QString::fromUtf8(m_Ship.description.c_str());
	if(!desc.isEmpty())
		desc += QString("\n");
	desc += QString("Converted from the old design \"%1\"").arg(name);
	if(!m_Ship.architect.empty())
		desc += QString(" by %1").arg(QString::fromUtf8(m_Ship.architect.c_str()));
	desc += QString(".");
	design.SetDescription(desc);

	AuDesignTex palette[SCTEXI_Count];
	design.TexturePalette(palette);
	palette[AtlasSlot].SetTexLibId(TEXLIB_OldSpacecraft);
	design.SetTexturePalette(palette);

	design.SetStructure(SC_Spaceship);
	design.SetStreamline(m_Ship.streamline ? SLINE_Atmospheric : SLINE_None);
	design.SetLifeSupportModule(CID_LifeSupportGravityModule);
	Log("Life support is the module with gravity, as in the stock designs: without it people float aboard once the ship "
			"is out of an atmosphere.");

	design.RefreshObstructionsCacheIfNeeded();
	SuggestVolumes(m_Ship, design, m_Info, m_Log);
	if(!m_Info.suggestion.made)
	{
		const char* must[] = { "volume maneuver drive", "volume power plant", "volume capacitor", "volume fuel", "volume life support",
				"volume sensors" };
		for(size_t i = 0; i < sizeof(must) / sizeof(must[0]); i++)
			SetVolume(design, m_Info, QString(must[i]), LeastVolume(QString(must[i]), m_Info));
		Log(QString("The game finds no hull volume: the systems get the least a design must have, a cubic metre each and life "
				"support for the berths."));
	}
}

bool Converter::CheckSize()
{
	const quint64 memory = m_Design.MemoryUsage();
	const quint64 memoryLimit = MemoryLimit();
	const int packed = qCompress(m_Design.Pack()).size();
	const int packedLimit = (int)m_Design.CompressedSizeLimit();
	Log(QString("Size: %L1 bytes in memory (limit %L2), %L3 bytes compressed (limit %L4).").arg(memory).arg(memoryLimit)
			.arg(packed).arg(packedLimit));
	if(memory > memoryLimit || packed > packedLimit)
	{
		Log(QString("TOO BIG: the converted design exceeds the blueprint size limit. Nothing is merged or left out to make "
				"it fit yet."));
		return false;
	}
	return true;
}

}
