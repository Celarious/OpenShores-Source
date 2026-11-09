#include "aulegacysccollision.h"
#include "aubox.h"
#include "aulegacyship.h"
#include "aulegacyshipmesh.h"
#include "auscdesign.h"
#include "auscparts.h"
#include "auscreporthelpers.h"

#include <QVector>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <limits>

namespace AuLegacyScGeometry
{
namespace
{
constexpr double Skin = .01;
constexpr double GroundTolerance = 1.0 / 12;
constexpr double MaximumFall = 1000000;
constexpr int MaximumCollisionTriangles = 1000000;
constexpr double MaximumReplayDistance = 6.561679790026246;
constexpr double MaximumBoardingStep = 1.38; // 1.38 to account for the ground tolerance plus a bit of margin
constexpr double CoplanarNormalTolerance = 1e-8;
constexpr double CoplanarPlaneTolerance = 1e-5;

double Coordinate(const AuPoint& point, int axis)
{
	return axis == 0 ? point.x : axis == 1 ? point.y : point.z;
}

bool Finite(const AuPoint& point)
{
	return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z)
		&& std::abs(point.x) < MaximumFall && std::abs(point.y) < MaximumFall && std::abs(point.z) < MaximumFall;
}

struct Bounds
{
	AuPoint minimum, maximum;
	explicit Bounds(const AuPoint& point = AuPoint(0, 0, 0)) : minimum(point), maximum(point) {}
	void Include(const AuPoint& point) { minimum = minimum.Min(point); maximum = maximum.Max(point); }
	bool Intersects(const Bounds& other) const
	{
		return maximum.x >= other.minimum.x && minimum.x <= other.maximum.x
			&& maximum.y >= other.minimum.y && minimum.y <= other.maximum.y
			&& maximum.z >= other.minimum.z && minimum.z <= other.maximum.z;
	}
	bool ContainsInterior(const AuPoint& point) const
	{
		return point.x > minimum.x + Skin && point.x < maximum.x - Skin
			&& point.y > minimum.y + Skin && point.y < maximum.y - Skin
			&& point.z > minimum.z + Skin && point.z < maximum.z - Skin;
	}
	double InteriorDepth(const AuPoint& point) const
	{
		return std::min({point.x - minimum.x, maximum.x - point.x, point.y - minimum.y,
			maximum.y - point.y, point.z - minimum.z, maximum.z - point.z});
	}
	AuPoint EscapeNormal(const AuPoint& point) const
	{
		double distance = point.x - minimum.x; AuPoint normal(-1, 0, 0);
		const double distances[] = {maximum.x - point.x, point.y - minimum.y, maximum.y - point.y,
			point.z - minimum.z, maximum.z - point.z};
		const AuPoint normals[] = {{1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
		for(int index = 0; index < 5; ++index)
			if(distances[index] < distance) { distance = distances[index]; normal = normals[index]; }
		return normal;
	}
	bool IntersectsRay(const AuPoint& origin, const AuPoint& direction, double limit) const
	{
		double entry = 0, exit = limit;
		for(int axis = 0; axis < 3; ++axis)
		{
			const double component = Coordinate(direction, axis), loc = Coordinate(origin, axis);
			const double low = Coordinate(minimum, axis) - Skin, high = Coordinate(maximum, axis) + Skin;
			if(std::abs(component) < 1e-12)
			{
				if(loc < low || loc > high) return false;
				continue;
			}
			double first = (low - loc) / component, last = (high - loc) / component;
			if(first > last) std::swap(first, last);
			entry = std::max(entry, first); exit = std::min(exit, last);
			if(entry > exit) return false;
		}
		return true;
	}
};

struct Triangle
{
	AuPoint a, b, c, normal;
	Bounds bounds;
	bool stair = false;
	bool boardingStep = false;
	int obstacle = -1;
	int patch = -1;
	quint16 doorId = 0;
	QString group;
	Triangle(const AuScTri& triangle, bool stairSurface, const QString& sourceGroup)
		: a(triangle.V0()), b(triangle.V1()), c(triangle.V2()),
		  normal((b - a).Cross(c - a)), bounds(a), stair(stairSurface),
		  boardingStep(sourceGroup == QStringLiteral("legacy/boarding-step")), group(sourceGroup)
	{
		normal.Unit(); bounds.Include(b); bounds.Include(c);
	}
	Triangle(const AuPoint& first, const AuPoint& second, const AuPoint& third, bool stairSurface, const QString& sourceGroup)
		: a(first), b(second), c(third), normal((b - a).Cross(c - a)), bounds(a), stair(stairSurface), group(sourceGroup)
	{
		normal.Unit(); bounds.Include(b); bounds.Include(c);
	}
	bool Contains(const AuPoint& point) const
	{
		return (b - a).Cross(point - a).Dot(normal) >= -1e-7
			&& (c - b).Cross(point - b).Dot(normal) >= -1e-7
			&& (a - c).Cross(point - c).Dot(normal) >= -1e-7;
	}
};

struct EdgeVertex
{
	qint64 x, y, z;
	explicit EdgeVertex(const AuPoint& point)
		: x(qint64(std::llround(point.x / CoplanarPlaneTolerance))),
		  y(qint64(std::llround(point.y / CoplanarPlaneTolerance))),
		  z(qint64(std::llround(point.z / CoplanarPlaneTolerance))) {}
	bool operator==(const EdgeVertex& other) const { return x == other.x && y == other.y && z == other.z; }
	bool operator<(const EdgeVertex& other) const
	{
		return x != other.x ? x < other.x : y != other.y ? y < other.y : z < other.z;
	}
};

struct SharedEdge
{
	EdgeVertex a, b;
	SharedEdge(const AuPoint& first, const AuPoint& second) : a(first), b(second) { if(b < a) std::swap(a, b); }
	bool operator==(const SharedEdge& other) const { return a == other.a && b == other.b; }
};

uint qHash(const SharedEdge& edge, uint seed = 0)
{
	for(qint64 value : {edge.a.x, edge.a.y, edge.a.z, edge.b.x, edge.b.y, edge.b.z})
		seed ^= ::qHash(quint64(value)) + 0x9e3779b9U + (seed << 6) + (seed >> 2);
	return seed;
}

bool Coplanar(const Triangle& first, const Triangle& second)
{
	// Opposite winding describes the same plane. Shared-edge connectivity is
	// checked separately; an angle match alone must not join distant walls.
	if(std::abs(first.normal.Dot(second.normal)) < 1 - CoplanarNormalTolerance) return false;
	for(const AuPoint& point : {first.a, first.b, first.c})
		if(std::abs((point - second.a).Dot(second.normal)) > CoplanarPlaneTolerance) return false;
	for(const AuPoint& point : {second.a, second.b, second.c})
		if(std::abs((point - first.a).Dot(first.normal)) > CoplanarPlaneTolerance) return false;
	return true;
}

QString DescribeTriangle(const QString& stage, const Triangle& triangle)
{
	return QStringLiteral("stage=%1 group=%2 geometricNormal=(%3,%4,%5) triangleA=(%6,%7,%8) triangleB=(%9,%10,%11) triangleC=(%12,%13,%14)")
		.arg(stage).arg(triangle.group).arg(triangle.normal.x).arg(triangle.normal.y).arg(triangle.normal.z)
		.arg(triangle.a.x).arg(triangle.a.y).arg(triangle.a.z).arg(triangle.b.x).arg(triangle.b.y).arg(triangle.b.z)
		.arg(triangle.c.x).arg(triangle.c.y).arg(triangle.c.z);
}

struct Body
{
	AuPoint corners[8], edges[3], axes[3], center;
	Bounds bounds;
	bool valid = true;
	double contactSkin = Skin;
	bool SameShape(const Body& other) const
	{
		for(int index = 0; index < 8; ++index)
			if(corners[index] != other.corners[index]) return false;
		return true;
	}
	explicit Body(const AuBox& offsets) : center(0, 0, 0), bounds(offsets.m_fCorners[0])
	{
		for(int index = 0; index < 8; ++index)
		{
			corners[index] = offsets.m_fCorners[index];
			valid = valid && Finite(corners[index]);
			bounds.Include(corners[index]); center += corners[index] / 8;
		}
		edges[0] = corners[3] - corners[0];
		edges[1] = corners[1] - corners[0];
		edges[2] = corners[4] - corners[0];
		axes[0] = edges[1].Cross(edges[2]);
		axes[1] = edges[2].Cross(edges[0]);
		axes[2] = edges[0].Cross(edges[1]);
		// A zero-size body remains a valid point query, used by native hand rays.
		if(axes[0].IsZero() && axes[1].IsZero() && axes[2].IsZero())
		{
			axes[0] = AuPoint(1, 0, 0); axes[1] = AuPoint(0, 1, 0); axes[2] = AuPoint(0, 0, 1);
		}
	}
};

void TriangleAxes(const Triangle& triangle, const Body& body, AuPoint (&axes)[13])
{
	axes[0] = body.axes[0]; axes[1] = body.axes[1]; axes[2] = body.axes[2]; axes[3] = triangle.normal;
	const AuPoint edges[] = {triangle.b - triangle.a, triangle.c - triangle.b, triangle.a - triangle.c};
	int index = 4;
	for(const AuPoint& edge : edges)
		for(const AuPoint& bodyEdge : body.edges) axes[index++] = edge.Cross(bodyEdge);
}

void ProjectBody(const Body& body, const AuPoint& axis, double& low, double& high)
{
	low = high = body.corners[0].Dot(axis);
	for(int index = 1; index < 8; ++index)
	{
		low = std::min(low, body.corners[index].Dot(axis));
		high = std::max(high, body.corners[index].Dot(axis));
	}
	const double inset = std::min(body.contactSkin, (high - low) * .25);
	low += inset; high -= inset;
}

double PlanePenetration(const Body& body, const AuPoint& origin, const AuPoint& normal, const AuPoint& planePoint)
{
	double low, high; ProjectBody(body, normal, low, high);
	return std::max(0.0, (planePoint - origin).Dot(normal) - low);
}

bool SupportedRamp(const Triangle& triangle, const Body& body, const AuPoint& origin)
{
	return triangle.stair && triangle.normal.z >= .55
		&& (origin + AuPoint(0, 0, body.bounds.minimum.z) - triangle.a).Dot(triangle.normal) >= -GroundTolerance;
}

bool TriangleOverlap(const Triangle& triangle, const Body& body, const AuPoint& origin,
	double& penetration, AuPoint& normal)
{
	if(SupportedRamp(triangle, body, origin)) return false;
	AuPoint axes[13]; TriangleAxes(triangle, body, axes);
	penetration = std::numeric_limits<double>::infinity();
	for(AuPoint axis : axes)
	{
		if(axis.Length2() < 1e-18) continue;
		axis.Unit();
		const double triangleLow = std::min({triangle.a.Dot(axis), triangle.b.Dot(axis), triangle.c.Dot(axis)});
		const double triangleHigh = std::max({triangle.a.Dot(axis), triangle.b.Dot(axis), triangle.c.Dot(axis)});
		double low, high; ProjectBody(body, axis, low, high);
		low += origin.Dot(axis); high += origin.Dot(axis);
		if(high < triangleLow - 1e-8 || low > triangleHigh + 1e-8) return false;
		const double negativeDepth = high - triangleLow, positiveDepth = triangleHigh - low;
		const double depth = std::min(negativeDepth, positiveDepth);
		if(depth < penetration)
		{
			penetration = std::max(0.0, depth);
			normal = positiveDepth <= negativeDepth ? axis : -axis;
		}
	}
	return std::isfinite(penetration);
}

// Continuous separating-axis test: an oriented body box against a triangle.
// The interval intersection gives the first contact even for a large movement
// step, without sampling through thin walls or expanding a doorway silhouette.
bool SweepTriangle(const Triangle& triangle, const Body& body, const AuPoint& origin,
	const AuPoint& displacement, double& fraction, AuPoint& normal)
{
	if(SupportedRamp(triangle, body, origin) && SupportedRamp(triangle, body, origin + displacement))
	{
		return false;
	}
	AuPoint axes[13]; TriangleAxes(triangle, body, axes);
	double entry = -std::numeric_limits<double>::infinity(), exit = std::numeric_limits<double>::infinity();
	AuPoint enteringNormal(0, 0, 0);
	double initialPenetration = std::numeric_limits<double>::infinity();
	AuPoint initialNormal(0, 0, 0);
	for(AuPoint axis : axes)
	{
		if(axis.Length2() < 1e-18) continue;
		axis.Unit();
		const double triangleLow = std::min({triangle.a.Dot(axis), triangle.b.Dot(axis), triangle.c.Dot(axis)});
		const double triangleHigh = std::max({triangle.a.Dot(axis), triangle.b.Dot(axis), triangle.c.Dot(axis)});
		double boxLow, boxHigh; ProjectBody(body, axis, boxLow, boxHigh);
		boxLow += origin.Dot(axis); boxHigh += origin.Dot(axis);
		const double negativeDepth = boxHigh - triangleLow, positiveDepth = triangleHigh - boxLow;
		const double depth = std::min(negativeDepth, positiveDepth);
		if(depth >= 0 && depth < initialPenetration)
		{
			initialPenetration = depth;
			initialNormal = positiveDepth <= negativeDepth ? axis : -axis;
		}
		const double speed = displacement.Dot(axis);
		if(std::abs(speed) < 1e-12)
		{
			if(boxHigh < triangleLow - 1e-8 || boxLow > triangleHigh + 1e-8) return false;
			continue;
		}
		double first = (triangleLow - boxHigh) / speed, last = (triangleHigh - boxLow) / speed;
		AuPoint contactNormal = -axis;
		if(first > last) { std::swap(first, last); contactNormal = axis; }
		if(first > entry) { entry = first; enteringNormal = contactNormal; }
		exit = std::min(exit, last);
		if(entry > exit) return false;
	}
	if(exit < 0 || entry > 1 || entry > exit) return false;
	if(entry <= 0)
	{
		// Native movement starts from its existing authorized posture. Its
		// movement core can already touch a face: stationary/tangent
		// movement must retain that contact, and an edge provides an escape.
		AuPoint faceSide = triangle.normal;
		// The atom pivot establishes its original side, independent of animated
		// asymmetric limbs shifting the box center across the face.
		if((origin - triangle.a).Dot(faceSide) < 0) faceSide = -faceSide;
		const double inward = -displacement.Dot(faceSide);
		const double planeDistance = (origin - triangle.a).Dot(faceSide);
		if(inward > 1e-8 && planeDistance / inward <= std::min(1.0, exit) + 1e-8)
		{
			enteringNormal = faceSide;
			fraction = 0; normal = enteringNormal; return true;
		}
		// An initially overlapping body can leave a finite jamb/hull edge
		// during this movement before crossing the face's plane. An escape
		// beyond the endpoint does not make the current inward step clear.
		if(inward > 1e-8 && exit >= 0 && exit <= 1 + 1e-8
			&& exit < planeDistance / inward - 1e-8) return false;
		// A separating triangle edge is insufficient while the body moves
		// farther into the physical plane and stays in contact at the endpoint.
		if(displacement.Dot(initialNormal) >= -1e-8 && inward <= 1e-8) return false;
		enteringNormal = inward > 1e-8 ? faceSide : initialNormal;
	}
	if(enteringNormal.IsZero()) return false;
	fraction = std::max(0.0, entry); normal = enteringNormal;
	return true;
}
}

AuBox MovementBodyOffsets(const AuBox& anatomy, const AuPoint& gravityUp)
{
	AuPoint minimum, maximum;
	anatomy.GetMinMax(minimum, maximum);
	const double radius = std::max(Skin, std::min(maximum.x - minimum.x, maximum.y - minimum.y) * .5);
	AuBox body;
	body.SetFromMinMax(AuPoint(-radius, -radius, std::min(-Skin, minimum.z)),
		AuPoint(radius, radius, std::max(Skin, maximum.z)));
	AuPoint up = gravityUp;
	up.Unit();
	if(up.IsZero()) up = AuPoint(0, 0, 1);
	const AuPoint reference = std::abs(up.x) < .9 ? AuPoint(1, 0, 0) : AuPoint(0, 1, 0);
	AuPoint forward = up.Cross(reference);
	forward.Unit();
	const AuPoint right = forward.Cross(up);
	for(AuPoint& corner : body.m_fCorners)
		corner = right * corner.x + forward * corner.y + up * corner.z;
	return body;
}

bool FindLadderToGrab(const QVector<HatchLadder>& ladders, const AuPoint& loc, double deckHeight,
	AuMatrix4x4& matrix, const std::function<bool(const AuPoint&, const AuPoint&)>& blockedPath,
	const AuPoint* routeTarget, QString* diagnostic)
{
	if(diagnostic) diagnostic->clear();
	double closest = 36;
	bool found = false;
	int onDeck = 0, withinReach = 0, blocked = 0;
	for(const HatchLadder& ladder : ladders)
	{
		if(loc.z < ladder.anchor.z - deckHeight - .05 || loc.z >= ladder.anchor.z + deckHeight - .05) continue;
		if(routeTarget)
		{
			if(std::abs(routeTarget->x - ladder.anchor.x) > .05 || std::abs(routeTarget->y - ladder.anchor.y) > .05
				|| routeTarget->z < ladder.anchor.z - deckHeight - .05 || routeTarget->z >= ladder.anchor.z + deckHeight - .05) continue;
			const double topOriginZ = routeTarget->z < ladder.anchor.z - .05 ? routeTarget->z + deckHeight : routeTarget->z;
			if(loc.z < topOriginZ - deckHeight - 1 || loc.z > topOriginZ + 1) continue;
		}
		++onDeck;
		const AuPoint target(ladder.anchor.x, ladder.anchor.y, loc.z);
		const double distance = loc.Distance2(target);
		if(distance >= closest) continue;
		++withinReach;
		if(blockedPath && blockedPath(loc, target)) { ++blocked; continue; }
		const AuPoint right = ladder.facing.Cross(AuPoint(0, 0, 1));
		matrix = AuMatrix4x4(target, right, ladder.facing);
		closest = distance; found = true;
	}
	if(!found && diagnostic)
		*diagnostic = QStringLiteral("stage=ladder-reach ladders=%1 onDeck=%2 withinReach=%3 blocked=%4")
			.arg(ladders.size()).arg(onDeck).arg(withinReach).arg(blocked);
	return found;
}

class CollisionWorld::Impl
{
public:
	struct Node
	{
		Bounds bounds;
		int first = 0, count = 0, left = -1, right = -1;
	};
	QVector<Triangle> triangles;
	QVector<Node> nodes;
	QHash<quint32, quint16> hatchCells;
	AuPoint authorOffset = AuPoint(0, 0, 0);
	QVector<Bounds> obstacles;
	struct Patch { AuPoint normal, point; };
	struct PatchContact { AuPoint normal; double depth; };
	QVector<Patch> patches;
	bool valid = false;
	Bounds boardingBounds;
	bool hasBoardingBounds = false;
	bool statefulDoors = false;

