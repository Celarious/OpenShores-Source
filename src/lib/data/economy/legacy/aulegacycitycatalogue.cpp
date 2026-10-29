#include "aulegacycitycatalogue.h"

#include <QCryptographicHash>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QResource>
#include <QSet>
#include <QSharedPointer>
#include <algorithm>
#include <cmath>
#include <cstring>

// Q_INIT_RESOURCE must be outside a namespace. This reference also keeps the
// resource object in the static AuData library from being discarded by linking.
static void InitLegacyCityCatalogueResource()
{
	Q_INIT_RESOURCE(aulegacycitycatalogue);
}

namespace
{
using namespace AuLegacyCity;

class Reader
{
public:
	Reader(const QByteArray& source, QString& error) : m_source(source), m_error(error) {}
	int Offset() const { return m_offset; }
	int Remaining() const { return m_source.size() - m_offset; }
	const QByteArray& Source() const { return m_source; }

	bool Fail(const QString& message)
	{
		m_error = QStringLiteral("Legacy GD offset 0x%1: %2").arg(m_offset, 0, 16).arg(message);
		return false;
	}

	bool Need(quint32 bytes)
	{
		return bytes <= quint32(Remaining()) || Fail(QStringLiteral("truncated record"));
	}

	bool U8(quint8& value)
	{
		if(!Need(1)) return false;
		value = quint8(m_source.at(m_offset++));
		return true;
	}

	bool I8(qint8& value)
	{
		quint8 raw = 0;
		if(!U8(raw)) return false;
		value = qint8(raw >= 128 ? int(raw) - 256 : int(raw));
		return true;
	}

	bool U16(quint16& value)
	{
		if(!Need(2)) return false;
		value = quint16((quint16(quint8(m_source.at(m_offset))) << 8)
			| quint8(m_source.at(m_offset + 1)));
		m_offset += 2;
		return true;
	}

	bool I16(qint16& value)
	{
		quint16 raw = 0;
		if(!U16(raw)) return false;
		value = qint16(raw >= 32768 ? int(raw) - 65536 : int(raw));
		return true;
	}

	bool U32(quint32& value)
	{
		if(!Need(4)) return false;
		value = 0;
		for(int index = 0; index < 4; ++index)
			value = (value << 8) | quint8(m_source.at(m_offset++));
		return true;
	}

	bool Float(float& value)
	{
		quint32 raw = 0;
		if(!U32(raw)) return false;
		static_assert(sizeof(value) == sizeof(raw), "GD requires an IEEE 32-bit float");
		std::memcpy(&value, &raw, sizeof(value));
		return std::isfinite(value) || Fail(QStringLiteral("non-finite float"));
	}

	bool Double(double& value)
	{
		if(!Need(8)) return false;
		quint64 raw = 0;
		for(int index = 0; index < 8; ++index)
			raw = (raw << 8) | quint8(m_source.at(m_offset++));
		static_assert(sizeof(value) == sizeof(raw), "GD requires an IEEE 64-bit double");
		std::memcpy(&value, &raw, sizeof(value));
		return std::isfinite(value) || Fail(QStringLiteral("non-finite double"));
	}

