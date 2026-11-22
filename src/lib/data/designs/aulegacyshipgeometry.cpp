#include "aulegacyshipgeometry.h"
#include "aulegacyship.h"

#include <QPoint>
#include <QVector>
#include <algorithm>
#include <cmath>

// Port of the 2016 DnFloorBuffer8, not a general polygon Boolean operation.
// Function addresses and raw decompilations are in PAINTING-RULES.txt.
namespace AuLegacyShip
{
namespace Geometry
{
namespace
{
constexpr double RasterSpacing = 1.41421356225;
constexpr int RasterSize = 516;
constexpr int RasterOrigin = 256;
constexpr int NoScanStart = 9999;

struct Shape
{
	int count;
	int xy[8];
};

const Shape Shapes[16][4] =
{
	{ // 0x0
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}}
	},
	{ // 0x1
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}}
	},
	{ // 0x2
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}}
	},
	{ // 0x3
		{4, {0, 0, 2, 2, 4, 2, 2, 0}},
		{4, {0, 0, -2, 0, 0, 2, 2, 2}},
		{4, {0, 0, -2, 0, 0, 2, 2, 2}},
		{4, {0, 0, 2, 2, 4, 2, 2, 0}}
	},
	{ // 0x4
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}}
	},
	{ // 0x5
		{4, {0, 0, 0, 2, 2, 2, 2, 0}},
		{4, {0, 0, 0, 2, 2, 2, 2, 0}},
		{4, {0, 0, 0, 2, 2, 2, 2, 0}},
		{4, {0, 0, 0, 2, 2, 2, 2, 0}}
	},
	{ // 0x6
		{4, {0, 0, 2, 2, 2, 0, 0, -2}},
		{4, {0, 0, 0, 2, 2, 4, 2, 2}},
		{4, {0, 0, 0, 2, 2, 4, 2, 2}},
		{4, {0, 0, 2, 2, 2, 0, 0, -2}}
	},
	{ // 0x7
		{3, {0, 0, 2, 2, 2, 0, 0, 0}},
		{3, {0, 0, 0, 2, 2, 2, 0, 0}},
		{3, {0, 0, 0, 2, 2, 2, 0, 0}},
		{3, {0, 0, 2, 2, 2, 0, 0, 0}}
	},
	{ // 0x8
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}},
		{0, {0, 0, 0, 0, 0, 0, 0, 0}}
	},
	{ // 0x9
		{4, {0, 2, 2, 2, 4, 0, 2, 0}},
		{4, {0, 2, 2, 2, 4, 0, 2, 0}},
		{4, {0, 2, 2, 0, 0, 0, -2, 2}},
		{4, {0, 2, 2, 0, 0, 0, -2, 2}}
	},
	{ // 0xA
		{4, {1, 1, 2, 2, 3, 1, 2, 0}},
		{4, {1, 1, 0, 2, 1, 3, 2, 2}},
		{4, {1, 1, 0, 0, -1, 1, 0, 2}},
		{4, {1, 1, 2, 0, 1, -1, 0, 0}}
	},
	{ // 0xB
		{4, {1, 1, 2, 2, 3, 1, 2, 0}},
		{3, {1, 1, 0, 2, 2, 2, 0, 0}},
		{4, {1, 1, 0, 0, -1, 1, 0, 2}},
		{3, {1, 1, 2, 0, 0, 0, 0, 0}}
	},
	{ // 0xC
		{4, {0, 2, 0, 4, 2, 2, 2, 0}},
		{4, {0, 2, 0, 4, 2, 2, 2, 0}},
		{4, {0, 2, 2, 0, 2, -2, 0, 0}},
		{4, {0, 2, 2, 0, 2, -2, 0, 0}}
	},
	{ // 0xD
		{3, {2, 0, 0, 2, 2, 2, 0, 0}},
		{3, {2, 0, 0, 2, 2, 2, 0, 0}},
		{3, {2, 0, 0, 0, 0, 2, 0, 0}},
		{3, {2, 0, 0, 0, 0, 2, 0, 0}}
	},
	{ // 0xE
		{3, {1, 1, 2, 2, 2, 0, 0, 0}},
		{4, {1, 1, 0, 2, 1, 3, 2, 2}},
		{3, {1, 1, 0, 0, 0, 2, 0, 0}},
		{4, {1, 1, 2, 0, 1, -1, 0, 0}}
	},
	{ // 0xF
		{3, {1, 1, 2, 2, 2, 0, 0, 0}},
		{3, {1, 1, 0, 2, 2, 2, 0, 0}},
		{3, {1, 1, 0, 0, 0, 2, 0, 0}},
		{3, {1, 1, 2, 0, 0, 0, 0, 0}}
	}
};

