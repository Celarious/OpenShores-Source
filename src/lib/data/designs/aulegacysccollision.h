#ifndef AuLegacyScCollision_H
#define AuLegacyScCollision_H

#include "audata.h"
#include "aupoint.h"
#include <QScopedPointer>
#include <QVector>
#include <functional>

class AuBox;
class AuDoorStateHash;
class AuScDesign;
class QString;
namespace AuLegacyShip { namespace Rendering { struct Mesh; } }

namespace AuLegacyScGeometry
{
struct HatchLadder { AuPoint anchor, facing; };
// Shared Legacy ship/building reach, facing and attachment-height rules.
AUDATA_DLL bool FindLadderToGrab(const QVector<HatchLadder>& ladders, const AuPoint& loc, double deckHeight,
	AuMatrix4x4& matrix, const std::function<bool(const AuPoint&, const AuPoint&)>& blocked,
	const AuPoint* routeTarget = nullptr, QString* diagnostic = nullptr);

// Only newly generated legacy blueprints opt in to shared movement collision.
AUDATA_DLL bool IsCollisionDesign(const AuScDesign& design);

// A gravity-aligned movement core derived from anatomy and stance. Camera
// rotation and limb reach do not change its clearance around the atom origin.
AUDATA_DLL AuBox MovementBodyOffsets(const AuBox& anatomy, const AuPoint& gravityUp);

// Immutable index of the recovered structural, door and obstacle-cell triangles.
// Both scene client and server consume the same serialized blueprint geometry.
class AUDATA_DLL CollisionWorld
{
public:
	struct MovementContact
	{
		AuPoint location;
		AuPoint normal;
	};

	explicit CollisionWorld(const AuScDesign& design);
	// Original building geometry uses the same swept-body/contact solver;
	// importing it here leaves ship blueprints and rendering unchanged.
	explicit CollisionWorld(const AuLegacyShip::Rendering::Mesh& mesh);
	~CollisionWorld();
	bool IsValid() const;
	int TriangleCount() const;
	// Include exterior landings in the peer collision broad phase.
	void ExpandExtents(AuPoint& minimum, AuPoint& maximum) const;

	// Body corners are offsets from the moving atom origin in ship coordinates.
	// Use the stable movement core, including growth and gravity alignment.
	bool IsBodyPathBlocked(const AuPoint& from, const AuPoint& to, const AuBox& bodyOffsets,
		AuPoint& hitLoc, AuPoint& hitNormal, const AuDoorStateHash* doorStates = nullptr) const;
	// Native door part IDs at the first obstruction along this attempted movement.
	QVector<quint16> BlockedDoors(const AuPoint& from, const AuPoint& to, const AuBox& bodyOffsets,
		const AuDoorStateHash* doorStates = nullptr) const;
	// Packet validation compares the previous authorized body and the proposed
	// body, retaining existing contact while rejecting deeper overlap.
	// Slide replay is limited to short updates in the same parent frame.
	// Doorway steps also validate an upward leg followed by a forward leg.
	bool IsBodyTransitionBlocked(const AuPoint& from, const AuPoint& to, const AuBox& oldOffsets,
		const AuBox& newOffsets, AuPoint& hitLoc, AuPoint& hitNormal, bool allowSlideReplay = false,
		QString* diagnostic = nullptr, const AuDoorStateHash* doorStates = nullptr) const;
	// Native movement supplies ground state and the creature's CG-to-feet height.
	// Ground adjustment is limited to nearby recovered ramps, then every segment
	// is swept against the same collision geometry used for server validation.
	// Exterior walking can step up at most 3/4 ft onto a hull-door landing.
	// Ground vehicles opt into that checked step independently of preserving
	// terrain height, so their exit slide can still cancel downward contact.
	// Optional contacts retain every slide obstruction in ship coordinates,
	// so velocity response does not lose an earlier wall to a later floor hit.
	// Buildings opt into a swept stair-to-floor exit with up to 1.5 ft of lift.
	AuPoint ResolveDestination(const AuPoint& from, const AuPoint& to, const AuBox& bodyOffsets,
		const AuPoint& gravityUp, double standingHeight, bool allowGroundSnap, AuPoint& hitNormal,
		bool preserveGroundHeight = false, const AuDoorStateHash* doorStates = nullptr,
		bool allowBoardingStep = false, QVector<MovementContact>* contacts = nullptr,
		bool allowStairExit = false) const;

	// Distances from the atom origin, matching the native capsule query contract.
	// A hole has no implicit room-void floor or ceiling.
	double HeightAboveFloor(const AuPoint& loc, const AuPoint& gravityUp, AuPoint& floorNormal,
		const AuDoorStateHash* doorStates = nullptr) const;
	double HeightOfCeiling(const AuPoint& loc, const AuPoint& gravityUp,
		const AuDoorStateHash* doorStates = nullptr) const;
	bool AboveOpenHatch(const AuPoint& loc, const AuDoorStateHash* doorStates = nullptr) const;
	// An exterior landing can support walking before the actor joins the ship.
	bool IsOnBoardingStep(const AuPoint& loc, const AuPoint& gravityUp, double standingHeight,
		const AuDoorStateHash* doorStates = nullptr) const;

private:
	Q_DISABLE_COPY(CollisionWorld)
	class Impl;
	QScopedPointer<Impl> m_Impl;
};
}

#endif
