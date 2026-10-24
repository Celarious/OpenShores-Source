#include "oldshipconverter.h"

#include "dbcommodity.h"
#include "oldshipedit.h"
#include <QSet>
#include <cmath>

namespace OldShip
{

namespace
{

const double Pi = 3.14159265358979323846;

struct StationKind
{
	int						detailType;
	AuScPart::PART_TYPE		part;
	const char*				name;
};

const StationKind Stations[] =
{
	{ 12, AuScPart::Part_ConsoleHelm, "helm" },
	{ 73, AuScPart::Part_ConsoleCommand, "captain's chair" },
	{ 1, AuScPart::Part_ConsoleNavigator, "navigator station" },
	{ 6, AuScPart::Part_ConsoleEngineer, "engineer station" },
	{ 7, AuScPart::Part_ConsoleFireControl, "fire control station" },
	{ 15, AuScPart::Part_ConsoleSensor, "sensor station" },
	{ 17, AuScPart::Part_ConsoleShield, "shield station" },
	{ 13, AuScPart::Part_ConsolePowerRelay, "power relay station" },
	{ 19, AuScPart::Part_ConsoleTransporter, "transporter station" },
	{ 4, AuScPart::Part_ConsoleMedic, "medic station" },
	{ 141, AuScPart::Part_TroopPost, "troop station" },
};

COMMODITYID ChosenModule(int cid, SYSTEM system, COMMODITYID plainest)
{
	return cid > 0 && cid < CID_Count && DbCommodity::IsScModule(system, (COMMODITYID)cid) ? (COMMODITYID)cid : plainest;
}

int ModuleFor(const std::vector<std::pair<int, int> >& modules, int number)
{
	for(size_t i = 0; i < modules.size(); i++)
		if(modules[i].first == number)
			return modules[i].second;
	return 0;
}

int BayNumberOf(const Design& ship, const Detail& d)
{
	for(size_t i = 0; i < ship.rooms.size(); i++)
	{
		const Room& r = ship.rooms[i];
		if(r.deck == d.deck && r.type == 21 && PointInOutline(r.points, d.x, d.y, true))
			return r.style;
	}
	return -1;
}

int StationBay(const Design& ship, const Detail& station)
{
	const int own = BayNumberOf(ship, station);
	if(own >= 0)
		return own;
	QSet<int> manned;
	for(size_t i = 0; i < ship.details.size(); i++)
		if(ship.details[i].type == 7)
			manned.insert(BayNumberOf(ship, ship.details[i]));
	int lowest = -1, loose = -1;
	for(size_t i = 0; i < ship.rooms.size(); i++)
	{
		if(ship.rooms[i].type != 21)
			continue;
		const int bay = ship.rooms[i].style;
		if(lowest < 0 || bay < lowest)
			lowest = bay;
		if(!manned.contains(bay) && (loose < 0 || bay < loose))
			loose = bay;
	}
	return loose >= 0 ? loose : lowest;
}

}

void Converter::BuildStations()
{
	QMap<int, int> made;
	QStringList names;
	int troopBerths = 0;
	for(int i = 0; i < m_Info.berths.size(); i++)
		if(m_Info.berths[i].what == QString("troop berth"))
			troopBerths++;
	QSet<int> padsUsed, surgeryUsed;
	bool sensorStation = false;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
		sensorStation = sensorStation || m_Ship.details[i].type == 15;
	QStringList modules;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const Detail& d = m_Ship.details[i];
		const StationKind* kind = 0;
		for(size_t k = 0; k < sizeof(Stations) / sizeof(Stations[0]); k++)
			if(Stations[k].detailType == d.type)
				kind = &Stations[k];
		if(!kind)
			continue;
		const Vec3 loc = d.Loc();
		P2 at(loc.x, loc.y);
		const int room = RoomAt(d.deck, at, 0.05);
		if(!Walkable(room))
		{
			Skip(QString("The %1 %2 is not in a room people can enter: left out.").arg(kind->name).arg(Where(d)));
			continue;
		}
		const RoomData& rd = m_Rooms[room];
		if(!Contains(rd.inner, at))
		{
			at = at + (rd.centre - at).Unit() * 1.0;
			if(!Contains(rd.inner, at))
			{
				Skip(QString("The %1 %2 stands in a wall of %3: left out.").arg(kind->name).arg(Where(d)).arg(rd.name));
				continue;
			}
		}
		if(made[d.type] >= 10)
		{
			Skip(QString("The %1 %2 is the eleventh of its kind; the game allows ten: left out.").arg(kind->name).arg(Where(d)));
			continue;
		}
		if(d.type == 7 && !sensorStation)
		{
			Skip(QString("The fire control station %1 is left out: in the current format it is a weapon bay, and the game "
					"refuses a weapon bay on a ship with no sensor station.").arg(Where(d)));
			continue;
		}
		if(d.type == 141 && made[d.type] >= troopBerths)
		{
			Skip(QString("The troop station %1 is left out: the game allows no more troop posts than troop berths (%2).")
					.arg(Where(d)).arg(troopBerths));
			continue;
		}
		const double z = rd.floorZ + VoidFloor;
		AuScPart* part = AuScPart::Create(kind->part);
		AuScIcon* icon = dynamic_cast<AuScIcon*>(part);
		if(!icon)
		{
			delete part;
			continue;
		}
		icon->MakeIcon(At(at, z), 0.f, AuMatrix4x4());

		if(d.type == 19)
		{
			AuPointArray pads;
			for(size_t k = 0; k < m_Ship.details.size(); k++)
			{
				const Detail& p = m_Ship.details[k];
				const Vec3 pl = p.Loc();
				if(p.type == 20 && p.deck == d.deck && !padsUsed.contains((int)k) && RoomAt(p.deck, P2(pl.x, pl.y), 0.05) == room
						&& Contains(rd.inner, P2(pl.x, pl.y)))
				{
					pads.append(At(pl.x, pl.y, z));
					padsUsed.insert((int)k);
				}
			}
			if(pads.isEmpty())
				Skip(QString("The transporter station %1 has no transporter pad in its room: it has no pad locations.").arg(Where(d)));
			else
				part->SetPadLocs(pads);
		}
		int surgeryModule = 0;
		if(d.type == 4)
		{
			bool found = false;
			for(size_t k = 0; k < m_Ship.details.size() && !found; k++)
			{
				const Detail& s = m_Ship.details[k];
				const Vec3 sl = s.Loc();
				if(s.type == 11 && s.deck == d.deck && !surgeryUsed.contains((int)k) && RoomAt(s.deck, P2(sl.x, sl.y), 0.05) == room
						&& Contains(rd.inner, P2(sl.x, sl.y)))
				{
					AuScConsoleMedic* medic = dynamic_cast<AuScConsoleMedic*>(part);
					if(medic)
						medic->SetPatient(AuScPart::Patient_Prone, At(sl.x, sl.y, z), At(sl.x, sl.y + 1.0, z));
					surgeryUsed.insert((int)k);
					surgeryModule = s.extra;
					found = true;
				}
			}
			if(!found)
				Skip(QString("The medic station %1 has no surgery unit in its room: its patient has no place.").arg(Where(d)));
		}
		const int roomNumber = rd.index >= 0 && rd.index < (int)m_Ship.rooms.size() ? m_Ship.rooms[rd.index].style : -1;
		const COMMODITYID plainest = d.type == 7 ? CID_BallisticWeaponBayModule : d.type == 15 ? CID_SensorModule
				: d.type == 17 ? CID_EnergyShieldModule : d.type == 19 ? CID_PersonnelTransporterModule
				: d.type == 4 ? CID_SurgeryMedicalModule : CID_Invalid;
		const COMMODITYID module = d.type == 7 ? ChosenModule(ModuleFor(m_Ship.bayModules, StationBay(m_Ship, d)), SYS_Weapon, plainest)
				: d.type == 15 ? ChosenModule(m_Ship.modules[6], SYS_Sensor, plainest)
				: d.type == 17 ? ChosenModule(m_Ship.modules[7], SYS_Shield, plainest)
				: d.type == 19 ? ChosenModule(ModuleFor(m_Ship.transporterModules, roomNumber), SYS_Transporter, plainest)
				: d.type == 4 ? ChosenModule(surgeryModule, SYS_SurgeryUnit, plainest) : CID_Invalid;
		if(module != CID_Invalid)
		{
			part->SetModule(module);
			if(module == plainest)
				modules.append(QString("%1: %2").arg(kind->name).arg(d.type == 7 ? "ballistic weapon bay" : d.type == 15 ? "sensor"
						: d.type == 17 ? "energy shield" : d.type == 19 ? "personnel transporter" : "surgery"));
			else
				modules.append(QString("%1: the old design's module %2").arg(kind->name).arg((int)module));
		}
		part->SetGroupName(rd.name);
		const quint16 id = Add(*part, QString("%1 %2").arg(kind->name).arg(Where(d)));
		delete part;
		if(!id)
			continue;
		made[d.type]++;
		if(d.type == 7)
			GiveBayVolume(id, Where(d));
		ConvertInfo::StationInfo si;
		si.detailType = d.type;
		si.detailId = d.id;
		si.part = id;
		si.room = rd.name;
		si.what = QString(kind->name);
		si.at = At(at, z);
		m_Info.stations.append(si);
	}
	for(size_t k = 0; k < sizeof(Stations) / sizeof(Stations[0]); k++)
		if(made.value(Stations[k].detailType))
			names.append(QString("%1 x %2").arg(made.value(Stations[k].detailType)).arg(Stations[k].name));
	Log(QString("Stations, as consoles facing the bow: %1.").arg(names.isEmpty() ? QString("none") : names.join(", ")));