	bool DoorOpen(const Triangle& triangle, const AuDoorStateHash* doorStates) const
	{
		return triangle.doorId && doorStates && doorStates->IsDoorOpen(triangle.doorId);
	}

	void BuildPatches()
	{
		struct EdgeOwner { int triangle, next; };
		QVector<int> parents(triangles.size());
		for(int index = 0; index < parents.size(); ++index) parents[index] = index;
		const auto root = [&](int index) {
			while(parents[index] != index)
			{
				parents[index] = parents[parents[index]]; index = parents[index];
			}
			return index;
		};
		QHash<SharedEdge, int> edgeHeads;
		QVector<EdgeOwner> owners;
		for(int index = 0; index < triangles.size(); ++index)
		{
			const Triangle& triangle = triangles[index];
			const AuPoint vertices[] = {triangle.a, triangle.b, triangle.c};
			for(int edge = 0; edge < 3; ++edge)
			{
				const SharedEdge key(vertices[edge], vertices[(edge + 1) % 3]);
				const int head = edgeHeads.value(key, -1);
				for(int owner = head; owner >= 0; owner = owners[owner].next)
				{
					const int other = owners[owner].triangle;
					// A cell's face can have a tessellation seam too. Its six faces
					// retain separate planes, and different cells retain their own
					// solid-volume identities even when adjacent faces share edges.
					if(triangle.obstacle != triangles[other].obstacle) continue;
					if(triangle.doorId != triangles[other].doorId) continue;
					// A landing must not authorize existing overlap with the real
					// deck floor when the actor's feet are still below the sill.
					if(triangle.boardingStep != triangles[other].boardingStep) continue;
					const int firstRoot = root(index), secondRoot = root(other);
					if(firstRoot != secondRoot && Coplanar(triangle, triangles[other])
						&& Coplanar(triangles[firstRoot], triangles[secondRoot])) parents[secondRoot] = firstRoot;
				}
				edgeHeads.insert(key, owners.size()); owners.append(EdgeOwner{index, head});
			}
		}
		QHash<int, int> patchIds;
		for(int index = 0; index < triangles.size(); ++index)
		{
			const int representative = root(index);
			int id = patchIds.value(representative, -1);
			if(id < 0)
			{
				id = patches.size(); patchIds.insert(representative, id);
				patches.append(Patch{triangles[representative].normal, triangles[representative].a});
			}
			triangles[index].patch = id;
		}
	}

