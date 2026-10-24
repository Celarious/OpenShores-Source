#ifndef OLDPATH_AUMATH_STEP_H
#define OLDPATH_AUMATH_STEP_H

#include "oldpathmath.h"

namespace OldPath
{
namespace AuMath
{

const double Eps = Fuzz;
typedef std::vector<Point> PointArray;

Point Sub(const Point& a, const Point& b);
Point Add(const Point& a, const Point& b);
double ZRotation(const Point& p);
double DistToLine(const Point& p, const Point& a, const Point& b);
bool LineIntersection2D(Point& out, const Point& a, const Point& b, const Point& c, const Point& d);
bool SegmentIntersection2D(Point& out, const Point& a, const Point& b, const Point& c, const Point& d, bool lineCD,
		bool lineAB);
bool IntersectBoundary2D(const PointArray& pts, const Point& a, const Point& b, Point& out);
bool IntersectBoundary2D(const PointArray& pts, const Point& a, const Point& b, PointArray& out);
bool DoesIntersectBoundary2D(const PointArray& pts, const Point& a, const Point& b, Point& e1, Point& e2);

}
}

#endif
