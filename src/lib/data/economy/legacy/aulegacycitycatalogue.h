#ifndef AuLegacyCityCatalogue_H
#define AuLegacyCityCatalogue_H

#include "audata.h"
#include "dbcommodity.h"
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>
#include <array>

// These are Legacy catalogue identifiers, never modern manufacturing or industry
// identifiers. Conversion to a runtime commodity is explicit at the boundary.
namespace AuLegacyCity
{
template<class Tag, class Storage> struct Id
{
	Storage value = 0;
	constexpr Id() = default;
	explicit constexpr Id(Storage raw) : value(raw) {}
	constexpr bool IsNull() const { return value == 0; }
	friend constexpr bool operator==(Id a, Id b) { return a.value == b.value; }
	friend constexpr bool operator!=(Id a, Id b) { return !(a == b); }
};

struct CommodityTag;
struct IndustryTag;
struct ManufacturingProcessTag;
struct ConstructionProcessTag;
using CommodityId = Id<CommodityTag, quint16>;
using IndustryId = Id<IndustryTag, quint8>;
using ManufacturingProcessId = Id<ManufacturingProcessTag, quint16>;
using ConstructionProcessId = Id<ConstructionProcessTag, quint8>;

template<class Tag, class Storage>
inline uint qHash(Id<Tag, Storage> id, uint seed = 0) noexcept
{
	return ::qHash(id.value, seed);
}

struct RecordSource
{
	quint32 offset = 0;
	QByteArray bytes;
};

struct ShipDetailTypeRecord
{
	quint8 id = 0;
	std::array<QString, 4> names;
	quint8 bridge = 0;
	quint16 crew = 0;
	qint16 placement = 0;
	quint8 minimumTechnology = 0;
	quint16 mass = 0;
	RecordSource source;
};

struct ShipRoomTypeRecord
{
	quint32 id = 0;
	std::array<QString, 4> names;
	RecordSource source;
};

struct CommodityRecord
{
	CommodityId id;
	// descriptor retains the historical ID here; RuntimeCommodityDescriptors
	// contains separate copies with canonical runtime IDs.
	DbCommodity descriptor;
	// Absorption uses SQL percentages, including historical Energy/Kinetic entries.
	std::array<float, 17> armorAbsorb = {};
	std::array<float, 17> armorBlock = {};
	std::array<float, 4> modeAbsorb = {};
	std::array<float, 4> modeBlock = {};
	QString deprecatedDescription;
	RecordSource source;
	bool IsTechnology() const;
};

struct IndustryRecord
{
	IndustryId id;
	QString name;
	quint8 populationPerLevel = 0;
	qint8 employees = 0;
	quint8 storageTens = 0;
	quint8 capacitorTens = 0;
	quint8 buildingHitBase = 0; // Derived from the original lumber construction requirement.
	RecordSource source;
	quint16 StorageUnits() const { return quint16(storageTens) * 10; }
	quint16 CapacitorUnits() const { return quint16(capacitorTens) * 10; }
	bool IsPlaceholder() const { return name.isEmpty(); }
};

struct ManufacturingProcessRecord
{
	ManufacturingProcessId id;
	QString name;
	CommodityId commodity;
	IndustryId industry;
	quint16 quantity = 0;
	quint16 labor = 0;
	quint16 serializedLabor = 0;
	quint8 craft = 0;
	quint8 minimumTechnology = 0;
	bool isDefault = false;
	// The SQL nat value is authoritative, including the recovered exceptions.
	bool naturalResourceQuality = true;
	bool naturalFlagSerialized = false;
	RecordSource source;
};

struct ManufacturingComponentRecord
{
	quint16 id = 0;
	ManufacturingProcessId process;
	CommodityId commodity;
	quint16 quantity = 0;
	quint8 effect = 0;
	RecordSource source;
};

struct ConstructionProcessRecord
{
	ConstructionProcessId id;
	QString name;
	IndustryId industry;
	quint8 sitePreparation = 0;
	// Packed original stack-limit flags, not a literal number of layers.
	quint8 stackLimit = 0;
	quint8 terrain = 0;
	quint8 buildRadius = 0;
	RecordSource source;
};

struct ConstructionComponentRecord
{
	quint16 id = 0;
	ConstructionProcessId process;
	CommodityId commodity;
	quint8 quantity = 0;
	quint8 effect = 0;
	quint8 level = 0;
	RecordSource source;
};

// One current construction level. Zero quantity denotes a reusable tool.
// Commodity IDs and effects remain historical, just like the source records.
struct ConstructionRequirement
{
	CommodityId commodity;
	quint8 effect = 0;
	quint32 quantity = 0;
};

struct TableSource
{
	QString name;
	quint32 offset = 0;
	quint32 bytes = 0;
	quint32 records = 0;
	QByteArray sha256;
};
}

