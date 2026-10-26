#include "aulegacystudioprotocol.h"
#include "aulegacyshipdetails.h"
#include <QDataStream>
#include <QIODevice>
#include <QSet>
#include <cmath>
#include <utility>
#include <zlib.h>

namespace
{
using namespace AuLegacyStudioProtocol;
const quint32 DocumentMagic = 0x4c535744; // LSWD: legacy studio working document.
const quint32 StateMagic = 0x4c535354;    // LSST: persisted studio state.
const quint8 CodecVersion = 1;
const quint8 StateCodecVersion = 2;
const quint8 ReplyCodecVersion = 3;
const int MaximumMemberNameCharacters = 128;
const int MaximumMessageCharacters = 4096;

bool Available(QDataStream& in, quint64 bytes)
{
	return in.status() == QDataStream::Ok && bytes <= quint64(in.device()->bytesAvailable());
}

// Avoid QDataStream's allocating QString/QVector/QByteArray extraction operators
// on untrusted input. Counts are checked before allocating any backing storage.
bool WriteString(QDataStream& out, const QString& value, int maximumCharacters)
{
	if(value.size() > maximumCharacters
		|| quint64(out.device()->pos()) + 4 + quint64(value.size()) * 2 > MaximumDocumentBytes) return false;
	if(value.isNull()) { out << quint32(0xffffffffu); return true; }
	out << quint32(value.size());
	for(int index = 0; index < value.size(); ++index) out << quint16(value.at(index).unicode());
	return out.status() == QDataStream::Ok;
}

bool ReadString(QDataStream& in, QString& value, int maximumCharacters)
{
	quint32 count = 0; in >> count;
	if(in.status() != QDataStream::Ok) return false;
	if(count == 0xffffffffu) { value = QString(); return true; }
	if(count > quint32(maximumCharacters) || !Available(in, quint64(count) * 2)) return false;
	QString parsed = QStringLiteral(""); parsed.resize(int(count));
	for(quint32 index = 0; index < count; ++index)
	{
		quint16 unit = 0; in >> unit; parsed[int(index)] = QChar(unit);
	}
	if(in.status() != QDataStream::Ok) return false;
	value = std::move(parsed); return true;
}

bool WriteBytes(QDataStream& out, const QByteArray& value, int maximum)
{
	if(value.size() > maximum) return false;
	out << quint32(value.size());
	return out.writeRawData(value.constData(), value.size()) == value.size();
}

bool ReadBytes(QDataStream& in, QByteArray& value, int maximum)
{
	quint32 count = 0; in >> count;
	if(count > quint32(maximum) || !Available(in, count)) return false;
	QByteArray parsed(int(count), '\0');
	if(in.readRawData(parsed.data(), int(count)) != int(count)) return false;
	value = std::move(parsed); return true;
}

bool ReadFlag(QDataStream& in, bool& value)
{
	quint8 byte = 0; in >> byte;
	if(in.status() != QDataStream::Ok || byte > 1) return false;
	value = byte != 0; return true;
}

bool ReadCount(QDataStream& in, quint32& count, quint32 minimumBytes, quint32 maximum)
{
	in >> count;
	return count <= maximum && Available(in, quint64(count) * minimumBytes);
}

bool FinitePoint(const QPointF& point)
{
	return std::isfinite(point.x()) && std::isfinite(point.y());
}

bool ValidCommand(const Command& command)
{
	if(command.kind > Armor) return false;
	if(command.kind <= EraseDeck)
	{
		if(command.deck < -128 || command.deck > 127) return false;
		if(command.kind != EraseDeck && (!FinitePoint(command.cursor)
			|| std::abs(command.cursor.x()) > 1024 || std::abs(command.cursor.y()) > 1024)) return false;
	}
	if(command.kind == PaintCell)
		return command.gridMask <= 15 && (command.roomType == -1
			|| (command.roomType >= 1 && command.roomType <= 21 && command.roomType != 12));
	if(command.kind == PlaceDetail || command.kind == EraseDetails)
		return command.radius >= 0 && command.radius <= 25
			&& (command.kind != PlaceDetail || (command.detailType >= 1 && command.detailType <= 142));
	if(command.kind == MoveCopy)
	{
		const auto& move = command.move;
		return int(move.mode) >= int(AuLegacyShip::MoveCopyMode::MoveEntireDesign)
			&& int(move.mode) <= int(AuLegacyShip::MoveCopyMode::CopyCurrentDeck)
			&& move.deck >= -128 && move.deck <= 127 && move.lateral >= -80 && move.lateral <= 80
			&& move.longitudinal >= -80 && move.longitudinal <= 80 && move.vertical >= -84 && move.vertical <= 84
			&& move.copies >= 1 && move.copies <= 84;
	}
	if(command.kind == Streamline) return command.value == 0 || command.value == 1;
	if(command.kind == MinTech) return command.value >= 0 && command.value <= 31;
	if(command.kind == Armor) return command.value == -1 || command.value == 1;
	return true;
}

void WriteCommand(QDataStream& out, const Command& command)
{
	out << quint8(command.kind) << qint32(command.deck) << double(command.cursor.x()) << double(command.cursor.y())
		<< command.gridMask << command.roomNumber << qint32(command.roomType) << command.detailType
		<< command.commodity << qint32(command.radius) << quint8(command.move.mode) << qint32(command.move.deck)
		<< qint32(command.move.lateral) << qint32(command.move.longitudinal) << qint32(command.move.vertical)
		<< qint32(command.move.copies) << qint32(command.value);
}

bool ReadCommand(QDataStream& in, Command& command)
{
	quint8 kind = 0, mode = 0;
	qint32 deck = 0, roomType = 0, radius = 0, moveDeck = 0, lateral = 0, longitudinal = 0, vertical = 0, copies = 0, value = 0;
	double x = 0, y = 0;
	in >> kind >> deck >> x >> y >> command.gridMask >> command.roomNumber >> roomType >> command.detailType
		>> command.commodity >> radius >> mode >> moveDeck >> lateral >> longitudinal >> vertical >> copies >> value;
	if(in.status() != QDataStream::Ok || kind > Armor || mode > quint8(AuLegacyShip::MoveCopyMode::CopyCurrentDeck)) return false;
	command.kind = static_cast<EditKind>(kind); command.deck = deck; command.cursor = QPointF(x, y);
	command.roomType = roomType; command.radius = radius; command.value = value;
	command.move.mode = static_cast<AuLegacyShip::MoveCopyMode>(mode); command.move.deck = moveDeck;
	command.move.lateral = lateral; command.move.longitudinal = longitudinal; command.move.vertical = vertical; command.move.copies = copies;
	return ValidCommand(command);
}

bool ValidRequest(const Request& request)
{
	if(request.action > LoadDesign || !request.token || !request.environmentId
		|| (request.action != Snapshot && request.action != Library
			&& (!request.operationId || !request.generation || !request.revision))
		|| (request.action == Edit && !ValidCommand(request.edit))
		|| request.document.size() > MaximumCompressedDocumentBytes
		|| (request.action == Replace ? request.document.isEmpty() : !request.document.isEmpty())
		|| request.password.size() > MaximumPasswordCharacters
		|| (request.action != Password && !request.password.isEmpty())
		|| request.name.size() > MaximumNameCharacters
		|| (request.action != Saved && !request.name.isEmpty())
		|| (request.action == LoadDesign ? !request.designId : request.designId != 0)) return false;
	return true;
}

bool WritePairs(QDataStream& out, const QVector<AuLegacyShip::ModulePair>& pairs)
{
	if(pairs.size() > AuLegacyShip::MaximumRecords) return false;
	out << quint32(pairs.size());
	for(const auto& pair : pairs) out << pair.first << pair.secondRaw << pair.secondNormalized;
	return out.status() == QDataStream::Ok;
}

bool ReadPairs(QDataStream& in, QVector<AuLegacyShip::ModulePair>& pairs, quint32& records)
{
	quint32 count = 0;
	if(!ReadCount(in, count, 6, records)) return false;
	records -= count; pairs.reserve(int(count));
	for(quint32 index = 0; index < count; ++index)
	{
		AuLegacyShip::ModulePair pair; in >> pair.first >> pair.secondRaw >> pair.secondNormalized;
		pairs.append(pair);
	}
	return in.status() == QDataStream::Ok;
}

QByteArray UncompressDocument(const QByteArray& bytes, QString& error)
{
	if(bytes.size() < 5 || bytes.size() > MaximumCompressedDocumentBytes)
	{ error = QStringLiteral("The studio document exceeds the compressed size limit."); return QByteArray(); }
	const quint32 declared = (quint32(quint8(bytes[0])) << 24) | (quint32(quint8(bytes[1])) << 16)
		| (quint32(quint8(bytes[2])) << 8) | quint32(quint8(bytes[3]));
	if(!declared || declared > quint32(MaximumDocumentBytes))
	{ error = QStringLiteral("The studio document exceeds the working size limit."); return QByteArray(); }
	QByteArray raw(int(declared), '\0');
	z_stream stream = {};
	stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(bytes.constData() + 4));
	stream.avail_in = uInt(bytes.size() - 4);
	stream.next_out = reinterpret_cast<Bytef*>(raw.data()); stream.avail_out = uInt(raw.size());
	if(inflateInit(&stream) != Z_OK)
	{ error = QStringLiteral("The studio document decompressor could not start."); return QByteArray(); }
	// qUncompress may grow beyond a forged length prefix. Give zlib exactly
	// the checked destination size and require both lengths to match instead.
	const int result = inflate(&stream, Z_FINISH);
	const bool complete = result == Z_STREAM_END && stream.total_out == declared && stream.avail_in == 0;
	inflateEnd(&stream);
	if(!complete) { error = QStringLiteral("The compressed studio document is malformed."); return QByteArray(); }
	return raw;
}
}

