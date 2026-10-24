#include "oldpathmathstep.h"

#include <cmath>

namespace OldPath
{
namespace AuMath
{

namespace
{
inline bool Same3(const Point& a, const Point& b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}
}

Point Sub(const Point& a, const Point& b)
{
	return Point(a.x - b.x, a.y - b.y, a.z - b.z);
}

Point Add(const Point& a, const Point& b)
{
	return Point(b.x + a.x, b.y + a.y, b.z + a.z);
}

double ZRotation(const Point& p)
{
	double y = p.y, x = p.x;
	if(std::fabs(y) <= Eps)
		return x < -Eps ? 3.141592653589793 : 0.0;
	if(Eps < std::fabs(x))
		return std::atan2(y, x);
	return y < -Eps ? -1.5707963267948966 : 1.5707963267948966;
}

double DistToLine(const Point& p, const Point& a, const Point& b)
{
	double ax = a.x, ay = a.y, az = a.z;
	double dx = b.x - ax, dy = b.y - ay, dz = b.z - az;
	double len = std::sqrt(dy * dy + dx * dx + dz * dz);
	if(len == 0.0)
		return std::sqrt((p.x - ax) * (p.x - ax) + (p.y - ay) * (p.y - ay) + (p.z - az) * (p.z - az));
	double t = ((p.y - ay) * dy + (p.x - ax) * dx + (p.z - az) * dz) / len;
	if(t == 0.0)
		dx = dy = dz = 0.0;
	else
	{
		dx = dx * t;
		dy = dy * t;
		dz = dz * t;
	}
	double rx = p.x - (dx / len + ax);
	double ry = p.y - (dy / len + ay);
	double rz = p.z - (dz / len + az);
	return std::sqrt(ry * ry + rx * rx + rz * rz);
}

namespace
{
struct Line
{
	Point n;
	double d;
	Point p;
	Point dir;
};

void ExplicitToImplicit(Line& l)
{
	Point u = l.dir;
	Unit(u);
	double dot = u.y * l.dir.y + u.x * l.dir.x + u.z * l.dir.z;
	if(dot != 0.0)
	{
		double t = ((u.y * 0.0 + u.x * 0.0 + u.z * 0.0) - (u.y * l.p.y + u.x * l.p.x + u.z * l.p.z)) / dot;
		Point s = Mul(l.dir, t);
		l.n.x = s.x + l.p.x;
		l.n.y = s.y + l.p.y;
		l.n.z = s.z + l.p.z;
	}
	Unit(l.n);
	l.d = l.p.y * l.n.y + l.p.x * l.n.x + l.p.z * l.n.z;
}
}

bool LineIntersection2D(Point& out, const Point& a, const Point& b, const Point& c, const Point& d)
{
	Line l1;
	l1.n = Point(0, 0, 0);
	l1.d = 0;
	l1.p = Point(a.x, a.y, 0.0);
	l1.dir = Point(b.x - a.x, b.y - a.y, 0.0);
	Unit(l1.dir);
	ExplicitToImplicit(l1);
	Line l2;
	l2.n = Point(0, 0, 0);
	l2.d = 0;
	l2.p = Point(c.x, c.y, 0.0);
	l2.dir = Point(d.x - c.x, d.y - c.y, 0.0);
	Unit(l2.dir);
	ExplicitToImplicit(l2);
	double den = l2.dir.y * l1.n.y + l2.dir.x * l1.n.x + l2.dir.z * l1.n.z;
	if(den == 0.0)
	{
		out = a;
		return false;
	}
	double t = (l1.d - (l2.p.y * l1.n.y + l2.p.x * l1.n.x + l2.p.z * l1.n.z)) / den;
	Point s = Mul(l2.dir, t);
	out.z = l2.p.z + s.z;
	out.x = l2.p.x + s.x;
	out.y = l2.p.y + s.y;
	if(l1.n.z == 0.0)
		out.z = a.z;
	else
		out.z = ((-l1.d - (l2.p.x + s.x) * l1.n.x) - (l2.p.y + s.y) * l1.n.y) / l1.n.z;
	return true;
}

bool SegmentIntersection2D(Point& out, const Point& a, const Point& b, const Point& c, const Point& d, bool lineCD,
		bool lineAB)
{
	Point tmp;
	if(!PerpConn(Point(a.x, a.y, 0.0), Point(b.x, b.y, 0.0), Point(c.x, c.y, 0.0), Point(d.x, d.y, 0.0), out, tmp, Eps))
		return false;
	if(!(std::fabs(out.x - tmp.x) <= Eps && std::fabs(out.y - tmp.y) <= Eps))
		return false;
	if(!lineAB && !Between(out, Point(a.x, a.y, out.z), Point(b.x, b.y, out.z), Eps))
		return false;
	if(!lineCD && !Between(out, Point(c.x, c.y, out.z), Point(d.x, d.y, out.z), Eps))
		return false;
	return true;
}

bool IntersectBoundary2D(const PointArray& pts, const Point& a, const Point& b, Point& out)
{
	if(pts.empty())
		return false;
	Point prev = pts.back();
	for(size_t i=0; i<pts.size(); i++)
	{
		const Point& q = pts[i];
		if(Same3(q, prev))
			continue;
		if(SegmentIntersection2D(out, a, b, q, prev, false, false))
			return true;
		prev = q;
	}
	return false;
}

namespace
{
bool EdgeCrossing(const Point& a, const Point& b, const Point& q, const Point& prev, Point& r1)
{
	Point r2;
	return PerpConn(Point(a.x, a.y, 0.0), Point(b.x, b.y, 0.0), Point(q.x, q.y, 0.0), Point(prev.x, prev.y, 0.0), r1,
				r2, Eps) && std::fabs(r1.x - r2.x) <= Eps && std::fabs(r1.y - r2.y) <= Eps &&
		Between(r1, Point(a.x, a.y, r1.z), Point(b.x, b.y, r1.z), Eps) &&
		Between(r1, Point(q.x, q.y, r1.z), Point(prev.x, prev.y, r1.z), Eps);
}
}

bool IntersectBoundary2D(const PointArray& pts, const Point& a, const Point& b, PointArray& out)
{
	bool found = false;
	if(pts.empty())
		return false;
	Point prev = pts.back();
	for(size_t i=0; i<pts.size(); i++)
	{
		const Point& q = pts[i];
		if(Same3(q, prev))
			continue;
		Point r1;
		if(EdgeCrossing(a, b, q, prev, r1))
		{
			bool dup = false;
			for(size_t j=0; j<out.size(); j++)
				if(std::fabs(out[j].x - r1.x) <= Eps && std::fabs(out[j].y - r1.y) <= Eps &&
					std::fabs(out[j].z - r1.z) <= Eps)
				{
					dup = true;
					break;
				}
			if(!dup)
				out.push_back(r1);
			found = true;
		}
		prev = q;
	}
	return found;
}

bool DoesIntersectBoundary2D(const PointArray& pts, const Point& a, const Point& b, Point& e1, Point& e2)
{
	if(pts.empty())
		return false;
	Point prev = pts.back();
	for(size_t i=0; i<pts.size(); i++)
	{
		const Point& q = pts[i];
		if(Same3(q, prev))
			continue;
		Point r1;
		if(EdgeCrossing(a, b, q, prev, r1))
		{
			e1 = prev;
			e2 = q;
			return true;
		}
		prev = q;
	}
	return false;
}

}
}
