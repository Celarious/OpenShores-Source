#include "aulegacyshipedit.h"
#include "aulegacybd.h"
#include "aulegacyshipdetails.h"
#include "aulegacyshipgeometry.h"
#include "aupoint.h"
#include "aulegacyshipsave.h"

#include <QSet>
#include <cmath>
#include <utility>

namespace
{
using namespace AuLegacyShip;

bool HasBoundary(const Room& room)
{
	return room.hasWorkingBoundary ? !room.workingBoundary.isEmpty() : !room.packedVertices.isEmpty();
}

bool IsRoof(const Room& room)
{
	return room.roomType >= 0x16 && room.roomType <= 0x18;
}

bool SameRoomKey(const Room& room, int deck, int type, quint8 number)
{
	return int(room.deck) == deck && int(room.roomType) == type && room.roomSubtype == number;
}

void InsertRoom(QVector<Room>& rooms, Room&& room);

quint16 NextRoomId(const Design& design)
{
	QSet<quint16> used;
	for(const Room& room : design.rooms)
	{
		if(HasBoundary(room) && room.roomId != 0)
			used.insert(room.roomId);
	}
	for(unsigned int id = 1; id <= 0xffff; ++id)
	{
		if(!used.contains(quint16(id)))
			return quint16(id);
	}
	// SpaceShipDesignNextRoomId wraps to zero when every nonzero ID is used.
	return 0;
}

bool SetBoundary(Room& room, const QPolygonF& boundary, QString& error)
{
	QVector<quint16> packed;
	packed.reserve(boundary.size());
	for(const QPointF& point : boundary)
	{
		const double x = std::round(point.x() / CoordinateSpacing);
		const double y = std::round(point.y() / CoordinateSpacing);
		if(!std::isfinite(x) || !std::isfinite(y) || x < -128 || x > 127 || y < -128 || y > 127)
		{
			error = QStringLiteral("The resulting boundary exceeds the legacy coordinate range.");
			return false;
		}
		packed.append(quint16(((int(x) & 0xff) << 8) | (int(y) & 0xff)));
	}
	room.hasWorkingBoundary = true;
	room.workingBoundary = boundary;
	room.packedVertices = std::move(packed);
	return true;
}

bool SameDetails(const QVector<Detail>& left, const QVector<Detail>& right)
{
	if(left.size() != right.size())
		return false;
	for(int i = 0; i < left.size(); ++i)
	{
		const Detail& a = left[i];
		const Detail& b = right[i];
		if(a.deck != b.deck || a.detailType != b.detailType || a.hiddenRaw != b.hiddenRaw
			|| a.detailId != b.detailId || a.designId != b.designId || a.packedXY != b.packedXY
			|| a.roomId != b.roomId || a.commodityIdRaw != b.commodityIdRaw
			|| a.commodityIdNormalized != b.commodityIdNormalized)
			return false;
	}
	return true;
}
}

int AuLegacyShip::DesignMinimumDeck(const Design& design)
{
	return design.building ? AuLegacyBd::MinimumDesignDeck(design.buildingIndustry) : MinimumDeck;
}

int AuLegacyShip::DesignMaximumDeck(const Design& design)
{
	return design.building ? AuLegacyBd::DesignerIndustry(design.buildingIndustry).maximumDeck : MaximumDeck;
}