void SetError(QString* error, const char* message)
{
	if(error)
		*error = QString::fromLatin1(message);
}

bool RasterPoints(const QPolygonF& boundary, QVector<QPoint>& result, QString* error)
{
	if(boundary.size() < 3)
	{
		SetError(error, "A room boundary needs at least three vertices.");
		return false;
	}
	result.reserve(boundary.size());
	for(const QPointF& point : boundary)
	{
		const double x = std::round(point.x() / RasterSpacing);
		const double y = std::round(point.y() / RasterSpacing);
		// Leave the two-sample neighborhood used by the original vertex tests.
		// The normal authoring limit (+/-120 half-grid units) is well inside it.
		if(!std::isfinite(x) || !std::isfinite(y)
			|| x < 2 - RasterOrigin || x > RasterSize - 3 - RasterOrigin
			|| y < 2 - RasterOrigin || y > RasterSize - 3 - RasterOrigin)
		{
			SetError(error, "The room boundary exceeds the legacy raster bounds.");
			return false;
		}
		result.append(QPoint(int(x) + RasterOrigin, int(y) + RasterOrigin));
	}
	QPoint previous = result.last();
	for(const QPoint& point : result)
	{
		const int dx = point.x() - previous.x();
		const int dy = point.y() - previous.y();
		if(dx != 0 && dy != 0 && std::abs(dx) != std::abs(dy))
		{
			SetError(error, "This boundary contains an edge outside the legacy cell grid.");
			return false;
		}
		previous = point;
	}
	return true;
}

bool EqualBoundary(const QPolygonF& a, const QPolygonF& b)
{
	// AuPointArray compares stored doubles in order; QPointF equality is fuzzy.
	if(a.size() != b.size())
		return false;
	for(int i = 0; i < a.size(); ++i)
		if(a.at(i).x() != b.at(i).x() || a.at(i).y() != b.at(i).y())
			return false;
	return true;
}

bool PointOnSegments(const QPolygonF& boundary, const QPointF& point)
{
	// AuMath13!PointOnSegments_2D (18000daa0) / PointOnSegment_2D (18000d850).
	// The closing segment is deliberately excluded. Retain its two divisions
	// by length and the original 1e-5 world-foot proximity threshold.
	for(int i = 1; i < boundary.size(); ++i)
	{
		const QPointF& start = boundary.at(i - 1);
		const QPointF& end = boundary.at(i);
		const double dx = end.x() - start.x(), dy = end.y() - start.y();
		const double length = std::sqrt(dx * dx + dy * dy);
		QPointF closest = start;
		if(length != 0)
		{
			const double distance = ((point.x() - start.x()) * dx
				+ (point.y() - start.y()) * dy) / length;
			if(distance > 0)
			{
				if(distance < length)
					closest = QPointF((dx * distance) / length + start.x(),
						(dy * distance) / length + start.y());
				else
					closest = end;
			}
		}
		const double ex = point.x() - closest.x(), ey = point.y() - closest.y();
		if(std::sqrt(ex * ex + ey * ey) <= 1e-5)
			return true;
	}
	return false;
}

struct Raster
{
	QVector<quint8> samples = QVector<quint8>(RasterSize * RasterSize, 0);

	quint8 At(int x, int y) const
	{
		// Normal validated boundaries never reach this guard.
		if(x < 0 || y < 0 || x >= RasterSize || y >= RasterSize)
			return 0;
		return samples.at(x * RasterSize + y);
	}

	quint8& At(int x, int y) { return samples[x * RasterSize + y]; }

	struct Bounds
	{
		int xmin, xmax, ymin, ymax;
		explicit Bounds(const QPoint& p)
			: xmin(p.x()), xmax(p.x()), ymin(p.y()), ymax(p.y()) {}
		void Include(const QPoint& p)
		{
			xmin = std::min(xmin, p.x()); xmax = std::max(xmax, p.x());
			ymin = std::min(ymin, p.y()); ymax = std::max(ymax, p.y());
		}
	};

	template<typename Visit>
	Bounds WalkPerimeter(const QVector<QPoint>& polygon, Visit visit)
	{
		QPoint previous = polygon.last();
		Bounds bounds(previous);
		for(const QPoint& point : polygon)
		{
			bounds.Include(point);
			const int dx = (point.x() > previous.x()) - (point.x() < previous.x());
			const int dy = (point.y() > previous.y()) - (point.y() < previous.y());
			while(previous != point)
			{
				visit(previous.x(), previous.y());
				previous += QPoint(dx, dy);
			}
		}
		return bounds;
	}

