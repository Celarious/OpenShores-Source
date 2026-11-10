#ifndef AuLegacyShipMesh_H
#define AuLegacyShipMesh_H

#include "aulegacyship.h"
#include <QStringList>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

namespace AuLegacyShip
{
namespace Rendering
{
// Original DaSpaceShip::SHIP_TEXTURE numbers. UVs address an individual atlas
// region, rather than the whole image, so a renderer can repeat cropped tiles.
enum class Material
{
	Untextured = -1,
	Berth = 0,
	BerthBottom = 1,
	LargeInteriorDoor = 2,
	LargeCrewDoor = 5,
	LargeHullDoor = 8,
	CrewDoor = 11,
	CrewHatch = 14,
	GravityDrive = 17,
	HullBottom = 18,
	HullDoor = 19,
	HullHatch = 22,
	HullTop = 25,
	HullTopBottomWindow = 26,
	HullWall = 27,
	HullWallWindow = 28,
	InteriorCeiling = 29,
	InteriorDoor = 30,
	InteriorFloor = 33,
	InteriorHatch = 34,
	InteriorWallEndCap = 37,
	InteriorWallFinished = 38,
	InteriorWallFill = 39,
	InteriorWallCargo = 40,
	InteriorWallCargoFill = 41,
	InteriorWallEquipment = 42,
	InteriorWallEquipmentFill = 43,
	Ladder = 44,
	LadderPole = 45,
	LifeSupport = 46,
	PowerPlant = 47,
	Railing = 48,
	RailingPost = 49,
	Seat = 50,
	Sensor = 51,
	SensorTop = 52,
	Shield = 53,
	Surgery = 54,
	TransporterPad = 55,
	Turret = 56,
	WeaponBay = 57,
	FTLDrive = 58,
	StairCasing = 59,
	StairRiser = 60,
	StairTread = 61
};

struct Vertex
{
	QVector3D position;
	QVector3D normal;
	QVector2D uv;
};

struct Batch
{
	Material material = Material::HullWall;
	QVector<Vertex> vertices;
	int deck = 0;
	bool interior = false;
	bool equipment = false;
	// Only the animated door/hatch panel carries its persistent detail ID.
	// Both sides share the ID; surrounding hull/frame geometry remains static.
	quint32 doorId = 0;
	// Occupied consoles use the native renderer instead of these static batches.
	quint32 consoleId = 0;
	// Only the gun model, not the fixed dome, follows a turret operator.
	quint32 turretId = 0;
	// Sight ownership is independent of collision and console animation.
	quint32 detailId = 0;
	// Procedural equipment shares draw batches; one owner per triangle.
	QVector<quint32> triangleDetails;
	QString textureFile;
	QVector4D tint = QVector4D(1, 1, 1, 1);
	float selfIllumination = 0;
	bool twoSided = false;
	QVector3D rotationPivot;
	QVector3D turretMuzzle;
	QVector3D rotationAxis = QVector3D(0, 0, 1);
	float degreesPerSecond = 0;
	QVector2D uvVelocity;
	int effectFirstFrame = -1;
	int effectFrameCount = 0;
	float effectFrameSeconds = 0.1f;
};

struct HullOpening
{
	HullOpening() = default;
	HullOpening(const QPointF& first, const QPointF& second, int floorDeck, int type, quint32 id)
		: a(first), b(second), deck(floorDeck), detailType(type), detailId(id) {}

	QPointF a;
	QPointF b;
	int deck = 0;
	int detailType = 0;
	quint32 detailId = 0;
};

struct HatchOpening
{
	HatchOpening() = default;
	HatchOpening(const QPolygonF& shape, int floorDeck, quint32 id)
		: boundary(shape), deck(floorDeck), detailId(id) {}

	QPolygonF boundary;
	int deck = 0;
	quint32 detailId = 0;
};

struct HullWindowBoundary
{
	HullWindowBoundary() = default;
	HullWindowBoundary(const QPointF& first, const QPointF& second, int floorDeck)
		: a(first), b(second), deck(floorDeck) {}

	QPointF a;
	QPointF b;
	int deck = 0;
};

struct Mesh
{
	QVector<Batch> batches;
	// Actual rendered wall apertures, including permanent hull openings which
	// have no panel batch. Their endpoints retain the surrounding hull jambs.
	QVector<HullOpening> hullOpenings;
	QVector<HatchOpening> hatchOpenings;
	// Movement boundaries at the inner frame of sloped hull windows. These
	// are separate from the visible glass and its recovered wall profile.
	QVector<HullWindowBoundary> hullWindowBoundaries;
	QVector3D minimum;
	QVector3D maximum;
	QStringList warnings;
};

struct WireframeVertex
{
	QVector3D position;
	QVector4D color;
};

struct WireframeSpriteVertex
{
	QVector3D position;
	QVector2D uv;
};

struct WireframeLabel
{
	WireframeLabel() = default;
	WireframeLabel(const QVector3D& point, const QString& label, const QVector4D& tint, bool center)
		: position(point), text(label), color(tint), centered(center) {}

	QVector3D position;
	QString text;
	QVector4D color;
	bool centered = false;
};

struct Wireframe
{
	QVector<WireframeVertex> vertices;
	QVector<WireframeVertex> triangles;
	QVector<WireframeVertex> gridVertices;
	QVector<WireframeSpriteVertex> sprites;
	QVector<WireframeLabel> labels;
};

// Canonical 2016 room type text shared by designer labels and live location text.
QString RoomTypeName(quint8 type);

// The line drawing uses room boundaries and vertical corners, independently
// of the tessellation/materials in the solid model. Coordinates remain feet.
// Tactical mode follows the 2016 in-world current-deck filtering and shading.
Wireframe BuildWireframe(const Design& design, int activeDeck, bool gridVisible, quint8 gridMask, bool tactical = false);

// Positions retain the author's feet coordinates: x/y plan, z up. Triangle
// winding and normals face the visible side of each exterior/interior surface.
Mesh BuildMesh(const Design& design);
}
}

#endif
