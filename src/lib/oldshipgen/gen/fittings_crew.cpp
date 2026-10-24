#include "model.h"

namespace OldShip
{

namespace
{

const int NoEffect[3] = { 0, 0, 0 };
const float NoEffectUV = -1.0f;

const double BunkLength = 5.656854249;
const double BunkWidth = 2.8284271245;
const double BunkThickness = 1.0;
const double BunkLift = 0.5;
const double UpperBunkLift = 3.5;
const int BunkInts[3] = { 0, 0, 1 };
const int BunkEffect = 0;

void RenderBunk(TriList& out, const Vec3& corner, double rot)
{
	float s, t, right, top;
	ShipTexture(ST_Berth, s, t, right, top);
	float bs, bt, bright, btop;
	ShipTexture(ST_BerthBottom, bs, bt, bright, btop);
	float es, et, eright, etop;
	EffectTexture(BunkEffect, es, et, eright, etop);

	const float texWidth = right - s, texHeight = top - t;
	const float effWidth = eright - es, effHeight = etop - et;
	const float turn = (float)rot;

	const float uEnd0 = (float)((double)texWidth * 0.0 + (double)s);
	const float uEnd1 = (float)((double)texWidth * 0.2421875 + (double)s);
	const float uSide0 = (float)((double)texWidth * 0.2578125 + (double)s);
	const float uSide1 = (float)((double)texWidth * 0.9921875 + (double)s);
	const float euEnd0 = (float)((double)effWidth * 0.0 + (double)es);
	const float euEnd1 = (float)((double)effWidth * 0.2421875 + (double)es);
	const float euSide0 = (float)((double)effWidth * 0.2578125 + (double)es);
	const float euSide1 = (float)((double)effWidth * 0.9921875 + (double)es);
	const float vLow0 = texHeight * 0.0f + t;
	const float vLow1 = (float)((double)texHeight * 0.46875 + (double)t);
	const float vHigh0 = (float)((double)texHeight * 0.5 + (double)t);
	const float vHigh1 = (float)((double)texHeight * 0.96875 + (double)t);
	const float evLow0 = effHeight * 0.0f + et;
	const float evLow1 = (float)((double)effHeight * 0.46875 + (double)et);
	const float evHigh0 = (float)((double)effHeight * 0.5 + (double)et);
	const float evHigh1 = (float)((double)effHeight * 0.96875 + (double)et);

	const double L = BunkLength, W = BunkWidth, H = BunkThickness;

	ModelAdd(out, Vec3(1.0, 0.0, 0.0), BunkInts, 0.0f, false, corner, turn,
			uEnd0, vHigh0, euEnd0, evHigh0, Vec3(L, 0.0, 0.0),
			uEnd0, vHigh1, euEnd0, evHigh1, Vec3(L, 0.0, H),
			uEnd1, vHigh1, euEnd1, evHigh1, Vec3(L, W, H),
			uEnd1, vHigh0, euEnd1, evHigh0, Vec3(L, W, 0.0));
	ModelAdd(out, Vec3(-1.0, 0.0, 0.0), BunkInts, 0.0f, false, corner, turn,
			uEnd0, vLow0, euEnd0, evLow0, Vec3(0.0, W, 0.0),
			uEnd0, vLow1, euEnd0, evLow1, Vec3(0.0, W, H),
			uEnd1, vLow1, euEnd1, evLow1, Vec3(0.0, 0.0, H),
			uEnd1, vLow0, euEnd1, evLow0, Vec3(0.0, 0.0, 0.0));
	ModelAdd(out, Vec3(0.0, 1.0, 0.0), BunkInts, 0.0f, false, corner, turn,
			uSide1, vLow0, euSide1, evLow0, Vec3(L, W, 0.0),
			uSide1, vLow1, euSide1, evLow1, Vec3(L, W, H),
			uSide0, vLow1, euSide0, evLow1, Vec3(0.0, W, H),
			uSide0, vLow0, euSide0, evLow0, Vec3(0.0, W, 0.0));
	ModelAdd(out, Vec3(0.0, -1.0, 0.0), BunkInts, 0.0f, false, corner, turn,
			uSide0, vLow0, euSide0, evLow0, Vec3(0.0, 0.0, 0.0),
			uSide0, vLow1, euSide0, evLow1, Vec3(0.0, 0.0, H),
			uSide1, vLow1, euSide1, evLow1, Vec3(L, 0.0, H),
			uSide1, vLow0, euSide1, evLow0, Vec3(L, 0.0, 0.0));
	ModelAdd(out, Vec3(0.0, 0.0, 1.0), NoEffect, 0.0f, false, corner, turn,
			uSide0, vHigh0, NoEffectUV, NoEffectUV, Vec3(0.0, 0.0, H),
			uSide0, vHigh1, NoEffectUV, NoEffectUV, Vec3(0.0, W, H),
			uSide1, vHigh1, NoEffectUV, NoEffectUV, Vec3(L, W, H),
			uSide1, vHigh0, NoEffectUV, NoEffectUV, Vec3(L, 0.0, H));
	ModelAdd(out, Vec3(0.0, 0.0, -1.0), NoEffect, 0.0f, false, corner, turn,
			bs, bt, NoEffectUV, NoEffectUV, Vec3(0.0, W, 0.0),
			bs, btop, NoEffectUV, NoEffectUV, Vec3(0.0, 0.0, 0.0),
			bright, btop, NoEffectUV, NoEffectUV, Vec3(L, 0.0, 0.0),
			bright, bt, NoEffectUV, NoEffectUV, Vec3(L, W, 0.0));
}

Vec3 BunkCorner(const Vec3& at, double rot)
{
	const Vec3 offset = RotateZ(Vec3(-1.41421356225, -1.41421356225, BunkLift), rot);
	return Vec3(at.x + offset.x, at.y + offset.y, at.z + offset.z);
}

const double LadderReach = 1.834;
const double LadderHalfWidth = 0.9428090415;
const double PoleHalfWidth = 0.15;
const double PoleOffset = 0.780763737492;

const double PadFar = 1.34687005929;
const double PadNear = 0.557891844483;
const double PadBottom = 0.0416666666667;
const double PadTop = 0.166666666667;
const int PadEffect = 0x2f;
const int FirstAnimation = 10;
const double OctagonCut = 0.382683432;

const double SurgeryFar = 2.2627416996;
const double SurgeryNear = 0.937258298732;
const double SurgeryHeight = 8.0;
const int SurgeryEffect = 0x28;
const int SurgeryInts[3] = { 0, 0, 8 };

const double Diagonal = 0.7071;

struct Octagon
{
	Vec3 corner[9];
	Vec3 normal[8];

