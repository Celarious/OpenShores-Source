#include "oldshipconverter.h"

#include "auscreport.h"
#include "oldshipedit.h"
#include <QMap>
#include <QProgressDialog>
#include <QRegExp>
#include <QSet>
#include <cmath>

namespace OldShip
{

namespace
{

const double FeetInMetre = 3.28083992;
const double SquareFeetInSquareMetre = 10.7639104;

QString Red(const QString& s)
{
	return QString("<span style=\"color: rgb(255, 32, 32);\">%1</span><br>").arg(s.toHtmlEscaped());
}

QString Yellow(const QString& s)
{
	return QString("<span style=\"color: rgb(255, 255, 32);\">%1</span><br>").arg(s.toHtmlEscaped());
}

QString Section(const QString& title)
{
	return QString("<hr><b>%1</b><br>").arg(title);
}

QString Line(const QString& s)
{
	return s.toHtmlEscaped() + QString("<br>");
}

QString Sub(const QString& html)
{
	return html.isEmpty() ? html : QString("<div style=\"margin-left: 10px;\">%1</div>").arg(html);
}

QString Cubic(quint32 metres)
{
	return QString("%L1m%2").arg(metres).arg(QChar(0x00B3));
}

QString Plain(QString html)
{
	html.replace(QRegExp("<[^>]*>"), QString());
	html.replace(QString("&nbsp;"), QString(" "));
	return html.trimmed();
}

QStringList UnreachedRooms(const Design& ship, const ConvertInfo& info, bool& noWayIn)
{
	QMap<QString, QSet<QString> > links;
	QSet<QString> reached, frontier;
	for(int i = 0; i < info.doors.size(); i++)
	{
		const ConvertInfo::DoorInfo& d = info.doors[i];
		if(d.roomA.isEmpty() || d.roomB.isEmpty())
			frontier.insert(d.roomA.isEmpty() ? d.roomB : d.roomA);
		else
		{
			links[d.roomA].insert(d.roomB);
			links[d.roomB].insert(d.roomA);
		}
	}
	for(int i = 0; i < info.stations.size(); i++)
		if(info.stations[i].detailType == 19)
			frontier.insert(info.stations[i].room);
	for(int i = 0; i < info.stairs.size(); i++)
		if(!info.stairs[i].roomHigh.isEmpty())
		{
			links[info.stairs[i].roomLow].insert(info.stairs[i].roomHigh);
			links[info.stairs[i].roomHigh].insert(info.stairs[i].roomLow);
		}
	for(int i = 0; i < info.rooms.size(); i++)
	{
		const ConvertInfo::RoomInfo& r = info.rooms[i];
		if(!r.openBelow)
			continue;
		const double gx = (r.centre.x + info.offset.x) / GridUnit, gy = (r.centre.y + info.offset.y) / GridUnit;
		for(int k = 0; k < info.rooms.size(); k++)
		{
			const ConvertInfo::RoomInfo& under = info.rooms[k];
			if(under.deck == r.deck - 1 && under.room >= 0 && under.room < (int)ship.rooms.size()
					&& PointInOutline(ship.rooms[under.room].points, gx, gy, true))
			{
				links[r.name].insert(under.name);
				links[under.name].insert(r.name);
			}
		}
	}
	noWayIn = frontier.isEmpty();
	while(!frontier.isEmpty())
	{
		const QString room = *frontier.begin();
		frontier.remove(room);
		if(reached.contains(room))
			continue;
		reached.insert(room);
		foreach(const QString& next, links.value(room))
			if(!reached.contains(next))
				frontier.insert(next);
	}
	QStringList unreached;
	for(int i = 0; i < info.rooms.size(); i++)
		if(info.rooms[i].walkable && !reached.contains(info.rooms[i].name))
			unreached.append(QString("%1 (deck %2)").arg(info.rooms[i].name).arg(info.rooms[i].deck));
	return unreached;
}

int DeckOfDetail(const Design& ship, int detailId, int detailType)
{
	for(size_t i = 0; i < ship.details.size(); i++)
		if(ship.details[i].id == detailId && ship.details[i].type == detailType)
			return ship.details[i].deck;
	return 0;
}

int CountDetails(const Design& ship, int from, int to)
{
	int n = 0;
	for(size_t i = 0; i < ship.details.size(); i++)
		if(ship.details[i].type >= from && ship.details[i].type <= to)
			n++;
	return n;
}

double RoomArea(const Design& ship, int roomType, int& count)
{
	double area = 0;
	count = 0;
	for(size_t i = 0; i < ship.rooms.size(); i++)
	{
		const Room& r = ship.rooms[i];
		if(r.type != roomType)
			continue;
		long twice = 0;
		for(size_t k = 0; k < r.points.size(); k++)
			twice += (long)r.points[k].x * r.points[(k + 1) % r.points.size()].y - (long)r.points[(k + 1) % r.points.size()].x * r.points[k].y;
		area += fabs((double)twice) / 2 * GridUnit * GridUnit / SquareFeetInSquareMetre;
		count++;
	}
	return area;
}

}

QStringList RoomsNobodyReaches(const Design& ship, const ConvertInfo& info, bool& noWayIn)
{
	return UnreachedRooms(ship, info, noWayIn);
}

bool AnalyzeConverted(AuScDesign& design, AuScReport& report, QStringList& msgs)
{
	QProgressDialog progress;
	progress.setMinimumDuration(1000000);
	const bool ok = design.FinalizeDesign(msgs, progress);
	progress.reset();
	design.ReportDesign(report);
	return ok;
}

static QString BerthRooms(const ConvertInfo& info, const char* what)
{
	QStringList rooms;
	for(int i = 0; i < info.berths.size(); i++)
		if(info.berths[i].what == QString(what) && !rooms.contains(info.berths[i].room))
			rooms.append(info.berths[i].room);
	return rooms.isEmpty() ? QString() : QString(", in %1").arg(rooms.join(", "));
}

static QString Linked(const QList<Problem>* problems, Problem::KIND kind, const QString& line, const QString& shown, bool red)
{
	const char* rgb = red ? "255, 32, 32" : "255, 255, 32";
	return QString("<span style=\"color: rgb(%1);\">%2</span><br>").arg(QString(rgb))
			.arg(ProblemLink(FindProblem(problems, kind, line), shown, rgb));
}

QString ReportHtml(const Design& ship, const ConvertInfo& info, const QStringList& log, bool converted, const AuScReport* game,
		const QStringList& gameMsgs, bool stale, const QList<Problem>* problems)
{
	QString name = QString::fromUtf8(ship.name.c_str()).trimmed();
	if(game && !game->DesignName().trimmed().isEmpty())
		name = game->DesignName();
	if(name.isEmpty())
		name = QString("Noname");
	const DesignCounts counts = Count(ship);

	QStringList errors, warnings, notDone, notes;
	for(int i = 0; i < gameMsgs.size(); i++)
	{
		const QString line = Plain(gameMsgs[i]);
		if(line.startsWith("Error"))
			errors.append(line.mid(line.indexOf(':') + 1).trimmed());
		else if(line.startsWith("Warning"))
			warnings.append(line.mid(line.indexOf(':') + 1).trimmed());
	}
	for(int i = 0; i < log.size(); i++)
	{
		if(log[i].startsWith("WARNING") || log[i].startsWith("FAILED") || log[i].startsWith("TOO BIG"))
			errors.append(log[i]);
		else if(log[i].startsWith("NOT DONE: "))
			notDone.append(log[i].mid(10));
		else if(log[i].startsWith("NOTE: ") && !info.stairsShut.contains(log[i].mid(6)))
			notes.append(log[i].mid(6));
	}
	bool noWayIn = false;
	const QStringList unreached = converted || !info.rooms.isEmpty() ? UnreachedRooms(ship, info, noWayIn) : QStringList();

	QString html("<html>");
	html += QString("<b>%1</b><br>").arg(name.toHtmlEscaped());
	if(ship.rooms.empty())
	{
		html += Line(QString("No rooms yet. Pick a room button and paint cells on the grid."));
		return html + QString("</html>");
	}
	if(!converted)
		html += QString("<b>%1</b><br>").arg(Red(QString("INVALID DESIGN")));
	else if(game)
		html += QString("<b>%1%2</b><br>").arg(game->Streamline() != SLINE_None ? QString("STREAMLINED ") : QString())
				.arg(SpacecraftStructureName(game->Structure()).toUpper().toHtmlEscaped());
	QString head;
	if(game)
		head += Line(QString("Quality %1").arg(game->Quality()));
	if(stale)
		head += Yellow(QString("The design was changed since these figures were worked out."));
	if(!converted)
		head += Red(QString("The design cannot be made into a blueprint as it is."));
	if(game && errors.isEmpty())
		head += Line(QString("The game will finalize the blueprint of this design."));
	html += Sub(head);

	html += Section("HULL");
	QString hull;
	for(int i = 0; i < errors.size(); i++)
		hull += Red(errors[i]);
	if(noWayIn && converted)
		hull += Linked(problems, Problem::NoWayIn, QString(), QString("Hull Opening or Transporter Needed: nobody can get in from outside."), true);
	if(!unreached.isEmpty() && !noWayIn)
	{
		QStringList rooms;
		for(int i = 0; i < unreached.size(); i++)
			rooms.append(ProblemLink(FindProblem(problems, Problem::RoomUnreached, unreached[i]), unreached[i], "255, 32, 32"));
		hull += QString("<span style=\"color: rgb(255, 32, 32);\">%1%2</span><br>")
				.arg(QString("%1 rooms cannot be reached, having no door, hatch, opening or stairs: ").arg(unreached.size()).toHtmlEscaped())
				.arg(rooms.join(", "));
	}
	if(problems)
		for(int i = 0; i < problems->size(); i++)
			if((*problems)[i].kind == Problem::InUnreachedRoom)
				hull += Linked(problems, Problem::InUnreachedRoom, (*problems)[i].line, (*problems)[i].line, false);
	for(int i = 0; i < warnings.size(); i++)
		hull += Yellow(warnings[i]);
	for(int i = 0; i < info.stairsShut.size(); i++)
		hull += Linked(problems, Problem::StairsShut, info.stairsShut[i], info.stairsShut[i], false);
	hull += Line(QString("%1 Rooms on %2 Decks (%3 to %4)").arg(counts.rooms).arg(counts.decks).arg(ship.minDeck).arg(ship.maxDeck));
	hull += Line(ship.armor ? QString("Armor Level %1 of the old design: the look of the hull's walls and windows").arg(ship.armor)
			: QString("Unarmored in the old design: the look of the hull's walls and windows"));
	if(game)
	{
		hull += Line(QString("Armor Level %1, %2 of Armor").arg(game->ArmorLevel(), 0, 'f', 1).arg(Cubic(game->ArmorVolume())));
		hull += Line(QString("%1 Volume").arg(Cubic(game->HullVolume())));
		AuPoint lo, hi;
		game->Extents(lo, hi);
		QString size;
		const bool mustSpace = game->MustBeBuiltInSpace();
		size += Line(QString("%L1m Length").arg((hi.y - lo.y) / FeetInMetre, 0, 'f', 1));
		size += Line(QString("%L1m Width").arg((hi.x - lo.x) / FeetInMetre, 0, 'f', 1));
		size += Line(QString("%L1m Height, %2 Decks").arg((hi.z - lo.z) / FeetInMetre, 0, 'f', 1).arg(counts.decks));
		if(mustSpace)
			size += QString("<b>Must Be Built in Space</b><br>");
		hull += Sub(size);
		hull += Line(QString("%L1kg Mass").arg((double)game->MassKg(), 0, 'f', 0));
	}
	html += Sub(hull);

	html += Section("ENGINEERING");
	QString eng;
	const int drives = CountDetails(ship, 10, 10), plants = CountDetails(ship, 14, 14), ftl = CountDetails(ship, 24, 24);
	eng += Line(QString("%L1 Maneuver Drive Units, %L2 Power Plant Units, %L3 FTL Drive Units drawn (for looks)").arg(drives).arg(plants).arg(ftl));
	if(game)
	{
		eng += Line(QString("%1 Maneuver Drive").arg(Cubic(game->ManeuverDriveVolume())));
		eng += Sub(Line(QString("%L1m/sec/sec Acceleration").arg(game->ManeuverAcceleration() / FeetInMetre, 0, 'f', 1))
				+ Line(QString("%L1m/sec/sec Vertical Acceleration").arg(game->ManeuverAccelerationVertical() / FeetInMetre, 0, 'f', 1)));
		eng += Line(QString("%1 Power Plant").arg(Cubic(game->PowerPlantVolume())));
		eng += Sub(Line(QString("%1 Capacitor, Holding %L2").arg(Cubic(game->CapacitorVolume())).arg(game->CapacitorMax(), 0, 'f', 1)));
		if(game->HasFTLDrive())
			eng += Line(QString("%1 FTL Drive").arg(Cubic(game->FTLDriveVolume())))
					+ Sub(Line(QString("%L1m%2 Sufficient to Engage").arg(ceil(game->FTLDriveUnitsToEngageCuFt() / 35.3146667), 0, 'f', 0)
								.arg(QChar(0x00B3))));
		else
			eng += Line(QString("No FTL Drive"));
	}
	html += Sub(eng);

	html += Section("FUEL");
	QString fuel;
	int count = 0;
	const double fuelArea = RoomArea(ship, 9, count);
	if(count)
		fuel += Line(QString("%1 Fuel Cells drawn, %L2m%3 of floor (for looks)").arg(count).arg(fuelArea, 0, 'f', 0).arg(QChar(0x00B2)));
	if(game)
		fuel += game->FuelVolume() ? Line(QString("%1 Fuel, Room for %L2 Units of Hydrogen").arg(Cubic(game->FuelVolume())).arg(game->FuelCapacity()))
				: Red(QString("NO FUEL"));
	html += Sub(fuel);

	html += Section("AVIONICS");
	QString avionics;
	for(int i = 0; i < info.stations.size(); i++)
	{
		const ConvertInfo::StationInfo& s = info.stations[i];
		QString what = s.what;
		what[0] = what[0].toUpper();
		avionics += Line(QString("%1 - %2 Deck %3").arg(what).arg(s.room).arg(DeckOfDetail(ship, s.detailId, s.detailType)));
	}
	if(!info.tables.isEmpty())
		avionics += Line(QString("%L1 Game Tables").arg(info.tables.size()));
	if(game)
	{
		avionics += Line(QString("%1 Life Support").arg(Cubic(game->LifeSupportVolume())));
		avionics += Line(QString("%1 Sensors").arg(Cubic(game->SensorVolume())));
		if(game->HasSensor())
			avionics += Sub(Line(QString("Range %L1m").arg(game->SensorRange() / FeetInMetre, 0, 'f', 0)));
		avionics += Line(QString("%1 Shields").arg(Cubic(game->ShieldVolume())));
	}
	int doors = 0, openings = 0;
	for(int i = 0; i < info.doors.size(); i++)
		(info.doors[i].part ? doors : openings)++;
	avionics += Line(QString("%L1 Operable Doors/Hatches, %L2 Openings, %L3 Stairs, %L4 Railings").arg(doors).arg(openings).arg(info.stairs.size())
			.arg(info.railings.size()));
	html += Sub(avionics);

	html += Section("ARMAMENT");
	QString arms;
	int bays = 0;
	for(int i = 0; i < info.stations.size(); i++)
	{
		const ConvertInfo::StationInfo& s = info.stations[i];
		if(s.detailType != 7)
			continue;
		bays++;
		bool emitter = false;
		for(int e = 0; e < info.emitters.size(); e++)
			emitter = emitter || info.emitters[e].part == s.part;
		arms += Line(QString("Weapon Bay %1, Fire Control Station in %2%3").arg(bays).arg(s.room)
				.arg(game ? QString(", %1").arg(Cubic(game->BayVolume(s.part))) : QString()));
		arms += Sub(Line(emitter ? QString("Fires From Its Weapon Emitter") : QString("No Weapon Emitter: Fires From the Middle of the Ship")));
	}
	if(!bays)
		arms += Line(QString("No Weapon Bays (a weapon bay is a fire control station)"));
	const int turrets = CountDetails(ship, 21, 22) + CountDetails(ship, 33, 40);
	if(turrets)
		arms += Line(QString("%L1 Turrets drawn (for looks)").arg(turrets));
	html += Sub(arms);

	html += Section("CREW");
	QString crew;
	if(game)
	{
		crew += Line(QString("%L1 Officer Stations").arg(game->CommandStationCount()));
		crew += Line(QString("%L1 Crew Stations").arg(game->CrewStationCount()));
		if(!game->GuardPosts().empty())
			crew += Line(QString("%L1 Guards").arg((int)game->GuardPosts().size()));
	}
	else
		crew += Line(QString("%L1 Stations").arg(info.stations.size()));
	html += Sub(crew);

	html += Section("QUARTERS");
	QString quarters;
	if(game)
	{
		quarters += Line(QString("%L1 Captain's Berth%2%3").arg(game->CaptBerths()).arg(game->CaptBerths() == 1 ? "" : "s")
				.arg(BerthRooms(info, "captain's berth")));
		quarters += Line(QString("%L1 Officer Berths%2").arg(game->OfficerBerths()).arg(BerthRooms(info, "officer berth")));
		quarters += Line(QString("%L1 Crew Berths%2").arg(game->CrewBerths()).arg(BerthRooms(info, "crew berth")));
		quarters += Line(QString("%L1 Passenger Berths%2").arg(game->PassengerBerths()).arg(BerthRooms(info, "passenger berth")));
		quarters += Line(QString("%L1 Troop Berths%2").arg(game->TroopBerths()).arg(BerthRooms(info, "troop berth")));
		if(game->CryoBerths())
			quarters += Line(QString("%L1 Cryo Berths").arg(game->CryoBerths()));
		if(!game->CaptBerths())
			quarters += Sub(Line(QString("No captain's berth: put a berth in an Officer Cabin. The first bunk there is yours.")));
		else if(!game->OfficerBerths())
			quarters += Sub(Line(QString("The only bunk in the officer cabins became the captain's berth, so there is none for an "
					"officer. Add a second bunk to an Officer Cabin (or use a double berth) for one.")));
	}
	else
		quarters += Line(QString("%L1 Berths").arg(info.berths.size()));
	html += Sub(quarters);

	html += Section("HOLD");
	QString hold;
	const double holdArea = RoomArea(ship, 5, count);
	if(count)
		hold += Line(QString("%1 Holds drawn, %L2m%3 of floor").arg(count).arg(holdArea, 0, 'f', 0).arg(QChar(0x00B2)));
	if(game)
		hold += Line(QString("%1 Storage Space").arg(Cubic(game->HoldVolume())))
				+ Sub(Line(QString("Room for %L1 Commodities").arg(game->HoldCapacity())));
	html += Sub(hold);

	html += Section("VEHICLE BAYS");
	QString bay;
	const double bayArea = RoomArea(ship, 13, count);
	bay += Line(QString("%1 Vehicle Bays, %L2m%3 of floor").arg(count).arg(bayArea, 0, 'f', 0).arg(QChar(0x00B2)));
	int ground = 0, space = 0;
	for(int i = 0; i < info.parking.size(); i++)
		(info.parking[i].what.startsWith("ground") ? ground : space)++;
	bay += Line(QString("%L1 Ground Vehicle Parking Spots").arg(ground));
	bay += Line(QString("%L1 Space Vehicle Parking Spots").arg(space));
	html += Sub(bay);

	if(game)
	{
		html += Section("DAMAGE CONTROL");
		QString damage;
		damage += Line(QString("%L1 Total Hit Points").arg(game->TotalHits()));
		damage += Line(QString("%L1 Structure Hit Points").arg(game->StructureHits()));
		damage += Line(QString("%L1 Equipment Hit Points").arg(game->NonStructureHits()));
		html += Sub(damage);
	}

	html += Section("MANUFACTURE");
	QString made;
	for(int i = 0; i < log.size(); i++)
		if(log[i].startsWith("Size:") || log[i].startsWith("Made "))
			made += Line(log[i]);
	if(game)
		made += errors.isEmpty() ? Line(QString("A spacecraft can be manufactured using the blueprint."))
				: Red(QString("UNABLE TO MANUFACTURE: see the lines in red under HULL."));
	html += Sub(made);

	if(!notDone.isEmpty() || !notes.isEmpty())
	{
		html += Section("NOTES");
		QString rest;
		for(int i = 0; i < notDone.size(); i++)
		{
			const int at = qMax(FindProblem(problems, Problem::LeftOut, notDone[i]), FindProblem(problems, Problem::RoomShut, notDone[i]));
			rest += QString("<span style=\"color: rgb(255, 255, 32);\">%1</span><br>")
					.arg(ProblemLink(at, QString("Not done: ") + notDone[i], "255, 255, 32"));
		}
		for(int i = 0; i < notes.size(); i++)
			rest += Line(notes[i]);
		html += Sub(rest);
	}
	QString rest;
	for(int i = 0; i < log.size(); i++)
		if(!log[i].startsWith("WARNING") && !log[i].startsWith("FAILED") && !log[i].startsWith("TOO BIG") && !log[i].startsWith("NOT DONE: ")
				&& !log[i].startsWith("NOTE: ") && !log[i].startsWith("Size:") && !log[i].startsWith("Made "))
			rest += Line(log[i]);
	if(!rest.isEmpty())
		html += Section("HOW THE BLUEPRINT IS MADE") + Sub(QString("<span style=\"color: rgb(160, 160, 160);\">%1</span>").arg(rest));
	return html + QString("</html>");
}

}