AuLegacyShip::CellEditResult AuLegacyShip::EditCell(Design& design, int deck,
	const QPointF& cursor, quint8 gridMask, int roomType, quint8 roomNumber)
{
	CellEditResult result;
	// The spacecraft palette exposes 1..21 except 12. Roof creation is a
	// separate building-only command (DaDesignEnvironment::CreateRoof).
	if(roomType != -1 && (roomType < 1 || roomType > 21 || (design.building
		? !AuLegacyBd::BuildingRoomAllowed(design.buildingIndustry, roomType) : roomType == 12)))
	{
		result.error = QStringLiteral("Select an available room type or the Erase Cells tool.");
		return result;
	}
	const bool erase = roomType == -1;
	if(deck < -128 || deck > 127 || (!erase && (deck < DesignMinimumDeck(design) || deck > DesignMaximumDeck(design))))
	{
		result.error = QStringLiteral("The selected deck is outside this design's deck limits.");
		return result;
	}
	if(!std::isfinite(cursor.x()) || !std::isfinite(cursor.y()))
	{
		result.error = QStringLiteral("The cell position is not finite.");
		return result;
	}
	const QPolygonF cell = Geometry::FillShape(gridMask, cursor);
	if(cell.size() < 3)
	{
		result.error = QStringLiteral("Enable at least two grid directions to form a cell.");
		return result;
	}
	for(const QPointF& point : cell)
	{
		const double x = std::round(point.x() / CoordinateSpacing);
		const double y = std::round(point.y() / CoordinateSpacing);
		if(!std::isfinite(x) || !std::isfinite(y) || x < -120 || x > 120 || y < -120 || y > 120
			|| (design.building && !AuLegacyBd::ValidDesignerPoint(design.buildingIndustry, deck, point)))
		{
			result.error = QStringLiteral("This cell extends beyond the 2016 design grid.");
			return result;
		}
	}

	QPolygonF proposed = cell;
	bool existingRoom = false;
	if(!erase)
	{
		for(const Room& room : design.rooms)
		{
			if(!HasBoundary(room) || !SameRoomKey(room, deck, roomType, roomNumber))
				continue;
			QPolygonF boundary = RoomBoundary(room, &result.error);
			if(!result.error.isEmpty())
				return result;
			if(boundary.size() >= 3)
			{
				if(!Geometry::Merge(boundary, cell, &result.error))
					return result;
				proposed = std::move(boundary);
				existingRoom = true;
			}
			break;
		}
	}

	// Original add subtracts the whole proposed room, not just the cursor cell.
	// Empty file records remain inspectable but never enter the original room
	// array, so they cannot block edits or reserve working room IDs here.
	const auto affected = [&](const Room& room)
	{
		return HasBoundary(room) && int(room.deck) == deck
			&& (!design.building || !IsRoof(room))
			&& (erase || (!IsRoof(room) && !SameRoomKey(room, deck, roomType, roomNumber)));
	};
	for(const Room& room : design.rooms)
	{
		if(!affected(room))
			continue;
		const QPolygonF boundary = RoomBoundary(room, &result.error);
		if(!result.error.isEmpty())
			return result;
		if(!Geometry::CanSubtract(boundary, proposed, &result.error))
		{
			if(result.error.isEmpty())
				result.error = QStringLiteral("This edit would leave an invalid boundary in room %1.").arg(room.roomId);
			return result;
		}
	}

	// Geometry and equipment changes are committed together. A failed preflight
	// or unavailable validation dependency cannot leave a partly repainted ship.
	Design candidate = design;
	bool changed = false;
	for(int i = 0; i < candidate.rooms.size();)
	{
		Room& room = candidate.rooms[i];
		if(!affected(room))
		{
			++i;
			continue;
		}
		QPolygonF boundary = RoomBoundary(room, &result.error);
		if(!result.error.isEmpty())
			return result;
		if(Geometry::Subtract(boundary, proposed, &result.error))
		{
			changed = true;
			if(boundary.size() < 3)
			{
				candidate.rooms.removeAt(i);
				continue;
			}
			if(!SetBoundary(room, boundary, result.error))
				return result;
		}
		else if(!result.error.isEmpty())
			return result;
		++i;
	}
	if(!erase)
	{
		if(existingRoom)
		{
			for(Room& room : candidate.rooms)
			{
				if(HasBoundary(room) && SameRoomKey(room, deck, roomType, roomNumber))
				{
					if(!SetBoundary(room, proposed, result.error))
						return result;
					break;
				}
			}
		}
		else
		{
			Room room;
			room.streamOffset = -1;
			room.designId = candidate.header.designId;
			room.roomId = NextRoomId(candidate);
			room.deck = qint8(deck);
			room.roomType = quint8(roomType);
			room.roomSubtype = roomNumber;
			if(!SetBoundary(room, proposed, result.error))
				return result;
			if(design.building) InsertRoom(candidate.rooms, std::move(room));
			else candidate.rooms.append(std::move(room));
		}
		changed = true;
	}

	// The legacy erase path verifies details even when no room area was removed.
	if(!ReconcileDetails(candidate, result.error))
		return result;
	result.accepted = true;
	result.changed = changed || !SameDetails(design.details, candidate.details);
	if(result.changed)
		design = std::move(candidate);
	return result;
}

