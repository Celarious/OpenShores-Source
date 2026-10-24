#include "oldbuilding.h"

#include <algorithm>
#include <cmath>

namespace OldBuilding
{

void SetTopTexture(int buildingTex);

using OldShip::Outline;
using OldShip::TriList;
using namespace OldShip::DetailType;

namespace
{

bool ShipOnly(int t)
{
	return t == 10 || t == 14 || t == 16 || t == 18 || t == 23 || t == 24 || t == 32;
}

FloorBuffer Shown(const FloorBuffer& fb, bool glassHere)
{
	FloorBuffer r = fb;
	for(int x = 0; x < FloorBuffer::Size; x++)
		for(int y = 0; y < FloorBuffer::Size; y++)
		{
			uint16_t& c = r.At(x, y);
			const int type = (c >> 8) & 0x1f;
			if(type == 14)
				c = (uint16_t)((c & ~0x1f00) | 0x1e00);
			else if(type == 2 && !glassHere)
				c = (uint16_t)((c & ~0x1f00) | 0x1f00);
		}
	return r;
}

void OtherDiagonal(OldShip::TriList& tris, size_t from)
{
	for(size_t i = from; i + 1 < tris.size(); i++)
	{
		OldShip::Tri& a = tris[i];
		OldShip::Tri& b = tris[i + 1];
		const Vec3& p0 = a.v[0].pos; const Vec3& p1 = a.v[1].pos; const Vec3& p2 = a.v[2].pos;
		const bool turned = OldShip::FuzzyEquals(b.v[0].pos, p1) && OldShip::FuzzyEquals(b.v[1].pos, p0);
		const Vec3& p3 = turned ? b.v[2].pos : b.v[1].pos;
		if(!turned && (!OldShip::FuzzyEquals(b.v[0].pos, p2) || !OldShip::FuzzyEquals(b.v[2].pos, p0)))
			continue;
		const double side = 4 * OldShip::LatticeUnit;
		const double minX = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x)), maxX = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
		const double minY = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y)), maxY = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));
		if(fabs(maxX - minX - side) > 1e-6 || fabs(maxY - minY - side) > 1e-6)
			continue;
		bool corners = true;
		const Vec3* q[4] = { &p0, &p1, &p2, &p3 };
		for(int k = 0; k < 4; k++)
			if((fabs(q[k]->x - minX) > 1e-6 && fabs(q[k]->x - maxX) > 1e-6) || (fabs(q[k]->y - minY) > 1e-6 && fabs(q[k]->y - maxY) > 1e-6))
				corners = false;
		if(!corners)
			continue;
		if(turned)
		{
			if(fabs(p0.x - p1.x) < 1e-6 || fabs(p0.y - p1.y) < 1e-6)
				continue;
			const OldShip::Vertex x0 = a.v[0], x1 = a.v[1], x2 = a.v[2], x3 = b.v[2];
			a.v[0] = x3; a.v[1] = x1; a.v[2] = x2;
			b.v[0] = x2; b.v[1] = x0; b.v[2] = x3;
			i++;
			continue;
		}
		if(fabs(p0.x - p2.x) < 1e-6 || fabs(p0.y - p2.y) < 1e-6)
			continue;
		const OldShip::Vertex x0 = a.v[0], x1 = a.v[1], x2 = a.v[2], x3 = b.v[1];
		a.v[0] = x3; a.v[1] = x1; a.v[2] = x2;
		b.v[0] = x1; b.v[1] = x3; b.v[2] = x0;
		i++;
	}
}

void TurretFaces(OldShip::TriList& tris, size_t from, size_t to, bool inside)
{
	for(size_t block = from; block + 16 <= to; block += 16)
		for(int f = 1; f < 16; f += 4)
		{
			OldShip::Tri& t = tris[block + f];
			const Vec3 p0 = t.v[0].pos, p1 = t.v[1].pos, p2 = t.v[2].pos;
			if(!inside)
			{
				t.v[0].pos = p1; t.v[1].pos = p2; t.v[2].pos = p0;
			}
			else
			{
				t.v[0].pos = p2; t.v[1].pos = p0; t.v[2].pos = p1;
			}
		}
}