// A validated immutable catalogue. SQL and client payloads pass the same range,
// unique-ID, reference, cost, and runtime-identity validation before publication.
// Reset is permitted only after scene/UI consumers have released their pointers.
class AUDATA_DLL AuLegacyCityCatalogue
{
public:
	static constexpr quint16 RuntimeAliasFirst = 664;
	static constexpr quint16 RuntimeAliasLast = 741;
	static constexpr int MaximumSourceBytes = 16 * 1024 * 1024;

	static QString DataVersion();
	// Source bytes are a versioned SQL-row payload, never the historical GD cache.
	static bool Read(const QByteArray& source, AuLegacyCityCatalogue& output, QString& error);
	static bool LoadBuiltIn(QString& error);
	static bool InstallBuiltIn(const QByteArray& source, QString& error);
	// Only after existing scene/UI consumers have released catalogue pointers.
	static void ResetBuiltIn();
	static const AuLegacyCityCatalogue* BuiltIn();

	bool IsValid() const { return m_valid; }
	const QByteArray& SourceSha256() const { return m_sourceSha256; }
	const QByteArray& SourceBytes() const { return m_sourceBytes; }
	const QVector<AuLegacyCity::TableSource>& Tables() const { return m_tables; }

	const QVector<AuLegacyCity::ShipDetailTypeRecord>& ShipDetailTypes() const { return m_details; }
	const QVector<AuLegacyCity::ShipRoomTypeRecord>& ShipRoomTypes() const { return m_rooms; }
	const QVector<AuLegacyCity::CommodityRecord>& CommodityRecords() const { return m_commodities; }
	const QVector<AuLegacyCity::IndustryRecord>& IndustryRecords() const { return m_industries; }
	const QVector<AuLegacyCity::ManufacturingProcessRecord>& ManufacturingProcesses() const { return m_processes; }
	const QVector<AuLegacyCity::ManufacturingComponentRecord>& ManufacturingComponents() const { return m_components; }
	const QVector<AuLegacyCity::ConstructionProcessRecord>& ConstructionProcesses() const { return m_construction; }
	const QVector<AuLegacyCity::ConstructionComponentRecord>& ConstructionComponents() const { return m_constructionComponents; }