QByteArray AuLegacyStudioProtocol::EncodeRequest(const Request& request)
{
	if(!ValidRequest(request)) return QByteArray();
	QByteArray bytes; QDataStream out(&bytes, QIODevice::WriteOnly); out.setVersion(QDataStream::Qt_4_1);
	out << CodecVersion << quint8(request.action) << request.token << request.environmentId
		<< request.operationId << request.generation << request.revision;
	if(request.action == Edit) WriteCommand(out, request.edit);
	if(request.action == Replace && !WriteBytes(out, request.document, MaximumCompressedDocumentBytes)) return QByteArray();
	if(request.action == Password && !WriteString(out, request.password, MaximumPasswordCharacters)) return QByteArray();
	if(request.action == Saved && !WriteString(out, request.name, MaximumNameCharacters)) return QByteArray();
	if(request.action == LoadDesign) out << request.designId;
	return out.status() == QDataStream::Ok && bytes.size() <= MaximumPacketBytes ? bytes : QByteArray();
}

bool AuLegacyStudioProtocol::IsLibraryNameValid(const QString& name)
{
	if(name.isEmpty() || name.size() > 30 || name.simplified() != name) return false;
	const QString forbidden = QStringLiteral("/:!|<>&\\");
	for(const QChar character : name)
		if(!character.isPrint() || forbidden.contains(character)) return false;
	return true;
}