BTri GroundHole(const Vec3& a, const Vec3& b, const Vec3& c)
{
	const Vec3 up(0.0, 0.0, 1.0);
	BTri t;
	t.v[0] = BVertex(a, up, 0.0, 0.0);
	t.v[1] = BVertex(b, up, 0.0, 0.0);
	t.v[2] = BVertex(c, up, 0.0, 0.0);
	t.window = 4;
	return t;
}

void HatchCovers(OldShip::TriList& tris, size_t from, size_t to, bool flipped, BTriList* ground)
{
	for(size_t i = from; i + 1 < to; i++)
	{
		OldShip::Tri& t0 = tris[i];
		OldShip::Tri& t1 = tris[i + 1];
		float uMin = t0.v[0].u, uMax = uMin, vMin = t0.v[0].v, vMax = vMin;
		for(int k = 0; k < 6; k++)
		{
			const OldShip::Vertex& x = k < 3 ? t0.v[k] : t1.v[k - 3];
			uMin = std::min(uMin, x.u); uMax = std::max(uMax, x.u);
			vMin = std::min(vMin, x.v); vMax = std::max(vMax, x.v);
		}
		if(uMin == uMax || vMin == vMax)
			continue;
		const OldShip::Vertex* corner[4] = { 0, 0, 0, 0 };
		bool ok = true;
		for(int k = 0; k < 6 && ok; k++)
		{
			const OldShip::Vertex& x = k < 3 ? t0.v[k] : t1.v[k - 3];
			if((x.u != uMin && x.u != uMax) || (x.v != vMin && x.v != vMax))
				ok = false;
			else
				corner[(x.u == uMax ? 1 : 0) + (x.v == vMax ? 2 : 0)] = &x;
		}
		if(!ok || !corner[0] || !corner[1] || !corner[2] || !corner[3])
			continue;
		OldShip::Vertex c = *corner[0], b = *corner[1], d = *corner[2], a = *corner[3];
		const Vec3 e1 = t0.v[1].pos - t0.v[0].pos, e2 = t0.v[2].pos - t0.v[0].pos;
		const bool clockwise = e1.x * e2.y - e1.y * e2.x < 0.0;
		const bool upper = clockwise != flipped;
		const Vec3 n(0.0, 0.0, upper ? 1.0 : -1.0);
		c.normal = b.normal = d.normal = a.normal = n;
		if(clockwise)
		{
			t0.v[0] = c; t0.v[1] = d; t0.v[2] = b;
			t1.v[0] = b; t1.v[1] = d; t1.v[2] = a;
		}
		else
		{
			t0.v[0] = c; t0.v[1] = b; t0.v[2] = d;
			t1.v[0] = b; t1.v[1] = a; t1.v[2] = d;
		}
		if(ground && upper)
		{
			ground->push_back(GroundHole(c.pos, d.pos, b.pos));
			ground->push_back(GroundHole(b.pos, d.pos, a.pos));
		}
		i++;
	}
}

