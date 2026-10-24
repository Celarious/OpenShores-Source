#include "model.h"

#include <cmath>

namespace OldShip
{

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
	if(fabs(dx) <= Tiny)
		dx = 0.0;
	if(fabs(dy) <= Tiny)
		dy = 0.0;
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

const double BallRim = GridUnit;
const Vec3 Ball[13] =
{
	Vec3(BallRim, 0.0, 0.0), Vec3(2.0, 0.0, -2.0), Vec3(0.0, 0.0, -BallRim), Vec3(-2.0, 0.0, -2.0),
	Vec3(-BallRim, 0.0, 0.0), Vec3(-2.0, 0.0, 2.0), Vec3(0.0, 0.0, BallRim), Vec3(2.0, 0.0, 2.0),
	Vec3(2.0, 2.0, 0.0), Vec3(0.0, 2.0, -2.0), Vec3(-2.0, 2.0, 0.0), Vec3(0.0, 2.0, 2.0),
	Vec3(0.0, BallRim, 0.0),
};

const int BallFaces[16][3] =
{
	{ 8, 12, 9 }, { 1, 8, 9 }, { 8, 1, 0 }, { 8, 0, 7 },
	{ 9, 12, 10 }, { 3, 9, 10 }, { 9, 3, 2 }, { 9, 2, 1 },
	{ 10, 12, 11 }, { 5, 10, 11 }, { 10, 5, 4 }, { 10, 4, 3 },
	{ 11, 12, 8 }, { 7, 11, 8 }, { 11, 7, 6 }, { 11, 6, 5 },
};

const float BallTexMiddle = 0.6767767f;

}

void Builder::RenderExteriorWalls(TriList& out, ShipTex tex, WallType wall, const std::vector<Outline>& outlines, bool inside) const
{
	float s, t, u, v;
	ShipTexture(tex, s, t, u, v);
	int ints[3] = { 0, 0, inside ? 0 : 3 };
	const float shine = inside ? 0.0f : 50.0f;
	float es, et, eu, ev;
	EffectTexture(0x1f, es, et, eu, ev);
	const float uMid = (u - s) * BallTexMiddle + s;
	const float vMid = (v - t) * BallTexMiddle + t;
	const float euMid = (eu - es) * BallTexMiddle + es;
	const float evMid = (ev - et) * BallTexMiddle + et;
	int door = 0;

	for(size_t o = 0; o < outlines.size(); o++)
	{
		const std::vector<Vec3>& points = outlines[o].points;
		const std::vector<uint16_t>& segs = outlines[o].segs;
		const std::vector<uint32_t>& ids = outlines[o].ids;
		const int count = (int)points.size();
		if(count < 2 || (int)segs.size() < count || (int)ids.size() < count)
			continue;

		uint16_t seg = segs[count - 1];
		uint32_t id = ids[count - 1];
		std::vector<Vec3> profile;
		std::vector<int> runs;
		WallProfile((seg >> 8) & 0x1f, m.armor, profile, runs);
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

			if(id != 0 && (wall == WT_LargeDoor || wall == WT_Door))
			{
				const int index = DoorIndexDesignLocked((int)id);
				if(index >= 0)
				{
					const int animated = AnimatedDoors();
					door = index < animated ? index + 10 : animated + 9;
					ints[0] = door;
				}
			}

			const Vec3 along = Unit(step);
			double lowOut = 0.0, lowUp = 0.0;
			int run = WT_None;
			for(int k = 0; k < (int)profile.size(); k++)
			{
				if(k != 0 && run == wall)
				{
					const Vec3 fromLow = Place(from, mitreFrom, lowOut, lowUp);
					const Vec3 fromHigh = Place(from, mitreFrom, profile[k].x, profile[k].z);
					const Vec3 toLow = Place(to, mitreTo, lowOut, lowUp);
					const Vec3 toHigh = Place(to, mitreTo, profile[k].x, profile[k].z);
					const Vec3 normal = Unit(Cross(Unit(toHigh - toLow), along));
					if(wall == WT_Turret)
					{
						const Vec3 middle = (toHigh + fromLow) * (1.0 / 2.0);
						Vec3 ball[13];
						for(int b = 0; b < 13; b++)
							ball[b] = RotateZ(Ball[b], HeadingAngle[heading]) + middle;
						ModelAdd(out, normal, ints, shine, inside, u, v, eu, ev, toLow,
								u, t, eu, et, ball[0], uMid, vMid, euMid, evMid, ball[1]);
						ModelAdd(out, normal, ints, shine, inside, u, v, eu, ev, toLow,
								uMid, vMid, euMid, evMid, ball[1], s, v, es, ev, ball[2]);
						ModelAdd(out, normal, ints, shine, inside, u, v, eu, ev, toHigh,
								u, t, eu, et, ball[6], uMid, vMid, euMid, evMid, ball[7]);
						ModelAdd(out, normal, ints, shine, inside, u, v, eu, ev, toHigh,
								uMid, vMid, euMid, evMid, ball[7], s, v, es, ev, ball[0]);
						ModelAdd(out, normal, ints, shine, inside, u, v, eu, ev, fromHigh,
								u, t, eu, et, ball[4], uMid, vMid, euMid, evMid, ball[5]);
						ModelAdd(out, normal, ints, shine, inside, u, v, eu, ev, fromHigh,
								uMid, vMid, euMid, evMid, ball[5], s, v, es, ev, ball[6]);
						ModelAdd(out, normal, ints, shine, inside, u, v, eu, ev, fromLow,
								u, t, eu, et, ball[2], uMid, vMid, euMid, evMid, ball[3]);
						ModelAdd(out, normal, ints, shine, inside, u, v, eu, ev, fromLow,
								uMid, vMid, euMid, evMid, ball[3], s, v, es, ev, ball[4]);
						if(Distance2D(fromHigh, toHigh) - 5.656854249 > Tiny)
						{
							ModelAdd(out, normal, ints, shine, inside, s, v, es, ev, fromLow,
									u, v, eu, ev, ball[4], u, t, eu, et, fromHigh);
							ModelAdd(out, normal, ints, shine, inside, s, v, es, ev, toHigh,
									u, v, eu, ev, ball[0], u, t, eu, et, toLow);
						}
						for(int f = 0; f < 16; f++)
						{
							const Vec3& a = ball[BallFaces[f][0]];
							const Vec3& b = ball[BallFaces[f][1]];
							const Vec3& c = ball[BallFaces[f][2]];
							const Vec3 faceNormal = Unit(Cross(Unit(c - a), Unit(b - a)));
							ModelAdd(out, faceNormal, ints, shine, inside, s, t, es, et, a, s, v, es, ev, b, u, t, eu, et, c);
						}
					}
					else
					{
						ModelAdd(out, normal, ints, shine, inside, s, t, es, et, toLow, s, v, es, ev, toHigh, u, v, eu, ev, fromHigh);
						ModelAdd(out, normal, ints, shine, inside, u, v, eu, ev, fromHigh, u, t, eu, et, fromLow, s, t, es, et, toLow);
					}
				}
				lowOut = profile[k].x;
				lowUp = profile[k].z;
				run = k < (int)runs.size() ? runs[k] : WT_None;
			}

			if(id != 0)
			{
				if(wall == WT_LargeDoor || wall == WT_Door)
					door = 0;
				ints[0] = door;
			}

			if(seg != segs[i])
			{
				if(wall == WT_Plate)
					RenderWallEndCap(out, profile, mitreTo, to, false, inside);
				const bool otherKind = ((segs[i] ^ seg) & FloorBuffer::TypeMask) != 0;
				seg = segs[i];
				if(otherKind)
					WallProfile((seg >> 8) & 0x1f, m.armor, profile, runs);
				if(wall == WT_Plate)
					RenderWallEndCap(out, profile, mitreTo, to, true, inside);
			}
			from = to;
			headingIn = heading;
			id = ids[i];
		}
	}
}

