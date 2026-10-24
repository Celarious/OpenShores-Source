#include "oldbuildingconverter.h"
#include "oldbuildingspots.h"

#include "aubdreport.h"
#include "audef.h"
#include "auglobal.h"
#include "autexturelib.h"
#include "dgdata.h"
#include <QProgressDialog>
#include <cmath>

namespace OldShip
{

namespace
{

const double SiteMargin = 10.0;
const double FlagTop = 31.5;
const double FlagWidth = 7.5, FlagHeight = 5.0;
const double FlagHoist = 0.2;
const double FlagGap = 0.01;
const double LampHeight = 25.5;

int LowestDeck(const OldBuilding::Design& b)
{
	int lo, hi;
	b.DeckRange(lo, hi);
	return lo;
}

int HighestDeck(const OldBuilding::Design& b)
{
	int lo, hi;
	b.DeckRange(lo, hi);
	return hi;
}

COMMODITYID HullModuleOf(int material)
{
	switch(material)
	{
	case BM_Wood: return CID_WoodHullModule;
	case BM_Lumber: return CID_LumberHullModule;
	case BM_Metal: return CID_MetalHullModule;
	case BM_Magmium: return CID_MagmiumHullModule;
	case BM_Vulcium: return CID_VulciumHullModule;
	}
	return CID_StoneHullModule;
}

}

int BuildingAtlas(int material)
{
	switch(material)
	{
	case BM_Wood: return TEXLIB_OldBuildingWood;
	case BM_Lumber: return TEXLIB_OldBuildingLumber;
	case BM_Metal: return TEXLIB_OldBuildingMetal;
	case BM_Magmium: return TEXLIB_OldBuildingMagmium;
	case BM_Vulcium: return TEXLIB_OldBuildingVulcium;
	}
	return TEXLIB_OldBuildingStone;
}

QString BuildingMaterialName(int material)
{
	switch(material)
	{
	case BM_Wood: return QString("wood (logs)");
	case BM_Lumber: return QString("lumber");
	case BM_Metal: return QString("metal");
	case BM_Magmium: return QString("magmium");
	case BM_Vulcium: return QString("vulcium");
	}
	return QString("stone");
}

bool ConvertBuilding(const OldBuilding::Design& building, AuBdDesign& out, QStringList& log, BuildingInfo* info,
		const BuildingOptions* options)
{
	BuildingInfo own;
	BuildingConverter c(building, out, log, info ? *info : own, options ? *options : BuildingOptions());
	return c.Run();
}

bool AnalyzeConvertedBuilding(AuBdDesign& design, AuBdReport& report, QStringList& msgs)
{
	QProgressDialog progress;
	progress.setMinimumDuration(1000000);
	const bool ok = design.FinalizeDesign(msgs, progress);
	progress.reset();
	design.ReportDesign(report);
	return ok;
}

static OldBuilding::Design WithoutSpots(const OldBuilding::Design& building)
{
	OldBuilding::Design drawn(building);
	drawn.details.clear();
	for(size_t i = 0; i < building.details.size(); i++)
		if(!IsSpot(building.details[i].type))
			drawn.details.push_back(building.details[i]);
	return drawn;
}

BuildingModel::BuildingModel(const OldBuilding::Design& building, int material)
	: m_Building(building), m_Lo(LowestDeck(building)), m_Hi(HighestDeck(building)), m_Drawn(WithoutSpots(building)),
	m_BdBuilder(m_Drawn, material, building.industry, m_Lo, m_Hi), m_AsShip(m_BdBuilder.AsShip())
{
	m_AsShip.name = building.name;
	m_AsShip.architect = building.architect;
	m_AsShip.minDeck = m_Lo;
	m_AsShip.maxDeck = m_Hi;
}

BuildingConverter::BuildingConverter(const OldBuilding::Design& building, AuBdDesign& design, QStringList& log,
		BuildingInfo& info, const BuildingOptions& options)
	: BuildingModel(building, options.material), Converter(m_AsShip, design, log, info, ConvertOptions()), m_BdInfo(info),
	m_BdOptions(options), m_HasIndustry(false), m_SensorStations(0)
{
	m_Options.keepOrigin = options.keepOrigin;
	m_BdInfo.industry = building.industry;
	m_BdInfo.material = options.material;
	const DgData& gd = g_Au.LockDataForRead();
	for(DbIndustryArray::const_iterator ii = gd.m_bIndustryArray.cbegin(); ii != gd.m_bIndustryArray.cend(); ii++)
		if((int)(*ii).IndustryId() == building.industry && building.industry != IID_None && (*ii).Structure() == BD_Building)
		{
			m_Industry = *ii;
			m_HasIndustry = true;
			m_BdInfo.industryName = (*ii).m_name;
		}
	g_Au.UnlockData();
}

Tri BuildingConverter::AsTri(const OldBuilding::BTri& b)
{
	Tri t;
	for(int k = 0; k < 3; k++)
	{
		Vertex& v = t.v[k];
		v.pos = b.v[k].pos;
		v.normal = b.v[k].normal;
		v.u = (float)b.v[k].u;
		v.v = (float)b.v[k].v;
		v.ints[0] = b.door;
		v.ints[1] = b.window;
	}
	return t;
}

void BuildingConverter::RenderModels()
{
	std::vector<OldBuilding::Opening> openings;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const Detail& d = m_Ship.details[i];
		const int t = d.type;
		if(t == 3 || t == 5 || t == 49 || t == 50 || t == 53 || t == 54 || DetailType::IsAnyHatch(t))
		{
			OldBuilding::Opening o;
			o.id = d.id;
			o.type = t;
			o.deck = d.deck;
			o.loc = d.Loc();
			bool repeated = false;
			for(size_t k = 0; k < openings.size(); k++)
				repeated = repeated || openings[k].id == o.id;
			if(repeated || o.id == 0)
			{
				Skip(QString("Detail type %1 %2 has no id of its own: left as it is drawn, not a door.").arg(t).arg(Where(d)));
				continue;
			}
			openings.push_back(o);
			DoorData dd;
			dd.detail = (int)i;
			m_Doors.append(dd);
		}
	}
	m_BdBuilder.SetOpenings(openings, true);
	m_DoorMade.fill(false, m_Doors.size());
	m_DoorPart.fill(0, m_Doors.size());