	void ContactNode(int id, const Bounds& bounds, const AuPoint& origin, const Body& body,
		QHash<int, PatchContact>& contacts, const AuDoorStateHash* doorStates) const
	{
		const Node& node = nodes[id];
		if(!node.bounds.Intersects(bounds)) return;
		if(node.count)
		{
			for(int index = node.first; index < node.first + node.count; ++index)
			{
				const Triangle& triangle = triangles[index];
				if(triangle.patch < 0 || DoorOpen(triangle, doorStates) || !triangle.bounds.Intersects(bounds)) continue;
				double depth; AuPoint normal;
				if(!TriangleOverlap(triangle, body, origin, depth, normal)) continue;
				const Patch& patch = patches[triangle.patch];
				normal = patch.normal;
				if((origin - patch.point).Dot(normal) < 0) normal = -normal;
				contacts.insert(triangle.patch, {normal, PlanePenetration(body, origin, normal, patch.point)});
			}
			return;
		}
		ContactNode(node.left, bounds, origin, body, contacts, doorStates);
		ContactNode(node.right, bounds, origin, body, contacts, doorStates);
	}

	QHash<int, PatchContact> Contacts(const AuPoint& origin, const Body& body, const AuDoorStateHash* doorStates) const
	{
		Bounds bounds(origin + body.bounds.minimum); bounds.Include(origin + body.bounds.maximum);
		QHash<int, PatchContact> contacts;
		ContactNode(0, bounds, origin, body, contacts, doorStates);
		return contacts;
	}

