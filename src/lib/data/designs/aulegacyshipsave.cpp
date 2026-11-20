#include "aulegacyshipsave.h"
#include "aulegacybd.h"

#include <QDataStream>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <list>
#include <utility>

namespace
{
using namespace AuLegacyShip;

// The file stores the iteration order of the 2016 MSVC unordered_map. Using
// the host's QHash/std::unordered_map would give different serialized bytes.
class ModuleMap
{
public:
	explicit ModuleMap(const QVector<ModulePair>& pairs) : buckets(8, entries.end())
	{
		for(const ModulePair& pair : pairs)
			Insert(pair);
	}

	QVector<ModulePair> Reconcile(const QSet<quint16>& required, quint16 defaultCommodity)
	{
		QList<quint16> keys = required.values();
		std::sort(keys.begin(), keys.end());
		// ReconcileModules walks a QMap in ascending key order, inserts defaults
		// first, then trims obsolete keys. Growth before trimming affects order.
		for(quint16 key : keys)
		{
			ModulePair pair;
			pair.first = key;
			pair.secondRaw = pair.secondNormalized = defaultCommodity;
			Insert(pair);
		}
		QVector<ModulePair> result;
		result.reserve(required.size());
		for(const ModulePair& pair : entries)
			if(required.contains(pair.first))
				result.append(pair);
		return result;
	}

private:
	static quint64 Hash(quint16 key)
	{
		// AuShipWhichHash::operator(), AuData13.dll:0x18002a120.
		const quint64 hash = (((quint64(key & 0xff) ^ Q_UINT64_C(0xcbf29ce484222325))
			* Q_UINT64_C(0x100000001b3)) ^ quint64(key >> 8)) * Q_UINT64_C(0x8a97b0004e7feab);
		return (hash >> 32) ^ hash;
	}

	void Insert(const ModulePair& pair)
	{
		// The original stream reader keeps the first value of duplicate keys.
		if(present.contains(pair.first))
			return;
		present.insert(pair.first);
		const int bucket = int(Hash(pair.first) & quint64(buckets.size() - 1));
		buckets[bucket] = entries.insert(buckets[bucket], pair);
		if(entries.size() <= size_t(buckets.size()))
			return;
		// FUN_180048600: 8 -> 64 -> 512, then doubling. Rehash visits the
		// previous list order, moving each node to its new bucket's front.
		QVector<Iterator> previous;
		previous.reserve(int(entries.size()));
		for(Iterator it = entries.begin(); it != entries.end(); ++it)
			previous.append(it);
		buckets.fill(entries.end(), buckets.size() * (buckets.size() < 512 ? 8 : 2));
		for(Iterator it : previous)
		{
			const int target = int(Hash(it->first) & quint64(buckets.size() - 1));
			entries.splice(buckets[target], entries, it);
			buckets[target] = it;
		}
	}

	typedef std::list<ModulePair>::iterator Iterator;
	std::list<ModulePair> entries;
	QVector<Iterator> buckets;
	QSet<quint16> present;
};

bool EncodePoint(const QPointF& point, quint16& packed)
{
	// DnBuilding::EncodeGridCoord, AuData13.dll:0x18017a390. Round ties
	// away from zero. Reject overflow rather than silently wrapping geometry.
	const double x = std::round(point.x() / CoordinateSpacing);
	const double y = std::round(point.y() / CoordinateSpacing);
	if(!std::isfinite(x) || !std::isfinite(y) || x < -128 || x > 127 || y < -128 || y > 127)
		return false;
	packed = quint16(((int(x) & 0xff) << 8) | (int(y) & 0xff));
	return true;
}

bool RoomVertices(const Room& room, QVector<quint16>& vertices, QString& error)
{
	if(!room.hasWorkingBoundary && room.roomType != 0x18)
	{
		vertices = room.packedVertices;
		return true;
	}
	QPolygonF boundary = RoomBoundary(room, &error);
	if(!error.isEmpty())
		return false;
	if(room.roomType == 0x18 && !boundary.isEmpty())
	{
		// DnRoom::operator>>, 0x180167a50: center, first vertex, then the
		// vertex at 3/4 of the expanded boundary. Preserve this axis order.
		const int count = boundary.size();
		const QPointF center = (boundary.at(0) + boundary.at(count / 2)) / 2.0;
		const QPointF first = boundary.at(0);
		const QPointF second = boundary.at((count * 3) / 4);
		boundary.clear();
		boundary << center << first << second;
	}
	vertices.reserve(boundary.size());
	for(const QPointF& point : boundary)
	{
		quint16 packed = 0;
		if(!EncodePoint(point, packed))
		{
			error = QStringLiteral("Room %1 has a non-finite coordinate or exceeds the legacy coordinate range.")
				.arg(room.roomId);
			return false;
		}
		vertices.append(packed);
	}
	return true;
}

struct SavedRoom
{
	const Room* room;
	QVector<quint16> vertices;
};
}

