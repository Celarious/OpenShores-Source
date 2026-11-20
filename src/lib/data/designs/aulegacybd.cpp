#include "aulegacybd.h"
#include "aulegacycitycatalogue.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <zlib.h>

static void InitializeLegacyBuildingResources()
{
	Q_INIT_RESOURCE(aulegacybdresources);
}

namespace AuLegacyBd
{
bool SelectableIndustry(int industry)
{
	return industry >= 3 && industry < 130 && industry != 10 && industry != 114 && !DesignerIndustry(industry).name.isEmpty();
}

// DbIndustry::DoesManufacture, AuData13 0x18002ff50, in the 2016 ID namespace.
bool DoesManufacture(int industry)
{
	return (industry >= 30 && industry <= 32) || industry == 34 || industry == 35 ||
		(industry >= 40 && industry <= 48) || industry == 50 || industry == 51 ||
		(industry >= 60 && industry <= 67) || (industry >= 70 && industry <= 89) || industry == 110 || industry == 125;
}

// The original refresh at EXE 14017bc60 uses DbIndustry population and
// storage plus two jump tables. Ordinary structural tools always remain.
bool BuildingRoomAllowed(int industry, int type)
{
	const auto record = DesignerIndustry(industry);
	const bool militarySystem = industry == 11 || industry == 12;
	const bool residential = record.population != 0;
	switch(type)
	{
	case 1:  return militarySystem || industry == 128;
	case 2:  return militarySystem || industry == 111 || industry == 127;
	case 3:  return industry == 124 || industry == 112 || industry == 113 || industry == 30 || industry == 45;
	case 5: case 9: return record.storage || (industry >= 60 && industry <= 67);
	case 6: case 11: case 13: case 15: case 22: case 23: case 24: return true;
	case 8:
		return DoesManufacture(industry) || industry == 3 || industry == 103 || industry == 104;
	case 10: case 14:
		return residential || militarySystem || industry == 102 || industry == 105 || industry == 111
			|| industry == 112 || industry == 113 || industry == 120 || industry == 121 || industry == 122
			|| industry == 125 || (type == 14 && (industry == 100 || industry == 101 || industry == 123 || industry == 124));
	case 16: case 20: return industry == 111;
	case 17: return industry == 12;
	case 18: return industry == 121;
	case 19: return residential;
	case 21: return industry == 11;
	default: return false;
	}
}

bool BuildingDetailAllowed(int industry, int type)
{
	const bool militarySystem = industry == 11 || industry == 12;
	switch(type)
	{
	case 1: return industry == 127;
	case 2: case 25: case 26: case 27: case 28: case 29: case 30: case 31:
	case 65: case 66: case 67: case 68: case 69: case 70: case 71: case 72:
		return DesignerIndustry(industry).population || militarySystem || industry == 128;
	case 4: case 11: return industry == 121;
	case 7: case 99: return industry == 11;
	case 15: case 19: case 20: return industry == 111;
	case 17: return industry == 12;
	case 21: case 22: case 33: case 34: case 35: case 36: case 37: case 38: case 39: case 40:
		return industry == 128;
	case 49: case 51: case 54: case 84: case 85: case 86: case 87: case 88: case 89: case 90:
		return militarySystem || industry == 111 || (industry >= 121 && industry <= 125) || industry == 127 || industry == 128;
	case 74: case 75: return industry == 102 || industry == 105;
	case 3: case 5: case 8: case 9: case 50: case 52: case 53: case 55: case 56: case 142:
		return true;
	default:
		return (type >= 77 && type <= 83) || (type >= 91 && type <= 97) || (type >= 101 && type <= 140);
	}
}

namespace
{
class Reader
{
public:
	Reader(const QByteArray& bytes, QString& error) : m_Bytes(bytes), m_Error(error) {}
	int Offset() const { return m_Offset; }
	int Remaining() const { return m_Bytes.size() - m_Offset; }
	bool Fail(const QString& text)
	{
		m_Error = QStringLiteral("building offset 0x%1: %2").arg(m_Offset, 0, 16).arg(text);
		return false;
	}
	bool Need(quint32 count)
	{
		return count <= quint32(Remaining()) || Fail(QStringLiteral("truncated record"));
	}
	bool U8(quint8& value)
	{
		if(!Need(1)) return false;
		value = quint8(m_Bytes.at(m_Offset++)); return true;
	}
	bool I8(qint8& value)
	{
		quint8 raw = 0; if(!U8(raw)) return false;
		value = qint8(raw < 128 ? int(raw) : int(raw) - 256); return true;
	}
	bool U16(quint16& value)
	{
		if(!Need(2)) return false;
		value = (quint16(quint8(m_Bytes.at(m_Offset))) << 8) | quint8(m_Bytes.at(m_Offset + 1));
		m_Offset += 2; return true;
	}
	bool U32(quint32& value)
	{
		if(!Need(4)) return false;
		value = (quint32(quint8(m_Bytes.at(m_Offset))) << 24)
			| (quint32(quint8(m_Bytes.at(m_Offset + 1))) << 16)
			| (quint32(quint8(m_Bytes.at(m_Offset + 2))) << 8) | quint8(m_Bytes.at(m_Offset + 3));
		m_Offset += 4; return true;
	}
	bool String(QString& value)
	{
		quint32 length = 0; if(!U32(length)) return false;
		if(length == 0xffffffffU) { value = QString(); return true; }
		if(length > 64 * 1024 || (length & 1U)) return Fail(QStringLiteral("invalid string length"));
		if(!Need(length)) return false;
		value = QStringLiteral(""); value.resize(int(length / 2));
		for(int i = 0; i < value.size(); ++i)
		{
			quint16 unit = 0; if(!U16(unit)) return false; value[i] = QChar(unit);
		}
		return true;
	}
	bool Count(quint32 count, int minimumBytes)
	{
		if(count > quint32(Remaining() / minimumBytes) || count > quint32(m_RecordBudget))
			return Fail(QStringLiteral("record count exceeds bounded input"));
		m_RecordBudget -= int(count); return true;
	}
private:
	const QByteArray& m_Bytes;
	QString& m_Error;
	int m_Offset = 0;
	int m_RecordBudget = MaximumRecords;
};

quint16 NormalizeModule(quint16 raw)
{
	switch(raw)
	{
	case 0xfd: return 0xfc;
	case 0xff: return 0xfe;
	case 0xf9: return 0xf8;
	case 0xfb: return 0xfa;
	default: return raw;
	}
}

QByteArray Hash(const QByteArray& bytes)
{
	return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}
Catalog& BuiltInCatalog()
{
	static Catalog catalog;
	return catalog;
}
QMutex& BuiltInMutex()
{
	static QMutex mutex;
	return mutex;
}
}

bool Uncompress(const QByteArray& compressed, QByteArray& output, QString& error)
{
	if(compressed.size() < 6 || compressed.size() > MaximumCacheBytes)
	{
		error = QStringLiteral("Invalid bounded qCompress input size."); return false;
	}
	Reader reader(compressed, error);
	quint32 length = 0;
	if(!reader.U32(length) || !length || length > quint32(MaximumPayloadBytes))
	{
		error = QStringLiteral("qCompress output exceeds the building payload limit."); return false;
	}
	QByteArray decoded(int(length), char(0));
	z_stream stream = {};
	stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.constData() + 4));
	stream.avail_in = uInt(compressed.size() - 4);
	stream.next_out = reinterpret_cast<Bytef*>(decoded.data());
	stream.avail_out = uInt(length);
	if(inflateInit(&stream) != Z_OK)
	{
		error = QStringLiteral("Unable to initialize building decompression."); return false;
	}
	const int result = inflate(&stream, Z_FINISH);
	const bool valid = result == Z_STREAM_END && stream.total_out == length && !stream.avail_in;
	inflateEnd(&stream);
	if(!valid)
	{
		error = QStringLiteral("Building compression is truncated, oversized or contains trailing data."); return false;
	}
	output = std::move(decoded); error.clear(); return true;
}