int AuLegacyShip::RoomAtPoint(const Design& design, int deck, const QPointF& cursor,
	QString* error)
{
	if(error) error->clear();
	if(deck < -128 || deck > 127 || !std::isfinite(cursor.x()) || !std::isfinite(cursor.y()))
	{
		if(error) *error = QStringLiteral("The room position is outside the legacy coordinate range.");
		return -1;
	}
	const AuPoint location(cursor.x(), cursor.y(), deck * AuLegacyShip::DeckHeight);
	for(int i = 0; i < design.rooms.size(); ++i)
	{
		const Room& room = design.rooms[i];
		if(int(room.deck) != deck || !HasBoundary(room)) continue;
		QString geometryError;
		const QPolygonF boundary = RoomBoundary(room, &geometryError);
		if(!geometryError.isEmpty())
		{
			if(error) *error = geometryError;
			return -1;
		}
		AuPointArray polygon;
		for(const QPointF& vertex : boundary)
			polygon.append(AuPoint(vertex.x(), vertex.y(), deck * AuLegacyShip::DeckHeight));
		if(polygon.PointInBoundary(location)) return i;
	}
	return -1;
}

AuLegacyShip::CellEditResult AuLegacyShip::DeleteRoomAt(Design& design, int deck,
	const QPointF& cursor)
{
	CellEditResult result;
	// DaSpaceShip::DesignDeleteRoom / DgData::SpaceShipDesignDeleteRoom,
	// AuAtom13.dll 0x1803e3d80 / AuData13.dll 0x180145a30.
	const int index = RoomAtPoint(design, deck, cursor, &result.error);
	if(!result.error.isEmpty()) return result;
	result.accepted = true;
	if(index < 0 || !design.rooms[index].roomId) return result;
	Design candidate = design;
	candidate.rooms.removeAt(index);
	if(!ReconcileDetails(candidate, result.error))
	{
		result.accepted = false;
		return result;
	}
	design = std::move(candidate);
	result.changed = true;
	return result;
}

AuLegacyShip::CellEditResult AuLegacyShip::DeleteDeck(Design& design, int deck)
{
	CellEditResult result;
	if(deck < -128 || deck > 127)
	{
		result.error = QStringLiteral("The deck is outside the legacy deck range.");
		return result;
	}
	// DgData::SpaceShipDesignDeleteDeck, AuData13.dll 0x180145140:
	// remove details and rooms on this deck, then verify the remaining details.
	Design candidate = design;
	for(int i = candidate.details.size() - 1; i >= 0; --i)
		if(int(candidate.details[i].deck) == deck) candidate.details.removeAt(i);
	for(int i = candidate.rooms.size() - 1; i >= 0; --i)
		if(int(candidate.rooms[i].deck) == deck) candidate.rooms.removeAt(i);
	if(candidate.rooms.size() == design.rooms.size() && candidate.details.size() == design.details.size())
	{
		result.accepted = true;
		return result;
	}
	if(!ReconcileDetails(candidate, result.error)) return result;
	design = std::move(candidate);
	result.accepted = result.changed = true;
	return result;
}

namespace
{
constexpr int MoveCopyDeckLimit = 42;
constexpr double MoveCopyCoordinateLimit = 339.41125494;

bool TranslateRoom(Room& room, int lateral, int longitudinal, int vertical, QString& error)
{
	if(int(room.deck) + vertical < -128 || int(room.deck) + vertical > 127)
	{
		error = QStringLiteral("A room would exceed the stored deck range.");
		return false;
	}
	if(room.hasWorkingBoundary)
	{
		QPolygonF boundary = room.workingBoundary;
		boundary.translate(lateral * GridSpacing, longitudinal * GridSpacing);
		if(!SetBoundary(room, boundary, error)) return false;
	}
	else
	{
		// Translate stored controls directly, including the three ellipse controls.
		// One whole grid square is exactly two packed coordinate steps.
		for(quint16& packed : room.packedVertices)
		{
			const int x = qint8(packed >> 8) + lateral * 2;
			const int y = qint8(packed & 255) + longitudinal * 2;
			if(x < -128 || x > 127 || y < -128 || y > 127)
			{
				error = QStringLiteral("A room would exceed the stored grid coordinate range.");
				return false;
			}
			packed = quint16(((x & 255) << 8) | (y & 255));
		}
	}
	room.deck = qint8(int(room.deck) + vertical);
	return true;
}

bool TranslateDetail(Detail& detail, int lateral, int longitudinal, int vertical, QString& error)
{
	const int x = qint8(detail.packedXY >> 8) + lateral * 2;
	const int y = qint8(detail.packedXY & 255) + longitudinal * 2;
	const int deck = int(detail.deck) + vertical;
	if(x < -128 || x > 127 || y < -128 || y > 127 || deck < -128 || deck > 127)
	{
		error = QStringLiteral("Equipment would exceed the stored coordinate range.");
		return false;
	}
	detail.packedXY = quint16(((x & 255) << 8) | (y & 255));
	detail.deck = qint8(deck);
	return true;
}

void InsertRoom(QVector<Room>& rooms, Room&& room)
{
	int at = 0;
	for(; at < rooms.size(); ++at)
	{
		const Room& other = rooms[at];
		if(room.deck < other.deck || (room.deck == other.deck
			&& (room.roomType < other.roomType || (room.roomType == other.roomType
				&& room.roomSubtype < other.roomSubtype)))) break;
	}
	rooms.insert(at, std::move(room));
}
}