	OldBuilding::BTriList outside, inside, ground;
	OldBuilding::Builder::Sections sections;
	m_BdBuilder.RenderExterior(outside, false, &sections);
	m_BdBuilder.RenderInteriorDecks(inside, ground);
	m_BdBuilder.RenderExterior(inside, true);

	Vec3 lo(1e30, 1e30, 1e30), hi(-1e30, -1e30, -1e30);
	m_BdInfo.groundHoles = (int)ground.size();
	for(size_t i = 0; i < sections.ground + sections.shell && i < outside.size(); i++)
	{
		if(outside[i].window == 4)
		{
			m_BdInfo.groundHoles++;
			continue;
		}
		const Tri t = AsTri(outside[i]);
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
		if(inside[i].window == 4)
		{
			m_BdInfo.groundHoles++;
			continue;
		}
		const Tri t = AsTri(inside[i]);
		const int door = t.v[0].ints[0] - 10;
		if(door >= 0 && door < m_Doors.size())
			m_Doors[door].inside.append(t);
		else
			m_Inside.append(t);
	}

	for(size_t i = 0; i < m_Building.rooms.size(); i++)
	{
		if(!m_Building.rooms[i].IsRoof())
			continue;
		OldBuilding::BTriList roof;
		m_BdBuilder.RenderRoof(roof, m_Building.rooms[i]);
		if(roof.empty())
			continue;
		for(size_t k = 0; k < roof.size(); k++)
			m_Roofs.append(AsTri(roof[k]));
		m_RoofSizes.append((int)roof.size());
	}

