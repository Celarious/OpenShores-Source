#include "model.h"

namespace OldShip
{

namespace
{

const double OpeningHalfSize = 2.0;
const double CornerCut = LatticeUnit;

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

struct DeckTexture
{
	float u[FracCount], v[FracCount];

	explicit DeckTexture(ShipTex tex)
	{
		float s, t, right, top;
		ShipTexture(tex, s, t, right, top);
		Spread(u, s, right);
		Spread(v, t, top);
	}

private:
	static void Spread(float* at, float low, float high)
	{
		const double size = (double)(high - low);
		at[FracZero] = low;
		at[FracOctLow] = (float)(size * 0.1464466093759525 + (double)low);
		at[FracQuarter] = (float)(size * 0.25 + (double)low);
		at[FracHalf] = (float)(size * 0.5 + (double)low);
		at[FracThreeQuarter] = (float)(size * 0.75 + (double)low);
		at[FracOctHigh] = (float)(size * 0.8535533906240476 + (double)low);
		at[FracOne] = high;
	}
};

struct DeckVertex
{
	Vec3	p;
	float	u, v;
};

inline DeckVertex Vert(const DeckTexture& tx, const Vec3& p, Frac across, Frac up)
{
	DeckVertex r;
	r.p = p;
	r.u = tx.u[across];
	r.v = tx.v[up];
	return r;
}

struct DeckCell
{
	DeckVertex sw, nw, ne, se;
	DeckVertex c;
	DeckVertex e, n, w, s;
	DeckVertex cutSW, cutNW, cutNE, cutSE;
	DeckVertex octSW, octNW, octNE, octSE;

	DeckCell(const DeckTexture& tx, int x, int y, double z)
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
		const double k = CornerCut;
		cutSW = Vert(tx, Vec3(m.x - k, m.y - k, m.z), FracQuarter, FracQuarter);
		cutNW = Vert(tx, Vec3(m.x - k, m.y + k, m.z), FracQuarter, FracThreeQuarter);
		cutNE = Vert(tx, Vec3(m.x + k, m.y + k, m.z), FracThreeQuarter, FracThreeQuarter);
		cutSE = Vert(tx, Vec3(m.x + k, m.y - k, m.z), FracThreeQuarter, FracQuarter);
		const double h = OpeningHalfSize;
		octSW = Vert(tx, Vec3(m.x - h, m.y - h, m.z), FracOctLow, FracOctLow);
		octNW = Vert(tx, Vec3(m.x - h, m.y + h, m.z), FracOctLow, FracOctHigh);
		octNE = Vert(tx, Vec3(m.x + h, m.y + h, m.z), FracOctHigh, FracOctHigh);
		octSE = Vert(tx, Vec3(m.x + h, m.y - h, m.z), FracOctHigh, FracOctLow);
	}
};

inline void AddTri(TriList& out, const Vec3& normal, const DeckVertex& a, const DeckVertex& b, const DeckVertex& c)
{
	ModelAdd(out, normal, 0, 0.0f, false, a.u, a.v, -1.0f, -1.0f, a.p, b.u, b.v, -1.0f, -1.0f, b.p, c.u, c.v, -1.0f, -1.0f, c.p);
}

struct Side
{
	uint16_t	cell;
	bool		below;
	bool		covered;
	bool		open;
	bool		windowed;
	int			roomType;

	Side(uint16_t value, int armor) : cell(value)
	{
		below = (value & FloorBuffer::Below) != 0;
		covered = (value & FloorBuffer::Covered) != 0;
		open = (value & FloorBuffer::OpenBelow) != 0;
		roomType = (value >> 8) & 0x1f;
		windowed = roomType == 2 || roomType == 14
				|| (roomType == 21 && armor < 8)
				|| (roomType == 4 && armor < 7);
	}
	bool Solid() const { return cell != 0 && !below && !open; }
	bool Floor() const { return Solid() && (!windowed || covered); }
	bool Slab() const { return Solid() && covered; }
};

struct Quarter
{
	const DeckVertex	*a, *b;
	const DeckVertex	*mid;
	const DeckVertex	*cutA, *cutB;
	const DeckVertex	*octA, *octB;
	bool				openA, openB, openMid;

