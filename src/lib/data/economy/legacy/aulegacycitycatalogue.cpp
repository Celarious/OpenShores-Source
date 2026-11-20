#include "aulegacycitycatalogue.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QSharedPointer>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
using namespace AuLegacyCity;

// Both SQL and the network/cache path use this decoder. Values are checked
// before narrowing, so an edited database cannot wrap IDs or component counts.
class RowReader
{
public:
	RowReader(const QJsonObject& row, const QString& table, QString& error) :
		m_row(row), m_table(table), m_error(error) {}
	bool Fail(const QString& key) const
	{
		m_error = QStringLiteral("Legacy catalogue %1 row %2: invalid or missing %3")
			.arg(m_table).arg(m_row.value(QStringLiteral("id")).toDouble()).arg(key);
		return false;
	}
	template<class T> bool Integer(const QString& key, T& value, qint64 minimum, qint64 maximum) const
	{
		const QJsonValue field = m_row.value(key.toLower());
		const double number = field.toDouble();
		if(!field.isDouble() || !std::isfinite(number) || std::floor(number) != number
			|| number < double(minimum) || number > double(maximum)) return Fail(key);
		value = static_cast<T>(qint64(number));
		return true;
	}
	template<class T> bool Number(const QString& key, T& value, double minimum, double maximum) const
	{
		const QJsonValue field = m_row.value(key.toLower());
		const double number = field.toDouble();
		if(!field.isDouble() || !std::isfinite(number) || number < minimum || number > maximum) return Fail(key);
		value = static_cast<T>(number);
		return true;
	}
	bool String(const QString& key, QString& value) const
	{
		const QJsonValue field = m_row.value(key.toLower());
		if(!field.isString() && !field.isNull()) return Fail(key);
		value = field.isNull() ? QString() : field.toString();
		return value.size() <= 32768 || Fail(key);
	}
	bool Boolean(const QString& key, bool& value) const
	{
		const QJsonValue field = m_row.value(key.toLower());
		if(!field.isBool()) return Fail(key);
		value = field.toBool();
		return true;
	}
private:
	QJsonObject m_row;
	QString m_table;
	QString& m_error;
};

class CommodityDescriptor : public DbCommodity
{
public:
	void SetArmor(const CommodityRecord& record);
	void SetArmorSpecialty(quint8 specialty) { m_armorSpec = specialty; }
};

void CommodityDescriptor::SetArmor(const CommodityRecord& record)
{
	// Energy and kinetic entries at 6 and 9 have no modern member. The original
	// values remain in the catalogue arrays and the complete SQL-row payload.
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
		if(absorb[index]) *absorb[index] = float(record.armorAbsorb[index]) / 100.f;
		if(block[index]) *block[index] = float(record.armorBlock[index]);
	}
	m_ArmorModeAbsorbArea = record.modeAbsorb[0] / 100.f;
	m_ArmorModeAbsorbContact = record.modeAbsorb[1] / 100.f;
	m_ArmorModeAbsorbProjectile = record.modeAbsorb[2] / 100.f;
	m_ArmorModeAbsorbPsionic = record.modeAbsorb[3] / 100.f;
	m_ArmorModeBlockArea = record.modeBlock[0];
	m_ArmorModeBlockContact = record.modeBlock[1];
	m_ArmorModeBlockProjectile = record.modeBlock[2];
	m_ArmorModeBlockPsionic = record.modeBlock[3];
}