	bool String(QString& value)
	{
		quint32 bytes = 0;
		if(!U32(bytes)) return false;
		if(bytes == 0xffffffffU)
		{
			value = QString();
			return true;
		}
		if(bytes > 65536 || (bytes & 1U))
			return Fail(QStringLiteral("invalid or oversized UTF-16 string"));
		if(!Need(bytes)) return false;
		value = QStringLiteral("");
		value.resize(int(bytes / 2));
		for(int index = 0; index < value.size(); ++index)
		{
			quint16 unit = 0;
			if(!U16(unit)) return false;
			value[index] = QChar(unit);
		}
		return true;
	}

private:
	const QByteArray& m_source;
	QString& m_error;
	int m_offset = 0;
};

template<class Record, class Decode>
bool ReadTable(Reader& reader, QVector<Record>& records, QVector<TableSource>& tables,
		const QString& name, quint32 expectedCount, bool wideCount, Decode decode)
{
	const int start = reader.Offset();
	quint32 count = 0;
	if(wideCount)
	{
		if(!reader.U32(count)) return false;
	}
	else
	{
		quint16 narrowCount = 0;
		if(!reader.U16(narrowCount)) return false;
		count = narrowCount;
	}
	// Exact versioned counts prevent malformed lengths from allocating memory.
	if(count != expectedCount)
		return reader.Fail(QStringLiteral("%1 has %2 records; expected %3")
			.arg(name).arg(count).arg(expectedCount));
	records.reserve(int(count));
	for(quint32 index = 0; index < count; ++index)
	{
		Record record;
		const int recordStart = reader.Offset();
		if(!decode(reader, record)) return false;
		record.source.offset = quint32(recordStart);
		record.source.bytes = reader.Source().mid(recordStart, reader.Offset() - recordStart);
		records.append(record);
	}
	TableSource table;
	table.name = name;
	table.offset = quint32(start);
	table.bytes = quint32(reader.Offset() - start);
	table.records = count;
	table.sha256 = QCryptographicHash::hash(reader.Source().mid(start, int(table.bytes)),
		QCryptographicHash::Sha256).toHex();
	tables.append(table);
	return true;
}

struct WeaponEffect
{
	quint8 effect = 0;
	quint16 damage = 0;
	quint8 modifier = 0;
	quint16 radius = 0;
	quint8 armorPiercing = 0;
	quint16 strikeThrough = 0;
};

struct Weapon
{
	quint8 mode = 0;
	quint16 range = 0;
	std::array<WeaponEffect, 2> effects;
};

bool ReadWeapon(Reader& reader, Weapon& weapon)
{
	if(!reader.U8(weapon.mode) || !reader.U16(weapon.range)) return false;
	for(WeaponEffect& effect : weapon.effects)
		if(!reader.U8(effect.effect) || !reader.U16(effect.damage) || !reader.U8(effect.modifier)
			|| !reader.U16(effect.radius) || !reader.U8(effect.armorPiercing)
			|| !reader.U16(effect.strikeThrough)) return false;
	return true;
}

class CommodityDescriptor : public DbCommodity
{
public:
	void SetArmor(const CommodityRecord& record);
	void SetArmorSpecialty(quint8 specialty) { m_armorSpec = specialty; }
};

void CommodityDescriptor::SetArmor(const CommodityRecord& record)
{
	// Energy and kinetic entries at 6 and 9 have no modern member. The original
	// bytes remain in record.armorAbsorb/armorBlock and record.source.
	std::array<float*, 17> absorb = {{&m_ArmorEffectAbsorbAcid,
		&m_ArmorEffectAbsorbBludgeon, &m_ArmorEffectAbsorbCold,
		&m_ArmorEffectAbsorbCrush, &m_ArmorEffectAbsorbDisease,
		&m_ArmorEffectAbsorbElectric, nullptr, &m_ArmorEffectAbsorbHeat,
		&m_ArmorEffectAbsorbImplant, nullptr, &m_ArmorEffectAbsorbPierce,
		&m_ArmorEffectAbsorbPoison, &m_ArmorEffectAbsorbRadiation,
		&m_ArmorEffectAbsorbSlash, &m_ArmorEffectAbsorbSonic,
		&m_ArmorEffectAbsorbStun, &m_ArmorEffectAbsorbSuffocate}};
	std::array<float*, 17> block = {{&m_ArmorEffectBlockAcid,
		&m_ArmorEffectBlockBludgeon, &m_ArmorEffectBlockCold,
		&m_ArmorEffectBlockCrush, &m_ArmorEffectBlockDisease,
		&m_ArmorEffectBlockElectric, nullptr, &m_ArmorEffectBlockHeat,
		&m_ArmorEffectBlockImplant, nullptr, &m_ArmorEffectBlockPierce,
		&m_ArmorEffectBlockPoison, &m_ArmorEffectBlockRadiation,
		&m_ArmorEffectBlockSlash, &m_ArmorEffectBlockSonic,
		&m_ArmorEffectBlockStun, &m_ArmorEffectBlockSuffocate}};
	for(size_t index = 0; index < absorb.size(); ++index)
	{
		if(absorb[index]) *absorb[index] = float(record.armorAbsorb[index]) / 200.f;
		if(block[index]) *block[index] = float(record.armorBlock[index]);
	}
	m_ArmorModeAbsorbArea = record.modeAbsorb[0] / 200.f;
	m_ArmorModeAbsorbContact = record.modeAbsorb[1] / 200.f;
	m_ArmorModeAbsorbProjectile = record.modeAbsorb[2] / 200.f;
	m_ArmorModeAbsorbPsionic = record.modeAbsorb[3] / 200.f;
	m_ArmorModeBlockArea = record.modeBlock[0];
	m_ArmorModeBlockContact = record.modeBlock[1];
	m_ArmorModeBlockProjectile = record.modeBlock[2];
	m_ArmorModeBlockPsionic = record.modeBlock[3];
}

bool ReadCommodity(Reader& reader, CommodityRecord& record)
{
	CommodityDescriptor descriptor;
	quint8 ammoType1 = 0, ammoType2 = 0;
	if(!reader.U16(record.id.value) || !reader.U8(descriptor.m_ammo1Qty)
		|| !reader.U8(ammoType1) || !reader.U8(descriptor.m_ammo2Qty)
		|| !reader.U8(ammoType2)) return false;
	descriptor.m_id = COMMODITYID(record.id.value);
	descriptor.m_ammo1Type = AMMOTYPE(ammoType1);
	descriptor.m_ammo2Type = AMMOTYPE(ammoType2);
	for(quint8& value : record.armorAbsorb) if(!reader.U8(value)) return false;
	for(quint8& value : record.armorBlock) if(!reader.U8(value)) return false;
	for(quint8& value : record.modeAbsorb) if(!reader.U8(value)) return false;
	for(quint8& value : record.modeBlock) if(!reader.U8(value)) return false;
	descriptor.SetArmor(record);
	quint8 armorSpecialty = 0, capacityWidth = 0, capacityHeight = 0;
	if(!reader.U8(armorSpecialty) || !reader.U32(descriptor.m_bitsWear)
		|| !reader.U8(capacityWidth) || !reader.U8(capacityHeight)
		|| !reader.U32(descriptor.m_combits) || !reader.String(record.deprecatedDescription)
		|| !reader.U8(descriptor.m_food) || !reader.I8(descriptor.m_health)
		|| !reader.String(descriptor.m_iconPath) || !reader.String(descriptor.m_modelPath)
		|| !reader.Float(descriptor.m_insertHeight)) return false;
	descriptor.SetArmorSpecialty(armorSpecialty);
	descriptor.m_capacity = QSize(capacityWidth, capacityHeight);
	double upX = 0, upY = 0, upZ = 0;
	quint8 width = 0, height = 0;
	if(!reader.Double(upX) || !reader.Double(upY) || !reader.Double(upZ)
		|| !reader.U8(descriptor.m_light) || !reader.String(descriptor.m_name)
		|| !reader.U8(descriptor.m_newcond) || !reader.U8(width) || !reader.U8(height)
		|| !reader.U8(descriptor.m_stackLimit)) return false;
	descriptor.m_UpVector = AuPoint(upX, upY, upZ);
	descriptor.m_size = QSize(width, height);
	Weapon first, second;
	if(!ReadWeapon(reader, first) || !ReadWeapon(reader, second)
		|| !reader.Float(descriptor.m_wt)) return false;
	descriptor.m_wpn1Mode = WEAPONMODE(first.mode);
	descriptor.m_wpn1Range = first.range;
	descriptor.m_wpn1Effect1 = WEAPONEFFECT(first.effects[0].effect);
	descriptor.m_wpn1Damage1 = first.effects[0].damage;
	descriptor.m_wpn1DamageMod1 = first.effects[0].modifier;
	descriptor.m_wpn1Radius1 = first.effects[0].radius;
	descriptor.m_wpn1AP1 = first.effects[0].armorPiercing;
	descriptor.m_wpn1ST1 = first.effects[0].strikeThrough;
	descriptor.m_wpn1Effect2 = WEAPONEFFECT(first.effects[1].effect);
	descriptor.m_wpn1Damage2 = first.effects[1].damage;
	descriptor.m_wpn1DamageMod2 = first.effects[1].modifier;
	descriptor.m_wpn1Radius2 = first.effects[1].radius;
	descriptor.m_wpn1AP2 = first.effects[1].armorPiercing;
	descriptor.m_wpn1ST2 = first.effects[1].strikeThrough;
	descriptor.m_wpn2Mode = WEAPONMODE(second.mode);
	descriptor.m_wpn2Range = second.range;
	descriptor.m_wpn2Effect1 = WEAPONEFFECT(second.effects[0].effect);
	descriptor.m_wpn2Damage1 = second.effects[0].damage;
	descriptor.m_wpn2DamageMod1 = second.effects[0].modifier;
	descriptor.m_wpn2Radius1 = second.effects[0].radius;
	descriptor.m_wpn2AP1 = second.effects[0].armorPiercing;
	descriptor.m_wpn2ST1 = second.effects[0].strikeThrough;
	descriptor.m_wpn2Effect2 = WEAPONEFFECT(second.effects[1].effect);
	descriptor.m_wpn2Damage2 = second.effects[1].damage;
	descriptor.m_wpn2DamageMod2 = second.effects[1].modifier;
	descriptor.m_wpn2Radius2 = second.effects[1].radius;
	descriptor.m_wpn2AP2 = second.effects[1].armorPiercing;
	descriptor.m_wpn2ST2 = second.effects[1].strikeThrough;
	record.descriptor = descriptor;
	return true;
}

bool IsRuntimeAlias(quint16 id)
{
	return (id >= 158 && id <= 225) || id == 286 || id == 348
		|| id == 237 || id == 280 || id == 281
		|| id == 249 || id == 251 || id == 253 || id == 255 || id == 328;
}

// Shared storage only; no catalogue reader or accessor references g_Au or
// changes modern arrays. Bootstrap installs the validated descriptor view.
QMutex& CatalogueMutex()
{
	static QMutex mutex;
	return mutex;
}

QSharedPointer<const AuLegacyCityCatalogue>& BuiltInCatalogue()
{
	static QSharedPointer<const AuLegacyCityCatalogue> catalogue;
	return catalogue;
}

float QualityCost(quint16 id, quint8 quality, quint8 technology, const QVector<float>& costs)
{
	if(id == 157) return 1.f;
	float value = costs[int(id)];
	if(value < 0) return value;
	if(id != 3)
	{
		value *= float(technology) / 10.f + 1.f;
		if(!((id >= 158 && id <= 225) || id == 286 || id == 348))
			value = (value * float(quality)) * .0078125f;
	}
	return std::max(1.f, value);
}
}

