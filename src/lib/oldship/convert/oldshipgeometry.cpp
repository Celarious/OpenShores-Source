#include "oldshipconverter.h"

#include <cmath>

namespace OldShip
{

double P2::Length() const
{
	return sqrt(x * x + y * y);
}

P2 P2::Unit() const
{
	const double l = Length();
	return l > 0.0 ? P2(x / l, y / l) : P2(0, 0);
}

double SignedArea(const Polygon& poly)
{
	double a = 0;
	for(int i = 0; i < poly.size(); i++)
		a += poly[i].Cross(poly[(i + 1) % poly.size()]);
	return a / 2;
}

bool Contains(const Polygon& poly, const P2& p)
{
	bool in = false;
	for(int i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
	{
		const P2& a = poly[i];
		const P2& b = poly[j];
		if((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)
			in = !in;
	}
	return in;
}

static double DistanceToSegment(const P2& p, const P2& a, const P2& b)
{
	const P2 ab = b - a;
	const double l2 = ab.Dot(ab);
	double t = l2 > 0.0 ? (p - a).Dot(ab) / l2 : 0.0;
	t = t < 0.0 ? 0.0 : t > 1.0 ? 1.0 : t;
	return (p - (a + ab * t)).Length();
}

double DistanceTo(const Polygon& poly, const P2& p)
{
	if(poly.isEmpty())
		return 1e30;
	if(Contains(poly, p))
		return 0.0;
	double best = 1e30;
	for(int i = 0; i < poly.size(); i++)
		best = qMin(best, DistanceToSegment(p, poly[i], poly[(i + 1) % poly.size()]));
	return best;
}

Polygon Tidied(const Polygon& poly)
{
	Polygon out;
	for(int i = 0; i < poly.size(); i++)
		if(out.isEmpty() || (poly[i] - out.last()).Length() > 1e-6)
			out.append(poly[i]);
	while(out.size() > 1 && (out.first() - out.last()).Length() <= 1e-6)
		out.removeLast();
	for(bool again = true; again && out.size() > 3; )
	{
		again = false;
		for(int i = 0; i < out.size(); i++)
		{
			const P2& a = out[(i + out.size() - 1) % out.size()];
			const P2& b = out[i];
			const P2& c = out[(i + 1) % out.size()];
			if(qAbs((b - a).Unit().Cross((c - b).Unit())) < 1e-9)
			{
				out.remove(i);
				again = true;
				break;
			}
		}
	}
	if(SignedArea(out) < 0)
	{
		Polygon turned;
		for(int i = out.size() - 1; i >= 0; i--)
			turned.append(out[i]);
		out = turned;
	}
	return out;
}

bool Inset(const Polygon& poly, double by, Polygon& out)
{
	out.clear();
	const int n = poly.size();
	if(n < 3)
		return false;
	for(int i = 0; i < n; i++)
	{
		const P2 u = (poly[i] - poly[(i + n - 1) % n]).Unit();
		const P2 v = (poly[(i + 1) % n] - poly[i]).Unit();
		const P2 a = poly[i] + u.Left() * by;
		const P2 b = poly[i] + v.Left() * by;
		const double d = u.Cross(v);
		if(qAbs(d) < 1e-9)
			out.append(a);
		else
			out.append(a + u * ((b - a).Cross(v) / d));
	}
	if(SignedArea(out) <= 0)
		return false;
	for(int i = 0; i < n; i++)
	{
		const P2 was = poly[(i + 1) % n] - poly[i];
		const P2 is = out[(i + 1) % n] - out[i];
		if(was.Dot(is) <= 1e-6)
			return false;
	}
	for(int i = 0; i < n; i++)
		for(int j = i + 2; j < n; j++)
		{
			if(i == 0 && j == n - 1)
				continue;
			const P2& a = out[i];
			const P2& b = out[(i + 1) % n];
			const P2& c = out[j];
			const P2& d = out[(j + 1) % n];
			const double d1 = (b - a).Cross(c - a), d2 = (b - a).Cross(d - a);
			const double d3 = (d - c).Cross(a - c), d4 = (d - c).Cross(b - c);
			if(((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0)))
				return false;
		}
	return true;
}

void Triangulate(const Polygon& poly, QVector<int>& corners)
{
	corners.clear();
	QVector<int> left;
	for(int i = 0; i < poly.size(); i++)
		left.append(i);
	int guard = poly.size() * poly.size() + 8;
	while(left.size() > 3 && guard-- > 0)
	{
		bool cut = false;
		for(int i = 0; i < left.size() && !cut; i++)
		{
			const int ia = left[(i + left.size() - 1) % left.size()], ib = left[i], ic = left[(i + 1) % left.size()];
			const P2& a = poly[ia];
			const P2& b = poly[ib];
			const P2& c = poly[ic];
			if((b - a).Cross(c - b) <= 1e-12)
				continue;
			bool empty = true;
			for(int k = 0; k < left.size() && empty; k++)
			{
				const int ip = left[k];
				if(ip == ia || ip == ib || ip == ic)
					continue;
				const P2& p = poly[ip];
				if((b - a).Cross(p - a) >= -1e-12 && (c - b).Cross(p - b) >= -1e-12 && (a - c).Cross(p - c) >= -1e-12)
					empty = false;
			}
			if(!empty)
				continue;
			corners << ia << ib << ic;
			left.remove(i);
			cut = true;
		}
		if(!cut)
			break;
	}
	if(left.size() == 3)
		corners << left[0] << left[1] << left[2];
}

P2 PointInside(const Polygon& poly)
{
	QVector<int> corners;
	Triangulate(poly, corners);
	P2 best = poly.isEmpty() ? P2() : poly[0];
	double bestArea = -1;
	for(int i = 0; i + 2 < corners.size(); i += 3)
	{
		const P2& a = poly[corners[i]];
		const P2& b = poly[corners[i + 1]];
		const P2& c = poly[corners[i + 2]];
		const double area = qAbs((b - a).Cross(c - a));
		if(area > bestArea)
		{
			bestArea = area;
			best = P2((a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3);
		}
	}
	return best;
}

void AddTri(AuScFaceList& faces, const AuPoint& a, const AuPoint& b, const AuPoint& c, const AuPoint& n)
{
	if((b - a).Cross(c - a).Length() < 1e-9)
		return;
	AuScFaceTri* face = new AuScFaceTri(AuScPoint(a, 0.f, 0.f, n), AuScPoint(b, 1.f, 0.f, n), AuScPoint(c, 0.f, 1.f, n));
	if(face->FaceNormal().Dot(n) < 0)
	{
		delete face;
		face = new AuScFaceTri(AuScPoint(a, 0.f, 0.f, n), AuScPoint(c, 0.f, 1.f, n), AuScPoint(b, 1.f, 0.f, n));
	}
	faces.append(face);
}

void AddQuad(AuScFaceList& faces, const AuPoint& a, const AuPoint& b, const AuPoint& c, const AuPoint& d, const AuPoint& n)
{
	if((b - a).Cross(c - a).Length() < 1e-9 || (c - a).Cross(d - a).Length() < 1e-9)
	{
		AddTri(faces, a, b, c, n);
		AddTri(faces, a, c, d, n);
		return;
	}
	AuScFaceQuad* face = new AuScFaceQuad(AuScPoint(a, 0.f, 0.f, n), AuScPoint(b, 0.f, 1.f, n), AuScPoint(c, 1.f, 1.f, n),
			AuScPoint(d, 1.f, 0.f, n));
	if(face->FaceNormal().Dot(n) < 0)
	{
		delete face;
		face = new AuScFaceQuad(AuScPoint(d, 1.f, 0.f, n), AuScPoint(c, 1.f, 1.f, n), AuScPoint(b, 0.f, 1.f, n),
				AuScPoint(a, 0.f, 0.f, n));
	}
	faces.append(face);
}

void AddPrism(AuScFaceList& faces, const QVector<AuPoint>& bottom, const QVector<AuPoint>& top)
{
	const int n = bottom.size();
	if(n < 3 || top.size() != n)
		return;
	Polygon plan;
	for(int i = 0; i < n; i++)
		plan.append(P2(bottom[i].x, bottom[i].y));
	QVector<int> corners;
	Triangulate(plan, corners);
	AuPoint down = (bottom[corners[1]] - bottom[corners[0]]).Cross(bottom[corners[2]] - bottom[corners[0]]);
	down.Unit();
	if(down.z > 0)
		down = down * -1.0;
	AuPoint up = (top[corners[1]] - top[corners[0]]).Cross(top[corners[2]] - top[corners[0]]);
	up.Unit();
	if(up.z < 0)
		up = up * -1.0;
	if(n == 4)
	{
		AddQuad(faces, bottom[0], bottom[1], bottom[2], bottom[3], down);
		AddQuad(faces, top[0], top[1], top[2], top[3], up);
	}
	else
		for(int i = 0; i + 2 < corners.size(); i += 3)
		{
			AddTri(faces, bottom[corners[i]], bottom[corners[i + 1]], bottom[corners[i + 2]], down);
			AddTri(faces, top[corners[i]], top[corners[i + 1]], top[corners[i + 2]], up);
		}
	for(int i = 0; i < n; i++)
	{
		const int j = (i + 1) % n;
		const P2 out = (plan[j] - plan[i]).Unit().Left() * -1.0;
		AddQuad(faces, bottom[i], top[i], top[j], bottom[j], AuPoint(out.x, out.y, 0));
	}
}

}