AuLegacyShip::CellEditResult AuLegacyShip::ChangeRoomTypeAt(Design& design, int deck,
	const QPointF& cursor, int roomType)
{
	CellEditResult result;
	if(!design.building || roomType < 1 || roomType > 21
		|| !AuLegacyBd::BuildingRoomAllowed(design.buildingIndustry, roomType))
	{ result.error = QStringLiteral("Select an available building room type."); return result; }
	const int index = RoomAtPoint(design, deck, cursor, &result.error);
	if(!result.error.isEmpty()) return result;
	if(index < 0 || !design.rooms[index].roomId || IsRoof(design.rooms[index])
		|| design.rooms[index].roomType == roomType)
	{ result.accepted = true; return result; }
	Room replacement = design.rooms[index];
	for(const Room& other : design.rooms)
		if(SameRoomKey(other, deck, roomType, replacement.roomSubtype))
		{
			result.error = QStringLiteral("A room of that type and number already exists on this story.");
			return result;
		}
	// BuildingDesignChangeRoomType (AuData13 0x18012e4b0) assigns the
	// first free ID *after* removing the room, then reinserts in key order.
	Design candidate = design;
	candidate.rooms.removeAt(index);
	replacement.roomId = NextRoomId(candidate);
	replacement.roomType = quint8(roomType);
	InsertRoom(candidate.rooms, std::move(replacement));
	if(!ReconcileDetails(candidate, result.error)) return result;
	design = std::move(candidate);
	result.accepted = result.changed = true;
	return result;
}

AuLegacyShip::CellEditResult AuLegacyShip::CreateRoof(Design& design, int deck, int type,
	const QPointF& first, const QPointF& axis, const QPointF& edge)
{
	CellEditResult result;
	if(!design.building || type < 22 || type > 24 || deck < 0 || deck > 127
		|| !AuLegacyBd::BuildingRoomAllowed(design.buildingIndustry, type)
		|| !AuLegacyBd::ValidDesignerPoint(design.buildingIndustry, deck, first)
		|| !AuLegacyBd::ValidDesignerPoint(design.buildingIndustry, deck, axis)
		|| !AuLegacyBd::ValidDesignerPoint(design.buildingIndustry, deck, edge))
	{
		result.error = QStringLiteral("The roof must fit inside the building site on an above-ground deck.");
		return result;
	}
	// DaDesignEnvironment::CreateRoof (0x1802f7dd0) orients gable and hip
	// rectangles clockwise, and mirrors the second ellipse axis when needed.
	const QPointF along = axis - first, across = edge - first;
	const double cross = along.x() * across.y() - along.y() * across.x();
	QPolygonF boundary;
	if(cross != 0)
	{
		if(type == 24) boundary = EllipseBoundary(first, axis, cross < 0 ? first - across : edge);
		else if(cross < 0) boundary << first << axis << edge + along << edge;
		else boundary << axis << first << edge << edge + along;
	}
	if(boundary.isEmpty())
	{
		result.error = QStringLiteral("Choose three distinct, noncollinear roof points.");
		return result;
	}
	for(const QPointF& point : boundary)
		if(!AuLegacyBd::ValidDesignerPoint(design.buildingIndustry, deck, point))
		{
			result.error = QStringLiteral("The roof extends beyond the building site.");
			return result;
		}
	Room roof;
	roof.streamOffset = -1;
	roof.deck = qint8(deck);
	roof.roomType = quint8(type);
	roof.designId = design.header.designId;
	roof.roomId = NextRoomId(design);
	if(!roof.roomId || design.rooms.size() >= 65535)
	{
		result.error = QStringLiteral("No room identifier is available for the roof.");
		return result;
	}
	// BuildingDesignNextRoomNumber returns zero for a missing room array,
	// otherwise the first free number in 1..255, wrapping back to zero.
	if(!design.rooms.isEmpty())
	{
		QSet<quint8> used;
		for(const Room& room : design.rooms)
			if(room.deck == deck && room.roomType == type) used.insert(room.roomSubtype);
		for(int number = 1; number <= 255; ++number)
			if(!used.contains(quint8(number))) { roof.roomSubtype = quint8(number); break; }
	}
	if(!SetBoundary(roof, boundary, result.error)) return result;
	Design candidate = design;
	InsertRoom(candidate.rooms, std::move(roof));
	if(!ReconcileDetails(candidate, result.error)) return result;
	design = std::move(candidate);
	result.changed = result.accepted = true;
	return result;
}

