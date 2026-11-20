#include "aulegacyship.h"

#include <QFile>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
// The original streams use Qt_4_0 QDataStream, big endian. Read explicitly so
// untrusted lengths are checked before QString or container allocations.
class Reader
{
public:
	Reader(const QByteArray& bytes, QString& error) : m_bytes(bytes), m_error(error) {}
	int Offset() const { return m_offset; }
	int Remaining() const { return m_bytes.size() - m_offset; }

	bool Fail(const QString& message)
	{
		m_error = QStringLiteral("offset 0x%1: %2").arg(m_offset, 0, 16).arg(message);
		return false;
	}

	bool Need(quint32 size, const QString& label)
	{
		if(size > quint32(Remaining()))
			return Fail(QStringLiteral("%1: need %2 bytes, only %3 remain")
				.arg(label).arg(size).arg(Remaining()));
		return true;
	}

	bool U8(quint8& value, const QString& label)
	{
		if(!Need(1, label)) return false;
		value = quint8(m_bytes.at(m_offset++));
		return true;
	}

	bool I8(qint8& value, const QString& label)
	{
		quint8 raw = 0;
		if(!U8(raw, label)) return false;
		value = qint8(raw >= 128 ? int(raw) - 256 : int(raw));
		return true;
	}

	bool U16(quint16& value, const QString& label)
	{
		if(!Need(2, label)) return false;
		value = quint16((quint16(quint8(m_bytes.at(m_offset))) << 8)
			| quint8(m_bytes.at(m_offset + 1)));
		m_offset += 2;
		return true;
	}

	bool U32(quint32& value, const QString& label)
	{
		if(!Need(4, label)) return false;
		value = (quint32(quint8(m_bytes.at(m_offset))) << 24)
			| (quint32(quint8(m_bytes.at(m_offset + 1))) << 16)
			| (quint32(quint8(m_bytes.at(m_offset + 2))) << 8)
			| quint32(quint8(m_bytes.at(m_offset + 3)));
		m_offset += 4;
		return true;
	}

	bool String(QString& value, const QString& label)
	{
		quint32 byteCount = 0;
		if(!U32(byteCount, label + QStringLiteral(".byte_length"))) return false;
		if(byteCount == 0xffffffffU)
		{
			value = QString();
			return true;
		}
		if(byteCount & 1U)
			return Fail(QStringLiteral("%1: odd UTF-16 byte length %2").arg(label).arg(byteCount));
		if(!Need(byteCount, label)) return false;
		// Assign code units directly: retain null versus empty and lone surrogates.
		value = QStringLiteral("");
		value.resize(int(byteCount / 2));
		for(int index = 0; index < value.size(); ++index)
		{
			const quint16 unit = quint16((quint16(quint8(m_bytes.at(m_offset))) << 8)
				| quint8(m_bytes.at(m_offset + 1)));
			value[index] = QChar(unit);
			m_offset += 2;
		}
		return true;
	}

	bool Count(quint32 count, quint32 minimumBytes, const QString& label)
	{
		if(count > quint32(Remaining()) / minimumBytes)
			return Fail(QStringLiteral("%1: count %2 exceeds remaining %3 bytes")
				.arg(label).arg(count).arg(Remaining()));
		if(count > quint32(m_recordsRemaining))
			return Fail(QStringLiteral("%1: exceeds combined %2-record safety limit")
				.arg(label).arg(AuLegacyShip::MaximumRecords));
		m_recordsRemaining -= int(count);
		return true;
	}

private:
	const QByteArray& m_bytes;
	QString& m_error;
	int m_offset = 0;
	int m_recordsRemaining = AuLegacyShip::MaximumRecords;
};

bool ReadPairs(Reader& reader, QVector<AuLegacyShip::ModulePair>& pairs,
	const QString& label, bool normalize)
{
	quint32 count = 0;
	if(!reader.U32(count, label + QStringLiteral(".count")) || !reader.Count(count, 4, label))
		return false;
	pairs.reserve(int(count));
	for(quint32 index = 0; index < count; ++index)
	{
		AuLegacyShip::ModulePair pair;
		if(!reader.U16(pair.first, label + QStringLiteral(".first"))
			|| !reader.U16(pair.secondRaw, label + QStringLiteral(".second")))
			return false;
		pair.secondNormalized = pair.secondRaw;
		if(normalize && (pair.secondRaw == 0xfd || pair.secondRaw == 0xff))
			--pair.secondNormalized;
		pairs.append(pair);
	}
	return true;
}
}