	template<typename Visit>
	void FillInterior(const Bounds& bounds, int marker, bool erase, Visit visit)
	{
		const auto isBoundary = [marker, erase](int sample)
			{ return erase ? sample == 0x56 : sample >= marker; };
		const auto isContinuation = [marker, erase](int sample)
			{ return erase ? sample == 0x56 || sample == 9 : sample >= marker; };
		for(int y = bounds.ymin; y <= bounds.ymax; ++y)
		{
			bool consecutive = false;
			int start = NoScanStart;
			for(int x = bounds.xmin; x <= bounds.xmax; ++x)
			{
				if(!isBoundary(At(x, y)))
				{
					if(consecutive)
					{
						start = x == start + 2 || !isContinuation(At(x + 1, y - 1))
							? NoScanStart : x - 1;
						consecutive = false;
					}
					continue;
				}
				const int rx = x % 4, ry = y % 4;
				if((rx == 0 && ry == 0) || (rx == 2 && ry == 2))
				{
					const bool nw = isBoundary(At(x - 1, y + 1));
					const bool north = isBoundary(At(x, y + 1));
					const bool ne = isBoundary(At(x + 1, y + 1));
					const bool east = isBoundary(At(x + 1, y));
					// Original scanline corner/center crossing predicate.
					if((nw && north) || (north && ne) || (nw && ne)
						|| !(nw || north || ne || east))
						continue;
				}
				if(start == NoScanStart)
					start = x;
				else if(x == start + 1)
					consecutive = true;
				else if(!consecutive)
				{
					for(int inside = start + 1; inside < x; ++inside)
						visit(inside, y);
					start = NoScanStart;
				}
			}
		}
	}

	bool Add(const QVector<QPoint>& polygon, int marker, QString* error = nullptr)
	{
		// 1801681f0: counts are visits, including repeated visits, not areas.
		int newVisits = 0, overlapVisits = 0;
		const auto add = [&](int x, int y)
		{
			quint8& sample = At(x, y);
			if(sample < marker)
				sample += marker;
			if(sample == 2)
				++newVisits;
			else if(sample == 3)
				++overlapVisits;
		};
		const Bounds bounds = WalkPerimeter(polygon, add);
		FillInterior(bounds, marker, false, add);
		if(newVisits && overlapVisits < 2)
			SetError(error, "Connect to the existing room on this deck, or choose a different room number.");
		return newVisits != 0 && overlapVisits >= 2;
	}

	bool Zero(const QVector<QPoint>& polygon)
	{
		// 180177770: perimeter 0x56 and interior 9 must remain distinct until
		// scan conversion finishes, then repair the surviving boundary.
		int removed = 0;
		const Bounds bounds = WalkPerimeter(polygon, [&](int x, int y)
		{
			quint8& sample = At(x, y);
			if(sample != 0x56)
			{
				if(sample != 0) ++removed;
				sample = 0x56;
			}
		});
		FillInterior(bounds, 0, true, [&](int x, int y)
		{
			quint8& sample = At(x, y);
			if(sample != 9 && sample != 0x56)
			{
				if(sample != 0) ++removed;
				sample = 9;
			}
		});
		for(int y = bounds.ymin; y <= bounds.ymax; ++y)
			for(int x = bounds.xmin; x <= bounds.xmax; ++x)
				if(At(x, y) == 9)
					At(x, y) = 0;
		for(int y = bounds.ymin; y <= bounds.ymax; ++y)
			for(int x = bounds.xmin; x <= bounds.xmax; ++x)
			{
				if(At(x, y) != 0x56)
					continue;
				At(x, y) = 1;
				const int rx = x % 4, ry = y % 4;
				const int bx = x - rx, by = y - ry;
				bool supported = true;
				if(rx == 0)
				{
					if(ry == 0)
						supported = At(x + 2, y + 1) || At(x + 1, y + 2)
							|| At(x - 1, y + 2) || At(x - 2, y + 1)
							|| At(x - 2, y - 1) || At(x - 1, y - 2)
							|| At(x + 1, y - 2) || At(x + 2, y - 1);
					else
						supported = At(bx - 1, by + 2) || At(bx + 1, by + 2);
				}
				else if(rx == 2 && ry == 2)
					supported = At(x + 1, y) || At(x, y + 1)
						|| At(x - 1, y) || At(x, y - 1);
				else if(ry == 0)
					supported = At(bx + 2, by - 1) || At(bx + 2, by + 1);
				else if(rx == 1 && ry == 1)
					supported = At(bx + 2, by + 1) || At(bx + 1, by + 2);
				else if(rx == 1 && ry == 3)
					supported = At(bx + 1, by + 2) || At(bx + 2, by + 3);
				else if(rx == 3 && ry == 3)
					supported = At(bx + 3, by + 2) || At(bx + 2, by + 3);
				else if(rx == 3 && ry == 1)
					supported = At(bx + 2, by + 1) || At(bx + 3, by + 2);
				if(supported)
					--removed;
				else
					At(x, y) = 0;
			}
		return removed != 0;
	}