bool AuLegacyCity::CommodityRecord::IsTechnology() const
{
	return (id.value >= 158 && id.value <= 225) || id.value == 286 || id.value == 348;
}

QString AuLegacyCityCatalogue::DataVersion()
{
	return QStringLiteral("2016-GD-0x3a");
}

QByteArray AuLegacyCityCatalogue::ExpectedSourceSha256()
{
	return QByteArrayLiteral("dac1d7b576a8405d55a85f9eb2723d757d0cce0ba7dc49aeb88f7be8e34f9106");
}

bool AuLegacyCityCatalogue::Read(const QByteArray& source, AuLegacyCityCatalogue& output, QString& error)
{
	error.clear();
	if(source.size() != ExpectedSourceBytes)
	{
		error = QStringLiteral("Legacy GD has %1 bytes; expected %2.").arg(source.size()).arg(ExpectedSourceBytes);
		return false;
	}
	const QByteArray hash = QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex();
	if(hash != ExpectedSourceSha256())
	{
		error = QStringLiteral("Legacy GD SHA-256 does not match the recovered 2016 catalogue.");
		return false;
	}
	AuLegacyCityCatalogue accepted;
	Reader reader(source, error);
	quint8 marker = 0, version = 0;
	if(!reader.U8(marker) || !reader.U8(version)) return false;
	if(marker != 0x18 || version != 0x3a)
		return reader.Fail(QStringLiteral("unsupported GD version"));
	if(!ReadTable(reader, accepted.m_details, accepted.m_tables, QStringLiteral("ship_details"), 143, false,
		[](Reader& r, ShipDetailTypeRecord& record) {
			if(!r.U8(record.id)) return false;
			for(QString& name : record.names) if(!r.String(name)) return false;
			return r.U8(record.bridge) && r.U16(record.crew) && r.I16(record.placement)
				&& r.U8(record.minimumTechnology) && r.U16(record.mass);
		})) return false;
	if(!ReadTable(reader, accepted.m_rooms, accepted.m_tables, QStringLiteral("ship_rooms"), 25, false,
		[](Reader& r, ShipRoomTypeRecord& record) {
			if(!r.U32(record.id)) return false;
			for(QString& name : record.names) if(!r.String(name)) return false;
			return true;
		})) return false;
	if(!ReadTable(reader, accepted.m_commodities, accepted.m_tables, QStringLiteral("commodities"), 350, false,
		ReadCommodity)) return false;
	if(!ReadTable(reader, accepted.m_industries, accepted.m_tables, QStringLiteral("industries"), 130, false,
		[](Reader& r, IndustryRecord& record) {
			return r.U8(record.capacitorTens) && r.U8(record.id.value) && r.String(record.name)
				&& r.U8(record.populationPerLevel) && r.I8(record.employees) && r.U8(record.storageTens);
		})) return false;
	if(!ReadTable(reader, accepted.m_processes, accepted.m_tables, QStringLiteral("manufacturing_processes"), 526, false,
		[](Reader& r, ManufacturingProcessRecord& record) {
			if(!r.U16(record.id.value) || !r.String(record.name) || !r.U16(record.commodity.value)
				|| !r.U8(record.industry.value) || !r.U16(record.quantity) || !r.U16(record.serializedLabor)
				|| !r.U8(record.craft) || !r.U8(record.minimumTechnology)) return false;
			record.labor = record.serializedLabor & 0x7fffU;
			record.isDefault = (record.serializedLabor & 0x8000U) != 0;
			// GD has no nat field; apply the agreed exceptions to its true default.
			switch(record.id.value)
			{
			case 12:  // Fertilizer using Atmosphere
			case 14:  // Fertilizer using Air
			case 23:  // Hydrogen using Natural Gas
			case 58:  // Water using Air and Hydrogen
			case 92:  // Hydrogen using Water
			case 127: // Air using Water
			case 170: // Water using Environment
			case 171: // Water using Ice
				record.naturalResourceQuality = false;
				break;
			default:
				break;
			}
			return true;
		})) return false;
	if(!ReadTable(reader, accepted.m_components, accepted.m_tables, QStringLiteral("manufacturing_components"), 2420, true,
		[](Reader& r, ManufacturingComponentRecord& record) {
			return r.U16(record.id) && r.U16(record.process.value) && r.U16(record.commodity.value)
				&& r.U16(record.quantity) && r.U8(record.effect);
		})) return false;
	if(!ReadTable(reader, accepted.m_construction, accepted.m_tables, QStringLiteral("construction_processes"), 82, false,
		[](Reader& r, ConstructionProcessRecord& record) {
			return r.U8(record.id.value) && r.String(record.name) && r.U8(record.industry.value)
				&& r.U8(record.sitePreparation) && r.U8(record.stackLimit) && r.U8(record.terrain)
				&& r.U8(record.buildRadius);
		})) return false;
	if(!ReadTable(reader, accepted.m_constructionComponents, accepted.m_tables, QStringLiteral("construction_components"), 476, true,
		[](Reader& r, ConstructionComponentRecord& record) {
			return r.U16(record.id) && r.U8(record.process.value) && r.U16(record.commodity.value)
				&& r.U8(record.quantity) && r.U8(record.effect) && r.U8(record.level);
		})) return false;
	if(reader.Remaining() != 0)
		return reader.Fail(QStringLiteral("trailing data after construction components"));
	accepted.m_sourceBytes = source;
	accepted.m_sourceSha256 = hash;
	if(!accepted.Validate(error) || !accepted.CalculateBaseCosts(error)) return false;
	accepted.BuildCrosswalk();
	if(accepted.m_runtimeDescriptors.size() != 349
		|| accepted.m_fromRuntime.size() != 349 || accepted.m_toRuntime.size() != 349)
	{
		error = QStringLiteral("Legacy commodity crosswalk is incomplete or contains a runtime collision.");
		return false;
	}
	accepted.m_valid = true;
	output = accepted;
	return true;
}

