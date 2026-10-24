#include "oldbuildingconverter.h"

#include "aubdreport.h"
#include "oldbuildingedit.h"
#include "oldshipedit.h"
#include "oldshiptypes.h"
#include <QMap>
#include <QRegExp>
#include <QSet>
#include <cmath>

namespace OldShip
{

namespace
{

const double FeetInMetre = 3.28083992;

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

QString RoomName(const Room& r)
{
	const char* name = RoomTypeBuildingName(r.type);
	const RoomTypeInfo* info = RoomTypeInfoOf(r.type);
	return QString("%1 %2").arg(QString::fromUtf8(name ? name : info && info->name ? info->name : "Room")).arg(r.style + 1);
}

QString DetailName(int type)
{
	const char* name = DetailTypeBuildingName(type);
	const DetailTypeInfo* info = DetailTypeInfoOf(type);
	return QString::fromUtf8(name ? name : info && info->name ? info->name : "equipment");
}

QString StoreyName(int deck)
{
	if(deck == 0)
		return QString("Ground storey (0)");
	return deck > 0 ? QString("Storey %1").arg(deck) : QString("Basement %1").arg(deck);
}

int CountDetails(const Design& d, int from, int to)
{
	int n = 0;
	for(size_t i = 0; i < d.details.size(); i++)
		if(d.details[i].type >= from && d.details[i].type <= to)
			n++;
	return n;
}

}

QList<Problem> BuildingProblems(const OldBuilding::Design& building, const BuildingInfo& info)
{
	Design rooms;
	BuildingToEdit(building, rooms);
	for(size_t i = rooms.rooms.size(); i > 0; i--)
		if(IsRoofType(rooms.rooms[i - 1].type))
			rooms.rooms.erase(rooms.rooms.begin() + (i - 1));
	QList<Problem> all = Problems(rooms, info);
	bool anyWay = false, groundWay = false;
	for(int i = 0; i < info.doors.size(); i++)
	{
		const ConvertInfo::DoorInfo& d = info.doors[i];
		if(d.roomA.isEmpty() == d.roomB.isEmpty())
			continue;
		anyWay = true;
		if(d.kind != ConvertInfo::DoorInfo::HullDoor && d.kind != ConvertInfo::DoorInfo::HullOpening)
			continue;
		for(size_t k = 0; k < rooms.details.size(); k++)
			if(rooms.details[k].id == d.detailId && rooms.details[k].type == d.detailType && rooms.details[k].deck == 0)
				groundWay = true;
	}
	if(anyWay && !groundWay)
	{
		bool said = false;
		for(int i = 0; i < info.rooms.size(); i++)
		{
			const ConvertInfo::RoomInfo& r = info.rooms[i];
			if(r.deck != 0 || !r.walkable)
				continue;
			Problem p;
			p.kind = Problem::NoWayIn;
			p.line = QString("%1 (deck %2)").arg(r.name).arg(r.deck);
			if(!said)
				p.text = QString("Nobody can walk in: no exterior door on the ground storey");
			said = true;
			p.deck = 0;
			p.room = r.name;
			p.at = AuPoint(r.centre.x + info.offset.x, r.centre.y + info.offset.y, 0);
			if(r.room >= 0 && r.room < (int)rooms.rooms.size())
				p.outline = OutlineOnFloor(rooms.rooms[r.room]);
			all.prepend(p);
		}
	}
	return all;
}

static QString Linked(const QList<Problem>* problems, int at, const QString& shown, bool red)
{
	const char* rgb = red ? "255, 32, 32" : "255, 255, 32";
	Q_UNUSED(problems);
	return QString("<span style=\"color: rgb(%1);\">%2</span><br>").arg(QString(rgb)).arg(ProblemLink(at, shown, rgb));
}

QString BuildingReportHtml(const OldBuilding::Design& building, const BuildingInfo& info, const QStringList& log, bool converted,
		const AuBdReport* game, const QStringList& gameMsgs, bool stale, const QList<Problem>* problems)
{
	Design asEdited;
	BuildingToEdit(building, asEdited);
	Design rooms(asEdited);
	for(size_t i = rooms.rooms.size(); i > 0; i--)
		if(IsRoofType(rooms.rooms[i - 1].type))
			rooms.rooms.erase(rooms.rooms.begin() + (i - 1));

	QString name = QString::fromUtf8(building.name.c_str()).trimmed();
	if(game && !game->DesignName().trimmed().isEmpty())
		name = game->DesignName();
	if(name.isEmpty())
		name = QString("Noname");
	const BuildingIndustryInfo* industry = BuildingIndustryInfoOf(building.industry);
	const QString industryName = !info.industryName.isEmpty() ? info.industryName
			: industry ? QString::fromUtf8(industry->name) : QString("industry %1").arg(building.industry);
	const int material = info.material ? info.material : (int)BM_Stone;

	QString html("<html>");
	html += QString("<b>%1</b><br>").arg(name.toHtmlEscaped());
	if(rooms.rooms.empty())
	{
		html += Line(QString("%1, of %2").arg(industryName.toUpper()).arg(BuildingMaterialName(material)));
		html += Line(QString("No rooms yet. Pick a room button and paint cells on the grid."));
		return html + QString("</html>");
	}

	QStringList errors, warnings, notDone, notes, gameLines;
	for(int i = 0; i < gameMsgs.size(); i++)
	{
		const QString line = Plain(gameMsgs[i]);
		if(line.startsWith("Error"))
			errors.append(line.mid(line.indexOf(':') + 1).trimmed());
		else if(line.startsWith("Warning"))
			warnings.append(line.mid(line.indexOf(':') + 1).trimmed());
		else if(!line.isEmpty())
			gameLines.append(line);
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
	const BuildingRules rules(building.industry);
	Editor editor(asEdited, &rules);
	std::vector<std::string> faults;
	editor.BuildingFaults(faults);

	bool noWayIn = false;
	const QStringList unreached = converted || !info.rooms.isEmpty() ? RoomsNobodyReaches(rooms, info, noWayIn) : QStringList();
	int exteriorDoors = 0, exteriorOpenings = 0, exteriorHatches = 0, doors = 0, openings = 0, hatches = 0, groundWays = 0;
	for(int i = 0; i < info.doors.size(); i++)
	{
		const ConvertInfo::DoorInfo& d = info.doors[i];
		int deck = 0;
		for(size_t k = 0; k < asEdited.details.size(); k++)
			if(asEdited.details[k].id == d.detailId && asEdited.details[k].type == d.detailType)
				deck = asEdited.details[k].deck;
		switch(d.kind)
		{
		case ConvertInfo::DoorInfo::HullDoor:
			exteriorDoors++;
			if(deck == 0)
				groundWays++;
			break;
		case ConvertInfo::DoorInfo::HullOpening:
			exteriorOpenings++;
			if(deck == 0)
				groundWays++;
			break;
		case ConvertInfo::DoorInfo::HullHatchTop:
		case ConvertInfo::DoorInfo::HullHatchBottom:
			exteriorHatches++;
			break;
		case ConvertInfo::DoorInfo::Hatch:
			hatches++;
			break;
		case ConvertInfo::DoorInfo::Opening:
			openings++;
			break;
		default:
			doors++;
			break;
		}
	}

	if(!converted)
		html += QString("<b>%1</b><br>").arg(Red(QString("INVALID DESIGN")));
	else
		html += QString("<b>%1</b><br>").arg(industryName.toUpper().toHtmlEscaped());
	QString head;
	head += Line(QString("Old building design, of %1").arg(BuildingMaterialName(material)));
	if(stale)
		head += Yellow(QString("The design was changed since these figures were worked out."));
	if(!converted)
		head += Red(QString("The design cannot be made into a blueprint as it is."));
	for(int i = 0; i < errors.size(); i++)
		head += Red(errors[i]);
	for(size_t i = 0; i < faults.size(); i++)
		head += Red(QString::fromStdString(faults[i]));
	if(converted && noWayIn)
		head += Linked(problems, FindProblem(problems, Problem::NoWayIn, QString()), QString("NOBODY CAN GET IN: the building has no "
				"exterior door. Put an Exterior Door, a Large Wall Door or an Exterior Wall Opening in an outside wall of the ground "
				"storey."), true);
	else if(converted && !groundWays)
		head += Linked(problems, FindProblem(problems, Problem::NoWayIn, QString()), QString("NOBODY CAN WALK IN: the building has no "
				"exterior door or opening on the ground storey; the only ways in are above or below the ground."), true);
	if(!unreached.isEmpty() && !noWayIn)
	{
		QStringList shut;
		for(int i = 0; i < unreached.size(); i++)
			shut.append(ProblemLink(FindProblem(problems, Problem::RoomUnreached, unreached[i]), unreached[i], "255, 32, 32"));
		head += QString("<span style=\"color: rgb(255, 32, 32);\">%1%2</span><br>")
				.arg(QString("%1 rooms cannot be reached, having no door, hatch, opening or stairs: ").arg(unreached.size()).toHtmlEscaped())
				.arg(shut.join(", "));
	}
	if(problems)
		for(int i = 0; i < problems->size(); i++)
			if((*problems)[i].kind == Problem::InUnreachedRoom)
				head += Linked(problems, i, (*problems)[i].line, false);
	if(!info.industryFault.isEmpty())
		head += Red(QString("The blueprint cannot be used for the industry %1: %2").arg(industryName).arg(info.industryFault));
	for(int i = 0; i < warnings.size(); i++)
		head += Yellow(warnings[i]);
	for(int i = 0; i < info.stairsShut.size(); i++)
		head += Linked(problems, FindProblem(problems, Problem::StairsShut, info.stairsShut[i]), info.stairsShut[i], false);
	if(game && errors.isEmpty())
		head += Line(QString("The game will finalize the blueprint of this design."));
	html += Sub(head);

	html += Section("BUILDING");
	QString bd;
	int lo = 0, hi = 0;
	QMap<int, QStringList> byStorey;
	for(size_t i = 0; i < rooms.rooms.size(); i++)
	{
		const Room& r = rooms.rooms[i];
		if(i == 0 || r.deck < lo) lo = r.deck;
		if(i == 0 || r.deck > hi) hi = r.deck;
		byStorey[r.deck].append(RoomName(r));
	}
	bd += Line(QString("%1 Rooms on %2 Storeys (%3 to %4)%5").arg((int)rooms.rooms.size()).arg(byStorey.size()).arg(lo).arg(hi)
			.arg(lo < 0 ? QString(", %1 of them below the ground").arg(-lo) : QString()));
	const int allowed = BuildingMaxDeck(building.industry, material), allowedAtBest = BuildingMaxDeck(building.industry);
	if(hi > allowed && hi <= allowedAtBest)
		bd += Yellow(QString("Of %1 the old game built a %2 up to storey %3 only. The blueprint has all the storeys of the design.")
				.arg(BuildingMaterialName(material)).arg(industryName).arg(allowed));
	if(game)
	{
		bd += Line(QString("%1 Volume").arg(Cubic(game->HullVolume())));
		AuPoint low, high;
		game->Extents(low, high);
		bd += Sub(Line(QString("%L1m North to South").arg((high.y - low.y) / FeetInMetre, 0, 'f', 1))
				+ Line(QString("%L1m East to West").arg((high.x - low.x) / FeetInMetre, 0, 'f', 1))
				+ Line(QString("%L1m Height").arg((high.z - low.z) / FeetInMetre, 0, 'f', 1)));
	}
	if(converted)
		bd += Line(QString("Site %L1m by %L2m").arg((info.siteHi.x - info.siteLo.x) / FeetInMetre, 0, 'f', 1)
				.arg((info.siteHi.y - info.siteLo.y) / FeetInMetre, 0, 'f', 1));
	int gable = 0, hip = 0, domes = 0;
	for(size_t i = 0; i < asEdited.rooms.size(); i++)
	{
		gable += asEdited.rooms[i].type == 22 ? 1 : 0;
		hip += asEdited.rooms[i].type == 23 ? 1 : 0;
		domes += asEdited.rooms[i].type == 24 ? 1 : 0;
	}
	if(gable + hip + domes)
		bd += Line(QString("%1 Roofs: %2 gable, %3 hip, %4 round").arg(gable + hip + domes).arg(gable).arg(hip).arg(domes));
	else
		bd += Line(QString("No roofs: the top storey's ceiling is the roof"));
	if(info.windows)
		bd += Line(QString("Windows of %1 rooms light up at night, while the building is in use and has power").arg(info.windows));
	const int flagPoles = CountDetails(asEdited, 100, 100), lightPoles = CountDetails(asEdited, 142, 142);
	if(flagPoles || lightPoles)
		bd += Line(QString("%1 Flag Pole, %2 Light Pole").arg(flagPoles).arg(lightPoles));
	html += Sub(bd);

	html += Section("ROOMS");
	QString rm;
	QMapIterator<int, QStringList> storey(byStorey);
	storey.toBack();
	while(storey.hasPrevious())
	{
		storey.previous();
		rm += Line(QString("%1: %2").arg(StoreyName(storey.key())).arg(storey.value().join(", ")));
	}
	html += Sub(rm);

	html += Section("DOORS");
	QString dr;
	if(converted && noWayIn)
		dr += Red(QString("No way in from outside."));
	dr += Line(QString("%1 Exterior Doors, %2 Exterior Wall Openings, %3 Exterior Hatches").arg(exteriorDoors).arg(exteriorOpenings)
			.arg(exteriorHatches));
	if(converted && !noWayIn)
		dr += Sub(Line(QString("%1 of the doors and openings are on the ground storey").arg(groundWays)));
	dr += Line(QString("%1 Doors, %2 Wall Openings, %3 Hatches between rooms").arg(doors).arg(openings).arg(hatches));
	int stairsOpen = 0;
	for(int i = 0; i < info.stairs.size(); i++)
		if(info.stairs[i].open)
			stairsOpen++;
	if(!info.stairs.isEmpty())
		dr += Line(QString("%1 Stairs, of which %2 lead from storey to storey").arg(info.stairs.size()).arg(stairsOpen));
	if(!info.railings.isEmpty())
		dr += Line(QString("%1 Railings").arg(info.railings.size()));
	html += Sub(dr);

	html += Section("QUARTERS");
	QString quarters;
	const int beds = CountDetails(asEdited, 65, 72), bunks = CountDetails(asEdited, 2, 2) + CountDetails(asEdited, 25, 31);
	quarters += Line(QString("%1 Beds, %2 Bunk Beds in the old design").arg(beds).arg(bunks));
	if(converted)
	{
		QMap<QString, int> kinds;
		for(int i = 0; i < info.berths.size(); i++)
			kinds[info.berths[i].what]++;
		QStringList made;
		for(QMap<QString, int>::const_iterator ki = kinds.constBegin(); ki != kinds.constEnd(); ki++)
			made.append(QString("%1 %2s").arg(ki.value()).arg(ki.key()));
		quarters += Line(made.isEmpty() ? QString("No berths in the blueprint") : QString("In the blueprint: %1").arg(made.join(", ")));
	}
	if(game)
		quarters += Line(QString("%1 Beds counted by the game").arg(game->Beds()));
	html += Sub(quarters);

	html += Section("STATIONS");
	QString st;
	for(int i = 0; i < info.stations.size(); i++)
		st += Line(QString("%1 in %2").arg(DetailName(info.stations[i].detailType)).arg(info.stations[i].room));
	if(info.stations.isEmpty())
		st += Line(QString("None"));
	if(!info.tables.isEmpty())
		st += Line(QString("%1 Game Tables").arg(info.tables.size()));
	for(int i = 0; i < info.skipped.size(); i++)
		st += Linked(problems, qMax(FindProblem(problems, Problem::LeftOut, info.skipped[i]),
					FindProblem(problems, Problem::RoomShut, info.skipped[i])), info.skipped[i], false);
	html += Sub(st);

	html += Section("INDUSTRY");
	QString ind;
	ind += Line(QString("%1 (%2)").arg(industryName).arg(building.industry));
	if(game)
	{
		struct Share
		{
			const char*	name;
			quint32		volume;
		};
		const Share shares[] =
		{
			{ "Arena", game->ArenaVolume() }, { "Auditorium", game->AuditoriumVolume() }, { "Large Homes", game->HomeLargeVolume() },
			{ "Medium Homes", game->HomeMediumVolume() }, { "Small Homes", game->HomeSmallVolume() }, { "Lounge", game->LoungeVolume() },
			{ "Office", game->OfficeVolume() }, { "Workshop", game->ShopVolume() }, { "Storage", game->HoldVolume() },
			{ "Generator", game->PowerPlantVolume() }, { "Battery", game->CapacitorVolume() }, { "Radar", game->SensorVolume() },
			{ "Life Support", game->LifeSupportVolume() }, { "Armor", game->ArmorVolume() }, { "Weapon Bays", game->BayVolume() }
		};
		quint32 given = 0;
		QString list;
		for(size_t i = 0; i < sizeof(shares) / sizeof(shares[0]); i++)
			if(shares[i].volume)
			{
				list += Line(QString("%1 %2").arg(Cubic(shares[i].volume)).arg(shares[i].name));
				given += shares[i].volume;
			}
		ind += Line(QString("%1 given to the industry's needs, of %2 the building holds").arg(Cubic(given)).arg(Cubic(game->HullVolume())));
		ind += Sub(list);
		if(given > game->HullVolume())
			ind += Yellow(QString("More volume is given out than the building has: the least the industry asks of a blueprint is more "
					"than these rooms hold. The game accepts it; a bigger building would hold it honestly."));
	}
	html += Sub(ind);

	if(game || !gameLines.isEmpty())
	{
		html += Section("CONSTRUCTION");
		QString made;
		for(int i = 0; i < log.size(); i++)
			if(log[i].startsWith("Size:") || log[i].startsWith("Made "))
				made += Line(log[i]);
		for(int i = 0; i < gameLines.size(); i++)
			made += Line(gameLines[i]);
		if(game && !errors.isEmpty())
			made += Red(QString("UNABLE TO CONSTRUCT: see the lines in red at the top."));
		html += Sub(made);
	}

	if(!notDone.isEmpty() || !notes.isEmpty())
	{
		html += Section("NOTES");
		QString rest;
		for(int i = 0; i < notDone.size(); i++)
			rest += Linked(problems, qMax(FindProblem(problems, Problem::LeftOut, notDone[i]),
						FindProblem(problems, Problem::RoomShut, notDone[i])), QString("Not done: ") + notDone[i], false);
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