bool ReadCommodity(const RowReader& r, CommodityRecord& record)
{
	CommodityDescriptor descriptor;
	if(!r.Integer(QStringLiteral("id"), record.id.value, 0, 65535)) return false;
	descriptor.m_id = COMMODITYID(record.id.value);
	if(!r.Integer(QStringLiteral("ammo1Qty"), descriptor.m_ammo1Qty, 0, 255)
		|| !r.Integer(QStringLiteral("ammo1Type"), descriptor.m_ammo1Type, 0, 255)
		|| !r.Integer(QStringLiteral("ammo2Qty"), descriptor.m_ammo2Qty, 0, 255)
		|| !r.Integer(QStringLiteral("ammo2Type"), descriptor.m_ammo2Type, 0, 255)) return false;
	static const char* effects[] = {"Acid", "Bludgeon", "Cold", "Crush", "Disease", "Electric", "Energy", "Heat",
		"Implant", "Kinetic", "Pierce", "Poison", "Radiation", "Slash", "Sonic", "Stun", "Suffocate"};
	static const char* modes[] = {"Area", "Contact", "Projectile", "Psionic"};
	for(size_t index = 0; index < record.armorAbsorb.size(); ++index)
		if(!r.Number(QStringLiteral("armorEffectAbsorb") + QLatin1String(effects[index]), record.armorAbsorb[index], 0, 999.9)
			|| !r.Number(QStringLiteral("armorEffectBlock") + QLatin1String(effects[index]), record.armorBlock[index], 0, 32767)) return false;
	for(size_t index = 0; index < record.modeAbsorb.size(); ++index)
		if(!r.Number(QStringLiteral("armorModeAbsorb") + QLatin1String(modes[index]), record.modeAbsorb[index], 0, 999.9)
			|| !r.Number(QStringLiteral("armorModeBlock") + QLatin1String(modes[index]), record.modeBlock[index], 0, 32767)) return false;
	descriptor.SetArmor(record);
	quint8 specialty = 0, capacityWidth = 0, capacityHeight = 0, width = 0, height = 0;
	double upX = 0, upY = 0, upZ = 0;
	const double floatMaximum = std::numeric_limits<float>::max();
	const double doubleMaximum = std::numeric_limits<double>::max();
	if(!r.Integer(QStringLiteral("armorSpec"), specialty, 0, 255)
		|| !r.Integer(QStringLiteral("bitsWear"), descriptor.m_bitsWear, 0, 4294967295LL)
		|| !r.Integer(QStringLiteral("capwid"), capacityWidth, 0, 255)
		|| !r.Integer(QStringLiteral("capht"), capacityHeight, 0, 255)
		|| !r.Integer(QStringLiteral("combits"), descriptor.m_combits, 0, 4294967295LL)
		|| !r.String(QStringLiteral("description"), record.deprecatedDescription)
		|| !r.Integer(QStringLiteral("food"), descriptor.m_food, 0, 255)
		|| !r.Integer(QStringLiteral("health"), descriptor.m_health, -128, 127)
		|| !r.String(QStringLiteral("iconPath"), descriptor.m_iconPath)
		|| !r.String(QStringLiteral("modelPath"), descriptor.m_modelPath)
		|| !r.Number(QStringLiteral("insertHeight"), descriptor.m_insertHeight, -floatMaximum, floatMaximum)
		|| !r.Number(QStringLiteral("upVectorX"), upX, -doubleMaximum, doubleMaximum)
		|| !r.Number(QStringLiteral("upVectorY"), upY, -doubleMaximum, doubleMaximum)
		|| !r.Number(QStringLiteral("upVectorZ"), upZ, -doubleMaximum, doubleMaximum)
		|| !r.Integer(QStringLiteral("light"), descriptor.m_light, 0, 255)
		|| !r.String(QStringLiteral("name"), descriptor.m_name)
		|| !r.Integer(QStringLiteral("newcond"), descriptor.m_newcond, 0, 255)
		|| !r.Integer(QStringLiteral("sizewid"), width, 0, 255)
		|| !r.Integer(QStringLiteral("sizeht"), height, 0, 255)
		|| !r.Integer(QStringLiteral("stackLimit"), descriptor.m_stackLimit, 0, 255)) return false;
	descriptor.SetArmorSpecialty(specialty);
	descriptor.m_capacity = QSize(capacityWidth, capacityHeight);
	descriptor.m_size = QSize(width, height);
	descriptor.m_UpVector = AuPoint(upX, upY, upZ);
	if(!r.Integer(QStringLiteral("wpn1Mode"), descriptor.m_wpn1Mode, 0, 255)
		|| !r.Integer(QStringLiteral("wpn1Range"), descriptor.m_wpn1Range, 0, 65535)
		|| !r.Integer(QStringLiteral("wpn1Effect1"), descriptor.m_wpn1Effect1, 0, 255)
		|| !r.Integer(QStringLiteral("wpn1Damage1"), descriptor.m_wpn1Damage1, 0, 65535)
		|| !r.Integer(QStringLiteral("wpn1DamageMod1"), descriptor.m_wpn1DamageMod1, 0, 255)
		|| !r.Integer(QStringLiteral("wpn1Radius1"), descriptor.m_wpn1Radius1, 0, 65535)
		|| !r.Integer(QStringLiteral("wpn1AP1"), descriptor.m_wpn1AP1, 0, 255)
		|| !r.Integer(QStringLiteral("wpn1ST1"), descriptor.m_wpn1ST1, 0, 65535)
		|| !r.Integer(QStringLiteral("wpn1Effect2"), descriptor.m_wpn1Effect2, 0, 255)
		|| !r.Integer(QStringLiteral("wpn1Damage2"), descriptor.m_wpn1Damage2, 0, 65535)
		|| !r.Integer(QStringLiteral("wpn1DamageMod2"), descriptor.m_wpn1DamageMod2, 0, 255)
		|| !r.Integer(QStringLiteral("wpn1Radius2"), descriptor.m_wpn1Radius2, 0, 65535)
		|| !r.Integer(QStringLiteral("wpn1AP2"), descriptor.m_wpn1AP2, 0, 255)
		|| !r.Integer(QStringLiteral("wpn1ST2"), descriptor.m_wpn1ST2, 0, 65535)) return false;
	if(!r.Integer(QStringLiteral("wpn2Mode"), descriptor.m_wpn2Mode, 0, 255)
		|| !r.Integer(QStringLiteral("wpn2Range"), descriptor.m_wpn2Range, 0, 65535)
		|| !r.Integer(QStringLiteral("wpn2Effect1"), descriptor.m_wpn2Effect1, 0, 255)
		|| !r.Integer(QStringLiteral("wpn2Damage1"), descriptor.m_wpn2Damage1, 0, 65535)
		|| !r.Integer(QStringLiteral("wpn2DamageMod1"), descriptor.m_wpn2DamageMod1, 0, 255)
		|| !r.Integer(QStringLiteral("wpn2Radius1"), descriptor.m_wpn2Radius1, 0, 65535)
		|| !r.Integer(QStringLiteral("wpn2AP1"), descriptor.m_wpn2AP1, 0, 255)
		|| !r.Integer(QStringLiteral("wpn2ST1"), descriptor.m_wpn2ST1, 0, 65535)
		|| !r.Integer(QStringLiteral("wpn2Effect2"), descriptor.m_wpn2Effect2, 0, 255)
		|| !r.Integer(QStringLiteral("wpn2Damage2"), descriptor.m_wpn2Damage2, 0, 65535)
		|| !r.Integer(QStringLiteral("wpn2DamageMod2"), descriptor.m_wpn2DamageMod2, 0, 255)
		|| !r.Integer(QStringLiteral("wpn2Radius2"), descriptor.m_wpn2Radius2, 0, 65535)
		|| !r.Integer(QStringLiteral("wpn2AP2"), descriptor.m_wpn2AP2, 0, 255)
		|| !r.Integer(QStringLiteral("wpn2ST2"), descriptor.m_wpn2ST2, 0, 65535)) return false;
	if(!r.Number(QStringLiteral("wt"), descriptor.m_wt, 0, floatMaximum)) return false;
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
	return QStringLiteral("legacy-sql-v1");
}