	Quarter(const DeckVertex& ca, const DeckVertex& cb, const DeckVertex& m, const DeckVertex& ka, const DeckVertex& kb,
			const DeckVertex& oa, const DeckVertex& ob, bool holeA, bool holeB, bool holeMid)
		: a(&ca), b(&cb), mid(&m), cutA(&ka), cutB(&kb), octA(&oa), octB(&ob),
		  openA(holeA), openB(holeB), openMid(holeMid) {}
};

void RenderQuarter(TriList& out, const Vec3& normal, bool down, const DeckVertex& centre, bool openCentre, const Quarter& q)
{
	if(openCentre)
	{
		if(!down)
		{
			AddTri(out, normal, *q.octA, *q.a, *q.b);
			AddTri(out, normal, *q.b, *q.octB, *q.octA);
		}
		else
		{
			AddTri(out, normal, *q.a, *q.octA, *q.b);
			AddTri(out, normal, *q.b, *q.octA, *q.octB);
		}
	}
	else if(!q.openA && !q.openB && !q.openMid)
	{
		if(!down)
			AddTri(out, normal, centre, *q.a, *q.b);
		else
			AddTri(out, normal, centre, *q.b, *q.a);
	}
	else if(q.openA || q.openB)
	{
		const DeckVertex& endA = q.openA ? *q.cutA : *q.a;
		const DeckVertex& endB = q.openB ? *q.cutB : *q.b;
		if(!down)
		{
			AddTri(out, normal, centre, endA, *q.mid);
			AddTri(out, normal, *q.mid, endB, centre);
		}
		else
		{
			AddTri(out, normal, centre, *q.mid, endA);
			AddTri(out, normal, *q.mid, centre, endB);
		}
	}
}

inline void Extend(int x, int y, int& minX, int& maxX, int& minY, int& maxY)
{
	if(x < minX) minX = x;
	if(maxX < x) maxX = x;
	if(y < minY) minY = y;
	if(maxY < y) maxY = y;
}

struct Openings
{
	bool sw, nw, ne, se, c, e, n, w, s;
	bool Any() const { return sw || nw || ne || se || c || e || n || w || s; }
};

}

void Builder::RenderInteriorFloors(TriList& out, ShipTex tex, double z, bool hatches, int& minX, int& maxX, int& minY, int& maxY,
		const FloorBuffer& fb) const
{
	const DeckTexture tx(tex);
	const Vec3 up(0.0, 0.0, 1.0);

	for(int y = 0; y < FloorBuffer::Size - 1; y += 4)
		for(int x = 0; x < FloorBuffer::Size - 1; x += 4)
		{
			const Side east(fb.At(x + 3, y + 2), m.armor);
			const Side north(fb.At(x + 2, y + 3), m.armor);
			const Side west(fb.At(x + 1, y + 2), m.armor);
			const Side south(fb.At(x + 2, y + 1), m.armor);
			if(east.cell == 0 && north.cell == 0 && west.cell == 0 && south.cell == 0)
				continue;
			Extend(x, y, minX, maxX, minY, maxY);

			const DeckCell cell(tx, x, y, z);
			Openings open;
			open.sw = IsTopOrBottomOpeningAt(cell.sw.p);
			open.nw = IsTopOrBottomOpeningAt(cell.nw.p);
			open.ne = IsTopOrBottomOpeningAt(cell.ne.p);
			open.se = IsTopOrBottomOpeningAt(cell.se.p);
			open.c = IsTopOrBottomOpeningAt(cell.c.p);
			open.e = IsTopOrBottomOpeningAt(cell.e.p);
			open.n = IsTopOrBottomOpeningAt(cell.n.p);
			open.w = IsTopOrBottomOpeningAt(cell.w.p);
			open.s = IsTopOrBottomOpeningAt(cell.s.p);

			if(!hatches)
			{
				if(!open.Any() && east.Floor() && north.Floor() && west.Floor() && south.Floor()
						&& east.roomType == north.roomType && north.roomType == west.roomType && west.roomType == south.roomType)
				{
					ModelAdd(out, up, 0, 0.0f, false,
							cell.sw.u, cell.sw.v, -1.0f, -1.0f, cell.sw.p,
							cell.nw.u, cell.nw.v, -1.0f, -1.0f, cell.nw.p,
							cell.ne.u, cell.ne.v, -1.0f, -1.0f, cell.ne.p,
							cell.se.u, cell.se.v, -1.0f, -1.0f, cell.se.p);
					continue;
				}
				if(east.Floor())
					RenderQuarter(out, up, false, cell.c, open.c, Quarter(cell.ne, cell.se, cell.e, cell.cutNE, cell.cutSE, cell.octNE, cell.octSE, open.ne, open.se, open.e));
				if(north.Floor())
					RenderQuarter(out, up, false, cell.c, open.c, Quarter(cell.nw, cell.ne, cell.n, cell.cutNW, cell.cutNE, cell.octNW, cell.octNE, open.nw, open.ne, open.n));
				if(west.Floor())
					RenderQuarter(out, up, false, cell.c, open.c, Quarter(cell.sw, cell.nw, cell.w, cell.cutSW, cell.cutNW, cell.octSW, cell.octNW, open.sw, open.nw, open.w));
				if(south.Floor())
					RenderQuarter(out, up, false, cell.c, open.c, Quarter(cell.se, cell.sw, cell.s, cell.cutSE, cell.cutSW, cell.octSE, cell.octSW, open.se, open.sw, open.s));
				continue;
			}

			int type = 0;
			if(west.Slab() && south.Slab() && IsHatchAt(cell.sw.p, type))
				RenderHatch(out, cell.sw.p, true, 0, false, type);
			else if(south.Slab() && IsHatchAt(cell.s.p, type))
				RenderHatch(out, cell.s.p, true, 0, false, type);
			else if(west.Slab() && IsHatchAt(cell.w.p, type))
				RenderHatch(out, cell.w.p, true, 0, false, type);
			else if(east.Slab() && north.Slab() && west.Slab() && south.Slab() && IsHatchAt(cell.c.p, type))
				RenderHatch(out, cell.c.p, false, 0, false, type);
		}
}

