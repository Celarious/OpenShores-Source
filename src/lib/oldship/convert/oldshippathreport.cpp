#include "oldshipconvert.h"
//
#include "oldpath.h"
#include "oldpathship.h"
#include "oldshiptypes.h"
#include <QHash>
#include <QVector>
#include <algorithm>

namespace OldShip
{

namespace
{

enum TARGETRULE { RuleNone, RuleCrew, RuleTroop, RuleOfficer, RulePublic };

TARGETRULE Rule(int type)
{
	switch(type)
	{
	case 1: case 6: case 7: case 12: case 13: case 15: case 17:
	case 57: case 58: case 59: case 60: case 61: case 62: case 63: case 64:
		return RuleCrew;
	case 21: case 22: case 33: case 34: case 35: case 36: case 37: case 38: case 39: case 40:
		return RuleTroop;
	case 73:
		return RuleOfficer;
	case 4: case 11: case 19: case 20: case 74: case 75:
		return RulePublic;
	default:
		return RuleNone;
	}
}

bool IsBerth(int type)
{
	return type == 2 || (type >= 25 && type <= 31) || (type >= 65 && type <= 72);
}

bool IsOutsideStation(int type)
{
	return Rule(type) != RuleNone && !(type >= 57 && type <= 64) && type != 20;
}

bool IsPublicOutsideStation(int type)
{
	return type == 4 || type == 11 || type == 19 || type == 20 || type == 74 || type == 75;
}

bool ChecksBerth(TARGETRULE rule, int roomType)
{
	if(roomType == 19)
		return rule != RuleNone;
	switch(rule)
	{
	case RuleCrew:
		return roomType == 4 || roomType == 7;
	case RuleTroop:
		return roomType == 1;
	case RuleOfficer:
		return roomType == 4;
	case RulePublic:
		return true;
	default:
		return false;
	}
}

QString RoomName(int type)
{
	const RoomTypeInfo* info = RoomTypeInfoOf(type);
	return info && info->name ? QString::fromUtf8(info->name) : QString("Unknown Room");
}

QString DetailName(int type)
{
	const DetailTypeInfo* info = DetailTypeInfoOf(type);
	return info && info->name ? QString::fromUtf8(info->name) : QString("Detail %1").arg(type);
}

QString Place(const Room* room, int deck, int minDeck)
{
	return QString("%1 %2 deck %3").arg((room ? RoomName(room->type) : QString("Unknown Room")).toHtmlEscaped())
			.arg(room ? room->style + 1 : 1).arg(deck - minDeck + 1);
}

QString Station(const Detail& detail, const Room* room, int minDeck, bool roomDeck)
{
	return QString("%1 in %2").arg(DetailName(detail.type).toHtmlEscaped())
			.arg(Place(room, roomDeck && room ? room->deck : detail.deck, minDeck));
}

void Warning(QString& html, PathReportCounts& counts, const QString& message, bool security = false)
{
	(security ? counts.securityWarnings : counts.pathWarnings)++;
	html += QString("&middot; <span style=\"color: rgb(%1);\">%2 Warning:</span> %3<br>")
			.arg(security ? QString("255,255,0") : QString("255,0,0")).arg(security ? QString("Security") : QString("Path"))
			.arg(message);
}

void FinishSection(QString& html, bool warned)
{
	if(!warned)
		html += QString("&middot; <span style=\"color: rgb(0,255,0);\">Ok</span><br>");
	html += QString("<br>");
}

QString Canceled(const QString& html)
{
	return html + QString("<span style=\"color: rgb(255,255,0);\">Canceled</span><br>");
}

void SeatPoint(const Detail& d, double& x, double& y)
{
	x = d.x * GridUnit;
	y = d.y * GridUnit;
	switch(d.type)
	{
	case 21: x -= 0.5; break;
	case 34: x -= 0.35; y -= 0.35; break;
	case 35: y -= 0.5; break;
	case 36: x += 0.35; y -= 0.35; break;
	case 37: x += 0.5; break;
	case 38: x += 0.35; y += 0.35; break;
	case 39: y += 0.5; break;
	case 40: x -= 0.35; y += 0.35; break;
	case 74: case 75: x += 4.0; y += 4.0; break;
	default: break;
	}
}

int SeatDeck(const Detail& d)
{
	return d.deck - (d.type == 22 ? 1 : 0);
}

struct Router
{
	OldPath::Openings		openings;
	OldPath::Path			path;
	OldPath::RoomArray		rooms;
	OldPath::ShipFrame		frame;
	OldPath::DoorStates		doors;
	std::vector<OldPath::Point>	outside;

