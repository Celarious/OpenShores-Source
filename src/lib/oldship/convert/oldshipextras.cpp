#include "oldshipconverter.h"

#include "oldshipedit.h"
#include <cmath>

namespace OldShip
{

namespace
{

const double Pi = 3.14159265358979323846;
const double EmitterHeight = DeckHeight / 2;

bool IsFighterLoc(int t)
{
	return t >= 41 && t <= 48;
}

bool IsSpaceVehicle(int commodity)
{
	return commodity == VEHICLE_SpaceFighter || commodity == VEHICLE_SpaceFighter2 || commodity == VEHICLE_SpaceTransport;
}

AuScPart* FloorIcon(AuScPart::PART_TYPE type, const AuPoint& at, float turn)
{
	AuScPart* part = AuScPart::Create(type);
	AuScIcon* icon = dynamic_cast<AuScIcon*>(part);
	if(!icon)
	{
		delete part;
		return 0;
	}
	icon->MakeIcon(at, turn, AuMatrix4x4());
	return part;
}

}

void Converter::BuildGameTables()
{
	int made = 0;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const Detail& d = m_Ship.details[i];
		if(!DetailType::IsGameTable(d.type))
			continue;
		int room;
		P2 at;
		if(!FloorPlace(d, room, at))
		{
			Skip(QString("The game table %1 is not in a room people can enter: left out.").arg(Where(d)));
			continue;
		}
		const RoomData& rd = m_Rooms[room];
		const AuPoint loc = At(at, rd.floorZ + VoidFloor);
		AuScPart* part = FloorIcon(AuScPart::Part_GameTable, loc, d.type == 75 ? (float)(Pi / 4) : 0.f);
		if(!part)
			continue;
		part->SetGroupName(rd.name);
		const quint16 id = Add(*part, QString("game table %1").arg(Where(d)));
		delete part;
		if(!id)
			continue;
		made++;
		ConvertInfo::StationInfo si;
		si.detailType = d.type;
		si.detailId = d.id;
		si.part = id;
		si.room = rd.name;
		si.what = QString("game table");
		si.at = loc;
		m_Info.tables.append(si);
	}
	if(made)
		Log(QString("Game tables: %1, as game table parts where the old ones stood.").arg(made));
}

void Converter::BuildParking()
{
	int ground = 0, space = 0, tight = 0;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const Detail& d = m_Ship.details[i];
		if(!DetailType::IsVehicleLoc(d.type) && !IsFighterLoc(d.type))
			continue;
		const bool forSpace = IsFighterLoc(d.type) || IsSpaceVehicle(d.extra);
		const QString what = forSpace ? QString("space vehicle parking spot") : QString("ground vehicle parking spot");
		int room;
		P2 at;
		if(!FloorPlace(d, room, at))
		{
			Skip(QString("The vehicle location %1 is not in a room people can enter: left out.").arg(Where(d)));
			continue;
		}
		const RoomData& rd = m_Rooms[room];
		const AuPoint loc = At(at, rd.floorZ + VoidFloor);
		const double heading = (double)DetailType::Rotation(d.type);
		AuScPart* part = FloorIcon(forSpace ? AuScPart::Part_SpaceVehicleParkingSpot : AuScPart::Part_GroundVehicleParkingSpot, loc,
				(float)(heading - Pi / 2));
		AuScIcon* icon = dynamic_cast<AuScIcon*>(part);
		if(!icon)
			continue;
		const P2 fore(cos(heading), sin(heading)), left = fore.Left();
		const P2 corner[4] = { at + fore * icon->IconFore() + left * icon->IconLeft(), at + fore * icon->IconFore() - left * icon->IconRight(),
				at - fore * icon->IconBack() - left * icon->IconRight(), at - fore * icon->IconBack() + left * icon->IconLeft() };
		bool fits = true;
		for(int k = 0; k < 4; k++)
			fits = fits && Contains(rd.poly, corner[k]);
		if(!fits)
			tight++;
		part->SetGroupName(rd.name);
		const quint16 id = Add(*part, QString("%1 %2").arg(what).arg(Where(d)));
		delete part;
		if(!id)
			continue;
		(forSpace ? space : ground)++;
		ConvertInfo::StationInfo si;
		si.detailType = d.type;
		si.detailId = d.id;
		si.part = id;
		si.room = rd.name;
		si.what = what;
		si.at = loc;
		m_Info.parking.append(si);
	}
	if(ground + space)
		Log(QString("Vehicle locations, as parking spots: %1 for ground vehicles, %2 for space vehicles.").arg(ground).arg(space));
	if(tight)
		Note(QString("%1 parking spots are bigger than the room around them leaves place for: the current game's parking spots "
				"are 6 by 12 metres for ground vehicles and 12 by 17 for space vehicles.").arg(tight));
}

