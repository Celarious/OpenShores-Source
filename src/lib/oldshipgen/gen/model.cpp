#include "model.h"

#include <cmath>

namespace OldShip
{

namespace
{
thread_local TextureSource textureSource = 0;
}

TextureSource SetTextureSource(TextureSource source)
{
	const TextureSource before = textureSource;
	textureSource = source;
	return before;
}

TextureSource CurrentTextureSource()
{
	return textureSource;
}

Vec3 RotateZ(const Vec3& p, double radians)
{
	const double c = cos(radians);
	const double s = sin(radians);
	return Vec3(p.x * c - p.y * s, p.x * s + p.y * c, p.z);
}

bool FuzzyEquals(const Vec3& a, const Vec3& b)
{
	return fabs(a.x - b.x) <= 1e-5 && fabs(a.y - b.y) <= 1e-5 && fabs(a.z - b.z) <= 1e-5;
}

double Distance(const Vec3& a, const Vec3& b)
{
	const Vec3 d = a - b;
	return sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

double Distance2D(const Vec3& a, const Vec3& b)
{
	const double dx = a.x - b.x;
	const double dy = a.y - b.y;
	return sqrt(dx * dx + dy * dy);
}

Vec3 Unit(const Vec3& v)
{
	const double len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	if(len == 0.0)
		return Vec3(1.0, v.y, v.z);
	return Vec3(v.x / len, v.y / len, v.z / len);
}

Vec3 Cross(const Vec3& a, const Vec3& b)
{
	return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

bool IsOnSegment(const Vec3& p, const Vec3& a, const Vec3& b)
{
	const Vec3 ab = b - a;
	const double len2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
	double off;
	if(len2 == 0.0)
		off = Distance(p, a);
	else
	{
		const Vec3 c = Cross(p - a, ab);
		off = sqrt(c.x * c.x + c.y * c.y + c.z * c.z) / sqrt(len2);
	}
	if(off > 1e-5)
		return false;
	if(FuzzyEquals(p, a) || FuzzyEquals(p, b))
		return true;
	if(len2 == 0.0)
		return false;
	const double len = sqrt(len2);
	const double along = (p.x - a.x) * (ab.x / len) + (p.y - a.y) * (ab.y / len) + (p.z - a.z) * (ab.z / len);
	return along >= 0.0 && along <= len;
}

int ClosestDeck(double z)
{
	return (int)lround(z / DeckHeight);
}

namespace
{

const int NoInts[3] = { 0, 0, 0 };

inline Vertex Make(const Vec3& p, float u, float v, const Vec3& n, float shine, float eu, float ev, const int* ints)
{
	Vertex x;
	x.pos = p;
	x.u = u; x.v = v;
	x.normal = n;
	x.shine = shine;
	x.eu = eu; x.ev = ev;
	x.ints[0] = ints[0]; x.ints[1] = ints[1]; x.ints[2] = ints[2];
	return x;
}

inline void Add(TriList& out, const Vertex& a, const Vertex& b, const Vertex& c)
{
	Tri t;
	t.v[0] = a; t.v[1] = b; t.v[2] = c;
	out.push_back(t);
}

}

void ModelAdd(TriList& out, const Vec3& normal, const int* ints, float shine, bool flip,
		float u0, float v0, float eu0, float ev0, const Vec3& p0,
		float u1, float v1, float eu1, float ev1, const Vec3& p1,
		float u2, float v2, float eu2, float ev2, const Vec3& p2)
{
	if(!ints)
		ints = NoInts;
	if(!flip)
		Add(out, Make(p0, u0, v0, normal, shine, eu0, ev0, ints), Make(p1, u1, v1, normal, shine, eu1, ev1, ints),
				Make(p2, u2, v2, normal, shine, eu2, ev2, ints));
	else
	{
		const Vec3 n = normal * -1.0;
		Add(out, Make(p0, u0, v0, n, shine, eu0, ev0, ints), Make(p2, u2, v2, n, shine, eu2, ev2, ints),
				Make(p1, u1, v1, n, shine, eu1, ev1, ints));
	}
}

void ModelAdd(TriList& out, const Vec3& normal, const int* ints, float shine, bool flip,
		float u0, float v0, float eu0, float ev0, const Vec3& p0,
		float u1, float v1, float eu1, float ev1, const Vec3& p1,
		float u2, float v2, float eu2, float ev2, const Vec3& p2,
		float u3, float v3, float eu3, float ev3, const Vec3& p3)
{
	if(!ints)
		ints = NoInts;
	ModelAdd(out, normal, ints, shine, flip, u0, v0, eu0, ev0, p0, u1, v1, eu1, ev1, p1, u2, v2, eu2, ev2, p2);
	if(!flip)
		Add(out, Make(p2, u2, v2, normal, shine, eu2, ev2, ints), Make(p3, u3, v3, normal, shine, eu3, ev3, ints),
				Make(p0, u0, v0, normal, shine, eu0, ev0, ints));
	else
	{
		const Vec3 n = normal * -1.0;
		Add(out, Make(p2, u2, v2, n, shine, eu2, ev2, ints), Make(p0, u0, v0, n, shine, eu0, ev0, ints),
				Make(p3, u3, v3, n, shine, eu3, ev3, ints));
	}
}

void ModelAdd(TriList& out, const Vec3& normal, const int* ints, float shine, bool flip, const Vec3& origin, float rot,
		float u0, float v0, float eu0, float ev0, const Vec3& p0,
		float u1, float v1, float eu1, float ev1, const Vec3& p1,
		float u2, float v2, float eu2, float ev2, const Vec3& p2)
{
	ModelAdd(out, normal, ints, shine, flip,
			u0, v0, eu0, ev0, RotateZ(p0, rot) + origin,
			u1, v1, eu1, ev1, RotateZ(p1, rot) + origin,
			u2, v2, eu2, ev2, RotateZ(p2, rot) + origin);
}

void ModelAdd(TriList& out, const Vec3& normal, const int* ints, float shine, bool flip, const Vec3& origin, float rot,
		float u0, float v0, float eu0, float ev0, const Vec3& p0,
		float u1, float v1, float eu1, float ev1, const Vec3& p1,
		float u2, float v2, float eu2, float ev2, const Vec3& p2,
		float u3, float v3, float eu3, float ev3, const Vec3& p3)
{
	ModelAdd(out, normal, ints, shine, flip,
			u0, v0, eu0, ev0, RotateZ(p0, rot) + origin,
			u1, v1, eu1, ev1, RotateZ(p1, rot) + origin,
			u2, v2, eu2, ev2, RotateZ(p2, rot) + origin,
			u3, v3, eu3, ev3, RotateZ(p3, rot) + origin);
}

void ModelAdd(TriList& out, const Vec3& normal, const Vec3& origin, float rot,
		const Vec3& p0, float u0, float v0, const Vec3& p1, float u1, float v1, const Vec3& p2, float u2, float v2)
{
	Add(out, Make(origin + RotateZ(p0, rot), u0, v0, normal, 0.0f, -1.0f, -1.0f, NoInts),
			Make(origin + RotateZ(p1, rot), u1, v1, normal, 0.0f, -1.0f, -1.0f, NoInts),
			Make(origin + RotateZ(p2, rot), u2, v2, normal, 0.0f, -1.0f, -1.0f, NoInts));
}

void ModelAdd(TriList& out, const Vec3& normal, const Vec3& origin, float rot,
		const Vec3& p0, float u0, float v0, const Vec3& p1, float u1, float v1, const Vec3& p2, float u2, float v2,
		const Vec3& p3, float u3, float v3)
{
	ModelAdd(out, normal, origin, rot, p0, u0, v0, p1, u1, v1, p2, u2, v2);
	Add(out, Make(origin + RotateZ(p2, rot), u2, v2, normal, 0.0f, -1.0f, -1.0f, NoInts),
			Make(origin + RotateZ(p3, rot), u3, v3, normal, 0.0f, -1.0f, -1.0f, NoInts),
			Make(origin + RotateZ(p0, rot), u0, v0, normal, 0.0f, -1.0f, -1.0f, NoInts));
}

}