	int BuildNode(int first, int count)
	{
		Node node;
		node.first = first; node.count = count; node.bounds = triangles[first].bounds;
		for(int index = first + 1; index < first + count; ++index)
		{
			node.bounds.Include(triangles[index].bounds.minimum); node.bounds.Include(triangles[index].bounds.maximum);
		}
		const int id = nodes.size(); nodes.append(node);
		if(count <= 16) return id;
		const AuPoint span = node.bounds.maximum - node.bounds.minimum;
		const int axis = span.x >= span.y && span.x >= span.z ? 0 : span.y >= span.z ? 1 : 2;
		const int middle = first + count / 2;
		std::nth_element(triangles.begin() + first, triangles.begin() + middle, triangles.begin() + first + count,
			[axis](const Triangle& a, const Triangle& b) {
				return Coordinate(a.bounds.minimum + a.bounds.maximum, axis) < Coordinate(b.bounds.minimum + b.bounds.maximum, axis);
			});
		const int left = BuildNode(first, middle - first), right = BuildNode(middle, first + count - middle);
		nodes[id].left = left; nodes[id].right = right; nodes[id].count = 0;
		return id;
	}

	void SweepNode(int id, const Bounds& bounds, const Body& body, const AuPoint& from, const AuPoint& displacement,
		double& fraction, AuPoint& normal, bool& hit, const Triangle*& hitTriangle,
		const QHash<int, PatchContact>& contacts, const AuDoorStateHash* doorStates) const
	{
		const Node& node = nodes[id];
		if(!node.bounds.Intersects(bounds)) return;
		if(node.count)
		{
			for(int index = node.first; index < node.first + node.count; ++index)
			{
				const Triangle& triangle = triangles[index];
				if(DoorOpen(triangle, doorStates) || !triangle.bounds.Intersects(bounds)) continue;
				// Each equipment path cell is a volume, not just its twelve faces.
				// Catch a small actor entirely contained by a cell after a pose edit.
				if(triangle.obstacle >= 0 && displacement.Length2() < 1e-16
					&& obstacles[triangle.obstacle].ContainsInterior(from + body.center))
				{
					fraction = 0; normal = obstacles[triangle.obstacle].EscapeNormal(from + body.center); hit = true;
					hitTriangle = &triangle;
					continue;
				}
				double nextFraction; AuPoint nextNormal;
				if(SweepTriangle(triangle, body, from, displacement, nextFraction, nextNormal))
				{
					const auto contact = contacts.constFind(triangle.patch);
					if(contact != contacts.constEnd())
					{
						// Internal diagonals are not physical edges. A body already
						// touching this connected plane may slide onto its neighbor.
						// New finite-edge arrivals still use SAT because they have no
						// contact with this patch at the beginning of the query.
						if(displacement.Dot(contact->normal) >= -1e-8) continue;
						if(!triangle.stair) { nextFraction = 0; nextNormal = contact->normal; }
					}
					if(!hit || nextFraction < fraction)
					{
						fraction = nextFraction; normal = nextNormal; hit = true;
						hitTriangle = &triangle;
					}
				}
			}
			return;
		}
		SweepNode(node.left, bounds, body, from, displacement, fraction, normal, hit, hitTriangle, contacts, doorStates);
		SweepNode(node.right, bounds, body, from, displacement, fraction, normal, hit, hitTriangle, contacts, doorStates);
	}

	bool Sweep(const AuPoint& from, const AuPoint& to, const Body& body, double& fraction, AuPoint& normal,
		const Triangle** hitTriangle, const AuDoorStateHash* doorStates) const
	{
		Bounds bounds(from + body.bounds.minimum);
		bounds.Include(from + body.bounds.maximum); bounds.Include(to + body.bounds.minimum); bounds.Include(to + body.bounds.maximum);
		bounds.minimum -= AuPoint(Skin, Skin, Skin); bounds.maximum += AuPoint(Skin, Skin, Skin);
		bool hit = false; fraction = 1;
		const Triangle* triangle = nullptr;
		const auto contacts = Contacts(from, body, doorStates);
		SweepNode(0, bounds, body, from, to - from, fraction, normal, hit, triangle, contacts, doorStates);
		if(hitTriangle) *hitTriangle = triangle;
		return hit;
	}

	void BlockedDoorNode(int id, const Bounds& bounds, const AuPoint& from, const AuPoint& displacement,
		const Body& body, const QHash<int, PatchContact>& contacts, QSet<quint16>& blocked,
		double maximumFraction, const AuDoorStateHash* doorStates) const
	{
		const Node& node = nodes[id];
		if(!node.bounds.Intersects(bounds)) return;
		if(node.count)
		{
			for(int index = node.first; index < node.first + node.count; ++index)
			{
				const Triangle& triangle = triangles[index];
				if(!triangle.doorId || DoorOpen(triangle, doorStates) || !triangle.bounds.Intersects(bounds)) continue;
				const auto contact = contacts.constFind(triangle.patch);
				if(contact != contacts.constEnd() && displacement.Dot(contact->normal) >= -1e-8) continue;
				double fraction; AuPoint normal;
				if(SweepTriangle(triangle, body, from, displacement, fraction, normal)
					&& fraction <= maximumFraction + 1e-8) blocked.insert(triangle.doorId);
			}
			return;
		}
		BlockedDoorNode(node.left, bounds, from, displacement, body, contacts, blocked, maximumFraction, doorStates);
		BlockedDoorNode(node.right, bounds, from, displacement, body, contacts, blocked, maximumFraction, doorStates);
	}

	bool DestinationBlocked(int id, const Bounds& bounds, const AuPoint& from, const AuPoint& to,
		const Body& oldBody, const Body& newBody, AuPoint& normal, QString* diagnostic,
		const QHash<int, PatchContact>& contacts, double contactTolerance, const AuDoorStateHash* doorStates) const
	{
		const Node& node = nodes[id];
		if(!node.bounds.Intersects(bounds)) return false;
		if(node.count)
		{
			for(int index = node.first; index < node.first + node.count; ++index)
			{
				const Triangle& triangle = triangles[index];
				if(DoorOpen(triangle, doorStates) || !triangle.bounds.Intersects(bounds)) continue;
				if(triangle.obstacle >= 0)
				{
					const Bounds& cell = obstacles[triangle.obstacle];
					const AuPoint oldCenter = from + oldBody.center, newCenter = to + newBody.center;
					if(cell.ContainsInterior(newCenter) && (!cell.ContainsInterior(oldCenter) || !oldBody.SameShape(newBody)
						|| cell.InteriorDepth(newCenter) > cell.InteriorDepth(oldCenter) + contactTolerance + 1e-8))
					{
						if(diagnostic) *diagnostic = DescribeTriangle(QStringLiteral("destination-cell"), triangle)
							+ QStringLiteral(" oldCenterDepth=%1 newCenterDepth=%2 sameShape=%3")
								.arg(cell.InteriorDepth(oldCenter)).arg(cell.InteriorDepth(newCenter)).arg(oldBody.SameShape(newBody));
						normal = cell.EscapeNormal(newCenter); return true;
					}
				}
				double newDepth, oldDepth; AuPoint newNormal, oldNormal;
				if(!TriangleOverlap(triangle, newBody, to, newDepth, newNormal)) continue;
				bool oldOverlap = TriangleOverlap(triangle, oldBody, from, oldDepth, oldNormal);
				const auto contact = contacts.constFind(triangle.patch);
				if(contact != contacts.constEnd())
				{
					oldOverlap = true; oldDepth = contact->depth;
					newDepth = PlanePenetration(newBody, to, contact->normal, patches[triangle.patch].point);
					newNormal = contact->normal;
				}
				// Packet checks allow native floor-contact rounding while retaining
				// existing overlap or movement clear of the surface.
				if(newDepth > (oldOverlap ? oldDepth : 0) + contactTolerance + 1e-8)
				{
					if(diagnostic) *diagnostic = DescribeTriangle(QStringLiteral("destination-triangle"), triangle)
						+ QStringLiteral(" oldOverlap=%1 oldDepth=%2 newDepth=%3")
							.arg(oldOverlap).arg(oldOverlap ? oldDepth : 0).arg(newDepth);
					normal = newNormal; return true;
				}
			}
			return false;
		}
		return DestinationBlocked(node.left, bounds, from, to, oldBody, newBody, normal, diagnostic, contacts, contactTolerance, doorStates)
			|| DestinationBlocked(node.right, bounds, from, to, oldBody, newBody, normal, diagnostic, contacts, contactTolerance, doorStates);
	}

