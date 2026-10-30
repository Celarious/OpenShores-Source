#include "aulegacybuildinggeometry.h"
#include "aulegacybiodome.h"
#include "aulegacyshipgeometry.h"
#include "aulegacyshipstairs.h"
#include "aulegacyshipequipment.h"
#include "aulegacyshipvehicles.h"
#include "aupoint.h"
#include "aulegacycitystate.h"
#include "aulegacycitycatalogue.h"
#include "aunoise.h"

#include <QDataStream>
#include <QHash>
#include <QPoint>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace AuLegacyBd
{
namespace Geometry
{
namespace
{
constexpr int RasterSize = 516;
constexpr int RasterOrigin = 256;
constexpr double RasterSpacing = 1.41421356225;
constexpr double Epsilon = 1e-5;
constexpr int MaximumVertices = 6000000;
constexpr double RoofInset = 1.0 / 96.0;
int Type(quint16 tag) { return (tag >> 8) & 31; }
bool Current(quint16 tag) { return tag && !(tag & 0x8000); }
bool HasInterior(int type) { return type != 9 && !(type >= 22 && type <= 24); }

struct ProfileNode { double offset, height; int type; };
struct ProfilePoint { double offset, height; };
struct ProfileCap { ProfilePoint points[3]; };
struct Profile { const ProfileNode* points; int count; const ProfileCap* caps; int capCount; };
#include "aulegacybuildingprofiles.inc"
#include "aulegacybuildingraster.inc"

int MaterialIndex(quint16 commodity)
{
	switch(commodity)
	{
	case 86: return 0; // Wood.
	case 48: return 1; // Lumber.
	case 76: return 2; // Stone.
	case 51: return 3; // Metal.
	case 146: return 4; // Magmium.
	case 145: return 5; // Vulcium.
	default: return -1;
	}
}

struct Builder
{
	Mesh mesh;
	QHash<QString, int> batchIndices;
	int vertices = 0;
	bool hasBounds = false;
	bool noCollision = false;
	void Warning(const QString& text)
	{
		if(!mesh.warnings.contains(text)) mesh.warnings.append(text);
	}
	void Triangle(int slot, int deck, bool interior, const QVector3D& a, const QVector3D& b,
		const QVector3D& c, const QVector2D& ua = {0, 0}, const QVector2D& ub = {1, 0},
		const QVector2D& uc = {1, 1}, quint32 doorId = 0, quint32 detailId = 0,
		bool twoSided = false, bool stairs = false, bool collisionOnly = false, bool depthOnly = false,
		bool equipment = false)
	{
		if(vertices + 3 > MaximumVertices)
		{
			Warning(QStringLiteral("Building geometry exceeds its bounded vertex budget.")); return;
		}
		QVector3D normal = QVector3D::crossProduct(b - a, c - a);
		if(normal.lengthSquared() < 1e-12f) return;
		normal.normalize();
		const QString key = QStringLiteral("%1/%2/%3/%4/%5/%6/%7/%8/%9/%10/%11")
			.arg(slot).arg(deck).arg(interior).arg(doorId).arg(detailId).arg(twoSided).arg(stairs).arg(collisionOnly).arg(depthOnly).arg(equipment).arg(noCollision);
		int index = batchIndices.value(key, -1);
		if(index < 0)
		{
			index = mesh.batches.size(); batchIndices.insert(key, index);
			Batch batch; batch.textureSlot = slot; batch.deck = deck; batch.interior = interior;
			batch.doorId = doorId; batch.detailId = detailId; batch.twoSided = twoSided;
			batch.equipment = equipment;
			batch.stairs = stairs;
			batch.collisionOnly = collisionOnly;
			batch.noCollision = noCollision;
			batch.depthOnly = depthOnly;
			mesh.batches.append(batch);
		}
		QVector<AuLegacyShip::Rendering::Vertex>& result = mesh.batches[index].vertices;
		result.append({a, normal, ua}); result.append({b, normal, ub}); result.append({c, normal, uc});
		if(!hasBounds) { mesh.minimum = mesh.maximum = a; hasBounds = true; }
		for(const QVector3D& p : {a, b, c})
			for(int axis = 0; axis < 3; ++axis)
			{
				mesh.minimum[axis] = std::min(mesh.minimum[axis], p[axis]);
				mesh.maximum[axis] = std::max(mesh.maximum[axis], p[axis]);
			}
		vertices += 3;
	}
	void Quad(int slot, int deck, bool interior, const QVector3D& a, const QVector3D& b,
		const QVector3D& c, const QVector3D& d, const QVector2D& ua = {0, 0},
		const QVector2D& ub = {1, 0}, const QVector2D& uc = {1, 1}, const QVector2D& ud = {0, 1},
		quint32 doorId = 0, quint32 detailId = 0, bool twoSided = false)
	{
		Triangle(slot, deck, interior, a, b, c, ua, ub, uc, doorId, detailId, twoSided);
		Triangle(slot, deck, interior, a, c, d, ua, uc, ud, doorId, detailId, twoSided);
	}
	void TiledTriangle(int slot, int deck, bool interior, const QVector3D& a, const QVector3D& b,
		const QVector3D& c, const QVector2D& ua, const QVector2D& ub, const QVector2D& uc)
	{
		// Keep each repeated tile in [0,1] so the renderer can still inset its
		// cropped atlas texture. Clip the original triangle, preserving its plane,
		// winding and collision coverage instead of changing the wall profile.
		const QVector2D minimum(std::min(ua.x(), std::min(ub.x(), uc.x())),
			std::min(ua.y(), std::min(ub.y(), uc.y())));
		const QVector2D maximum(std::max(ua.x(), std::max(ub.x(), uc.x())),
			std::max(ua.y(), std::max(ub.y(), uc.y())));
		if(minimum.x() >= 0 && minimum.y() >= 0 && maximum.x() <= 1 && maximum.y() <= 1)
		{
			Triangle(slot, deck, interior, a, b, c, ua, ub, uc); return;
		}
		struct TileVertex { QVector3D position; QVector2D uv; };
		const QVector<TileVertex> source = {{a, ua}, {b, ub}, {c, uc}};
		const int firstX = int(std::floor(minimum.x())), firstY = int(std::floor(minimum.y()));
		const int endX = std::max(firstX + 1, int(std::ceil(maximum.x())));
		const int endY = std::max(firstY + 1, int(std::ceil(maximum.y())));
		for(int x = firstX; x < endX; ++x)
			for(int y = firstY; y < endY; ++y)
			{
				if(vertices + 3 > MaximumVertices)
				{
					Warning(QStringLiteral("Building geometry exceeds its bounded vertex budget.")); return;
				}
				QVector<TileVertex> polygon = source;
				for(int axis = 0; axis < 2 && !polygon.isEmpty(); ++axis)
					for(int side = 0; side < 2 && !polygon.isEmpty(); ++side)
					{
						const float boundary = float((axis ? y : x) + side);
						QVector<TileVertex> clipped;
						TileVertex previous = polygon.last();
						float before = previous.uv[axis] - boundary;
						for(const TileVertex& vertex : polygon)
						{
							const float after = vertex.uv[axis] - boundary;
							const bool previousInside = side ? before <= 0 : before >= 0;
							const bool inside = side ? after <= 0 : after >= 0;
							if(previousInside != inside)
							{
								const float t = before / (before - after);
								clipped.append({previous.position + (vertex.position - previous.position) * t,
									previous.uv + (vertex.uv - previous.uv) * t});
							}
							if(inside) clipped.append(vertex);
							previous = vertex; before = after;
						}
						polygon.swap(clipped);
					}
				const QVector2D origin{float(x), float(y)};
				for(int i = 1; i + 1 < polygon.size(); ++i)
					Triangle(slot, deck, interior, polygon[0].position, polygon[i].position, polygon[i + 1].position,
						polygon[0].uv - origin, polygon[i].uv - origin, polygon[i + 1].uv - origin);
			}
	}
	void TiledQuad(int slot, int deck, bool interior, const QVector3D& a, const QVector3D& b,
		const QVector3D& c, const QVector3D& d, const QVector2D& ua, const QVector2D& ub,
		const QVector2D& uc, const QVector2D& ud)
	{
		TiledTriangle(slot, deck, interior, a, b, c, ua, ub, uc);
		TiledTriangle(slot, deck, interior, a, c, d, ua, uc, ud);
	}
};

#include "aulegacybuildingturrets.inc"

struct Opening
{
	QPolygonF boundary;
	QPointF center;
	quint32 id = 0;
	int type = 0;
	int slot = 10;
	bool diamond = false;
};

QVector<Opening> FloorOpenings(const Design& design, int deck)
{
	QVector<Opening> result;
	QSet<quint16> occupied;
	for(const AuLegacyShip::Detail& detail : design.details)
	{
		const int type = detail.detailType;
		if(detail.deck != deck || (!AuLegacyShip::Rendering::IsHatchDetail(type)
			&& type != 22 && type != 33) || occupied.contains(detail.packedXY)) continue;
		occupied.insert(detail.packedXY);
		Opening opening; opening.center = AuLegacyShip::DecodePoint(detail.packedXY);
		opening.id = detail.detailId; opening.type = type;
		opening.slot = type == 8 || (type >= 77 && type <= 83) ? 19
			: type == 51 || (type >= 84 && type <= 90) ? 6 : 10;
		const int x = int(std::round(opening.center.x() / RasterSpacing)) + RasterOrigin;
		const int y = int(std::round(opening.center.y() / RasterSpacing)) + RasterOrigin;
		const bool diamond = x % 4 != 2 || y % 4 != 2;
		opening.diamond = diamond;
		if(type == 22 || type == 33)
		{
			opening.id = 0; opening.slot = 36;
			opening.boundary << opening.center + QPointF(AuLegacyShip::CoordinateSpacing, 0)
				<< opening.center + QPointF(2, 2) << opening.center + QPointF(0, AuLegacyShip::CoordinateSpacing)
				<< opening.center + QPointF(-2, 2) << opening.center + QPointF(-AuLegacyShip::CoordinateSpacing, 0)
				<< opening.center + QPointF(-2, -2) << opening.center + QPointF(0, -AuLegacyShip::CoordinateSpacing)
				<< opening.center + QPointF(2, -2);
		}
		else if(diamond)
			opening.boundary << opening.center + QPointF(-AuLegacyShip::CoordinateSpacing, 0)
				<< opening.center + QPointF(0, -AuLegacyShip::CoordinateSpacing)
				<< opening.center + QPointF(AuLegacyShip::CoordinateSpacing, 0)
				<< opening.center + QPointF(0, AuLegacyShip::CoordinateSpacing);
		else opening.boundary << opening.center + QPointF(-2, -2) << opening.center + QPointF(2, -2)
			<< opening.center + QPointF(2, 2) << opening.center + QPointF(-2, 2);
		result.append(opening);
	}
	return result;
}

void SplitPolygon(const QPolygonF& polygon, const QPointF& a, const QPointF& b,
	QPolygonF& inside, QPolygonF& outside)
{
	if(polygon.isEmpty()) return;
	QPointF previous = polygon.last();
	double before = Cross(b - a, previous - a);
	for(const QPointF& p : polygon)
	{
		const double after = Cross(b - a, p - a);
		if((before < -Epsilon && after > Epsilon) || (before > Epsilon && after < -Epsilon))
		{
			const QPointF intersection = previous + (p - previous) * (before / (before - after));
			inside.append(intersection); outside.append(intersection);
		}
		if(after >= -Epsilon) inside.append(p);
		if(after <= Epsilon) outside.append(p);
		previous = p; before = after;
	}
}

// Clip against a counterclockwise convex boundary, retaining the outside
// pieces for floor openings and overlapping road pavement alike.
QPolygonF ClipConvexPolygon(const QPolygonF& polygon, const QPolygonF& boundary,
	QVector<QPolygonF>& outsidePieces)
{
	QPolygonF intersection = polygon;
	for(int i = 0; i < boundary.size() && !intersection.isEmpty(); ++i)
	{
		QPolygonF inside, outside;
		SplitPolygon(intersection, boundary.at(i), boundary.at((i + 1) % boundary.size()), inside, outside);
		if(outside.size() >= 3) outsidePieces.append(outside);
		intersection = inside;
	}
	return intersection;
}

void Plane(Builder& out, const QPoint points[3], int x, int y, double z, int slot,
	int deck, bool interior, bool up, const QVector<Opening>& openings)
{
	const auto appendPolygon = [&](const QPolygonF& polygon, int texture, const Opening* opening)
	{
		const auto uv = [&](const QPointF& p)
		{
			if(opening)
			{
				if(opening->diamond)
				{
					const QPointF delta = p - opening->center;
					return QVector2D(float((delta.x() + delta.y()) / (2 * AuLegacyShip::CoordinateSpacing) + .5),
						float((delta.y() - delta.x()) / (2 * AuLegacyShip::CoordinateSpacing) + .5));
				}
				const QRectF bounds = opening->boundary.boundingRect();
				return QVector2D(float((p.x() - bounds.left()) / bounds.width()),
					float((p.y() - bounds.top()) / bounds.height()));
			}
			return QVector2D(float((p.x() / RasterSpacing + RasterOrigin - x) / 4),
				float((p.y() / RasterSpacing + RasterOrigin - y) / 4));
		};
		for(int i = 1; i + 1 < polygon.size(); ++i)
		{
			const QPointF a = polygon.first(), b = polygon.at(up ? i : i + 1), c = polygon.at(up ? i + 1 : i);
			out.Triangle(texture, deck, interior, Position(a, z), Position(b, z), Position(c, z),
				uv(a), uv(b), uv(c), opening ? opening->id : 0, opening ? opening->id : 0);
		}
	};
	QPolygonF triangle;
	for(int i = 0; i < 3; ++i) triangle.append(PlanPoint(points[i]));
	QVector<QPolygonF> remainder = {triangle};
	for(const Opening& opening : openings)
	{
		if(!triangle.boundingRect().intersects(opening.boundary.boundingRect())) continue;
		QVector<QPolygonF> next;
		for(const QPolygonF& polygon : remainder)
		{
			const QPolygonF intersection = ClipConvexPolygon(polygon, opening.boundary, next);
			// The hatch panel is the only collision surface that follows its ID.
			if(opening.id) appendPolygon(intersection, opening.slot, &opening);
		}
		remainder = next;
	}
	for(const QPolygonF& polygon : remainder) appendPolygon(polygon, slot, nullptr);
}

struct WallSpan
{
	QPointF a, b;
	int type = 0;
	int detailType = 0;
	quint32 detailId = 0;
};

QVector<WallSpan> WallSpans(const Edge& edge, const Design& design, int deck, bool exterior)
{
	const QPointF a = PlanPoint(edge.a), b = PlanPoint(edge.b), delta = b - a;
	const double length = Length(delta);
	const QPointF direction = Unit(delta);
	struct Cut { double first, last; int type, detail; quint32 id; };
	QVector<Cut> details;
	QVector<double> cuts = {0, length};
	for(const AuLegacyShip::Detail& detail : design.details)
	{
		if(detail.deck != deck) continue;
		const int type = detail.detailType;
		const bool sideTurret = type == 21 || (type >= 34 && type <= 40);
		if(exterior ? (type != 3 && type != 9 && type != 50 && !sideTurret)
			: (type != 5 && type != 49 && type != 50 && type != 53 && type != 54 && type != 55 && type != 56)) continue;
		const QPointF p = AuLegacyShip::DecodePoint(detail.packedXY);
		const double distance = QPointF::dotProduct(p - a, direction);
		if(distance < Epsilon || distance > length - Epsilon || std::abs(Cross(p - a, direction)) > Epsilon) continue;
		double first = distance - 2.5, last = distance + 2.5;
		if(sideTurret) { first = distance - AuLegacyShip::CoordinateSpacing; last = distance + AuLegacyShip::CoordinateSpacing; }
		else if(exterior && (type == 3 || type == 9)) { first = 1; last = length - 1; }
		else if(!exterior && type >= 53) { first = 0; last = length; }
		first = std::max(0.0, first); last = std::min(length, last);
		if(last - first < Epsilon) continue;
		const int profileType = sideTurret ? 31 : type == 9 || type == 55 ? 29 : type == 50 || type == 54 ? 30 : 28;
		details.append({first, last, profileType, type, detail.detailId});
		cuts.append(first); cuts.append(last);
	}
	std::sort(cuts.begin(), cuts.end());
	cuts.erase(std::unique(cuts.begin(), cuts.end(), [](double x, double y) { return std::abs(x - y) < Epsilon; }), cuts.end());
	QVector<WallSpan> result;
	for(int i = 1; i < cuts.size(); ++i)
	{
		WallSpan span; span.a = a + direction * cuts.at(i - 1); span.b = a + direction * cuts.at(i); span.type = Type(edge.tag);
		const double middle = (cuts.at(i - 1) + cuts.at(i)) / 2;
		for(const Cut& detail : details)
			if(middle > detail.first - Epsilon && middle < detail.last + Epsilon)
			{
				span.type = detail.type; span.detailType = detail.detail; span.detailId = detail.id; break;
			}
		result.append(span);
	}
	return result;
}

int InteriorWallSlot(int roomType, double length)
{
	const bool fill = length <= 5 + Epsilon;
	if(roomType == 3 || roomType == 5 || roomType == 13) return fill ? 24 : 23;
	if(roomType == 8 || roomType == 16 || roomType == 17 || roomType == 21) return fill ? 26 : 25;
	return fill ? 22 : 21;
}

float WallRepeat(int slot, double length)
{
	// One full-width wall tile spans an eight-foot deck. Narrow fill and rail
	// tiles retain their width relative to the atlas, across all six materials.
	const double width = AuLegacyBd::DeckHeight * TextureTile(slot).width() / TextureTile(13).width();
	return float(length / width);
}

void ExteriorWalls(Builder& out, const QVector<Edge>& edges, const Design& design,
	int deck, int material, bool interior)
{
	for(const QVector<Edge>& loop : Loops(MergeEdges(edges, true)))
	{
		QVector<WallSpan> spans;
		for(const Edge& edge : loop) spans += WallSpans(edge, design, deck, true);
		for(int i = 0; i < spans.size(); ++i)
		{
			const WallSpan& span = spans.at(i);
			const WallSpan& previous = spans.at((i + spans.size() - 1) % spans.size());
			const WallSpan& next = spans.at((i + 1) % spans.size());
			const QPointF left = JoinNormal(span.a - previous.a, span.b - span.a);
			const QPointF right = JoinNormal(span.b - span.a, next.b - span.b);
			const int type = deck < 0 ? 3 : span.type;
			const Profile& profile = profiles[interior ? interiorProfiles[type][material] : exteriorProfiles[type][material]];
			const double base = deck * AuLegacyBd::DeckHeight;
			const double length = Length(span.b - span.a);
			if(!interior && span.detailType && span.type >= 28 && span.type <= 30)
				out.mesh.wallOpenings.append({span.a, span.b, deck, span.detailType, span.detailId});
			for(int j = 1; j < profile.count; ++j)
			{
				const ProfileNode& p = profile.points[j - 1];
				const ProfileNode& q = profile.points[j];
				if(p.type == 0) continue;
				// Original wall-type to BuildingTexture table at 0x5bbf80.
				static const int textureSlots[] = {0, 4, 9, 36, 13, 15, 14};
				int slot = textureSlots[p.type];
				if(interior && (p.type == 4 || p.type == 6)) slot = InteriorWallSlot(span.type, length);
				if(interior && p.type == 1) slot = 4;
				if(interior && p.type == 2) slot = 9;
				const QVector3D a = Position(span.a + left * p.offset, base + p.height);
				const QVector3D b = Position(span.b + right * p.offset, base + p.height);
				const QVector3D c = Position(span.b + right * q.offset, base + q.height);
				const QVector3D d = Position(span.a + left * q.offset, base + q.height);
				if(p.type == 3) { SideTurret(out, deck, a, b, c, d, interior); continue; }
				const bool panel = p.type == 1 || p.type == 2;
				// Deliberate visual fix: repeat wall, window and trim artwork at
				// a fixed scale, while keeping authored door panels intact.
				const float repeat = panel ? 1 : WallRepeat(slot, length);
				const float bottom = p.type == 4 ? float(p.height / AuLegacyBd::DeckHeight) : 0;
				const float top = p.type == 4 ? float(q.height / AuLegacyBd::DeckHeight) : 1;
				if(panel)
				{
					if(interior) out.Quad(slot, deck, true, b, a, d, c, {0, bottom}, {1, bottom}, {1, top}, {0, top}, span.detailId);
					else out.Quad(slot, deck, false, a, b, c, d, {0, bottom}, {1, bottom}, {1, top}, {0, top}, span.detailId);
				}
				else if(interior) out.TiledQuad(slot, deck, true, b, a, d, c, {0, bottom}, {repeat, bottom}, {repeat, top}, {0, top});
				else out.TiledQuad(slot, deck, false, a, b, c, d, {0, bottom}, {repeat, bottom}, {repeat, top}, {0, top});
			}
			if(span.type != next.type)
			{
				const Profile& exterior = profiles[exteriorProfiles[deck < 0 ? 3 : span.type][material]];
				const Profile& adjacent = profiles[exteriorProfiles[deck < 0 ? 3 : next.type][material]];
				const auto cap = [&](const Profile& shape, bool reverse)
				{
					for(int j = 0; j < shape.capCount; ++j)
					{
						QVector3D p[3]; QVector2D uv[3];
						for(int v = 0; v < 3; ++v)
						{
							const ProfilePoint& control = shape.caps[j].points[v];
							p[v] = Position(span.b + right * control.offset, base + control.height);
							uv[v] = QVector2D(control.offset ? 1.0f : 0.0f, float(control.height / AuLegacyBd::DeckHeight));
						}
						const bool flipped = reverse != interior;
						out.Triangle(13, deck, interior, p[0], p[flipped ? 2 : 1], p[flipped ? 1 : 2],
							uv[0], uv[flipped ? 2 : 1], uv[flipped ? 1 : 2]);
					}
				};
				cap(exterior, false); cap(adjacent, true);
			}
		}
	}
}

const QPoint SampleOffsets[4] = {{3, 2}, {2, 3}, {1, 2}, {2, 1}};
const QPoint Corners[4] = {{4, 0}, {4, 4}, {0, 4}, {0, 0}};

void DeckSurfaces(Builder& out, const Raster& raster, const Design& design,
	int deck, const Options& options, int material)
{
	const QVector<Opening> openings = FloorOpenings(design, deck);
	for(const Opening& opening : openings)
		if(opening.id) out.mesh.floorOpenings.append({opening.boundary, deck, opening.id});
	for(const AuLegacyShip::Detail& detail : design.details)
	{
		if(detail.deck != deck || (detail.detailType != 22 && detail.detailType != 33)) continue;
		const bool top = detail.detailType == 22;
		const QVector3D center = Position(AuLegacyShip::DecodePoint(detail.packedXY), deck * AuLegacyBd::DeckHeight);
		const QVector<QVector3D> points = TurretPoints(center, {1, 0, 0}, {0, 0, top ? 1.0f : -1.0f}, {0, top ? -1.0f : 1.0f, 0});
		if(options.exterior) TurretDome(out, points, top ? deck - 1 : deck, false);
		if(options.interior) TurretDome(out, points, top ? deck - 1 : deck, true);
	}
	QVector<Edge> exterior, partitions;
	for(int x = 0; x + 4 < RasterSize; x += 4)
		for(int y = 0; y + 4 < RasterSize; y += 4)
			for(int t = 0; t < 4; ++t)
			{
				const QPoint origin(x, y), sample = origin + SampleOffsets[t];
				const quint16 tag = raster.At(sample.x(), sample.y());
				if(!tag) continue;
				const int owner = raster.Owner(sample.x(), sample.y());
				const QPoint points[3] = {origin + QPoint(2, 2), origin + Corners[t], origin + Corners[(t + 1) % 4]};
				// Original building floor windows are room type 2 only, and
				// underground levels always use opaque foundation surfaces.
				const bool window = deck >= 0 && Type(tag) == 2;
				const auto plane = [&](int slot, int planeDeck, bool interior, bool up)
				{
					if(interior ? !options.interior : !options.exterior) return;
					Plane(out, points, x, y, deck * AuLegacyBd::DeckHeight, slot, planeDeck, interior, up, openings);
				};
				if(tag & 0x8000)
				{
					plane(window ? 12 : 11, deck - 1, false, true);
					plane(window ? 12 : 16, deck - 1, true, false);
					continue;
				}
				// RenderInteriorFloors (2016 0x5ccc20) emits invisible, upward
				// depth-writing triangles over ground-floor open-below rooms.
				// Draw these after the interior and before terrain, never as floors.
				if(deck == 0 && (tag & 0x2000) && options.interior)
					out.Triangle(-1, deck, true, Position(PlanPoint(points[0]), 0),
						Position(PlanPoint(points[1]), 0), Position(PlanPoint(points[2]), 0),
						{}, {}, {}, 0, 0, false, false, false, true);
				if(!(tag & 0x4000))
				{
					plane(window ? 12 : 8, deck, false, false);
					if(window) plane(12, deck, true, true);
				}
				if(!(tag & 0x2000))
				{
					if(HasInterior(Type(tag)) && (!window || (tag & 0x4000))) plane(18, deck, true, true);
					// RenderInteriorCeilings depends on the floor-buffer flags,
					// independently of the current room's interior classification.
					if(tag & 0x4000) plane(16, deck - 1, true, false);
				}
				for(int side = 0; side < 3; ++side)
				{
					const QPoint a = points[side], b = points[(side + 1) % 3];
					QPoint neighbor;
					if(side == 0) neighbor = origin + SampleOffsets[(t + 3) % 4];
					else if(side == 2) neighbor = origin + SampleOffsets[(t + 1) % 4];
					else neighbor = sample + QPoint(t == 0 ? 2 : t == 2 ? -2 : 0, t == 1 ? 2 : t == 3 ? -2 : 0);
					const quint16 other = raster.At(neighbor.x(), neighbor.y());
					const int adjacent = raster.Owner(neighbor.x(), neighbor.y());
					if(!Current(other)) exterior.append({a, b, tag});
					else if(owner != adjacent && HasInterior(Type(tag))) partitions.append({a, b, tag, other, owner, adjacent});
				}
			}
	if(deck > options.maximumDeck) return;
	if(options.exterior) ExteriorWalls(out, exterior, design, deck, material, false);
	if(options.interior) ExteriorWalls(out, exterior, design, deck, material, true);
	if(options.interior)
		for(const Edge& edge : MergeEdges(partitions))
			for(const WallSpan& span : WallSpans(edge, design, deck, false))
			{
				if(span.detailType == 55) continue;
				const double base = deck * AuLegacyBd::DeckHeight;
				const double length = Length(span.b - span.a);
				if(span.detailType == 56)
				{
					const QPointF post = Unit(span.b - span.a) * .1;
					const float repeat = WallRepeat(29, length);
					out.TiledQuad(29, deck, true, Position(span.b, base + 2.7), Position(span.a, base + 2.7),
						Position(span.a, base + 3), Position(span.b, base + 3), {0, 0}, {repeat, 0}, {repeat, 1}, {0, 1});
					out.Quad(30, deck, true, Position(span.a + post, base), Position(span.a, base), Position(span.a, base + 2.7), Position(span.a + post, base + 2.7));
					out.Quad(30, deck, true, Position(span.b, base), Position(span.b - post, base), Position(span.b - post, base + 2.7), Position(span.b, base + 2.7));
					continue;
				}
				int slot = InteriorWallSlot(Type(edge.tag), length);
				switch(span.detailType)
				{
				case 5: slot = 17; break;
				case 49: case 50: slot = 5; break;
				case 53: slot = 2; break;
				case 54: slot = 3; break;
				}
				const bool door = span.detailType && span.detailType != 56;
				if(door) out.Quad(slot, deck, true, Position(span.b, base), Position(span.a, base),
					Position(span.a, base + AuLegacyBd::DeckHeight), Position(span.b, base + AuLegacyBd::DeckHeight),
					{0, 0}, {1, 0}, {1, 1}, {0, 1}, span.detailId);
				else
				{
					const float repeat = WallRepeat(slot, length);
					out.TiledQuad(slot, deck, true, Position(span.b, base), Position(span.a, base),
						Position(span.a, base + AuLegacyBd::DeckHeight), Position(span.b, base + AuLegacyBd::DeckHeight),
						{0, 0}, {repeat, 0}, {repeat, 1}, {0, 1});
				}
			}
}

void Roof(Builder& out, const AuLegacyShip::Room& room)
{
	const QPolygonF boundary = AuLegacyShip::RoomBoundary(room);
	if(boundary.size() < 3) return;
	const int deck = room.deck;
	const double base = deck * AuLegacyBd::DeckHeight;
	double boundaryArea = 0;
	for(int i = 0; i < boundary.size(); ++i)
		boundaryArea += Cross(boundary.at(i), boundary.at((i + 1) % boundary.size()));
	const bool reverseExterior = boundaryArea < 0;
	const auto triangle = [&](int slot, const QVector3D& a, const QVector3D& b, const QVector3D& c,
		const QVector2D& ua, const QVector2D& ub, const QVector2D& uc, bool underside = false)
	{
		// RenderRoof's authored boundaries are clockwise. Builder derives
		// normals from CCW triangles, so reverse the outer skin (including
		// vertical gable ends) together with its UVs. The underside faces down.
		if((!underside && reverseExterior)
			|| (underside && QVector3D::crossProduct(b - a, c - a).z() > 0))
		{
			if(slot == 13) out.TiledTriangle(slot, deck, false, a, c, b, ua, uc, ub);
			else out.Triangle(slot, deck, false, a, c, b, ua, uc, ub);
		}
		else if(slot == 13) out.TiledTriangle(slot, deck, false, a, b, c, ua, ub, uc);
		else out.Triangle(slot, deck, false, a, b, c, ua, ub, uc);
	};
	// RenderRoof 0x5d3490: the underside sits 1/96 foot above the
	// authored deck; the top uses a 1:3 slope and does not shift its ridge.
	if(room.roomType == 24)
	{
		const QRectF bounds = boundary.boundingRect();
		const QPointF center = bounds.center();
		const double rise = std::max(bounds.width(), bounds.height()) / 6;
		const QVector3D apex = Position(center, base + rise);
		for(int i = 0; i < boundary.size(); ++i)
		{
			const QPointF a = boundary.at(i), b = boundary.at((i + 1) % boundary.size());
			triangle(11, Position(b, base), apex, Position(a, base), {0, 0}, {.5f, 1}, {1, 0});
			triangle(8, Position(a, base + RoofInset), Position(center, base + RoofInset),
				Position(b, base + RoofInset), {0, 0}, {.5f, 1}, {1, 0}, true);
		}
		return;
	}
	if(boundary.size() != 4)
	{
		out.Warning(QStringLiteral("A rectangular building roof has an invalid boundary.")); return;
	}
	QVector3D p[4];
	for(int i = 0; i < 4; ++i) p[i] = Position(boundary.at(i), base);
	QPointF first = (boundary.at(0) + boundary.at(1)) / 2;
	QPointF last = (boundary.at(2) + boundary.at(3)) / 2;
	const double halfWidth = Length(boundary.at(1) - boundary.at(0)) / 2;
	const double rise = halfWidth / 3;
	if(room.roomType == 23)
	{
		const double ridgeLength = Length(last - first);
		if(ridgeLength <= halfWidth * 2) first = last = (first + last) / 2;
		else
		{
			const QPointF along = Unit(last - first) * halfWidth;
			first += along; last -= along;
		}
	}
	const QVector3D r0 = Position(first, base + rise), r1 = Position(last, base + rise);
	const float gableRepeat = room.roomType == 22 ? WallRepeat(13, halfWidth * 2) : 1;
	const float gableTop = room.roomType == 22 ? float(rise / AuLegacyBd::DeckHeight) : 1;
	triangle(room.roomType == 22 ? 13 : 11, p[1], r0, p[0], {0, 0}, {gableRepeat / 2, gableTop}, {gableRepeat, 0});
	triangle(room.roomType == 22 ? 13 : 11, p[3], r1, p[2], {0, 0}, {gableRepeat / 2, gableTop}, {gableRepeat, 0});
	if(room.roomType == 22)
	{
		triangle(11, p[0], r0, r1, {0, 0}, {0, 1}, {1, 1});
		triangle(11, r1, p[3], p[0], {1, 1}, {1, 0}, {0, 0});
		triangle(11, p[2], r1, r0, {0, 0}, {0, 1}, {1, 1});
		triangle(11, r0, p[1], p[2], {1, 1}, {1, 0}, {0, 0});
	}
	else if(Length(last - first) <= Epsilon)
	{
		triangle(11, p[0], r0, p[3], {0, 0}, {.5f, 1}, {1, 0});
		triangle(11, p[2], r0, p[1], {0, 0}, {.5f, 1}, {1, 0});
	}
	else
	{
		// The original hip roof splits each long face into its two end
		// triangles and center rectangle. Each region spans its own tile;
		// merging them changes the artwork even when the plane is identical.
		const QPointF along = Unit(boundary.at(3) - boundary.at(0)) * halfWidth;
		const QVector3D s0 = Position(boundary.at(0) + along, base);
		const QVector3D s1 = Position(boundary.at(3) - along, base);
		const QVector3D t0 = Position(boundary.at(1) + along, base);
		const QVector3D t1 = Position(boundary.at(2) - along, base);
		triangle(11, r0, s0, p[0], {1, 1}, {1, 0}, {0, 0});
		triangle(11, s0, r0, r1, {0, 0}, {0, 1}, {1, 1});
		triangle(11, r1, s1, s0, {1, 1}, {1, 0}, {0, 0});
		triangle(11, p[3], s1, r1, {1, 0}, {0, 0}, {0, 1});
		triangle(11, r1, t1, p[2], {1, 1}, {1, 0}, {0, 0});
		triangle(11, t1, r1, r0, {0, 0}, {0, 1}, {1, 1});
		triangle(11, r0, t0, t1, {1, 1}, {1, 0}, {0, 0});
		triangle(11, p[1], t0, r0, {1, 0}, {0, 0}, {0, 1});
	}
	const QVector3D a = Position(boundary.at(0), base + RoofInset), b = Position(boundary.at(1), base + RoofInset);
	const QVector3D c = Position(boundary.at(2), base + RoofInset), d = Position(boundary.at(3), base + RoofInset);
	triangle(8, b, a, d, {0, 0}, {0, 1}, {1, 1}, true);
	triangle(8, d, c, b, {1, 1}, {1, 0}, {0, 0}, true);
}

void SharedDetails(Builder& out, const Design& design, const Options& options)
{
	const AuLegacyShip::Design authored = AuthoringDesign(design, options);
	// DpBuilding::RenderInterior uses the same original stair, berth, ladder,
	// surgery and transporter primitives, with independent building UV slots.
	// Rebase one authored deck at a time before calling shared primitives.
	// Their spacecraft +/-28 guard must never clip a taller building.
	for(int deck = options.minimumDeck; deck <= options.maximumDeck + 1; ++deck)
	{
		AuLegacyShip::Design level, supported;
		for(const AuLegacyShip::Detail& detail : authored.details)
		{
			if(detail.deck != deck) continue;
			AuLegacyShip::Detail local = detail; local.deck = 0;
			level.details.append(local);
			const int t = detail.detailType;
			if(t == 2 || (t >= 25 && t <= 31) || (t >= 65 && t <= 72)
				|| AuLegacyShip::Rendering::IsHatchDetail(t) || t == 11 || t == 20)
				supported.details.append(local);
		}
		AuLegacyShip::Rendering::Mesh mesh, walking, furniture;
		AuLegacyShip::Rendering::AppendEquipment(supported, mesh);
		AuLegacyShip::Rendering::AppendStairGeometry(level, mesh);
		AuLegacyShip::Rendering::AppendStairWalkingSurfaces(level, walking);
		AuLegacyShip::Design resourceDetails;
		for(const AuLegacyShip::Detail& detail : level.details)
			if(detail.detailType == 74 || detail.detailType == 75 || detail.detailType == 21
				|| detail.detailType == 22 || (detail.detailType >= 33 && detail.detailType <= 40))
				resourceDetails.details.append(detail);
		AuLegacyShip::Rendering::AppendResourceEquipment(resourceDetails, furniture);
		const auto append = [&](const AuLegacyShip::Rendering::Mesh& source, bool collisionOnly)
		{
			for(const AuLegacyShip::Rendering::Batch& batch : source.batches)
			{
				using Material = AuLegacyShip::Rendering::Material;
				int slot = -1;
				switch(batch.material)
				{
				case Material::Berth: slot = 0; break;
				case Material::BerthBottom: slot = 1; break;
				case Material::Ladder: slot = 27; break;
				case Material::LadderPole: slot = 28; break;
				case Material::Surgery: slot = 34; break;
				case Material::TransporterPad: slot = 35; break;
				case Material::StairCasing: slot = 31; break;
				case Material::StairRiser: slot = 32; break;
				case Material::StairTread: slot = 33; break;
				default: break;
				}
				if(slot < 0) continue;
				const QVector3D offset(0, 0, float(deck * AuLegacyBd::DeckHeight));
				for(int i = 0; i + 2 < batch.vertices.size(); i += 3)
				{
					const auto& a = batch.vertices.at(i); const auto& b = batch.vertices.at(i + 1); const auto& c = batch.vertices.at(i + 2);
					out.Triangle(slot, batch.deck + deck, true, a.position + offset, b.position + offset, c.position + offset,
						a.uv, b.uv, c.uv, batch.doorId, batch.detailId, batch.twoSided, slot >= 31 && slot <= 33,
						collisionOnly, false, batch.equipment);
				}
			}
		};
		append(mesh, false); append(walking, true);
		for(const AuLegacyShip::Rendering::Batch& batch : furniture.batches)
		{
			if(out.vertices + batch.vertices.size() > MaximumVertices)
			{
				out.Warning(QStringLiteral("Building resource geometry exceeds its vertex budget.")); break;
			}
			Batch imported; imported.vertices = batch.vertices; imported.deck = deck + batch.deck;
			imported.textureSlot = -1; imported.interior = true; imported.detailId = batch.detailId;
			imported.equipment = batch.equipment;
			imported.textureFile = batch.textureFile; imported.tint = batch.tint;
			imported.selfIllumination = batch.selfIllumination; imported.twoSided = batch.twoSided;
			const QVector3D offset(0, 0, float(deck * AuLegacyBd::DeckHeight));
			for(auto& vertex : imported.vertices)
			{
				vertex.position += offset;
				if(!out.hasBounds) { out.mesh.minimum = out.mesh.maximum = vertex.position; out.hasBounds = true; }
				for(int axis = 0; axis < 3; ++axis)
				{
					out.mesh.minimum[axis] = std::min(out.mesh.minimum[axis], vertex.position[axis]);
					out.mesh.maximum[axis] = std::max(out.mesh.maximum[axis], vertex.position[axis]);
				}
			}
			out.vertices += imported.vertices.size(); out.mesh.batches.append(std::move(imported));
		}
		for(const QString& warning : furniture.warnings) out.Warning(warning);
	}
}

void FlagPole(Builder& out, const QPointF& location, int deck)
{
	// CreateFlagPole 0x59d8e0: eight sides, 1/6-foot radius, a half-foot
	// foundation and a one-foot pointed cap. Flag cloth is a separate actor.
	const double base = deck * AuLegacyBd::DeckHeight;
	const float twoPi = 6.2831855f;
	QVector3D ring[9];
	for(int i = 0; i <= 8; ++i)
	{
		const float angle = float(i) * .125f * twoPi;
		ring[i] = QVector3D(float(location.x()) + std::cos(angle) * .16666667f,
			float(location.y()) + std::sin(angle) * .16666667f, float(base));
	}
	const QVector3D apex = Position(location, base + 32);
	for(int i = 1; i <= 8; ++i)
	{
		const QVector3D a = ring[i] + QVector3D(0, 0, -.5f), b = ring[i - 1] + QVector3D(0, 0, -.5f);
		const QVector3D c = ring[i - 1] + QVector3D(0, 0, 31), d = ring[i] + QVector3D(0, 0, 31);
		const float current = float(i) / 8, previous = float(i - 1) / 8;
		out.Quad(7, deck, false, a, b, c, d, {current, 0}, {previous, 0}, {previous, 1}, {current, 1});
		out.Triangle(7, deck, false, c, apex, d, {previous, 1}, {.5f, 0}, {current, 1});
	}
}

void StreetLight(Builder& out, const QPointF& location, int deck, float foundationDepth = 0)
{
	// DpDevelopment::CreateStreetLight 0x64df90. The glass shade is the
	// separate BuildingTexture 37 tile; no spacecraft light geometry is used.
	// Keep lamp triangles in separate batches so nearby solid geometry that
	// shares their texture retains collision.
	const bool previousNoCollision = out.noCollision;
	out.noCollision = true;
	const double base = deck * AuLegacyBd::DeckHeight;
	const float twoPi = 6.2831855f;
	const QVector3D center = Position(location, base);
	const QVector3D buriedSupport(0, 0, foundationDepth);
	out.mesh.exteriorLights.append({center + QVector3D(0, 0, 25.5f), false});
	QVector3D shaft[9], rim[9];
	for(int i = 0; i <= 8; ++i)
	{
		const float angle = float(i) * .125f * twoPi;
		const float cosine = std::cos(angle), sine = std::sin(angle);
		shaft[i] = center + QVector3D(cosine * .2916666567f, sine * .2916666567f, 0);
		rim[i] = center + QVector3D(cosine * 1.25f, sine * 1.25f, 26);
	}
	const QVector3D apex = center + QVector3D(0, 0, 26.5f);
	for(int i = 1; i <= 8; ++i)
	{
		const float current = float(i) / 8, previous = float(i - 1) / 8;
		// CreateStreetLight emits native clockwise faces. Builder expects
		// counterclockwise faces, like the rest of the shared collision mesh.
		// Reverse positions and their UVs together to keep the original art.
		out.Quad(7, deck, false, shaft[i - 1] - buriedSupport, shaft[i] - buriedSupport,
			shaft[i] + QVector3D(0, 0, 25), shaft[i - 1] + QVector3D(0, 0, 25),
			{previous, 0}, {current, 0}, {current, 1}, {previous, 1});
		out.Triangle(7, deck, false, rim[i - 1], rim[i], apex, {previous, 0}, {current, 0}, {previous + .0625f, 1});
		out.Quad(37, deck, false, shaft[i - 1] + QVector3D(0, 0, 25), shaft[i] + QVector3D(0, 0, 25),
			rim[i], rim[i - 1], {0, 0}, {1, 0}, {1, 1}, {0, 1});
	}
	out.noCollision = previousNoCollision;
}

void RunwayLight(Builder& out, const QPointF& location, float foundationDepth = 0)
{
	// DpDevelopment::CreateRunwayLight 0x64bc40: eight-sided one-foot
	// fixture, 1/6-foot stem and half-foot light cap (BuildingTexture 38).
	out.mesh.exteriorLights.append({Position(location, 1), true});
	for(int i=0;i<8;++i)
	{
		const float a=float(i)*6.2831855f/8, b=float(i+1)*6.2831855f/8;
		const auto p=[&](float angle,float radius,float z) {
			return QVector3D(float(location.x())+std::cos(angle)*radius,
				float(location.y())+std::sin(angle)*radius,z); };
		out.Quad(7,0,false,p(a,1.f/6,-foundationDepth),p(b,1.f/6,-foundationDepth),p(b,1.f/6,2.f/3),p(a,1.f/6,2.f/3));
		out.Quad(38,0,false,p(a,1.f/6,2.f/3),p(b,1.f/6,2.f/3),p(b,.5f,5.f/6),p(a,.5f,5.f/6));
		out.Triangle(38,0,false,p(a,.5f,5.f/6),p(b,.5f,5.f/6),Position(location,1));
	}
}
}

int Mesh::TriangleCount() const
{
	int count = 0; for(const Batch& batch : batches) count += batch.vertices.size() / 3; return count;
}

QString MaterialAtlas(quint16 commodity)
{
	static const char* names[] = {"Wood", "Lumber", "Stone", "Metal", "Magmium", "Vulcium"};
	const int index = MaterialIndex(commodity);
	return index < 0 ? QString() : QStringLiteral(":/legacy/cities/materials/Bld%1.png").arg(QString::fromLatin1(names[index]));
}

int SiteTextureSlot(quint16 commodity, bool constructed)
{
	// DpBuilding::AdjustAltMSL, AuAtom13 0x597380. Original WORLDTEX
	// indices 10,9,8,7,3,2 map to the six material images below.
	const int material = MaterialIndex(commodity);
	static const int materialSiteTextures[] = {10, 9, 8, 7, 3, 2};
	return material < 0 ? -1 : constructed ? materialSiteTextures[material] : 1;
}

QString SiteTextureFile(int slot)
{
	// DaWorldLL::CreateTerrainTexture, AuAtom13 0x5144c0: retain WORLDTEX order.
	static const char* names[] = {"ConstructionRoad", "ConstructionZone", "ConcreteDarkModerate",
		"ConcreteHeavy", "AsphaltHeavy", "Tarmac", "DirtSmooth", "BrickIrregular",
		"CobblestoneLight", "BrickLight", "DirtRocky", "DirtCrackedModerate",
		"DirtCrackedSmooth", "Grass", "Pebbles", "RockCrushed", "RockRedSmooth", "RockTan"};
	return slot < 0 || slot >= 18 ? QString()
		: QStringLiteral(":/legacy/cities/sites/%1.png").arg(QString::fromLatin1(names[slot]));
}

QRect TextureTile(int slot)
{
	// BuildingTexture 0x59ad70 uses a bottom-origin 128 x 4096 atlas.
	// Both 2016 AuGLTexture::CreateFromBitmap (0x36b00) and the modern
	// upload reverse QImage rows. Convert the logical rect to the top-origin
	// 256 x 8192 PNG before cropping; preserve its orientation on upload.
	static const int tiles[40][4] = {
		{0,480,128,32},{0,480,24,12},{0,1920,128,128},{0,2304,128,128},
		{0,2688,128,128},{0,1408,64,128},{0,960,64,64},{96,544,32,64},
		{0,3456,128,128},{0,1664,64,128},{0,1088,64,64},{0,3584,128,128},
		{0,3200,128,128},{0,3712,128,128},{0,448,128,32},{0,3328,128,128},
		{0,0,128,128},{0,1152,64,128},{0,3840,128,128},{0,832,64,64},
		{96,1152,32,128},{0,3968,128,128},{64,1152,32,128},{0,256,128,128},
		{96,1408,32,128},{0,128,128,128},{64,1408,32,128},{0,640,64,128},
		{0,640,11,128},{96,624,32,16},{96,608,16,16},{0,416,128,32},
		{0,412,128,36},{0,384,128,32},{64,640,64,128},{64,608,32,32},
		{0,3072,128,128},{96,512,16,16},{96,528,16,16},{0,2944,128,128}
	};
	if(slot < 0 || slot >= 40) return QRect();
	return QRect(tiles[slot][0] * 2, 8192 - (tiles[slot][1] + tiles[slot][3]) * 2,
		tiles[slot][2] * 2, tiles[slot][3] * 2);
}

QVector2D TextureInset(int slot)
{
	const QRect tile = TextureTile(slot);
	return tile.isEmpty() ? QVector2D() : QVector2D(1.0f / tile.width(), 1.0f / tile.height());
}

AuLegacyShip::Design AuthoringDesign(const Design& design, const Options& options)
{
	AuLegacyShip::Design result = design.AuthoringDesign();
	result.rooms.erase(std::remove_if(result.rooms.begin(), result.rooms.end(), [&](const AuLegacyShip::Room& room)
		{ return room.deck < options.minimumDeck || room.deck > options.maximumDeck + 1; }), result.rooms.end());
	result.details.erase(std::remove_if(result.details.begin(), result.details.end(), [&](const AuLegacyShip::Detail& detail)
		{ return detail.deck < options.minimumDeck || detail.deck > options.maximumDeck
			+ (AuLegacyShip::Rendering::IsHatchDetail(detail.detailType) ? 1 : 0); }), result.details.end());
	return result;
}

Mesh Build(const Design& design, const Options& options)
{
	Builder out;
	const int material = MaterialIndex(options.materialCommodity);
	if(material < 0 || options.minimumDeck < -127 || options.maximumDeck > 126
		|| options.minimumDeck > options.maximumDeck || options.maximumDeck - options.minimumDeck > 127)
	{
		out.Warning(QStringLiteral("Invalid original building material or constructed deck bounds.")); return out.mesh;
	}
	if(!options.constructed)
	{
		// RenderGroundFloorUnderConstruction 0x5bfbb0 triangulates the
		// authored deck-zero footprint with untextured, upward vertices.
		// This rendering state does not start any construction simulation.
		if(options.exterior)
			for(const AuLegacyShip::Room& room : design.rooms)
			{
				if(room.deck != 0) continue;
				AuPointArray boundary, triangles;
				for(const QPointF& p : AuLegacyShip::RoomBoundary(room)) boundary.append(AuPoint(p.x(), p.y(), 0));
				QVector<bool> edges;
				boundary.TriangulateBoundary(triangles, edges);
				for(int i = 0; i + 2 < triangles.size(); i += 3)
				{
					const AuPoint& a = triangles.at(i); const AuPoint& b = triangles.at(i + 1); const AuPoint& c = triangles.at(i + 2);
					const QVector3D p(float(a.x), float(a.y), float(a.z));
					const QVector3D q(float(b.x), float(b.y), float(b.z));
					const QVector3D r(float(c.x), float(c.y), float(c.z));
					if(QVector3D::crossProduct(q - p, r - p).z() >= 0) out.Triangle(-1, 0, false, p, q, r, {}, {}, {});
					else out.Triangle(-1, 0, false, p, r, q, {}, {}, {});
				}
			}
		return std::move(out.mesh);
	}
	Raster raster;
	for(int deck = options.minimumDeck; deck <= options.maximumDeck + 1; ++deck)
	{
		if(deck <= options.maximumDeck)
			for(int i = 0; i < design.rooms.size(); ++i)
			{
				const AuLegacyShip::Room& room = design.rooms.at(i);
				if(room.deck != deck || room.roomType >= 22) continue;
				QString error;
				const QPolygonF boundary = AuLegacyShip::RoomBoundary(room, &error);
				if(!AuLegacyShip::Geometry::IsSupportedBoundary(boundary, &error))
				{
					out.Warning(error); continue;
				}
				raster.Fill(boundary, room.roomType, room.roomSubtype, i);
				FloorState state; state.deck = deck; state.roomId = room.roomId; state.roomType = room.roomType;
				state.boundary = boundary; state.floor = room.roomType != 11 && room.roomType != 15;
				state.ceiling = room.roomType != 9; out.mesh.floors.append(state);
			}
		DeckSurfaces(out, raster, design, deck, options, material);
		raster.NextDeck();
	}
	if(options.exterior)
		for(const AuLegacyShip::Room& room : design.rooms)
			if(room.deck >= options.minimumDeck && room.deck <= options.maximumDeck + 1
				&& room.roomType >= 22 && room.roomType <= 24)
			{
				Roof(out, room);
				FloorState state; state.deck = room.deck; state.roomId = room.roomId; state.roomType = room.roomType;
				state.boundary = AuLegacyShip::RoomBoundary(room); state.roof = true; out.mesh.floors.append(state);
			}
	if(options.interior) SharedDetails(out, design, options);
	if(options.exterior)
		for(const AuLegacyShip::Detail& detail : design.details)
		{
			if(detail.deck < options.minimumDeck || detail.deck > options.maximumDeck) continue;
			const QPointF location = AuLegacyShip::DecodePoint(detail.packedXY);
			if(detail.detailType == 100) FlagPole(out, location, detail.deck);
			else if(detail.detailType == 142) StreetLight(out, location, detail.deck);
		}
	return std::move(out.mesh);
}

Mesh BuildProcedural(const AuLegacyCity::Development& dev, const RoadFixtureHeight& roadFixtureHeight,
	const QVector<QPolygonF>& roadSurfaceOccluders)
{
	if(dev.industryId == 129) return AuLegacyBd::Biodome::Build(dev);
	Builder out;
	if(dev.demolished) return out.mesh;
	if(dev.industryId == 1 || dev.industryId == 10)
	{
		if(!dev.constructed) return out.mesh;
		FlagPole(out, QPointF(), 0);
		// Cloth uses the owning empire's image at upload time. Keep its surface
		// separate from solid collision, like the original flag VAO.
		out.Quad(-2,0,false,{0,0,31},{8,0,31},{8,0,23},{0,0,23},
			{0,1},{1,1},{1,0},{0,0},0,0,true);
		for(auto& batch : out.mesh.batches) if(batch.textureSlot == -2) batch.empireFlag = true;
		float radius = 0;
		if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
			for(const auto& process : catalogue->ConstructionProcesses())
				if(process.industry.value == dev.industryId) { radius = float(process.buildRadius * FeetPerMeter()); break; }
		if(dev.industryId == 1)
		{
			// DpBuilding::CreateTownSquare 0x59e310: one street light, its
			// repeatable position derived from the site's original coordinates.
			const float noise = AuNoiseF::IntegerNoise(0, int(dev.location.Latitude() * 57.295776f * 100),
				int(dev.location.Longitude() * 57.295776f * 100));
			const float offset = std::abs(std::sin(noise * 3.1415927f)) * radius;
			StreetLight(out, QPointF(offset, offset), 0);
		}
		else StreetLight(out,QPointF(0,radius),0);
	}
	if(dev.industryId == 2)
	{
		const float halfWidth=float(dev.roadWidthMeters*FeetPerMeter()/2)+8.f;
		const float length=float(dev.roadLengthMeters*FeetPerMeter());
		if(dev.constructed && dev.components.isEmpty() && !dev.roadSlab && length > 0
			&& (dev.constructionProcessId == 3 || dev.constructionProcessId == 4))
		{
			// Restore the older streets' yellow dashed center line. A separate
			// visual surface keeps its width fixed for every road width and lets
			// asphalt be darker without changing slabs or other terrain paving.
			const auto surfacePoint = [&](float x, float along, float lift) {
				const float grade=float(dev.roadStartAltitudeMSL-dev.altitudeMSL
					+(dev.roadEndAltitudeMSL-dev.roadStartAltitudeMSL)*(along/length));
				const QVector3D p(x,along-length/2,grade);
				return QVector3D(x,p.y(),float(roadFixtureHeight ? roadFixtureHeight(p).altitude : grade)+lift);
			};
			// Match the existing terrain sampler at intersections and subdivide
			// for globe curvature. The runtime projects these vertices onto MSL.
			// Include both paint edges and every dash endpoint in the pavement
			// grid, so the paint cannot sink into its own surface on a slope.
			const int sideCount=std::max(1,int(std::ceil((halfWidth-.25f)/15.f)));
			QVector<float> columns;
			for(int x=0;x<=sideCount;++x) columns.append(-halfWidth+(halfWidth-.25f)*x/sideCount);
			for(int x=0;x<=sideCount;++x) columns.append(.25f+(halfWidth-.25f)*x/sideCount);
			const int across=columns.size()-1;
			const int alongCount=std::max(1,int(std::ceil(length/10.f)));
			QVector<QVector3D> surface;
			surface.reserve((across+1)*(alongCount+1));
			for(int y=0;y<=alongCount;++y)
				for(int x=0;x<=across;++x)
					surface.append(surfacePoint(columns[x],std::min(length,y*10.f),.04f));
			const auto appendSurface = [&](int slot, const QVector3D& a, const QVector3D& b,
				const QVector3D& c, const QVector3D& d, float lift) {
				QPolygonF polygon;
				for(const QVector3D& p : {a,b,c,d}) polygon.append(QPointF(p.x(),p.y()));
				QVector<QPolygonF> pieces = {polygon};
				bool clipped=false;
				for(const QPolygonF& boundary : roadSurfaceOccluders)
				{
					if(!polygon.boundingRect().intersects(boundary.boundingRect())) continue;
					QVector<QPolygonF> remainder;
					for(const QPolygonF& piece : pieces) ClipConvexPolygon(piece,boundary,remainder);
					pieces.swap(remainder); clipped=true;
					if(pieces.isEmpty()) return;
				}
				const auto uv = [length](const QVector3D& p) { return QVector2D(p.x()/15.f,(p.y()+length/2)/15.f); };
				if(!clipped)
				{ out.Quad(slot,0,false,a,b,c,d,uv(a),uv(b),uv(c),uv(d)); return; }
				for(const QPolygonF& piece : pieces)
					for(int i=1;i+1<piece.size();++i)
					{
						const QVector3D p=surfacePoint(float(piece[0].x()),float(piece[0].y())+length/2,lift);
						const QVector3D q=surfacePoint(float(piece[i].x()),float(piece[i].y())+length/2,lift);
						const QVector3D r=surfacePoint(float(piece[i+1].x()),float(piece[i+1].y())+length/2,lift);
						out.Triangle(slot,0,false,p,q,r,uv(p),uv(q),uv(r));
					}
			};
			for(int y=0;y<alongCount;++y)
			{
				for(int x=0;x<across;++x)
				{
					const int first=y*(across+1)+x,next=first+across+1;
					appendSurface(-3,surface[first],surface[first+1],surface[next+1],surface[next],.04f);
				}
			}
			// Six-inch stripe, ten-foot dashes and twenty-foot gaps. x == 0 is
			// the authored road center, independent of pavement width/margins.
			for(int y=0;y<alongCount;y+=3)
			{
				const int first=y*(across+1)+sideCount,next=first+across+1;
				const QVector3D lift(0,0,.02f);
				appendSurface(-4,surface[first]+lift,surface[first+1]+lift,surface[next+1]+lift,surface[next]+lift,.06f);
			}
			for(auto& batch : out.mesh.batches)
			{
				if(batch.textureSlot == -3)
				{
					batch.surfaceOverlay=true;
					batch.textureFile=SiteTextureFile(dev.constructionProcessId == 3 ? 4 : 3);
					if(dev.constructionProcessId == 3) batch.tint=QVector4D(.35f,.35f,.35f,1);
				}
				else if(batch.textureSlot == -4)
				{ batch.surfaceOverlay=true; batch.tint=QVector4D(.95f,.72f,.08f,1); }
			}
		}
		if(dev.constructed && (dev.constructionProcessId == 3 || dev.constructionProcessId == 4 || dev.constructionProcessId == 7))
		{
			// DpGlobeRoad::CreateVAO 0x6531f0. Paved roads alternate street
			// lights every 100 feet; slabs have paired runway lights every 50.
			const float spacing=dev.roadSlab ? 50.f : 100.f;
			for(int i=0;i<=int(std::ceil(length/spacing));++i)
			{
				const float along=std::min(length,i*spacing);
				const float grade=float(dev.roadStartAltitudeMSL-dev.altitudeMSL
					+(dev.roadEndAltitudeMSL-dev.roadStartAltitudeMSL)*(along/length));
				for(int fixture=0;fixture<(dev.roadSlab ? 2 : 1);++fixture)
				{
					const QPointF base(dev.roadSlab ? (fixture ? halfWidth : -halfWidth)
						: (i%2 ? halfWidth : -halfWidth),along-length/2);
					// A neighboring building can lower the ground below the stored
					// road grade. Move each complete fixture and its light together.
					const RoadFixtureGround ground=roadFixtureHeight
						? roadFixtureHeight(QVector3D(base.x(),base.y(),grade)) : RoadFixtureGround(grade,grade);
					const float altitude=float(ground.altitude);
					const float foundationDepth=float(std::max(0.0,ground.altitude-ground.foundationAltitude));
					const int firstBatch=out.mesh.batches.size();
					const int firstLight=out.mesh.exteriorLights.size();
					out.batchIndices.clear();
					if(dev.roadSlab) RunwayLight(out,base,foundationDepth);
					else StreetLight(out,base,0,foundationDepth);
					for(int batch=firstBatch;batch<out.mesh.batches.size();++batch)
						for(auto& vertex : out.mesh.batches[batch].vertices) vertex.position.setZ(vertex.position.z()+altitude);
					for(int light=firstLight;light<out.mesh.exteriorLights.size();++light)
						out.mesh.exteriorLights[light].position.setZ(out.mesh.exteriorLights[light].position.z()+altitude);
				}
			}
		}
		// Even dirt roads with no above-ground triangles occupy their full site.
		out.mesh.minimum=QVector3D(-halfWidth,-length/2,float(std::min(dev.roadStartAltitudeMSL,dev.roadEndAltitudeMSL)-dev.altitudeMSL-.5));
		out.mesh.maximum=QVector3D(halfWidth,length/2,float(std::max(dev.roadStartAltitudeMSL,dev.roadEndAltitudeMSL)-dev.altitudeMSL+27));
	}
	// Other road surfaces and construction markings retain terrain paving.
	return std::move(out.mesh);
}

bool ReadGeometryFixture(const QByteArray& source, QVector<FixtureTriangle>& triangles, QString& error)
{
	if(source.size() < 10 || source.size() > MaximumCacheBytes
		|| (quint8(source.at(0)) != 0x1d && quint8(source.at(0)) != 0x1f) || quint8(source.at(1)) != 0x12)
	{
		error = QStringLiteral("Invalid bounded original building geometry envelope."); return false;
	}
	QDataStream envelope(source); envelope.setVersion(QDataStream::Qt_4_0);
	quint8 first = 0, second = 0; quint32 length = 0;
	envelope >> first >> second >> length;
	if(length != quint32(source.size() - 6))
	{
		error = QStringLiteral("Original geometry QByteArray length mismatch."); return false;
	}
	QByteArray payload;
	if(!Uncompress(source.mid(6), payload, error)) return false;
	QDataStream stream(payload); stream.setVersion(QDataStream::Qt_4_0);
	stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
	quint32 count = 0; stream >> count;
	if(count > quint32(MaximumRecords) || quint64(count) * 193 + 4 != quint64(payload.size()))
	{
		error = QStringLiteral("Original geometry triangle count exceeds its bounded stream."); return false;
	}
	QVector<FixtureTriangle> candidate; candidate.resize(int(count));
	for(FixtureTriangle& triangle : candidate)
	{
		for(double& value : triangle.coordinates)
		{
			stream >> value;
			if(!std::isfinite(value)) { error = QStringLiteral("Nonfinite original geometry vertex."); return false; }
		}
		stream >> triangle.flag;
	}
	if(stream.status() != QDataStream::Ok || !stream.atEnd())
	{
		error = QStringLiteral("Truncated original geometry stream."); return false;
	}
	triangles = std::move(candidate); error.clear(); return true;
}
}
}
