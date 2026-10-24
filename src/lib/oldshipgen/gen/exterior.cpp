#include "model.h"

namespace OldShip
{

void Builder::CreateModels(TriList& exterior, TriList& interior) const
{
	RenderExterior(exterior, false);
	RenderInterior(interior);
	RenderExterior(interior, true);
}

void Builder::RenderExterior(TriList& out, bool inside) const
{
	FloorBuffer fb;
	std::vector<Outline> outlines;

	for(int deck = m.minDeck; deck <= m.maxDeck + 1; deck++)
	{
		const double z = deck * DeckHeight;
		for(size_t i = 0; i < m.rooms.size(); i++)
			if(m.rooms[i].deck == deck)
				fb.FillFloor(m.rooms[i]);

		int minX = FloorBuffer::Size, maxX = 0, minY = FloorBuffer::Size, maxY = 0;
		RenderExteriorTops(out, ST_HullTop, WT_Plate, z, minX, maxX, minY, maxY, fb, inside);
		RenderExteriorTops(out, ST_HullTopBottomWindow, WT_Window, z, minX, maxX, minY, maxY, fb, inside);
		RenderExteriorTops(out, ST_Turret, WT_Turret, z, minX, maxX, minY, maxY, fb, inside);
		RenderExteriorTops(out, ST_HullHatchHalfOpen, WT_Door, z, minX, maxX, minY, maxY, fb, inside);
		if(!inside)
			RenderExteriorBottoms(out, ST_HullBottom, WT_Plate, z, minX, maxX, minY, maxY, fb, false);
		RenderExteriorBottoms(out, ST_HullTopBottomWindow, WT_Window, z, minX, maxX, minY, maxY, fb, inside);
		RenderExteriorBottoms(out, ST_Turret, WT_Turret, z, minX, maxX, minY, maxY, fb, inside);
		RenderExteriorBottoms(out, ST_HullHatchHalfOpen, WT_Door, z, minX, maxX, minY, maxY, fb, inside);

		if(deck > m.maxDeck || maxX >= FloorBuffer::Size || maxY >= FloorBuffer::Size)
			continue;
		maxX += 4;
		maxY += 4;

		for(int y = minY; y <= maxY; y++)
			for(int x = minX; x <= maxX; x++)
				if(fb.At(x, y) & FloorBuffer::Below)
					fb.At(x, y) = 0;

		std::vector<GridPoint> lattice;
		std::vector<uint16_t> segs;
		while(fb.ChaseBoundary(lattice, segs))
		{
			Outline o;
			for(size_t i = 0; i < lattice.size(); i++)
				o.points.push_back(DeckCoord(lattice[i].x, lattice[i].y, z));
			o.segs = segs;
			o.ids.assign(o.points.size(), 0);

			for(size_t k = 0; k < m.details.size() && !o.points.empty(); k++)
			{
				const Detail& d = m.details[k];
				const bool turret = DetailType::IsWallTurret(d.type);
				if(d.deck != deck || !(d.type == 3 || d.type == 9 || d.type == 50 || turret))
					continue;
				const Vec3 loc = d.Loc();
				Vec3 last = o.points.back();
				uint16_t lastSeg = o.segs.back();
				uint32_t lastId = o.ids.back();
				for(size_t i = 0; i < o.points.size(); i++)
				{
					const Vec3 pt = o.points[i];
					if(!FuzzyEquals(loc, last) && !FuzzyEquals(loc, pt) && IsOnSegment(loc, last, pt))
					{
						const Vec3 along = Unit(pt - last);
						const double dist = Distance(loc, last);
						Vec3 a, b;
						if(turret)
						{
							a = last + along * (dist - GridUnit);
							b = last + along * (dist + GridUnit);
						}
						else if(d.type != 3 && d.type != 9)
						{
							a = last + along * (dist - 2.5);
							b = last + along * (dist + 2.5);
						}
						else
						{
							a = last + along;
							b = pt - along;
						}
						o.points.insert(o.points.begin() + i, b);
						o.segs.insert(o.segs.begin() + i, lastSeg);
						o.ids.insert(o.ids.begin() + i, lastId);
						o.points.insert(o.points.begin() + i, a);
						if(turret)
						{
							o.segs.insert(o.segs.begin() + i, (uint16_t)0x1f00);
							o.ids.insert(o.ids.begin() + i, 0u);
						}
						else if(d.type == 3)
						{
							o.segs.insert(o.segs.begin() + i, (uint16_t)0x1c00);
							o.ids.insert(o.ids.begin() + i, (uint32_t)d.id);
						}
						else if(d.type == 9)
						{
							o.segs.insert(o.segs.begin() + i, (uint16_t)0x1d00);
							o.ids.insert(o.ids.begin() + i, 0u);
						}
						else
						{
							o.segs.insert(o.segs.begin() + i, (uint16_t)0x1e00);
							o.ids.insert(o.ids.begin() + i, (uint32_t)d.id);
						}
						break;
					}
					last = pt;
					lastSeg = o.segs[i];
					lastId = o.ids[i];
				}
			}

			if(!o.points.empty())
			{
				while(o.points.size() > 1 && FuzzyEquals(o.points.front(), o.points.back()))
				{
					o.points.pop_back();
					o.segs.pop_back();
					o.ids.pop_back();
				}
				for(size_t i = 0; i + 1 < o.points.size(); )
				{
					if(FuzzyEquals(o.points[i], o.points[i + 1]))
					{
						o.points.erase(o.points.begin() + i);
						o.segs.erase(o.segs.begin() + i);
						o.ids.erase(o.ids.begin() + i);
						i = 0;
					}
					else
						i++;
				}
				for(int n = (int)o.segs.size() - 1; n > 0 && ((o.segs.back() ^ o.segs.front()) & 0x1f00) != 0; n--)
				{
					o.points.push_back(o.points.front()); o.points.erase(o.points.begin());
					o.segs.push_back(o.segs.front()); o.segs.erase(o.segs.begin());
					o.ids.push_back(o.ids.front()); o.ids.erase(o.ids.begin());
				}
			}
			outlines.push_back(o);
		}

		for(int y = minY; y <= maxY; y++)
			for(int x = minX; x <= maxX; x++)
				if(fb.At(x, y) != 0)
					fb.At(x, y) = (uint16_t)((fb.At(x, y) & ~FloorBuffer::OpenBelow) | FloorBuffer::Below);
	}

	const bool anyDetails = !m.details.empty();
	if(anyDetails)
		RenderExteriorWalls(out, ST_Turret, WT_Turret, outlines, inside);
	RenderExteriorWalls(out, ST_HullWall, WT_Plate, outlines, inside);
	RenderExteriorWalls(out, ST_HullWallWindow, WT_Window, outlines, inside);
	if(anyDetails)
	{
		RenderExteriorWalls(out, ST_HullDoorHalfOpen, WT_Door, outlines, inside);
		RenderExteriorWalls(out, ST_LargeHullDoorHalfOpen, WT_LargeDoor, outlines, inside);
	}
}

}