bool AuLegacyCityCatalogue::Read(const QByteArray& source, AuLegacyCityCatalogue& output, QString& error)
{
	error.clear();
	if(source.isEmpty() || source.size() > MaximumSourceBytes)
	{ error = QStringLiteral("Legacy catalogue payload is empty or exceeds 16 MiB."); return false; }
	QJsonParseError parseError;
	const QJsonDocument document = QJsonDocument::fromJson(source, &parseError);
	if(parseError.error != QJsonParseError::NoError || !document.isObject())
	{ error = QStringLiteral("Invalid Legacy catalogue JSON: %1").arg(parseError.errorString()); return false; }
	const QJsonObject object = document.object();
	if(object.value(QStringLiteral("format")).toString() != DataVersion())
	{ error = QStringLiteral("Unsupported Legacy catalogue payload version."); return false; }
	AuLegacyCityCatalogue accepted;
	const char* tableNames[] = {"commodities", "industries", "manufacturing_processes", "manufacturing_components",
		"construction_processes", "construction_components"};
	const int maximumCounts[] = {65536, 256, 65535, 65536, 255, 65536};
	for(int table = 0; table < 6; ++table)
	{
		const QString name = QLatin1String(tableNames[table]);
		const QJsonValue field = object.value(name);
		if(!field.isArray() || field.toArray().isEmpty() || field.toArray().size() > maximumCounts[table])
		{ error = QStringLiteral("Legacy catalogue %1 has an invalid record count.").arg(name); return false; }
		const QJsonArray rows = field.toArray();
		QSet<int> sourceOrders;
		int previousOrder = 0;
		for(const QJsonValue& value : rows)
		{
			if(!value.isObject())
			{ error = QStringLiteral("Legacy catalogue %1 contains a non-record value.").arg(name); return false; }
			const RowReader r(value.toObject(), name, error);
			if(table == 2 || table == 4)
			{
				int order = 0;
				if(!r.Integer(QStringLiteral("sourceOrder"), order, 1, 2147483647)) return false;
				if(order <= previousOrder || sourceOrders.contains(order)) return r.Fail(QStringLiteral("sourceOrder"));
				previousOrder = order;
				sourceOrders.insert(order);
			}
			switch(table)
			{
			case 0:
			{
				CommodityRecord record;
				if(!ReadCommodity(r, record)) return false;
				accepted.m_commodities.append(record);
				break;
			}
			case 1:
			{
				IndustryRecord record;
				if(!r.Integer(QStringLiteral("id"), record.id.value, 0, 255)
					|| !r.String(QStringLiteral("name"), record.name)
					|| !r.Integer(QStringLiteral("pop"), record.populationPerLevel, 0, 255)
					|| !r.Integer(QStringLiteral("emp"), record.employees, -128, 127)
					|| !r.Integer(QStringLiteral("storage"), record.storageTens, 0, 255)
					|| !r.Integer(QStringLiteral("capacitor"), record.capacitorTens, 0, 255)) return false;
				accepted.m_industries.append(record);
				break;
			}
			case 2:
			{
				ManufacturingProcessRecord record;
				if(!r.Integer(QStringLiteral("id"), record.id.value, 1, 65535)
					|| !r.String(QStringLiteral("name"), record.name)
					|| !r.Integer(QStringLiteral("idc"), record.commodity.value, 1, 65535)
					|| !r.Integer(QStringLiteral("idi"), record.industry.value, 0, 255)
					|| !r.Integer(QStringLiteral("qty"), record.quantity, 1, 65535)
					|| !r.Integer(QStringLiteral("labor"), record.labor, 0, 65535)
					|| !r.Integer(QStringLiteral("craft"), record.craft, 0, 2)
					|| !r.Integer(QStringLiteral("techLevel"), record.minimumTechnology, 0, 31)
					|| !r.Boolean(QStringLiteral("isdefault"), record.isDefault)
					|| !r.Boolean(QStringLiteral("nat"), record.naturalResourceQuality)) return false;
				record.naturalFlagSerialized = true;
				accepted.m_processes.append(record);
				break;
			}
			case 3:
			{
				ManufacturingComponentRecord record;
				if(!r.Integer(QStringLiteral("id"), record.id, 1, 65535)
					|| !r.Integer(QStringLiteral("idmp"), record.process.value, 1, 65535)
					|| !r.Integer(QStringLiteral("idc"), record.commodity.value, 1, 65535)
					|| !r.Integer(QStringLiteral("consumed"), record.quantity, 0, 65535)
					|| !r.Integer(QStringLiteral("ide"), record.effect, 1, 6)) return false;
				accepted.m_components.append(record);
				break;
			}
			case 4:
			{
				ConstructionProcessRecord record;
				if(!r.Integer(QStringLiteral("id"), record.id.value, 1, 255)
					|| !r.String(QStringLiteral("name"), record.name)
					|| !r.Integer(QStringLiteral("idi"), record.industry.value, 1, 255)
					|| !r.Integer(QStringLiteral("sitePrep"), record.sitePreparation, 0, 255)
					|| !r.Integer(QStringLiteral("stackLimit"), record.stackLimit, 0, 255)
					|| !r.Integer(QStringLiteral("idt"), record.terrain, 0, 255)
					|| !r.Integer(QStringLiteral("buildRadius"), record.buildRadius, 0, 255)) return false;
				accepted.m_construction.append(record);
				break;
			}
			case 5:
			{
				ConstructionComponentRecord record;
				if(!r.Integer(QStringLiteral("id"), record.id, 1, 65535)
					|| !r.Integer(QStringLiteral("idcp"), record.process.value, 1, 255)
					|| !r.Integer(QStringLiteral("idc"), record.commodity.value, 1, 65535)
					|| !r.Integer(QStringLiteral("consumed"), record.quantity, 0, 255)
					|| !r.Integer(QStringLiteral("ide"), record.effect, 1, 6)
					|| !r.Integer(QStringLiteral("lvl"), record.level, 0, 255)) return false;
				accepted.m_constructionComponents.append(record);
				break;
			}
			}
		}
	}
	if(!accepted.Finalize(error)) return false;
	accepted.m_sourceBytes = document.toJson(QJsonDocument::Compact);
	accepted.m_sourceSha256 = QCryptographicHash::hash(accepted.m_sourceBytes, QCryptographicHash::Sha256).toHex();
	output = accepted;
	return true;
}