void Converter::BuildEmitters()
{
	int contrails = 0;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const Detail& d = m_Ship.details[i];
		if(d.type != 98)
			continue;
		const Vec3 old = d.Loc();
		const AuPoint loc = At(old.x, old.y, old.z + EmitterHeight);
		AuScPart* part = FloorIcon(AuScPart::Part_ContrailEmitter, loc, 0.f);
		if(!part)
			continue;
		const int room = RoomAt(d.deck, P2(old.x, old.y), 0.05);
		if(room >= 0)
			part->SetGroupName(m_Rooms[room].name);
		const quint16 id = Add(*part, QString("contrail emitter %1").arg(Where(d)));
		delete part;
		if(!id)
			continue;
		contrails++;
		ConvertInfo::StationInfo si;
		si.detailType = d.type;
		si.detailId = d.id;
		si.part = id;
		si.room = RoomName(room);
		si.what = QString("contrail emitter");
		si.at = loc;
		m_Info.contrails.append(si);
	}
	if(contrails)
		Log(QString("Contrail emitters: %1, as contrail emitter parts at the same places, at the middle of the deck's height.")
				.arg(contrails));

	QMap<int, QList<int> > byNumber;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const Detail& d = m_Ship.details[i];
		if(d.type != 99)
			continue;
		const Vec3 old = d.Loc();
		const int room = RoomAt(d.deck, P2(old.x, old.y), 0.05);
		byNumber[room >= 0 ? m_Ship.rooms[m_Rooms[room].index].style : 0].append((int)i);
	}
	if(byNumber.isEmpty())
		return;

	QVector<int> station, stationNumber;
	for(int s = 0; s < m_Info.stations.size(); s++)
	{
		if(m_Info.stations[s].detailType != 7)
			continue;
		int number = -1;
		for(size_t k = 0; k < m_Ship.details.size(); k++)
		{
			const Detail& d = m_Ship.details[k];
			if(d.type != 7 || d.id != m_Info.stations[s].detailId)
				continue;
			const Vec3 old = d.Loc();
			const int room = RoomAt(d.deck, P2(old.x, old.y), 0.05);
			if(room >= 0 && m_Rooms[room].type == 21)
				number = m_Ship.rooms[m_Rooms[room].index].style;
		}
		station.append(s);
		stationNumber.append(number);
	}
	QVector<bool> taken(station.size(), false);
	QMap<int, int> given;
	for(int pass = 0; pass < 2; pass++)
		for(QMap<int, QList<int> >::const_iterator bi = byNumber.constBegin(); bi != byNumber.constEnd(); bi++)
			for(int e = 0; e < bi.value().size(); e++)
			{
				if(given.contains(bi.value()[e]))
					continue;
				for(int s = 0; s < station.size(); s++)
					if(!taken[s] && (pass == 1 || stationNumber[s] == bi.key()))
					{
						taken[s] = true;
						given[bi.value()[e]] = s;
						break;
					}
			}

	int linked = 0;
	for(QMap<int, QList<int> >::const_iterator bi = byNumber.constBegin(); bi != byNumber.constEnd(); bi++)
		for(int e = 0; e < bi.value().size(); e++)
		{
			const Detail& d = m_Ship.details[bi.value()[e]];
			const Vec3 old = d.Loc();
			ConvertInfo::EmitterInfo ei;
			ei.detailId = d.id;
			ei.bayNumber = bi.key();
			ei.part = 0;
			ei.at = At(old.x, old.y, old.z + EmitterHeight);
			if(given.contains(bi.value()[e]))
			{
				const ConvertInfo::StationInfo& si = m_Info.stations[station[given[bi.value()[e]]]];
				AuScParts::iterator pi = m_Design.Parts().find(si.part);
				AuScConsoleFireControl* bay = pi != m_Design.Parts().end() ? dynamic_cast<AuScConsoleFireControl*>(*pi) : 0;
				if(bay)
				{
					bay->SetEmitLoc(ei.at);
					ei.part = si.part;
					linked++;
					Log(QString("The weapon emitter %1 (weapon bays number %2) is where the weapon bay of the fire control station in "
							"%3 (part %4) fires from.").arg(Where(d)).arg(bi.key() + 1).arg(si.room).arg(si.part));
				}
			}
			if(!ei.part)
				Skip(QString("The weapon emitter %1 (weapon bays number %2) has no fire control station left to belong to: in the "
						"current format a weapon bay is a fire control station. It is not used.").arg(Where(d)).arg(bi.key() + 1));
			m_Info.emitters.append(ei);
		}
	if(linked < station.size())
		Log(QString("%1 fire control stations have no weapon emitter: their weapon bays fire from the design's origin until a "
				"point is set (Weapon Emit Location in the design studio).").arg(station.size() - linked));
}