	Octagon(double farOut, double nearOut)
	{
		const double xy[8][2] =
		{
			{ farOut, -nearOut }, { farOut, nearOut }, { nearOut, farOut }, { -nearOut, farOut },
			{ -farOut, nearOut }, { -farOut, -nearOut }, { -nearOut, -farOut }, { nearOut, -farOut },
		};
		const double n[8][2] =
		{
			{ 1.0, 0.0 }, { Diagonal, Diagonal }, { 0.0, 1.0 }, { -Diagonal, Diagonal },
			{ -1.0, 0.0 }, { -Diagonal, -Diagonal }, { 0.0, -1.0 }, { Diagonal, -Diagonal },
		};
		for(int i = 0; i < 8; i++)
		{
			corner[i] = Vec3(xy[i][0], xy[i][1], 0.0);
			normal[i] = Vec3(n[i][0], n[i][1], 0.0);
		}
		corner[8] = corner[0];
	}
};

inline Vec3 AtHeight(const Vec3& p, double z)
{
	return Vec3(p.x, p.y, z);
}

}

void Builder::RenderBerthSingle(TriList& out, const Vec3& at, double rot) const
{
	RenderBunk(out, BunkCorner(at, rot), rot);
}

void Builder::RenderBerthDouble(TriList& out, const Vec3& at, double rot) const
{
	const Vec3 lower = BunkCorner(at, rot);
	RenderBunk(out, lower, rot);
	RenderBunk(out, Vec3(lower.x + 0.0, lower.y + 0.0, lower.z + UpperBunkLift), rot);
}

void Builder::RenderLadder(TriList& out, const Vec3& at, float rot) const
{
	const double turn = (double)rot;
	const Vec3 reach = RotateZ(Vec3(LadderReach, 0.0, 0.0), turn);
	const Vec3 origin = Vec3(at.x + reach.x, at.y + reach.y, at.z + reach.z);

	float s, t, right, top;
	ShipTexture(ST_Ladder, s, t, right, top);
	float ps, pt, pright, ptop;
	ShipTexture(ST_LadderPole, ps, pt, pright, ptop);

	const double down = -DeckHeight;
	const float none = NoEffectUV;

	Vec3 normal = RotateZ(Vec3(1.0, 0.0, 0.0), turn);
	ModelAdd(out, normal, NoEffect, 0.0f, false, origin, rot,
			s, t, none, none, Vec3(0.0, -LadderHalfWidth, down),
			s, top, none, none, Vec3(0.0, -LadderHalfWidth, 0.0),
			right, top, none, none, Vec3(0.0, LadderHalfWidth, 0.0),
			right, t, none, none, Vec3(0.0, LadderHalfWidth, down));
	normal = Vec3(-normal.x, -normal.y, -normal.z);
	ModelAdd(out, normal, NoEffect, 0.0f, false, origin, rot,
			right, t, none, none, Vec3(0.0, LadderHalfWidth, down),
			right, top, none, none, Vec3(0.0, LadderHalfWidth, 0.0),
			s, top, none, none, Vec3(0.0, -LadderHalfWidth, 0.0),
			s, t, none, none, Vec3(0.0, -LadderHalfWidth, down));

	normal = RotateZ(Vec3(0.0, 1.0, 0.0), turn);
	ModelAdd(out, normal, NoEffect, 0.0f, false, origin, rot,
			ps, pt, none, none, Vec3(PoleHalfWidth, -PoleOffset, down),
			ps, ptop, none, none, Vec3(PoleHalfWidth, -PoleOffset, 0.0),
			pright, ptop, none, none, Vec3(-PoleHalfWidth, -PoleOffset, 0.0),
			pright, pt, none, none, Vec3(-PoleHalfWidth, -PoleOffset, down));
	normal = Vec3(-normal.x, -normal.y, -normal.z);
	ModelAdd(out, normal, NoEffect, 0.0f, false, origin, rot,
			pright, pt, none, none, Vec3(-PoleHalfWidth, -PoleOffset, down),
			pright, ptop, none, none, Vec3(-PoleHalfWidth, -PoleOffset, 0.0),
			ps, ptop, none, none, Vec3(PoleHalfWidth, -PoleOffset, 0.0),
			ps, pt, none, none, Vec3(PoleHalfWidth, -PoleOffset, down));
	ModelAdd(out, normal, NoEffect, 0.0f, false, origin, rot,
			pright, pt, none, none, Vec3(-PoleHalfWidth, PoleOffset, down),
			pright, ptop, none, none, Vec3(-PoleHalfWidth, PoleOffset, 0.0),
			ps, ptop, none, none, Vec3(PoleHalfWidth, PoleOffset, 0.0),
			ps, pt, none, none, Vec3(PoleHalfWidth, PoleOffset, down));
	normal = Vec3(-normal.x, -normal.y, -normal.z);
	ModelAdd(out, normal, NoEffect, 0.0f, false, origin, rot,
			ps, pt, none, none, Vec3(PoleHalfWidth, PoleOffset, down),
			ps, ptop, none, none, Vec3(PoleHalfWidth, PoleOffset, 0.0),
			pright, ptop, none, none, Vec3(-PoleHalfWidth, PoleOffset, 0.0),
			pright, pt, none, none, Vec3(-PoleHalfWidth, PoleOffset, down));
}

void Builder::RenderTransporterPad(TriList& out, const Vec3& at, uint16_t roomId, const std::vector<Room>& rooms) const
{
	float s, t, right, top;
	ShipTexture(ST_TransporterPad, s, t, right, top);
	const double uMid = (double)(right - s) * 0.5 + (double)s;
	const double uHalfSide = (double)(right - s) * OctagonCut * 0.5;
	const double vMid = (double)(top - t) * 0.5 + (double)t;
	const double vHalfSide = (double)(top - t) * OctagonCut * 0.5;
	const float uHigh = (float)(uHalfSide + uMid), uLow = (float)(uMid - uHalfSide);
	const float vHigh = (float)(vHalfSide + vMid), vLow = (float)(vMid - vHalfSide);

	float es, et, eright, etop;
	EffectTexture(PadEffect, es, et, eright, etop);
	const double euHalfSide = (double)(eright - es) * OctagonCut * 0.5;
	const double euMid = (double)((eright - es) * 0.5f + es);
	const float euHigh = (float)(euHalfSide + euMid), euLow = (float)(euMid - euHalfSide);
	const double evHalfSide = (double)(etop - et) * OctagonCut * 0.5;
	const double evMid = (double)((etop - et) * 0.5f + et);
	const float evHigh = (float)(evHalfSide + evMid), evLow = (float)(evMid - evHalfSide);

	int ints[3] = { 0, 0, (int)weaponBays.size() + FirstAnimation + AnimatedDoors() };
	for(size_t i = 0; i < rooms.size(); i++)
	{
		const Room& room = rooms[i];
		if((uint16_t)room.id == roomId)
		{
			ints[2] += TransporterSystemIndex((uint16_t)(((room.deck & 0xff) << 8) | (room.style & 0xff)));
			break;
		}
	}

	const Octagon oct(PadFar, PadNear);
	for(int i = 0; i < 8; i++)
	{
		const Vec3& from = oct.corner[i];
		const Vec3& to = oct.corner[i + 1];
		ModelAdd(out, oct.normal[i], ints, 0.0f, false, at, 0.0f,
				s, t, es, et, AtHeight(from, PadBottom),
				s, top, es, etop, AtHeight(from, PadTop),
				right, top, eright, etop, AtHeight(to, PadTop),
				right, t, eright, et, AtHeight(to, PadBottom));
	}

	const Vec3 up(0.0, 0.0, 1.0);
	const Vec3* c = oct.corner;
	ModelAdd(out, up, ints, 0.0f, false, at, 0.0f,
			s, vHigh, es, evHigh, AtHeight(c[4], PadTop),
			uLow, top, euLow, etop, AtHeight(c[3], PadTop),
			uHigh, top, euHigh, etop, AtHeight(c[2], PadTop),
			right, vHigh, eright, evHigh, AtHeight(c[1], PadTop));
	ModelAdd(out, up, ints, 0.0f, false, at, 0.0f,
			right, vHigh, eright, evHigh, AtHeight(c[1], PadTop),
			right, vLow, eright, evLow, AtHeight(c[0], PadTop),
			s, vLow, es, evLow, AtHeight(c[5], PadTop),
			s, vHigh, es, evHigh, AtHeight(c[4], PadTop));
	ModelAdd(out, up, ints, 0.0f, false, at, 0.0f,
			right, vLow, eright, evLow, AtHeight(c[0], PadTop),
			uHigh, t, euHigh, et, AtHeight(c[7], PadTop),
			uLow, t, euLow, et, AtHeight(c[6], PadTop),
			s, vLow, es, evLow, AtHeight(c[5], PadTop));
}

void Builder::RenderSurgeryUnit(TriList& out, const Vec3& at) const
{
	float s, t, right, top;
	ShipTexture(ST_Surgery, s, t, right, top);
	float es, et, eright, etop;
	EffectTexture(SurgeryEffect, es, et, eright, etop);

	const Octagon oct(SurgeryFar, SurgeryNear);
	const double h = SurgeryHeight;
	for(int i = 0; i < 8; i++)
	{
		const Vec3& from = oct.corner[i];
		const Vec3& to = oct.corner[i + 1];
		ModelAdd(out, oct.normal[i], SurgeryInts, 0.0f, false, at, 0.0f,
				s, t, es, et, AtHeight(from, 0.0),
				s, top, es, etop, AtHeight(from, h),
				right, top, eright, etop, AtHeight(to, h),
				right, t, eright, et, AtHeight(to, 0.0));
	}
	for(int i = 0; i < 8; i++)
	{
		const Vec3& from = oct.corner[i];
		const Vec3& to = oct.corner[i + 1];
		const Vec3 inward(-oct.normal[i].x, -oct.normal[i].y, 0.0);
		ModelAdd(out, inward, SurgeryInts, 0.0f, false, at, 0.0f,
				s, top, es, etop, AtHeight(from, h),
				s, t, es, et, AtHeight(from, 0.0),
				right, t, eright, et, AtHeight(to, 0.0),
				right, top, eright, etop, AtHeight(to, h));
	}
}

}