float FixedTerrainLevelRadius(int industry)
{
	// DnBuilding::TerrainLevelRadius, AuData13 0x180046d70.
	switch(industry)
	{
	case 20: return 26.24672f;
	case 30: case 32: return 42.650917f;
	case 31: case 50: case 51: return 32.8084f;
	case 66: case 67: return 49.2126f;
	case 113: return 65.6168f;
	case 114: return 9.84252f;
	default: return 0;
	}
}

DesignerIndustryInfo DesignerIndustry(int industry)
{
	using namespace AuLegacyCity;
	DesignerIndustryInfo info;
	if(industry < 0 || industry >= 130) return info;
	const IndustryId id{quint8(industry)};
	const CommodityId vulcium(145);
	if(const AuLegacyCityCatalogue* catalogue = AuLegacyCityCatalogue::BuiltIn())
	{
		if(const auto* record = catalogue->Industry(id))
		{
			info.name = record->name;
			info.population = record->populationPerLevel;
			info.storage = record->StorageUnits();
			info.capacitor = record->CapacitorUnits();
			info.employees = record->employees;
		}
		if(const auto* recipe = catalogue->ConstructionProcessForIndustry(id))
		{
			info.siteRadius = recipe->buildRadius * 3.28083989501312;
			info.stackLimit = recipe->stackLimit;
		}
		info.maximumDeck = catalogue->BuildingDesignMaximumDeck(id, vulcium);
		return info;
	}

	// Only the standalone editor uses these two tables from the bundled,
	// immutable 0x18/0x3a GD. Their boundaries are in catalogue-manifest.json.
	// Online game data continues to come from the negotiated SQL catalogue.
	static const QVector<DesignerIndustryInfo> original = []() {
		InitializeLegacyBuildingResources();
		QFile file(QStringLiteral(":/legacy/cities/GD2016"));
		QVector<DesignerIndustryInfo> rows(130);
		if(!file.open(QIODevice::ReadOnly) || !file.seek(153107)) return QVector<DesignerIndustryInfo>();
		QDataStream in(&file);
		in.setVersion(QDataStream::Qt_4_0);
		quint16 count;
		in >> count;
		if(count != 130) return QVector<DesignerIndustryInfo>();
		for(int i = 0; i < count; ++i)
		{
			quint8 capacitor, industryId, population, storage;
			qint8 employees;
			QString name;
			in >> capacitor >> industryId >> name >> population >> employees >> storage;
			if(in.status() != QDataStream::Ok || industryId >= rows.size()) return QVector<DesignerIndustryInfo>();
			rows[industryId].name = name;
			rows[industryId].population = population;
			rows[industryId].storage = storage * 10;
			rows[industryId].capacitor = capacitor * 10;
			rows[industryId].employees = employees;
		}
		if(!file.seek(206135)) return QVector<DesignerIndustryInfo>();
		in >> count;
		if(count != 82) return QVector<DesignerIndustryInfo>();
		QSet<int> seen;
		for(int i = 0; i < count; ++i)
		{
			quint8 process, industryId, preparation, stack, terrain, radius;
			QString name;
			in >> process >> name >> industryId >> preparation >> stack >> terrain >> radius;
			if(in.status() != QDataStream::Ok || industryId >= rows.size()) return QVector<DesignerIndustryInfo>();
			if(seen.contains(industryId)) continue;
			seen.insert(industryId);
			rows[industryId].stackLimit = stack;
			rows[industryId].maximumDeck = AuLegacyCityCatalogue::BuildingDesignMaximumDeck(
				IndustryId(industryId), CommodityId(145), stack);
			rows[industryId].siteRadius = radius * 3.28083989501312;
		}
		return rows;
	}();
	return original.value(industry);
}