bool AuLegacyCityCatalogue::LoadBuiltIn(QString& error)
{
	if(BuiltIn()) { error.clear(); return true; }
	QSqlDatabase database = QSqlDatabase::database(QStringLiteral("AuUniverse"), false);
	if(!database.isOpen())
	{ error = QStringLiteral("The AuUniverse database must be open before loading the Legacy catalogue."); return false; }
	if(!database.transaction())
	{ error = QStringLiteral("Cannot start Legacy catalogue database snapshot: %1").arg(database.lastError().text()); return false; }
	QSqlQuery query(database);
	if(!query.exec(QStringLiteral("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ READ ONLY")))
	{ error = query.lastError().text(); database.rollback(); return false; }
	QJsonObject object;
	object.insert(QStringLiteral("format"), DataVersion());
	const char* tableNames[] = {"commodities", "industries", "manufacturing_processes", "manufacturing_components",
		"construction_processes", "construction_components"};
	const char* sqlTables[] = {"b_Commodity_Legacy", "b_Industry_Legacy", "b_ManufacturingProcess_Legacy",
		"b_ManufacturingComponent_Legacy", "b_ConstructionProcess_Legacy", "b_ConstructionComponent_Legacy"};
	int payloadBytes = 0;
	for(int table = 0; table < 6; ++table)
	{
		const QString order = table == 2 || table == 4 ? QStringLiteral("sourceOrder, id") : QStringLiteral("id");
		// PostgreSQL JSON preserves booleans, numeric precision, and SQL NULLs.
		const QString sql = QStringLiteral("SELECT row_to_json(record)::text FROM (SELECT * FROM %1 ORDER BY %2) record")
			.arg(QLatin1String(sqlTables[table]), order);
		query.setForwardOnly(true);
		if(!query.exec(sql))
		{
			error = QStringLiteral("Cannot load %1: %2").arg(QLatin1String(sqlTables[table]), query.lastError().text());
			database.rollback(); return false;
		}
		QJsonArray rows;
		while(query.next())
		{
			const QByteArray row = query.value(0).toByteArray();
			payloadBytes += row.size();
			if(payloadBytes > MaximumSourceBytes || rows.size() >= 65536)
			{ error = QStringLiteral("Legacy catalogue database exceeds supported payload limits."); database.rollback(); return false; }
			QJsonParseError parseError;
			const QJsonDocument record = QJsonDocument::fromJson(row, &parseError);
			if(parseError.error != QJsonParseError::NoError || !record.isObject())
			{ error = QStringLiteral("Cannot decode a Legacy catalogue database row."); database.rollback(); return false; }
			rows.append(record.object());
		}
		if(query.lastError().isValid())
		{ error = query.lastError().text(); database.rollback(); return false; }
		object.insert(QLatin1String(tableNames[table]), rows);
	}
	query.finish();
	if(!database.commit())
	{ error = QStringLiteral("Cannot complete Legacy catalogue database snapshot: %1").arg(database.lastError().text()); database.rollback(); return false; }
	return InstallBuiltIn(QJsonDocument(object).toJson(QJsonDocument::Compact), error);
}

