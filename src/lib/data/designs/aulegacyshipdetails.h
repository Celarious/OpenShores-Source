#ifndef AuLegacyShipDetails_H
#define AuLegacyShipDetails_H

#include "aulegacyship.h"

namespace AuLegacyShip
{
struct DetailEditResult
{
	bool accepted = false;
	bool changed = false;
	QString error;
	quint32 detailId = 0;
	int added = 0;
	int removed = 0;
};

// Names and placement rules from the original 143-entry equipment table.
QString DetailName(int type);
QString DetailPlacementDescription(int type);
int CanonicalDetailType(int type);
int DetailRotationStep(int type);
QRectF BerthFootprint(int type);
int RotateDetailType(int type, int steps = 1);

// One original equipment brush event. Positions snap to the half-cell grid.
// The center must succeed before the brush expands. Rejected candidates leave
// the working design unchanged, including equipment they would replace.
DetailEditResult PlaceDetail(Design& design, int deck, const QPointF& cursor,
	int type, quint16 commodity = 0, int brushRadius = 0);
DetailEditResult DeleteDetails(Design& design, int deck, const QPointF& cursor,
	int brushRadius = 0);

// Replays the 2016 ordered equipment-validation pass after a room edit.
// Invalid equipment is removed; valid equipment can change type/room ID.
// An unavailable dependency returns false without changing the detail array.
bool ReconcileDetails(Design& candidate, QString& error);
}

#endif // AuLegacyShipDetails_H