	explicit Router(const Design& design)
	{
		OldPath::AnalyzeOpenings(design, openings);
		path.CreateSpacecraft(design, openings);
		rooms.FromDesign(design);
		OldPath::AnalyzeFrame(design, frame);
		doors.SetDoorCount((unsigned)openings.doors.size());
		const OldPath::Point cg((frame.min.x + frame.max.x) / 2, (frame.min.y + frame.max.y) / 2,
				(frame.min.z + frame.max.z) / 2);
		const double ringFar = 341.41125494, ringHalf = 170.70562747;
		const double ring[16][2] =
		{
			{ ringFar, 0 }, { ringFar, ringHalf }, { ringFar, ringFar }, { ringHalf, ringFar }, { 0, ringFar }, { -ringHalf, ringFar }, { -ringFar, ringFar }, { -ringFar, ringHalf },
			{ -ringFar, 0 }, { -ringFar, -ringHalf }, { -ringFar, -ringFar }, { -ringHalf, -ringFar }, { 0, -ringFar }, { ringHalf, -ringFar }, { ringFar, -ringFar },
			{ ringFar, -ringHalf }
		};
		for(int i = 0; i < 16; i++)
			outside.push_back(OldPath::Point(cg.x + ring[i][0], cg.y + ring[i][1], cg.z));
	}

	bool Reach(double x, double y, int deck, const OldPath::Point& to, bool crew) const
	{
		OldPath::Route route;
		return path.SpacecraftRoute(route, OldPath::Point(x, y, deck * DeckHeight + 0.1), to, rooms, false, false, doors, crew,
				false, false);
	}