	if(!m_Hull.isEmpty() && !m_Options.keepOrigin)
	{
		m_Offset = Vec3((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, -m_BdOptions.groundLift);
		m_BdInfo.lift = m_BdOptions.groundLift;
	}
	m_Info.offset = AuPoint(m_Offset.x, m_Offset.y, m_Offset.z);
}

bool BuildingConverter::RenderFitting(TriList& out, const Detail& d) const
{
	OldBuilding::BTriList tris;
	if(!m_BdBuilder.RenderFitting(tris, d))
		return false;
	for(size_t i = 0; i < tris.size(); i++)
		out.push_back(AsTri(tris[i]));
	return true;
}

void BuildingConverter::Decorate(AuScFace& face, const Tri& t) const
{
	if(t.v[0].ints[1] == 3)
		face.SetSelfIllumination(1.f);
}

void BuildingConverter::DoorPicture(AuScFaceList& faces, const QVector<Tri>& tris, int picture) const
{
	const double keep = picture < 0 ? 1.0 : picture == 0 ? 0.5 : 0.125;
	Vec3 lo(1e30, 1e30, 1e30), hi(-1e30, -1e30, -1e30);
	for(int i = 0; i < tris.size(); i++)
		for(int k = 0; k < 3; k++)
		{
			const Vec3& p = tris[i].v[k].pos;
			lo = Vec3(qMin(lo.x, p.x), qMin(lo.y, p.y), qMin(lo.z, p.z));
			hi = Vec3(qMax(hi.x, p.x), qMax(hi.y, p.y), qMax(hi.z, p.z));
		}
	const bool flat = hi.z - lo.z < 0.5;
	const bool alongX = hi.x - lo.x >= hi.y - lo.y;
	for(int i = 0; i < tris.size(); i++)
	{
		Tri t = tris[i];
		if(keep < 1.0)
			for(int k = 0; k < 3; k++)
			{
				Vec3& p = t.v[k].pos;
				if(!flat)
					p.z = hi.z - (hi.z - p.z) * keep;
				else if(alongX)
					p.x = lo.x + (p.x - lo.x) * keep;
				else
					p.y = lo.y + (p.y - lo.y) * keep;
			}
		const Vec3 n = Cross(t.v[1].pos - t.v[0].pos, t.v[2].pos - t.v[0].pos);
		if(n.x * n.x + n.y * n.y + n.z * n.z < 1e-16)
			continue;
		faces.append(Face(t));
	}
}

QString BuildingConverter::RoomTypeName(int type) const
{
	static const char* names[25] =
	{
		"", "Barracks", "Command Center", "Cell", "", "Storage", "Hall", "", "Shop", "Liquid Storage", "Kitchen", "Gravity Well",
		"", "Garage", "Living Room", "Open to Story Below", "Radar Room", "Shield Room", "Doctor's Office", "Bedroom",
		"Transporter Room", "Weapon Room", "Roof", "Roof", "Roof",
	};
	return type >= 0 && type < 25 ? QString(names[type]) : QString();
}

void BuildingConverter::VoidProperties(AuScRoomVoid& v, const RoomData& rd, bool doorway) const
{
	static const HOLD_ACCESS access[25] =
	{
		HOLD_Inaccessible, HOLD_Armory , HOLD_Inaccessible, HOLD_Inaccessible, HOLD_Inaccessible,
		HOLD_Hold , HOLD_Inaccessible, HOLD_Inaccessible, HOLD_Inaccessible ,
		HOLD_Inaccessible, HOLD_Pantry , HOLD_Inaccessible, HOLD_Inaccessible, HOLD_VehicleBay ,
		HOLD_Bar , HOLD_Inaccessible, HOLD_Inaccessible, HOLD_Inaccessible, HOLD_Pharmacy ,
		HOLD_Wardrobe , HOLD_Inaccessible, HOLD_Armory , HOLD_Inaccessible, HOLD_Inaccessible,
		HOLD_Inaccessible,
	};
	v.SetHoldAccess(!doorway && rd.type >= 0 && rd.type < 25 ? access[rd.type] : HOLD_Inaccessible);
	v.SetPersonnelAccess(PERSONNEL_Everyone);
	v.SetGravity(rd.type == 11 ? G_Zero : G_Natural, AuPoint(0, 0, 1));
}

double BuildingConverter::HullOpeningReach(int roomType) const
{
	double reach = 0;
	const int types[2] = { 0x1d, roomType };
	for(int i = 0; i < 2; i++)
	{
		const OldBuilding::TblProfile& profile = OldBuilding::WallProfile(true, types[i], m_BdOptions.material, false);
		for(int k = 0; k < profile.npoints; k++)
			reach = qMax(reach, profile.points[k * 3]);
	}
	return reach;
}

quint64 BuildingConverter::MemoryLimit() const
{
	return AuBdReport::SizeLimit();
}

void BuildingConverter::GiveBayVolume(quint16 part, const QString& where)
{
	Bd().SetBayVolume(part, 1);
	Log(QString("The fire control station %1 is a weapon bay in the current format: it gets a placeholder 1 cubic metre "
			"of the building's volume.").arg(where));
}

double BuildingConverter::WallSolidDepth() const
{
	return 2.0;
}

double BuildingConverter::RoofSolidRise() const
{
	return 2.0 + 0.2137;
}

double BuildingConverter::HullVoidDrop(int deck) const
{
	return deck == 0 ? 1.0 : 0.0;
}

double BuildingConverter::HullVoidFrom(double reach) const
{
	return qMax(reach / 2 - 0.2, 0.25);
}

double BuildingConverter::DoorwayReach(double reach) const
{
	return HullVoidFrom(reach) + 0.4;
}

void BuildingConverter::BuildSurroundings()
{
	BuildRoofs();
	BuildWindows();
	BuildPoles();
	BuildParkingSpots();
	BuildSite();
	BuildBasins();
	BuildLivestockPosts();
	BuildTurretPlugs();
	BuildThresholds();
	BuildOpenGround();
}

void BuildingConverter::BuildOpenGround()
{
	AuScFaceList faces;
	int count = 0;
	for(int i = 0; i < m_Hull.size(); i++)
	{
		const Tri& t = m_Hull[i];
		if(fabs(t.v[0].pos.z) > 1e-6 || fabs(t.v[1].pos.z) > 1e-6 || fabs(t.v[2].pos.z) > 1e-6 || t.v[0].normal.z <= 0)
			continue;
		Polygon plan;
		for(int k = 0; k < 3; k++)
			plan.append(P2(t.v[k].pos.x, t.v[k].pos.y));
		const double area = SignedArea(plan);
		if(fabs(area) < 1e-6)
			continue;
		if(area < 0)
			qSwap(plan[1], plan[2]);
		QVector<AuPoint> bottom, top;
		for(int k = 0; k < 3; k++)
		{
			bottom.append(At(plan[k], m_Offset.z));
			top.append(At(plan[k], 0.9));
		}
		AddPrism(faces, bottom, top);
		count++;
	}
	if(faces.isEmpty())
		return;
	if(AddHullVoid(faces, 0, QString("hull void over the storeys below the ground")))
		Log(QString("Open ground over the storeys below the ground: a hull void over the %1 triangles of their top that lie "
				"in the open, so that the ground there can be walked on.").arg(count));
}

void BuildingConverter::AddLevelSlab(AuScFaceList& faces, const P2& mid, const P2& u, const P2& n, double from, double to,
		double half, double zLow, double zHigh, bool up) const
{
	const P2 c[4] = { mid - u * half + n * from, mid + u * half + n * from, mid + u * half + n * to, mid - u * half + n * to };
	const AuPoint look(0, 0, up ? 1 : -1);
	const double heights[2] = { zLow, zHigh };
	for(int i = 0; i < 2; i++)
		AddQuad(faces, At(c[0], heights[i]), At(c[1], heights[i]), At(c[2], heights[i]), At(c[3], heights[i]), look);
}

void BuildingConverter::BuildTurretPlugs()
{
	int made = 0;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const Detail& d = m_Ship.details[i];
		if(!DetailType::IsTurret(d.type) || DetailType::IsWallTurret(d.type))
			continue;
		const Vec3 loc = d.Loc();
		const P2 at(loc.x, loc.y);
		const int room = RoomAt(d.deck, at, 0.05);
		if(!Walkable(room))
			continue;
		const double floorZ = d.deck * DeckHeight, half = GridUnit + 0.3 + OffGrid;
		AuScFaceList faces;
		if(RoomAt(d.deck - 1, at, 0.05) < 0)
			AddLevelSlab(faces, at, P2(1, 0), P2(0, 1), -half, half, half, floorZ - KeelDepth, floorZ, false);
		if(RoomAt(d.deck + 1, at, 0.05) < 0)
			AddLevelSlab(faces, at, P2(1, 0), P2(0, 1), -half, half, half, floorZ + VoidCeiling + SolidSkin,
					floorZ + DeckHeight + RoofSolidRise(), true);
		if(faces.isEmpty())
			continue;
		AuScBarrier barrier;
		barrier.AddFaces(faces);
		barrier.SetFaceOpacity(VoidOpacity);
		if(Add(barrier, QString("solid at the turret %1").arg(Where(d))))
			made++;
	}
	if(made)
		Log(QString("Solid at ceiling and floor turrets: %1 barriers (not drawn), closing the hole each leaves in the solid "
				"under the building's bottom or over its top.").arg(made));
}