void Converter::ReportRailings()
{
	int count = 0;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const Detail& d = m_Ship.details[i];
		if(d.type != 56)
			continue;
		count++;
		const Vec3 loc = d.Loc();
		const P2 m(loc.x, loc.y);
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
			continue;
		const P2 n = u.Left();
		const int roomA = RoomAt(d.deck, m - n * 1.0), roomB = RoomAt(d.deck, m + n * 1.0);
		const double out = VoidInset + 0.4, z = d.deck * DeckHeight + PathHeight;
		if(!Walkable(roomA) || !Walkable(roomB) || !Contains(m_Rooms[roomA].inner, m - n * out)
				|| !Contains(m_Rooms[roomB].inner, m + n * out))
			continue;
		ConvertInfo::RailingInfo ri;
		ri.roomA = m_Rooms[roomA].name;
		ri.roomB = m_Rooms[roomB].name;
		ri.sideA = At(m - n * out, z);
		ri.sideB = At(m + n * out, z);
		m_Info.railings.append(ri);
	}
	if(count)
		Log(QString("Railings: %1. Each is a wall one sees over: only the rail is drawn, and nobody passes, as the rooms' voids "
				"do not join there.").arg(count));
}

void Converter::ReportTheRest()
{
	int poles = 0, ftl = 0, pivots = 0;
	for(size_t i = 0; i < m_Ship.details.size(); i++)
	{
		const int t = m_Ship.details[i].type;
		if(t == 100 || t == 142)
			poles++;
		else if(t == 24)
			ftl++;
		else if(t == 76)
			pivots++;
	}
	if(poles)
		Note(QString("%1 flag poles and light poles are of buildings: a spacecraft has none, and nothing is made of them.").arg(poles));
	if(pivots && m_Info.pivot)
		Log(QString("The pivot point is the new design's origin, the point the ship turns about."));
	else if(pivots && m_Options.keepOrigin)
		Log(QString("The pivot point will be the origin of the finished blueprint, the point the ship turns about."));
	else if(pivots)
		Note(QString("The pivot point lies outside the hull's box and cannot be the design's origin: the middle of the hull is."));
	Q_UNUSED(ftl);
}

}