bool AuLegacyStudioProtocol::DecodeRequest(const QByteArray& bytes, Request& request, QString& error)
{
	error = QStringLiteral("Malformed legacy studio request.");
	if(bytes.size() < 30 || bytes.size() > MaximumPacketBytes) return false;
	QDataStream in(bytes); in.setVersion(QDataStream::Qt_4_1);
	Request parsed; quint8 version = 0, action = 0;
	in >> version >> action >> parsed.token >> parsed.environmentId >> parsed.operationId >> parsed.generation >> parsed.revision;
	if(version != CodecVersion || action > LoadDesign) return false;
	parsed.action = static_cast<Action>(action);
	if(parsed.action == Edit && !ReadCommand(in, parsed.edit)) return false;
	if(parsed.action == Replace && !ReadBytes(in, parsed.document, MaximumCompressedDocumentBytes)) return false;
	if(parsed.action == Password && !ReadString(in, parsed.password, MaximumPasswordCharacters)) return false;
	if(parsed.action == Saved && !ReadString(in, parsed.name, MaximumNameCharacters)) return false;
	if(parsed.action == LoadDesign) in >> parsed.designId;
	if(in.status() != QDataStream::Ok || !in.atEnd() || !ValidRequest(parsed)) return false;
	request = std::move(parsed); error.clear(); return true;
}