namespace
{
QString MoveCopyGeometryError(const Design& design, const MoveCopyRequest& request)
{
	// SpaceShipDesignTestMove/TestCopy, AuData13 18014a8c0 / 18014a2c0.
	const bool entire = request.mode == MoveCopyMode::MoveEntireDesign;
	const bool differentDeck = !entire && request.vertical != 0;
	const int destination = request.deck + request.vertical;
	const QPointF offset(request.lateral * GridSpacing, request.longitudinal * GridSpacing);
	for(const Detail& detail : design.details)
	{
		if(differentDeck && int(detail.deck) == destination)
			return QStringLiteral("Equipment on destination deck blocks the operation.");
		if(!entire && int(detail.deck) != request.deck) continue;
		if((entire || differentDeck) && std::abs(int(detail.deck) + request.vertical) > MoveCopyDeckLimit)
			return QStringLiteral("Equipment would exceed the deck limit.");
		const QPointF point = DecodePoint(detail.packedXY) + offset;
		if(std::abs(point.x()) - MoveCopyCoordinateLimit > 1e-5
			|| std::abs(point.y()) - MoveCopyCoordinateLimit > 1e-5)
			return QStringLiteral("Equipment would exceed the grid coordinates limit.");
	}
	for(const Room& room : design.rooms)
	{
		if(!HasBoundary(room)) continue;
		if(differentDeck && int(room.deck) == destination)
			return QStringLiteral("A room on destination deck blocks the operation.");
		if(!entire && int(room.deck) != request.deck) continue;
		if((entire || differentDeck) && std::abs(int(room.deck) + request.vertical) > MoveCopyDeckLimit)
			return QStringLiteral("A room would exceed the deck limit.");
		QString error;
		const QPolygonF boundary = RoomBoundary(room, &error);
		if(!error.isEmpty()) return error;
		for(const QPointF& point : boundary)
			if(std::abs(point.x() + offset.x()) - MoveCopyCoordinateLimit > 1e-5
				|| std::abs(point.y() + offset.y()) - MoveCopyCoordinateLimit > 1e-5)
				return QStringLiteral("A room would exceed the grid coordinates limit.");
	}
	return QString();
}
}

QString AuLegacyShip::MoveCopyError(const Design& design, const MoveCopyRequest& request)
{
	if(request.lateral == 0 && request.longitudinal == 0 && request.vertical == 0)
		return QStringLiteral("No movement has been specified.");
	if(request.mode == MoveCopyMode::CopyCurrentDeck && request.vertical == 0)
		return QStringLiteral("Cannot copy to current deck.");
	if(request.deck < -128 || request.deck > 127 || request.lateral < -80 || request.lateral > 80
		|| request.longitudinal < -80 || request.longitudinal > 80
		|| request.vertical < -84 || request.vertical > 84 || request.copies < 1 || request.copies > 84)
		return QStringLiteral("The move/copy settings are outside the 2016 dialog ranges.");
	return MoveCopyGeometryError(design, request);
}