	const AuLegacyCity::CommodityRecord* Commodity(AuLegacyCity::CommodityId id) const;
	const AuLegacyCity::IndustryRecord* Industry(AuLegacyCity::IndustryId id) const;
	const AuLegacyCity::ManufacturingProcessRecord* ManufacturingProcess(AuLegacyCity::ManufacturingProcessId id) const;
	const AuLegacyCity::ConstructionProcessRecord* ConstructionProcess(AuLegacyCity::ConstructionProcessId id) const;
	QVector<AuLegacyCity::ManufacturingProcessId> ProcessesForIndustry(AuLegacyCity::IndustryId id) const;
	AuLegacyCity::ManufacturingProcessId DefaultManufacturingProcess(AuLegacyCity::IndustryId id) const;
	QVector<AuLegacyCity::ManufacturingComponentRecord> ComponentsForProcess(AuLegacyCity::ManufacturingProcessId id) const;
	QVector<AuLegacyCity::ConstructionComponentRecord> ComponentsForConstruction(AuLegacyCity::ConstructionProcessId id) const;
	const AuLegacyCity::ConstructionProcessRecord* ConstructionProcessForIndustry(AuLegacyCity::IndustryId id) const;
	int BuildingDesignMaximumDeck(AuLegacyCity::IndustryId industry, AuLegacyCity::CommodityId material) const;
	static int BuildingDesignMaximumDeck(AuLegacyCity::IndustryId industry, AuLegacyCity::CommodityId material,
		int stackLimit);
	// A count of industrial levels, distinct from the authored design's top deck.
	int ConstructionLevelLimit(AuLegacyCity::ConstructionProcessId process, AuLegacyCity::CommodityId material,
		bool flammableMaterialsAllowed = true) const;
	int ConstructionResourceLevelLimit(AuLegacyCity::ConstructionProcessId process,
		quint32 rocks, quint32 shrubs, quint32 trees) const;
	quint16 SitePreparationLabor(AuLegacyCity::ConstructionProcessId process, quint8 completedLevels,
		quint32 rocks, quint32 shrubs, quint32 trees) const;
	QVector<AuLegacyCity::ConstructionRequirement> ConstructionRequirements(AuLegacyCity::ConstructionProcessId process,
		AuLegacyCity::CommodityId material, quint8 environment, quint32 pavementQuantity = 0,
		quint32 materialMultiplier = 1) const;
	static quint32 RoadPavementQuantity(double lengthFeet, double widthFeet);
	static int BiodomeSegmentCount(double radiusFeet);
	static quint16 RoadPreparationLabor(AuLegacyCity::ConstructionProcessId process,
		double lengthFeet, double widthFeet, double meanAbsoluteTerrainDifference);

	quint16 RuntimeCommodityId(AuLegacyCity::CommodityId id) const;
	bool HistoricalCommodityId(quint16 runtimeId, AuLegacyCity::CommodityId& output) const;
	const AuLegacyCity::CommodityRecord* CommodityForRuntimeId(quint16 runtimeId) const;
	const DbCommodity* RuntimeCommodity(quint16 runtimeId) const;
	const QHash<quint16, DbCommodity>& RuntimeCommodityDescriptors() const { return m_runtimeDescriptors; }
	const QVector<quint16>& RuntimeCommodityIds() const { return m_runtimeIds; }
	quint8 RuntimeIndustryId(AuLegacyCity::IndustryId id) const;
	const AuLegacyCity::IndustryRecord* IndustryForRuntimeId(quint8 runtimeId) const;

private:
	bool Validate(QString& error);
	bool Finalize(QString& error);
	bool CalculateBaseCosts(QString& error);
	bool BuildCrosswalk(QString& error);
	bool m_valid = false;
	QByteArray m_sourceBytes;
	QByteArray m_sourceSha256;
	QVector<AuLegacyCity::TableSource> m_tables;
	QVector<AuLegacyCity::ShipDetailTypeRecord> m_details;
	QVector<AuLegacyCity::ShipRoomTypeRecord> m_rooms;
	QVector<AuLegacyCity::CommodityRecord> m_commodities;
	QVector<AuLegacyCity::IndustryRecord> m_industries;
	QVector<AuLegacyCity::ManufacturingProcessRecord> m_processes;
	QVector<AuLegacyCity::ManufacturingComponentRecord> m_components;
	QVector<AuLegacyCity::ConstructionProcessRecord> m_construction;
	QVector<AuLegacyCity::ConstructionComponentRecord> m_constructionComponents;
	QHash<quint16, int> m_commodityIndex;
	QHash<quint8, int> m_industryIndex;
	QHash<quint16, int> m_processIndex;
	QHash<quint8, int> m_constructionIndex;
	QHash<quint16, quint16> m_toRuntime;
	QHash<quint16, quint16> m_fromRuntime;
	QHash<quint16, DbCommodity> m_runtimeDescriptors;
	QVector<quint16> m_runtimeIds;
};

#endif // AuLegacyCityCatalogue_H
