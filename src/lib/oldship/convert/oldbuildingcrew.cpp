#include "oldbuildingconverter.h"

#include "dbcommodity.h"
#include <cmath>

namespace OldShip
{

namespace
{

struct StationKind
{
	int						detailType;
	AuScPart::PART_TYPE		part;
	const char*				name;
	COMMODITYID				module;
	const char*				moduleName;
	const char*				system;
};

const StationKind Stations[] =
{
	{ 1, AuScPart::Part_ConsoleNavigator, "telescope station", CID_Invalid, 0, 0 },
	{ 15, AuScPart::Part_ConsoleSensor, "radar station", CID_SensorModule, "sensor", "a radar" },
	{ 17, AuScPart::Part_ConsoleShield, "shield station", CID_EnergyShieldModule, "energy shield", "shields" },
	{ 7, AuScPart::Part_ConsoleFireControl, "fire control station", CID_BallisticWeaponBayModule, "ballistic weapon bay", "a weapon bay" },
	{ 19, AuScPart::Part_ConsoleTransporter, "transporter station", CID_PersonnelTransporterModule, "personnel transporter", "a transporter" },
	{ 4, AuScPart::Part_ConsoleMedic, "doctor's station", CID_SurgeryMedicalModule, "surgery", "a surgery" },
	{ 141, AuScPart::Part_TroopPost, "troop station", CID_Invalid, 0, "a guard post" },
};

}

void BuildingConverter::BuildStations()
{
	QMap<int, int> made;
	QStringList names, modules;
	QSet<int> padsUsed, surgeryUsed;
	for(size_t k = 0; k < sizeof(Stations) / sizeof(Stations[0]); k++)
	{
		const StationKind& kind = Stations[k];
		const qint8 rule = !m_HasIndustry ? 0 : kind.detailType == 15 ? m_Industry.Sensor() : kind.detailType == 17 ? m_Industry.Shield()
				: kind.detailType == 7 ? m_Industry.WeaponBay() : kind.detailType == 19 ? m_Industry.Transporter()
				: kind.detailType == 4 ? m_Industry.Surgery() : kind.detailType == 141 ? m_Industry.GuardPost() : 0;
		int refused = 0;
		for(size_t i = 0; i < m_Ship.details.size(); i++)
		{
			const Detail& d = m_Ship.details[i];
			if(d.type != kind.detailType)
				continue;
			if(!Allowed(rule))
			{
				refused++;
				continue;
			}
			const Vec3 loc = d.Loc();
			P2 at(loc.x, loc.y);
			const int room = RoomAt(d.deck, at, 0.05);
			if(!Walkable(room))
			{
				Skip(QString("The %1 %2 is not in a room people can enter: left out.").arg(kind.name).arg(Where(d)));
				continue;
			}
			const RoomData& rd = m_Rooms[room];
			if(!Contains(rd.inner, at))
			{
				at = at + (rd.centre - at).Unit() * 1.0;
				if(!Contains(rd.inner, at))
				{
					Skip(QString("The %1 %2 stands in a wall of %3: left out.").arg(kind.name).arg(Where(d)).arg(rd.name));
					continue;
				}
			}
			if(made[d.type] >= 10)
			{
				Skip(QString("The %1 %2 is the eleventh of its kind; the game allows ten: left out.").arg(kind.name).arg(Where(d)));
				continue;
			}
			if(d.type == 19 && !m_SensorStations)
			{
				Skip(QString("The transporter station %1 is left out: the game refuses a transporter in a building with no "
						"radar, and the old design has no radar station (or its industry may have none).").arg(Where(d)));
				continue;
			}
			const double z = rd.floorZ + VoidFloor;
			AuScPart* part = AuScPart::Create(kind.part);
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
				for(size_t p = 0; p < m_Ship.details.size(); p++)
				{
					const Detail& pad = m_Ship.details[p];
					const Vec3 pl = pad.Loc();
					if(pad.type == 20 && pad.deck == d.deck && !padsUsed.contains((int)p) && RoomAt(pad.deck, P2(pl.x, pl.y), 0.05) == room
							&& Contains(rd.inner, P2(pl.x, pl.y)))
					{
						pads.append(At(pl.x, pl.y, z));
						padsUsed.insert((int)p);
					}
				}
				if(pads.isEmpty())
					Skip(QString("The transporter station %1 has no transporter pad in its room: it has no pad locations.").arg(Where(d)));
				else
					part->SetPadLocs(pads);
			}
			if(d.type == 4)
			{
				bool found = false;
				for(size_t s = 0; s < m_Ship.details.size() && !found; s++)
				{
					const Detail& unit = m_Ship.details[s];
					const Vec3 sl = unit.Loc();
					if(unit.type == 11 && unit.deck == d.deck && !surgeryUsed.contains((int)s)
							&& RoomAt(unit.deck, P2(sl.x, sl.y), 0.05) == room && Contains(rd.inner, P2(sl.x, sl.y)))
					{
						AuScConsoleMedic* medic = dynamic_cast<AuScConsoleMedic*>(part);
						if(medic)
							medic->SetPatient(AuScPart::Patient_Prone, At(sl.x, sl.y, z), At(sl.x, sl.y + 1.0, z));
						surgeryUsed.insert((int)s);
						found = true;
					}
				}
				if(!found)
					Skip(QString("The doctor's station %1 has no surgery unit in its room: its patient has no place.").arg(Where(d)));
			}
			if(kind.module != CID_Invalid)
			{
				part->SetModule(kind.module);
				const QString line = QString("%1: %2").arg(kind.name).arg(kind.moduleName);
				if(!modules.contains(line))
					modules.append(line);
			}
			part->SetGroupName(rd.name);
			const quint16 id = Add(*part, QString("%1 %2").arg(kind.name).arg(Where(d)));
			delete part;
			if(!id)
				continue;
			made[d.type]++;
			if(d.type == 15)
				m_SensorStations++;
			if(d.type == 7)
				GiveBayVolume(id, Where(d));
			ConvertInfo::StationInfo si;
			si.detailType = d.type;
			si.detailId = d.id;
			si.part = id;
			si.room = rd.name;
			si.what = QString(kind.name);
			si.at = At(at, z);
			m_Info.stations.append(si);
		}
		if(refused)
			Skip(QString("%1 x %2 left out: a building of this industry (%3) may not have %4 in the current game, and a "
					"blueprint that has one cannot be used for it.").arg(refused).arg(kind.name).arg(m_BdInfo.industryName)
					.arg(kind.system));
		if(made.value(kind.detailType))
			names.append(QString("%1 x %2").arg(made.value(kind.detailType)).arg(kind.name));
	}
	Log(QString("Stations, as consoles (troop stations as troop posts): %1.").arg(names.isEmpty() ? QString("none") : names.join(", ")));
	if(!modules.isEmpty())
		Log(QString("Default modules given to stations that are systems: %1.").arg(modules.join(", ")));

	int pads = 0, units = 0;
	for(size_t k = 0; k < m_Ship.details.size(); k++)
	{
		if(m_Ship.details[k].type == 20 && !padsUsed.contains((int)k))
			pads++;
		if(m_Ship.details[k].type == 11 && !surgeryUsed.contains((int)k))
			units++;
	}
	if(pads)
		Skip(QString("%1 transporter pads have no transporter station of their own in their room: they are decoration only "
				"(in the current format pads are locations of a transporter station).").arg(pads));
	if(units)
		Skip(QString("%1 surgery units have no doctor's station of their own in their room: they are decoration only (in the "
				"current format the patient's place belongs to a medic station).").arg(units));
}

}
