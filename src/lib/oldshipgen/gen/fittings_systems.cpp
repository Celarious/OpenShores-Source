#include "model.h"

namespace OldShip
{

namespace
{

struct Ring
{
	double wide, narrow;
};

const Ring DriveRing = { 1.41421356225, 0.5857864367071931 };
const Ring WideRing = { 1.4886458550000001, 0.6166173017970454 };
const Ring NarrowRing = { 0.9229604301000001, 0.3823027271141682 };
const Ring WideRingSingle = { 1.4886459112167358, 0.6166173219680786 };
const Ring NarrowRingSingle = { 0.9229604601860046, 0.38230273127555847 };

Vec3 Corner(const Ring& ring, int k, double z)
{
	static const struct { bool xWide; double signX, signY; } At[8] =
	{
		{ true, 1.0, -1.0 }, { true, 1.0, 1.0 }, { false, 1.0, 1.0 }, { false, -1.0, 1.0 },
		{ true, -1.0, 1.0 }, { true, -1.0, -1.0 }, { false, -1.0, -1.0 }, { false, 1.0, -1.0 },
	};
	const double x = At[k].xWide ? ring.wide : ring.narrow;
	const double y = At[k].xWide ? ring.narrow : ring.wide;
	return Vec3(At[k].signX * x, At[k].signY * y, z);
}

const double SideNormal[8][2] =
{
	{ 1.0, 0.0 }, { 0.7071, 0.7071 }, { 0.0, 1.0 }, { -0.7071, 0.7071 },
	{ -1.0, 0.0 }, { -0.7071, -0.7071 }, { 0.0, -1.0 }, { 0.7071, -0.7071 },
};

enum Place
{
	Low,
	InnerLow,
	InnerHigh,
	High,
	PlaceCount
};

struct FitTexture
{
	float u[PlaceCount], v[PlaceCount];
	float eu[PlaceCount], ev[PlaceCount];
};

void Spread(float* at, float low, float high, bool effect)
{
	const double middle = effect ? (double)((high - low) * 0.5f + low) : (double)(high - low) * 0.5 + (double)low;
	const double reach = (double)(high - low) * 0.382683432 * 0.5;
	at[Low] = low;
	at[InnerLow] = (float)(middle - reach);
	at[InnerHigh] = (float)(reach + middle);
	at[High] = high;
}

void SpreadSingle(float* at, float low, float high)
{
	const float reach = (high - low) * 0.38268343f * 0.5f;
	const float middle = (high - low) * 0.5f + low;
	at[Low] = low;
	at[InnerLow] = middle - reach;
	at[InnerHigh] = reach + middle;
	at[High] = high;
}

FitTexture MakeTexture(ShipTex tex, int effect, bool single)
{
	FitTexture tx;
	float s, t, u, v;
	ShipTexture(tex, s, t, u, v);
	if(single)
	{
		SpreadSingle(tx.u, s, u);
		SpreadSingle(tx.v, t, v);
	}
	else
	{
		Spread(tx.u, s, u, false);
		Spread(tx.v, t, v, false);
	}
	EffectTexture(effect, s, t, u, v);
	if(single)
	{
		SpreadSingle(tx.eu, s, u);
		SpreadSingle(tx.ev, t, v);
	}
	else
	{
		Spread(tx.eu, s, u, true);
		Spread(tx.ev, t, v, true);
	}
	return tx;
}

void AddWalls(TriList& out, const FitTexture& tx, const int* ints, const Vec3& at, const Ring& low, double zLow,
		const Ring& high, double zHigh)
{
	for(int k = 0; k < 8; k++)
	{
		const int next = (k + 1) & 7;
		const Vec3 normal(SideNormal[k][0], SideNormal[k][1], 0.0);
		ModelAdd(out, normal, ints, 0.0f, false, at, 0.0f,
				tx.u[Low], tx.v[Low], tx.eu[Low], tx.ev[Low], Corner(low, k, zLow),
				tx.u[Low], tx.v[High], tx.eu[Low], tx.ev[High], Corner(high, k, zHigh),
				tx.u[High], tx.v[High], tx.eu[High], tx.ev[High], Corner(high, next, zHigh),
				tx.u[High], tx.v[Low], tx.eu[High], tx.ev[Low], Corner(low, next, zLow));
	}
}

const Place CornerAcross[8] = { High, High, InnerHigh, InnerLow, Low, Low, InnerLow, InnerHigh };
const Place CornerUp[8] = { InnerLow, InnerHigh, High, High, InnerHigh, InnerLow, Low, Low };

struct CapVertex
{
	int		corner;
	Place	effectAcross, effectUp;
};

const CapVertex Caps[6][4] =
{
	{ { 4, Low, InnerHigh }, { 3, InnerLow, High }, { 2, InnerHigh, High }, { 1, High, InnerHigh } },
	{ { 1, High, InnerHigh }, { 0, High, InnerLow }, { 5, Low, InnerLow }, { 4, Low, InnerHigh } },
	{ { 0, High, InnerLow }, { 7, InnerHigh, Low }, { 6, InnerLow, Low }, { 5, Low, InnerLow } },
	{ { 3, InnerLow, High }, { 4, Low, InnerHigh }, { 1, High, InnerHigh }, { 2, InnerHigh, High } },
	{ { 0, High, InnerLow }, { 1, High, InnerHigh }, { 4, Low, InnerHigh }, { 5, Low, InnerLow } },
	{ { 7, InnerHigh, Low }, { 0, High, InnerLow }, { 5, Low, InnerLow }, { 6, InnerLow, Low } },
};

const CapVertex LifeSupportCaps[6][4] =
{
	{ { 4, Low, InnerHigh }, { 3, InnerLow, High }, { 2, InnerHigh, High }, { 1, High, InnerHigh } },
	{ { 1, Low, InnerHigh }, { 0, Low, InnerLow }, { 5, High, InnerLow }, { 4, High, InnerHigh } },
	{ { 0, Low, InnerLow }, { 7, InnerHigh, High }, { 6, InnerLow, High }, { 5, High, InnerLow } },
	{ { 3, InnerLow, Low }, { 4, Low, InnerHigh }, { 1, High, InnerHigh }, { 2, InnerHigh, Low } },
	{ { 0, Low, InnerLow }, { 1, Low, InnerHigh }, { 4, High, InnerHigh }, { 5, High, InnerLow } },
	{ { 7, InnerHigh, Low }, { 0, Low, InnerLow }, { 5, High, InnerLow }, { 6, InnerLow, Low } },
};

void AddCaps(TriList& out, const FitTexture& tx, const int* ints, const Vec3& at, const CapVertex caps[6][4],
		const Ring& top, double zTop, const Ring& bottom, double zBottom, const Vec3& bottomNormal)
{
	for(int q = 0; q < 6; q++)
	{
		const bool upper = q < 3;
		const Ring& ring = upper ? top : bottom;
		const double z = upper ? zTop : zBottom;
		const CapVertex* c = caps[q];
		ModelAdd(out, upper ? Vec3(0.0, 0.0, 1.0) : bottomNormal, ints, 0.0f, false, at, 0.0f,
				tx.u[CornerAcross[c[0].corner]], tx.v[CornerUp[c[0].corner]], tx.eu[c[0].effectAcross], tx.ev[c[0].effectUp], Corner(ring, c[0].corner, z),
				tx.u[CornerAcross[c[1].corner]], tx.v[CornerUp[c[1].corner]], tx.eu[c[1].effectAcross], tx.ev[c[1].effectUp], Corner(ring, c[1].corner, z),
				tx.u[CornerAcross[c[2].corner]], tx.v[CornerUp[c[2].corner]], tx.eu[c[2].effectAcross], tx.ev[c[2].effectUp], Corner(ring, c[2].corner, z),
				tx.u[CornerAcross[c[3].corner]], tx.v[CornerUp[c[3].corner]], tx.eu[c[3].effectAcross], tx.ev[c[3].effectUp], Corner(ring, c[3].corner, z));
	}
}

const Vec3 Down(0.0, 0.0, -1.0);

const int ManeuverDriveLight = 2;
const int LifeSupportLight = 4;
const int PowerPlantLight = 5;
const int SensorLight = 6;
const int ShieldLight = 7;
const int FTLDriveLight = 9;
const int FirstWeaponBayLight = 10;

}

void Builder::RenderDriveUnit(TriList& out, ShipTex tex, const Vec3& at) const
{
	int ints[3] = { 0, 0, FTLDriveLight };
	int effect = 0x37;
	if(tex == ST_GravityDrive)
	{
		ints[2] = ManeuverDriveLight;
		effect = 0x10;
	}
	else if(tex == ST_PowerPlant)
	{
		ints[2] = PowerPlantLight;
		effect = 0x14;
	}
	const FitTexture tx = MakeTexture(tex, effect, false);
	const double zLow = 0.1, zHigh = 7.9;

	AddWalls(out, tx, ints, at, DriveRing, zLow, DriveRing, zHigh);
	AddCaps(out, tx, ints, at, Caps, DriveRing, zHigh, DriveRing, zLow, Vec3((double)tx.u[Low], 0.0, -1.0));
}

void Builder::RenderLifeSupport(TriList& out, const Vec3& at) const
{
	const int ints[3] = { 0, 0, LifeSupportLight };
	const FitTexture tx = MakeTexture(ST_LifeSupport, 8, true);
	const double zLow = 0.10000000149011612, zBulge = 2.0250000953674316, zHigh = 7.800000190734863;

	AddWalls(out, tx, ints, at, WideRingSingle, zBulge, NarrowRingSingle, zHigh);
	AddWalls(out, tx, ints, at, NarrowRingSingle, zLow, WideRingSingle, zBulge);
	AddCaps(out, tx, ints, at, LifeSupportCaps, NarrowRingSingle, zHigh, NarrowRingSingle, zLow, Down);
}

void Builder::RenderShield(TriList& out, const Vec3& at) const
{
	const int ints[3] = { 0, 0, ShieldLight };
	const FitTexture tx = MakeTexture(ST_Shield, 0x1b, false);
	const double zLow = 0.1, zWaist = 3.9499999999999997, zHigh = 7.8;

	AddWalls(out, tx, ints, at, NarrowRing, zWaist, WideRing, zHigh);
	AddWalls(out, tx, ints, at, WideRing, zLow, NarrowRing, zWaist);
	AddCaps(out, tx, ints, at, Caps, WideRing, zHigh, WideRing, zLow, Down);
}

int Builder::WeaponBayIndex(int bay) const
{
	for(size_t i = 0; i < weaponBays.size(); i++)
		if(weaponBays[i] == bay)
			return (int)i;
	return -1;
}

void Builder::RenderWeapon(TriList& out, const Vec3& at, uint16_t roomId, const std::vector<Room>& rooms) const
{
	const FitTexture tx = MakeTexture(ST_WeaponBay, 0x33, false);
	int ints[3] = { 0, 0, AnimatedDoors() + FirstWeaponBayLight };
	for(size_t i = 0; i < rooms.size(); i++)
		if(rooms[i].id == roomId)
		{
			ints[2] += WeaponBayIndex(rooms[i].style);
			break;
		}
	const double zLow = 0.1, zMiddle = 3.9499999999999997, zHigh = 7.8;

	AddWalls(out, tx, ints, at, WideRing, zMiddle, NarrowRing, zHigh);
	AddWalls(out, tx, ints, at, NarrowRing, zLow, WideRing, zMiddle);
	AddCaps(out, tx, ints, at, Caps, NarrowRing, zHigh, NarrowRing, zLow, Down);
}

void Builder::RenderSensor(TriList& out, const Vec3& at) const
{
	const double h = LatticeUnit;
	const double zLow = 0.1, zHigh = 7.9;
	const int ints[3] = { 0, 0, SensorLight };
	const int noInts[3] = { 0, 0, 0 };

	float s, t, u, v;
	ShipTexture(ST_Sensor, s, t, u, v);
	float es, et, eu, ev;
	EffectTexture(0x18, es, et, eu, ev);

	ModelAdd(out, Vec3(1.0, 0.0, 0.0), ints, 0.0f, false, at, 0.0f,
			s, t, es, et, Vec3(h, -h, zLow),
			s, v, es, ev, Vec3(h, -h, zHigh),
			u, v, eu, ev, Vec3(h, h, zHigh),
			u, t, eu, et, Vec3(h, h, zLow));
	ModelAdd(out, Vec3(-1.0, 0.0, 0.0), ints, 0.0f, false, at, 0.0f,
			u, t, eu, et, Vec3(-h, -h, zLow),
			s, t, es, et, Vec3(-h, h, zLow),
			s, v, es, ev, Vec3(-h, h, zHigh),
			u, v, eu, ev, Vec3(-h, -h, zHigh));
	ModelAdd(out, Vec3(0.0, -1.0, 0.0), ints, 0.0f, false, at, 0.0f,
			s, t, es, et, Vec3(-h, -h, zLow),
			s, v, es, ev, Vec3(-h, -h, zHigh),
			u, v, eu, ev, Vec3(h, -h, zHigh),
			u, t, eu, et, Vec3(h, -h, zLow));
	ModelAdd(out, Vec3(0.0, 1.0, 0.0), ints, 0.0f, false, at, 0.0f,
			u, t, eu, et, Vec3(-h, h, zLow),
			s, t, es, et, Vec3(h, h, zLow),
			s, v, es, ev, Vec3(h, h, zHigh),
			u, v, eu, ev, Vec3(-h, h, zHigh));

	ShipTexture(ST_SensorTop, s, t, u, v);
	ModelAdd(out, Vec3(0.0, 0.0, 1.0), noInts, 0.0f, false, at, 0.0f,
			s, t, -1.0f, -1.0f, Vec3(-h, -h, zHigh),
			s, v, -1.0f, -1.0f, Vec3(-h, h, zHigh),
			u, v, -1.0f, -1.0f, Vec3(h, h, zHigh),
			u, t, -1.0f, -1.0f, Vec3(h, -h, zHigh));
	ModelAdd(out, Down, noInts, 0.0f, false, at, 0.0f,
			s, t, -1.0f, -1.0f, Vec3(-h, h, zLow),
			s, v, -1.0f, -1.0f, Vec3(-h, -h, zLow),
			u, v, -1.0f, -1.0f, Vec3(h, -h, zLow),
			u, t, -1.0f, -1.0f, Vec3(h, h, zLow));
}

}