bool AuLegacyShip::Read(const QByteArray& bytes, Design& output, QString& error)
{
	error.clear();
	Reader reader(bytes, error);
	if(bytes.size() > MaximumFileBytes)
		return reader.Fail(QStringLiteral("input exceeds %1-byte safety limit").arg(MaximumFileBytes));

	Design parsed;
	Header& header = parsed.header;
	if(!reader.U32(header.designId, QStringLiteral("header.design_id"))
		|| !reader.String(header.name, QStringLiteral("header.name"))
		|| !reader.U32(header.architectId, QStringLiteral("header.architect_id"))
		|| !reader.U8(header.streamline, QStringLiteral("header.streamline"))
		|| !reader.U8(header.armor, QStringLiteral("header.armor"))
		|| !reader.U8(header.versionStateRaw, QStringLiteral("header.version_state")))
		return false;
	header.version = header.versionStateRaw >> 4;
	header.state = header.versionStateRaw & 0x0f;
	if(header.version > 2)
		return reader.Fail(QStringLiteral("unsupported .ship version %1; recovered versions are 0, 1 and 2")
			.arg(header.version));
	if(!reader.String(header.architectName, QStringLiteral("header.architect_name"))) return false;
	if(header.version < 2
		&& !reader.String(header.legacyDiscardedString, QStringLiteral("header.legacy_discarded")))
		return false;
	if(!reader.U8(header.minimumTechLevel, QStringLiteral("header.minimum_tech_level"))) return false;
	if(header.version > 1)
	{
		if(!reader.U32(header.hullVolume, QStringLiteral("header.hull_volume"))
			|| !reader.U8(header.structure, QStringLiteral("header.structure"))
			|| !reader.U8(header.techLevel, QStringLiteral("header.tech_level")))
			return false;
	}
	if(header.version != 0)
	{
		if(!ReadPairs(reader, header.firstPairs, QStringLiteral("modules.first_pairs"), true)) return false;
		header.scalarSlots.reserve(8);
		for(int index = 0; index < 8; ++index)
		{
			quint16 value = 0;
			if(!reader.U16(value, QStringLiteral("modules.scalar[%1]").arg(index))) return false;
			header.scalarSlots.append(value);
		}
		if(!ReadPairs(reader, header.secondPairs, QStringLiteral("modules.second_pairs"), false)) return false;
	}

	quint16 roomCount = 0;
	if(!reader.U16(roomCount, QStringLiteral("rooms.count"))
		|| !reader.Count(roomCount, 11, QStringLiteral("rooms")))
		return false;
	parsed.rooms.reserve(roomCount);
	for(int index = 0; index < roomCount; ++index)
	{
		Room room;
		room.streamOffset = reader.Offset();
		const QString label = QStringLiteral("rooms[%1]").arg(index);
		quint16 vertexCount = 0;
		if(!reader.U32(room.designId, label + QStringLiteral(".design_id"))
			|| !reader.U16(room.roomId, label + QStringLiteral(".room_id"))
			|| !reader.I8(room.deck, label + QStringLiteral(".deck"))
			|| !reader.U8(room.roomType, label + QStringLiteral(".room_type"))
			|| !reader.U8(room.roomSubtype, label + QStringLiteral(".room_subtype"))
			|| !reader.U16(vertexCount, label + QStringLiteral(".vertex_count"))
			|| !reader.Count(vertexCount, 2, label + QStringLiteral(".vertices")))
			return false;
		room.packedVertices.reserve(vertexCount);
		for(int vertex = 0; vertex < vertexCount; ++vertex)
		{
			quint16 packed = 0;
			if(!reader.U16(packed, label + QStringLiteral(".vertices"))) return false;
			room.packedVertices.append(packed);
		}
		// Retain empty records for inspection even though the original array reader
		// discards them. They have an empty display boundary.
		parsed.rooms.append(room);
	}

	quint32 detailCount = 0;
	if(!reader.U32(detailCount, QStringLiteral("details.count"))
		|| !reader.Count(detailCount, 17, QStringLiteral("details")))
		return false;
	parsed.details.reserve(int(detailCount));
	for(quint32 index = 0; index < detailCount; ++index)
	{
		Detail detail;
		detail.streamOffset = reader.Offset();
		const QString label = QStringLiteral("details[%1]").arg(index);
		if(!reader.I8(detail.deck, label + QStringLiteral(".deck"))
			|| !reader.U8(detail.detailType, label + QStringLiteral(".detail_type"))
			|| !reader.U8(detail.hiddenRaw, label + QStringLiteral(".hidden"))
			|| !reader.U32(detail.detailId, label + QStringLiteral(".detail_id"))
			|| !reader.U32(detail.designId, label + QStringLiteral(".design_id"))
			|| !reader.U16(detail.packedXY, label + QStringLiteral(".packed_xy"))
			|| !reader.U16(detail.roomId, label + QStringLiteral(".room_id"))
			|| !reader.U16(detail.commodityIdRaw, label + QStringLiteral(".commodity_id")))
			return false;
		detail.commodityIdNormalized = detail.commodityIdRaw;
		if(detail.commodityIdRaw == 0xf9 || detail.commodityIdRaw == 0xfb)
			--detail.commodityIdNormalized;
		parsed.details.append(detail);
	}
	if(reader.Remaining())
		return reader.Fail(QStringLiteral("unexpected trailing data: %1 bytes").arg(reader.Remaining()));
	parsed.bytesConsumed = reader.Offset();
	output = std::move(parsed);
	return true;
}

