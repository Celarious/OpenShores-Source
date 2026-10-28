#ifndef AuLegacyBuildingGeometry_H
#define AuLegacyBuildingGeometry_H

#include "aulegacybd.h"
#include "aulegacyshipmesh.h"
#include <QRect>
#include <QSet>
#include <functional>

namespace AuLegacyCity { struct Development; }

namespace AuLegacyBd
{
namespace Geometry
{
struct Options
{
	int minimumDeck = 0;
	int maximumDeck = 0;
	quint16 materialCommodity = 76; // Original CID_Stone, not modern material IDs.
	bool constructed = true;
	bool exterior = true;
	bool interior = true;
};

// BuildingTexture indices are independent of DaSpaceShip::SHIP_TEXTURE.
struct Batch
{
	QVector<AuLegacyShip::Rendering::Vertex> vertices;
	int textureSlot = 0;
	int deck = 0;
	bool interior = false;
	bool twoSided = false;
	bool equipment = false;
	bool stairs = false;
	bool collisionOnly = false;
	bool depthOnly = false; // Original AuBuildingTri window effect 4: terrain mask.
	bool empireFlag = false;
	// Identifies an always-open door/hatch frame. Render its alpha-cutout art,
	// but omit the full frame quad from collision and sight obstruction.
	quint32 doorId = 0;
	quint32 detailId = 0;
	QString textureFile;
	QVector4D tint = QVector4D(1, 1, 1, 1);
	float selfIllumination = 0;
};

struct FloorState
{
	int deck = 0;
	quint16 roomId = 0;
	quint8 roomType = 0;
	QPolygonF boundary;
	// Original room geometry is clipped by constructed deck limits, without
	// mutating the authored room or its cross-deck detail references.
	bool floor = false;
	bool ceiling = false;
	bool roof = false;
};

struct ExteriorLight
{
	QVector3D position;
	bool runway = false;
	ExteriorLight() = default;
	ExteriorLight(const QVector3D& location, bool isRunway) : position(location), runway(isRunway) {}
};

struct Mesh
{
	QVector<Batch> batches;
	QVector<FloorState> floors;
	QVector<ExteriorLight> exteriorLights;
	QVector<AuLegacyShip::Rendering::HullOpening> wallOpenings;
	QVector<AuLegacyShip::Rendering::HatchOpening> floorOpenings;
	QVector3D minimum;
	QVector3D maximum;
	QStringList warnings;
	int TriangleCount() const;
};

// Positions are feet in original local z-up coordinates. Building atlas UVs
// address one tile in [0,1]; the renderer maps them inside TextureInset margins.
// Resource-model textureFile UVs remain in their original independent domain.
// CPU triangles use CCW order for shared collision; scene upload reverses their
// vertex order to the original 2016 CW rendering convention, preserving normals.
AUDATA_DLL Mesh Build(const Design& design, const Options& options);
struct RoadFixtureGround
{
	double altitude, foundationAltitude; // Feet relative to the road site's MSL.
	RoadFixtureGround(double height, double foundation) : altitude(height), foundationAltitude(foundation) {}
};
// Sample the fixture height and the bottom of its buried support. Terrain
// triangles can cut below the point sample near the edge of a leveled site.
// Without a sampler, previews retain the stored road grade.
using RoadFixtureHeight = std::function<RoadFixtureGround(const QVector3D&)>;
AUDATA_DLL Mesh BuildProcedural(const AuLegacyCity::Development& development,
	const RoadFixtureHeight& roadFixtureHeight = {});
AUDATA_DLL QString MaterialAtlas(quint16 originalCommodity);
// Original 18-entry terrain pavement palette. These slots are
// used only in Legacy universes; they are not modern WORLDTEX material IDs.
AUDATA_DLL int SiteTextureSlot(quint16 originalCommodity, bool constructed);
AUDATA_DLL QString SiteTextureFile(int slot);
// BuildingTexture uses bottom-origin OpenGL coordinates; this rect converts
// them to the top-origin original PNG. AuGLTexture flips the crop on upload.
AUDATA_DLL QRect TextureTile(int originalBuildingTextureSlot);
// Half of one original logical pixel is one physical PNG pixel. Apply this
// affine inset to atlas batch UVs after cropping; slot16 also mirrors x.
AUDATA_DLL QVector2D TextureInset(int originalBuildingTextureSlot);
AUDATA_DLL AuLegacyShip::Design AuthoringDesign(const Design& design, const Options& options);

// Exact original derived streams are immutable parity fixtures. They are never
// substituted for generated runtime geometry or included in public defaults.
struct FixtureTriangle
{
	double coordinates[24] = {};
	quint8 flag = 0;
};
AUDATA_DLL bool ReadGeometryFixture(const QByteArray& source,
	QVector<FixtureTriangle>& triangles, QString& error);
}
}

#endif
