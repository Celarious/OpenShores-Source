#include "model.h"

#include <cmath>

namespace OldShip
{

namespace
{

const int HullEffect = 0x1f;
const float HullShine = 50.0f;
const int HullInts[3] = { 0, 0, 3 };
const int InsideInts[3] = { 0, 0, 0 };

const double OpeningHalfSize = 2.0;
const double TopCornerCut = 1.41421356225;
const double BottomCornerCut = 1.4142135381698608;

const int TopTurret = 22;
const int BottomTurret = 33;
const double QuarterTurn = 1.5707963267948966;

enum Frac
{
	FracZero,
	FracOctLow,
	FracQuarter,
	FracHalf,
	FracThreeQuarter,
	FracOctHigh,
	FracOne,
	FracCount
};

struct CapTexture
{
	float u[FracCount], v[FracCount];
	float eu[FracCount], ev[FracCount];

	explicit CapTexture(ShipTex tex)
	{
		float s, t, right, top;
		ShipTexture(tex, s, t, right, top);
		Spread(u, s, right);
		Spread(v, t, top);
		EffectTexture(HullEffect, s, t, right, top);
		Spread(eu, s, right);
		Spread(ev, t, top);
	}

private:
	static void Spread(float* at, float low, float high)
	{
		const float size = high - low;
		at[FracZero] = low;
		at[FracOctLow] = size * 0.14644661f + low;
		at[FracQuarter] = size * 0.25f + low;
		at[FracHalf] = size * 0.5f + low;
		at[FracThreeQuarter] = size * 0.75f + low;
		at[FracOctHigh] = size * 0.85355339f + low;
		at[FracOne] = high;
	}
};

struct CapVertex
{
	Vec3	p;
	float	u, v, eu, ev;
};

inline CapVertex Vert(const CapTexture& tx, const Vec3& p, Frac across, Frac up)
{
	CapVertex r;
	r.p = p;
	r.u = tx.u[across]; r.v = tx.v[up];
	r.eu = tx.eu[across]; r.ev = tx.ev[up];
	return r;
}

struct CapCell
{
	CapVertex sw, nw, ne, se;
	CapVertex c;
	CapVertex e, n, w, s;
	CapVertex cutSW, cutNW, cutNE, cutSE;
	CapVertex octSW, octNW, octNE, octSE;

	CapCell(const CapTexture& tx, int x, int y, double z, double cut)
	{
		sw = Vert(tx, DeckCoord(x, y, z), FracZero, FracZero);
		nw = Vert(tx, DeckCoord(x, y + 4, z), FracZero, FracOne);
		ne = Vert(tx, DeckCoord(x + 4, y + 4, z), FracOne, FracOne);
		se = Vert(tx, DeckCoord(x + 4, y, z), FracOne, FracZero);
		c = Vert(tx, DeckCoord(x + 2, y + 2, z), FracHalf, FracHalf);
		e = Vert(tx, DeckCoord(x + 4, y + 2, z), FracOne, FracHalf);
		n = Vert(tx, DeckCoord(x + 2, y + 4, z), FracHalf, FracOne);
		w = Vert(tx, DeckCoord(x, y + 2, z), FracZero, FracHalf);
		s = Vert(tx, DeckCoord(x + 2, y, z), FracHalf, FracZero);
		const Vec3& m = c.p;
		cutSW = Vert(tx, Vec3(m.x - cut, m.y - cut, m.z), FracQuarter, FracQuarter);
		cutNW = Vert(tx, Vec3(m.x - cut, m.y + cut, m.z), FracQuarter, FracThreeQuarter);
		cutNE = Vert(tx, Vec3(m.x + cut, m.y + cut, m.z), FracThreeQuarter, FracThreeQuarter);
		cutSE = Vert(tx, Vec3(m.x + cut, m.y - cut, m.z), FracThreeQuarter, FracQuarter);
		const double h = OpeningHalfSize;
		octSW = Vert(tx, Vec3(m.x - h, m.y - h, m.z), FracOctLow, FracOctLow);
		octNW = Vert(tx, Vec3(m.x - h, m.y + h, m.z), FracOctLow, FracOctHigh);
		octNE = Vert(tx, Vec3(m.x + h, m.y + h, m.z), FracOctHigh, FracOctHigh);
		octSE = Vert(tx, Vec3(m.x + h, m.y - h, m.z), FracOctHigh, FracOctLow);
	}
};

struct CapStyle
{
	Vec3		normal;
	const int*	ints;
	float		shine;
	bool		inside;
	bool		bottom;
};

inline void AddTri(TriList& out, const CapStyle& st, const CapVertex& a, const CapVertex& b, const CapVertex& c)
{
	ModelAdd(out, st.normal, st.ints, st.shine, st.inside,
			a.u, a.v, a.eu, a.ev, a.p, b.u, b.v, b.eu, b.ev, b.p, c.u, c.v, c.eu, c.ev, c.p);
}

struct Side
{
	uint16_t	cell;
	bool		below;
	bool		covered;
	bool		windowed;