void BuildingConverter::BuildThresholds()
{
	int made = 0;
	struct Way { int deck; P2 mid, u, n; double half; };
	QVector<Way> ways;
	for(int i = 0; i < m_HullDoorSpans.size(); i++)
	{
		const Way w = { m_HullDoorSpans[i].deck, m_HullDoorSpans[i].mid, m_HullDoorSpans[i].u, m_HullDoorSpans[i].n, m_HullDoorSpans[i].half };
		ways.append(w);
	}
	for(int i = 0; i < m_OpenSpans.size(); i++)
	{
		const Span& s = m_OpenSpans[i];
		const P2 left = s.u.Left();
		const bool roomLeft = RoomAt(s.deck, s.mid + left * 1.0) >= 0, roomRight = RoomAt(s.deck, s.mid - left * 1.0) >= 0;
		if(roomLeft == roomRight)
			continue;
		const Way w = { s.deck, s.mid, s.u, roomLeft ? left * -1.0 : left, s.half };
		ways.append(w);
	}
	for(int i = 0; i < ways.size(); i++)
	{
		const Way& w = ways[i];
		if(w.deck == 0)
			continue;
		const double floorZ = w.deck * DeckHeight;
		AuScFaceList faces;
		AddLevelSlab(faces, w.mid + w.u * OffGrid, w.u, w.n, -0.3, 2.5, w.half + 0.3, floorZ - KeelDepth, floorZ, false);
		AuScBarrier barrier;
		barrier.AddFaces(faces);
		barrier.SetFaceOpacity(VoidOpacity);
		if(Add(barrier, QString("solid under the exterior doorway on deck %1").arg(w.deck)))
			made++;
	}
	if(made)
		Log(QString("Solid under exterior doorways above or below the ground: %1 barriers (not drawn).").arg(made));
}

bool BuildingConverter::AddNew(AuScPart& part, const QString& what)
{
	const int before = m_Design.Parts().size();
	return Add(part, what) && m_Design.Parts().size() > before;
}

void BuildingConverter::BuildWindows()
{
	if(!m_BdOptions.litWindows)
		return;
	const double setBack = 0.15;
	const int textures[2] = { OldBuilding::BT_HullWallWindow, OldBuilding::BT_HullTopBottomWindow };
	double low[2], high[2];
	for(int k = 0; k < 2; k++)
	{
		float s, t, u, v;
		OldBuilding::BuildingTexture(textures[k], s, t, u, v);
		low[k] = qMin(t, v) - 1e-4;
		high[k] = qMax(t, v) + 1e-4;
	}
	QMap<int, AuScFaceList*> byRoom;
	for(int i = 0; i < m_Hull.size(); i++)
	{
		const Tri& t = m_Hull[i];
		bool panes = false;
		for(int k = 0; k < 2 && !panes; k++)
			panes = t.v[0].v >= low[k] && t.v[0].v <= high[k] && t.v[1].v >= low[k] && t.v[1].v <= high[k] && t.v[2].v >= low[k]
					&& t.v[2].v <= high[k];
		if(!panes || t.v[0].ints[1] == 3)
			continue;
		Vec3 n = Cross(t.v[1].pos - t.v[0].pos, t.v[2].pos - t.v[0].pos);
		const double length = sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
		if(length < 1e-8)
			continue;
		n = n * (1.0 / length);
		if(n.x * t.v[0].normal.x + n.y * t.v[0].normal.y + n.z * t.v[0].normal.z < 0)
			n = n * -1.0;
		const Vec3 mid((t.v[0].pos.x + t.v[1].pos.x + t.v[2].pos.x) / 3, (t.v[0].pos.y + t.v[1].pos.y + t.v[2].pos.y) / 3,
				(t.v[0].pos.z + t.v[1].pos.z + t.v[2].pos.z) / 3);
		const Vec3 within = mid - n * 1.5;
		const int deck = qBound(m_Lo, (int)floor(within.z / DeckHeight), m_Hi);
		const int room = RoomAt(deck, P2(within.x, within.y), 4.0);
		AuScPoint p[3];
		for(int k = 0; k < 3; k++)
			p[k] = AuScPoint(At(t.v[k].pos - n * setBack), 0.5f, 0.5f, AuPoint(n.x, n.y, n.z));
		AuScFaceTri glass(p[0], p[1], p[2]);
		glass.SetTextureIndex(SCTEXI_Material);
		glass.SetPaint(QColor(34, 44, 58, 255));
		glass.SetOpacity(0.5f);
		if(!byRoom.contains(room))
			byRoom.insert(room, new AuScFaceList);
		byRoom[room]->AddFace(glass);
	}
	for(QMap<int, AuScFaceList*>::iterator ri = byRoom.begin(); ri != byRoom.end(); ri++)
	{
		AuScWindow window;
		window.AddFaces(**ri);
		window.SetGroupName(ri.key() >= 0 ? QString("%1 windows").arg(m_Rooms[ri.key()].name) : QString("Windows"));
		if(AddNew(window, QString("glass of the windows of %1").arg(ri.key() >= 0 ? m_Rooms[ri.key()].name : QString("no room"))))
		{
			m_BdInfo.windows++;
			m_BdInfo.windowTriangles += (*ri)->size();
		}
		delete *ri;
	}
	if(m_BdInfo.windows)
		Log(QString("Windows: glass behind the panes, %1 window parts (one for each room's windows, %2 triangles), which the game "
				"lights at night while the building is in use and has power, a room at a time, and puts out by day.")
				.arg(m_BdInfo.windows).arg(m_BdInfo.windowTriangles));
}