double DesignerSiteRadius(int industry, int deck)
{
	const double fixed = FixedTerrainLevelRadius(industry);
	if(industry == 50 && deck < -1)
		return std::min(DesignerIndustry(industry).siteRadius, fixed + (-1 - deck) * AuLegacyShip::GridSpacing);
	return fixed ? fixed : DesignerIndustry(industry).siteRadius;
}

int MinimumDesignDeck(int industry)
{
	switch(industry)
	{
	case 11: case 12: return -5;
	case 50: return -8;
	case 51: return -1;
	default: return 0;
	}
}

bool ValidDesignerPoint(int industry, int deck, const QPointF& point)
{
	// AuData13 0x180159bd0 restricts coastal sites to the landward half.
	const bool coastal = industry == 34 || industry == 62 || industry == 85;
	const double radius = DesignerSiteRadius(industry, deck);
	return std::abs(point.x()) <= radius && std::abs(point.y()) <= radius && (!coastal || point.y() <= 0);
}

bool ReadPayload(const QByteArray& payload, Design& output, QString& error)
{
	if(payload.isEmpty() || payload.size() > MaximumPayloadBytes)
	{
		error = QStringLiteral("Building payload exceeds its size limit."); return false;
	}
	Reader reader(payload, error);
	Design candidate;
	Header& h = candidate.header;
	quint32 count = 0;
	if(!reader.U32(h.designId) || !reader.String(h.name) || !reader.U32(h.architectId)
		|| !reader.String(h.architectName) || !reader.U8(h.industryId) || !reader.U16(h.moduleRaw)
		|| !reader.U8(h.state) || !reader.U32(count) || !reader.Count(count, 4)) return false;
	if(!h.designId || h.industryId > 130 || h.state > 7)
		return reader.Fail(QStringLiteral("invalid original building header"));
	h.module = NormalizeModule(h.moduleRaw);
	h.publishedEmpires.reserve(int(count));
	for(quint32 i = 0; i < count; ++i)
	{
		quint32 id = 0; if(!reader.U32(id)) return false; h.publishedEmpires.append(id);
	}
	quint16 roomCount = 0;
	if(!reader.U16(roomCount) || !reader.Count(roomCount, 11)) return false;
	candidate.rooms.reserve(roomCount);
	QSet<quint16> roomIds;
	for(int i = 0; i < roomCount; ++i)
	{
		AuLegacyShip::Room room; room.streamOffset = reader.Offset();
		quint16 vertices = 0;
		if(!reader.U32(room.designId) || !reader.U16(room.roomId) || !reader.I8(room.deck)
			|| !reader.U8(room.roomType) || !reader.U8(room.roomSubtype) || !reader.U16(vertices)
			|| !reader.Count(vertices, 2)) return false;
		if(room.designId != h.designId || !room.roomId || roomIds.contains(room.roomId)
			|| vertices < 3 || room.roomType > 24 || room.deck < -127)
			return reader.Fail(QStringLiteral("invalid room identity or boundary"));
		room.packedVertices.reserve(vertices);
		for(int v = 0; v < vertices; ++v)
		{
			quint16 point = 0; if(!reader.U16(point)) return false; room.packedVertices.append(point);
		}
		// Preserve the three stored ellipse controls. RoomBoundary expands them
		// only for analysis/rendering, exactly as the original reader did.
		roomIds.insert(room.roomId); candidate.rooms.append(std::move(room));
	}
	if(!reader.U32(count) || !reader.Count(count, 17)) return false;
	candidate.details.reserve(int(count));
	QSet<quint32> detailIds;
	for(quint32 i = 0; i < count; ++i)
	{
		AuLegacyShip::Detail detail; detail.streamOffset = reader.Offset();
		if(!reader.I8(detail.deck) || !reader.U8(detail.detailType) || !reader.U8(detail.hiddenRaw)
			|| !reader.U32(detail.detailId) || !reader.U32(detail.designId) || !reader.U16(detail.packedXY)
			|| !reader.U16(detail.roomId) || !reader.U16(detail.commodityIdRaw)) return false;
		if(detail.designId != h.designId || !detail.detailId || detailIds.contains(detail.detailId)
			|| detail.hiddenRaw > 1 || (detail.roomId && !roomIds.contains(detail.roomId)))
			return reader.Fail(QStringLiteral("invalid detail identity or room reference"));
		// Cross-deck references occur in the recovered originals and are valid.
		detail.commodityIdNormalized = detail.commodityIdRaw == 0xf9 ? 0xf8
			: detail.commodityIdRaw == 0xfb ? 0xfa : detail.commodityIdRaw;
		detailIds.insert(detail.detailId); candidate.details.append(detail);
	}
	if(reader.Remaining()) return reader.Fail(QStringLiteral("trailing building payload"));
	candidate.bytesConsumed = reader.Offset(); candidate.payloadHash = Hash(payload);
	output = std::move(candidate); error.clear(); return true;
}

