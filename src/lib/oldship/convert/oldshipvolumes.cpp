#include "oldshipconverter.h"

#include "audef.h"
#include "dbcommodity.h"
#include "oldshipedit.h"
#include <cmath>

namespace OldShip
{

namespace
{

const double UnitCubicFeet = 256.0;

double UnitCubicMetres()
{
	return UnitCubicFeet / CubicFeetPerCubicMeter();
}

int CountOf(const Design& ship, int detailType)
{
	int n = 0;
	for(size_t i = 0; i < ship.details.size(); i++)
		if(ship.details[i].type == detailType)
			n++;
	return n;
}

const double FtlShare = 0.25;
const double EngineShares[3] = { 0.40, 0.30, 0.30 };

double RoomVolume(const Room& r)
{
	double twice = 0;
	for(size_t k = 0; k < r.points.size(); k++)
	{
		const GridPoint& a = r.points[k];
		const GridPoint& b = r.points[(k + 1) % r.points.size()];
		twice += (double)a.x * b.y - (double)b.x * a.y;
	}
	return fabs(twice) / 2 * GridUnit * GridUnit * DeckHeight / CubicFeetPerCubicMeter();
}

double RoomsVolume(const Design& ship, int roomType)
{
	double cubicMetres = 0;
	for(size_t i = 0; i < ship.rooms.size(); i++)
		if(ship.rooms[i].type == roomType)
			cubicMetres += RoomVolume(ship.rooms[i]);
	return cubicMetres;
}

int BayNumberAt(const Design& ship, const Detail& d)
{
	for(size_t i = 0; i < ship.rooms.size(); i++)
	{
		const Room& r = ship.rooms[i];
		if(r.deck == d.deck && r.type == 21 && PointInOutline(r.points, d.x, d.y, true))
			return r.style;
	}
	return -1;
}

const Detail* DetailOf(const Design& ship, int id, int type)
{
	for(size_t i = 0; i < ship.details.size(); i++)
		if(ship.details[i].id == id && ship.details[i].type == type)
			return &ship.details[i];
	return 0;
}

quint16 StationPart(const ConvertInfo& info, int detailType, int detailId)
{
	for(int i = 0; i < info.stations.size(); i++)
		if(info.stations[i].detailType == detailType && info.stations[i].detailId == detailId)
			return info.stations[i].part;
	return 0;
}

struct Share
{
	QString	setting;
	double	want;
	quint32	least;
	QString	how;
};

}

QVector<AuPoint> OutlineOnFloor(const Room& room)
{
	QVector<AuPoint> outline;
	for(size_t k = 0; k < room.points.size(); k++)
		outline.append(AuPoint(room.points[k].x * GridUnit, room.points[k].y * GridUnit, room.deck * DeckHeight));
	return outline;
}

quint32 LeastVolume(const QString& setting, const ConvertInfo& info)
{
	if(setting == QString("volume maneuver drive") || setting == QString("volume power plant") || setting == QString("volume capacitor")
			|| setting == QString("volume fuel"))
		return 1;
	if(setting == QString("volume life support"))
		return (quint32)qMax(1, info.berths.size() * 3 / 2 + 1);
	if(setting.startsWith(QString("volume bay ")))
		return 1;
	if(setting == QString("volume sensors"))
	{
		for(int i = 0; i < info.stations.size(); i++)
			if(info.stations[i].detailType == 7)
				return 1;
	}
	return 0;
}

bool SetVolume(AuScDesign& design, const ConvertInfo& info, const QString& setting, quint32 volume)
{
	if(setting == QString("volume capacitor"))
		design.SetCapacitorVolume(volume);
	else if(setting == QString("volume fuel"))
		design.SetFuelVolume(volume);
	else if(setting == QString("volume hold"))
		design.SetHoldVolume(volume);
	else if(setting == QString("volume life support"))
		design.SetLifeSupportVolume(volume);
	else if(setting == QString("volume maneuver drive"))
		design.SetManeuverDriveVolume(volume);
	else if(setting == QString("volume power plant"))
		design.SetPowerPlantVolume(volume);
	else if(setting == QString("volume sensors"))
		design.SetSensorVolume(volume);
	else if(setting == QString("volume shields"))
		design.SetShieldVolume(volume);
	else if(setting == QString("volume armor"))
		design.SetArmorVolume(volume);
	else if(setting == QString("volume cryo berths"))
		design.SetCryoBerthVolume(volume);
	else if(setting == QString("volume framework"))
		design.SetSkeletonVolume(volume);
	else if(setting.startsWith(QString("volume bay ")))
	{
		const quint16 part = StationPart(info, 7, setting.mid(11).toInt());
		if(!part)
			return false;
		design.SetBayVolume(part, volume);
	}
	else if(setting.startsWith(QString("volume ftl ")))
	{
		const quint16 part = StationPart(info, 6, setting.mid(11).toInt());
		if(!part)
			return false;
		design.SetFTLDriveVolume(part, volume);
	}
	else
		return false;
	return true;
}

QString VolumeTitle(const QString& setting, const ConvertInfo& info)
{
	if(setting.startsWith(QString("volume bay ")) || setting.startsWith(QString("volume ftl ")))
	{
		const bool bay = setting.startsWith(QString("volume bay "));
		const int id = setting.mid(11).toInt();
		QString room;
		for(int i = 0; i < info.stations.size(); i++)
			if(info.stations[i].detailId == id && info.stations[i].detailType == (bay ? 7 : 6))
				room = info.stations[i].room;
		return bay ? QString("Weapon Bay of the Fire Control Station in %1").arg(room)
				: QString("FTL Drive of the Engineer Station in %1").arg(room);
	}
	if(setting == QString("volume capacitor")) return QString("Capacitor");
	if(setting == QString("volume fuel")) return QString("Fuel");
	if(setting == QString("volume hold")) return QString("Hold");
	if(setting == QString("volume life support")) return QString("Life Support");
	if(setting == QString("volume maneuver drive")) return QString("Maneuver Drive");
	if(setting == QString("volume power plant")) return QString("Power Plant");
	if(setting == QString("volume sensors")) return QString("Sensors");
	if(setting == QString("volume shields")) return QString("Shields");
	if(setting == QString("volume armor")) return QString("Armor");
	if(setting == QString("volume cryo berths")) return QString("Cryo Berths");
	if(setting == QString("volume framework")) return QString("Framework");
	return setting;
}

void SuggestVolumes(const Design& ship, AuScDesign& design, ConvertInfo& info, QStringList& log)
{
	VolumeSuggestion& s = info.suggestion;
	s = VolumeSuggestion();
	const quint32 hull = design.HullVolume();
	if(!hull)
		return;
	s.made = true;
	s.hullVolume = hull;
	s.kept = design.ArmorVolume() + design.CryoBerthVolume();
	const double unit = UnitCubicMetres();

	bool sensorStation = false, shieldStation = false;
	int engineer = 0;
	QList<int> fireControl;
	for(int i = 0; i < info.stations.size(); i++)
	{
		const ConvertInfo::StationInfo& st = info.stations[i];
		sensorStation = sensorStation || st.detailType == 15;
		shieldStation = shieldStation || st.detailType == 17;
		if(st.detailType == 6 && !engineer)
			engineer = st.detailId;
		if(st.detailType == 7)
			fireControl.append(st.detailId);
	}

	QList<Share> shares;
	Q_UNUSED(unit);
	const COMMODITYID ftlModule = (COMMODITYID)ship.modules[1];
	const bool starship = ftlModule != CID_Invalid && ScModuleSystem(ftlModule) == SYS_FTLDrive;
	const double engine = RoomsVolume(ship, 8), sensorRooms = RoomsVolume(ship, 16), shieldRooms = RoomsVolume(ship, 17);
	const double engineLeft = engine * (starship && engineer ? 1.0 - FtlShare : 1.0);
	const QString ofEngine = QString("%1% of the engine rooms (%2 cubic metres)");
	{
		const Share share = { QString("volume maneuver drive"), engineLeft * EngineShares[0], 0,
				ofEngine.arg(engineLeft / qMax(engine, 1e-9) * EngineShares[0] * 100, 0, 'f', 0).arg(engine, 0, 'f', 0) };
		shares.append(share);
	}
	{
		const Share share = { QString("volume power plant"), engineLeft * EngineShares[1], 0,
				ofEngine.arg(engineLeft / qMax(engine, 1e-9) * EngineShares[1] * 100, 0, 'f', 0).arg(engine, 0, 'f', 0) };
		shares.append(share);
	}
	{
		const Share share = { QString("volume capacitor"), engineLeft * EngineShares[2], 0,
				ofEngine.arg(engineLeft / qMax(engine, 1e-9) * EngineShares[2] * 100, 0, 'f', 0).arg(engine, 0, 'f', 0) };
		shares.append(share);
	}
	{
		const Share share = { QString("volume fuel"), RoomsVolume(ship, 9), 0, QString("the fuel cells' floor times a deck's height") };
		shares.append(share);
	}
	{
		const Share share = { QString("volume life support"), 0.0, 0,
				QString("what the %1 berths need").arg(info.berths.size()) };
		shares.append(share);
	}
	{
		const Share share = { QString("volume sensors"), sensorStation ? sensorRooms : 0.0, 0, sensorStation
				? QString("the sensor rooms' floor times a deck's height")
				: sensorRooms > 0 ? QString("none: sensor rooms, but no sensor station to work them") : QString("none") };
		shares.append(share);
	}
	{
		const Share share = { QString("volume shields"), shieldStation ? shieldRooms : 0.0, 0, shieldStation
				? QString("the shield rooms' floor times a deck's height")
				: shieldRooms > 0 ? QString("none: shield rooms, but no shield station to work them") : QString("none") };
		shares.append(share);
	}
	{
		const Share share = { QString("volume hold"), RoomsVolume(ship, 5), 0, QString("the holds' floor times a deck's height") };
		shares.append(share);
	}

	double bayRooms = 0;
	for(size_t i = 0; i < ship.rooms.size(); i++)
		if(ship.rooms[i].type == 21)
			bayRooms += RoomVolume(ship.rooms[i]);
	if(!fireControl.isEmpty())
	{
		QMap<int, double> unitsOf;
		for(size_t i = 0; i < ship.rooms.size(); i++)
			if(ship.rooms[i].type == 21)
				unitsOf[ship.rooms[i].style] += RoomVolume(ship.rooms[i]);
		QMap<int, QList<int> > stationsOf;
		for(int i = 0; i < fireControl.size(); i++)
		{
			const Detail* d = DetailOf(ship, fireControl[i], 7);
			stationsOf[d ? BayNumberAt(ship, *d) : -1].append(fireControl[i]);
		}
		QMap<int, double> units;
		double loose = 0;
		for(QMap<int, double>::const_iterator ui = unitsOf.constBegin(); ui != unitsOf.constEnd(); ui++)
		{
			const QList<int> theirs = ui.key() < 0 ? QList<int>() : stationsOf.value(ui.key());
			if(theirs.isEmpty())
				loose += ui.value();
			for(int k = 0; k < theirs.size(); k++)
				units[theirs[k]] += (double)ui.value() / theirs.size();
		}
		if(loose > 0)
		{
			QList<int> takers;
			for(int i = 0; i < fireControl.size(); i++)
				if(!units.contains(fireControl[i]))
					takers.append(fireControl[i]);
			if(takers.isEmpty())
				takers = fireControl;
			for(int k = 0; k < takers.size(); k++)
				units[takers[k]] += loose / takers.size();
		}
		for(int i = 0; i < fireControl.size(); i++)
		{
			const double has = units.value(fireControl[i]);
			const Share share = { QString("volume bay %1").arg(fireControl[i]), has, 0,
					QString("%1 cubic metres of weapon bay rooms").arg(has, 0, 'f', 0) };
			shares.append(share);
		}
	}
	else if(bayRooms > 0)
		s.lines.append(QString("Weapon systems: none. The old design has weapon bay rooms but no fire control station that "
				"became a weapon bay."));

	for(int i = 0; i < shares.size(); i++)
	{
		shares[i].least = LeastVolume(shares[i].setting, info);
		shares[i].want = qMax(shares[i].want, (double)shares[i].least);
	}
	const double room = hull > s.kept ? (double)(hull - s.kept) : 0.0;

	if(starship && engineer && StationPart(info, 6, engineer))
	{
		const quint16 part = StationPart(info, 6, engineer);
		const COMMODITYID module = ftlModule;
		const quint32 driveLeast = (quint32)ceil(design.FTLDriveUnitsToEngage() / CubicFeetPerCubicMeter()) + 1;
		const quint32 capacitorBefore = design.CapacitorVolume();
		quint32 capacitorLeast = 1;
		bool engages = false;
		while(capacitorLeast <= hull && !engages)
		{
			design.SetCapacitorVolume(capacitorLeast);
			engages = design.MaxCapacitorFTLSetting(module, design.MinQ(), design.MinQ()) >= 0;
			if(!engages)
				capacitorLeast += qMax((quint32)1, capacitorLeast / 64);
		}
		design.SetCapacitorVolume(capacitorBefore);
		double least = driveLeast;
		for(int i = 0; i < shares.size(); i++)
			least += shares[i].setting == QString("volume capacitor") ? qMax(shares[i].least, capacitorLeast) : shares[i].least;
		if(engages && least <= room)
		{
			for(int i = 0; i < shares.size(); i++)
				if(shares[i].setting == QString("volume capacitor"))
				{
					shares[i].least = qMax(shares[i].least, capacitorLeast);
					shares[i].want = qMax(shares[i].want, (double)shares[i].least);
					shares[i].how += QString(", and not less than engages the FTL drive");
				}
			s.lines.append(QString("FTL drive: to engage it this hull of %1 cubic metres needs %2 cubic metres of drive and %3 of "
					"capacitor at the design's minimum quality of %4 (a higher quality needs less), which come before everything "
					"else.").arg(hull).arg(driveLeast).arg(capacitorLeast).arg(design.MinQ()));
			const Share share = { QString("volume ftl %1").arg(engineer), qMax(engine * FtlShare, (double)driveLeast), driveLeast,
					QString("%1% of the engine rooms, and not less than this hull needs to engage it").arg(FtlShare * 100, 0, 'f', 0) };
			shares.append(share);
			s.ftlDetail = engineer;
			s.ftlModule = (int)module;
			AuScParts::iterator pi = design.Parts().find(part);
			if(pi != design.Parts().end())
				(*pi)->SetModule(module);
			design.SetStructure(SC_Starship);
		}
		else
			s.lines.append(QString("FTL drive: none. The old design is a Starship, but to engage an FTL drive this hull of %1 "
					"cubic metres needs %2 cubic metres of drive and %3 of capacitor, which do not fit beside what a spacecraft "
					"must have. The design is a Spaceship.").arg(hull).arg(driveLeast).arg(capacitorLeast));
	}
	else if(starship)
		s.lines.append(QString("FTL drive: none. The old design is a Starship but has no engineer station, which is what has "
				"the FTL drive in a current blueprint. The design is a Spaceship: add an engineer station."));

	double want = 0, least = 0;
	for(int i = 0; i < shares.size(); i++)
	{
		want += shares[i].want;
		least += shares[i].least;
	}
	QVector<quint32> given(shares.size());
	quint32 sum = 0;
	for(int i = 0; i < shares.size(); i++)
	{
		given[i] = qMax(shares[i].least, (quint32)floor(shares[i].want + 0.5));
		sum += given[i];
	}
	if(sum > room)
	{
		s.scaled = true;
		const double f = want > least && room > least ? (room - least) / (want - least) : 0.0;
		sum = 0;
		for(int i = 0; i < shares.size(); i++)
		{
			given[i] = shares[i].least + (quint32)floor((shares[i].want - shares[i].least) * qMin(1.0, f));
			sum += given[i];
		}
	}
	s.framework = room > sum ? (quint32)room - sum : 0;
	for(int i = 0; i < shares.size(); i++)
	{
		s.volumes.insert(shares[i].setting, given[i]);
		s.least.insert(shares[i].setting, shares[i].least);
		SetVolume(design, info, shares[i].setting, given[i]);
		s.lines.append(QString("%1: %2 cubic metres (%3)").arg(VolumeTitle(shares[i].setting, info)).arg(given[i]).arg(shares[i].how));
	}
	design.SetSkeletonVolume(s.framework);
	s.lines.append(QString("Framework: %1 cubic metres, what is left of the hull's %2%3").arg(s.framework).arg(hull)
			.arg(s.kept ? QString(" (armor and cryo berths have %1)").arg(s.kept) : QString()));

	log.append(QString("System volumes are suggested from the old design's rooms (its equipment is decoration); the hull has %1 "
			"cubic metres.%2").arg(hull)
			.arg(s.scaled ? QString(" The rooms ask for more than that (%1): everything is cut down alike.").arg((int)ceil(want))
				: QString()));
	for(int i = 0; i < s.lines.size(); i++)
		log.append(QString("  ") + s.lines[i]);
	if(sum > room)
		log.append(QString("WARNING: the hull is too small for what a spacecraft must have: %1 cubic metres are given out of %2.")
				.arg(sum).arg((quint32)room));
	if(s.ftlDetail)
		log.append(QString("The design is a Starship: its engineer station has the FTL drive."));
}

}