	if(!modules.isEmpty())
		Log(QString("Modules given to stations that are systems: %1.").arg(modules.join(", ")));

	int pads = 0, units = 0;
	for(size_t k = 0; k < m_Ship.details.size(); k++)
	{
		if(m_Ship.details[k].type == 20 && !padsUsed.contains((int)k))
			pads++;
		if(m_Ship.details[k].type == 11 && !surgeryUsed.contains((int)k))
			units++;
	}
	if(pads)
		Skip(QString("%1 transporter pads have no transporter station in their room: they are decoration only (in the current "
				"format pads are locations of a transporter station).").arg(pads));
	if(units)
		Skip(QString("%1 surgery units have no medic station in their room: they are decoration only (in the current format "
				"the patient's place belongs to a medic station).").arg(units));
	if(!made.value(12))
		Log(QString("WARNING: the old design has no helm in a room people can enter. The game will not finalize a design "
				"without a helm station."));
}

void Converter::GiveBayVolume(quint16 part, const QString& where)
{
	static_cast<AuScDesign&>(m_Design).SetBayVolume(part, 1);
	Log(QString("The fire control station %1 is a weapon bay in the current format, with hull volume of its own.").arg(where));
}

void Converter::BuildBerths()
{
	bool captain = false;
	QMap<QString, int> made;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const Detail& d = m_Ship.details[i];
		const int bunks = DetailType::Bunks(d.type);
		TriList tris;
		if(bunks <= 0 || !RenderFitting(tris, d) || tris.empty())
			continue;
		Vec3 lo(1e30, 1e30, 1e30), hi(-1e30, -1e30, -1e30);
		for(size_t t = 0; t < tris.size(); t++)
			for(int k = 0; k < 3; k++)
			{
				const Vec3& p = tris[t].v[k].pos;
				lo = Vec3(qMin(lo.x, p.x), qMin(lo.y, p.y), qMin(lo.z, p.z));
				hi = Vec3(qMax(hi.x, p.x), qMax(hi.y, p.y), qMax(hi.z, p.z));
			}
		const P2 mid((lo.x + hi.x) / 2, (lo.y + hi.y) / 2);
		const int room = RoomAt(d.deck, mid, 0.05);
		if(!Walkable(room) || !Contains(m_Rooms[room].inner, mid))
		{
			Skip(QString("The berth %1 is not in a room people can enter: it is decoration only.").arg(Where(d)));
			continue;
		}
		const RoomData& rd = m_Rooms[room];
		const float turn = (float)((double)DetailType::Rotation(d.type) - Pi / 2);
		for(int b = 0; b < bunks; b++)
		{
			const double z = b == 0 ? lo.z + 1.0 : hi.z;
			AuScPart* part;
			QString what;
			if(rd.type == 4 && !captain)
			{
				part = new AuScBerthCapt;
				what = QString("captain's berth");
				captain = true;
			}
			else if(rd.type == 4)
			{
				part = new AuScBerthOfficer;
				what = QString("officer berth");
			}
			else if(rd.type == 1)
			{
				part = new AuScBerthTroop;
				what = QString("troop berth");
			}
			else if(rd.type == 19)
			{
				part = new AuScBerthPassenger;
				what = QString("passenger berth");
			}
			else
			{
				part = new AuScBerthCrew;
				what = QString("crew berth");
			}
			AuScIcon* icon = dynamic_cast<AuScIcon*>(part);
			icon->MakeIcon(At(mid, z), turn, AuMatrix4x4());
			part->SetGroupName(rd.name);
			const quint16 id = Add(*part, QString("%1 %2").arg(what).arg(Where(d)));
			delete part;
			if(!id)
				continue;
			made[what]++;
			ConvertInfo::StationInfo si;
			si.detailType = d.type;
			si.detailId = d.id;
			si.part = id;
			si.room = rd.name;
			si.what = what;
			si.at = At(mid, z);
			m_Info.berths.append(si);
		}
	}
	QStringList list;
	for(QMap<QString, int>::const_iterator mi = made.constBegin(); mi != made.constEnd(); mi++)
		list.append(QString("%1 x %2").arg(mi.value()).arg(mi.key()));
	Log(QString("Berths, one part per bunk: %1.").arg(list.isEmpty() ? QString("none") : list.join(", ")));
}

void Converter::ReportTurrets()
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
		Note(QString("%1 wall turrets and %2 top or bottom turrets are decoration: they stay part of the hull's look and no "
				"turret parts are made of them. The ship's guns are its weapon bays (the fire control stations).")
				.arg(wall).arg(flat));
}

}
