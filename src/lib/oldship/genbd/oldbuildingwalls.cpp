#include "oldbuilding.h"

#include <cmath>

namespace OldBuilding
{

using OldShip::Outline;
using OldShip::Unit;
using OldShip::Cross;
using OldShip::RotateZ;
using OldShip::Distance2D;

namespace
{

const double Tiny = 1e-5;
const double DegToRad = 0.0174532925199;

int Heading(double dx, double dy)
{
	if(dx > 0.0)
		return dy == 0.0 ? 0 : dy > 0.0 ? 1 : 7;
	if(dx < 0.0)
		return dy > 0.0 ? 3 : dy == 0.0 ? 4 : 5;
	return dy > 0.0 ? 2 : dy < 0.0 ? 6 : 7;
}

int Heading(const Vec3& a, const Vec3& b)
{
	double dx = b.x - a.x;
	double dy = b.y - a.y;
	if(fabs(dx) <= Tiny) dx = 0.0;
	if(fabs(dy) <= Tiny) dy = 0.0;
	return Heading(dx, dy);
}

const double HeadingAngle[8] =
{
	0.0, 0.78539816339744828, 1.5707963267948966, 2.3561944901923448,
	3.1415926535897931, 3.9269908169872414, 4.7123889803846897, 5.4977871437821380,
};

const double MitreSlide[8] =
{
	0.0, -0.41421356237309503, -1.0, -2.414213562373094, 0.0, 2.414213562373095, 1.0, 0.41421356237309515,
};

Vec3 Mitre(int headingIn, int headingOut)
{
	int turn = headingOut - headingIn;
	if(turn < 0)
		turn += 8;
	return RotateZ(Vec3(MitreSlide[turn], 1.0, 0.0), (double)headingIn * 45.0 * DegToRad);
}

Vec3 Place(const Vec3& corner, const Vec3& mitre, double outward, double height)
{
	const Vec3 p = mitre * outward;
	return Vec3(p.x + corner.x, p.y + corner.y, height + corner.z);
}

const double BallRim = OldShip::GridUnit;
const Vec3 Ball[13] =
{
	Vec3(BallRim, 0.0, 0.0), Vec3(2.0, 0.0, -2.0), Vec3(0.0, 0.0, -BallRim), Vec3(-2.0, 0.0, -2.0),
	Vec3(-BallRim, 0.0, 0.0), Vec3(-2.0, 0.0, 2.0), Vec3(0.0, 0.0, BallRim), Vec3(2.0, 0.0, 2.0),
	Vec3(2.0, 2.0, 0.0), Vec3(0.0, 2.0, -2.0), Vec3(-2.0, 2.0, 0.0), Vec3(0.0, 2.0, 2.0),
	Vec3(0.0, BallRim, 0.0),
};

const int BallFaces[16][3] =
{
	{ 8, 12, 9 }, { 8, 9, 1 }, { 8, 1, 0 }, { 8, 0, 7 },
	{ 9, 12, 10 }, { 9, 10, 3 }, { 9, 3, 2 }, { 9, 2, 1 },
	{ 10, 12, 11 }, { 10, 11, 5 }, { 10, 5, 4 }, { 10, 4, 3 },
	{ 11, 12, 8 }, { 11, 8, 7 }, { 11, 7, 6 }, { 11, 6, 5 },
};

const float BallTexMiddle = 0.6767767f;

const int RunTexture[7] = { 0, BT_LargeHullDoor, BT_HullDoor, BT_Turret, BT_HullWall, BT_HullWallWindow, BT_HullTrim };

int WallTextureType(uint16_t cell)
{
	switch((cell >> 8) & 0x1f)
	{
	case 3: case 5: case 0xd:
		return 1;
	case 8: case 0x10: case 0x11: case 0x15:
		return 2;
	}
	return 0;
}

void Emit(BTriList& out, const BVertex& a, const BVertex& b, const BVertex& c, bool inside, unsigned char window, int door = 0)
{
	BTri t;
	t.v[0] = a;
	t.v[1] = inside ? c : b;
	t.v[2] = inside ? b : c;
	t.window = window;
	t.door = door;
	out.push_back(t);
}

Vec3 Neg(const Vec3& v) { return Vec3(-v.x, -v.y, -v.z); }

void WallEndCap(BTriList& out, const TblProfile& profile, const Vec3& mitre, const Vec3& corner, bool start, unsigned char window)
{
	float s, t, u, v;
	BuildingTexture(BT_HullWall, s, t, u, v);
	const float rise = v - t;
	for(int i = 0; i < profile.ncaps; i++)
	{
		const double* c = profile.caps + i * 9;
		BVertex vert[3];
		for(int k = 0; k < 3; k++)
		{
			const double outward = c[k * 3], height = c[k * 3 + 2];
			const Vec3 p = mitre * outward;
			vert[k].pos = Vec3(p.x + corner.x, p.y + corner.y, height + corner.z);
			vert[k].u = (double)(outward != 0.0 ? u : s);
			vert[k].v = (double)((float)(height * 0.125) * rise + t);
		}
		Vec3 n = Unit(Cross(vert[2].pos - vert[0].pos, vert[1].pos - vert[0].pos));
		if(start)
			n = Neg(n);
		vert[0].normal = vert[1].normal = vert[2].normal = n;
		Emit(out, vert[0], vert[1], vert[2], start, window);
	}
}

}

void Builder::RenderExteriorWalls(BTriList& out, const std::vector<Outline>& outlines, bool inside) const
{
	const unsigned char lit = Window();
	const int windowTexture = industry == 0x78 ? (int)BT_StainedGlass : (int)BT_HullWallWindow;
	int texture = 1;
	float s = 0, t = 0, u = 0, v = 0;

	for(size_t o = 0; o < outlines.size(); o++)
	{
		const std::vector<Vec3>& points = outlines[o].points;
		const std::vector<uint16_t>& segs = outlines[o].segs;
		const std::vector<uint32_t>& ids = outlines[o].ids;
		const int count = (int)points.size();
		if(count < 2 || (int)segs.size() < count)
			continue;

		uint16_t seg = segs[count - 1];
		uint32_t doorId = (int)ids.size() >= count ? ids[count - 1] : 0;
		const bool below = points[0].z < 0.0;
		const TblProfile* profile = &WallProfile(!inside, (seg >> 8) & 0x1f, material, below);
		Vec3 from = points[count - 1];
		int headingIn = Heading(points[count - 2], from);

		for(int i = 0; i < count; i++)
		{
			const Vec3& to = points[i];
			Vec3 step = to - from;
			if(fabs(step.x) <= Tiny) step.x = 0.0;
			if(fabs(step.y) <= Tiny) step.y = 0.0;
			if(fabs(step.z) <= Tiny) step.z = 0.0;
			const int heading = Heading(step.x, step.y);
			const int headingOut = Heading(to, points[i + 1 < count ? i + 1 : 0]);
			const Vec3 mitreFrom = Mitre(headingIn, heading);
			const Vec3 mitreTo = Mitre(heading, headingOut);
			const Vec3 along = Unit(step);

			double lowOut = 0.0, lowUp = 0.0;
			int run = 0;
			for(int k = 0; k < profile->npoints; k++)
			{
				const double* pt = profile->points + k * 3;
				if(k != 0 && run != 0)
				{
					const Vec3 fromLow = Place(from, mitreFrom, lowOut, lowUp);
					const Vec3 fromHigh = Place(from, mitreFrom, pt[0], pt[2]);
					const Vec3 toLow = Place(to, mitreTo, lowOut, lowUp);
					const Vec3 toHigh = Place(to, mitreTo, pt[0], pt[2]);
					Vec3 normal = Unit(Cross(Unit(toHigh - toLow), along));
					if(inside)
						normal = Neg(normal);

					int tex = run == 5 ? windowTexture : RunTexture[run < 7 ? run : 0];
					bool byHeight = false;
					if(!inside)
						byHeight = tex == BT_HullWall;
					else if(tex == BT_HullWall || tex == BT_HullTrim)
					{
						byHeight = true;
						const int finish = WallTextureType(seg);
						tex = finish == 0 ? BT_IntWallFinished : finish == 1 ? BT_IntWallCargo : BT_IntWallEquipment;
					}
					if(texture != tex)
					{
						BuildingTexture(tex, s, t, u, v);
						texture = tex;
					}

					if(run == 3)
					{
						const float uMid = (u - s) * BallTexMiddle + s;
						const float vMid = (v - t) * BallTexMiddle + t;
						const Vec3 middle = (toHigh + fromLow) * (1.0 / 2.0);
						Vec3 ball[13];
						for(int b = 0; b < 13; b++)
							ball[b] = RotateZ(Ball[b], HeadingAngle[heading]) + middle;
#define BV(p, uu, vv) BVertex(p, normal, (double)(uu), (double)(vv))
						Emit(out, BV(toLow, u, v), BV(ball[0], u, t), BV(ball[1], uMid, vMid), inside, lit);
						Emit(out, BV(toLow, u, v), BV(ball[1], uMid, vMid), BV(ball[2], s, v), inside, lit);
						Emit(out, BV(toHigh, u, v), BV(ball[6], u, t), BV(ball[7], uMid, vMid), inside, lit);
						Emit(out, BV(toHigh, u, v), BV(ball[7], uMid, vMid), BV(ball[0], s, v), inside, lit);
						Emit(out, BV(fromHigh, u, v), BV(ball[4], u, t), BV(ball[5], uMid, vMid), inside, lit);
						Emit(out, BV(fromHigh, u, v), BV(ball[5], uMid, vMid), BV(ball[6], s, v), inside, lit);
						Emit(out, BV(fromLow, u, v), BV(ball[2], u, t), BV(ball[3], uMid, vMid), inside, lit);
						Emit(out, BV(fromLow, u, v), BV(ball[3], uMid, vMid), BV(ball[4], s, v), inside, lit);
						if(Distance2D(fromHigh, toHigh) - 5.656854249 > Tiny)
						{
							Emit(out, BV(fromLow, s, v), BV(ball[4], u, v), BV(fromHigh, u, t), inside, lit);
							Emit(out, BV(toHigh, s, v), BV(ball[0], u, v), BV(toLow, u, t), inside, lit);
						}
#undef BV
						for(int f = 0; f < 16; f++)
						{
							const Vec3& a = ball[BallFaces[f][0]];
							const Vec3& b = ball[BallFaces[f][1]];
							const Vec3& c = ball[BallFaces[f][2]];
							Vec3 faceNormal = Unit(Cross(Unit(c - a), Unit(b - a)));
							if(inside)
								faceNormal = Neg(faceNormal);
							Emit(out, BVertex(a, faceNormal, s, t), BVertex(b, faceNormal, s, v), BVertex(c, faceNormal, u, t), inside, lit);
						}
					}
					else if(byHeight)
					{
						const int deck = (int)floor((toLow.z + 0.05) * 0.125);
						const double vLow = (double)(float)((toLow.z - (double)deck * 8.0) * (double)(v - t) * 0.125 + (double)t);
						const double vHigh = (double)(float)((toHigh.z - (double)deck * 8.0) * (double)(v - t) * 0.125 + (double)t);
						Emit(out, BVertex(toLow, normal, s, vLow), BVertex(toHigh, normal, s, vHigh), BVertex(fromHigh, normal, u, vHigh), inside, lit);
						Emit(out, BVertex(fromHigh, normal, u, vHigh), BVertex(fromLow, normal, u, vLow), BVertex(toLow, normal, s, vLow), inside, lit);
					}
					else
					{
						const unsigned char w = tex == BT_StainedGlass ? 3 : lit;
						const int door = run == 1 || run == 2 ? DoorTag(doorId) : 0;
						Emit(out, BVertex(toLow, normal, s, t), BVertex(toHigh, normal, s, v), BVertex(fromHigh, normal, u, v), inside, w, door);
						Emit(out, BVertex(fromHigh, normal, u, v), BVertex(fromLow, normal, u, t), BVertex(toLow, normal, s, t), inside, w, door);
					}
				}
				lowOut = pt[0];
				lowUp = pt[2];
				run = k < profile->nwalls ? profile->walls[k] : 0;
			}

			if(seg != segs[i])
			{
				if(!inside && profile->ncaps != 0)
					WallEndCap(out, *profile, mitreTo, to, false, lit);
				const bool otherKind = ((segs[i] ^ seg) & 0x1f00) != 0;
				seg = segs[i];
				if(otherKind)
					profile = &WallProfile(!inside, (seg >> 8) & 0x1f, material, below);
				if(!inside && profile->ncaps != 0)
					WallEndCap(out, *profile, mitreTo, to, true, lit);
			}
			if(i < (int)ids.size())
				doorId = ids[i];
			from = to;
			headingIn = heading;
		}
	}
}

void Builder::CreateFlagPole(BTriList& out, const Vec3& at) const
{
	float s, t, u, v;
	BuildingTexture(BT_Pole, s, t, u, v);
	const float across = u - s;
	const float uMid = (s + u) * 0.5f;
	const double radius = 0.166666666667;
	float prevU = 0.0f;
	Vec3 prevN, prev;
	for(int i = 0; i < 9; i++)
	{
		const float part = (float)i * 0.125f;
		const float angle = part * 6.283185f;
		const Vec3 n((double)cosf(angle), (double)sinf(angle), 0.0);
		const float uCur = across * part + s;
		const Vec3 cur((double)(float)(n.x * radius), (double)(float)(n.y * radius), 0.0);
		if(i != 0)
		{
			const Vec3 curLow = Vec3(cur.x, cur.y, -0.5) + at, curHigh = Vec3(cur.x, cur.y, 31.0) + at;
			const Vec3 prevLow = Vec3(prev.x, prev.y, -0.5) + at, prevHigh = Vec3(prev.x, prev.y, 31.0) + at;
			BTri a;
			a.v[0] = BVertex(curLow, n, uCur, t);
			a.v[1] = BVertex(prevLow, prevN, prevU, t);
			a.v[2] = BVertex(prevHigh, prevN, prevU, v);
			out.push_back(a);
			a.v[0] = BVertex(prevHigh, prevN, prevU, v);
			a.v[1] = BVertex(curHigh, n, uCur, v);
			a.v[2] = BVertex(curLow, n, uCur, t);
			out.push_back(a);
			a.v[0] = BVertex(prevHigh, prevN, prevU, v);
			a.v[1] = BVertex(Vec3(0.0, 0.0, 32.0) + at, (prevN + n) * (1.0 / 2.0), uMid, t);
			a.v[2] = BVertex(curHigh, n, uCur, v);
			out.push_back(a);
		}
		prevU = uCur;
		prevN = n;
		prev = cur;
	}
}

void Builder::CreateStreetLight(BTriList& out, const Vec3& at) const
{
	float s, t, u, v;
	float ls, lt, lu, lv;
	BuildingTexture(BT_Pole, s, t, u, v);
	BuildingTexture(BT_Lamp, ls, lt, lu, lv);
	const Vec3 capNormal = Unit(Vec3(0.5, 0.0, 1.25));
	const Vec3 lampNormal = Unit(Vec3(1.0, 0.0, -0.95833337307));
	const Vec3 top = Vec3(0.0, 0.0, 26.5) + at;
	const float postRadius = 0.2916667f, lampRadius = 1.25f;
	float prevU = 0.0f;
	Vec3 prevFoot, prevN, prevNeck, prevRim, prevUnder, prevCap;
	for(int i = 0; i < 9; i++)
	{
		const float part = (float)i * 0.125f;
		const float angle = part * 6.283185f;
		const float fc = cosf(angle), fs = sinf(angle);
		const double c = (double)fc, sn = (double)fs;
		const Vec3 n(c, sn, 0.0);
		const Vec3 under(c * lampNormal.x - sn * lampNormal.y, c * lampNormal.y + sn * lampNormal.x, lampNormal.z);
		const Vec3 cap(c * capNormal.x - sn * capNormal.y, c * capNormal.y + sn * capNormal.x, capNormal.z);
		const Vec3 foot = Vec3((double)(fc * postRadius), (double)(fs * postRadius), 0.0) + at;
		const Vec3 neck = Vec3((double)(fc * postRadius), (double)(fs * postRadius), 25.0) + at;
		const Vec3 rim = Vec3((double)(fc * lampRadius), (double)(fs * lampRadius), 26.0) + at;
		const float uCur = (u - s) * part + s;
		if(i != 0)
		{
			BTri a;
			a.window = 3;
			a.v[0] = BVertex(prevFoot, prevN, prevU, t);
			a.v[1] = BVertex(prevNeck, prevN, prevU, v);
			a.v[2] = BVertex(neck, n, uCur, v);
			out.push_back(a);
			a.v[0] = BVertex(neck, n, uCur, v);
			a.v[1] = BVertex(foot, n, uCur, t);
			a.v[2] = BVertex(prevFoot, prevN, prevU, t);
			out.push_back(a);
			a.v[0] = BVertex(prevRim, prevCap, prevU, t);
			a.v[1] = BVertex(top, (prevCap + cap) * (1.0 / 2.0), (double)((uCur + prevU) * 0.5f), v);
			a.v[2] = BVertex(rim, cap, uCur, t);
			out.push_back(a);
			a.v[0] = BVertex(prevNeck, prevUnder, ls, lt);
			a.v[1] = BVertex(prevRim, prevUnder, ls, lv);
			a.v[2] = BVertex(rim, under, lu, lv);
			out.push_back(a);
			a.v[0] = BVertex(rim, under, lu, lv);
			a.v[1] = BVertex(neck, under, lu, lt);
			a.v[2] = BVertex(prevNeck, prevUnder, ls, lt);
			out.push_back(a);
		}
		prevU = uCur;
		prevFoot = foot; prevN = n; prevNeck = neck; prevRim = rim; prevUnder = under; prevCap = cap;
	}
}

}