	AuPoint Slide(const AuPoint& from, const AuPoint& to, const Body& body, const AuPoint& up, AuPoint& hitNormal,
		bool preserveGroundHeight, const AuDoorStateHash* doorStates, QVector<MovementContact>* contacts = nullptr) const
	{
		AuPoint current = from, target = to;
		for(int iteration = 0; iteration < 4; ++iteration)
		{
			double fraction; AuPoint normal;
			if(!Sweep(current, target, body, fraction, normal, nullptr, doorStates)) return target;
			hitNormal = normal;
			AuPoint displacement = target - current;
			const double backoff = displacement.Length() > Skin ? Skin / displacement.Length() : 0;
			current += displacement * std::max(0.0, fraction - backoff);
			if(contacts) contacts->append({current, normal});
			AuPoint remaining = target - current;
			const double requestedVertical = remaining.Dot(up);
			const double allowedRise = std::max(0.0, requestedVertical);
			const double requestedLength2 = remaining.Length2();
			const double inward = remaining.Dot(normal);
			if(inward < 0) remaining -= normal * inward;
			// Grounded vertical movement comes from the native floor query.
			if(preserveGroundHeight && inward < 0)
			{
				// The world already fitted this destination to terrain. Hull
				// sliding must not push feet below it or add an upward step.
				remaining += up * (requestedVertical - remaining.Dot(up));
				const AuPoint horizontalNormal = normal - up * normal.Dot(up);
				const double residualInward = remaining.Dot(normal);
				if(residualInward < -1e-8)
				{
					if(horizontalNormal.Length2() < 1e-18) return current;
					remaining -= horizontalNormal * (residualInward / horizontalNormal.Length2());
					// A nearly horizontal ceiling cannot force a larger sideways
					// move than requested just to retain the terrain height.
					if(remaining.Length2() > requestedLength2 + 1e-8) return current;
				}
			}
			else if(normal.Dot(up) >= .55 && inward < 0)
			{
				remaining -= up * std::max(0.0, remaining.Dot(up) - allowedRise);
				// Capping a bevel's added rise must not restore inward movement
				// through that face. Retain its tangent component horizontally,
				// while a flat floor keeps the cancelled downward component.
				const AuPoint horizontalNormal = normal - up * normal.Dot(up);
				const double residualInward = remaining.Dot(normal);
				if(residualInward < -1e-8 && horizontalNormal.Length2() > 1e-18)
					remaining -= horizontalNormal * (residualInward / horizontalNormal.Length2());
			}
			if(remaining.Length2() < Skin * Skin) return current;
			target = current + remaining;
		}
		return current;
	}

	void RayNode(int id, const AuPoint& origin, const AuPoint& direction, double& distance,
		AuPoint& normal, bool& stair, bool* boardingStep, const AuDoorStateHash* doorStates) const
	{
		const Node& node = nodes[id];
		if(!node.bounds.IntersectsRay(origin, direction, distance)) return;
		if(node.count)
		{
			for(int index = node.first; index < node.first + node.count; ++index)
			{
				const Triangle& triangle = triangles[index];
				if(DoorOpen(triangle, doorStates)) continue;
				const double divisor = triangle.normal.Dot(direction);
				if(std::abs(divisor) < .5) continue;
				const double candidate = (triangle.a - origin).Dot(triangle.normal) / divisor;
				if(candidate < -Skin || candidate > distance) continue;
				if(!triangle.Contains(origin + direction * candidate)) continue;
				distance = std::max(0.0, candidate); normal = divisor < 0 ? triangle.normal : -triangle.normal; stair = triangle.stair;
				if(boardingStep) *boardingStep = triangle.boardingStep;
			}
			return;
		}
		RayNode(node.left, origin, direction, distance, normal, stair, boardingStep, doorStates);
		RayNode(node.right, origin, direction, distance, normal, stair, boardingStep, doorStates);
	}

	double Floor(const AuPoint& origin, const AuPoint& up, AuPoint& normal, bool& stair, bool* boardingStep,
		const AuDoorStateHash* doorStates) const
	{
		double distance = MaximumFall; normal = AuPoint(0, 0, 0); stair = false;
		if(boardingStep) *boardingStep = false;
		RayNode(0, origin, -up, distance, normal, stair, boardingStep, doorStates);
		return distance;
	}

	bool BoardingFloor(const AuPoint& from, const AuPoint& to, const Body& body, const AuPoint& up,
		double& distance, AuPoint& normal, const AuDoorStateHash* doorStates) const
	{
		AuPoint forward = to - from;
		forward -= up * forward.Dot(up);
		if(forward.Length2() < 1e-16) forward = AuPoint(0, 0, 0);
		else forward.Unit();
		double reach = 0;
		for(const AuPoint& corner : body.corners) reach = std::max(reach, corner.Dot(forward));
		// Probe the leading footprint before it hits the landing's outside edge.
		// Each query still has to hit an authored doorway landing, never a wall.
		const AuPoint probes[] = {to, from, to + forward * reach, from + forward * reach,
			from + (to - from) * .25, from + (to - from) * .5, from + (to - from) * .75};
		for(const AuPoint& probe : probes)
		{
			bool stair, boardingStep;
			const AuPoint origin = probe + up * MaximumBoardingStep;
			const double floor = Floor(origin, up, normal, stair, &boardingStep, doorStates);
			if(!boardingStep || normal.Dot(up) < .55) continue;
			const AuPoint floorPoint = origin - up * floor;
			distance = (to - floorPoint).Dot(normal) / normal.Dot(up);
			return true;
		}
		return false;
	}

