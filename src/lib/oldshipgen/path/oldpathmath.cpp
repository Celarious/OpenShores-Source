#include "oldpathmath.h"

#include <cmath>

namespace OldPath
{

namespace AuMath
{

bool FuzzyEquals(const Point& p, const Point& q)
{
	return std::fabs(p.x - q.x) <= Fuzz && std::fabs(p.y - q.y) <= Fuzz && std::fabs(p.z - q.z) <= Fuzz;
}

bool FuzzyEquals2D(const Point& p, const Point& q)
{
	if(Fuzz < std::fabs(p.x - q.x))
		return false;
	return std::fabs(p.y - q.y) <= Fuzz;
}

double Distance2D(const Point& p, const Point& q)
{
	return std::sqrt((p.y - q.y) * (p.y - q.y) + (p.x - q.x) * (p.x - q.x));
}

Point PointOnSegment2D(const Point& p, const Point& a, const Point& b)
{
	const double dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
	const double len = std::sqrt(dx * dx + dy * dy);
	if(len == 0.0)
		return a;
	const double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len;
	if(t <= 0.0)
		return a;
	if(t < len)
		return Point((dx * t) / len + a.x, (dy * t) / len + a.y, (dz * t) / len + a.z);
	return b;
}

Point Mul(const Point& p, double s)
{
	if(s == 0.0)
		return Point();
	return Point(p.x * s, p.y * s, p.z * s);
}

void Unit(Point& p)
{
	const double len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
	if(len == 0.0)
		p.x = 1.0;
	else
	{
		p.x = p.x / len;
		p.y = p.y / len;
		p.z = p.z / len;
	}
}

void Unit(Point& p, double want)
{
	const double len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
	if(len == 0.0)
	{
		p.x = want;
		return;
	}
	if(len != 1.0)
		want = want / len;
	if(want != 1.0)
	{
		p.x = p.x * want;
		p.y = p.y * want;
		p.z = p.z * want;
	}
}

bool Between(const Point& p, const Point& a, const Point& b, double eps)
{
	const double dx = p.x - a.x;
	if(std::fabs(dx) <= eps && std::fabs(p.y - a.y) <= eps && std::fabs(p.z - a.z) <= eps)
		return true;
	if(std::fabs(p.x - b.x) <= eps && std::fabs(p.y - b.y) <= eps && std::fabs(p.z - b.z) <= eps)
		return true;
	const double ex = b.x - a.x, ey = b.y - a.y, ez = b.z - a.z;
	double len = ex * ex + ey * ey + ez * ez;
	if(len != 0.0)
	{
		len = std::sqrt(len);
		const double t = (p.y - a.y) * (ey / len) + dx * (ex / len) + (p.z - a.z) * (ez / len);
		if(0.0 <= t && t <= len)
			return true;
	}
	return false;
}

bool Between2D(const Point& p, const Point& a, const Point& b)
{
	return Between(p, Point(a.x, a.y, p.z), Point(b.x, b.y, p.z), Fuzz);
}

bool PerpConn(const Point& p, const Point& p2, const Point& a, const Point& b, Point& onP, Point& onA, double eps)
{
	const double dy = b.y - a.y, dx = b.x - a.x, dz = b.z - a.z;
	double ux = p2.x - p.x, uy = p2.y - p.y, uz = p2.z - p.z;
	const double wx = a.x - p.x, wy = a.y - p.y, wz = a.z - p.z;
	const double uu = ux * ux + uy * uy + uz * uz;
	const double ud = ux * dx + dy * uy + dz * uz;
	const double dd = dy * dy + dx * dx + dz * dz;
	const double ud2 = ud * ud;
	const double prod = dd * uu;
	const double det = ud2 - prod;
	if(!((std::fabs(prod) + std::fabs(ud2)) * eps < std::fabs(det)))
	{
		onP = Point();
		onA = Point();
		return false;
	}
	const double wu = wx * ux + wy * uy + wz * uz;
	const double wd = wy * dy + wx * dx + wz * dz;
	const double t = (wd * ud - wu * dd) / det;
	if(t == 0.0)
		ux = uy = uz = 0.0;
	else
	{
		ux = ux * t;
		uy = uy * t;
		uz = uz * t;
	}
	onP = Point(p.x + ux, p.y + uy, p.z + uz);
	const double s = (wd * uu - wu * ud) / det;
	double sx = 0.0, sy = 0.0, sz = 0.0;
	if(s != 0.0)
	{
		sx = dx * s;
		sy = dy * s;
		sz = dz * s;
	}
	onA = Point(sx + a.x, sy + a.y, sz + a.z);
	return true;
}

void Extents(const std::vector<Point>& pts, Point& mn, Point& mx)
{
	if(pts.empty())
	{
		mn = mx = Point();
		return;
	}
	mn = mx = pts[0];
	for(size_t i = 1; i < pts.size(); i++)
	{
		const Point& q = pts[i];
		mn = Point(mn.x <= q.x ? mn.x : q.x, mn.y <= q.y ? mn.y : q.y, mn.z <= q.z ? mn.z : q.z);
		mx = Point(q.x <= mx.x ? mx.x : q.x, q.y <= mx.y ? mx.y : q.y, q.z <= mx.z ? mx.z : q.z);
	}
}

double ClosestBoundarySegment2D(const std::vector<Point>& pts, const Point& p, Point& a, Point& b)
{
	const size_t n = pts.size();
	double best = -1.0;
	if(n < 2)
		return best;
	bool found = false;
	for(size_t i = 1; i < n; i++)
	{
		const Point r = PointOnSegment2D(p, pts[i - 1], pts[i]);
		const double d = std::sqrt((p.y - r.y) * (p.y - r.y) + (p.x - r.x) * (p.x - r.x));
		if(!found || d < best)
		{
			found = true;
			best = d;
			a = pts[i - 1];
			b = pts[i];
		}
	}
	if(n > 2)
	{
		const Point r = PointOnSegment2D(p, pts[n - 1], pts[0]);
		const double d = std::sqrt((p.y - r.y) * (p.y - r.y) + (p.x - r.x) * (p.x - r.x));
		if(!found || d < best)
		{
			best = d;
			a = pts[n - 1];
			b = pts[0];
		}
	}
	return best;
}

bool PointOnBoundary2D(const std::vector<Point>& pts, const Point& p)
{
	const size_t n = pts.size();
	if(n < 2)
		return false;
	for(size_t i = 1; i < n; i++)
	{
		const Point r = PointOnSegment2D(p, pts[i - 1], pts[i]);
		if(std::sqrt((p.x - r.x) * (p.x - r.x) + (p.y - r.y) * (p.y - r.y)) <= Fuzz)
			return true;
	}
	if(n > 2)
	{
		const Point r = PointOnSegment2D(p, pts[n - 1], pts[0]);
		if(std::sqrt((p.y - r.y) * (p.y - r.y) + (p.x - r.x) * (p.x - r.x)) <= Fuzz)
			return true;
	}
	return false;
}

bool PointInBoundary(const std::vector<Point>& pts, const Point& p)
{
	const size_t n = pts.size();
	if(n < 3)
		return false;
	Point mn, mx;
	Extents(pts, mn, mx);
	if(!(mn.x < p.x && p.x < mx.x))
		return false;
	if(!(mn.y < p.y && p.y < mx.y))
		return false;
	const double h = mx.y - mn.y, w = mx.x - mn.x;
	const double deg = 0.017453292519943295;
	const double c = std::cos(deg), s = std::sin(deg);
	const double pz = p.z + 0.0;
	double ry = h * c + w * s;
	double rx = w * c - h * s;
	int vertexHits = 0;
	for(;;)
	{
		const Point rayEnd(p.x + rx, p.y + ry, pz);
		Point prev = pts[n - 1];
		int crossings = 0;
		bool turn = false;
		for(size_t i = 0; i < n && !turn; i++)
		{
			const Point& q = pts[i];
			if(q.x == prev.x && q.y == prev.y && q.z == prev.z)
				continue;
			Point x, y;
			if(!PerpConn(Point(p.x, p.y, 0.0), Point(rayEnd.x, rayEnd.y, 0.0), Point(q.x, q.y, 0.0),
					Point(prev.x, prev.y, 0.0), x, y, Fuzz))
			{
				const Point r = PointOnSegment2D(q, p, rayEnd);
				if(std::fabs(std::sqrt((q.y - r.y) * (q.y - r.y) + (q.x - r.x) * (q.x - r.x))) <= Fuzz)
				{
					turn = true;
					break;
				}
			}
			else if(std::fabs(x.x - y.x) <= Fuzz && std::fabs(x.y - y.y) <= Fuzz
					&& Between(x, Point(p.x, p.y, x.z), Point(rayEnd.x, rayEnd.y, x.z), Fuzz)
					&& Between(x, Point(q.x, q.y, x.z), Point(prev.x, prev.y, x.z), Fuzz))
			{
				if(std::fabs(x.x - p.x) <= Fuzz && std::fabs(x.y - p.y) <= Fuzz)
					return false;
				if((std::fabs(x.x - q.x) <= Fuzz && std::fabs(x.y - q.y) <= Fuzz)
						|| (std::fabs(x.x - prev.x) <= Fuzz && std::fabs(x.y - prev.y) <= Fuzz))
				{
					if(++vertexHits == 360)
						return false;
					turn = true;
				}
				crossings++;
				if(turn)
					break;
			}
			prev = q;
		}
		if(!turn)
			return (crossings & 1) != 0;
		const double t = ry * s;
		ry = ry * c + s * rx;
		rx = c * rx - t;
	}
}

}

}