bool ReadCache(const QByteArray& source, Design& output, QString& error)
{
	if(source.size() < 21 || source.size() > MaximumCacheBytes
		|| quint8(source.at(0)) != 0x1b || quint8(source.at(1)) != 0x11)
	{
		error = QStringLiteral("Not a bounded original 2016 building cache (1b11)."); return false;
	}
	// Fixed Qt_4_0 date/time envelope is decoded by Qt at version 7 only after
	// its nine bytes and the compressed QByteArray length have been bounded.
	Reader reader(source, error);
	quint8 discard = 0, timeSpec = 0;
	quint32 julian = 0, milliseconds = 0, length = 0;
	if(!reader.U8(discard) || !reader.U8(discard) || !reader.U32(julian)
		|| !reader.U32(milliseconds) || !reader.U8(timeSpec) || !reader.U32(length)) return false;
	if(length != quint32(reader.Remaining()) || timeSpec > 3)
		return reader.Fail(QStringLiteral("invalid original cache envelope"));
	QByteArray payload;
	if(!Uncompress(source.mid(reader.Offset()), payload, error)) return false;
	Design candidate;
	if(!ReadPayload(payload, candidate, error)) return false;
	QByteArray dateBytes = source.mid(2, 9);
	QDataStream dateStream(dateBytes);
	dateStream.setVersion(QDataStream::Qt_4_0);
	dateStream >> candidate.cacheTime;
	if(dateStream.status() != QDataStream::Ok || !candidate.cacheTime.isValid())
		return reader.Fail(QStringLiteral("invalid original cache timestamp"));
	candidate.cacheTime.setTimeSpec(Qt::UTC);
	candidate.sourceHash = Hash(source);
	output = std::move(candidate); error.clear(); return true;
}