	int CornerCandidate(int x, int y, int direction) const
	{
		// For each clockwise-boundary outgoing ray: midpoint, empty left
		// sample and occupied right sample. 180170770 and 180170990.
		static const int test[8][6] = {
			{2, 0, 2, 1, 2, -1}, {1, 1, 1, 2, 2, 1},
			{0, 2, -1, 2, 1, 2}, {-1, 1, -2, 1, -1, 2},
			{-2, 0, -2, -1, -2, 1}, {-1, -1, -1, -2, -2, -1},
			{0, -2, 1, -2, -1, -2}, {1, -1, 2, -1, 1, -2}
		};
		const int* p = test[direction];
		if(!At(x + p[0], y + p[1]) || At(x + p[2], y + p[3]))
			return -1;
		return At(x + p[4], y + p[5]) ? direction : -2;
	}

	int Corner(int x, int y, int incoming = -1) const
	{
		if(!At(x, y))
			return -1;
		if(incoming >= 0)
		{
			int direction = (incoming + 3) % 8;
			for(int i = 0; i < 7; ++i, direction = (direction + 7) % 8)
			{
				const int result = CornerCandidate(x, y, direction);
				if(result != -1)
					return result;
			}
			return -1;
		}
		int found = -1;
		for(int direction = 0; direction < 8; ++direction)
		{
			const int result = CornerCandidate(x, y, direction);
			if(result == -2 || (result >= 0 && found >= 0))
				return -2;
			if(result >= 0) found = result;
		}
		return found;
	}

	int Center(int x, int y) const
	{
		// 18016f740, with the same uniqueness rule as the initial corner test.
		if(!At(x, y))
			return -1;
		static const int test[4][6] = {
			{1, 1, 0, 1, 1, 0}, {-1, 1, -1, 0, 0, 1},
			{-1, -1, 0, -1, -1, 0}, {1, -1, 1, 0, 0, -1}
		};
		int found = -1;
		for(int i = 0; i < 4; ++i)
		{
			const int* p = test[i];
			if(!At(x + p[0], y + p[1]) || At(x + p[2], y + p[3]))
				continue;
			if(!At(x + p[4], y + p[5]) || found >= 0)
				return -2;
			found = i * 2 + 1;
		}
		return found;
	}

	bool Chase(QPolygonF& result, bool requireAllCorners) const
	{
		// 18016fc70. Preserve scan order, vertex order and the original
		// distinction between addition and subtraction's all-corners check.
		result.clear();
		int xmin = 515, ymin = 515, xmax = 0, ymax = 0, totalCorners = 0;
		for(int y = 0; y < 515; y += 4)
			for(int x = 0; x < 515; x += 4)
				if(At(x, y))
				{
					xmin = std::min(xmin, x); xmax = std::max(xmax, x);
					ymin = std::min(ymin, y); ymax = std::max(ymax, y);
					const int edge = Corner(x, y);
					if(edge == -2) return false;
					if(edge != -1) ++totalCorners;
				}
		if(xmax == 0)
			return true;
		int startX = NoScanStart, startY = NoScanStart, firstDirection = -1;
		bool corner = false;
		for(int y = ymin; y <= ymax && firstDirection < 0; y += 4)
			for(int x = xmin; x <= xmax; x += 4)
			{
				if(!At(x, y)) continue;
				firstDirection = Corner(x, y);
				if(firstDirection == -2) return false;
				if(firstDirection != -1)
				{
					corner = true; startX = x; startY = y;
					break;
				}
				firstDirection = Center(x, y);
				if(firstDirection == -2) return false;
				if(firstDirection != -1)
				{
					startX = x + 2; startY = y + 2;
					break;
				}
			}
		if(firstDirection < 0)
			return true;
		const auto worldPoint = [](int x, int y)
			{ return QPointF((x - RasterOrigin) * RasterSpacing,
				(y - RasterOrigin) * RasterSpacing); };
		result.append(worldPoint(startX, startY));
		static const int steps[8][2] = {
			{4, 0}, {2, 2}, {0, 4}, {-2, 2},
			{-4, 0}, {-2, -2}, {0, -4}, {2, -2}
		};
		int x = startX, y = startY, direction = firstDirection, visitedCorners = 0;
		for(int remaining = 10000; remaining > 0; --remaining)
		{
			x += steps[direction][0]; y += steps[direction][1];
			if(steps[direction][0] && steps[direction][1])
				corner = !corner;
			int nextDirection;
			if(corner)
			{
				++visitedCorners;
				nextDirection = Corner(x, y, direction);
			}
			else
				nextDirection = Center(x, y);
			if(nextDirection < 0)
				return false;
			const QPointF point = worldPoint(x, y);
			const bool closed = x == startX && y == startY;
			if(!closed && PointOnSegments(result, point))
				return false;
			if(nextDirection != direction)
				result.append(point);
			if(closed || remaining == 1)
			{
				if(requireAllCorners && totalCorners != visitedCorners)
					return false;
				if(nextDirection == firstDirection)
					result.remove(0);
				if(result.size() >= 3)
					return true;
				result.clear();
				return false;
			}
			direction = nextDirection;
		}
		return false;
	}
};