void TurretGround(const OldShip::TriList& tris, size_t from, size_t to, bool inside, BTriList& ground)
{
	static const int Cut[16][3] =
	{
		{ 5, 6, 7 }, { 4, 5, 7 }, { 7, 0, 4 }, { 4, 0, 1 }, { 1, 3, 4 }, { 1, 2, 3 },
		{ 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
	};
	const int second = inside ? 2 : 1, third = inside ? 1 : 2;
	for(size_t block = from; block + 16 <= to; block += 16)
	{
		Vec3 p[8];
		p[1] = tris[block + 2].v[second].pos; p[0] = tris[block + 2].v[third].pos;
		p[7] = tris[block + 3].v[third].pos;
		p[3] = tris[block + 6].v[second].pos; p[2] = tris[block + 6].v[third].pos;
		p[5] = tris[block + 10].v[second].pos; p[4] = tris[block + 10].v[third].pos;
		p[6] = tris[block + 14].v[third].pos;
		for(int k = 0; k < 16; k++)
			ground.push_back(GroundHole(p[Cut[k][0]], p[Cut[k][1]], p[Cut[k][2]]));
	}
}

void GlassGround(const OldShip::TriList& tris, size_t from, size_t to, bool inside, BTriList& ground)
{
	const double cell = 4 * OldShip::LatticeUnit, half = 2 * OldShip::LatticeUnit;
	for(size_t i = from; i < to; i++)
	{
		const Vec3& p0 = tris[i].v[0].pos; const Vec3& p1 = tris[i].v[1].pos; const Vec3& p2 = tris[i].v[2].pos;
		const double w = std::max(p0.x, std::max(p1.x, p2.x)) - std::min(p0.x, std::min(p1.x, p2.x));
		const double h = std::max(p0.y, std::max(p1.y, p2.y)) - std::min(p0.y, std::min(p1.y, p2.y));
		const bool whole = fabs(w - cell) < 1e-6 && fabs(h - cell) < 1e-6;
		const bool quarter = (fabs(w - cell) < 1e-6 && fabs(h - half) < 1e-6) || (fabs(w - half) < 1e-6 && fabs(h - cell) < 1e-6);
		if(!inside)
		{
			if(whole || quarter)
				ground.push_back(GroundHole(p0, p1, p2));
			continue;
		}
		if(!quarter)
			continue;
		const double mx = (p1.x + p2.x) * 0.5 - p0.x, my = (p1.y + p2.y) * 0.5 - p0.y;
		const bool west = mx < -1e-6 && fabs(my) < 1e-6, south = my < -1e-6 && fabs(mx) < 1e-6;
		if(west || south)
			ground.push_back(GroundHole(p0, p2, p1));
	}
}

void Prepend(BTriList& out, const BTriList& ground)
{
	out.insert(out.begin(), ground.rbegin(), ground.rend());
}

BTri Tri3(const BVertex& a, const BVertex& b, const BVertex& c, unsigned char window = 0)
{
	BTri t;
	t.v[0] = a; t.v[1] = b; t.v[2] = c;
	t.window = window;
	return t;
}

}

Builder::Builder(const Design& d, int materialCommodity, int industryId, int lowest, int highest)
	: m(d), material(materialCommodity), industry(industryId), lo(lowest), hi(highest), candleLit(IsCandleLit(industryId)), tag(false)
{
	ship.id = d.id;
	ship.armor = 99;
	ship.minDeck = lo;
	ship.maxDeck = hi;
	for(size_t i = 0; i < d.rooms.size(); i++)
	{
		const Room& r = d.rooms[i];
		if(r.IsRoof())
			continue;
		OldShip::Room s;
		s.design = r.design; s.id = r.id; s.deck = r.deck; s.type = r.type; s.style = r.number; s.points = r.points;
		ship.rooms.push_back(s);
	}
	for(size_t i = 0; i < d.details.size(); i++)
		if(!ShipOnly(d.details[i].type))
			ship.details.push_back(d.details[i]);
}

void Builder::Take(BTriList& out, const TriList& tris, size_t from, unsigned char window) const
{
	for(size_t i = from; i < tris.size(); i++)
	{
		BTri t;
		for(int k = 0; k < 3; k++)
		{
			const OldShip::Vertex& v = tris[i].v[k];
			t.v[k] = BVertex(v.pos, v.normal, (double)v.u, (double)v.v);
		}
		t.window = window;
		t.door = tris[i].v[0].ints[0];
		out.push_back(t);
	}
}

