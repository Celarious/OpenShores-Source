#ifndef AuLegacyShipEdit_H
#define AuLegacyShipEdit_H

#include "aulegacyship.h"
#include <QMap>

namespace AuLegacyShip
{
bool SetBoundary(Room& room, const QPolygonF& boundary, QString& error);
struct ModuleChanges
{
	QMap<quint16, quint16> scalars, bays, transporters;
};

ModuleChanges CurrentModules(const Design& design);
// Sparse changes keep other participants' independent module choices intact.
bool ApplyModuleChanges(Design& design, const ModuleChanges& modules, bool& changed, QString& error);

struct CellEditResult
{
	// Erasing unoccupied space can succeed without changing any records.
	bool accepted = false;
	bool changed = false;
	QString error;
};

// Apply one original mouse-event position to an offline working design.
// roomType -1 erases cells; 1..21 except 12 are spacecraft room-paint tools.
// Building paint tools follow the selected industry. Roofs have three controls.
// Rejection, including unavailable validation data, leaves design unchanged.
// The input file is never written. Room Number is the saved subtype byte.
CellEditResult EditCell(Design& design, int deck, const QPointF& cursor,
	quint8 gridMask, int roomType, quint8 roomNumber);
int DesignMinimumDeck(const Design& design);
int DesignMaximumDeck(const Design& design);
CellEditResult CreateRoof(Design& design, int deck, int type, const QPointF& first,
	const QPointF& axis, const QPointF& edge);

// DnRoomArray::InsideRoom: first room containing the point, excluding walls.
// Returns -1 when no room is selected or the geometry cannot be read.
int RoomAtPoint(const Design& design, int deck, const QPointF& cursor,
	QString* error = nullptr);

// Whole-room/deck erasure revalidates equipment before committing the edit.
CellEditResult DeleteRoomAt(Design& design, int deck, const QPointF& cursor);
CellEditResult ChangeRoomTypeAt(Design& design, int deck, const QPointF& cursor, int roomType);
CellEditResult DeleteDeck(Design& design, int deck);

enum class MoveCopyMode { MoveEntireDesign, MoveCurrentDeck, CopyCurrentDeck };

struct MoveCopyRequest
{
	MoveCopyMode mode = MoveCopyMode::MoveEntireDesign;
	int deck = 0;
	int lateral = 0;
	int longitudinal = 0;
	int vertical = 0;
	int copies = 1;
};

struct MoveCopyResult : CellEditResult
{
	int copiesMade = 0;
};

// The dialog validates the first displacement. Repeats preserve the recovered
// copy quirks; a packed-coordinate or record-limit stop retains complete copies.
QString MoveCopyError(const Design& design, const MoveCopyRequest& request);
MoveCopyResult MoveCopy(Design& design, const MoveCopyRequest& request);
}

#endif // AuLegacyShipEdit_H