bool ReadDesignFile(const QByteArray& bytes, AuLegacyShip::Design& output, QString& error)
{
	// The standalone 1a01 file contains precisely the uncompressed cache payload.
	if(bytes.size() < 2 || bytes.size() - 2 > MaximumPayloadBytes
		|| quint8(bytes.at(0)) != 0x1a || quint8(bytes.at(1)) != 1)
	{
		error = QStringLiteral("Expected a 2016 building design file (1a01).");
		return false;
	}
	Design parsed;
	if(!ReadPayload(bytes.mid(2), parsed, error)) return false;
	output = parsed.AuthoringDesign();
	output.bytesConsumed = bytes.size();
	return true;
}

bool LoadCache(const QString& fileName, Design& output, QString& error)
{
	QFile file(fileName);
	if(!file.open(QIODevice::ReadOnly)) { error = file.errorString(); return false; }
	if(file.size() > MaximumCacheBytes) { error = QStringLiteral("Building cache exceeds its size limit."); return false; }
	return ReadCache(file.readAll(), output, error);
}

int Design::MinimumDeck() const
{
	int result = 0;
	if(!rooms.isEmpty()) result = rooms.first().deck;
	for(const auto& room : rooms) result = std::min(result, int(room.deck));
	return result;
}
int Design::MaximumDeck() const
{
	int result = 0;
	if(!rooms.isEmpty()) result = rooms.first().deck;
	for(const auto& room : rooms) result = std::max(result, int(room.deck));
	return result;
}
AuLegacyShip::Design Design::AuthoringDesign() const
{
	AuLegacyShip::Design result;
	result.building = true;
	result.buildingIndustry = header.industryId;
	result.buildingModule = header.module;
	result.publishedEmpires = header.publishedEmpires;
	result.header.designId = header.designId; result.header.name = header.name;
	result.header.architectId = header.architectId; result.header.architectName = header.architectName;
	result.header.state = header.state; result.rooms = rooms; result.details = details;
	return result;
}