QByteArray AuLegacyStudioProtocol::EncodeReply(const Reply& reply)
{
	if(!reply.token || !reply.environmentId || reply.members.size() > MaximumMembers
		|| (reply.hasDocument ? reply.document.isEmpty() || !reply.generation || !reply.revision : !reply.document.isEmpty())
		|| reply.document.size() > MaximumCompressedDocumentBytes || reply.library.size() > MaximumLibraryEntries
		|| (!reply.hasLibrary && (!reply.library.isEmpty() || reply.libraryTruncated))) return QByteArray();
	QByteArray bytes; QDataStream out(&bytes, QIODevice::WriteOnly); out.setVersion(QDataStream::Qt_4_1);
	out << ReplyCodecVersion << reply.token << reply.environmentId << reply.ownerId << reply.generation << reply.revision
		<< reply.lastOperation << reply.libraryDesignId << quint8(reply.accepted) << quint8(reply.modified) << quint8(reply.hasDocument)
		<< quint8(reply.retry) << quint8(reply.canBurn);
	if(!WriteString(out, reply.message, MaximumMessageCharacters)) return QByteArray();
	out << quint16(reply.members.size());
	QSet<quint32> seen;
	for(const auto& member : reply.members)
	{
		if(!member.id || seen.contains(member.id) || !WriteString(out, member.name, MaximumMemberNameCharacters)) return QByteArray();
		seen.insert(member.id); out << member.id;
	}
	if(reply.hasDocument && !WriteBytes(out, reply.document, MaximumCompressedDocumentBytes)) return QByteArray();
	out << quint8(reply.hasLibrary) << quint8(reply.libraryTruncated) << quint16(reply.library.size());
	seen.clear();
	for(const auto& entry : reply.library)
	{
		if(!entry.id || seen.contains(entry.id) || entry.state > 5) return QByteArray();
		seen.insert(entry.id); out << entry.id << entry.state;
		if(!WriteString(out, entry.name, MaximumNameCharacters)) return QByteArray();
	}
	return out.status() == QDataStream::Ok && bytes.size() <= MaximumPacketBytes ? bytes : QByteArray();
}

bool AuLegacyStudioProtocol::DecodeReply(const QByteArray& bytes, Reply& reply)
{
	if(bytes.size() < 44 || bytes.size() > MaximumPacketBytes) return false;
	QDataStream in(bytes); in.setVersion(QDataStream::Qt_4_1);
	Reply parsed; quint8 version = 0;
	in >> version >> parsed.token >> parsed.environmentId >> parsed.ownerId >> parsed.generation >> parsed.revision >> parsed.lastOperation;
	if(version >= 2) in >> parsed.libraryDesignId;
	if((version < CodecVersion || version > ReplyCodecVersion) || !parsed.token || !parsed.environmentId
		|| !ReadFlag(in, parsed.accepted) || !ReadFlag(in, parsed.modified) || !ReadFlag(in, parsed.hasDocument)
		|| !ReadFlag(in, parsed.retry) || !ReadFlag(in, parsed.canBurn)
		|| !ReadString(in, parsed.message, MaximumMessageCharacters)) return false;
	quint16 count = 0; in >> count;
	if(count > MaximumMembers || !Available(in, quint64(count) * 8)) return false;
	QSet<quint32> seen;
	for(quint16 index = 0; index < count; ++index)
	{
		Member member;
		if(!ReadString(in, member.name, MaximumMemberNameCharacters)) return false;
		in >> member.id;
		if(in.status() != QDataStream::Ok || !member.id || seen.contains(member.id)) return false;
		seen.insert(member.id); parsed.members.append(member);
	}
	if(parsed.hasDocument && (!parsed.generation || !parsed.revision
		|| !ReadBytes(in, parsed.document, MaximumCompressedDocumentBytes) || parsed.document.isEmpty())) return false;
	if(version >= 3)
	{
		if(!ReadFlag(in, parsed.hasLibrary) || !ReadFlag(in, parsed.libraryTruncated)) return false;
		quint16 libraryCount = 0; in >> libraryCount;
		if(libraryCount > MaximumLibraryEntries || (!parsed.hasLibrary && (libraryCount || parsed.libraryTruncated))
			|| !Available(in, quint64(libraryCount) * 9)) return false;
		seen.clear();
		for(quint16 index = 0; index < libraryCount; ++index)
		{
			LibraryEntry entry; in >> entry.id >> entry.state;
			if(!entry.id || seen.contains(entry.id) || entry.state > 5
				|| !ReadString(in, entry.name, MaximumNameCharacters)) return false;
			seen.insert(entry.id); parsed.library.append(entry);
		}
	}
	if(in.status() != QDataStream::Ok || !in.atEnd()) return false;
	reply = std::move(parsed); return true;
}