void BuildingConverter::BuildRoofs()
{
	int from = 0;
	for(int r = 0; r < m_RoofSizes.size(); r++)
	{
		AuScFaceList faces;
		AddFaces(faces, m_Roofs.mid(from, m_RoofSizes[r]));
		from += m_RoofSizes[r];
		if(faces.isEmpty())
			continue;
		AuScHull hull;
		hull.AddFaces(faces);
		if(AddNew(hull, QString("roof %1").arg(r + 1)))
		{
			m_BdInfo.roofs++;
			m_BdInfo.roofTriangles += faces.size();
		}
	}
	if(m_BdInfo.roofs)
		Log(QString("Roofs: %1, each a hull part of its own (%2 triangles). A roof lies over its storey as the old model has "
				"it; the storey under it keeps its own flat top.").arg(m_BdInfo.roofs).arg(m_BdInfo.roofTriangles));
}

void BuildingConverter::BuildPoles()
{
	for(size_t i = 0; i < m_Building.details.size(); i++)
	{
		const Detail& d = m_Building.details[i];
		if(d.type != 100 && d.type != 142)
			continue;
		OldBuilding::BTriList pole;
		if(d.type == 100)
			m_BdBuilder.CreateFlagPole(pole, d.Loc());
		else
			m_BdBuilder.CreateStreetLight(pole, d.Loc());
		QVector<Tri> tris;
		for(size_t k = 0; k < pole.size(); k++)
			tris.append(AsTri(pole[k]));
		m_Poles += tris;
		AuScFaceList faces;
		AddFaces(faces, tris);
		if(faces.isEmpty())
			continue;
		AuScMesh mesh;
		mesh.AddFaces(faces);
		const QString what = d.type == 100 ? QString("flag pole") : QString("light pole");
		if(!AddNew(mesh, QString("%1 %2").arg(what).arg(Where(d))))
			continue;
		m_BdInfo.poleTriangles += faces.size();
		if(d.type == 100)
		{
			m_BdInfo.flagPoles++;
			const Vec3 foot = d.Loc();
			for(int side = 0; side < 2; side++)
			{
				AuMatrix4x4 plane;
				plane.Translate(At(foot.x, foot.y + (side ? FlagGap : -FlagGap), foot.z + FlagTop - FlagHeight));
				if(side)
					plane.RotateZ(PI);
				plane.RotateX(PI / 2);
				AuScEmpireFlag flag;
				flag.MakeScreen(side ? AuPoint(-FlagHoist - FlagWidth, 0, 0) : AuPoint(FlagHoist, 0, 0),
						side ? AuPoint(-FlagHoist, FlagHeight, 0) : AuPoint(FlagHoist + FlagWidth, FlagHeight, 0), AuMatrix4x4());
				flag.Transform(plane);
				if(AddNew(flag, QString("empire flag of the flag pole %1").arg(Where(d))))
					m_BdInfo.flags++;
			}
			continue;
		}
		m_BdInfo.lightPoles++;
		AuScLightBulb bulb;
		const Vec3 at = d.Loc();
		bulb.MakeIcon(At(at.x, at.y, at.z + LampHeight), 0.f, AuMatrix4x4());
		Add(bulb, QString("light bulb of the light pole %1").arg(Where(d)));
	}
	if(m_BdInfo.flagPoles)
		Log(QString("Flag poles: %1, each a mesh part as the old model draws it, with the empire's flag at its top (%2 empire "
				"flag parts, one for each side of the cloth, %3 by %4 ft).").arg(m_BdInfo.flagPoles).arg(m_BdInfo.flags)
				.arg(FlagWidth).arg(FlagHeight));
	if(m_BdInfo.lightPoles)
		Log(QString("Light poles: %1, each a mesh part with a light bulb part at its lamp, %2 ft above its foot.")
				.arg(m_BdInfo.lightPoles).arg(LampHeight));
}