bool AuLegacyCityCatalogue::LoadBuiltIn(QString& error)
{
	QMutexLocker lock(&CatalogueMutex());
	if(BuiltInCatalogue())
	{
		error.clear();
		return true;
	}
	InitLegacyCityCatalogueResource();
	QFile file(QStringLiteral(":/legacy/cities/GD2016"));
	if(!file.open(QIODevice::ReadOnly))
	{
		error = QStringLiteral("The packaged legacy city catalogue is unavailable: %1").arg(file.errorString());
		return false;
	}
	const QByteArray bytes = file.read(ExpectedSourceBytes + 1);
	if(file.error() != QFile::NoError || !file.atEnd())
	{
		error = QStringLiteral("The packaged legacy city catalogue could not be read completely.");
		return false;
	}
	QSharedPointer<AuLegacyCityCatalogue> accepted(new AuLegacyCityCatalogue);
	if(!Read(bytes, *accepted, error)) return false;
	BuiltInCatalogue() = accepted;
	return true;
}

const AuLegacyCityCatalogue* AuLegacyCityCatalogue::BuiltIn()
{
	QMutexLocker lock(&CatalogueMutex());
	return BuiltInCatalogue().data();
}

bool AuLegacyCityCatalogue::Validate(QString& error)
{
	auto fail = [&error](const QString& message) { error = QStringLiteral("Legacy GD: ") + message; return false; };
	QSet<quint8> detailIds;
	for(const ShipDetailTypeRecord& record : m_details)
	{
		if(detailIds.contains(record.id)) return fail(QStringLiteral("duplicate ship detail type"));
		detailIds.insert(record.id);
	}
	QSet<quint32> roomIds;
	for(const ShipRoomTypeRecord& record : m_rooms)
	{
		if(roomIds.contains(record.id)) return fail(QStringLiteral("duplicate ship room type"));
		roomIds.insert(record.id);
	}
	for(int index = 0; index < m_commodities.size(); ++index)
	{
		const CommodityRecord& record = m_commodities[index];
		if(record.id.value != index) return fail(QStringLiteral("commodity table is not contiguous"));
		if(index != 0 && record.descriptor.m_name.isEmpty()) return fail(QStringLiteral("unnamed commodity"));
		if(index != 0) m_commodityIndex.insert(record.id.value, index);
	}
	for(int index = 0; index < m_industries.size(); ++index)
	{
		const IndustryRecord& record = m_industries[index];
		if(record.IsPlaceholder()) continue;
		if(!record.id.value || m_industryIndex.contains(record.id.value))
			return fail(QStringLiteral("invalid or duplicate industry"));
		m_industryIndex.insert(record.id.value, index);
	}
	if(m_industryIndex.size() != 75) return fail(QStringLiteral("named industry count changed"));
	for(int index = 0; index < m_processes.size(); ++index)
	{
		const ManufacturingProcessRecord& record = m_processes[index];
		if(!record.id.value || m_processIndex.contains(record.id.value) || !Commodity(record.commodity)
			|| (record.industry.value && !Industry(record.industry))
			|| record.craft > 2 || record.minimumTechnology > 31)
			return fail(QStringLiteral("invalid manufacturing process %1").arg(record.id.value));
		m_processIndex.insert(record.id.value, index);
	}
	QSet<quint16> componentIds;
	for(const ManufacturingComponentRecord& record : m_components)
	{
		if(componentIds.contains(record.id) || !ManufacturingProcess(record.process)
			|| !Commodity(record.commodity) || record.effect < 1 || record.effect > 6)
			return fail(QStringLiteral("invalid manufacturing component %1").arg(record.id));
		componentIds.insert(record.id);
	}
	for(int index = 0; index < m_construction.size(); ++index)
	{
		const ConstructionProcessRecord& record = m_construction[index];
		if(!record.id.value || m_constructionIndex.contains(record.id.value) || !Industry(record.industry))
			return fail(QStringLiteral("invalid construction process %1").arg(record.id.value));
		m_constructionIndex.insert(record.id.value, index);
	}
	componentIds.clear();
	for(const ConstructionComponentRecord& record : m_constructionComponents)
	{
		if(componentIds.contains(record.id) || !ConstructionProcess(record.process)
			|| !Commodity(record.commodity) || record.effect < 1 || record.effect > 6)
			return fail(QStringLiteral("invalid construction component %1").arg(record.id));
		componentIds.insert(record.id);
	}
	return true;
}