bool Catalog::LoadDefaults(const QString& root, QString& error)
{
	QFile manifestFile(root + QStringLiteral("/manifest.json"));
	if(!manifestFile.open(QIODevice::ReadOnly)) { error = manifestFile.errorString(); return false; }
	if(manifestFile.size() > 256 * 1024) { error = QStringLiteral("Building manifest is oversized."); return false; }
	QJsonParseError problem;
	const QJsonDocument document = QJsonDocument::fromJson(manifestFile.readAll(), &problem);
	const QJsonArray entries = document.object().value(QStringLiteral("designs")).toArray();
	if(problem.error != QJsonParseError::NoError || entries.size() != 74)
	{
		error = QStringLiteral("The original public building catalog must contain 74 hash-verified designs."); return false;
	}
	QMap<quint32, QSharedPointer<const Design>> candidate;
	for(const QJsonValue& value : entries)
	{
		const QJsonObject entry = value.toObject(); const int id = entry.value(QStringLiteral("id")).toInt();
		if(id < 1 || id > 74 || candidate.contains(quint32(id)))
		{
			error = QStringLiteral("Invalid public building catalog ID."); return false;
		}
		Design design;
		if(!LoadCache(root + QStringLiteral("/designs/B%1").arg(id), design, error)) return false;
		const QByteArray expectedHash = QByteArray::fromHex(entry.value(QStringLiteral("sha256")).toString().toLatin1());
		if(expectedHash.size() != 32 || design.sourceHash != expectedHash || design.header.designId != quint32(id)
			|| !design.IsPublicDefault() || design.header.industryId != entry.value(QStringLiteral("industry_id")).toInt())
		{
			error = QStringLiteral("Public building B%1 failed its original hash or identity check.").arg(id); return false;
		}
		candidate.insert(quint32(id), QSharedPointer<const Design>(new Design(std::move(design))));
	}
	QCryptographicHash hash(QCryptographicHash::Sha256);
	static const char domain[] = "2016 public building source catalog";
	hash.addData(domain, sizeof(domain));
	for(auto it = candidate.cbegin(); it != candidate.cend(); ++it)
	{
		const quint32 id = it.key();
		const char encoded[] = {char(id >> 24), char(id >> 16), char(id >> 8), char(id)};
		hash.addData(encoded, 4); hash.addData(it.value()->sourceHash);
	}
	if(hash.result().toHex() != SourceSha256())
	{
		error = QStringLiteral("The bundled building sources do not match the recovered 2016 catalog."); return false;
	}
	m_Designs = std::move(candidate); error.clear(); return true;
}
bool Catalog::LoadBuiltIn(QString& error)
{
	InitializeLegacyBuildingResources();
	QMutexLocker lock(&BuiltInMutex());
	if(BuiltInCatalog().Count() == 74) { error.clear(); return true; }
	return BuiltInCatalog().LoadDefaults(QStringLiteral(":/legacy/cities"), error);
}
const Catalog& Catalog::BuiltIn() { return BuiltInCatalog(); }
QByteArray Catalog::SourceSha256()
{
	return QByteArrayLiteral("7b55d8b146a9bab311cf0e063a0373bda707a792f6fcbee7e8d42e8cb97d706d");
}
QSharedPointer<const Design> Catalog::Find(quint32 designId) const { return m_Designs.value(designId); }
QVector<quint32> Catalog::DesignsForIndustry(quint8 industryId) const
{
	QVector<quint32> result;
	for(auto i = m_Designs.cbegin(); i != m_Designs.cend(); ++i)
		if(i.value()->header.industryId == industryId) result.append(i.key());
	return result;
}
quint32 Catalog::DefaultForIndustry(quint8 industryId) const
{
	const QVector<quint32> ids = DesignsForIndustry(industryId);
	return ids.isEmpty() ? 0 : ids.first();
}
}