void BuildingConverter::BuildSite()
{
	double x0 = 1e30, x1 = -1e30, y0 = 1e30, y1 = -1e30;
	QVector<Tri> all = m_Hull;
	all += m_Roofs;
	all += m_Poles;
	for(int i = 0; i < m_Doors.size(); i++)
		all += m_Doors[i].outside;
	for(int i = 0; i < all.size(); i++)
		for(int k = 0; k < 3; k++)
		{
			const Vec3& p = all[i].v[k].pos;
			x0 = qMin(x0, p.x); x1 = qMax(x1, p.x);
			y0 = qMin(y0, p.y); y1 = qMax(y1, p.y);
		}
	for(int i = 0; i < m_SiteExtra.size(); i++)
	{
		x0 = qMin(x0, m_SiteExtra[i].x); x1 = qMax(x1, m_SiteExtra[i].x);
		y0 = qMin(y0, m_SiteExtra[i].y); y1 = qMax(y1, m_SiteExtra[i].y);
	}
	x0 -= SiteMargin; y0 -= SiteMargin; x1 += SiteMargin; y1 += SiteMargin;

	quint8 pavement;
	const int industry = m_Building.industry;
	const bool field = m_HasIndustry && m_Industry.Field() > 0;
	if(field || industry == IID_Farm || industry == IID_Orchard || industry == IID_LoggingCamp || industry == IID_House
			|| industry == IID_Park || industry == IID_Zoo)
		pavement = WORLDTEX_DirtCrackedSmooth;
	else if(industry == IID_Mine || industry == IID_Well || industry == IID_WindPowerPlant || industry == IID_SolarPowerPlant)
		pavement = WORLDTEX_RockTan;
	else if(m_BdOptions.material == BM_Wood)
		pavement = WORLDTEX_DirtSmooth;
	else if(m_BdOptions.material == BM_Vulcium)
		pavement = WORLDTEX_Tarmac;
	else if(m_BdOptions.material == BM_Magmium || m_BdOptions.material == BM_Metal)
		pavement = WORLDTEX_ConcreteHeavy;
	else
		pavement = WORLDTEX_CobblestoneLight;

	if(field)
	{
		const double need = m_Industry.Field() * (double)AuBdDesign::AreaFirstField() * FeetPerMeter() * FeetPerMeter() * 1.02;
		const double have = (x1 - x0) * (y1 - y0);
		if(have < need)
		{
			const double grow = sqrt(need / have);
			const double cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, hx = (x1 - x0) / 2 * grow, hy = (y1 - y0) / 2 * grow;
			x0 = cx - hx; x1 = cx + hx; y0 = cy - hy; y1 = cy + hy;
			Log(QString("The site is made %1 by %2 ft: the industry (%3) wants %4 field(s), and the game counts a field for "
					"every %5 square metres of a site that is farm land.").arg(x1 - x0, 0, 'f', 0).arg(y1 - y0, 0, 'f', 0)
					.arg(m_BdInfo.industryName).arg((int)m_Industry.Field()).arg(AuBdDesign::AreaFirstField()));
		}
	}

	const double z = m_Offset.z;
	AuPointArray boundary;
	boundary.append(At(x0, y0, z));
	boundary.append(At(x0, y1, z));
	boundary.append(At(x1, y1, z));
	boundary.append(At(x1, y0, z));
	AuScSite site;
	site.MakeSite(boundary, AuMatrix4x4());
	site.SetPavement(pavement);
	if(!Add(site, QString("site")))
		return;
	m_BdInfo.siteLo = At(x0, y0, z);
	m_BdInfo.siteHi = At(x1, y1, z);
	Log(QString("Site: a rectangle %1 by %2 ft on the ground, %3 ft past the building on every side at least; the game "
			"levels the terrain to it. The building's ground floor is %4 ft above it.").arg(x1 - x0, 0, 'f', 1)
			.arg(y1 - y0, 0, 'f', 1).arg(SiteMargin).arg(-m_Offset.z, 0, 'f', 2));
	if(m_Lo < 0)
		Note(QString("The building has %1 storey(s) below the ground. They are converted as they are, rooms, voids and all; "
				"what the current game does about the terrain over them is not known to the converter (the old client cut "
				"holes in the ground where the old model asked: %2 triangles here, of which nothing is made).")
				.arg(-m_Lo).arg(m_BdInfo.groundHoles));
	else if(m_BdInfo.groundHoles)
		Note(QString("The old model asks for %1 triangles to be cut out of the ground; nothing is made of them.")
				.arg(m_BdInfo.groundHoles));
}

void BuildingConverter::BuildLivestockPosts()
{
	if(!m_HasIndustry || m_Industry.LivestockPost() <= 0)
		return;
	int made = 0;
	for(int i = 0; i < m_Industry.LivestockPost() && i < 10; i++)
	{
		AuScLivestockPost post;
		post.MakeIcon(AuPoint(m_BdInfo.siteLo.x + 4.0 + 6.0 * i, m_BdInfo.siteLo.y + 4.0, 0.0), 0.f, AuMatrix4x4());
		if(Add(post, QString("livestock post %1").arg(i + 1)))
			made++;
	}
	Log(QString("Livestock posts: %1, on the site by its south edge. The industry (%2) wants them of a blueprint; an old "
			"design had none.").arg(made).arg(m_BdInfo.industryName));
}

