#include "oldbuildingconverter.h"
#include "oldbuildingspots.h"

#include "audef.h"
#include <cmath>

namespace OldShip
{

namespace
{

const double SpotGap = 6.0;
const double SpacecraftClear = 20.0;
const double BasinDepth = 32.8;
const double BasinMargin = 10.0;

AuScIcon* MakeIcon(AuScPart::PART_TYPE type, const AuPoint& at, float turn)
{
	AuScPart* part = AuScPart::Create(type);
	AuScIcon* icon = dynamic_cast<AuScIcon*>(part);
	if(!icon)
	{
		delete part;
		return 0;
	}
	icon->MakeIcon(at, turn, AuMatrix4x4());
	return icon;
}

void RecordSpot(ConvertInfo& info, AuScPart::PART_TYPE type, quint16 id, const QString& room, const QString& what, const AuPoint& at)
{
	if(!id || (type != AuScPart::Part_GroundVehicleParkingSpot && type != AuScPart::Part_SpaceVehicleParkingSpot))
		return;
	ConvertInfo::StationInfo si;
	si.detailType = 0;
	si.detailId = 0;
	si.part = id;
	si.room = room;
	si.what = what;
	si.at = at;
	info.parking.append(si);
}

}

void BuildingConverter::BuildingBounds(double& x0, double& x1, double& y0, double& y1) const
{
	x0 = 1e30; x1 = -1e30; y0 = 1e30; y1 = -1e30;
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
}

void BuildingConverter::BuildParkingSpots()
{
	m_SiteExtra.clear();
	m_Basins.clear();
	if(!m_HasIndustry)
		return;
	double bx0, bx1, by0, by1;
	BuildingBounds(bx0, bx1, by0, by1);
	if(bx0 > bx1)
		return;
	const double ground = m_Offset.z;
	const bool coastal = DbIndustry::IsCoastal((INDUSTRYID)m_Building.industry);
	const double front = coastal ? -1.0 : 1.0;
	double rowEdge = coastal ? by0 : by1;

	struct Kind
	{
		AuScPart::PART_TYPE		part;
		SpotKind				spot;
		qint8					need;
		const char*				name;
	};
	const Kind kinds[] =
	{
		{ AuScPart::Part_GroundVehicleParkingSpot, Spot_Ground, m_Industry.ParkGround(), "ground vehicle parking spot" },
		{ AuScPart::Part_SpaceVehicleParkingSpot, Spot_Space, m_Industry.ParkSpace(), "space vehicle parking spot" },
		{ AuScPart::Part_RocketParkingSpot, Spot_Rocket, m_Industry.ParkSpace(), "space rocket parking spot" },
		{ AuScPart::Part_WaterVehicleParkingSpot, Spot_Water, m_Industry.ParkWater(), "water vehicle parking spot" },
		{ AuScPart::Part_SpacecraftParkingSpot, Spot_Spacecraft, m_Industry.ParkSpacecraft(), "spacecraft parking spot" },
	};
	QStringList made;
	int spaceMade = 0;
	for(size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++)
	{
		const Kind& kind = kinds[k];
		int count = 0;

		for(size_t i = 0; i < m_Building.details.size(); i++)
		{
			const Detail& d = m_Building.details[i];
			if(SpotKindOf(d.type) != kind.spot)
				continue;
			if(!Allowed(kind.need))
			{
				Skip(QString("The %1 %2 is left out: a building of this industry (%3) may not have one.").arg(kind.name)
						.arg(Where(d)).arg(m_BdInfo.industryName));
				continue;
			}
			const Vec3 loc = d.Loc();
			const int room = RoomAt(d.deck, P2(loc.x, loc.y), 0.05);
			const double z = Walkable(room) ? m_Rooms[room].floorZ + VoidFloor : ground;
			AuScIcon* icon = MakeIcon(kind.part, At(loc.x, loc.y, z), (float)SpotTurn(d.type));
			if(!icon)
				continue;
			if(Walkable(room))
				icon->SetGroupName(m_Rooms[room].name);
			else if(kind.part == AuScPart::Part_WaterVehicleParkingSpot)
				m_Basins.append(QPair<P2, P2>(P2(loc.x - icon->IconLeft() - BasinMargin, loc.y - icon->IconBack() - BasinMargin),
						P2(loc.x + icon->IconRight() + BasinMargin, loc.y + icon->IconFore() + BasinMargin)));
			else
			{
				m_SiteExtra.append(P2(loc.x - icon->IconLeft(), loc.y - icon->IconBack()));
				m_SiteExtra.append(P2(loc.x + icon->IconRight(), loc.y + icon->IconFore()));
			}
			const quint16 id = Add(*icon, QString("%1 %2").arg(kind.name).arg(Where(d)));
			if(id)
				count++;
			RecordSpot(m_Info, kind.part, id, Walkable(room) ? m_Rooms[room].name : QString(), QString(kind.name), icon->Center());
			delete icon;
		}
		int need = kind.need - count;
		if(kind.part == AuScPart::Part_SpaceVehicleParkingSpot)
			spaceMade = count;
		else if(kind.part == AuScPart::Part_RocketParkingSpot)
			need = kind.need - spaceMade - count;
		if(kind.part == AuScPart::Part_RocketParkingSpot || need <= 0)
		{
			if(count)
				made.append(QString("%1 x %2 (placed in the design)").arg(count).arg(kind.name));
			continue;
		}

		int automatic = 0;
		if(kind.part == AuScPart::Part_GroundVehicleParkingSpot)
			for(int r = 0; r < m_Rooms.size() && need > 0; r++)
			{
				const RoomData& rd = m_Rooms[r];
				if(rd.type != 13 || !Walkable(r))
					continue;
				AuScIcon* icon = MakeIcon(kind.part, At(rd.centre, rd.floorZ + VoidFloor), 0.f);
				if(!icon)
					continue;
				const P2 c = rd.centre;
				const P2 corner[4] = { P2(c.x - icon->IconLeft(), c.y - icon->IconBack()), P2(c.x + icon->IconRight(), c.y - icon->IconBack()),
						P2(c.x + icon->IconRight(), c.y + icon->IconFore()), P2(c.x - icon->IconLeft(), c.y + icon->IconFore()) };
				bool fits = true;
				for(int q = 0; q < 4; q++)
					fits = fits && Contains(rd.inner, corner[q]);
				if(fits)
				{
					icon->SetGroupName(rd.name);
					const quint16 id = Add(*icon, QString("%1 in the garage %2").arg(kind.name).arg(rd.name));
					if(id)
					{
						need--;
						automatic++;
						RecordSpot(m_Info, kind.part, id, rd.name, QString(kind.name), icon->Center());
					}
				}
				delete icon;
			}

		if(kind.part == AuScPart::Part_SpacecraftParkingSpot)
		{
			const double cy = (by0 + by1) / 2;
			for(int i = 0; i < need; i++)
			{
				const double r = GroundSpawnSizeLimit();
				const double cx = i % 2 == 0 ? bx1 + SpacecraftClear + r + (i / 2) * (2 * r + SpotGap)
						: bx0 - SpacecraftClear - r - (i / 2) * (2 * r + SpotGap);
				AuScIcon* icon = MakeIcon(kind.part, At(cx, cy, ground), 0.f);
				if(!icon)
					continue;
				m_SiteExtra.append(P2(cx - icon->IconLeft(), cy - icon->IconBack()));
				m_SiteExtra.append(P2(cx + icon->IconRight(), cy + icon->IconFore()));
				if(Add(*icon, QString("%1 %2").arg(kind.name).arg(i + 1)))
					automatic++;
				delete icon;
			}
		}
		else if(kind.part == AuScPart::Part_WaterVehicleParkingSpot)
		{
			AuScIcon* probe = MakeIcon(kind.part, AuPoint(), 0.f);
			if(!probe)
				continue;
			const double width = probe->IconLeft() + probe->IconRight(), depth = probe->IconBack() + probe->IconFore();
			delete probe;
			const double cy = by1 + SpotGap + depth / 2 + BasinMargin;
			const double span = need * width + (need - 1) * SpotGap;
			for(int i = 0; i < need; i++)
			{
				const double cx = (bx0 + bx1) / 2 - span / 2 + width / 2 + i * (width + SpotGap);
				AuScIcon* icon = MakeIcon(kind.part, At(cx, cy, ground), 0.f);
				if(!icon)
					continue;
				if(Add(*icon, QString("%1 %2").arg(kind.name).arg(i + 1)))
					automatic++;
				delete icon;
			}
			m_Basins.append(QPair<P2, P2>(P2((bx0 + bx1) / 2 - span / 2 - BasinMargin, cy - depth / 2 - BasinMargin),
					P2((bx0 + bx1) / 2 + span / 2 + BasinMargin, cy + depth / 2 + BasinMargin)));
		}
		else if(need > 0)
		{
			AuScIcon* probe = MakeIcon(kind.part, AuPoint(), 0.f);
			if(!probe)
				continue;
			const double width = probe->IconLeft() + probe->IconRight(), depth = probe->IconBack() + probe->IconFore();
			delete probe;
			const double cy = rowEdge + front * (SpotGap + depth / 2);
			const double span = need * width + (need - 1) * SpotGap;
			for(int i = 0; i < need; i++)
			{
				const double cx = (bx0 + bx1) / 2 - span / 2 + width / 2 + i * (width + SpotGap);
				AuScIcon* icon = MakeIcon(kind.part, At(cx, cy, ground), front > 0 ? 0.f : (float)PI);
				if(!icon)
					continue;
				m_SiteExtra.append(P2(cx - width / 2, cy - depth / 2));
				m_SiteExtra.append(P2(cx + width / 2, cy + depth / 2));
				const quint16 id = Add(*icon, QString("%1 %2").arg(kind.name).arg(i + 1));
				if(id)
					automatic++;
				RecordSpot(m_Info, kind.part, id, QString(), QString(kind.name), icon->Center());
				delete icon;
			}
			rowEdge = cy + front * depth / 2;
		}
		if(count)
			made.append(QString("%1 x %2 (placed in the design)").arg(count).arg(kind.name));
		if(automatic)
			made.append(QString("%1 x %2 (added: the industry needs %3)").arg(automatic).arg(kind.name).arg((int)kind.need));
	}
	if(!made.isEmpty())
		Log(QString("Parking spots: %1. The old designer had none; the industry (%2) uses them for what it makes, lands or "
				"keeps.").arg(made.join(", ")).arg(m_BdInfo.industryName));
	if(!m_Basins.isEmpty())
		Note(QString("The water vehicle parking spots stand over a site sunk %1 ft into the ground on the building's +y side, "
				"as the stock Ship Yard has it: build the building with that side to the sea.").arg(BasinDepth));
}

void BuildingConverter::BuildBasins()
{
	for(int i = 0; i < m_Basins.size(); i++)
	{
		const P2 lo = m_Basins[i].first, hi = m_Basins[i].second;
		const double z = m_Offset.z - BasinDepth;
		AuPointArray boundary;
		boundary.append(At(lo.x, lo.y, z));
		boundary.append(At(lo.x, hi.y, z));
		boundary.append(At(hi.x, hi.y, z));
		boundary.append(At(hi.x, lo.y, z));
		AuScSite site;
		site.MakeSite(boundary, AuMatrix4x4());
		Add(site, QString("sunken site %1 of the water vehicle spots").arg(i + 1));
	}
}

void BuildingConverter::BuildIndustryNeeds()
{
	if(!m_HasIndustry)
		return;
	QStringList added;
	QMap<int, int> have;
	for(int i = 0; i < m_Info.stations.size(); i++)
		have[m_Info.stations[i].detailType]++;

	QSet<int> taken;
	auto roomFor = [&](int preferredType) -> int
	{
		int best = -1;
		double bestArea = 0;
		for(int r = 0; r < m_Rooms.size(); r++)
		{
			if(!Walkable(r) || taken.contains(r))
				continue;
			const RoomData& rd = m_Rooms[r];
			if(rd.type == preferredType)
				return r;
			if(rd.type == 6 || rd.type == 9 || rd.type == 11 || rd.type == 15)
				continue;
			const double area = qAbs(SignedArea(rd.inner));
			if(area > bestArea)
			{
				bestArea = area;
				best = r;
			}
		}
		return best;
	};

	struct Need
	{
		int					detailType;
		qint8				rule;
		AuScPart::PART_TYPE	part;
		COMMODITYID			module;
		int					roomType;
		const char*			name;
	};
	const Need needs[] =
	{
		{ 15, m_Industry.Sensor(), AuScPart::Part_ConsoleSensor, CID_SensorModule, 16, "radar station" },
		{ 17, m_Industry.Shield(), AuScPart::Part_ConsoleShield, CID_EnergyShieldModule, 17, "shield station" },
		{ 7, m_Industry.WeaponBay(), AuScPart::Part_ConsoleFireControl, CID_BallisticWeaponBayModule, 21, "fire control station" },
		{ 4, m_Industry.Surgery(), AuScPart::Part_ConsoleMedic, CID_SurgeryMedicalModule, 18, "doctor's station" },
		{ 141, m_Industry.GuardPost(), AuScPart::Part_TroopPost, CID_Invalid, 1, "troop station" },
	};
	for(size_t n = 0; n < sizeof(needs) / sizeof(needs[0]); n++)
	{
		const Need& need = needs[n];
		for(int i = have.value(need.detailType); i < need.rule; i++)
		{
			const int room = roomFor(need.roomType);
			if(room < 0)
			{
				Skip(QString("The industry (%1) needs a %2 and the building has no room for one.").arg(m_BdInfo.industryName)
						.arg(need.name));
				break;
			}
			taken.insert(room);
			const RoomData& rd = m_Rooms[room];
			const AuPoint at = At(rd.centre, rd.floorZ + VoidFloor);
			AuScIcon* icon = MakeIcon(need.part, at, 0.f);
			if(!icon)
				break;
			if(need.module != CID_Invalid)
				icon->SetModule(need.module);
			AuScConsoleMedic* medic = dynamic_cast<AuScConsoleMedic*>(icon);
			if(medic)
				medic->SetPatient(AuScPart::Patient_Prone, At(rd.centre.x + 3.0, rd.centre.y, rd.floorZ + VoidFloor),
						At(rd.centre.x + 3.0, rd.centre.y + 1.0, rd.floorZ + VoidFloor));
			icon->SetGroupName(rd.name);
			const quint16 id = Add(*icon, QString("%1 for the industry, in %2").arg(need.name).arg(rd.name));
			delete icon;
			if(!id)
				break;
			if(need.detailType == 15)
				m_SensorStations++;
			if(need.detailType == 7)
				GiveBayVolume(id, rd.name);
			ConvertInfo::StationInfo si;
			si.detailType = need.detailType;
			si.detailId = 0;
			si.part = id;
			si.room = rd.name;
			si.what = QString(need.name);
			si.at = at;
			m_Info.stations.append(si);
			added.append(QString("a %1 in %2").arg(need.name).arg(rd.name));
		}
	}

	if(m_Industry.Turret() > 0)
	{
		QVector<Vec3> places;
		for(size_t i = 0; i < m_Building.details.size(); i++)
			if(DetailType::IsTurret(m_Building.details[i].type) || DetailType::IsWallTurret(m_Building.details[i].type))
				places.append(m_Building.details[i].Loc());
		if(places.size() < m_Industry.Turret())
		{
			double bx0, bx1, by0, by1, top = -1e30;
			BuildingBounds(bx0, bx1, by0, by1);
			for(int i = 0; i < m_Hull.size(); i++)
				for(int k = 0; k < 3; k++)
					top = qMax(top, m_Hull[i].v[k].pos.z);
			for(int i = 0; i < m_Roofs.size(); i++)
				for(int k = 0; k < 3; k++)
					top = qMax(top, m_Roofs[i].v[k].pos.z);
			while(places.size() < m_Industry.Turret())
				places.append(Vec3((bx0 + bx1) / 2 + places.size() * 4.0, (by0 + by1) / 2, top + 1.0));
		}
		for(int i = 0; i < m_Industry.Turret(); i++)
		{
			const Vec3 c = places[i];
			const double s = 0.4;
			const Vec3 px(c.x + s, c.y, c.z), nx(c.x - s, c.y, c.z), py(c.x, c.y + s, c.z), ny(c.x, c.y - s, c.z),
					pz(c.x, c.y, c.z + s), nz(c.x, c.y, c.z - s);
			const Vec3 ring[4] = { px, py, nx, ny };
			QVector<Tri> tris;
			for(int q = 0; q < 4; q++)
				for(int up = 0; up < 2; up++)
				{
					Tri t;
					t.v[0].pos = up ? pz : nz;
					t.v[1].pos = up ? ring[q] : ring[(q + 1) % 4];
					t.v[2].pos = up ? ring[(q + 1) % 4] : ring[q];
					tris.append(t);
				}
			AuScFaceList faces;
			AddFaces(faces, tris);
			AuScTurret turret;
			turret.SetBasePivot(At(c));
			turret.SetBaseAxis(AuPoint(0, 0, 1));
			turret.AddFaces(faces);
			turret.SetModule(CID_BallisticWeaponTurretModule);
			if(Add(turret, QString("turret %1 for the industry").arg(i + 1)))
				added.append(QString("a turret (ballistic) at (%1, %2, %3)").arg(c.x, 0, 'f', 1).arg(c.y, 0, 'f', 1).arg(c.z, 0, 'f', 1));
		}
	}

	int posts = 0;
	for(int r = 0; r < m_Rooms.size() && posts < 12; r++)
	{
		const RoomData& rd = m_Rooms[r];
		if(!Walkable(r) || rd.type == 6 || rd.type == 9 || rd.type == 11 || rd.type == 13 || rd.type == 15)
			continue;
		const P2 at(rd.centre.x - 2.0, rd.centre.y - 2.0);
		if(!Contains(rd.inner, at))
			continue;
		AuScIcon* icon = MakeIcon(AuScPart::Part_CitizenPost, At(at, rd.floorZ + VoidFloor), 0.f);
		if(!icon)
			continue;
		icon->SetGroupName(rd.name);
		if(Add(*icon, QString("citizen post in %1").arg(rd.name)))
			posts++;
		delete icon;
	}
	if(posts)
		added.append(QString("%1 citizen posts (where workers stand)").arg(posts));
	if(!added.isEmpty())
		Log(QString("Added for the industry (%1), which the old design did not have: %2.").arg(m_BdInfo.industryName)
				.arg(added.join(", ")));
}

}