bool AuLegacyCityCatalogue::InstallBuiltIn(const QByteArray& source, QString& error)
{
	QSharedPointer<AuLegacyCityCatalogue> accepted(new AuLegacyCityCatalogue);
	if(!Read(source, *accepted, error)) return false;
	QMutexLocker lock(&CatalogueMutex());
	if(BuiltInCatalogue())
	{
		if(BuiltInCatalogue()->SourceSha256() != accepted->SourceSha256())
		{ error = QStringLiteral("Cannot replace a Legacy catalogue while it is in use. Reconnect after clearing the scene."); return false; }
		return true;
	}
	BuiltInCatalogue() = accepted;
	return true;
}

void AuLegacyCityCatalogue::ResetBuiltIn()
{
	QMutexLocker lock(&CatalogueMutex());
	BuiltInCatalogue().clear();
}

const AuLegacyCityCatalogue* AuLegacyCityCatalogue::BuiltIn()
{
	QMutexLocker lock(&CatalogueMutex());
	return BuiltInCatalogue().data();
}

bool AuLegacyCityCatalogue::Finalize(QString& error)
{
	if(!Validate(error) || !CalculateBaseCosts(error)) return false;
	// The first construction recipe supplies the industry's hit-point base.
	for(auto& industry : m_industries)
		if(const auto* process = ConstructionProcessForIndustry(industry.id))
			for(const auto& component : m_constructionComponents)
				if(component.process == process->id && component.commodity.value == 48
					&& component.effect == 5 && component.quantity)
				{ industry.buildingHitBase = component.quantity; break; }
	if(!BuildCrosswalk(error)) return false;
	m_valid = true;
	return true;
}