bool AuLegacyCityCatalogue::CalculateBaseCosts(QString& error)
{
	// Metadata calculation from 2016 DbManufacturingProcess::Cost and
	// DgData::CalcCommodityBaseCosts. This does not run production or simulation.
	QVector<float> costs(m_commodities.size(), -1.f);
	costs[0] = 0.f;
	for(int pass = 0; pass < costs.size(); ++pass)
	{
		int pending = 0;
		for(int id = 1; id < costs.size(); ++id)
		{
			if(costs[id] >= 0) continue;
			if(id == 87 || id == 88 || id == 89 || id == 94 || id == 147 || id == 148 || id == 149 || id == 150)
				costs[id] = 10.f;
			else
			{
				bool hasProcess = false;
				float bestCost = -1.f;
				for(const ManufacturingProcessRecord& process : m_processes)
				{
					if(process.commodity.value != id) continue;
					hasProcess = true;
					float total = 0.f;
					bool known = true;
					for(const ManufacturingComponentRecord& component : m_components)
					{
						if(component.process != process.id || component.effect != 5 || !component.quantity) continue;
						float price = QualityCost(component.commodity.value, 128, process.minimumTechnology, costs);
						if(price < 0) { known = false; break; }
						if(component.commodity.value == 157)
							total += float(component.quantity) * 100.f;
						else
							total = (total + float(component.quantity) * price) + float(component.quantity);
					}
					if(!known) continue;
					float cost = float(process.labor) + total;
					if(process.quantity > 1) cost = std::max(1.f, cost / float(process.quantity));
					if(bestCost < 0 || cost < bestCost) bestCost = cost;
				}
				if(!hasProcess) costs[id] = 1.f;
				else if(bestCost >= 0) costs[id] = bestCost;
			}
			if(costs[id] < 0) ++pending;
		}
		if(!pending)
		{
			for(int id = 0; id < costs.size(); ++id) m_commodities[id].descriptor.SetBaseCost(costs[id]);
			return true;
		}
	}
	error = QStringLiteral("Legacy GD manufacturing costs contain an unresolved dependency.");
	return false;
}