	bool StairExit(const AuPoint& from, const AuPoint& to, const Body& body, const AuPoint& up,
		double standingHeight, AuPoint& destination, const AuDoorStateHash* doorStates) const
	{
		AuPoint normal; bool stair;
		const double oldFloor = Floor(from, up, normal, stair, nullptr, doorStates);
		if(!stair || normal.Dot(up) < .55 || normal.Dot(up) > .999
			|| std::abs(oldFloor - standingHeight) > .25) return false;
		AuPoint forward = to - from; forward -= up * forward.Dot(up);
		if(forward.Length2() < 1e-16) return false;
		forward.Unit();
		double reach = 0;
		for(const AuPoint& corner : body.corners) reach = std::max(reach, corner.Dot(forward));
		// Building stair exits need enough lift for the body's leading edge
		// to clear the upper floor while its origin is still over the ramp.
		const double maximumRise = 1.5;
		const AuPoint probe = to + forward * reach + up * maximumRise;
		const double floor = Floor(probe, up, normal, stair, nullptr, doorStates) - maximumRise;
		if(stair || normal.Dot(up) < .999) return false;
		const double adjustment = standingHeight + GroundTolerance - floor;
		if(adjustment < -GroundTolerance || adjustment > maximumRise) return false;
		const AuPoint candidate = to + up * std::max(0.0, adjustment);
		const double rise = (candidate - from).Dot(up);
		if(rise <= 0 || rise > maximumRise) return false;
		const AuPoint raised = from + up * rise;
		double fraction;
		// Check both legs: neither a low ceiling nor a wall can be stepped through.
		if(Sweep(from, raised, body, fraction, normal, nullptr, doorStates)
			|| Sweep(raised, candidate, body, fraction, normal, nullptr, doorStates)) return false;
		destination = candidate;
		return true;
	}
};

bool IsCollisionDesign(const AuScDesign& design)
{
	for(auto part = design.Parts().cbegin(); part != design.Parts().cend(); ++part)
		if((*part)->IsHullMesh() && ((*part)->GroupName() == QStringLiteral("legacy/debug/static/v2")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v3")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v4")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v5")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v6")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v7")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v8")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v9")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v10")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v11")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v12"))) return true;
	return false;
}

CollisionWorld::CollisionWorld(const AuScDesign& design) : m_Impl(new Impl)
{
	if(!IsCollisionDesign(design)) return;
	for(auto part = design.Parts().cbegin(); part != design.Parts().cend(); ++part)
		if((*part)->IsHullMesh() && ((*part)->GroupName() == QStringLiteral("legacy/debug/static/v6")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v7")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v8")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v9")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v10")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v11")
			|| (*part)->GroupName() == QStringLiteral("legacy/debug/static/v12")))
			m_Impl->statefulDoors = true;
	for(auto part = design.Parts().cbegin(); part != design.Parts().cend(); ++part)
	{
		if(!(*part)->IsOmitted() && (*part)->IsLadderPath()
			&& (*part)->GroupName().startsWith(QStringLiteral("legacy/hatch-ladder/detail/")))
		{
			const AuPoint anchor = (*part)->PathSegment(0).Loc2();
			const QStringList metadata = (*part)->GroupName().split('/');
			bool cellOk = false, deckOk = false;
			const int packedXY = metadata.size() == 8 ? metadata[5].toInt(&cellOk) : -1;
			const int deck = metadata.size() == 8 ? metadata[7].toInt(&deckOk) : 0;
			if(cellOk && deckOk && metadata[4] == QStringLiteral("cell") && metadata[6] == QStringLiteral("deck")
				&& packedXY >= 0 && packedXY <= 65535 && deck >= AuLegacyShip::MinimumDeck && deck <= AuLegacyShip::MaximumDeck + 1)
			{
				const QPointF center = AuLegacyShip::DecodePoint(quint16(packedXY));
				const AuPoint offset = AuPoint(center.x(), center.y(), deck * AuLegacyShip::DeckHeight) - anchor;
				if(m_Impl->hatchCells.isEmpty()) m_Impl->authorOffset = offset;
				if(Finite(offset) && offset.Distance2(m_Impl->authorOffset) < 1e-8)
				{
					const quint32 cell = (quint32(quint8(deck)) << 16) | quint16(packedXY);
					if(!m_Impl->hatchCells.contains(cell) || !m_Impl->hatchCells.value(cell))
						m_Impl->hatchCells.insert(cell, m_Impl->statefulDoors ? (*part)->AssociatedPartId() : 0);
				}
			}
		}
		if((*part)->IsOmitted() || !(*part)->IsBarrier() || !(*part)->GroupName().startsWith(QStringLiteral("legacy/"))) continue;
		AuScFaceList faces; (*part)->Faces(faces);
		AuScTriList triangles; faces.Triangles(triangles);
		if(triangles.size() > MaximumCollisionTriangles - m_Impl->triangles.size())
		{
			m_Impl->triangles.clear(); return;
		}
		const bool stair = (*part)->GroupName() == QStringLiteral("legacy/stair-support");
		const quint16 doorId = m_Impl->statefulDoors
			&& (*part)->GroupName().startsWith(QStringLiteral("legacy/door-collision/detail/"))
			&& (*part)->AssociatedPartState() == AuScDoor::Closed ? (*part)->AssociatedPartId() : 0;
		const bool obstacleCells = (*part)->GroupName().startsWith(QStringLiteral("legacy/obstacle-chunk/"))
			&& triangles.size() % 12 == 0;
		const int firstObstacle = m_Impl->obstacles.size();
		if(obstacleCells)
			for(int first = 0; first < triangles.size(); first += 12)
			{
				Bounds cell(triangles[first].V0());
				for(int index = first; index < first + 12; ++index)
				{
					cell.Include(triangles[index].V0()); cell.Include(triangles[index].V1()); cell.Include(triangles[index].V2());
				}
				m_Impl->obstacles.append(cell);
			}
		for(int index = 0; index < triangles.size(); ++index)
		{
			Triangle triangle(triangles[index], stair, (*part)->GroupName());
			triangle.doorId = doorId;
			if(!Finite(triangle.a) || !Finite(triangle.b) || !Finite(triangle.c) || triangle.normal.IsZero()) continue;
			if(obstacleCells)
			{
				triangle.obstacle = firstObstacle + index / 12;
				triangle.bounds = m_Impl->obstacles[triangle.obstacle];
			}
			if(triangle.boardingStep)
			{
				if(!m_Impl->hasBoardingBounds) m_Impl->boardingBounds = triangle.bounds;
				else
				{
					m_Impl->boardingBounds.Include(triangle.bounds.minimum);
					m_Impl->boardingBounds.Include(triangle.bounds.maximum);
				}
				m_Impl->hasBoardingBounds = true;
			}
			m_Impl->triangles.append(triangle);
		}
	}
	if(m_Impl->triangles.isEmpty()) return;
	m_Impl->BuildPatches();
	m_Impl->BuildNode(0, m_Impl->triangles.size()); m_Impl->valid = true;
}

CollisionWorld::CollisionWorld(const AuLegacyShip::Rendering::Mesh& mesh) : m_Impl(new Impl)
{
	m_Impl->statefulDoors = true;
	for(const auto& batch : mesh.batches)
	{
		const bool stairs = batch.material == AuLegacyShip::Rendering::Material::StairTread
			|| batch.material == AuLegacyShip::Rendering::Material::StairRiser;
		if(batch.equipment && !stairs && !batch.doorId) continue;
		if(batch.vertices.size() % 3 || batch.doorId > 65535
			|| batch.vertices.size() / 3 > MaximumCollisionTriangles - m_Impl->triangles.size()) return;
		for(int i = 0; i < batch.vertices.size(); i += 3)
		{
			const auto point = [](const QVector3D& p) { return AuPoint(p.x(), p.y(), p.z()); };
			Triangle triangle(point(batch.vertices[i].position), point(batch.vertices[i+1].position),
				point(batch.vertices[i+2].position), stairs, QStringLiteral("legacy/building"));
			triangle.doorId = quint16(batch.doorId);
			if(!Finite(triangle.a) || !Finite(triangle.b) || !Finite(triangle.c) || triangle.normal.IsZero()) continue;
			m_Impl->triangles.append(triangle);
		}
	}
	if(m_Impl->triangles.isEmpty()) return;
	m_Impl->BuildPatches();
	m_Impl->BuildNode(0, m_Impl->triangles.size());
	m_Impl->valid = true;
}