QVector<quint16> AuLegacyShip::DefaultModuleSlots()
{
	static const QVector<quint16> defaults = { 0xea, 0x103, 0xed, 0xee, 0xeb, 0xef, 0x107, 0xf0 };
	return defaults;
}

quint16 AuLegacyShip::ScalarModule(const Design& design, int slot)
{
	const quint16 value = design.header.scalarSlots.value(slot);
	return value ? value : DefaultModuleSlots().value(slot);
}

void AuLegacyShip::SortDetails(QVector<Detail>& details)
{
	std::stable_sort(details.begin(), details.end(), [](const Detail& a, const Detail& b) {
		if(a.deck != b.deck) return a.deck < b.deck;
		if(a.roomId != b.roomId) return a.roomId < b.roomId;
		return a.detailId < b.detailId;
	});
}

void AuLegacyShip::ModuleSystemKeys(const Design& design, QSet<quint16>& bays, QSet<quint16>& transporters)
{
	QMap<quint16, const Room*> roomsById;
	bays.clear();
	transporters.clear();
	for(const Room& room : design.rooms)
	{
		if(room.hasWorkingBoundary ? room.workingBoundary.isEmpty() : room.packedVertices.isEmpty())
			continue;
		// AnalyzeDesign searches the first matching room ID, including when
		// malformed input contains duplicates. Empty source rooms are discarded.
		if(!roomsById.contains(room.roomId)) roomsById.insert(room.roomId, &room);
		if(room.roomType == 0x15) bays.insert(room.roomSubtype);
	}
	for(const Detail& detail : design.details)
	{
		const Room* room = roomsById.value(detail.roomId, nullptr);
		if(!room) continue;
		// DaSpaceShip::AnalyzeDesign at 0x1803c63c0: pads identify transporter
		// systems by room deck/number. Consoles or empty transporter rooms do not.
		if(detail.detailType == 0x14)
			transporters.insert(quint16((quint16(quint8(room->deck)) << 8) | room->roomSubtype));
		else if(detail.detailType == 0x17 || detail.detailType == 0x63)
			bays.insert(room->roomSubtype);
	}
}

bool AuLegacyShip::PrepareForSave(const Design& design, Design& output, QString& error)
{
	error.clear();
	if(design.building)
	{
		output = design;
		SortDetails(output.details);
		return true;
	}
	if(qint64(design.rooms.size()) + design.details.size() + design.header.firstPairs.size()
		+ design.header.secondPairs.size() > MaximumRecords)
	{
		error = QStringLiteral("The design exceeds the legacy record safety limit.");
		return false;
	}
	QSet<quint16> bays, transporters;
	ModuleSystemKeys(design, bays, transporters);
	Design prepared = design;
	ModuleMap bayModules(design.header.firstPairs);
	ModuleMap transporterModules(design.header.secondPairs);
	prepared.header.firstPairs = bayModules.Reconcile(bays, 0x105);
	prepared.header.secondPairs = transporterModules.Reconcile(transporters, 0xf6);
	SortDetails(prepared.details);
	output = std::move(prepared);
	return true;
}

