#ifndef OLDPATH_AUMATH_H
#define OLDPATH_AUMATH_H

#include "oldpath.h"

#include <vector>

namespace OldPath
{

namespace AuMath
{

const double Fuzz = 1e-05;

bool FuzzyEquals(const Point& p, const Point& q);
bool FuzzyEquals2D(const Point& p, const Point& q);
double Distance2D(const Point& p, const Point& q);
Point PointOnSegment2D(const Point& p, const Point& a, const Point& b);
Point Mul(const Point& p, double s);
void Unit(Point& p);
void Unit(Point& p, double len);
bool Between(const Point& p, const Point& a, const Point& b, double eps);
bool Between2D(const Point& p, const Point& a, const Point& b);
bool PerpConn(const Point& p, const Point& p2, const Point& a, const Point& b, Point& onP, Point& onA, double eps);

void Extents(const std::vector<Point>& pts, Point& min, Point& max);
double ClosestBoundarySegment2D(const std::vector<Point>& pts, const Point& p, Point& a, Point& b);
bool PointOnBoundary2D(const std::vector<Point>& pts, const Point& p);
bool PointInBoundary(const std::vector<Point>& pts, const Point& p);

}

}

#endif