bool Prepare(const QPolygonF& boundary, const QPolygonF& cell,
	QVector<QPoint>& oldPoints, QVector<QPoint>& newPoints, QString* error)
{
	if(error) error->clear();
	return RasterPoints(boundary, oldPoints, error) && RasterPoints(cell, newPoints, error);
}

bool Trace(Raster& raster, QPolygonF& result, bool requireAllCorners, QString* error)
{
	if(raster.Chase(result, requireAllCorners))
		return true;
	SetError(error, "This edit cannot form a single valid legacy room boundary.");
	return false;
}
}

QPolygonF FillShape(quint8 mask, const QPointF& cursor)
{
	if(mask >= 16 || Shapes[mask][0].count == 0)
		return {};
	const double sx = cursor.x() / GridSpacing, sy = cursor.y() / GridSpacing;
	if(!std::isfinite(sx) || !std::isfinite(sy)
		|| sx < -2147483647.0 || sx >= 2147483648.0
		|| sy < -2147483647.0 || sy >= 2147483648.0)
		return {};
	const int ix = int(sx) - (sx < 0.0), iy = int(sy) - (sy < 0.0);
	const double fx = sx - ix, fy = sy - iy;
	int sector;
	if(fx < 0.5)
	{
		if(fy <= 0.5)
			sector = 2 + (0.5 - fx <= 0.5 - fy);
		else
			sector = 1 + (fy - 0.5 <= 0.5 - fx);
	}
	else if(fy <= 0.5)
		sector = 0.5 - fy < fx - 0.5 ? 0 : 3;
	else
		sector = fx < fy;
	const Shape& shape = Shapes[mask][sector];
	QPolygonF result;
	result.reserve(shape.count);
	for(int i = 0; i < shape.count; ++i)
		result.append(QPointF(shape.xy[i * 2] * CoordinateSpacing + ix * GridSpacing,
			shape.xy[i * 2 + 1] * CoordinateSpacing + iy * GridSpacing));
	return result;
}

bool IsSupportedBoundary(const QPolygonF& boundary, QString* error)
{
	if(error) error->clear();
	QVector<QPoint> points;
	return RasterPoints(boundary, points, error);
}

bool Merge(QPolygonF& boundary, const QPolygonF& cell, QString* error)
{
	QVector<QPoint> oldPoints, newPoints;
	if(!Prepare(boundary, cell, oldPoints, newPoints, error))
		return false;
	Raster raster;
	raster.Add(oldPoints, 1);
	if(!raster.Add(newPoints, 2, error))
		return false;
	QPolygonF result;
	if(!Trace(raster, result, false, error) || EqualBoundary(result, boundary))
		return false;
	boundary = result;
	return true;
}

bool CanSubtract(const QPolygonF& boundary, const QPolygonF& cell, QString* error)
{
	QVector<QPoint> oldPoints, newPoints;
	if(!Prepare(boundary, cell, oldPoints, newPoints, error))
		return false;
	Raster raster;
	raster.Add(oldPoints, 1);
	if(!raster.Zero(newPoints))
		return true;
	QPolygonF result;
	return Trace(raster, result, true, error);
}

bool Subtract(QPolygonF& boundary, const QPolygonF& cell, QString* error)
{
	QVector<QPoint> oldPoints, newPoints;
	if(!Prepare(boundary, cell, oldPoints, newPoints, error))
		return false;
	Raster raster;
	raster.Add(oldPoints, 1);
	if(!raster.Zero(newPoints))
		return false;
	QPolygonF result;
	if(!Trace(raster, result, true, error) || EqualBoundary(result, boundary))
		return false;
	boundary = result;
	return true;
}
}
}