void AuLegacyCityCatalogue::BuildCrosswalk()
{
	quint16 nextAlias = RuntimeAliasFirst;
	for(const CommodityRecord& record : m_commodities)
	{
		quint16 id = record.id.value;
		if(!id) continue;
		quint16 runtime = id;
		if(IsRuntimeAlias(id)) runtime = nextAlias++;
		else if(id == 2) runtime = 161;
		else if(id == 113) runtime = 354;
		else if(id == 287) runtime = 651;
		else if(id == 288) runtime = 650;
		m_toRuntime.insert(id, runtime);
		m_fromRuntime.insert(runtime, id);
		DbCommodity descriptor(record.descriptor);
		descriptor.m_id = COMMODITYID(runtime);
		if(id == 103) { descriptor.m_modelPath = QStringLiteral(":data/mTank.3ds"); } // Our new Tank model 
		m_runtimeDescriptors.insert(runtime, descriptor);
		m_runtimeIds.append(runtime);
	}
	std::sort(m_runtimeIds.begin(), m_runtimeIds.end());
}

const AuLegacyCity::CommodityRecord* AuLegacyCityCatalogue::Commodity(AuLegacyCity::CommodityId id) const
{
	const auto found = m_commodityIndex.constFind(id.value);
	return found == m_commodityIndex.cend() ? nullptr : &m_commodities[*found];
}

const AuLegacyCity::IndustryRecord* AuLegacyCityCatalogue::Industry(AuLegacyCity::IndustryId id) const
{
	const auto found = m_industryIndex.constFind(id.value);
	return found == m_industryIndex.cend() ? nullptr : &m_industries[*found];
}

const AuLegacyCity::ManufacturingProcessRecord* AuLegacyCityCatalogue::ManufacturingProcess(AuLegacyCity::ManufacturingProcessId id) const
{
	const auto found = m_processIndex.constFind(id.value);
	return found == m_processIndex.cend() ? nullptr : &m_processes[*found];
}

const AuLegacyCity::ConstructionProcessRecord* AuLegacyCityCatalogue::ConstructionProcess(AuLegacyCity::ConstructionProcessId id) const
{
	const auto found = m_constructionIndex.constFind(id.value);
	return found == m_constructionIndex.cend() ? nullptr : &m_construction[*found];
}

QVector<AuLegacyCity::ManufacturingProcessId> AuLegacyCityCatalogue::ProcessesForIndustry(AuLegacyCity::IndustryId id) const
{
	QVector<AuLegacyCity::ManufacturingProcessId> result;
	for(const AuLegacyCity::ManufacturingProcessRecord& process : m_processes)
		if(process.industry == id) result.append(process.id);
	return result;
}

AuLegacyCity::ManufacturingProcessId AuLegacyCityCatalogue::DefaultManufacturingProcess(AuLegacyCity::IndustryId id) const
{
	AuLegacyCity::ManufacturingProcessId result;
	for(const AuLegacyCity::ManufacturingProcessRecord& process : m_processes)
	{
		if(process.industry != id) continue;
		if(result.IsNull() || process.isDefault) result = process.id;
		if(process.isDefault) break;
	}
	return result;
}

QVector<AuLegacyCity::ManufacturingComponentRecord> AuLegacyCityCatalogue::ComponentsForProcess(AuLegacyCity::ManufacturingProcessId id) const
{
	QVector<AuLegacyCity::ManufacturingComponentRecord> result;
	for(const AuLegacyCity::ManufacturingComponentRecord& component : m_components)
		if(component.process == id) result.append(component);
	return result;
}

QVector<AuLegacyCity::ConstructionComponentRecord> AuLegacyCityCatalogue::ComponentsForConstruction(AuLegacyCity::ConstructionProcessId id) const
{
	QVector<AuLegacyCity::ConstructionComponentRecord> result;
	for(const AuLegacyCity::ConstructionComponentRecord& component : m_constructionComponents)
		if(component.process == id) result.append(component);
	return result;
}