QByteArray AuLegacyStudioProtocol::EncodeDocument(const AuLegacyShip::Design& design, QString& error)
{
	error = QStringLiteral("The studio working document exceeds its limits.");
	const auto& header = design.header;
	const quint64 records = quint64(header.firstPairs.size()) + header.secondPairs.size() + design.rooms.size() + design.details.size();
	if(records > AuLegacyShip::MaximumRecords || header.scalarSlots.size() > AuLegacyShip::MaximumRecords) return QByteArray();
	QByteArray raw; QDataStream out(&raw, QIODevice::WriteOnly); out.setVersion(QDataStream::Qt_4_1);
	out << DocumentMagic << CodecVersion << header.designId;
	if(!WriteString(out, header.name, MaximumDocumentBytes / 2)) return QByteArray();
	out << header.architectId << header.streamline << header.armor << header.versionStateRaw << header.version << header.state;
	if(!WriteString(out, header.architectName, MaximumDocumentBytes / 2)
		|| !WriteString(out, header.legacyDiscardedString, MaximumDocumentBytes / 2)) return QByteArray();
	out << header.minimumTechLevel << header.hullVolume << header.structure << header.techLevel;
	if(!WritePairs(out, header.firstPairs)) return QByteArray();
	out << quint32(header.scalarSlots.size());
	for(const quint16 value : header.scalarSlots) out << value;
	if(!WritePairs(out, header.secondPairs)) return QByteArray();
	out << qint32(design.bytesConsumed) << quint32(design.rooms.size());
	for(const auto& room : design.rooms)
	{
		if(room.packedVertices.size() > AuLegacyShip::MaximumRecords || room.workingBoundary.size() > AuLegacyShip::MaximumRecords
			|| quint64(raw.size()) + 22 + quint64(room.packedVertices.size()) * 2
				+ quint64(room.workingBoundary.size()) * 16 > MaximumDocumentBytes) return QByteArray();
		out << qint32(room.streamOffset) << room.designId << room.roomId << room.deck << room.roomType << room.roomSubtype
			<< quint32(room.packedVertices.size());
		for(const quint16 vertex : room.packedVertices) out << vertex;
		out << quint8(room.hasWorkingBoundary) << quint32(room.workingBoundary.size());
		for(const auto& point : room.workingBoundary)
		{
			if(!FinitePoint(point)) { error = QStringLiteral("A studio room boundary is not finite."); return QByteArray(); }
			out << double(point.x()) << double(point.y());
		}
	}
	if(quint64(raw.size()) + 4 + quint64(design.details.size()) * 23 > MaximumDocumentBytes) return QByteArray();
	out << quint32(design.details.size());
	for(const auto& detail : design.details)
		out << qint32(detail.streamOffset) << detail.deck << detail.detailType << detail.hiddenRaw << detail.detailId
			<< detail.designId << detail.packedXY << detail.roomId << detail.commodityIdRaw << detail.commodityIdNormalized;
	if(out.status() != QDataStream::Ok || raw.size() > MaximumDocumentBytes) return QByteArray();
	QByteArray compressed = qCompress(raw);
	if(compressed.isEmpty() || compressed.size() > MaximumCompressedDocumentBytes) return QByteArray();
	error.clear(); return compressed;
}