bool AuLegacyCityCatalogue::Validate(QString& error)
{
	auto fail = [&error](const QString& message) { error = QStringLiteral("Legacy catalogue: ") + message; return false; };
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
	int previousCommodityId = -1;
	for(int index = 0; index < m_commodities.size(); ++index)
	{
		const CommodityRecord& record = m_commodities[index];
		if(record.id.value <= previousCommodityId) return fail(QStringLiteral("commodity IDs are duplicated or unordered"));
		previousCommodityId = record.id.value;
		if(record.id.value && record.descriptor.m_name.isEmpty()) return fail(QStringLiteral("unnamed commodity"));
		if(record.id.value) m_commodityIndex.insert(record.id.value, index);
	}
	if(m_commodityIndex.isEmpty()) return fail(QStringLiteral("no named commodities"));
	for(int index = 0; index < m_industries.size(); ++index)
	{
		const IndustryRecord& record = m_industries[index];
		if(record.IsPlaceholder()) continue;
		if(!record.id.value || m_industryIndex.contains(record.id.value))
			return fail(QStringLiteral("invalid or duplicate industry"));
		m_industryIndex.insert(record.id.value, index);
	}
	if(m_industryIndex.isEmpty()) return fail(QStringLiteral("no named industries"));
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
	QVector<float> costs(int(m_commodities.last().id.value) + 1, -1.f);
	costs[0] = 0.f;
	for(int pass = 0; pass < m_commodities.size(); ++pass)
	{
		int pending = 0;
		for(const CommodityRecord& record : m_commodities)
		{
			const int id = record.id.value;
			if(!id) continue;
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
			for(CommodityRecord& record : m_commodities) record.descriptor.SetBaseCost(costs[record.id.value]);
			return true;
		}
	}
	error = QStringLiteral("Legacy catalogue manufacturing costs contain an unresolved dependency.");
	return false;
}

