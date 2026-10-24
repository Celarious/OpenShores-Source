#include "oldshipconverter.h"

#include <cmath>

namespace OldShip
{

namespace
{

AuPoint OnOldFloor(const AuPoint& at, const AuPoint& offset)
{
	const AuPoint old = at + offset;
	return AuPoint(old.x, old.y, floor(old.z / DeckHeight + 1e-6) * DeckHeight);
}

}

QList<Problem> Problems(const Design& ship, const ConvertInfo& info)
{
	QList<Problem> all;
	if(!info.rooms.isEmpty())
	{
		bool noWayIn = false;
		const QStringList unreached = RoomsNobodyReaches(ship, info, noWayIn);
		QSet<QString> shut;
		QSet<int> decksSaid;
		for(int i = 0; i < info.rooms.size(); i++)
		{
			const ConvertInfo::RoomInfo& r = info.rooms[i];
			const QString label = QString("%1 (deck %2)").arg(r.name).arg(r.deck);
			if(!unreached.contains(label))
				continue;
			shut.insert(r.name);
			Problem p;
			p.kind = noWayIn ? Problem::NoWayIn : Problem::RoomUnreached;
			p.line = label;
			if(!noWayIn)
				p.text = QString("%1 cannot be reached").arg(r.name);
			else if(!decksSaid.contains(r.deck))
				p.text = QString("Nobody can get in from outside");
			decksSaid.insert(r.deck);
			p.deck = r.deck;
			p.room = r.name;
			p.at = AuPoint(r.centre.x + info.offset.x, r.centre.y + info.offset.y, r.deck * DeckHeight);
			if(r.room >= 0 && r.room < (int)ship.rooms.size())
				p.outline = OutlineOnFloor(ship.rooms[r.room]);
			all.append(p);
		}
		if(!noWayIn)
			for(int pass = 0; pass < 2; pass++)
			{
				const QList<ConvertInfo::StationInfo>& list = pass ? info.stations : info.berths;
				for(int i = 0; i < list.size(); i++)
				{
					if(!shut.contains(list[i].room))
						continue;
					Problem p;
					p.kind = Problem::InUnreachedRoom;
					QString what = list[i].what;
					if(!what.isEmpty())
						what[0] = what[0].toUpper();
					p.text = QString("%1 in a room nobody can reach").arg(what);
					p.at = OnOldFloor(list[i].at, info.offset);
					p.deck = (int)floor(p.at.z / DeckHeight + 0.5);
					p.line = QString("%1 in %2 (deck %3), which cannot be reached").arg(what).arg(list[i].room).arg(p.deck);
					p.detailId = list[i].detailId;
					p.detailType = list[i].detailType;
					p.room = list[i].room;
					all.append(p);
				}
			}
	}
	all += info.problems;
	return all;
}

int FindProblem(const QList<Problem>* problems, Problem::KIND kind, const QString& line)
{
	if(problems)
		for(int i = 0; i < problems->size(); i++)
			if((*problems)[i].kind == kind && (line.isEmpty() || (*problems)[i].line == line))
				return i;
	return -1;
}

QString ProblemLink(int index, const QString& text, const char* rgb)
{
	if(index < 0)
		return text.toHtmlEscaped();
	return QString("<a href=\"problem:%1\" style=\"color: rgb(%2);\">%3</a>").arg(index).arg(QString(rgb)).arg(text.toHtmlEscaped());
}

}