bool AuLegacyStudioProtocol::DecodeDocument(const QByteArray& bytes, AuLegacyShip::Design& design, QString& error)
{
	error.clear(); const QByteArray raw = UncompressDocument(bytes, error);
	if(raw.isEmpty()) return false;
	error = QStringLiteral("Malformed legacy studio working document.");
	QDataStream in(raw); in.setVersion(QDataStream::Qt_4_1);
	quint32 magic = 0; quint8 version = 0;
	AuLegacyShip::Design parsed; auto& header = parsed.header;
	in >> magic >> version >> header.designId;
	if(magic != DocumentMagic || version != CodecVersion || !ReadString(in, header.name, MaximumDocumentBytes / 2)) return false;
	in >> header.architectId >> header.streamline >> header.armor >> header.versionStateRaw >> header.version >> header.state;
	if(!ReadString(in, header.architectName, MaximumDocumentBytes / 2)
		|| !ReadString(in, header.legacyDiscardedString, MaximumDocumentBytes / 2)) return false;
	in >> header.minimumTechLevel >> header.hullVolume >> header.structure >> header.techLevel;
	quint32 records = AuLegacyShip::MaximumRecords;
	if(!ReadPairs(in, header.firstPairs, records)) return false;
	quint32 count = 0;
	if(!ReadCount(in, count, 2, AuLegacyShip::MaximumRecords)) return false;
	header.scalarSlots.reserve(int(count));
	for(quint32 index = 0; index < count; ++index) { quint16 value = 0; in >> value; header.scalarSlots.append(value); }
	if(!ReadPairs(in, header.secondPairs, records)) return false;
	qint32 consumed = 0; in >> consumed; parsed.bytesConsumed = consumed;
	if(!ReadCount(in, count, 22, records)) return false;
	records -= count; parsed.rooms.reserve(int(count));
	for(quint32 index = 0; index < count; ++index)
	{
		AuLegacyShip::Room room; qint32 offset = 0;
		in >> offset >> room.designId >> room.roomId >> room.deck >> room.roomType >> room.roomSubtype;
		room.streamOffset = offset; quint32 vertices = 0;
		if(!ReadCount(in, vertices, 2, AuLegacyShip::MaximumRecords)) return false;
		room.packedVertices.reserve(int(vertices));
		for(quint32 vertex = 0; vertex < vertices; ++vertex) { quint16 packed = 0; in >> packed; room.packedVertices.append(packed); }
		if(!ReadFlag(in, room.hasWorkingBoundary) || !ReadCount(in, vertices, 16, AuLegacyShip::MaximumRecords)) return false;
		room.workingBoundary.reserve(int(vertices));
		for(quint32 vertex = 0; vertex < vertices; ++vertex)
		{
			double x = 0, y = 0; in >> x >> y;
			const QPointF point(x, y); if(!FinitePoint(point)) return false;
			room.workingBoundary.append(point);
		}
		parsed.rooms.append(std::move(room));
	}
	if(!ReadCount(in, count, 23, records)) return false;
	parsed.details.reserve(int(count));
	for(quint32 index = 0; index < count; ++index)
	{
		AuLegacyShip::Detail detail; qint32 offset = 0;
		in >> offset >> detail.deck >> detail.detailType >> detail.hiddenRaw >> detail.detailId >> detail.designId
			>> detail.packedXY >> detail.roomId >> detail.commodityIdRaw >> detail.commodityIdNormalized;
		detail.streamOffset = offset; parsed.details.append(detail);
	}
	if(in.status() != QDataStream::Ok || !in.atEnd()) return false;
	design = std::move(parsed); error.clear(); return true;
}