int Builder::DoorTag(uint32_t id) const
{
	if(tag && id != 0)
		for(size_t i = 0; i < openings.size(); i++)
			if((uint32_t)openings[i].id == id)
				return (int)i + 10;
	return 0;
}

void Builder::CreateModels(BTriList& exterior, BTriList& interior) const
{
	RenderExterior(exterior, false);
	RenderInterior(interior);
	RenderExterior(interior, true);
}

void Builder::RenderExterior(BTriList& out, bool inside, Sections* sections) const
{
	using namespace OldShip;
	BuildingTextures textures;
	OldShip::Builder sb(ship);
	sb.SetOpenings(openings, tag ? (int)openings.size() : 0);
	const size_t before = out.size();
	FloorBuffer fb;
	std::vector<Outline> outlines;
	std::vector<const Room*> roofs;
	TriList tmp;
	BTriList ground;

	for(int deck = lo; deck <= hi + 1; deck++)
	{
		const double z = deck * DeckHeight;
		for(size_t i = 0; i < m.rooms.size(); i++)
		{
			const Room& r = m.rooms[i];
			if(r.deck != deck)
				continue;
			if(r.IsRoof())
				roofs.push_back(&r);
			else if(deck <= hi)
				fb.FillFloor(r.points, r.type, r.number);
		}

		FloorBuffer shown = Shown(fb, deck >= 0);
		FloorBuffer shownUnder = Shown(fb, deck > 0);
		int minX = FloorBuffer::Size, maxX = 0, minY = FloorBuffer::Size, maxY = 0;
		const size_t from = tmp.size();
		SetTopTexture(inside ? BT_IntCeiling : BT_HullTop);
		sb.RenderExteriorTops(tmp, ST_HullTop, WT_Plate, z, minX, maxX, minY, maxY, shown, inside);
		SetTopTexture(BT_HullTop);
		size_t mark = tmp.size();
		sb.RenderExteriorTops(tmp, ST_HullTopBottomWindow, WT_Window, z, minX, maxX, minY, maxY, shown, inside);
		if(deck == 0)
			GlassGround(tmp, mark, tmp.size(), inside, ground);
		mark = tmp.size();
		sb.RenderExteriorTops(tmp, ST_Turret, WT_Turret, z, minX, maxX, minY, maxY, shown, inside);
		if(deck == 0)
			TurretGround(tmp, mark, tmp.size(), inside, ground);
		TurretFaces(tmp, mark, tmp.size(), inside);
		mark = tmp.size();
		sb.RenderExteriorTops(tmp, ST_HullHatchHalfOpen, WT_Door, z, minX, maxX, minY, maxY, shown, inside);
		HatchCovers(tmp, mark, tmp.size(), inside, deck == 0 && !inside ? &ground : 0);
		mark = tmp.size();
		if(!inside)
			sb.RenderExteriorBottoms(tmp, ST_HullBottom, WT_Plate, z, minX, maxX, minY, maxY, shownUnder, false);
		sb.RenderExteriorBottoms(tmp, ST_HullTopBottomWindow, WT_Window, z, minX, maxX, minY, maxY, shownUnder, inside);
		OtherDiagonal(tmp, mark);
		mark = tmp.size();
		sb.RenderExteriorBottoms(tmp, ST_Turret, WT_Turret, z, minX, maxX, minY, maxY, shownUnder, inside);
		TurretFaces(tmp, mark, tmp.size(), inside);
		mark = tmp.size();
		sb.RenderExteriorBottoms(tmp, ST_HullHatchHalfOpen, WT_Door, z, minX, maxX, minY, maxY, shownUnder, inside);
		HatchCovers(tmp, mark, tmp.size(), inside, 0);
		Take(out, tmp, from, Window());

		if(deck > hi || maxX >= FloorBuffer::Size || maxY >= FloorBuffer::Size)
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
				const bool turret = d.type == 0x15 || (d.type >= 0x22 && d.type <= 0x28);
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
						uint16_t seg = 0x1e00;
						uint32_t id = (uint32_t)d.id;
						if(turret) { seg = 0x1f00; id = 0; }
						else if(d.type == 3) seg = 0x1c00;
						else if(d.type == 9) { seg = 0x1d00; id = 0; }
						o.segs.insert(o.segs.begin() + i, seg);
						o.ids.insert(o.ids.begin() + i, id);
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

	RenderExteriorWalls(out, outlines, inside);
	Prepend(out, ground);
	if(sections)
	{
		sections->ground = ground.size();
		sections->shell = out.size() - before - ground.size();
	}
	if(inside)
		return;
	size_t mark = out.size();
	for(size_t i = 0; i < roofs.size(); i++)
		RenderRoof(out, *roofs[i]);
	if(sections)
		sections->roofs = out.size() - mark;
	mark = out.size();
	for(size_t i = 0; i < m.details.size(); i++)
	{
		const Detail& d = m.details[i];
		if(d.type == 100)
			CreateFlagPole(out, d.Loc());
		else if(d.type == 142)
			CreateStreetLight(out, d.Loc());
	}
	if(sections)
		sections->poles = out.size() - mark;
}

void Builder::RenderInterior(BTriList& out) const
{
	BTriList ground;
	RenderInteriorDecks(out, ground);
	for(size_t i = 0; i < m.details.size(); i++)
		RenderFitting(out, m.details[i]);
	Prepend(out, ground);
}

void Builder::RenderInteriorDecks(BTriList& out, BTriList& ground) const
{
	using namespace OldShip;
	BuildingTextures textures;
	OldShip::Builder sb(ship);
	sb.SetOpenings(openings, tag ? (int)openings.size() : 0);
	FloorBuffer fb;
	TriList tmp;

	for(int deck = lo; deck <= hi + 1; deck++)
	{
		const double z = deck * DeckHeight;
		if(deck <= hi)
			for(size_t i = 0; i < m.rooms.size(); i++)
				if(m.rooms[i].deck == deck && !m.rooms[i].IsRoof())
					fb.FillFloor(m.rooms[i].points, m.rooms[i].type, m.rooms[i].number);

		int minX = FloorBuffer::Size, maxX = 0, minY = FloorBuffer::Size, maxY = 0;
		const FloorBuffer plain = Shown(fb, false);
		sb.RenderInteriorCeilings(tmp, z, minX, maxX, minY, maxY, plain);
		OtherDiagonal(tmp, 0);
		const FloorBuffer floors = Shown(fb, deck > 0);
		sb.RenderInteriorFloors(tmp, ST_IntFloor, z, false, minX, maxX, minY, maxY, floors);
		Take(out, tmp, 0, 0);
		tmp.clear();
		if(deck == 0)
		{
			for(int y = 0; y < FloorBuffer::Size - 1; y += 4)
				for(int x = 0; x < FloorBuffer::Size - 1; x += 4)
				{
					const bool east = (fb.At(x + 3, y + 2) & FloorBuffer::OpenBelow) != 0, north = (fb.At(x + 2, y + 3) & FloorBuffer::OpenBelow) != 0;
					const bool west = (fb.At(x + 1, y + 2) & FloorBuffer::OpenBelow) != 0, south = (fb.At(x + 2, y + 1) & FloorBuffer::OpenBelow) != 0;
					const Vec3 sw = DeckCoord(x, y, z), nw = DeckCoord(x, y + 4, z), ne = DeckCoord(x + 4, y + 4, z), se = DeckCoord(x + 4, y, z);
					const Vec3 c = DeckCoord(x + 2, y + 2, z);
					if(east && north && west && south)
					{
						ground.push_back(GroundHole(sw, nw, ne));
						ground.push_back(GroundHole(ne, se, sw));
					}
					else
					{
						if(east) ground.push_back(GroundHole(c, ne, se));
						if(north) ground.push_back(GroundHole(c, nw, ne));
						if(west) ground.push_back(GroundHole(c, sw, nw));
						if(south) ground.push_back(GroundHole(c, se, sw));
					}
				}
		}
		sb.RenderInteriorFloors(tmp, ST_HullHatchHalfOpen, z, true, minX, maxX, minY, maxY, floors);
		HatchCovers(tmp, 0, tmp.size(), false, deck == 0 ? &ground : 0);
		Take(out, tmp, 0, Window());
		tmp.clear();

		if(deck > hi || maxX >= FloorBuffer::Size || maxY >= FloorBuffer::Size)
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
			;
		for(size_t i = 0; i < ship.rooms.size(); i++)
		{
			const OldShip::Room& room = ship.rooms[i];
			if(room.deck != deck || room.type == 9 || room.points.empty())
				continue;
			sb.RenderInteriorWalls(tmp, room, fb);
		}
		Take(out, tmp, 0, 0);
		tmp.clear();
		for(int y = minY; y <= maxY; y++)
			for(int x = minX; x <= maxX; x++)
				if(fb.At(x, y) != 0)
					fb.At(x, y) = (uint16_t)((fb.At(x, y) & ~FloorBuffer::OpenBelow) | FloorBuffer::Below);
	}
}

bool Builder::RenderFitting(BTriList& out, const Detail& d) const
{
	using namespace OldShip;
	if(d.deck < lo || d.flag)
		return false;
	BuildingTextures textures;
	OldShip::Builder sb(ship);
	sb.SetOpenings(openings, tag ? (int)openings.size() : 0);
	TriList tmp;
	const int t = d.type;
	if(t == 2 || (t >= 25 && t <= 31))
	{
		if(d.deck <= hi)
			sb.RenderBerthDouble(tmp, d.Loc(), (double)Rotation(t));
	}
	else if(t == 8 || t == 0x33 || t == 0x34 || (t >= 0x4d && t <= 0x61))
	{
		if(d.deck <= hi + 1)
			sb.RenderLadder(tmp, d.Loc(), Rotation(t));
	}
	else if(t == 11)
	{
		if(d.deck <= hi)
			sb.RenderSurgeryUnit(tmp, d.Loc());
	}
	else if(t == 20)
	{
		if(d.deck <= hi)
			sb.RenderTransporterPad(tmp, d.Loc(), (uint16_t)d.room, ship.rooms);
	}
	else if(t >= 65 && t <= 72)
	{
		if(d.deck <= hi)
			sb.RenderBerthSingle(tmp, d.Loc(), (double)Rotation(t));
	}
	else if(t >= 101 && t <= 140 && d.deck < hi)
	{
		const int shape = (t - 101) / 8;
		const bool diagonal = ((t - 101) & 1) != 0;
		const double rot = (double)Rotation(t);
		switch(shape)
		{
		case 0: sb.RenderStairsStraight(tmp, d.Loc(), rot, diagonal); break;
		case 1: sb.RenderStairsTurnLeft(tmp, d.Loc(), rot, diagonal); break;
		case 2: sb.RenderStairsTurnRight(tmp, d.Loc(), rot, diagonal); break;
		case 3: sb.RenderStairsUTurnLeft(tmp, d.Loc(), rot, diagonal); break;
		case 4: sb.RenderStairsUTurnRight(tmp, d.Loc(), rot, diagonal); break;
		}
		if(shape == 0)
			for(size_t k = 7; k < tmp.size(); k += 6)
				for(int j = 0; j < 3; j++)
				{
					Vec3& n = tmp[k].v[j].normal;
					if(n.x == 0.0 && n.y == 1.0 && n.z == 0.0)
						n.y = -1.0;
				}
	}
	if(tmp.empty())
		return false;
	Take(out, tmp, 0, 0);
	return true;
}

void Builder::RenderRoof(BTriList& out, const Room& room) const
{
	using namespace OldShip;
	float ts, tt, tu, tv;
	float bs, bt, bu, bv;
	BuildingTexture(BT_HullTop, ts, tt, tu, tv);
	BuildingTexture(BT_HullBottom, bs, bt, bu, bv);
	const std::vector<Vec3>& p = room.outline;
	const Vec3 down(0.0, 0.0, -1.0);
	const double lift = 0.0104166666667;
	const double tm = (double)((ts + tu) * 0.5f);

	if(room.type == 22 || room.type == 23)
	{
		if(p.size() < 4)
			return;
		const Vec3 l0(p[0].x, p[0].y, p[0].z + lift), l1(p[1].x, p[1].y, p[1].z + lift);
		const Vec3 l2(p[2].x, p[2].y, p[2].z + lift), l3(p[3].x, p[3].y, p[3].z + lift);
		out.push_back(Tri3(BVertex(l1, down, bs, bt), BVertex(l0, down, bs, bv), BVertex(l3, down, bu, bv)));
		out.push_back(Tri3(BVertex(l3, down, bu, bv), BVertex(l2, down, bu, bt), BVertex(l1, down, bs, bt)));

		Vec3 a = (p[1] + p[0]) * (1.0 / 2.0);
		Vec3 b = (p[3] + p[2]) * (1.0 / 2.0);
		if(room.type == 22)
		{
			float ws, wt, wu, wv;
			BuildingTexture(BT_HullWall, ws, wt, wu, wv);
			a.z = a.z + (Distance2D(p[0], p[1]) * 0.5) / 3.0;
			b.z = a.z;
			const Vec3 across = Unit(p[1] - p[0]);
			const Vec3 along = Unit(p[3] - p[0]);
			const Vec3 front = Cross(Vec3(0.0, 0.0, 1.0), across);
			const Vec3 left = Cross(along, Unit(a - p[0]));
			const Vec3 right = Cross(Unit(a - p[1]), along);
			const double wm = (double)((ws + wu) * 0.5f);
			const Vec3 back(-front.x, -front.y, -front.z);
			out.push_back(Tri3(BVertex(p[1], front, ws, wt), BVertex(a, front, wm, wv), BVertex(p[0], front, wu, wt)));
			out.push_back(Tri3(BVertex(p[3], back, ws, wt), BVertex(b, back, wm, wv), BVertex(p[2], back, wu, wt)));
			out.push_back(Tri3(BVertex(p[0], left, ts, tt), BVertex(a, left, ts, tv), BVertex(b, left, tu, tv)));
			out.push_back(Tri3(BVertex(b, left, tu, tv), BVertex(p[3], left, tu, tt), BVertex(p[0], left, ts, tt)));
			out.push_back(Tri3(BVertex(p[2], right, ts, tt), BVertex(b, right, ts, tv), BVertex(a, right, tu, tv)));
			out.push_back(Tri3(BVertex(a, right, tu, tv), BVertex(p[1], right, tu, tt), BVertex(p[2], right, ts, tt)));
			return;
		}

		double half = Distance2D(p[0], p[1]) * 0.5;
		a.z = a.z + half / 3.0;
		b.z = a.z;
		Vec3 c = (a + b) * (1.0 / 2.0);
		const double halfLength = Distance2D(a, b) * 0.5;
		const Vec3 along = Unit(p[3] - p[0]);
		const Vec3 across = Unit(p[1] - p[0]);
		const bool peak = halfLength <= half;
		Vec3 q0, q1, q2, q3;
		if(peak)
		{
			a = b = c;
		}
		else
		{
			const Vec3 step = along * half;
			a = a + step;
			b = b - step;
			q0 = p[0] + step;
			q3 = p[3] - step;
			q1 = p[1] + step;
			q2 = p[2] - step;
		}
		const Vec3 front = Cross(Unit(a - (p[1] + p[0]) * (1.0 / 2.0)), across);
		const Vec3 back = Cross(across, Unit(b - (p[3] + p[2]) * (1.0 / 2.0)));
		const Vec3 left = Cross(along, Unit(c - (p[0] + p[3]) * (1.0 / 2.0)));
		const Vec3 right = Cross(Unit(c - (p[1] + p[2]) * (1.0 / 2.0)), along);
		out.push_back(Tri3(BVertex(p[1], front, ts, tt), BVertex(a, front, tm, tv), BVertex(p[0], front, tu, tt)));
		out.push_back(Tri3(BVertex(p[3], back, ts, tt), BVertex(b, back, tm, tv), BVertex(p[2], back, tu, tt)));
		if(peak)
		{
			out.push_back(Tri3(BVertex(p[0], left, ts, tt), BVertex(c, left, tm, tv), BVertex(p[3], left, tu, tt)));
			out.push_back(Tri3(BVertex(p[2], right, ts, tt), BVertex(c, right, tm, tv), BVertex(p[1], right, tu, tt)));
			return;
		}
		out.push_back(Tri3(BVertex(a, left, tu, tv), BVertex(q0, left, tu, tt), BVertex(p[0], left, ts, tt)));
		out.push_back(Tri3(BVertex(q0, left, ts, tt), BVertex(a, left, ts, tv), BVertex(b, left, tu, tv)));
		out.push_back(Tri3(BVertex(b, left, tu, tv), BVertex(q3, left, tu, tt), BVertex(q0, left, ts, tt)));
		out.push_back(Tri3(BVertex(p[3], left, tu, tt), BVertex(q3, left, ts, tt), BVertex(b, left, ts, tv)));
		out.push_back(Tri3(BVertex(b, right, tu, tv), BVertex(q2, right, tu, tt), BVertex(p[2], right, ts, tt)));
		out.push_back(Tri3(BVertex(q2, right, ts, tt), BVertex(b, right, ts, tv), BVertex(a, right, tu, tv)));
		out.push_back(Tri3(BVertex(a, right, tu, tv), BVertex(q1, right, tu, tt), BVertex(q2, right, ts, tt)));
		out.push_back(Tri3(BVertex(p[1], right, tu, tt), BVertex(q1, right, ts, tt), BVertex(a, right, ts, tv)));
		return;
	}

	if(room.type == 24 && !p.empty())
	{
		Vec3 mn = p[0], mx = p[0];
		for(size_t i = 1; i < p.size(); i++)
		{
			if(p[i].x < mn.x) mn.x = p[i].x;
			if(p[i].y < mn.y) mn.y = p[i].y;
			if(p[i].z < mn.z) mn.z = p[i].z;
			if(p[i].x > mx.x) mx.x = p[i].x;
			if(p[i].y > mx.y) mx.y = p[i].y;
			if(p[i].z > mx.z) mx.z = p[i].z;
		}
		Vec3 centre = (mn + mx) * (1.0 / 2.0);
		double span = mx.x - mn.x;
		if(mx.x - mn.x <= mx.y - mn.y)
			span = mx.y - mn.y;
		const Vec3 apex(centre.x, centre.y, (span * 0.5) / 3.0 + centre.z);
		centre.z = centre.z + lift;
		const double bm = (double)((bs + bu) * 0.5f);
		Vec3 prev = p.back();
		for(size_t i = 0; i < p.size(); i++)
		{
			const Vec3& cur = p[i];
			Vec3 edge = cur - prev;
			const Vec3 up = Unit(apex - edge * (1.0 / 2.0));
			edge = Unit(edge);
			const Vec3 n = Cross(up, edge);
			out.push_back(Tri3(BVertex(prev, down, bs, bt), BVertex(centre, down, bm, bv), BVertex(cur, down, bu, bt)));
			out.push_back(Tri3(BVertex(cur, n, ts, tt), BVertex(apex, n, tm, tv), BVertex(prev, n, tu, tt)));
			prev = cur;
		}
	}
}

}