bool AuLegacyShip::Write(const Design& design, QByteArray& output, QString& error,
	const QString& fallbackName)
{
	error.clear();
	const Header& header = design.header;
	if(design.building ? (!header.designId || design.buildingIndustry > 130 || header.state > 7)
		: (header.version > 2 || header.state > 0x0f))
	{
		error = QStringLiteral("Unsupported legacy format version or state.");
		return false;
	}
	const QVector<quint16> moduleSlots = header.version == 0 && header.scalarSlots.isEmpty()
		? DefaultModuleSlots() : header.scalarSlots;
	if(!design.building && moduleSlots.size() != 8)
	{
		error = QStringLiteral("The legacy module header must contain exactly eight scalar slots.");
		return false;
	}
	const QString name = header.name.isEmpty() && !fallbackName.isEmpty() ? fallbackName : header.name;
	// Fixed header (including both QString lengths), module counts/slots,
	// and room/detail counts. Use 64-bit arithmetic before any allocation.
	const qint64 headerRecords = design.building ? design.publishedEmpires.size()
		: qint64(header.firstPairs.size()) + header.secondPairs.size();
	qint64 byteCount = (design.building ? 32 : 56) + qint64(name.size()) * 2
		+ qint64(header.architectName.size()) * 2 + headerRecords * 4 + qint64(design.details.size()) * 17;
	qint64 records = headerRecords + design.details.size();
	const int maximumBytes = design.building ? AuLegacyBd::MaximumPayloadBytes + 2 : MaximumFileBytes;
	if(byteCount > maximumBytes || records + design.rooms.size() > MaximumRecords)
	{
		error = QStringLiteral("The design exceeds the legacy file size or record safety limit.");
		return false;
	}
	QVector<SavedRoom> rooms;
	rooms.reserve(design.rooms.size());
	for(const Room& room : design.rooms)
	{
		const int count = room.hasWorkingBoundary ? room.workingBoundary.size() : room.packedVertices.size();
		if(count > MaximumRecords || (room.roomType != 0x18 && count > 0xffff))
		{
			error = QStringLiteral("Room %1 has too many vertices for the legacy format.").arg(room.roomId);
			return false;
		}
		SavedRoom saved;
		saved.room = &room;
		if(!RoomVertices(room, saved.vertices, error))
			return false;
		// The original array reader drops empty rooms. Our inspector retains
		// them, but they are not part of the 2016 working design being saved.
		if(saved.vertices.isEmpty())
			continue;
		records += 1 + saved.vertices.size();
		byteCount += 11 + qint64(saved.vertices.size()) * 2;
		if(rooms.size() == 0xffff || records > MaximumRecords || byteCount > maximumBytes)
		{
			error = QStringLiteral("The design exceeds the legacy room count, record or file size limit.");
			return false;
		}
		rooms.append(std::move(saved));
	}

	QByteArray bytes;
	bytes.reserve(int(byteCount));
	QDataStream stream(&bytes, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_4_0);
	stream.setByteOrder(QDataStream::BigEndian);
	if(design.building)
	{
		// DnBuilding::Tx, AuData13 0x18015b850. Unlike .ship, building files
		// carry a two-byte envelope followed by the original building header.
		stream << quint8(0x1a) << quint8(1) << header.designId << name << header.architectId
			<< header.architectName << design.buildingIndustry << design.buildingModule
			<< header.state << design.publishedEmpires;
	}
	else
	{
		stream << header.designId << name << header.architectId << header.streamline << header.armor
			<< quint8(0x20 | header.state) << header.architectName << header.minimumTechLevel
			<< header.hullVolume << header.structure << header.techLevel;
		stream << quint32(header.firstPairs.size());
		for(const ModulePair& pair : header.firstPairs)
			stream << pair.first << pair.secondNormalized;
		for(quint16 slot : moduleSlots) stream << slot;
		stream << quint32(header.secondPairs.size());
		for(const ModulePair& pair : header.secondPairs)
			stream << pair.first << pair.secondNormalized;
	}
	stream << quint16(rooms.size());
	for(const SavedRoom& saved : rooms)
	{
		const Room& room = *saved.room;
		stream << room.designId << room.roomId << room.deck << room.roomType << room.roomSubtype
			<< quint16(saved.vertices.size());
		for(quint16 vertex : saved.vertices)
			stream << vertex;
	}
	stream << quint32(design.details.size());
	for(const Detail& detail : design.details)
		stream << detail.deck << detail.detailType << bool(detail.hiddenRaw != 0) << detail.detailId
			<< detail.designId << detail.packedXY << detail.roomId << detail.commodityIdNormalized;
	if(stream.status() != QDataStream::Ok || bytes.size() != byteCount)
	{
		error = QStringLiteral("Could not serialize the complete legacy design.");
		return false;
	}
	output = std::move(bytes);
	return true;
}

bool AuLegacyShip::Save(const QString& fileName, const Design& design, QString& error,
	const QString& fallbackName)
{
	Design prepared;
	if(!PrepareForSave(design, prepared, error))
		return false;
	QByteArray bytes;
	if(!Write(prepared, bytes, error, fallbackName))
		return false;
	QSaveFile file(fileName);
	file.setDirectWriteFallback(false);
	if(!file.open(QIODevice::WriteOnly))
	{
		error = QStringLiteral("Cannot open %1 for saving: %2").arg(fileName, file.errorString());
		return false;
	}
	if(file.write(bytes) != bytes.size())
	{
		error = QStringLiteral("Cannot write %1: %2").arg(fileName, file.errorString());
		file.cancelWriting();
		return false;
	}
	if(!file.commit())
	{
		error = QStringLiteral("Cannot replace %1: %2").arg(fileName, file.errorString());
		return false;
	}
	return true;
}