void BuildingConverter::SetProperties()
{
	AuBdDesign& design = Bd();
	QString name = QString::fromUtf8(m_Building.name.c_str()).trimmed();
	if(name.isEmpty())
		name = QString("Old Building");
	design.SetName(name);
	QString desc = QString("Converted from the old building design \"%1\"").arg(name);
	if(!m_Building.architect.empty())
		desc += QString(" by %1").arg(QString::fromUtf8(m_Building.architect.c_str()));
	desc += QString(".");
	design.SetDescription(desc);

	AuDesignTex palette[SCTEXI_Count];
	design.TexturePalette(palette);
	palette[AtlasSlot].SetTexLibId((quint16)BuildingAtlas(m_BdOptions.material));
	design.SetTexturePalette(palette);

	design.SetStructure(BD_Building);
	design.SetHullModule(HullModuleOf(m_BdOptions.material));
	Log(QString("Material: %1. An old design was drawn anew for whatever its building was built of; a blueprint has one "
			"look, so the material is chosen at conversion: it picks the walls' cross-sections and the texture atlas, and "
			"is the blueprint's hull material.").arg(BuildingMaterialName(m_BdOptions.material)));
	if(m_Building.module)
		Note(QString("The old design's building module (commodity %1) has no place in a current blueprint.").arg(m_Building.module));
	BuildIndustryNeeds();
	SetIndustry();
}

void BuildingConverter::SetIndustry()
{
	AuBdDesign& design = Bd();
	if(!m_HasIndustry)
	{
		design.SetIndustryId(IID_None);
		Note(QString("The old design's industry (number %1) is not one the current game builds from a blueprint, or the "
				"game's data is not at hand: the blueprint is given no industry and no volumes. Set them in the design's "
				"properties.").arg(m_Building.industry));
		return;
	}
	const DbIndustry& rule = m_Industry;
	design.SetIndustryId((INDUSTRYID)m_Building.industry);

	QStringList given;
	quint32 total = 0;
	struct Space { qint8 count; quint32 each; const char* name; void (AuBdDesign::*set)(quint32); };
	qint8 offices = rule.Office();
	qint8 shops = rule.Shop();
	if(shops == 0 && (m_Building.industry == IID_Mine || m_Building.industry == IID_Bank || m_Building.industry == IID_Wharf))
		shops = 1;
	if(offices == 0 && (shops > 0 || rule.Generator() > 0))
		offices = 1;
	const Space spaces[] =
	{
		{ rule.Arena(), AuBdDesign::VolumeFirstArena(), "arena", &AuBdDesign::SetArenaVolume },
		{ rule.Auditorium(), AuBdDesign::VolumeFirstAuditorium(), "auditorium", &AuBdDesign::SetAuditoriumVolume },
		{ rule.HomeLarge(), AuBdDesign::VolumeFirstHomeLarge(), "large home", &AuBdDesign::SetHomeLargeVolume },
		{ rule.HomeMedium(), AuBdDesign::VolumeFirstHomeMedium(), "medium home", &AuBdDesign::SetHomeMediumVolume },
		{ rule.HomeSmall(), AuBdDesign::VolumeFirstHomeSmall(), "small home", &AuBdDesign::SetHomeSmallVolume },
		{ rule.Lounge(), AuBdDesign::VolumeFirstLounge(), "lounge", &AuBdDesign::SetLoungeVolume },
		{ offices, AuBdDesign::VolumeFirstOffice(), "office", &AuBdDesign::SetOfficeVolume },
		{ shops, AuBdDesign::VolumeFirstShop(), "workshop", &AuBdDesign::SetShopVolume },
		{ rule.Store(), AuBdDesign::VolumeFirstStore(), "store", &AuBdDesign::SetStoreVolume },
		{ rule.Generator(), AuBdDesign::VolumeFirstGenerator(), "generator", &AuBdDesign::SetPowerPlantVolume },
		{ rule.CargoStorage(), 1, "cubic metre of storage", &AuBdDesign::SetHoldVolume },
		{ rule.Battery(), 1, "cubic metre of battery", &AuBdDesign::SetCapacitorVolume },
	};
	for(size_t i = 0; i < sizeof(spaces) / sizeof(spaces[0]); i++)
	{
		if(spaces[i].count <= 0)
			continue;
		const quint32 volume = (quint32)spaces[i].count * spaces[i].each;
		(design.*spaces[i].set)(volume);
		total += volume;
		given.append(QString("%1 x %2 (%3 cubic metres)").arg((int)spaces[i].count).arg(spaces[i].name).arg(volume));
	}
	if(rule.Battery() > 0)
		design.SetCapacitorModule(CID_CapacitorModule);
	if(rule.Generator() > 0)
	{
		COMMODITYID plant = CID_Invalid;
		switch(m_Building.industry)
		{
		case IID_CoalPowerPlant: plant = CID_CoalPowerPlantModule; break;
		case IID_GasolinePowerPlant: plant = CID_GasolinePowerPlantModule; break;
		case IID_HydroelectricPowerPlant: plant = CID_HydroelectricPowerPlantModule; break;
		case IID_HydrogenPowerPlant: plant = CID_HydrogenPowerPlantModule; break;
		case IID_NaturalGasPowerPlant: plant = CID_NaturalGasPowerPlantModule; break;
		case IID_NuclearPowerPlant: plant = CID_NuclearPowerPlantModule; break;
		case IID_SolarPowerPlant: plant = CID_SolarPowerPlantModule; break;
		case IID_WindPowerPlant: plant = CID_WindPowerPlantModule; break;
		}
		design.SetPowerPlantModule(plant);
	}
	int shieldStations = 0;
	for(int i = 0; i < m_Info.stations.size(); i++)
		if(m_Info.stations[i].detailType == 17)
			shieldStations++;
	if(m_SensorStations)
	{
		const quint32 volume = (quint32)qMax(1, (int)rule.Sensor());
		design.SetSensorVolume(volume);
		total += volume;
		given.append(QString("%1 cubic metres of radar").arg(volume));
	}
	if(shieldStations)
	{
		const quint32 volume = (quint32)qMax(1, (int)rule.Shield());
		design.SetShieldVolume(volume);
		total += volume;
		given.append(QString("%1 cubic metres of shields").arg(volume));
	}
	for(int i = 0; i < m_Info.stations.size(); i++)
		if(m_Info.stations[i].detailType == 7)
			total += 1;

	double cubicFeet = 0;
	for(int r = 0; r < m_Rooms.size(); r++)
		cubicFeet += qAbs(SignedArea(m_Rooms[r].poly)) * DeckHeight;
	const double holds = cubicFeet / CubicFeetPerCubicMeter();
	Log(QString("Industry: %1 (%2), as the old design says. Its blueprint must hold: %3. That is given: %4 cubic metres in "
			"all; the building's rooms hold about %5.").arg(m_BdInfo.industryName).arg(m_Building.industry)
			.arg(given.isEmpty() ? QString("nothing in particular") : given.join(", ")).arg(total).arg((int)holds));
	if(total > holds)
		Note(QString("The industry (%1) asks for %2 cubic metres and the building's rooms hold about %3: the building is too "
				"small for what it is meant to be. The game does not refuse a blueprint for that, but the design's "
				"properties will show more volume given out than there is.").arg(m_BdInfo.industryName).arg(total).arg((int)holds));

	CheckBuildingIndustry(design, m_BdInfo, m_Log);
}