CollisionWorld::~CollisionWorld() {}
bool CollisionWorld::IsValid() const { return m_Impl->valid; }
int CollisionWorld::TriangleCount() const { return m_Impl->triangles.size(); }

void CollisionWorld::ExpandExtents(AuPoint& minimum, AuPoint& maximum) const
{
	if(!IsValid() || !m_Impl->hasBoardingBounds) return;
	minimum = minimum.Min(m_Impl->boardingBounds.minimum);
	maximum = maximum.Max(m_Impl->boardingBounds.maximum);
}

bool CollisionWorld::IsBodyPathBlocked(const AuPoint& from, const AuPoint& to, const AuBox& bodyOffsets,
	AuPoint& hitLoc, AuPoint& hitNormal, const AuDoorStateHash* doorStates) const
{
	const Body body(bodyOffsets);
	if(!IsValid() || !body.valid || !Finite(from) || !Finite(to))
	{
		hitLoc = from; hitNormal = AuPoint(0, 0, 0); return true;
	}
	double fraction;
	if(!m_Impl->Sweep(from, to, body, fraction, hitNormal, nullptr, doorStates)) return false;
	const AuPoint displacement = to - from;
	const double backoff = displacement.Length() > Skin ? Skin / displacement.Length() : 0;
	hitLoc = from + displacement * std::max(0.0, fraction - backoff);
	return true;
}

QVector<quint16> CollisionWorld::BlockedDoors(const AuPoint& from, const AuPoint& to, const AuBox& bodyOffsets,
	const AuDoorStateHash* doorStates) const
{
	QVector<quint16> result;
	const Body body(bodyOffsets);
	if(!IsValid() || !body.valid || !Finite(from) || !Finite(to) || from == to) return result;
	double maximumFraction; AuPoint normal;
	if(!m_Impl->Sweep(from, to, body, maximumFraction, normal, nullptr, doorStates)) return result;
	Bounds bounds(from + body.bounds.minimum);
	bounds.Include(from + body.bounds.maximum); bounds.Include(to + body.bounds.minimum); bounds.Include(to + body.bounds.maximum);
	bounds.minimum -= AuPoint(Skin, Skin, Skin); bounds.maximum += AuPoint(Skin, Skin, Skin);
	QSet<quint16> blocked;
	const auto contacts = m_Impl->Contacts(from, body, doorStates);
	m_Impl->BlockedDoorNode(0, bounds, from, to - from, body, contacts, blocked, maximumFraction, doorStates);
	result.reserve(blocked.size());
	for(quint16 id : blocked) result.append(id);
	std::sort(result.begin(), result.end());
	return result;
}

bool CollisionWorld::IsBodyTransitionBlocked(const AuPoint& from, const AuPoint& to, const AuBox& oldOffsets,
	const AuBox& newOffsets, AuPoint& hitLoc, AuPoint& hitNormal, bool allowSlideReplay, QString* diagnostic,
	const AuDoorStateHash* doorStates) const
{
	if(diagnostic) diagnostic->clear();
	const Body oldBody(oldOffsets), newBody(newOffsets);
	if(!IsValid() || !oldBody.valid || !newBody.valid || !Finite(from) || !Finite(to))
	{
		if(diagnostic) *diagnostic = QStringLiteral("stage=invalid-query");
		hitLoc = from; hitNormal = AuPoint(0, 0, 0); return true;
	}
	Bounds bounds(from + oldBody.bounds.minimum);
	bounds.Include(from + oldBody.bounds.maximum); bounds.Include(to + newBody.bounds.minimum); bounds.Include(to + newBody.bounds.maximum);
	const auto contacts = m_Impl->Contacts(from, oldBody, doorStates);
	if(m_Impl->DestinationBlocked(0, bounds, from, to, oldBody, newBody, hitNormal, diagnostic, contacts, GroundTolerance, doorStates))
	{
		hitLoc = from; return true;
	}
	// Endpoint validation already checks a stationary pose or orientation change.
	// Foot alignment must not invent translation when the origin did not move.
	if(from == to) return false;
	// Test both authorized and proposed stances. A crouch
	// need not occupy its old shape's complete translated box. Keep the feet
	// aligned while testing the alternate pose, without snapping packet height
	// to collision bounds or replacing the native ground-height query.
	const AuPoint oldTarget = to + AuPoint(0, 0, newBody.bounds.minimum.z - oldBody.bounds.minimum.z);
	const AuPoint newStart = from + AuPoint(0, 0, oldBody.bounds.minimum.z - newBody.bounds.minimum.z);
	Body oldSweep(oldBody), newSweep(newBody);
	oldSweep.contactSkin = newSweep.contactSkin = GroundTolerance;
	double oldFraction, newFraction; AuPoint oldNormal, newNormal;
	const Triangle* oldTriangle = nullptr; const Triangle* newTriangle = nullptr;
	const bool oldBlocked = m_Impl->Sweep(from, oldTarget, oldSweep, oldFraction, oldNormal, &oldTriangle, doorStates);
	const bool newBlocked = m_Impl->Sweep(newStart, to, newSweep, newFraction, newNormal, &newTriangle, doorStates);
	if(!oldBlocked || !newBlocked) return false;
	const AuPoint displacement = oldTarget - from;
	if(displacement.Length2() <= MaximumReplayDistance * MaximumReplayDistance)
	{
		AuPoint up = oldBody.edges[2]; up.Unit();
		const double rise = displacement.Dot(up);
		double floor; AuPoint floorNormal;
		if(rise > 1e-8 && rise <= MaximumBoardingStep + 1e-8
			&& m_Impl->BoardingFloor(from, oldTarget, oldBody, up, floor, floorNormal, doorStates))
		{
			// A boarding packet can span the upward step and the parent change.
			// Validate both legs continuously, including lintels and equipment.
			// Vehicle orientation can change slightly at the parent handoff;
			// check both orientations instead of requiring byte-identical bounds.
			const AuPoint raised = from + up * rise;
			const double newRise = (to - newStart).Dot(up);
			const AuPoint newRaised = newStart + up * newRise;
			double fraction; AuPoint normal;
			if(newRise >= 0 && newRise <= MaximumBoardingStep + 1e-8
				&& !m_Impl->Sweep(from, raised, oldSweep, fraction, normal, nullptr, doorStates)
				&& !m_Impl->Sweep(raised, oldTarget, oldSweep, fraction, normal, nullptr, doorStates)
				&& !m_Impl->Sweep(newStart, newRaised, newSweep, fraction, normal, nullptr, doorStates)
				&& !m_Impl->Sweep(newRaised, to, newSweep, fraction, normal, nullptr, doorStates)) return false;
		}
	}
	if(allowSlideReplay && displacement.Length2() <= MaximumReplayDistance * MaximumReplayDistance
		&& std::abs(displacement.z) <= .75)
	{
		// Client sliding can traverse more than one segment between packets.
		// Reproduce a collision-free route inside this short update's bounds;
		// each segment still uses the same continuous body sweep. No ground snap.
		const AuPoint up(0, 0, 1);
		AuPoint replayNormal;
		const AuPoint resolved = m_Impl->Slide(from, oldTarget, oldSweep, up, replayNormal, false, doorStates);
		double connectorFraction; AuPoint connectorNormal;
		if(resolved.Distance2(oldTarget) <= GroundTolerance * GroundTolerance
			&& !m_Impl->Sweep(resolved, oldTarget, oldSweep, connectorFraction, connectorNormal, nullptr, doorStates)) return false;
		// A chassis can slide horizontally until its rear clears a deck edge,
		// then fall. The endpoints' straight chord cuts through that support.
		const AuPoint waypoints[] = {{oldTarget.x, from.y, from.z}, {from.x, oldTarget.y, from.z},
			{oldTarget.x, oldTarget.y, from.z}};
		for(const AuPoint& waypoint : waypoints)
		{
			if(from.Distance(waypoint) + waypoint.Distance(oldTarget) > MaximumReplayDistance) continue;
			double fraction; AuPoint normal;
			if(!m_Impl->Sweep(from, waypoint, oldSweep, fraction, normal, nullptr, doorStates)
				&& !m_Impl->Sweep(waypoint, oldTarget, oldSweep, fraction, normal, nullptr, doorStates)) return false;
		}
	}
	hitNormal = newFraction < oldFraction ? newNormal : oldNormal;
	const Triangle* hitTriangle = newFraction < oldFraction ? newTriangle : oldTriangle;
	if(diagnostic && hitTriangle) *diagnostic = DescribeTriangle(QStringLiteral("sweep"), *hitTriangle)
		+ QStringLiteral(" oldFraction=%1 newFraction=%2 replay=%3").arg(oldFraction).arg(newFraction).arg(allowSlideReplay);
	hitLoc = from + (to - from) * std::max(0.0, std::min(oldFraction, newFraction));
	return true;
}

