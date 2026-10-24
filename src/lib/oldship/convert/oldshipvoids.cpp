#include "oldshipconverter.h"

#include <cmath>

namespace OldShip
{

void Converter::BuildRoomVoids()
{
	int walkable = 0;
	for(int r = 0; r < m_Rooms.size(); r++)
	{
		const RoomData& rd = m_Rooms[r];
		ConvertInfo::RoomInfo ri;
		ri.room = r;
		ri.name = rd.name;
		ri.deck = rd.deck;
		ri.type = rd.type;
		ri.walkable = !rd.inner.isEmpty();
		ri.openBelow = rd.openBelow;
		ri.centre = At(rd.centre, rd.floorZ + VoidFloor);
		if(ri.walkable)
		{
			double bottom = rd.floorZ + VoidFloor;
			if(rd.openBelow)
			{
				bottom = rd.floorZ - DeckHeight + VoidCeiling - 0.5;
				ri.centre = At(rd.centre, rd.floorZ - DeckHeight + VoidFloor);
			}
			QVector<AuPoint> low, high;
			for(int k = 0; k < rd.inner.size(); k++)
			{
				low.append(At(rd.inner[k], bottom));
				high.append(At(rd.inner[k], rd.floorZ + VoidCeiling));
			}
			AuScFaceList faces;
			AddPrism(faces, low, high);
			const quint16 id = AddRoomVoid(faces, r, 0, QString("room void of %1").arg(rd.name));
			if(id)
			{
				ri.voids.append(id);
				walkable++;
			}
			if(!rd.openBelow)
			{
				double x0 = 1e30, x1 = -1e30, y0 = 1e30, y1 = -1e30;
				for(int k = 0; k < rd.inner.size(); k++)
				{
					x0 = qMin(x0, rd.inner[k].x); x1 = qMax(x1, rd.inner[k].x);
					y0 = qMin(y0, rd.inner[k].y); y1 = qMax(y1, rd.inner[k].y);
				}
				const double step = GridUnit / 4;
				for(double x = rd.inner[0].x + step * floor((x0 - rd.inner[0].x) / step); x <= x1; x += step)
					for(double y = rd.inner[0].y + step * floor((y0 - rd.inner[0].y) / step); y <= y1; y += step)
						if(Contains(rd.inner, P2(x + 0.0137, y + 0.0071)))
							ri.floorProbes.append(At(x + 0.0137, y + 0.0071, rd.floorZ + VoidFloor));
			}
			if(rd.type == 11)
				Log(QString("%1 (deck %2) is a gravity well: its void has zero gravity%3.").arg(rd.name).arg(rd.deck)
						.arg(rd.openBelow ? QString(" and is open to the room below") : QString()));
			else if(rd.openBelow)
				Log(QString("%1 (deck %2) is open to the deck below: its void has no floor of its own.").arg(rd.name).arg(rd.deck));
		}
		m_Info.rooms.append(ri);
	}
	Log(QString("Room voids: %1 of %2 rooms can be entered (fuel cells and solid hull cannot).").arg(walkable).arg(m_Rooms.size()));
}

void Converter::BuildWallProbes()
{
	for(int r = 0; r < m_Rooms.size(); r++)
	{
		const RoomData& rd = m_Rooms[r];
		if(rd.inner.isEmpty())
			continue;
		for(int k = 0; k < rd.poly.size(); k++)
		{
			const P2& a = rd.poly[k];
			const P2& b = rd.poly[(k + 1) % rd.poly.size()];
			const double length = (b - a).Length();
			const P2 u = (b - a).Unit();
			for(double s = 0.0; s <= length; s += 0.7)
			{
				const P2 p = a + u * s;
				bool open = false;
				for(int o = 0; o < m_OpenSpans.size() && !open; o++)
				{
					const Span& sp = m_OpenSpans[o];
					open = sp.deck == rd.deck && qAbs((p - sp.mid).Dot(sp.u)) <= sp.half + 0.05 && qAbs((p - sp.mid).Cross(sp.u)) < 0.3;
				}
				if(open)
					continue;
				m_Info.wallProbes.append(At(p, rd.floorZ + 1.0));
				m_Info.wallProbes.append(At(p, rd.floorZ + 5.0));
			}
		}
	}
}

}