	Side(uint16_t value, int armor) : cell(value)
	{
		below = (value & FloorBuffer::Below) != 0;
		covered = (value & FloorBuffer::Covered) != 0;
		const int roomType = (value >> 8) & 0x1f;
		windowed = roomType == 2 || roomType == 14
				|| (roomType == 21 && armor < 8)
				|| (roomType == 4 && armor < 7);
	}
	bool Roof() const { return cell != 0 && below; }
	bool Belly() const { return cell != 0 && !below && !covered; }
	bool DrawnAs(WallType wall) const { return wall == (windowed ? WT_Window : WT_Plate); }
};

struct Quarter
{
	const CapVertex	*a, *b;
	const CapVertex	*mid;
	const CapVertex	*midFirst;
	const CapVertex	*cutA, *cutB;
	const CapVertex	*octA, *octB;
	bool			openA, openB, openMid;

	Quarter(const CapVertex& ca, const CapVertex& cb, const CapVertex& m, const CapVertex& ka, const CapVertex& kb,
			const CapVertex& oa, const CapVertex& ob, bool holeA, bool holeB, bool holeMid)
		: a(&ca), b(&cb), mid(&m), midFirst(&m), cutA(&ka), cutB(&kb), octA(&oa), octB(&ob),
		  openA(holeA), openB(holeB), openMid(holeMid) {}
};

void RenderQuarter(TriList& out, const CapStyle& st, const CapVertex& centre, bool openCentre, const Quarter& q)
{
	if(openCentre)
	{
		if(!st.bottom)
		{
			AddTri(out, st, *q.octA, *q.a, *q.b);
			AddTri(out, st, *q.b, *q.octB, *q.octA);
		}
		else
		{
			AddTri(out, st, *q.a, *q.octA, *q.b);
			AddTri(out, st, *q.b, *q.octA, *q.octB);
		}
	}
	else if(!q.openA && !q.openB && !q.openMid)
	{
		if(!st.bottom)
			AddTri(out, st, centre, *q.a, *q.b);
		else
			AddTri(out, st, centre, *q.b, *q.a);
	}
	else if(q.openA || q.openB)
	{
		const CapVertex& endA = q.openA ? *q.cutA : *q.a;
		const CapVertex& endB = q.openB ? *q.cutB : *q.b;
		if(!st.bottom)
		{
			AddTri(out, st, centre, endA, *q.midFirst);
			AddTri(out, st, *q.mid, endB, centre);
		}
		else
		{
			AddTri(out, st, centre, *q.midFirst, endA);
			AddTri(out, st, *q.mid, centre, endB);
		}
	}
}

Vec3 RotateX(const Vec3& p, double radians)
{
	const double c = cos(radians);
	const double s = sin(radians);
	return Vec3(p.x, p.y * c - p.z * s, p.y * s + p.z * c);
}

bool FuzzyEquals2D(const Vec3& a, const Vec3& b)
{
	return fabs(a.x - b.x) <= 1e-5 && fabs(a.y - b.y) <= 1e-5;
}

bool IsOnWall(int detailType)
{
	switch(detailType)
	{
	case 3: case 5: case 9: case 49: case 50: case 53: case 54: case 55: case 56:
		return true;
	}
	return false;
}

ShipTex HatchTexture(int detailType)
{
	if(detailType == 8 || (detailType >= 0x4d && detailType <= 0x53))
		return ST_IntHatchHalfOpen;
	if(detailType == 0x33 || (detailType >= 0x54 && detailType <= 0x5a))
		return ST_CrewHatchHalfOpen;
	return ST_HullHatchHalfOpen;
}

}

bool Builder::IsTopOrBottomOpeningAt(const Vec3& at) const
{
	const int deck = ClosestDeck(at.z);
	for(size_t i = 0; i < m.details.size(); i++)
	{
		const Detail& d = m.details[i];
		const int t = d.type;
		if(d.deck == deck && (t == 8 || t == 0x33 || t == 0x34 || (t >= 0x4d && t <= 0x61) || t == BottomTurret || t == TopTurret)
				&& FuzzyEquals(d.Loc(), at))
			return true;
	}
	return false;
}

bool Builder::IsHatchAt(const Vec3& at, int& detailType) const
{
	const int deck = ClosestDeck(at.z);
	for(size_t i = 0; i < openings.size(); i++)
	{
		const Opening& o = openings[i];
		if(o.deck == deck && !IsOnWall(o.type) && FuzzyEquals2D(o.loc, at))
		{
			detailType = o.type;
			return true;
		}
	}
	detailType = 0;
	return false;
}

int Builder::HatchAt(const Vec3& at) const
{
	const int deck = ClosestDeck(at.z);
	for(size_t i = 0; i < openings.size(); i++)
	{
		const Opening& o = openings[i];
		if(o.deck == deck && !IsOnWall(o.type) && FuzzyEquals2D(o.loc, at))
			return o.id;
	}
	return 0;
}

int Builder::DoorIndexDesignLocked(int openingId) const
{
	for(size_t i = 0; i < openings.size(); i++)
		if(openings[i].id == openingId)
			return (int)i;
	return -1;
}

void Builder::RenderHatch(TriList& out, const Vec3& at, bool diagonal, int side, bool inside, int detailType) const
{
	Vec3 a, b, c, d;
	const double z = at.z + 0.0;
	if(!diagonal)
	{
		a = Vec3(at.x + OpeningHalfSize, at.y + OpeningHalfSize, z);
		b = Vec3(at.x + OpeningHalfSize, at.y - OpeningHalfSize, z);
		c = Vec3(at.x - OpeningHalfSize, at.y - OpeningHalfSize, z);
		d = Vec3(at.x - OpeningHalfSize, at.y + OpeningHalfSize, z);
	}
	else
	{
		a = Vec3(at.x + 0.0, at.y + GridUnit, z);
		b = Vec3(at.x + GridUnit, at.y + 0.0, z);
		c = Vec3(at.x + 0.0, at.y - GridUnit, z);
		d = Vec3(at.x - GridUnit, at.y + 0.0, z);
	}

	float s, t, u, v;
	ShipTexture(HatchTexture(detailType), s, t, u, v);
	int ints[3] = { 0, 0, 0 };
	const float shine = inside ? 0.0f : HullShine;

	const int id = HatchAt(at);
	if(id != 0)
	{
		const int door = DoorIndexDesignLocked(id);
		if(door >= 0)
		{
			const int animated = AnimatedDoors();
			ints[0] = door < animated ? door + 10 : animated + 9;
		}
	}

	const bool upper = inside ? (side == 0 || side == 2) : (side == 0 || side == 1);
	const bool lower = inside ? (side == 0 || side == 1) : (side == 0 || side == 2);
	if(upper)
	{
		const Vec3 up(0.0, 0.0, 1.0);
		ModelAdd(out, up, ints, shine, inside, s, t, -1.0f, -1.0f, c, s, v, -1.0f, -1.0f, d, u, v, -1.0f, -1.0f, a);
		ModelAdd(out, up, ints, shine, inside, u, v, -1.0f, -1.0f, a, u, t, -1.0f, -1.0f, b, s, t, -1.0f, -1.0f, c);
	}
	if(lower)
	{
		const Vec3 down(0.0, 0.0, -1.0);
		ModelAdd(out, down, ints, shine, inside, u, t, -1.0f, -1.0f, b, u, v, -1.0f, -1.0f, a, s, v, -1.0f, -1.0f, d);
		ModelAdd(out, down, ints, shine, inside, s, v, -1.0f, -1.0f, d, s, t, -1.0f, -1.0f, c, u, t, -1.0f, -1.0f, b);
	}
}

void Builder::RenderTurret(TriList& out, const Vec3& at, double tilt, bool inside) const
{
	static const double Shape[13][3] =
	{
		{ GridUnit, 0.0, 0.0 }, { 2.0, 0.0, -2.0 }, { 0.0, 0.0, -GridUnit }, { -2.0, 0.0, -2.0 },
		{ -GridUnit, 0.0, 0.0 }, { -2.0, 0.0, 2.0 }, { 0.0, 0.0, GridUnit }, { 2.0, 0.0, 2.0 },
		{ 2.0, 2.0, 0.0 }, { 0.0, 2.0, -2.0 }, { -2.0, 2.0, 0.0 }, { 0.0, 2.0, 2.0 },
		{ 0.0, GridUnit, 0.0 },
	};
	static const int Faces[16][3] =
	{
		{ 8, 12, 9 }, { 1, 8, 9 }, { 8, 1, 0 }, { 8, 0, 7 },
		{ 9, 12, 10 }, { 3, 9, 10 }, { 9, 3, 2 }, { 9, 2, 1 },
		{ 10, 12, 11 }, { 5, 10, 11 }, { 10, 5, 4 }, { 10, 4, 3 },
		{ 11, 12, 8 }, { 7, 11, 8 }, { 11, 7, 6 }, { 11, 6, 5 },
	};

	Vec3 pts[13];
	for(int i = 0; i < 13; i++)
	{
		const Vec3 r = RotateX(Vec3(Shape[i][0], Shape[i][1], Shape[i][2]), tilt);
		pts[i] = Vec3(at.x + r.x, at.y + r.y, at.z + r.z);
	}

	float s, t, u, v;
	ShipTexture(ST_Turret, s, t, u, v);
	const int* ints = inside ? InsideInts : HullInts;
	const float shine = inside ? 0.0f : HullShine;
	for(int i = 0; i < 16; i++)
	{
		const Vec3& p0 = pts[Faces[i][0]];
		const Vec3& p1 = pts[Faces[i][1]];
		const Vec3& p2 = pts[Faces[i][2]];
		const Vec3 normal = Unit(Cross(Unit(p2 - p0), Unit(p1 - p0)));
		ModelAdd(out, normal, ints, shine, inside, s, t, -1.0f, -1.0f, p0, s, v, -1.0f, -1.0f, p1, u, t, -1.0f, -1.0f, p2);
	}
}

namespace
{

const Vec3* FittingSpot(const Design& m, const CapCell& cell, int detailType)
{
	const Vec3* spots[4] = { &cell.sw.p, &cell.s.p, &cell.w.p, &cell.c.p };
	for(int k = 0; k < 4; k++)
	{
		const int deck = ClosestDeck(spots[k]->z);
		for(size_t i = 0; i < m.details.size(); i++)
		{
			const Detail& d = m.details[i];
			if(d.deck == deck && d.type == detailType && FuzzyEquals(d.Loc(), *spots[k]))
				return spots[k];
		}
	}
	return 0;
}

inline void Extend(int x, int y, int& minX, int& maxX, int& minY, int& maxY)
{
	if(x < minX) minX = x;
	if(maxX < x) maxX = x;
	if(y < minY) minY = y;
	if(maxY < y) maxY = y;
}

}

void Builder::RenderExteriorTops(TriList& out, ShipTex tex, WallType wall, double z, int& minX, int& maxX, int& minY, int& maxY,
		FloorBuffer& fb, bool inside) const
{
	const FloorBuffer& lattice = fb;
	const CapTexture tx(tex);
	CapStyle st;
	st.normal = Vec3(0.0, 0.0, 1.0);
	st.ints = inside ? InsideInts : HullInts;
	st.shine = inside ? 0.0f : HullShine;
	st.inside = inside;
	st.bottom = false;

	for(int y = 0; y < FloorBuffer::Size - 1; y += 4)
		for(int x = 0; x < FloorBuffer::Size - 1; x += 4)
		{
			const Side east(lattice.At(x + 3, y + 2), m.armor);
			const Side north(lattice.At(x + 2, y + 3), m.armor);
			const Side west(lattice.At(x + 1, y + 2), m.armor);
			const Side south(lattice.At(x + 2, y + 1), m.armor);
			if(east.cell == 0 && north.cell == 0 && west.cell == 0 && south.cell == 0)
				continue;
			Extend(x, y, minX, maxX, minY, maxY);

			const CapCell cell(tx, x, y, z, TopCornerCut);
			const bool openSW = IsTopOrBottomOpeningAt(cell.sw.p);
			const bool openNW = IsTopOrBottomOpeningAt(cell.nw.p);
			const bool openNE = IsTopOrBottomOpeningAt(cell.ne.p);
			const bool openSE = IsTopOrBottomOpeningAt(cell.se.p);
			const bool openC = IsTopOrBottomOpeningAt(cell.c.p);
			const bool openE = IsTopOrBottomOpeningAt(cell.e.p);
			const bool openN = IsTopOrBottomOpeningAt(cell.n.p);
			const bool openW = IsTopOrBottomOpeningAt(cell.w.p);
			const bool openS = IsTopOrBottomOpeningAt(cell.s.p);
			const bool anyOpening = openSW || openNW || openNE || openSE || openC || openE || openN || openW || openS;

			if(!anyOpening && east.Roof() && north.Roof() && west.Roof() && south.Roof()
					&& north.windowed == east.windowed && west.windowed == east.windowed && south.windowed == east.windowed)
			{
				if(east.DrawnAs(wall))
				{
					ModelAdd(out, st.normal, st.ints, st.shine, inside,
							cell.sw.u, cell.sw.v, cell.sw.eu, cell.sw.ev, cell.sw.p,
							cell.nw.u, cell.nw.v, cell.nw.eu, cell.nw.ev, cell.nw.p,
							cell.ne.u, cell.ne.v, cell.ne.eu, cell.ne.ev, cell.ne.p,
							cell.se.u, cell.se.v, cell.se.eu, cell.se.ev, cell.se.p);
					continue;
				}
			}
			else
			{
				if(east.Belly() && north.Belly() && west.Belly() && south.Belly())
					continue;
				if(east.Roof() && east.DrawnAs(wall))
					RenderQuarter(out, st, cell.c, openC, Quarter(cell.ne, cell.se, cell.e, cell.cutNE, cell.cutSE, cell.octNE, cell.octSE, openNE, openSE, openE));
				if(north.Roof() && north.DrawnAs(wall))
					RenderQuarter(out, st, cell.c, openC, Quarter(cell.nw, cell.ne, cell.n, cell.cutNW, cell.cutNE, cell.octNW, cell.octNE, openNW, openNE, openN));
				if(west.Roof() && west.DrawnAs(wall))
					RenderQuarter(out, st, cell.c, openC, Quarter(cell.sw, cell.nw, cell.w, cell.cutSW, cell.cutNW, cell.octSW, cell.octNW, openSW, openNW, openW));
				if(south.Roof() && south.DrawnAs(wall))
					RenderQuarter(out, st, cell.c, openC, Quarter(cell.se, cell.sw, cell.s, cell.cutSE, cell.cutSW, cell.octSE, cell.octSW, openSE, openSW, openS));
			}

			if(wall == WT_Turret)
			{
				const Vec3* spot = FittingSpot(m, cell, TopTurret);
				if(spot)
					RenderTurret(out, *spot, QuarterTurn, inside);
			}
			else if(wall == WT_Door)
			{
				const int side = inside ? 2 : 1;
				int type = 0;
				if(west.below && south.below && IsHatchAt(cell.sw.p, type))
					RenderHatch(out, cell.sw.p, true, side, inside, type);
				else if(south.below && IsHatchAt(cell.s.p, type))
					RenderHatch(out, cell.s.p, true, side, inside, type);
				else if(west.below && IsHatchAt(cell.w.p, type))
					RenderHatch(out, cell.w.p, true, side, inside, type);
				else if(east.below && north.below && west.below && south.below && IsHatchAt(cell.c.p, type))
					RenderHatch(out, cell.c.p, false, side, inside, type);
			}
		}
}

void Builder::RenderExteriorBottoms(TriList& out, ShipTex tex, WallType wall, double z, int& minX, int& maxX, int& minY, int& maxY,
		FloorBuffer& fb, bool inside) const
{
	const FloorBuffer& lattice = fb;
	const CapTexture tx(tex);
	CapStyle st;
	st.normal = Vec3(0.0, 0.0, -1.0);
	st.ints = inside ? InsideInts : HullInts;
	st.shine = inside ? 0.0f : HullShine;
	st.inside = inside;
	st.bottom = true;

	for(int y = 0; y < FloorBuffer::Size - 1; y += 4)
		for(int x = 0; x < FloorBuffer::Size - 1; x += 4)
		{
			const Side east(lattice.At(x + 3, y + 2), m.armor);
			const Side north(lattice.At(x + 2, y + 3), m.armor);
			const Side west(lattice.At(x + 1, y + 2), m.armor);
			const Side south(lattice.At(x + 2, y + 1), m.armor);
			if(east.cell == 0 && north.cell == 0 && west.cell == 0 && south.cell == 0)
				continue;
			Extend(x, y, minX, maxX, minY, maxY);

			const CapCell cell(tx, x, y, z, BottomCornerCut);
			const bool openSW = IsTopOrBottomOpeningAt(cell.sw.p);
			const bool openNW = IsTopOrBottomOpeningAt(cell.nw.p);
			const bool openNE = IsTopOrBottomOpeningAt(cell.ne.p);
			const bool openSE = IsTopOrBottomOpeningAt(cell.se.p);
			const bool openC = IsTopOrBottomOpeningAt(cell.c.p);
			const bool openE = IsTopOrBottomOpeningAt(cell.e.p);
			const bool openN = IsTopOrBottomOpeningAt(cell.n.p);
			const bool openW = IsTopOrBottomOpeningAt(cell.w.p);
			const bool openS = IsTopOrBottomOpeningAt(cell.s.p);
			const bool anyOpening = openSW || openNW || openNE || openSE || openC || openE || openN || openW || openS;

			if(east.Roof() && north.Roof() && west.Roof() && south.Roof())
				continue;

			if(!anyOpening && east.Belly() && north.Belly() && west.Belly() && south.Belly()
					&& north.windowed == east.windowed && west.windowed == east.windowed && south.windowed == east.windowed)
			{
				if(east.DrawnAs(wall))
				{
					ModelAdd(out, st.normal, st.ints, st.shine, inside,
							cell.se.u, cell.se.v, cell.sw.eu, cell.se.ev, cell.se.p,
							cell.ne.u, cell.ne.v, cell.ne.eu, cell.ne.ev, cell.ne.p,
							cell.nw.u, cell.nw.v, cell.nw.eu, cell.nw.ev, cell.nw.p,
							cell.sw.u, cell.sw.v, cell.sw.eu, cell.sw.ev, cell.sw.p);
					continue;
				}
			}
			else
			{
				if(east.Belly() && east.DrawnAs(wall))
					RenderQuarter(out, st, cell.c, openC, Quarter(cell.ne, cell.se, cell.e, cell.cutNE, cell.cutSE, cell.octNE, cell.octSE, openNE, openSE, openE));
				if(north.Belly() && north.DrawnAs(wall))
					RenderQuarter(out, st, cell.c, openC, Quarter(cell.nw, cell.ne, cell.n, cell.cutNW, cell.cutNE, cell.octNW, cell.octNE, openNW, openNE, openN));
				if(west.Belly() && west.DrawnAs(wall))
				{
					Quarter q(cell.sw, cell.nw, cell.w, cell.cutSW, cell.cutNW, cell.octSW, cell.octNW, openSW, openNW, openW);
					CapVertex oddMid = cell.w;
					if(!openSW)
					{
						oddMid.eu = cell.s.eu;
						oddMid.ev = cell.s.ev;
						q.midFirst = &oddMid;
					}
					RenderQuarter(out, st, cell.c, openC, q);
				}
				if(south.Belly() && south.DrawnAs(wall))
					RenderQuarter(out, st, cell.c, openC, Quarter(cell.se, cell.sw, cell.s, cell.cutSE, cell.cutSW, cell.octSE, cell.octSW, openSE, openSW, openS));
			}

			if(wall == WT_Turret)
			{
				const Vec3* spot = FittingSpot(m, cell, BottomTurret);
				if(spot)
					RenderTurret(out, *spot, -QuarterTurn, inside);
			}
			else if(wall == WT_Door)
			{
				const int side = inside ? 1 : 2;
				int type = 0;
				if(!west.covered && !south.covered && IsHatchAt(cell.sw.p, type))
					RenderHatch(out, cell.sw.p, true, side, inside, type);
				else if(!south.covered && IsHatchAt(cell.s.p, type))
					RenderHatch(out, cell.s.p, true, side, inside, type);
				else if(!west.covered && IsHatchAt(cell.w.p, type))
					RenderHatch(out, cell.w.p, true, side, inside, type);
				else if(!east.covered && !north.covered && !west.covered && !south.covered && IsHatchAt(cell.c.p, type))
					RenderHatch(out, cell.c.p, false, side, inside, type);
			}
		}
}

}