const AuLegacyCity::ConstructionProcessRecord* AuLegacyCityCatalogue::ConstructionProcessForIndustry(AuLegacyCity::IndustryId id) const
{
	for(const auto& process : m_construction)
		if(process.industry == id) return &process;
	return nullptr;
}

int AuLegacyCityCatalogue::BuildingDesignMaximumDeck(AuLegacyCity::IndustryId industry, AuLegacyCity::CommodityId material) const
{
	// DgData::BuildingDesignMaxDeckLimit, AuData13 0x131180. These fixed
	// shapes do not describe how many industrial levels can be constructed.
	switch(industry.value)
	{
	case 0: case 1: case 2: case 10: case 51: case 114: case 129: case 130: return 0;
	case 11: case 12: return 2;
	case 70: case 85: case 88: case 113: case 127: return 1;
	case 87: case 126: return 5;
	case 111: return 4;
	case 112: case 128: return 3;
	default: break;
	}
	const auto* process = ConstructionProcessForIndustry(industry);
	if(!process) return 0;
	const int limit = process->stackLimit;
	if(limit == 34) return 1;
	if(limit == 0 || limit == 32 || limit == 33 || material.value == 86) return 0;
	if(limit != 1 && limit != 3 && limit != 7 && limit != 15 && limit != 31) return 0;
	if(material.value == 48) return std::min(limit, 3);
	if(material.value == 76) return std::min(limit, 7);
	if(material.value == 51) return std::min(limit, 15);
	if(material.value == 146) return std::min(limit, 23);
	return limit;
}

int AuLegacyCityCatalogue::ConstructionLevelLimit(AuLegacyCity::ConstructionProcessId id,
	AuLegacyCity::CommodityId material, bool flammableMaterialsAllowed) const
{
	// DaWorldLL::LockTestBuildLocation, AuAtom13 0x5264d0. Resource-driven
	// recipes take their limit from the site instead of material strength.
	// DaWorld::OrbitZone (0x20f940) identifies this[0x3b0] as the orbit
	// zone. Its zero value is Inferno: wood/lumber/asphalt cannot be used
	// there. Vacuum is handled independently by the pressure-material bill.
	const auto* process = ConstructionProcess(id);
	if(!process) return 0;
	if(process->stackLimit == 32 || process->stackLimit == 33) return 32;
	if(process->stackLimit == 34) return 2;
	if(process->stackLimit >= 32) return 0;
	int limit = int(process->stackLimit) + 1;
	switch(material.value)
	{
	case 23: return flammableMaterialsAllowed ? limit : 0;
	case 48: return flammableMaterialsAllowed ? std::min(limit, 4) : 0;
	case 86: return flammableMaterialsAllowed ? 1 : 0;
	case 76: return std::min(limit, 8);
	case 51: return std::min(limit, 16);
	case 145: return std::min(limit, 32);
	case 146: return std::min(limit, 24);
	default: return 0;
	}
}

int AuLegacyCityCatalogue::ConstructionResourceLevelLimit(AuLegacyCity::ConstructionProcessId id,
	quint32 rocks, quint32 shrubs, quint32 trees) const
{
	const auto* process = ConstructionProcess(id);
	if(!process) return 0;
	if(process->stackLimit < 32) return int(process->stackLimit) + 1;
	if(process->stackLimit == 32) return int(std::min(rocks, quint32(32)));
	if(process->stackLimit == 33) return int(std::min(shrubs, quint32(32)));
	if(process->stackLimit == 34) return trees >= 3 ? 2 : trees ? 1 : 0;
	return 0;
}

quint16 AuLegacyCityCatalogue::SitePreparationLabor(AuLegacyCity::ConstructionProcessId id, quint8 completedLevels,
	quint32 rocks, quint32 shrubs, quint32 trees) const
{
	const auto* process = ConstructionProcess(id);
	if(!process) return 0;
	// LockAddBuilding, 0x524e20. Assembly 0x525243..0x5252aa also
	// recovers the shrub/tree branches missing from the manual C export.
	quint64 labor = 1;
	if(process->sitePreparation & 32) labor = (quint64(completedLevels) + 1) * (quint64(rocks) + 1) * 5 + 1;
	if(process->sitePreparation & 1) labor += quint64(rocks) * 15;
	if(process->sitePreparation & 2) labor += quint64(shrubs) * 5;
	if(process->sitePreparation & 4) labor += quint64(trees) * 10;
	return quint16(std::min(labor, quint64(65535)));
}