QByteArray AuLegacyStudioProtocol::EncodeState(const State& state, QString& error)
{
	error = QStringLiteral("Invalid legacy studio state.");
	if(!state.generation || !state.revision || state.appliedOperations.size() > MaximumMembers) return QByteArray();
	const QByteArray document = EncodeDocument(state.design, error); if(document.isEmpty()) return QByteArray();
	QByteArray bytes; QDataStream out(&bytes, QIODevice::WriteOnly); out.setVersion(QDataStream::Qt_4_1);
	out << StateMagic << StateCodecVersion << state.generation << state.revision << state.libraryDesignId << quint8(state.modified)
		<< quint16(state.appliedOperations.size());
	for(auto operation = state.appliedOperations.cbegin(); operation != state.appliedOperations.cend(); ++operation)
	{
		if(!operation.key()) { error = QStringLiteral("Invalid studio operation owner."); return QByteArray(); }
		out << operation.key() << operation.value();
	}
	if(!WriteBytes(out, document, MaximumCompressedDocumentBytes) || out.status() != QDataStream::Ok
		|| bytes.size() > MaximumPacketBytes) { error = QStringLiteral("The studio state exceeds the size limit."); return QByteArray(); }
	error.clear(); return bytes;
}

bool AuLegacyStudioProtocol::DecodeState(const QByteArray& bytes, State& state, QString& error)
{
	error = QStringLiteral("Malformed legacy studio state.");
	if(bytes.size() < 24 || bytes.size() > MaximumPacketBytes) return false;
	QDataStream in(bytes); in.setVersion(QDataStream::Qt_4_1);
	State parsed; quint32 magic = 0; quint8 version = 0; quint16 count = 0;
	in >> magic >> version >> parsed.generation >> parsed.revision;
	if(version >= 2) in >> parsed.libraryDesignId;
	if(magic != StateMagic || (version != CodecVersion && version != StateCodecVersion)
		|| !parsed.generation || !parsed.revision || !ReadFlag(in, parsed.modified)) return false;
	in >> count;
	if(count > MaximumMembers || !Available(in, quint64(count) * 12 + 4)) return false;
	for(quint16 index = 0; index < count; ++index)
	{
		quint32 person = 0; quint64 operation = 0; in >> person >> operation;
		if(!person || parsed.appliedOperations.contains(person)) return false;
		parsed.appliedOperations.insert(person, operation);
	}
	QByteArray document;
	if(!ReadBytes(in, document, MaximumCompressedDocumentBytes) || in.status() != QDataStream::Ok || !in.atEnd()
		|| !DecodeDocument(document, parsed.design, error)) return false;
	state = std::move(parsed); error.clear(); return true;
}

bool AuLegacyStudioProtocol::Apply(const Command& command, AuLegacyShip::Design& design, QString& error, bool& changed)
{
	error.clear(); changed = false;
	if(!ValidCommand(command)) { error = QStringLiteral("The studio edit is outside the legacy tool limits."); return false; }
	AuLegacyShip::CellEditResult result;
	switch(command.kind)
	{
	case PaintCell:
		result = AuLegacyShip::EditCell(design, command.deck, command.cursor, command.gridMask, command.roomType, command.roomNumber); break;
	case EraseRoom:
		result = AuLegacyShip::DeleteRoomAt(design, command.deck, command.cursor); break;
	case PlaceDetail:
	{
		const auto detail = AuLegacyShip::PlaceDetail(design, command.deck, command.cursor, command.detailType, command.commodity, command.radius);
		result.accepted = detail.accepted; result.changed = detail.changed; result.error = detail.error; break;
	}
	case EraseDetails:
	{
		const auto detail = AuLegacyShip::DeleteDetails(design, command.deck, command.cursor, command.radius);
		result.accepted = detail.accepted; result.changed = detail.changed; result.error = detail.error; break;
	}
	case EraseDeck:
		result = AuLegacyShip::DeleteDeck(design, command.deck); break;
	case MoveCopy:
		result = AuLegacyShip::MoveCopy(design, command.move); break;
	case Streamline:
		changed = design.header.streamline != command.value; design.header.streamline = quint8(command.value); return true;
	case MinTech:
		changed = design.header.minimumTechLevel != command.value; design.header.minimumTechLevel = quint8(command.value); return true;
	case Armor:
	{
		const int armor = int(design.header.armor) + command.value;
		if(armor < 0 || armor > 10) { error = QStringLiteral("Armor must remain between 0 and 10."); return false; }
		design.header.armor = quint8(armor); changed = true; return true;
	}
	}
	error = result.error; changed = result.changed; return result.accepted;
}