void Builder::RenderInteriorCeilings(TriList& out, double z, int& minX, int& maxX, int& minY, int& maxY,
		const FloorBuffer& fb) const
{
	const DeckTexture tx(ST_IntCeiling);
	const Vec3 down(0.0, 0.0, -1.0);

	for(int y = 0; y < FloorBuffer::Size - 1; y += 4)
		for(int x = 0; x < FloorBuffer::Size - 1; x += 4)
		{
			const Side east(fb.At(x + 3, y + 2), m.armor);
			const Side north(fb.At(x + 2, y + 3), m.armor);
			const Side west(fb.At(x + 1, y + 2), m.armor);
			const Side south(fb.At(x + 2, y + 1), m.armor);
			if(east.cell == 0 && north.cell == 0 && west.cell == 0 && south.cell == 0)
				continue;
			Extend(x, y, minX, maxX, minY, maxY);

			const DeckCell cell(tx, x, y, z);
			Openings open;
			open.sw = IsTopOrBottomOpeningAt(cell.sw.p);
			open.nw = IsTopOrBottomOpeningAt(cell.nw.p);
			open.ne = IsTopOrBottomOpeningAt(cell.ne.p);
			open.se = IsTopOrBottomOpeningAt(cell.se.p);
			open.c = IsTopOrBottomOpeningAt(cell.c.p);
			open.e = IsTopOrBottomOpeningAt(cell.e.p);
			open.n = IsTopOrBottomOpeningAt(cell.n.p);
			open.w = IsTopOrBottomOpeningAt(cell.w.p);
			open.s = IsTopOrBottomOpeningAt(cell.s.p);

			if(!open.Any() && east.Slab() && north.Slab() && west.Slab() && south.Slab())
			{
				ModelAdd(out, down, 0, 0.0f, false,
						cell.se.u, cell.se.v, -1.0f, -1.0f, cell.se.p,
						cell.ne.u, cell.ne.v, -1.0f, -1.0f, cell.ne.p,
						cell.nw.u, cell.nw.v, -1.0f, -1.0f, cell.nw.p,
						cell.sw.u, cell.sw.v, -1.0f, -1.0f, cell.sw.p);
				continue;
			}
			if(east.Slab())
				RenderQuarter(out, down, true, cell.c, open.c, Quarter(cell.ne, cell.se, cell.e, cell.cutNE, cell.cutSE, cell.octNE, cell.octSE, open.ne, open.se, open.e));
			if(north.Slab())
				RenderQuarter(out, down, true, cell.c, open.c, Quarter(cell.nw, cell.ne, cell.n, cell.cutNW, cell.cutNE, cell.octNW, cell.octNE, open.nw, open.ne, open.n));
			if(west.Slab())
				RenderQuarter(out, down, true, cell.c, open.c, Quarter(cell.sw, cell.nw, cell.w, cell.cutSW, cell.cutNW, cell.octSW, cell.octNW, open.sw, open.nw, open.w));
			if(south.Slab())
				RenderQuarter(out, down, true, cell.c, open.c, Quarter(cell.se, cell.sw, cell.s, cell.cutSE, cell.cutSW, cell.octSE, cell.octSW, open.se, open.sw, open.s));
		}
}

}