QVector<AuLegacyCity::ConstructionRequirement> AuLegacyCityCatalogue::ConstructionRequirements(
	AuLegacyCity::ConstructionProcessId id, AuLegacyCity::CommodityId material, quint8 environment,
	quint32 pavementQuantity, quint32 materialMultiplier) const
{
	QVector<AuLegacyCity::ConstructionRequirement> result;
	const auto* process = ConstructionProcess(id);
	if(!process) return result;
	auto add = [&](AuLegacyCity::CommodityId commodity, quint8 effect, quint32 quantity) {
		for(auto& requirement : result)
			if(requirement.commodity == commodity && requirement.effect == effect)
			{ requirement.quantity += quantity; return; }
		AuLegacyCity::ConstructionRequirement requirement;
		requirement.commodity = commodity; requirement.effect = effect; requirement.quantity = quantity;
		result.append(requirement);
	};
	// InitDbData 0x1181b0 uses every matching row, without reading its lvl
	// byte. AddRequirement 0x128600 merges the same commodity/effect.
	for(const auto& component : m_constructionComponents)
		if(component.process == id) add(component.commodity, component.effect, component.quantity);
	if(!(id.value >= 1 && id.value <= 5) && id.value != 74 && id.value != 80 && id.value != 81)
	{
		if(environment == 2)
		{ add(AuLegacyCity::CommodityId(27), 5, 1); add(AuLegacyCity::CommodityId(51), 5, 3); add(AuLegacyCity::CommodityId(63), 5, 2); }
		else if(environment == 4)
		{ add(AuLegacyCity::CommodityId(51), 5, 10); add(AuLegacyCity::CommodityId(63), 5, 4); }
	}
	for(auto& requirement : result)
	{
		if(process->industry.value != 2 && requirement.commodity.value == 48) requirement.commodity = material;
		// SetPavementMaterial 0x118470 replaces the selected surface's
		// quantity, including a nonzero amount for the smallest road.
		if(pavementQuantity && ((id.value == 3 && requirement.commodity.value == 23)
			|| ((id.value == 4 || id.value == 7) && requirement.commodity.value == 76)))
			requirement.quantity = pavementQuantity;
		if(requirement.effect == 5 && requirement.quantity) requirement.quantity *= materialMultiplier;
	}
	return result;
}

quint32 AuLegacyCityCatalogue::RoadPavementQuantity(double lengthFeet, double widthFeet)
{
	if(!std::isfinite(lengthFeet) || !std::isfinite(widthFeet) || lengthFeet <= 0 || widthFeet <= 0) return 0;
	// LockAddRoad 0x5253d0 converts to float before truncating to int.
	const double amount = float(lengthFeet * widthFeet * 2.0) / 19600.0f;
	return quint32(std::max(1.0, std::min(std::floor(amount), double(0x7fffffff))));
}

int AuLegacyCityCatalogue::BiodomeSegmentCount(double radiusFeet)
{
	// LockTestBuildLocation 0x5264d0 / LockAddBiodome 0x524910:
	// one structural/material section per fifty meters of circumference.
	if(!std::isfinite(radiusFeet) || radiusFeet <= 0) return 0;
	const double sections = std::floor(radiusFeet * 6.2831853071795864769 / 164.041994750656);
	return sections >= 6 && sections <= 100 ? int(sections) : 0;
}

quint16 AuLegacyCityCatalogue::RoadPreparationLabor(AuLegacyCity::ConstructionProcessId process,
	double lengthFeet, double widthFeet, double meanAbsoluteTerrainDifference)
{
	if(!std::isfinite(lengthFeet) || !std::isfinite(widthFeet) || !std::isfinite(meanAbsoluteTerrainDifference)
		|| lengthFeet <= 0 || widthFeet <= 0 || meanAbsoluteTerrainDifference < 0) return 0;
	double amount = lengthFeet * widthFeet;
	if(process.value != 74 && process.value != 80 && process.value != 81) amount *= meanAbsoluteTerrainDifference * .5;
	return quint16(std::max(1.0, std::min(65535.0, std::ceil(amount / 1000.0))));
}

quint16 AuLegacyCityCatalogue::RuntimeCommodityId(AuLegacyCity::CommodityId id) const
{
	return m_toRuntime.value(id.value, 0);
}

bool AuLegacyCityCatalogue::HistoricalCommodityId(quint16 runtimeId, AuLegacyCity::CommodityId& output) const
{
	const auto found = m_fromRuntime.constFind(runtimeId);
	if(found == m_fromRuntime.cend()) return false;
	output = AuLegacyCity::CommodityId(*found);
	return true;
}

const AuLegacyCity::CommodityRecord* AuLegacyCityCatalogue::CommodityForRuntimeId(quint16 runtimeId) const
{
	AuLegacyCity::CommodityId id;
	return HistoricalCommodityId(runtimeId, id) ? Commodity(id) : nullptr;
}

const DbCommodity* AuLegacyCityCatalogue::RuntimeCommodity(quint16 runtimeId) const
{
	const auto found = m_runtimeDescriptors.constFind(runtimeId);
	return found == m_runtimeDescriptors.cend() ? nullptr : &*found;
}

quint8 AuLegacyCityCatalogue::RuntimeIndustryId(AuLegacyCity::IndustryId id) const
{
	if(!Industry(id)) return 0;
	return id.value == 65 ? 68 : id.value;
}

const AuLegacyCity::IndustryRecord* AuLegacyCityCatalogue::IndustryForRuntimeId(quint8 runtimeId) const
{
	const AuLegacyCity::IndustryRecord* record = Industry(AuLegacyCity::IndustryId(runtimeId == 68 ? 65 : runtimeId));
	return record && RuntimeIndustryId(record->id) == runtimeId ? record : nullptr;
}