void Builder::RenderWallEndCap(TriList& out, const std::vector<Vec3>& profile, const Vec3& mitre, const Vec3& corner,
		bool start, bool inside) const
{
	const int count = (int)profile.size();
	if(count <= 2)
		return;
	const int ints[3] = { 0, 0, inside ? 0 : 3 };
	const float shine = inside ? 0.0f : 50.0f;
	float s, t, u, v;
	ShipTexture(inside ? ST_IntWallEndCap : ST_HullWall, s, t, u, v);

	Vec3 hub = Place(corner, mitre, profile[0].x, profile[0].z);
	Vec3 next = Place(corner, mitre, profile[1].x, profile[1].z);
	Vec3 outward = Unit(next - hub);
	int first = 1;
	if(fabs(outward.z) == 1.0)
	{
		hub = next;
		next = Place(corner, mitre, profile[2].x, profile[2].z);
		outward = Unit(next - hub);
		first = 2;
		if(fabs(outward.z) == 1.0)
			return;
	}
	Vec3 normal = Unit(Cross(outward, Vec3(0.0, 0.0, 1.0)));

	const double Skin = 0.0208333333333;
	Vec3 proud;
	if(inside)
	{
		const Vec3 n = (start ? normal : normal * -1.0) * 1.0;
		proud = Vec3(n.x / 48.0, n.y / 48.0, n.z / 48.0);
	}
	if(start)
		normal = normal * -1.0;

	std::vector<Vec3> fan;
	std::vector<float> fanU, fanV;
	fan.push_back(hub);
	fanU.push_back(s);
	fanV.push_back(t);
	for(int n = first; n < count; n++)
	{
		const int k = start ? n : count - 1 - (n - first);
		double reach = profile[k].x;
		if(inside && reach != 0.0)
			reach -= Skin;
		Vec3 p = Place(corner, mitre, reach, profile[k].z);
		double across = (double)s;
		if(profile[k].x != 0.0)
		{
			across = (double)u;
			if(inside)
				p = p + proud;
		}
		fan.push_back(p);
		fanU.push_back((float)across);
		fanV.push_back((float)(profile[k].z * 0.125 * (double)(v - t) + (double)t));
	}

	if(fan.size() <= 2)
		return;
	for(size_t k = 2; k < fan.size(); k++)
		ModelAdd(out, normal, ints, shine, inside, fanU[0], fanV[0], -1.0f, -1.0f, fan[0],
				fanU[k - 1], fanV[k - 1], -1.0f, -1.0f, fan[k - 1], fanU[k], fanV[k], -1.0f, -1.0f, fan[k]);
}

}
