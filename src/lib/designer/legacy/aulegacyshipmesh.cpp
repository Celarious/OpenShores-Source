#include "aulegacyshipmesh.h"
#include "aulegacybuildingpalette.h"
#include "aulegacyshipgeometry.h"
#include "aulegacybd.h"
#include "aulegacyshipedit.h"
#include "aulegacyshipequipment.h"
#include "aulegacyshipvehicles.h"
#include "aulegacyshipstairs.h"
#include "aulegacyshipdetails.h"
#include "aupoint.h"

#include <QHash>
#include <QPoint>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace AuLegacyShip
{
namespace Rendering
{
namespace
{
constexpr int RasterSize = 516;
constexpr int RasterOrigin = 256;
constexpr double RasterSpacing = 1.41421356225;
constexpr double Epsilon = 1e-5;
constexpr int MaximumVertices = 6000000;

struct ProfilePoint
{
	double offset;
	double height;
	int strip;
};

// AuData13!DnRoom::WallProfile, 180174d00. Each strip value describes
// the segment beginning at that point: 4 opaque hull, 5 hull window.
QVector<ProfilePoint> WallProfile(int type, int armor)
{
	QVector<ProfilePoint> result;
	const auto point = [&](double offset, double height, int strip)
		{ result.append({offset, height, strip}); };
	switch(type)
	{
	case 1:
		point(0, 0, armor < 2 ? 5 : 4);
		point(1, 1, armor < 6 ? 5 : 4); point(1, 6, 5); break;
	case 2:
		point(0, 0, 5); point(2, 1, 5);
		if(armor > 0) point(2, 1.5 + .5 * armor, 5);
		break;
	case 3:
		point(0, 0, 4); point(1, armor < 3 ? 4 : armor < 6 ? 5 : armor < 9 ? 6 : 7, 5);
		if(armor < 9) point(1, 7, 4);
		break;
	case 4:
		point(0, 0, armor < 6 ? 5 : 4); point(2, 2, 5); point(2, 6, armor < 9 ? 5 : 4); break;
	case 5:
		point(0, 0, armor < 3 ? 5 : 4); point(2, 1, 4); point(2, 7, armor < 7 ? 5 : 4); break;
	case 6:
	case 15:
		point(0, 0, armor < 2 ? 5 : 4); point(1, 1, armor < 4 ? 5 : 4);
		if(armor >= 4 && armor < 9) point(1, 6, 5);
		point(1, 7, armor < 7 ? 5 : 4); break;
	case 7:
		point(0, 0, armor < 8 ? 5 : 4); point(1, 4, armor < 3 ? 5 : 4); point(1, 5, 5); break;
	case 8:
		point(0, 0, 4);
		point(armor < 6 ? 1 : 1.5, armor < 6 || armor == 10 ? 1 : 1.5, 4);
		if(armor > 0) point(armor < 6 ? 1 : 1.5, armor < 6 ? armor + 1 : armor - 3.5, 4);
		break;
	case 9:
		point(0, 0, 4); point(2, armor < 6 ? 2 : 1, 4); point(2, armor < 6 ? 6 : 7, 4); break;
	case 10:
		point(0, 0, armor < 2 ? 5 : 4); point(1, 3, armor < 8 ? 5 : 4);
		point(1, 7, armor < 6 || (armor >= 8 && armor < 10) ? 5 : 4); break;
	case 11:
		point(0, 0, armor >= 4 && armor < 7 ? 5 : 4); point(1, 1, armor < 4 ? 5 : 4);
		point(1, 7, armor >= 4 && armor < 7 ? 5 : 4); break;
	case 13:
		point(0, 0, armor < 3 ? 5 : 4); point(2, 1, 4); point(2, 7, armor < 6 ? 5 : 4); break;
	case 14:
		point(0, 0, armor < 5 ? 5 : 4); point(1, 1, 5); point(2, 7, armor < 8 ? 5 : 4); break;
	case 16:
		point(0, 0, armor < 6 ? 5 : 4); point(.75, 7, armor < 3 || (armor >= 6 && armor < 8) ? 5 : 4); break;
	case 17:
		point(0, 0, 4); point(.75, 2, armor < 5 ? 5 : 4);
		point(1.75, 6, armor < 2 || (armor >= 5 && armor < 8) ? 5 : 4); break;
	case 18:
		point(0, 0, 4); point(1, 3, armor < 5 ? 5 : 4);
		point(1, 7, armor < 2 || (armor >= 5 && armor < 9) ? 5 : 4); break;
	case 19:
		point(0, 0, armor < 5 ? 5 : 4); point(1, armor < 9 ? 1 : 2, 5); break;
	case 20:
		point(0, 0, 4); point(1, 2, armor < 3 ? 5 : 4); point(1, 6, armor < 9 ? 5 : 4); break;
	case 21:
		point(0, 0, armor < 6 ? 5 : 4); point(1.75, 2, 5);
		point(1, 7, armor < 3 || (armor >= 6 && armor < 8) ? 5 : 4); break;
	case 28:
		point(0, 0, 1); break;
	case 29:
		point(0, 0, 0); break;
	case 30:
		point(0, 0, 2); point(0, 7, 4); point(1, 7, armor < 9 ? 5 : 4); break;
	case 31:
		point(0, 0, armor > 3 ? 4 : 5);
		point(1.5, 4 - CoordinateSpacing, 3);
		point(1.5, 4 + CoordinateSpacing, 5); break;
	default:
		point(0, 0, 4); break;
	}
	point(0, DeckHeight, 0);
	return result;
}

int Type(quint16 sample) { return (sample >> 8) & 31; }
bool Current(quint16 sample) { return sample && !(sample & 0x8000); }
bool WindowFloor(int type, int armor)
{
	return type == 2 || type == 14 || (type == 21 && armor < 8) || (type == 4 && armor < 7);
}
bool HasInterior(int type) { return type != 9 && !(type >= 22 && type <= 24); }
bool IsHatch(int type) { return type == 8 || type == 51 || type == 52 || (type >= 77 && type <= 97); }
bool IsSideTurret(int type) { return type == 21 || (type >= 34 && type <= 40); }

struct Hatch
{
	QPointF center;
	QPolygonF boundary;
	Material material;
	quint32 doorId = 0;
	bool diamond;
	bool turret = false;
};

QVector<Hatch> Hatches(const QVector<Detail>& details, int deck)
{
	QVector<Hatch> result;
	QSet<quint16> positions;
	for(const Detail& detail : details)
	{
		const bool turret = detail.detailType == 22 || detail.detailType == 33;
		if(detail.deck != deck || (!IsHatch(detail.detailType) && !turret)) continue;
		if(positions.contains(detail.packedXY)) continue;
		positions.insert(detail.packedXY);
		Hatch hatch;
		hatch.center = DecodePoint(detail.packedXY);
		hatch.doorId = turret ? 0 : detail.detailId;
		hatch.turret = turret;
		const int x = int(std::round(hatch.center.x() / RasterSpacing)) + RasterOrigin;
		const int y = int(std::round(hatch.center.y() / RasterSpacing)) + RasterOrigin;
		hatch.diamond = x % 4 != 2 || y % 4 != 2;
		hatch.material = detail.detailType == 8 || (detail.detailType >= 77 && detail.detailType <= 83)
			? Material::InteriorHatch : detail.detailType == 51 || (detail.detailType >= 84 && detail.detailType <= 90)
				? Material::CrewHatch : Material::HullHatch;
		if(turret)
			hatch.boundary << hatch.center + QPointF(CoordinateSpacing, 0)
				<< hatch.center + QPointF(2, 2) << hatch.center + QPointF(0, CoordinateSpacing)
				<< hatch.center + QPointF(-2, 2) << hatch.center + QPointF(-CoordinateSpacing, 0)
				<< hatch.center + QPointF(-2, -2) << hatch.center + QPointF(0, -CoordinateSpacing)
				<< hatch.center + QPointF(2, -2);
		else if(hatch.diamond)
			hatch.boundary << hatch.center + QPointF(-CoordinateSpacing, 0)
				<< hatch.center + QPointF(0, -CoordinateSpacing) << hatch.center + QPointF(CoordinateSpacing, 0)
				<< hatch.center + QPointF(0, CoordinateSpacing);
		else
			hatch.boundary << hatch.center + QPointF(-2, -2) << hatch.center + QPointF(2, -2)
				<< hatch.center + QPointF(2, 2) << hatch.center + QPointF(-2, 2);
		result.append(hatch);
	}
	return result;
}

QPointF PlanPoint(const QPoint& point)
{
	return QPointF((point.x() - RasterOrigin) * RasterSpacing,
		(point.y() - RasterOrigin) * RasterSpacing);
}
QVector3D Position(const QPointF& point, double z)
{
	return QVector3D(float(point.x()), float(point.y()), float(z));
}
double Cross(const QPointF& a, const QPointF& b) { return a.x() * b.y() - a.y() * b.x(); }
double Length(const QPointF& a) { return std::hypot(a.x(), a.y()); }
QPointF Unit(const QPointF& a)
{
	const double length = Length(a);
	return length > Epsilon ? a / length : QPointF();
}
quint32 PointKey(const QPoint& p) { return quint32(p.x() * RasterSize + p.y()); }

struct Raster
{
	QVector<quint16> samples = QVector<quint16>(RasterSize * RasterSize, 0);
	QVector<int> owners = QVector<int>(RasterSize * RasterSize, -1);
	quint16 At(int x, int y) const
	{
		return x >= 0 && y >= 0 && x < RasterSize && y < RasterSize
			? samples.at(x * RasterSize + y) : 0;
	}
	int Owner(int x, int y) const
	{
		return x >= 0 && y >= 0 && x < RasterSize && y < RasterSize
			? owners.at(x * RasterSize + y) : -1;
	}

	// The 16-bit floor buffer uses a perimeter walk followed by directed
	// rightward fills. Open-below keeps the previous room's tag, except fuel.
	// This is distinct from the designer's 8-bit paint occupancy buffer.
	void Fill(const QPolygonF& boundary, int type, int subtype, int owner)
	{
		QVector<QPoint> polygon;
		for(const QPointF& point : boundary)
		{
			const QPoint p(int(std::round(point.x() / RasterSpacing)) + RasterOrigin,
				int(std::round(point.y() / RasterSpacing)) + RasterOrigin);
			if(polygon.isEmpty() || p != polygon.last()) polygon.append(p);
		}
		if(polygon.size() > 1 && polygon.first() == polygon.last()) polygon.removeLast();
		if(polygon.size() < 3) return;
		double area = 0;
		for(int i = 0; i < polygon.size(); ++i)
			area += Cross(polygon.at(i), polygon.at((i + 1) % polygon.size()));
		if(area > 0) std::reverse(polygon.begin(), polygon.end());
		const quint16 tag = quint16(((type & 31) << 8) | subtype);
		const bool openBelow = type == 11 || type == 15;
		int xmin = polygon.first().x(), xmax = xmin, ymin = polygon.first().y(), ymax = ymin;
		for(const QPoint& p : polygon)
		{
			xmin = std::min(xmin, p.x()); xmax = std::max(xmax, p.x());
			ymin = std::min(ymin, p.y()); ymax = std::max(ymax, p.y());
		}
		const int height = ymax - ymin + 1;
		QVector<quint16> temporary;
		if(openBelow) temporary.fill(0, (xmax - xmin + 1) * height);
		const auto write = [&](int x, int y)
		{
			const int index = x * RasterSize + y;
			const quint16 old = samples.at(index);
			quint16 value = tag;
			if(old & 0xc000)
				value = openBelow && Type(old) != 9 ? quint16((old & 0x1fff) | 0x6000)
					: quint16(tag | 0x4000);
			if(openBelow) temporary[(x - xmin) * height + y - ymin] = value;
			else { samples[index] = value; owners[index] = owner; }
		};
		QVector<QPoint> perimeter;
		QPoint previous = polygon.last();
		for(const QPoint& p : polygon)
		{
			const QPoint step((p.x() > previous.x()) - (p.x() < previous.x()),
				(p.y() > previous.y()) - (p.y() < previous.y()));
			while(previous != p)
			{
				perimeter.append(previous);
				write(previous.x(), previous.y());
				previous += step;
			}
		}
		if(perimeter.size() < 3) return;
		for(int i = 0; i < perimeter.size(); ++i)
		{
			const QPoint p = perimeter.at(i);
			const QPoint incoming = p - perimeter.at((i + perimeter.size() - 1) % perimeter.size());
			const QPoint outgoing = perimeter.at((i + 1) % perimeter.size()) - p;
			bool fill = false;
			if(outgoing.x() == 1)
				fill = outgoing.y() == 1 && !(incoming.x() == -1 && incoming.y() >= -1 && incoming.y() <= 0);
			else if(outgoing.x() == 0)
				fill = outgoing.y() == 1
					? !(incoming.x() == -1 && incoming.y() >= -1 && incoming.y() <= 0)
						&& !(incoming.x() == 0 && incoming.y() == -1)
					: incoming.x() == -1 && incoming.y() == 1;
			else if(outgoing.y() == 1)
				fill = incoming.y() == 1 || (incoming.x() == 1 && incoming.y() == 0);
			else if(outgoing.y() == 0 || incoming.x() != 1)
				fill = incoming.x() == -1 && incoming.y() == 1;
			if(!fill) continue;
			for(int x = p.x() + 1; x <= xmax; ++x)
			{
				const int index = x * RasterSize + p.y();
				if(openBelow ? temporary.at((x - xmin) * height + p.y() - ymin) != 0 : (samples.at(index) & 0x9fff) == tag) break;
				write(x, p.y());
			}
		}
		if(openBelow)
			for(int x = xmin; x <= xmax; ++x)
				for(int y = ymin; y <= ymax; ++y)
				{
					const quint16 value = temporary.at((x - xmin) * height + y - ymin);
					if(value) { const int i = x * RasterSize + y; samples[i] = value; owners[i] = owner; }
				}
	}

	void NextDeck()
	{
		for(quint16& sample : samples)
		{
			if(sample & 0x8000) sample = 0;
			else if(sample) sample = quint16((sample & ~0x2000) | 0x8000);
		}
	}
};

struct Builder
{
	Mesh mesh;
	QHash<quint64, int> batchIndices;
	int vertices = 0;
	bool truncated = false;

	void Warning(const QString& message)
	{
		if(!mesh.warnings.contains(message)) mesh.warnings.append(message);
	}

	void Triangle(Material material, int deck, bool interior,
		const QVector3D& a, const QVector3D& b, const QVector3D& c,
		const QVector2D& ua, const QVector2D& ub, const QVector2D& uc, quint32 doorId = 0)
	{
		if(vertices >= MaximumVertices)
		{
			truncated = true;
			return;
		}
		QVector3D normal = QVector3D::crossProduct(b - a, c - a);
		if(normal.lengthSquared() < 1e-10f) return;
		normal.normalize();
		const quint32 surface = quint32(((deck - MinimumDeck + 1) * 2 + int(interior)) * 128 + int(material));
		const quint64 key = (quint64(doorId) << 32) | surface;
		int index = batchIndices.value(key, -1);
		if(index < 0)
		{
			index = mesh.batches.size();
			batchIndices.insert(key, index);
			Batch batch;
			batch.material = material; batch.deck = deck; batch.interior = interior;
			batch.doorId = doorId;
			mesh.batches.append(batch);
		}
		QVector<Vertex>& out = mesh.batches[index].vertices;
		out.append({a, normal, ua}); out.append({b, normal, ub}); out.append({c, normal, uc});
		if(vertices == 0) mesh.minimum = mesh.maximum = a;
		for(const QVector3D& p : {a, b, c})
			for(int axis = 0; axis != 3; ++axis)
			{
				mesh.minimum[axis] = std::min(mesh.minimum[axis], p[axis]);
				mesh.maximum[axis] = std::max(mesh.maximum[axis], p[axis]);
			}
		vertices += 3;
	}

	void Quad(Material material, int deck, bool interior,
		const QVector3D& a, const QVector3D& b, const QVector3D& c, const QVector3D& d,
		float repeat = 1, quint32 doorId = 0)
	{
		Triangle(material, deck, interior, a, b, c, {0, 0}, {repeat, 0}, {repeat, 1}, doorId);
		Triangle(material, deck, interior, a, c, d, {0, 0}, {repeat, 1}, {0, 1}, doorId);
	}
};

void SplitPolygon(const QPolygonF& polygon, const QPointF& a, const QPointF& b,
	QPolygonF& inside, QPolygonF& outside)
{
	if(polygon.isEmpty()) return;
	QPointF previous = polygon.last();
	double oldDistance = Cross(b - a, previous - a);
	for(const QPointF& point : polygon)
	{
		const double distance = Cross(b - a, point - a);
		if((distance > Epsilon && oldDistance < -Epsilon) || (distance < -Epsilon && oldDistance > Epsilon))
		{
			const QPointF crossing = previous + (point - previous) * (oldDistance / (oldDistance - distance));
			inside.append(crossing); outside.append(crossing);
		}
		if(distance >= -Epsilon) inside.append(point);
		if(distance <= Epsilon) outside.append(point);
		previous = point; oldDistance = distance;
	}
}

QVector<QVector3D> TurretPoints(const QVector3D& center, const QVector3D& right,
	const QVector3D& outward, const QVector3D& up)
{
	// DaSpaceShip::RenderTurret and RenderExteriorWalls use the same 13 points.
	const float r = float(CoordinateSpacing);
	const QVector3D shape[] = {{r, 0, 0}, {2, 0, -2}, {0, 0, -r}, {-2, 0, -2},
		{-r, 0, 0}, {-2, 0, 2}, {0, 0, r}, {2, 0, 2},
		{2, 2, 0}, {0, 2, -2}, {-2, 2, 0}, {0, 2, 2}, {0, r, 0}};
	QVector<QVector3D> points;
	for(const QVector3D& p : shape)
		points.append(center + right * p.x() + outward * p.y() + up * p.z());
	return points;
}

void TurretDome(Builder& out, const QVector<QVector3D>& points, int deck)
{
	static const int triangles[][3] = {{8,12,9}, {1,8,9}, {8,1,0}, {8,0,7},
		{9,12,10}, {3,9,10}, {9,3,2}, {9,2,1}, {10,12,11}, {5,10,11},
		{10,5,4}, {10,4,3}, {11,12,8}, {7,11,8}, {11,7,6}, {11,6,5}};
	// Both original dome paths use the lower-left glass half of ST_Turret.
	// The upper-right hull half belongs to the surrounding wall corners.
	const QVector2D uv[3] = {{0, 0}, {0, 1}, {1, 0}};
	for(const auto& tri : triangles)
	{
		out.Triangle(Material::Turret, deck, false, points[tri[0]], points[tri[2]], points[tri[1]], uv[0], uv[2], uv[1]);
		out.Triangle(Material::Turret, deck, true, points[tri[0]], points[tri[1]], points[tri[2]], uv[0], uv[1], uv[2]);
	}
}

void SideTurret(Builder& out, int deck, const QVector3D& a0, const QVector3D& b0,
	const QVector3D& b1, const QVector3D& a1)
{
	const QVector3D center = (a0 + b1) * .5f;
	const QVector3D right = (a0 - b0).normalized();
	const QVector3D outward(right.y(), -right.x(), 0);
	// right points opposite the boundary walk, so its left normal faces out.
	const QVector3D facing = -outward;
	const QVector<QVector3D> points = TurretPoints(center, right, facing, {0, 0, 1});
	TurretDome(out, points, deck);
	// RenderExteriorWalls maps a complete tile onto each corner separately.
	const QVector3D corners[] = {a0, a1, b1, b0};
	const int ring[][3] = {{0, 1, 2}, {6, 7, 0}, {4, 5, 6}, {2, 3, 4}};
	const QVector2D cornerUV(1, 1), firstUV(1, 0), lastUV(0, 1);
	const QVector2D bevelUV(.6767767f, .6767767f);
	const auto triangle = [&](const QVector3D& a, const QVector3D& b, const QVector3D& c,
		const QVector2D& ua, const QVector2D& ub, const QVector2D& uc)
	{
		out.Triangle(Material::Turret, deck, false, a, c, b, ua, uc, ub);
		out.Triangle(Material::Turret, deck, true, a, b, c, ua, ub, uc);
	};
	for(int i = 0; i < 4; ++i)
	{
		triangle(corners[i], points[ring[i][0]], points[ring[i][1]], cornerUV, firstUV, bevelUV);
		triangle(corners[i], points[ring[i][1]], points[ring[i][2]], cornerUV, bevelUV, lastUV);
	}
	if((a0 - b0).length() > 2 * CoordinateSpacing + Epsilon)
	{
		triangle(b0, points[4], b1, {0, 1}, {1, 1}, {1, 0});
		triangle(a1, points[0], a0, {0, 1}, {1, 1}, {1, 0});
	}
}

void Plane(Builder& out, const QPoint (&points)[3], int x, int y, double z,
	Material material, int deck, bool interior, bool up, const QVector<Hatch>& hatches)
{
	const auto emitPolygon = [&](const QPolygonF& polygon, Material texture, const Hatch* hatch)
	{
		const auto uv = [&](const QPointF& point)
		{
			QVector2D result;
			if(hatch)
			{
				const QPointF p = point - hatch->center;
				if(hatch->diamond)
					result = QVector2D(float((p.x() + p.y()) / (2 * CoordinateSpacing) + .5),
						float((p.y() - p.x()) / (2 * CoordinateSpacing) + .5));
				else result = QVector2D(float(p.x() / 4 + .5), float(p.y() / 4 + .5));
			}
			else
			{
				result = QVector2D(float((point.x() / RasterSpacing + RasterOrigin - x) / 4),
					float((point.y() / RasterSpacing + RasterOrigin - y) / 4));
				if(texture == Material::InteriorCeiling) result.setX(1 - result.x());
			}
			return result;
		};
		for(int i = 1; i + 1 < polygon.size(); ++i)
		{
			const QPointF a = polygon.first(), b = polygon.at(up ? i : i + 1), c = polygon.at(up ? i + 1 : i);
			out.Triangle(texture, deck, interior, Position(a, z), Position(b, z), Position(c, z),
				uv(a), uv(b), uv(c), hatch ? hatch->doorId : 0);
		}
	};
	QPolygonF triangle;
	for(const QPoint& p : points) triangle.append(PlanPoint(p));
	if(hatches.isEmpty()) { emitPolygon(triangle, material, nullptr); return; }
	QVector<QPolygonF> remainder = {triangle};
	for(const Hatch& hatch : hatches)
	{
		if(!triangle.boundingRect().intersects(hatch.boundary.boundingRect())) continue;
		QVector<QPolygonF> next;
		for(const QPolygonF& polygon : remainder)
		{
			QPolygonF intersection = polygon;
			for(int i = 0; i < hatch.boundary.size() && !intersection.isEmpty(); ++i)
			{
				QPolygonF inside, outside;
				SplitPolygon(intersection, hatch.boundary.at(i), hatch.boundary.at((i + 1) % hatch.boundary.size()), inside, outside);
				if(outside.size() >= 3) next.append(outside);
				intersection = inside;
			}
			// The renderer selects each hatch's closed/half/open atlas frame.
			if(!hatch.turret) emitPolygon(intersection, hatch.material, &hatch);
		}
		remainder = next;
	}
	for(const QPolygonF& polygon : remainder) emitPolygon(polygon, material, nullptr);
}

struct Edge
{
	QPoint a;
	QPoint b;
	quint16 tag;
	quint16 adjacent;
	int owner;
	int adjacentOwner;
	Edge(const QPoint& from, const QPoint& to, quint16 room, quint16 neighbor = 0,
		int roomOwner = -1, int neighborOwner = -1)
		: a(from), b(to), tag(room), adjacent(neighbor), owner(roomOwner), adjacentOwner(neighborOwner) {}
};

QVector<Edge> MergeEdges(const QVector<Edge>& edges)
{
	QHash<quint32, QVector<int>> starts, ends;
	for(int i = 0; i < edges.size(); ++i)
	{
		starts[PointKey(edges.at(i).a)].append(i);
		ends[PointKey(edges.at(i).b)].append(i);
	}
	QVector<bool> used(edges.size(), false);
	QVector<Edge> merged;
	for(int i = 0; i < edges.size(); ++i)
	{
		if(used.at(i)) continue;
		Edge edge = edges.at(i);
		used[i] = true;
		for(int direction : {1, -1})
		{
			bool extended;
			do
			{
				extended = false;
				const QVector<int> candidates = direction > 0
					? starts.value(PointKey(edge.b)) : ends.value(PointKey(edge.a));
				for(int candidate : candidates)
				{
					if(used.at(candidate)) continue;
					const Edge& other = edges.at(candidate);
					if(other.tag != edge.tag || other.adjacent != edge.adjacent
						|| other.owner != edge.owner || other.adjacentOwner != edge.adjacentOwner) continue;
					const QPointF a = edge.b - edge.a, b = other.b - other.a;
					if(Cross(a, b) != 0 || QPointF::dotProduct(a, b) <= 0) continue;
					if(direction > 0) edge.b = other.b; else edge.a = other.a;
					used[candidate] = true;
					extended = true;
					break;
				}
			} while(extended);
		}
		merged.append(edge);
	}
	return merged;
}

QVector<QVector<Edge>> Loops(const QVector<Edge>& edges)
{
	QHash<quint32, QVector<int>> starts;
	for(int i = 0; i < edges.size(); ++i) starts[PointKey(edges.at(i).a)].append(i);
	QVector<bool> used(edges.size(), false);
	QVector<QVector<Edge>> loops;
	for(int i = 0; i < edges.size(); ++i)
	{
		if(used.at(i)) continue;
		QVector<Edge> loop;
		int next = i;
		while(next >= 0 && !used.at(next))
		{
			used[next] = true;
			const Edge edge = edges.at(next);
			loop.append(edge);
			if(edge.b == loop.first().a) break;
			next = -1;
			double bestTurn = -4;
			const QPointF incoming = edge.b - edge.a;
			for(int candidate : starts.value(PointKey(edge.b)))
			{
				if(used.at(candidate)) continue;
				const QPointF outgoing = edges.at(candidate).b - edge.b;
				const double turn = std::atan2(Cross(incoming, outgoing),
					QPointF::dotProduct(incoming, outgoing));
				if(turn > bestTurn) { next = candidate; bestTurn = turn; }
			}
		}
		if(loop.size() >= 3 && loop.last().b == loop.first().a) loops.append(loop);
	}
	return loops;
}

QPointF JoinNormal(const QPointF& previous, const QPointF& next)
{
	const QPointF a = Unit(previous), b = Unit(next);
	const QPointF na(a.y(), -a.x()), nb(b.y(), -b.x());
	const double denominator = 1 + QPointF::dotProduct(na, nb);
	return denominator > Epsilon ? (na + nb) / denominator : nb;
}

Material InteriorWall(int type, double length)
{
	const bool fill = length <= 5.0 + Epsilon;
	if(type == 3 || type == 5 || type == 13)
		return fill ? Material::InteriorWallCargoFill : Material::InteriorWallCargo;
	if(type == 8 || type == 16 || type == 17 || type == 21)
		return fill ? Material::InteriorWallEquipmentFill : Material::InteriorWallEquipment;
	return fill ? Material::InteriorWallFill : Material::InteriorWallFinished;
}

struct WallSpan
{
	QPointF a;
	QPointF b;
	quint16 tag;
	int detail;
	quint32 doorId;
	quint32 detailId;
};

using DetailGrid = QHash<quint32, QVector<Detail>>;

QVector<WallSpan> WallSpans(const Edge& edge, bool exterior, const DetailGrid& details)
{
	const QPointF a = PlanPoint(edge.a), b = PlanPoint(edge.b), direction = Unit(b - a);
	const double length = Length(b - a);
	struct Opening { double start; double end; int type; quint32 id; };
	QVector<Opening> openings;
	QVector<double> cuts = {0, length};
	QVector<Detail> candidates;
	QPoint cursor = edge.a;
	const QPoint step((edge.b.x() > cursor.x()) - (edge.b.x() < cursor.x()),
		(edge.b.y() > cursor.y()) - (edge.b.y() < cursor.y()));
	while(cursor != edge.b)
	{
		const auto at = details.constFind(PointKey(cursor));
		if(at != details.constEnd()) candidates += at.value();
		cursor += step;
	}
	for(const Detail& detail : candidates)
	{
		const int type = detail.detailType;
		if(exterior ? type != 3 && type != 9 && type != 50 && !IsSideTurret(type)
			: type != 5 && type != 49 && type != 50 && type != 53 && type != 54 && type != 55 && type != 56) continue;
		const QPointF position = DecodePoint(detail.packedXY) - a;
		const double at = QPointF::dotProduct(position, direction);
		if(std::abs(Cross(direction, position)) > Epsilon || at <= Epsilon || at >= length - Epsilon) continue;
		double start = at - 2.5, end = at + 2.5;
		if(IsSideTurret(type)) { start = at - CoordinateSpacing; end = at + CoordinateSpacing; }
		else if(exterior && (type == 3 || type == 9)) { start = 1; end = length - 1; }
		else if(!exterior && type >= 53) { start = 0; end = length; }
		start = std::max(0.0, start); end = std::min(length, end);
		if(end - start <= Epsilon) continue;
		openings.append({start, end, type, detail.detailId}); cuts.append(start); cuts.append(end);
	}
	std::sort(cuts.begin(), cuts.end());
	cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
	QVector<WallSpan> spans;
	for(int i = 1; i < cuts.size(); ++i)
	{
		const double middle = (cuts.at(i - 1) + cuts.at(i)) / 2;
		WallSpan span = {a + direction * cuts.at(i - 1), a + direction * cuts.at(i), edge.tag, 0, 0, 0};
		for(const Opening& opening : openings)
			if(middle > opening.start && middle < opening.end)
			{
				span.detail = opening.type;
				span.detailId = opening.id;
				span.doorId = IsDoorDetail(opening.type) ? opening.id : 0;
				if(exterior) span.tag = quint16((IsSideTurret(opening.type) ? 31
					: opening.type == 3 ? 28 : opening.type == 9 ? 29 : 30) << 8);
				break;
			}
		spans.append(span);
	}
	return spans;
}

Material HullMaterial(int strip)
{
	if(strip == 1) return Material::LargeHullDoor;
	if(strip == 2) return Material::HullDoor;
	if(strip == 3) return Material::Turret;
	return strip == 5 ? Material::HullWallWindow : Material::HullWall;
}

void HullWalls(Builder& out, const QVector<Edge>& edges, int deck, int armor, const DetailGrid& details)
{
	for(const QVector<Edge>& loop : Loops(MergeEdges(edges)))
	{
		QVector<WallSpan> spans;
		for(const Edge& edge : loop) spans += WallSpans(edge, true, details);
		for(int i = 0; i < spans.size(); ++i)
		{
			const WallSpan& edge = spans.at(i);
			const WallSpan& previous = spans.at((i + spans.size() - 1) % spans.size());
			const WallSpan& next = spans.at((i + 1) % spans.size());
			const QPointF a = edge.a, b = edge.b;
			if(edge.detail == 3 || edge.detail == 9 || edge.detail == 50)
				out.mesh.hullOpenings.append(HullOpening{a, b, deck, edge.detail, edge.detailId});
			const QPointF direction = b - a;
			const QPointF na = JoinNormal(a - previous.a, direction);
			const QPointF nb = JoinNormal(direction, next.b - b);
			const QVector<ProfilePoint> profile = WallProfile(Type(edge.tag), armor);
			bool slopedWindow = false, opening = false;
			for(int j = 1; j < profile.size(); ++j)
			{
				const ProfilePoint p = profile.at(j - 1), q = profile.at(j);
				slopedWindow |= p.strip == 5 && std::abs(p.offset - q.offset) > Epsilon;
				opening |= p.strip >= 1 && p.strip <= 3;
				if(!p.strip) continue;
				const Material material = HullMaterial(p.strip);
				const QVector3D a0 = Position(a + na * p.offset, deck * DeckHeight + p.height);
				const QVector3D b0 = Position(b + nb * p.offset, deck * DeckHeight + p.height);
				const QVector3D a1 = Position(a + na * q.offset, deck * DeckHeight + q.height);
				const QVector3D b1 = Position(b + nb * q.offset, deck * DeckHeight + q.height);
				if(p.strip == 3)
				{
					SideTurret(out, deck, a0, b0, b1, a1);
					continue;
				}
				const quint32 doorId = p.strip == 1 || p.strip == 2 ? edge.doorId : 0;
				out.Quad(material, deck, false, a0, b0, b1, a1, 1, doorId);
				// CreateModels calls RenderExterior again with reversed faces for
				// the interior model, retaining each hull strip's texture and UVs.
				out.Triangle(material, deck, true, a0, b1, b0, {0, 0}, {1, 1}, {1, 0}, doorId);
				out.Triangle(material, deck, true, a0, a1, b1, {0, 0}, {0, 1}, {1, 1}, doorId);
			}
			// Keep movement inside the baseline frame across the entire deck,
			// before the glass or its sill can deflect a walking body vertically.
			// Window strips above doors or turrets must not close their apertures.
			if(slopedWindow && !opening)
				out.mesh.hullWindowBoundaries.append(HullWindowBoundary{a, b, deck});
			// RenderWallEndCap closes both room profiles to the baseline. Its
			// interior-facing strip uses the end-cap tile and height-scaled V.
			if(Type(edge.tag) != Type(next.tag))
			{
				const QVector<ProfilePoint> adjacent = WallProfile(Type(next.tag), armor);
				const auto cap = [&](const QVector<ProfilePoint>& points, bool reverse)
				{
					// AuAtom13!RenderWallEndCap 18044e0f0: interior vertices
					// with nonzero profile offsets subtract 1/48 ft radially
					// and move 1/48 ft along the interior-facing cap normal.
					// Baseline vertices remain shared with the exterior cap.
					constexpr double inset = 1.0 / 48.0;
					// The current span ends here; its exterior cap faces along
					// the span, while the adjacent span's start cap faces back.
					// Move interior caps toward their own room, not the neighbor.
					const QPointF interiorNormal = Unit(QPointF(nb.y(), -nb.x())) * (reverse ? -1 : 1);
					const auto point = [&](const ProfilePoint& p, bool interior)
					{
						const QPointF location = interior && p.offset != 0
							? b + nb * (p.offset - inset) + interiorNormal * inset
							: b + nb * p.offset;
						return Position(location, deck * DeckHeight + p.height);
					};
					for(int j = 1; j < points.size(); ++j)
					{
						const ProfilePoint p = points.at(j - 1), q = points.at(j);
						if(p.height == q.height || (p.offset == 0 && q.offset == 0)) continue;
						const QVector2D uv[4] = {{0, float(p.height / 8)}, {1, float(p.height / 8)},
							{1, float(q.height / 8)}, {0, float(q.height / 8)}};
						for(bool interior : {false, true})
						{
							const QVector3D quad[4] = {Position(b, deck * DeckHeight + p.height),
								point(p, interior), point(q, interior),
								Position(b, deck * DeckHeight + q.height)};
							const int order[4] = {0, (reverse == interior) ? 3 : 1, 2, (reverse == interior) ? 1 : 3};
							const Material material = interior ? Material::InteriorWallEndCap : Material::HullWall;
							out.Triangle(material, deck, interior, quad[order[0]], quad[order[1]], quad[order[2]],
								uv[order[0]], uv[order[1]], uv[order[2]]);
							out.Triangle(material, deck, interior, quad[order[0]], quad[order[2]], quad[order[3]],
								uv[order[0]], uv[order[2]], uv[order[3]]);
						}
					}
				};
				cap(profile, false); cap(adjacent, true);
			}
		}
	}
}

// The four triangle samples in each original full square. Keep the diagonals
// at the original center so triangles, diamonds and concave rooms all agree
// with the floor-plan cell masks without triangulating a polygon as a fan.
const QPoint SampleOffsets[4] = {{3, 2}, {2, 3}, {1, 2}, {2, 1}};
const QPoint Corners[4] = {{4, 0}, {4, 4}, {0, 4}, {0, 0}};

void DeckSurfaces(Builder& out, const Raster& raster, int deck, int armor, const QVector<Detail>& details)
{
	const QVector<Hatch> hatches = Hatches(details, deck);
	for(const Hatch& hatch : hatches)
		if(hatch.doorId) out.mesh.hatchOpenings.append(HatchOpening{hatch.boundary, deck, hatch.doorId});
	QSet<quint16> turrets;
	for(const Detail& detail : details)
	{
		if(detail.deck != deck || (detail.detailType != 22 && detail.detailType != 33)
			|| turrets.contains(detail.packedXY)) continue;
		turrets.insert(detail.packedXY);
		const bool top = detail.detailType == 22;
		const QVector3D center = Position(DecodePoint(detail.packedXY), deck * DeckHeight);
		TurretDome(out, TurretPoints(center, {1, 0, 0}, {0, 0, top ? 1.0f : -1.0f},
			{0, top ? -1.0f : 1.0f, 0}), top ? deck - 1 : deck);
	}
	DetailGrid wallDetails;
	QSet<quint32> wallPositions;
	for(const Detail& detail : details)
	{
		const int type = detail.detailType;
		if(detail.deck != deck || (type != 3 && type != 5 && type != 9 && type != 49 && type != 50
			&& type != 53 && type != 54 && type != 55 && type != 56 && !IsSideTurret(type))) continue;
		const quint32 id = (quint32(type) << 16) | detail.packedXY;
		if(wallPositions.contains(id)) continue;
		wallPositions.insert(id);
		const QPointF p = DecodePoint(detail.packedXY);
		const QPoint grid(int(std::round(p.x() / RasterSpacing)) + RasterOrigin,
			int(std::round(p.y() / RasterSpacing)) + RasterOrigin);
		wallDetails[PointKey(grid)].append(detail);
	}
	QHash<quint32, QVector<Hatch>> hatchCells;
	const QVector<Hatch> noHatches;
	for(const Hatch& hatch : hatches)
	{
		const QRectF bounds = hatch.boundary.boundingRect();
		const int xmin = int(std::floor((bounds.left() / RasterSpacing + RasterOrigin) / 4)) * 4;
		const int xmax = int(std::floor((bounds.right() / RasterSpacing + RasterOrigin) / 4)) * 4;
		const int ymin = int(std::floor((bounds.top() / RasterSpacing + RasterOrigin) / 4)) * 4;
		const int ymax = int(std::floor((bounds.bottom() / RasterSpacing + RasterOrigin) / 4)) * 4;
		for(int x = xmin; x <= xmax; x += 4)
			for(int y = ymin; y <= ymax; y += 4) hatchCells[PointKey(QPoint(x, y))].append(hatch);
	}
	QVector<Edge> hull;
	QVector<Edge> partitions;
	for(int x = 0; x + 4 < RasterSize; x += 4)
		for(int y = 0; y + 4 < RasterSize; y += 4)
			for(int t = 0; t < 4; ++t)
			{
				const QPoint origin(x, y), sample = origin + SampleOffsets[t];
				const quint16 tag = raster.At(sample.x(), sample.y());
				const int owner = raster.Owner(sample.x(), sample.y());
				if(!tag) continue;
				const QPoint points[3] = {origin + QPoint(2, 2), origin + Corners[t], origin + Corners[(t + 1) % 4]};
				const bool window = WindowFloor(Type(tag), armor);
				const auto plane = [&](Material material, int planeDeck, bool inside, bool up)
				{
					const auto cell = hatchCells.constFind(PointKey(origin));
					Plane(out, points, x, y, deck * DeckHeight, material, planeDeck, inside, up,
						cell == hatchCells.constEnd() ? noHatches : cell.value());
				};
				if(tag & 0x8000)
				{
					const Material material = window ? Material::HullTopBottomWindow : Material::HullTop;
					plane(material, deck - 1, false, true);
					plane(material, deck - 1, true, false);
					continue;
				}
				if(!(tag & 0x4000))
				{
					plane(window ? Material::HullTopBottomWindow : Material::HullBottom, deck, false, false);
					if(window) plane(Material::HullTopBottomWindow, deck, true, true);
				}
				if(!(tag & 0x2000))
				{
					if(!window || (tag & 0x4000)) plane(Material::InteriorFloor, deck, true, true);
					if(tag & 0x4000) plane(Material::InteriorCeiling, deck - 1, true, false);
				}
				for(int side = 0; side != 3; ++side)
				{
					const QPoint a = points[side], b = points[(side + 1) % 3];
					QPoint neighbor;
					if(side == 0) neighbor = origin + SampleOffsets[(t + 3) % 4];
					else if(side == 2) neighbor = origin + SampleOffsets[(t + 1) % 4];
					else neighbor = sample + QPoint((t == 0 ? 2 : t == 2 ? -2 : 0), (t == 1 ? 2 : t == 3 ? -2 : 0));
					const quint16 other = raster.At(neighbor.x(), neighbor.y());
					const int adjacentOwner = raster.Owner(neighbor.x(), neighbor.y());
					if(!Current(other)) hull.append({a, b, tag});
					else if(owner != adjacentOwner && HasInterior(Type(tag)))
						partitions.append({a, b, tag, other, owner, adjacentOwner});
				}
			}
	HullWalls(out, hull, deck, armor, wallDetails);
	for(const Edge& edge : MergeEdges(partitions))
		for(const WallSpan& span : WallSpans(edge, false, wallDetails))
		{
			if(span.detail == 55) continue;
			const double length = Length(span.b - span.a);
			if(span.detail == 56)
			{
				// RenderInterior 180430ca3: top rail is .3 ft high at z=3;
				// end posts use .1 ft of the normalized boundary direction.
				const QPointF post = (span.b - span.a) * (.1 / length);
				const double floor = deck * DeckHeight;
				out.Quad(Material::Railing, deck, true,
					Position(span.b, floor + 2.7), Position(span.a, floor + 2.7),
					Position(span.a, floor + 3), Position(span.b, floor + 3));
				out.Quad(Material::RailingPost, deck, true,
					Position(span.a + post, floor), Position(span.a, floor),
					Position(span.a, floor + 2.7), Position(span.a + post, floor + 2.7));
				out.Quad(Material::RailingPost, deck, true,
					Position(span.b, floor), Position(span.b - post, floor),
					Position(span.b - post, floor + 2.7), Position(span.b, floor + 2.7));
				continue;
			}
			Material material = InteriorWall(Type(edge.tag), length);
			switch(span.detail)
			{
			case 5: material = Material::InteriorDoor; break;
			case 49:
			case 50: material = Material::CrewDoor; break;
			case 53: material = Material::LargeInteriorDoor; break;
			case 54: material = Material::LargeCrewDoor; break;
			}
			out.Quad(material, deck, true,
				Position(span.b, deck * DeckHeight), Position(span.a, deck * DeckHeight),
				Position(span.a, (deck + 1) * DeckHeight), Position(span.b, (deck + 1) * DeckHeight),
				span.detail || length <= 5 + Epsilon ? 1 : float(std::ceil(length / 10)), span.doorId);
		}
}
}

Mesh BuildMesh(const Design& design)
{
	Builder builder;
	QHash<int, QVector<int>> byDeck;
	QVector<QPolygonF> boundaries(design.rooms.size());
	int lowest = MaximumDeck + 1, highest = MinimumDeck - 1;
	for(int i = 0; i < design.rooms.size(); ++i)
	{
		const Room& room = design.rooms.at(i);
		if(room.deck < MinimumDeck || room.deck > MaximumDeck) continue;
		if(room.roomType == 0 || room.roomType == 12 || room.roomType > 24)
		{
			builder.Warning(QStringLiteral("Room %1 has an unsupported spacecraft room type (%2).")
				.arg(room.roomId).arg(room.roomType));
			continue;
		}
		if(room.roomType >= 22 && room.roomType <= 24)
		{
			builder.Warning(QStringLiteral("Building roof geometry (room types 22–24) is not yet recovered."));
			continue;
		}
		QString error;
		const QPolygonF boundary = RoomBoundary(room, &error);
		if(!Geometry::IsSupportedBoundary(boundary, &error))
		{
			builder.Warning(QStringLiteral("Room %1: %2").arg(room.roomId).arg(error));
			continue;
		}
		double area = 0, perimeterSteps = 0;
		QPointF previous = boundary.last();
		for(const QPointF& p : boundary)
		{
			area += Cross(previous, p);
			perimeterSteps += std::max(std::abs(p.x() - previous.x()), std::abs(p.y() - previous.y())) / RasterSpacing;
			previous = p;
		}
		if(std::abs(area) < Epsilon || perimeterSteps > 65536)
		{
			builder.Warning(QStringLiteral("Room %1 has a degenerate or excessively complex boundary.").arg(room.roomId));
			continue;
		}
		boundaries[i] = boundary;
		byDeck[room.deck].append(i);
		lowest = std::min(lowest, int(room.deck)); highest = std::max(highest, int(room.deck));
	}
	Raster raster;
	for(int deck = lowest; deck <= highest + 1 && !builder.truncated; ++deck)
	{
		for(int index : byDeck.value(deck))
		{
			const Room& room = design.rooms.at(index);
			raster.Fill(boundaries.at(index), room.roomType, room.roomSubtype, index);
		}
		DeckSurfaces(builder, raster, deck, std::min(int(design.header.armor), 10), design.details);
		raster.NextDeck();
	}
	if(builder.truncated) builder.Warning(QStringLiteral("The render preview exceeded its geometry limit and was truncated."));
	AppendEquipment(design, builder.mesh);
	AppendResourceEquipment(design, builder.mesh);
	AppendStairGeometry(design, builder.mesh);
	for(const Detail& detail : design.details)
	{
		const int type = detail.detailType;
		if(detail.hiddenRaw || !type || detail.deck < MinimumDeck || detail.deck > MaximumDeck) continue;
		// The resource importer reports missing vehicle commodities/assets.
		if((type >= 57 && type <= 64) || EquipmentSupported(type) || ResourceEquipmentSupported(type)) continue;
		builder.Warning(QStringLiteral("Equipment type %1 has no recovered 3D preview model.").arg(type));
	}
	return builder.mesh;
}

namespace
{
void AppendTacticalStairLines(const Detail& detail, Wireframe& wireframe)
{
	// RenderDeckTactical 18041564d..18041a40b draws ramp rails and landing
	// outlines, rather than edges of the solid steps. Straight flights have
	// seven cross-lines; each half of a turned or return flight has three.
	const int orientation = DetailRotationStep(detail.detailType);
	const int family = (detail.detailType - 101) / 8;
	const bool diagonal = (orientation & 1) != 0;
	const double angle = double(float(orientation * .7853981633974483));
	const double cosine = std::cos(angle), sine = std::sin(angle);
	const QPointF origin = DecodePoint(detail.packedXY);
	const double grid = double(float(AuLegacyShip::GridSpacing));
	const double height = AuLegacyShip::DeckHeight;
	const QVector4D color(.5f, .5f, .5f, 1);
	const auto position = [&](const AuPoint& point) {
		return QVector3D(float(origin.x() + point.x * cosine - point.y * sine),
			float(origin.y() + point.x * sine + point.y * cosine),
			float(detail.deck * AuLegacyShip::DeckHeight + point.z));
	};
	const auto segment = [&](const AuPoint& first, const AuPoint& second) {
		if(wireframe.vertices.size() > MaximumVertices - 2) return;
		wireframe.vertices.append(WireframeVertex{position(first), color});
		wireframe.vertices.append(WireframeVertex{position(second), color});
	};
	const auto path = [&](std::initializer_list<AuPoint> points) {
		if(points.size() < 2) return;
		auto previous = points.begin();
		for(auto current = previous + 1; current != points.end(); ++current)
		{
			segment(*previous, *current);
			previous = current;
		}
	};
	const auto crossLines = [&](const AuPoint& firstStart, const AuPoint& firstEnd,
		const AuPoint& secondStart, const AuPoint& secondEnd, int divisions) {
		for(int step = 1; step < divisions; ++step)
		{
			const double fraction = double(step) / divisions;
			segment(firstStart + (firstEnd - firstStart) * fraction,
				secondStart + (secondEnd - secondStart) * fraction);
		}
	};
	const AuPoint base(0, 0, 0);
	if(family == 0)
	{
		const double run = diagonal ? 12.0 : grid * 2;
		const double width = diagonal ? 4.0 : grid;
		const AuPoint baseOuter(0, width, 0), topInner(run, 0, height), topOuter(run, width, height);
		segment(base, topInner);
		segment(baseOuter, topOuter);
		crossLines(baseOuter, topOuter, base, topInner, 8);
		return;
	}
	if(family == 1 || family == 2)
	{
		const double run = diagonal ? 8.0 : grid;
		const double width = diagonal ? 4.0 : grid;
		const double side = family == 1 ? 1.0 : -1.0;
		const AuPoint baseOuter(0, side * width, 0);
		const AuPoint landingInner(run, 0, height / 2), landingTurn(run, side * width, height / 2);
		const AuPoint landingOuter(run + width, side * width, height / 2), landingEnd(run + width, 0, height / 2);
		const AuPoint topInner(run, side * (run + width), height), topOuter(run + width, side * (run + width), height);
		path({baseOuter, landingTurn, landingOuter, landingEnd, landingInner, landingTurn, topInner});
		segment(base, landingInner);
		segment(topOuter, landingOuter);
		crossLines(baseOuter, landingTurn, base, landingInner, 4);
		crossLines(landingTurn, topInner, landingOuter, topOuter, 4);
		return;
	}
	const double side = family == 3 ? 1.0 : -1.0;
	// The diagonal right-return cases 134/136/138/140 use a six-foot run
	// (outer landing x = ten feet); left-return cases use eight/twelve feet.
	const double run = diagonal ? (family == 3 ? 8.0 : 6.0) : grid;
	const double width = diagonal ? 6.0 : grid;
	const double landing = diagonal ? 4.0 : grid;
	const AuPoint baseOuter(0, side * width, 0);
	const AuPoint landingInner(run, 0, height / 2), landingTurn(run, side * width, height / 2);
	const AuPoint landingOpposite(run, side * width * 2, height / 2);
	const AuPoint landingCorner(run + landing, side * width * 2, height / 2), landingEnd(run + landing, 0, height / 2);
	const AuPoint topInner(0, side * width, height), topOuter(0, side * width * 2, height);
	path({baseOuter, landingTurn, landingInner, landingEnd, landingCorner, landingOpposite, landingTurn, topInner});
	segment(base, landingInner);
	segment(topOuter, landingOpposite);
	crossLines(baseOuter, landingTurn, base, landingInner, 4);
	crossLines(landingTurn, topInner, landingOpposite, topOuter, 4);
}
}

QString RoomTypeName(quint8 type)
{
	static const char* roomNames[] = {
		"None", "Barracks", "Bridge", "Brig", "Officer Cabin", "Hold", "Hall", "Crew Cabin",
		"Engine Room", "Fuel Cell", "Galley", "Gravity Well", "", "Vehicle Bay", "Lounge",
		"Open to Deck Below", "Sensor Room", "Shield Room", "Sickbay", "Stateroom",
		"Transporter Room", "Weapon Bay", "Roof, Gable", "Roof, Hip", "Roof, Round"
	};
	return type < sizeof(roomNames) / sizeof(roomNames[0]) ? QString::fromLatin1(roomNames[type]) : QString();
}

Wireframe BuildWireframe(const Design& design, int activeDeck, bool gridVisible, quint8 gridMask, bool tactical)
{
	Wireframe result;
	// DaSpaceShip::RenderDesign, AuAtom13 18041a5b0. The original float
	// table at 180a0e620 stores these 25 sRGB colors divided by 255.
	static const quint8 roomColors[][3] = {
		{255,0,0}, {137,163,73}, {255,221,0}, {112,112,112}, {45,14,147},
		{221,195,88}, {163,226,147}, {183,172,128}, {183,183,183}, {237,250,255},
		{237,236,168}, {201,255,188}, {255,252,91}, {255,154,61}, {229,0,0},
		{128,112,128}, {255,0,135}, {93,227,244}, {105,244,202}, {165,0,80},
		{186,76,211}, {50,88,201}, {231,66,34}, {231,66,34}, {231,66,34}
	};
	const auto line = [](QVector<WireframeVertex>& output, const QVector3D& a,
		const QVector3D& b, const QVector4D& color)
	{
		if(output.size() > MaximumVertices - 2 || (a - b).lengthSquared() < 1e-10f) return;
		output.append(WireframeVertex{a, color});
		output.append(WireframeVertex{b, color});
	};
	for(const Room& room : design.rooms)
	{
		// DaSpaceShip::RenderDeckTactical, AuAtom13 1804139a0, displays
		// only the requested deck; the designer retains its other-deck context.
		if(tactical && room.deck != activeDeck) continue;
		if(room.roomType >= sizeof(roomColors) / sizeof(roomColors[0])) continue;
		const QPolygonF boundary = RoomBoundary(room);
		if(boundary.size() < 3) continue;
		bool finite = true;
		for(const QPointF& point : boundary)
			finite = finite && std::isfinite(point.x()) && std::isfinite(point.y());
		if(!finite) continue;
		const quint8* rgb = roomColors[room.roomType];
		const float brightness = room.deck == activeDeck ? 1.f : 1.f / 3.f;
		const QVector4D color(rgb[0] / 255.f * brightness, rgb[1] / 255.f * brightness,
			rgb[2] / 255.f * brightness, 1);
		const double floor = room.deck * DeckHeight;
		// Per-room plan labels belong to the designer's RenderDesign pass.
		// The live client's deck/room names are location-description metadata.
		if(!tactical && room.deck == activeDeck)
		{
			const auto* buildingRoom = design.building
				? AuLegacyBuildingPalette::FindBuildingTool(AuLegacyBuildingPalette::BuildingRooms, room.roomType) : nullptr;
			result.labels.append(WireframeLabel{Position(boundary.first(),floor),
				QStringLiteral("%1 %2").arg(buildingRoom ? QString::fromLatin1(buildingRoom->name) : RoomTypeName(room.roomType))
					.arg(int(room.roomSubtype)+1),color,false});
		}
		const bool roof = room.roomType >= 22 && room.roomType <= 24;
		const double contourHeight = tactical ? 4.0 : AuLegacyShip::DeckHeight;
		for(int i = 0; i < boundary.size(); ++i)
		{
			const QPointF& a = boundary.at(i);
			const QPointF& b = boundary.at((i + 1) % boundary.size());
			line(result.vertices, Position(a, floor), Position(b, floor), color);
			if(roof) continue;
			// The deck immediately above contributes its floor contour later
			// in RenderDesign; its ceiling contour is omitted from the first pass.
			if(room.deck != activeDeck + 1)
				line(result.vertices, Position(a, floor + contourHeight), Position(b, floor + contourHeight), color);
			line(result.vertices, Position(a, floor), Position(a, floor + contourHeight), color);
		}
		if(room.deck > activeDeck + 1 || boundary.size() > 65536) continue;
		if(tactical && (room.roomType == 11 || room.roomType == 15)) continue;
		AuPointArray polygon;
		for(const QPointF& point : boundary) polygon.append(AuPoint(point.x(), point.y(), floor));
		if(!polygon.IsClockwise()) polygon.ReverseOrder();
		AuPointArray triangles;
		QVector<bool> edges;
		polygon.TriangulateBoundary(triangles, edges);
		const float fillBrightness = !tactical && room.deck == activeDeck ? 1.f : .25f;
		const float alpha = !tactical && room.deck == activeDeck ? .8f : .2f;
		const QVector4D fill(rgb[0] / 255.f * fillBrightness, rgb[1] / 255.f * fillBrightness,
			rgb[2] / 255.f * fillBrightness, alpha);
		if(triangles.size() > MaximumVertices - result.triangles.size()) continue;
		for(const AuPoint& point : triangles)
			result.triangles.append(WireframeVertex{QVector3D(float(point.x), float(point.y), float(point.z)), fill});
	}
	// RenderDesign queues stairs on the active deck for a separate gray 3D
	// line pass. Tactical mode also includes the stairs leading up from below.
	// Reuse the recovered staircase dimensions and retain feature
	// edges while suppressing each planar face's triangulation diagonals.
	Design stairDesign;
	for(const Detail& detail : design.details)
		if((detail.deck == activeDeck || (tactical && detail.deck == activeDeck - 1))
			&& detail.detailType >= 101 && detail.detailType <= 140)
		{
			if(tactical)
			{
				AppendTacticalStairLines(detail, result);
				continue;
			}
			Detail visible = detail;
			visible.hiddenRaw = 0;
			stairDesign.details.append(visible);
		}
	Mesh stairMesh;
	AppendStairGeometry(stairDesign, stairMesh);
	struct StairEdge { QVector3D a, b, normal; int count = 0; bool corner = false; };
	QHash<QString, StairEdge> stairEdges;
	const auto pointKey = [](const QVector3D& point)
	{
		return QStringLiteral("%1,%2,%3").arg(qRound64(point.x()*100000.0))
			.arg(qRound64(point.y()*100000.0)).arg(qRound64(point.z()*100000.0));
	};
	for(const Batch& batch : stairMesh.batches)
		for(int i = 0; i + 2 < batch.vertices.size(); i += 3)
		{
			const QVector3D normal = QVector3D::crossProduct(batch.vertices.at(i+1).position-batch.vertices.at(i).position,
				batch.vertices.at(i+2).position-batch.vertices.at(i).position).normalized();
			for(int j = 0; j < 3; ++j)
			{
				const QVector3D a = batch.vertices.at(i+j).position, b = batch.vertices.at(i+(j+1)%3).position;
				const QString ka = pointKey(a), kb = pointKey(b);
				const QString key = ka < kb ? ka + QLatin1Char('/') + kb : kb + QLatin1Char('/') + ka;
				StairEdge& edge = stairEdges[key];
				if(!edge.count) { edge.a = a; edge.b = b; edge.normal = normal; }
				else if(std::abs(QVector3D::dotProduct(edge.normal,normal)) < .999f) edge.corner = true;
				++edge.count;
			}
		}
	for(const StairEdge& edge : stairEdges)
		if(edge.count == 1 || edge.corner)
			line(result.vertices,edge.a,edge.b,QVector4D(.5f,.5f,.5f,1));
	// Original DnDetail::DesignTextureCoords (AuData13 18002fb20) indexes
	// 16-pixel tiles in txSpacecraftDesign.png; only the active deck has icons.
	for(const Detail& detail : design.details)
	{
		const int type = detail.detailType;
		if(detail.deck != activeDeck || type <= 0 || type > 142 || (type >= 101 && type <= 140)) continue;
		if(tactical && ((type >= 57 && type <= 64) || type == 76 || type == 98 || type == 99 || type == 141)) continue;
		static const float iconSizes[] = {
			10.f,4.f,3.5f,2.f,4.f,2.f,4.f,4.f,4.f,2.f,2.8284271245f,3.5f,4.f,4.f,2.8284271245f,4.f,
			2.8284271245f,4.f,2.8284271245f,4.f,2.8284271245f,2.f,4.f,2.8284271245f,2.8284271245f,
			3.5f,3.5f,3.5f,3.5f,3.5f,3.5f,3.5f,2.8284271245f,4.f,2.f,2.f,2.f,2.f,2.f,2.f,2.f,
			10.f,10.f,10.f,10.f,10.f,10.f,10.f,10.f,2.f,2.f,4.f,4.f,2.f,2.f,2.f,2.f,
			35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,3.5f,3.5f,3.5f,3.5f,3.5f,3.5f,3.5f,3.5f,
			4.f,10.f,10.f,2.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,4.f,
			6.f,5.f,3.f,
			35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,
			35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,
			35.f,35.f,35.f,35.f,35.f,35.f,35.f,35.f,4.f,25.f
		};
		int tile = type - 1;
		if(type >= 25 && type <= 31) tile = 1;
		else if(type >= 65 && type <= 72) tile = 24;
		else if(type == 73) tile = 25;
		else if(type >= 77 && type <= 83) tile = 7;
		else if(type >= 84 && type <= 90) tile = 50;
		else if(type >= 91 && type <= 97) tile = 51;
		else if(type == 74 || type == 75) tile = 27;
		else if(type == 76) tile = 26;
		else if(type >= 98 && type <= 100) tile = type - 70;
		else if(type >= 101 && type <= 140) tile = 56 + (type - 101) / 8;
		else if(type == 141) tile = 61;
		else if(type == 142) tile = 62;
		if(type >= 57 && type <= 64)
		{
			tile = 40;
			switch(detail.commodityIdNormalized)
			{
			case 9: tile = 45; break;
			case 82: tile = 43; break;
			case 104: tile = 44; break;
			case 133: tile = 42; break;
			case 231: tile = 41; break;
			}
		}
		const QRectF berthBounds = BerthFootprint(type);
		const bool berth = !berthBounds.isEmpty();
		const bool table = type == 74 || type == 75;
		const bool vehicle = type >= 57 && type <= 64;
		const double size = tactical && table ? 10.0 : iconSizes[type];
		const QPointF center = DecodePoint(detail.packedXY);
		const bool rotated = berth || table || vehicle || IsHatch(type);
		const double angle = rotated ? DetailRotationStep(type) * .7853981633974483 : 0;
		const double c = std::cos(angle), s = std::sin(angle);
		QRectF footprint = vehicle ? VehicleFootprint(detail.commodityIdNormalized) : berthBounds;
		if(footprint.isEmpty()) footprint = QRectF(-size/2, -size/2, size, size);
		const QPointF local[4] = {footprint.topLeft(), footprint.bottomLeft(),
			footprint.bottomRight(), footprint.topRight()};
		const QVector2D uv[4] = {{tile/64.f,0}, {tile/64.f,1}, {(tile*16+15)/1024.f,1}, {(tile*16+15)/1024.f,0}};
		const double iconHeight = detail.deck * AuLegacyShip::DeckHeight + (berth || table || vehicle ? .2 : .1);
		QVector3D positions[4];
		for(int i = 0; i < 4; ++i)
			positions[i] = Position(center + QPointF(local[i].x()*c-local[i].y()*s,
				local[i].x()*s+local[i].y()*c), iconHeight);
		const int indices[6] = {0,1,2,0,2,3};
		if(result.sprites.size() > MaximumVertices - (tactical ? 12 : 6)) break;
		for(int index : indices) result.sprites.append(WireframeSpriteVertex{positions[index], uv[index]});
		if(tactical)
		{
			// RenderDeckTactical makes all icons visible from both sides. Doors,
			// hatches and turrets place the underside one tenth of a foot below
			// the detail plane; other icons reverse their top-face winding in place.
			const bool turret = type == 21 || type == 22 || (type >= 33 && type <= 40);
			if(IsHatch(type) || type == 5 || type == 49 || type == 50 || turret)
				for(QVector3D& point : positions) point.setZ(float(detail.deck * AuLegacyShip::DeckHeight - .1));
			const int reverseIndices[6] = {3,2,1,3,0,1};
			for(int index : reverseIndices) result.sprites.append(WireframeSpriteVertex{positions[index], uv[index]});
		}
	}
	if(gridVisible && design.building)
	{
		// DaDesignPlane::RenderBuildingGrid (AuAtom13 1802ff240). The
		// buildable surface changes with industry and, in mines, with depth.
		const double site = AuLegacyBd::DesignerSiteRadius(design.buildingIndustry, activeDeck);
		const double fullRadius = 40 * AuLegacyShip::GridSpacing;
		const double radius = site > 0 ? site : fullRadius;
		const int cells = int(radius / AuLegacyShip::CoordinateSpacing) / 2;
		const bool halfGrid = design.buildingIndustry == 34 || design.buildingIndustry == 85;
		const double top = halfGrid ? 0 : radius;
		const double z = activeDeck * AuLegacyBd::DeckHeight;
		const int highest = DesignMaximumDeck(design);
		const QVector3D color = activeDeck > highest + 1 || activeDeck < DesignMinimumDeck(design) ? QVector3D(.6f,.1f,.1f)
			: activeDeck == highest + 1 ? QVector3D(.2f,.2f,.2f) : activeDeck < 0 ? QVector3D(.4f,.2f,.1f)
			: activeDeck == 0 ? QVector3D(.1f,.4f,.1f) : QVector3D(.2f,.2f,.4f);
		for(int i = -2 * cells; i <= 2 * cells; ++i)
		{
			const double p = i * AuLegacyShip::GridSpacing;
			const QVector4D tint(i % 10 ? color : color + QVector3D(.05f,.05f,.05f), 1);
			const auto segment = [&](double x1, double y1, double x2, double y2) {
				line(result.gridVertices, QVector3D(float(x1),float(y1),float(z)), QVector3D(float(x2),float(y2),float(z)), tint);
			};
			if((gridMask & 1) && p >= -radius && p <= top) segment(-radius,p,radius,p);
			if((gridMask & 4) && std::abs(p) <= radius) segment(p,-radius,p,top);
			// A diagonal crosses y=0 at x=p. Clip the endpoints to the
			// (possibly half-depth) site rectangle.
			for(int slope = -1; slope <= 1; slope += 2)
			{
				if(!(gridMask & (slope > 0 ? 2 : 8))) continue;
				const double y1 = std::max(-radius, -radius - slope * p);
				const double y2 = std::min(top, radius - slope * p);
				if(y1 < y2) segment(p + slope * y1, y1, p + slope * y2, y2);
			}
		}
		const QString name = AuLegacyBd::DesignerIndustry(design.buildingIndustry).name;
		const QString level = activeDeck == 0 ? QStringLiteral("Ground Level") : activeDeck == -1 ? QStringLiteral("1 Story Below Grade")
			: activeDeck < 0 ? QStringLiteral("%1 Stories Below Grade").arg(-activeDeck)
			: QStringLiteral("%1 Stories Above Grade").arg(activeDeck + 1);
		const bool water = halfGrid || design.buildingIndustry == 62;
		const char* labels[3][3] = {{"Left Back", water ? "Back\nStreet" : "Back", "Right Back"},
			{"Left", "", "Right"}, {"Left Front", water ? "Front\nWater" : "Front", "Right Front"}};
		for(int y = -1; !name.isEmpty() && y <= 1; ++y)
			for(int x = -1; x <= 1; ++x)
				if(x || y)
					result.labels.append(WireframeLabel(QVector3D(float(x*fullRadius),float(y*fullRadius),float(z+10)),
						QStringLiteral("%1\n%2\n%3").arg(name, QLatin1String(labels[y+1][x+1]), level), QVector4D(.3f,.4f,.5f,1), true));
	}
	else if(gridVisible)
	{
		// DaDesignPlane::RenderSpacecraftGrid, 180301980. Its frame is the
		// original 80-cell spacecraft volume; grid lines lie on the design plane.
		const double radius = 40 * GridSpacing;
		const double height = activeDeck * DeckHeight;
		const QVector4D minor(.1f, .2f, .3f, 1), major(.2f, .3f, .4f, 1);
		struct GridLabel { const char* text; int x, y; };
		static const GridLabel gridLabels[] = {
			{"Fore",0,1}, {"Aft",0,-1}, {"Port",-1,0}, {"Starboard",1,0},
			{"Port Quarter",-1,-1}, {"Starboard Quarter",1,-1},
			{"Port Fore",-1,1}, {"Starboard Fore",1,1}
		};
		for(const GridLabel& label : gridLabels)
			result.labels.append(WireframeLabel{QVector3D(float(label.x*radius),float(label.y*radius),float(height+6)),
				QString::fromLatin1(label.text),QVector4D(.3f,.4f,.5f,1),true});
		const QPointF corners[4] = {{radius,radius}, {radius,-radius}, {-radius,-radius}, {-radius,radius}};
		for(int i = 0; i < 4; ++i)
		{
			const QPointF& a = corners[i];
			const QPointF& b = corners[(i + 1) % 4];
			line(result.gridVertices, Position(a, -224), Position(b, -224), minor);
			line(result.gridVertices, Position(a, 224), Position(b, 224), minor);
			line(result.gridVertices, Position(a, -224), Position(a, 224), minor);
		}
		for(int i = -40; i <= 40; ++i)
		{
			const double p = i * GridSpacing;
			const QVector4D& color = i % 10 ? minor : major;
			const auto gridLine = [&](double ax, double ay, double bx, double by)
			{
				line(result.gridVertices, QVector3D(float(ax),float(ay),float(height)),
					QVector3D(float(bx),float(by),float(height)), color);
			};
			if(gridMask & 1) gridLine(radius,p,-radius,p);
			if(gridMask & 4) gridLine(p,radius,p,-radius);
			if(i == 40) continue;
			if(gridMask & 2)
			{
				gridLine(p,-radius,radius,-p);
				if(i != -40) gridLine(-radius,p,-p,radius);
			}
			if(gridMask & 8)
			{
				gridLine(p,radius,radius,p);
				if(i != -40) gridLine(-radius,-p,-p,-radius);
			}
		}
	}
	return result;
}
}
}