bool AuLegacyShip::Load(const QString& fileName, Design& output, QString& error)
{
	error.clear();
	QFile file(fileName);
	if(!file.open(QIODevice::ReadOnly))
	{
		error = QStringLiteral("offset 0x0: cannot open %1: %2").arg(fileName, file.errorString());
		return false;
	}
	// The extra byte detects oversized files without relying on a size check
	// that could race with another process writing the source.
	const QByteArray bytes = file.read(qint64(MaximumFileBytes) + 1);
	if(file.error() != QFileDevice::NoError)
	{
		error = QStringLiteral("offset 0x%1: cannot read %2: %3")
			.arg(bytes.size(), 0, 16).arg(fileName, file.errorString());
		return false;
	}
	return Read(bytes, output, error);
}

QPointF AuLegacyShip::DecodePoint(quint16 packedXY)
{
	const int rawX = packedXY >> 8;
	const int rawY = packedXY & 0xff;
	const int x = rawX >= 128 ? rawX - 256 : rawX;
	const int y = rawY >= 128 ? rawY - 256 : rawY;
	return QPointF(x * CoordinateSpacing, y * CoordinateSpacing);
}

QPolygonF AuLegacyShip::RoomBoundary(const Room& room, QString* error)
{
	if(error) error->clear();
	if(room.hasWorkingBoundary)
		return room.workingBoundary;
	QPolygonF boundary;
	if(!room.IsEllipse())
	{
		boundary.reserve(room.packedVertices.size());
		for(quint16 packed : room.packedVertices)
			boundary.append(DecodePoint(packed));
		return boundary;
	}

	const QPointF center = DecodePoint(room.packedVertices.at(0));
	boundary = EllipseBoundary(center, DecodePoint(room.packedVertices.at(1)), DecodePoint(room.packedVertices.at(2)));
	if(boundary.isEmpty() && error)
		*error = QStringLiteral("offset 0x%1: room %2 has degenerate ellipse controls")
			.arg(room.streamOffset, 0, 16).arg(room.roomId);
	return boundary;
}

QPolygonF AuLegacyShip::EllipseBoundary(const QPointF& center, const QPointF& axisPoint, const QPointF& sidePoint)
{
	QPolygonF boundary;
	const QPointF axisX = axisPoint - center;
	const QPointF axisY = sidePoint - center;
	const double radiusX = std::sqrt(axisX.x() * axisX.x() + axisX.y() * axisX.y());
	const double radiusY = std::sqrt(axisY.x() * axisY.x() + axisY.y() * axisY.y());
	const double cross = axisX.x() * axisY.y() - axisX.y() * axisY.x();
	if(radiusX == 0.0 || radiusY == 0.0
		|| std::abs(cross) <= 4 * std::numeric_limits<double>::epsilon() * radiusX * radiusY)
		return boundary;

	// DnRoom::operator<< at AuData13.dll:0x180167cf0 casts to float before
	// sqrtf/ceilf. Keep that rounding when selecting the original facet count.
	const float gridRadius = float(std::max(radiusX, radiusY) / GridSpacing);
	const int facets = (int(std::ceil(std::sqrt(gridRadius))) + 1) * 4;
	// AuMath13.dll:0x180039600 uses float radii and trigonometry, unlike the
	// surviving modern CreateEllipse implementation. Preserve legacy rounding.
	const float legacyRadiusX = float(radiusX);
	const float legacyRadiusY = float(radiusY);
	const QPointF unitX = axisX / double(legacyRadiusX);
	const QPointF unitY = axisY / double(legacyRadiusY);
	const float twoPi = 6.2831855f;
	boundary.reserve(facets);
	for(int vertex = 0; vertex < facets; ++vertex)
	{
		const float angle = twoPi - (float(vertex) * twoPi) / float(facets);
		const float cosineTerm = std::cos(angle) * legacyRadiusX;
		const float sineTerm = std::sin(angle) * legacyRadiusY;
		boundary.append(center + unitX * double(cosineTerm) + unitY * double(sineTerm));
	}
	return boundary;
}