bool AuLegacyCityCatalogue::BuildCrosswalk(QString& error)
{
	for(const CommodityRecord& record : m_commodities)
	{
		quint16 id = record.id.value;
		if(!id) continue;
		quint16 runtime = id;
		if(IsRuntimeAlias(id))
		{
			// The alias position belongs to the historical ID, not to the current
			// row count. Removing a commodity must never renumber saved items.
			runtime = RuntimeAliasFirst;
			for(quint16 historical = 1; historical < id; ++historical)
				if(IsRuntimeAlias(historical)) ++runtime;
			if(runtime > RuntimeAliasLast)
			{ error = QStringLiteral("Legacy runtime alias range is exhausted."); return false; }
		}
		else if(id == 2) runtime = 161;
		else if(id == 113) runtime = 354;
		else if(id == 287) runtime = 651;
		else if(id == 288) runtime = 650;
		else if(id == 350) runtime = CID_Bow;
		else if(id == 351) runtime = CID_Arrows;
		else if(id == 352) runtime = CID_Spear;
		else if(id == 353) runtime = CID_Javelin;
		if(!IsRuntimeAlias(id) && runtime >= RuntimeAliasFirst && runtime <= RuntimeAliasLast)
		{
			error = QStringLiteral("Legacy commodity %1 uses runtime ID %2 reserved for historical aliases. Assign an explicit unused runtime mapping.")
				.arg(id).arg(runtime);
			return false;
		}
		if(m_fromRuntime.contains(runtime))
		{
			error = QStringLiteral("Legacy commodities %1 and %2 both map to runtime ID %3. Assign an explicit unused runtime mapping.")
				.arg(m_fromRuntime.value(runtime)).arg(id).arg(runtime);
			return false;
		}
		m_toRuntime.insert(id, runtime);
		m_fromRuntime.insert(runtime, id);
		DbCommodity descriptor(record.descriptor);
		descriptor.m_id = COMMODITYID(runtime);
		if(id == 103 && descriptor.m_modelPath == QLatin1String(":data/mTank.3dsX"))
			descriptor.m_modelPath = QStringLiteral(":data/mTank.3ds"); // Our new Tank model.
		if(id == 54 && descriptor.m_modelPath == QLatin1String(":data/mMineMachinery.3dsX"))
			descriptor.m_modelPath = QStringLiteral(":data/mMineMachinery.3ds"); // Ikkir's model.
		m_runtimeDescriptors.insert(runtime, descriptor);
		m_runtimeIds.append(runtime);
	}
	std::sort(m_runtimeIds.begin(), m_runtimeIds.end());
	return true;
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
	const auto* process = ConstructionProcessForIndustry(industry);
	return BuildingDesignMaximumDeck(industry, material, process ? process->stackLimit : 99);
}

int AuLegacyCityCatalogue::BuildingDesignMaximumDeck(AuLegacyCity::IndustryId industry,
	AuLegacyCity::CommodityId material, int limit)
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
