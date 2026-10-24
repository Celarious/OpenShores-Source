#include "model.h"

#include <cmath>

namespace OldShip
{

namespace
{

const double StairRise = 7.98958333333;
const double HalfRise = 3.99479166667;
const double Gap = 0.0104166666667;
const double TwoGaps = 0.0208333333333;
const double StepsPerRise = 12.3709681225;
const double StepsPerHalfRise = 6.18548406126;
const double Slope = 0.7071;

const double AlongGrid = 5.656854249;
const double AlongGridRun = 11.313708498;

struct Rect
{
	float s, t, u, v;
	explicit Rect(ShipTex tex) { ShipTexture(tex, s, t, u, v); }
};

struct StairTexture
{
	Rect casing, riser, tread;
	StairTexture() : casing(ST_StairCasing), riser(ST_StairRiser), tread(ST_StairTread) {}
};

struct Place
{
	const Vec3&	at;
	float		rot;
	Place(const Vec3& origin, double radians) : at(origin), rot((float)radians) {}
};

void SideTriangle(TriList& out, const Place& pl, const Vec3& normal,
		const Vec3& p0, float u0, float v0, const Vec3& p1, float u1, float v1, const Vec3& p2, float u2, float v2)
{
	ModelAdd(out, normal, pl.at, pl.rot, p0, u0, v0, p1, u1, v1, p2, u2, v2);
}

void Flight(TriList& out, const Place& pl, const StairTexture& tx, const Vec3& start, const Vec3& run, const Vec3& across,
		int steps, const Vec3& riserNormal, const Vec3& startSideNormal, const Vec3& farSideNormal)
{
	const double count = (double)steps;
	const Vec3 rise(0.0, 0.0, run.z / count);
	const Vec3 going(run.x / count, run.y / count, 0.0);
	const Vec3 up(0.0, 0.0, 1.0);
	const Rect& r = tx.riser;
	const Rect& t = tx.tread;
	for(int i = 0; i < steps; i++)
	{
		const double n = (double)i;
		const Vec3 foot = start + Vec3(run.x * n / count, run.y * n / count, run.z * n / count);
		const Vec3 footFar = foot + across;
		const Vec3 nose = foot + rise;
		const Vec3 back = nose + going;
		const Vec3 backFar = back + across;
		const Vec3 noseFar = nose + across;

		ModelAdd(out, riserNormal, pl.at, pl.rot, foot, r.s, r.v, nose, r.s, r.t, noseFar, r.u, r.t, footFar, r.u, r.v);
		ModelAdd(out, up, pl.at, pl.rot, nose, t.s, t.t, back, t.s, t.v, backFar, t.u, t.v, noseFar, t.u, t.t);
		SideTriangle(out, pl, startSideNormal, back, r.s, r.t, nose, r.u, r.t, foot, r.u, r.v);
		SideTriangle(out, pl, farSideNormal, footFar, r.s, r.v, noseFar, r.s, r.t, backFar, r.u, r.t);
	}
}

void Landing(TriList& out, const Place& pl, const StairTexture& tx, double x0, double x1, double y0, double y1,
		double top, double bottom)
{
	const Rect& c = tx.casing;
	const Rect& r = tx.riser;
	const Rect& t = tx.tread;
	ModelAdd(out, Vec3(0.0, 0.0, 1.0), pl.at, pl.rot,
			Vec3(x0, y0, top), t.s, t.t, Vec3(x0, y1, top), t.s, t.v, Vec3(x1, y1, top), t.u, t.v, Vec3(x1, y0, top), t.u, t.t);
	ModelAdd(out, Vec3(0.0, 0.0, -1.0), pl.at, pl.rot,
			Vec3(x0, y1, bottom), c.s, c.t, Vec3(x0, y0, bottom), c.s, c.v, Vec3(x1, y0, bottom), c.u, c.v, Vec3(x1, y1, bottom), c.u, c.t);
	ModelAdd(out, Vec3(0.0, -1.0, 0.0), pl.at, pl.rot,
			Vec3(x0, y0, bottom), r.s, r.v, Vec3(x0, y0, top), r.s, r.t, Vec3(x1, y0, top), r.u, r.t, Vec3(x1, y0, bottom), r.u, r.v);
	ModelAdd(out, Vec3(-1.0, 0.0, 0.0), pl.at, pl.rot,
			Vec3(x0, y1, bottom), r.s, r.v, Vec3(x0, y1, top), r.s, r.t, Vec3(x0, y0, top), r.u, r.t, Vec3(x0, y0, bottom), r.u, r.v);
	ModelAdd(out, Vec3(0.0, 1.0, 0.0), pl.at, pl.rot,
			Vec3(x1, y1, bottom), r.s, r.v, Vec3(x1, y1, top), r.s, r.t, Vec3(x0, y1, top), r.u, r.t, Vec3(x0, y1, bottom), r.u, r.v);
	ModelAdd(out, Vec3(1.0, 0.0, 0.0), pl.at, pl.rot,
			Vec3(x1, y0, bottom), r.s, r.v, Vec3(x1, y0, top), r.s, r.t, Vec3(x1, y1, top), r.u, r.t, Vec3(x1, y1, bottom), r.u, r.v);
}

void Casing(TriList& out, const Place& pl, const StairTexture& tx, const Vec3& normal,
		const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d)
{
	const Rect& k = tx.casing;
	ModelAdd(out, normal, pl.at, pl.rot, a, k.s, k.t, b, k.u, k.t, c, k.u, k.v, d, k.s, k.v);
}

}

void Builder::RenderStairsStraight(TriList& out, const Vec3& at, double rot, bool diagonal) const
{
	const double length = diagonal ? 12.0 : AlongGridRun;
	const double width = diagonal ? 4.0 : AlongGrid;
	const Place pl(at, rot);
	const StairTexture tx;

	const double farEdge = width - Gap;
	Casing(out, pl, tx, Vec3(Slope, 0.0, -Slope),
			Vec3(0.0, Gap, 0.0), Vec3(length, Gap, StairRise), Vec3(length, farEdge, StairRise), Vec3(0.0, farEdge, 0.0));

	const int steps = (int)ceil(StepsPerRise);
	Flight(out, pl, tx, Vec3(0.0, farEdge, 0.0), Vec3(length, 0.0, StairRise), Vec3(0.0, -(width - TwoGaps), 0.0), steps,
			Vec3(-1.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0), Vec3(0.0, 1.0, 0.0));
}

void Builder::RenderStairsTurnLeft(TriList& out, const Vec3& at, double rot, bool diagonal) const
{
	const double width = diagonal ? 4.0 : AlongGrid;
	const double run = diagonal ? 8.0 : AlongGrid;
	const Place pl(at, rot);
	const StairTexture tx;

	const int steps = (int)ceil(StepsPerHalfRise);
	const double riser = HalfRise / (double)steps;
	const double farX = (width + run) - Gap;
	Landing(out, pl, tx, run, farX, Gap, width, HalfRise, HalfRise - riser);

	Casing(out, pl, tx, Vec3(Slope, 0.0, -Slope),
			Vec3(0.0, width - Gap, 0.0), Vec3(0.0, Gap, 0.0), Vec3(run, Gap, HalfRise), Vec3(run, width, HalfRise));
	const double across = width - TwoGaps;
	Flight(out, pl, tx, Vec3(0.0, width - Gap, 0.0), Vec3(run, 0.0, HalfRise), Vec3(0.0, -across, 0.0), steps,
			Vec3(-1.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0), Vec3(0.0, -1.0, 0.0));

	Casing(out, pl, tx, Vec3(0.0, Slope, -Slope),
			Vec3(run, width, HalfRise), Vec3(farX, width, HalfRise), Vec3(farX, width + run, StairRise),
			Vec3(run + Gap, width + run, StairRise));
	Flight(out, pl, tx, Vec3(run + Gap, width, HalfRise), Vec3(0.0, run, HalfRise), Vec3(across, 0.0, 0.0), steps,
			Vec3(0.0, -1.0, 0.0), Vec3(-1.0, 0.0, 0.0), Vec3(1.0, 0.0, 0.0));
}

void Builder::RenderStairsTurnRight(TriList& out, const Vec3& at, double rot, bool diagonal) const
{
	const double run = diagonal ? 8.0 : AlongGrid;
	const double width = diagonal ? 4.0 : AlongGrid;
	const Place pl(at, rot);
	const StairTexture tx;

	const int steps = (int)ceil(StepsPerHalfRise);
	const double riser = HalfRise / (double)steps;
	const double farX = (width + run) - Gap;
	Landing(out, pl, tx, run, farX, -width, -Gap, HalfRise, HalfRise - riser);

	Casing(out, pl, tx, Vec3(Slope, 0.0, -Slope),
			Vec3(0.0, -Gap, 0.0), Vec3(0.0, -(width - Gap), 0.0), Vec3(run, -width, HalfRise), Vec3(run, -Gap, HalfRise));
	const double across = -(width - TwoGaps);
	Flight(out, pl, tx, Vec3(0.0, -Gap, 0.0), Vec3(run, 0.0, HalfRise), Vec3(0.0, across, 0.0), steps,
			Vec3(-1.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0), Vec3(0.0, -1.0, 0.0));

	Casing(out, pl, tx, Vec3(0.0, -Slope, -Slope),
			Vec3(farX, -width, HalfRise), Vec3(run, -width, HalfRise), Vec3(run + Gap, -(width + run), StairRise),
			Vec3(farX, -(width + run), StairRise));
	Flight(out, pl, tx, Vec3(farX, -width, HalfRise), Vec3(0.0, -run, HalfRise), Vec3(across, 0.0, 0.0), steps,
			Vec3(0.0, 1.0, 0.0), Vec3(1.0, 0.0, 0.0), Vec3(-1.0, 0.0, 0.0));
}

void Builder::RenderStairsUTurnLeft(TriList& out, const Vec3& at, double rot, bool diagonal) const
{
	const double run = diagonal ? 8.0 : AlongGrid;
	const double width = diagonal ? 6.0 : AlongGrid;
	const double depth = diagonal ? 4.0 : AlongGrid;
	const Place pl(at, rot);
	const StairTexture tx;

	const int steps = (int)ceil(StepsPerHalfRise);
	const double riser = HalfRise / (double)steps;
	const double farX = (depth + run) - Gap;
	const double farY = (width + width) - Gap;
	Landing(out, pl, tx, run, farX, Gap, farY, HalfRise, HalfRise - riser);

	Casing(out, pl, tx, Vec3(Slope, 0.0, -Slope),
			Vec3(0.0, width, 0.0), Vec3(0.0, Gap, 0.0), Vec3(run, Gap, HalfRise), Vec3(run, width, HalfRise));
	const double across = width - Gap;
	Flight(out, pl, tx, Vec3(0.0, width, 0.0), Vec3(run, 0.0, HalfRise), Vec3(0.0, -across, 0.0), steps,
			Vec3(-1.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0), Vec3(0.0, -1.0, 0.0));

	Casing(out, pl, tx, Vec3(-Slope, 0.0, -Slope),
			Vec3(run, width, HalfRise), Vec3(run, farY, HalfRise), Vec3(0.0, farY, StairRise), Vec3(0.0, width, StairRise));
	Flight(out, pl, tx, Vec3(run, width, HalfRise), Vec3(-run, 0.0, HalfRise), Vec3(0.0, across, 0.0), steps,
			Vec3(1.0, 0.0, 0.0), Vec3(0.0, -1.0, 0.0), Vec3(0.0, 1.0, 0.0));
}

void Builder::RenderStairsUTurnRight(TriList& out, const Vec3& at, double rot, bool diagonal) const
{
	const double run = diagonal ? 8.0 : AlongGrid;
	const double width = diagonal ? 6.0 : AlongGrid;
	const double depth = diagonal ? 4.0 : AlongGrid;
	const Place pl(at, rot);
	const StairTexture tx;

	const int steps = (int)ceil(StepsPerHalfRise);
	const double riser = HalfRise / (double)steps;
	const double farX = (depth + run) - Gap;
	const double farY = -((width + width) - Gap);
	Landing(out, pl, tx, run, farX, farY, -Gap, HalfRise, HalfRise - riser);

	Casing(out, pl, tx, Vec3(Slope, 0.0, -Slope),
			Vec3(0.0, -Gap, 0.0), Vec3(0.0, -width, 0.0), Vec3(run, -width, HalfRise), Vec3(run, -Gap, HalfRise));
	const double across = width - Gap;
	Flight(out, pl, tx, Vec3(0.0, -Gap, 0.0), Vec3(run, 0.0, HalfRise), Vec3(0.0, -across, 0.0), steps,
			Vec3(-1.0, 0.0, 0.0), Vec3(0.0, 1.0, 0.0), Vec3(0.0, -1.0, 0.0));

	Casing(out, pl, tx, Vec3(-Slope, 0.0, -Slope),
			Vec3(run, farY, HalfRise), Vec3(run, -width, HalfRise), Vec3(0.0, -width, StairRise), Vec3(0.0, farY, StairRise));
	Flight(out, pl, tx, Vec3(run, farY, HalfRise), Vec3(-run, 0.0, HalfRise), Vec3(0.0, across, 0.0), steps,
			Vec3(1.0, 0.0, 0.0), Vec3(0.0, -1.0, 0.0), Vec3(0.0, 1.0, 0.0));
}

}