AuLegacyShip::MoveCopyResult AuLegacyShip::MoveCopy(Design& design, const MoveCopyRequest& request)
{
	MoveCopyResult result;
	result.error = MoveCopyError(design, request);
	if(!result.error.isEmpty()) return result;
	const bool entire = request.mode == MoveCopyMode::MoveEntireDesign;
	const bool copy = request.mode == MoveCopyMode::CopyCurrentDeck;
	Design candidate = design;
	if(!copy)
	{
		bool changed = false;
		for(Detail& detail : candidate.details)
		{
			if(!entire && int(detail.deck) != request.deck) continue;
			if(!TranslateDetail(detail, request.lateral, request.longitudinal, request.vertical, result.error)) return result;
			changed = true;
		}
		if(entire)
		{
			for(Room& room : candidate.rooms)
			{
				if(!HasBoundary(room)) continue;
				if(!TranslateRoom(room, request.lateral, request.longitudinal, request.vertical, result.error)) return result;
				changed = true;
			}
		}
		else
		{
			// The original removes and reinserts each source room with the next
			// free ID, even for a purely horizontal current-deck move.
			QVector<quint16> ids;
			for(const Room& room : candidate.rooms)
				if(HasBoundary(room) && int(room.deck) == request.deck) ids.append(room.roomId);
			for(quint16 id : ids)
				for(int i = 0; i < candidate.rooms.size(); ++i)
				{
					if(candidate.rooms[i].roomId != id || !HasBoundary(candidate.rooms[i])) continue;
					Room room = candidate.rooms.takeAt(i);
					room.roomId = NextRoomId(candidate);
					if(!TranslateRoom(room, request.lateral, request.longitudinal, request.vertical, result.error)) return result;
					InsertRoom(candidate.rooms, std::move(room));
					changed = true;
					break;
				}
		}
		if(!ReconcileDetails(candidate, result.error)) return result;
		result.changed = changed || !SameDetails(design.details, candidate.details);
	}
	else
	{
		QVector<Room> rooms;
		QVector<Detail> details;
		QSet<quint16> roomIds;
		QSet<quint32> detailIds;
		for(const Room& room : design.rooms)
			if(HasBoundary(room))
			{
				roomIds.insert(room.roomId);
				if(int(room.deck) == request.deck) rooms.append(room);
			}
		for(const Detail& detail : design.details)
		{
			detailIds.insert(detail.detailId);
			if(int(detail.deck) == request.deck) details.append(detail);
		}
		quint32 nextRoomId = 1;
		quint32 nextDetailId = 1;
		for(int repeat = 1; repeat <= request.copies; ++repeat)
		{
			// Only the first destination uses spacecraft validation in 2016.
			// The repeat loop calls BuildingDesignTestCopy (180145025), which
			// finds no building for an offline spacecraft's working design ID.
			// Keep that quirk; stop before overflowing this adapter's packed records.
			if(design.building && repeat > 1)
			{
				MoveCopyRequest destination = request;
				destination.lateral *= repeat;
				destination.longitudinal *= repeat;
				destination.vertical *= repeat;
				if(!MoveCopyGeometryError(candidate, destination).isEmpty()) break;
			}
			QVector<Room> newRooms = rooms;
			QVector<Detail> newDetails = details;
			if(qint64(candidate.rooms.size()) + candidate.details.size() + newRooms.size()
				+ newDetails.size() + candidate.header.firstPairs.size()
				+ candidate.header.secondPairs.size() > MaximumRecords)
			{
				result.error = QStringLiteral("The design exceeds the legacy record safety limit.");
				break;
			}
			for(Room& room : newRooms)
				if(!TranslateRoom(room, request.lateral * repeat, request.longitudinal * repeat,
					request.vertical * repeat, result.error)) break;
			if(!result.error.isEmpty()) break;
			for(Detail& detail : newDetails)
				if(!TranslateDetail(detail, request.lateral * repeat, request.longitudinal * repeat,
					request.vertical * repeat, result.error)) break;
			if(!result.error.isEmpty()) break;
			for(Detail& detail : newDetails)
			{
				while(nextDetailId && detailIds.contains(nextDetailId)) ++nextDetailId;
				detail.detailId = nextDetailId;
				detailIds.insert(nextDetailId);
				detail.streamOffset = -1;
				candidate.details.append(detail);
			}
			for(Room& room : newRooms)
			{
				while(nextRoomId <= 0xffff && roomIds.contains(quint16(nextRoomId))) ++nextRoomId;
				room.roomId = nextRoomId <= 0xffff ? quint16(nextRoomId) : 0;
				roomIds.insert(room.roomId);
				room.streamOffset = -1;
				InsertRoom(candidate.rooms, std::move(room));
			}
			++result.copiesMade;
		}
		// SpaceShipDesignCopy does not call SpaceShipDesignVerifyDetails:
		// copied equipment retains its original room ID until a later room edit.
		result.changed = candidate.rooms.size() != design.rooms.size()
			|| candidate.details.size() != design.details.size();
		if(result.changed) SortDetails(candidate.details);
	}
	result.accepted = !copy || result.copiesMade > 0;
	if(result.accepted && result.changed) design = std::move(candidate);
	return result;
}