AuPoint CollisionWorld::ResolveDestination(const AuPoint& from, const AuPoint& to, const AuBox& bodyOffsets,
	const AuPoint& gravityUp, double standingHeight, bool allowGroundSnap, AuPoint& hitNormal,
	bool preserveGroundHeight, const AuDoorStateHash* doorStates, bool allowBoardingStep,
	QVector<MovementContact>* contacts, bool allowStairExit) const
{
	hitNormal = AuPoint(0, 0, 0);
	if(contacts) contacts->clear();
	const Body body(bodyOffsets);
	if(!IsValid() || !body.valid || !Finite(from) || !Finite(to) || !Finite(gravityUp)) return from;
	AuPoint up = gravityUp; up.Unit();
	if(up.IsZero()) return from;
	AuPoint target = to;
	if(allowStairExit && allowGroundSnap && std::isfinite(standingHeight) && standingHeight > 0
		&& m_Impl->StairExit(from, to, body, up, standingHeight, target, doorStates)) return target;
	if((preserveGroundHeight || allowBoardingStep) && std::isfinite(standingHeight) && standingHeight > 0)
	{
		double floor; AuPoint normal;
		if(m_Impl->BoardingFloor(from, to, body, up, floor, normal, doorStates))
		{
			const double adjustment = standingHeight + GroundTolerance - floor;
			if(adjustment > 0 && adjustment <= MaximumBoardingStep)
			{
				// Step up before translating into the sill. Raising diagonally
				// can hit the deck edge before the feet have enough clearance.
				const AuPoint candidate = to + up * adjustment;
				const double rise = std::max(0.0, (candidate - from).Dot(up));
				const AuPoint raised = from + up * rise;
				double fraction; AuPoint obstructionNormal;
				if(rise <= MaximumBoardingStep && !m_Impl->Sweep(from, raised, body, fraction, obstructionNormal, nullptr, doorStates))
				{
					target = candidate;
					const AuPoint accepted = m_Impl->Slide(raised, target, body, up, hitNormal, true, doorStates, contacts);
					if(hitNormal.IsZero())
					{
						hitNormal = normal;
						if(contacts) contacts->append({accepted, normal});
					}
					return accepted;
				}
			}
		}
	}
	if(allowGroundSnap && std::isfinite(standingHeight) && standingHeight > 0)
	{
		AuPoint oldNormal, newNormal; bool oldStair, newStair;
		const double oldFloor = m_Impl->Floor(from, up, oldNormal, oldStair, nullptr, doorStates);
		const AuPoint movement = to - from;
		const double horizontal = (movement - up * movement.Dot(up)).Length();
		// Probe from above the proposed origin, so the next ramp is visible even
		// if a long native tick placed the feet slightly below its surface.
		const double probeLift = horizontal * 2 + .75;
		const double newFloor = m_Impl->Floor(to + up * probeLift, up, newNormal, newStair, nullptr, doorStates) - probeLift;
		const double adjustment = standingHeight + GroundTolerance - newFloor;
		if(std::abs(oldFloor - standingHeight) <= .25 && (oldStair || newStair)
			&& newNormal.Dot(up) >= .55 && adjustment >= -.75 && adjustment <= probeLift)
			target += up * adjustment;
	}
	return m_Impl->Slide(from, target, body, up, hitNormal, preserveGroundHeight, doorStates, contacts);
}

double CollisionWorld::HeightAboveFloor(const AuPoint& loc, const AuPoint& gravityUp, AuPoint& floorNormal,
	const AuDoorStateHash* doorStates) const
{
	floorNormal = AuPoint(0, 0, 0);
	if(!IsValid() || !Finite(loc) || !Finite(gravityUp)) return MaximumFall;
	AuPoint up = gravityUp; up.Unit();
	if(up.IsZero()) return MaximumFall;
	bool stair;
	return m_Impl->Floor(loc, up, floorNormal, stair, nullptr, doorStates);
}

bool CollisionWorld::IsOnBoardingStep(const AuPoint& loc, const AuPoint& gravityUp, double standingHeight,
	const AuDoorStateHash* doorStates) const
{
	if(!IsValid() || !Finite(loc) || !Finite(gravityUp) || !std::isfinite(standingHeight)) return false;
	AuPoint up = gravityUp; up.Unit();
	if(up.IsZero()) return false;
	AuPoint normal; bool stair, boardingStep;
	const double floor = m_Impl->Floor(loc, up, normal, stair, &boardingStep, doorStates);
	return boardingStep && normal.Dot(up) >= .55 && std::abs(floor - standingHeight) <= .25;
}

double CollisionWorld::HeightOfCeiling(const AuPoint& loc, const AuPoint& gravityUp,
	const AuDoorStateHash* doorStates) const
{
	if(!IsValid() || !Finite(loc) || !Finite(gravityUp)) return MaximumFall;
	AuPoint up = gravityUp; up.Unit();
	if(up.IsZero()) return MaximumFall;
	double distance = MaximumFall; AuPoint normal; bool stair = false;
	m_Impl->RayNode(0, loc, up, distance, normal, stair, nullptr, doorStates);
	return distance;
}

bool CollisionWorld::AboveOpenHatch(const AuPoint& loc, const AuDoorStateHash* doorStates) const
{
	if(!IsValid() || !Finite(loc)) return false;
	const AuPoint authorLoc = loc + m_Impl->authorOffset;
	const double x = std::round(authorLoc.x / AuLegacyShip::CoordinateSpacing);
	const double y = std::round(authorLoc.y / AuLegacyShip::CoordinateSpacing);
	if(x < -120 || x > 120 || y < -120 || y > 120) return false;
	const int deck = int(std::floor((authorLoc.z + .05) / AuLegacyShip::DeckHeight));
	if(deck < -127 || deck > 127) return false;
	const quint16 packed = quint16(((int(x) & 0xff) << 8) | (int(y) & 0xff));
	const auto hatch = m_Impl->hatchCells.constFind((quint32(quint8(deck)) << 16) | packed);
	if(hatch == m_Impl->hatchCells.constEnd()) return false;
	return !m_Impl->statefulDoors || (hatch.value() && doorStates && doorStates->IsDoorOpen(hatch.value()));
}
}