void CheckBuildingIndustry(const AuBdDesign& design, BuildingInfo& info, QStringList& log)
{
	const QString head("NOT DONE: As it is the blueprint cannot be used for its industry");
	for(int i = log.size(); i > 0; i--)
		if(log[i - 1].startsWith(head))
			log.removeAt(i - 1);
	for(int i = info.skipped.size(); i > 0; i--)
		if(info.skipped[i - 1].startsWith(head.mid(10)))
			info.skipped.removeAt(i - 1);
	info.industryFault.clear();

	DbIndustry rule;
	bool known = false;
	const DgData& gd = g_Au.LockDataForRead();
	for(DbIndustryArray::const_iterator ii = gd.m_bIndustryArray.cbegin(); ii != gd.m_bIndustryArray.cend(); ii++)
		if((*ii).IndustryId() == design.IndustryId() && design.IndustryId() != IID_None && (*ii).Structure() == BD_Building)
		{
			rule = *ii;
			known = true;
		}
	g_Au.UnlockData();
	if(!known)
		return;
	AuBdReport report;
	design.ReportDesign(report);
	const int fault = report.IndustryCanUse(rule);
	if(!fault)
		return;
	info.industryFault = AuBdReport::IndustryCanUseMessage(fault);
	const QString line = QString("As it is the blueprint cannot be used for its industry (%1): %2. The game will not finalize it "
			"until that is put right in the design studio, or another industry is chosen in its properties.")
			.arg(info.industryName).arg(info.industryFault);
	info.skipped.append(line);
	log.append(QString("NOT DONE: ") + line);
}

void BuildingConverter::ReportTurrets()
{
	int wall = 0, flat = 0;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const int t = m_Ship.details[i].type;
		if(DetailType::IsWallTurret(t))
			wall++;
		else if(DetailType::IsTurret(t))
			flat++;
	}
	if(wall + flat)
		Note(QString("%1 wall turrets and %2 ceiling or floor turrets are decoration: they stay part of the building's look "
				"and no turret parts are made of them. A turret of the current game is a part with a gun on pivots, a "
				"gunner's place and a weapon module, none of which an old turret has.").arg(wall).arg(flat));
}

void BuildingConverter::ReportTheRest()
{
	int shipOnly = 0;
	for(size_t i = 0; i < m_Building.details.size(); i++)
	{
		const int t = m_Building.details[i].type;
		if(t == 6 || t == 10 || t == 12 || t == 13 || t == 14 || t == 16 || t == 18 || t == 23 || t == 24 || t == 32 || (t >= 41 && t <= 48)
				|| (t >= 57 && t <= 64) || t == 73 || t == 76 || t == 98)
			shipOnly++;
	}
	if(shipOnly)
		Note(QString("%1 details are equipment only spacecraft have: the old building model draws nothing for them, and "
				"nothing is made of them.").arg(shipOnly));
	for(int i = 0; i < m_Info.doors.size(); i++)
	{
		const ConvertInfo::DoorInfo& door = m_Info.doors[i];
		if(door.kind != ConvertInfo::DoorInfo::HullDoor && door.kind != ConvertInfo::DoorInfo::HullOpening)
			continue;
		const double height = door.sideB.z - 1.0;
		if(height > 1.0)
			Note(QString("The exterior door or opening of %1 is %2 ft above the ground: it opens on a drop.").arg(door.roomA)
					.arg(height, 0, 'f', 1));
		else if(height < -1.0)
			Note(QString("The exterior door or opening of %1 is %2 ft below the ground: it opens into the earth.").arg(door.roomA)
					.arg(-height, 0, 'f', 1));
	}
}

}