	bool ReachOutside(double x, double y, int deck, bool crew) const
	{
		for(size_t i = 0; i < outside.size(); i++)
			if(Reach(x, y, deck, outside[i], crew))
				return true;
		return false;
	}
};

}

QString PathReportHtml(const Design& design, const std::function<bool(const QString&, int, int)>& progress, PathReportCounts* counts)
{
	PathReportCounts own;
	PathReportCounts& tally = counts ? *counts : own;
	tally = PathReportCounts();

	QHash<int, const Room*> rooms;
	int minDeck = 0;
	bool hasDeck = false;
	for(size_t i = 0; i < design.rooms.size(); i++)
	{
		const Room& r = design.rooms[i];
		if(!rooms.contains(r.id))
			rooms.insert(r.id, &r);
		if(!(r.type >= 22 && r.type <= 24) && r.points.size() >= 3)
		{
			minDeck = hasDeck ? std::min(minDeck, r.deck) : r.deck;
			hasDeck = true;
		}
	}

	QVector<const Detail*> berths, outsideStations;
	for(size_t i = 0; i < design.details.size(); i++)
	{
		const Detail& d = design.details[i];
		if(!rooms.contains(d.room))
			continue;
		if(IsBerth(d.type))
			berths.append(&d);
		else if(IsOutsideStation(d.type))
			outsideStations.append(&d);
	}

	QString html;
	if(progress && !progress(QString("Checking paths..."), 0, 1))
		return Canceled(html);
	const Router router(design);

	qint64 matching[5] = { 0, 0, 0, 0, 0 };
	for(int rule = RuleCrew; rule <= RulePublic; rule++)
		for(int b = 0; b < berths.size(); b++)
			if(ChecksBerth((TARGETRULE)rule, rooms.value(berths[b]->room)->type))
				matching[rule]++;
	qint64 work = berths.size() + outsideStations.size();
	for(size_t i = 0; i < design.details.size(); i++)
		work += matching[Rule(design.details[i].type)];
	const int maximum = (int)std::max<qint64>(1, std::min<qint64>(work, 0x7fffffff));
	qint64 done = 0;
	auto tick = [&](const QString& label) -> bool
	{
		const bool go = !progress || progress(label, (int)std::min<qint64>(done, maximum), maximum);
		done++;
		return go;
	};

	html += QString("<b>Paths to Outside from Berths</b><br>");
	bool warned = false;
	for(int b = 0; b < berths.size(); b++)
	{
		if(!tick(QString("Finding paths to the outside from all berths...")))
			return Canceled(html);
		const Detail& d = *berths[b];
		const Room* room = rooms.value(d.room);
		const bool crew = room->type != 19;
		const double x = d.x * GridUnit, y = d.y * GridUnit;
		if(router.ReachOutside(x, y, d.deck, crew))
			continue;
		const QString place = Place(room, room->deck, minDeck);
		if(!crew && router.ReachOutside(x, y, d.deck, true))
			Warning(html, tally, QString("Path from berth in %1 to the outside is passable by crew but not by passengers.").arg(place));
		else
			Warning(html, tally, QString("No path from berth in %1 to the outside.").arg(place));
		warned = true;
	}
	FinishSection(html, warned);

	html += QString("<b>Paths to Outside from Stations and Turrets</b><br>");
	warned = false;
	for(int s = 0; s < outsideStations.size(); s++)
	{
		if(!tick(QString("Finding paths to the outside from all stations and turrets...")))
			return Canceled(html);
		const Detail& d = *outsideStations[s];
		double x, y;
		if(IsPublicOutsideStation(d.type) && d.type < 74)
		{
			x = d.x * GridUnit;
			y = d.y * GridUnit;
		}
		else
			SeatPoint(d, x, y);
		const int deck = SeatDeck(d);
		const bool publicAccess = router.ReachOutside(x, y, deck, false);
		const bool crewAccess = publicAccess || router.ReachOutside(x, y, deck, true);
		const QString station = Station(d, rooms.value(d.room), minDeck, true);
		if(IsPublicOutsideStation(d.type))
		{
			if(publicAccess)
				continue;
			Warning(html, tally, crewAccess
					? QString("Path from %1 to the outside is passable by crew but not by passengers or guests.").arg(station)
					: QString("No path from %1 to the outside.").arg(station));
		}
		else if(publicAccess)
			Warning(html, tally, QString("%1 is accessible to passengers or guests from a hull entrance.").arg(station), true);
		else if(!crewAccess)
			Warning(html, tally, QString("No path from %1 to the outside.").arg(station));
		else
			continue;
		warned = true;
	}
	FinishSection(html, warned);

	html += QString("<b>Paths to Stations and Turrets from Berths</b><br>");
	warned = false;
	for(size_t i = 0; i < design.details.size(); i++)
	{
		const Detail& d = design.details[i];
		const TARGETRULE rule = Rule(d.type);
		if(rule == RuleNone)
			continue;
		double tx, ty;
		if(rule == RuleTroop || d.type == 74 || d.type == 75)
			SeatPoint(d, tx, ty);
		else
		{
			tx = d.x * GridUnit;
			ty = d.y * GridUnit;
		}
		const OldPath::Point target(tx, ty, SeatDeck(d) * DeckHeight + 0.1);
		const QString station = Station(d, rooms.value(d.room), minDeck, false);
		for(int b = 0; b < berths.size(); b++)
		{
			const Detail& berth = *berths[b];
			const Room* from = rooms.value(berth.room);
			if(!ChecksBerth(rule, from->type))
				continue;
			if(!tick(QString("Finding paths to stations and turrets from berths...")))
				return Canceled(html);
			const bool passenger = from->type == 19;
			const double x = berth.x * GridUnit, y = berth.y * GridUnit;
			const bool reachable = router.Reach(x, y, berth.deck, target, !passenger);
			const QString place = Place(from, from->deck, minDeck);
			if(rule != RulePublic && passenger)
			{
				if(!reachable)
					continue;
				Warning(html, tally, QString("%1 is accessible to passenger berth in %2.").arg(station).arg(place), true);
			}
			else
			{
				if(reachable)
					continue;
				const bool crewAccess = passenger && router.Reach(x, y, berth.deck, target, true);
				Warning(html, tally, crewAccess
						? QString("Path from berth in %1 to %2 is passable by crew but not by passengers or guests.").arg(place).arg(station)
						: QString("No path from berth in %1 to %2.").arg(place).arg(station));
			}
			warned = true;
		}
	}
	FinishSection(html, warned);
	if(progress && !progress(QString("Checking paths..."), maximum, maximum))
		return Canceled(html);
	tally.complete = true;
	return html;
}

}
