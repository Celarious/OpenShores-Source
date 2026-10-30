#include "aulegacycityinventory.h"
#include "aulegacycitystate.h"
#include "auitem.h"
#include "auglobal.h"
#include <QDataStream>
#include <memory>

namespace AuLegacyCity
{
bool ReadCityInventory(const QByteArray& bytes, const AuLegacyCityCatalogue& catalogue,
	AuItemHash& output, QString& error)
{
	error.clear();
	// AuItem decoding consults generation-dependent descriptors immediately.
	// Reject even an empty snapshot until the complete immutable catalogue is
	// activated, so nested/tool inventories share the same readiness boundary.
	if(!catalogue.IsValid() || !AuLegacyItemTechnologyActive())
	{ error = QStringLiteral("Legacy commodity metadata is not ready."); return false; }
	for(const quint16 commodity : catalogue.RuntimeCommodityIds())
		if(!g_Au.HasLegacyCommodity(COMMODITYID(commodity)))
		{ error = QStringLiteral("Legacy commodity metadata is not ready."); return false; }
	AuItemHash inventory;
	if(bytes.isEmpty()) { output = inventory; return true; }
	if(bytes.size() > MaximumStateBytes / 2)
	{ error = QStringLiteral("Legacy city inventory exceeds its size limit."); return false; }
	QDataStream in(bytes); in.setVersion(QDataStream::Qt_4_1);
	quint32 count = 0; in >> count;
	// The smallest ordinary item has type, flags, commodity and quality.
	if(in.status() != QDataStream::Ok || count > 65535
		|| quint64(count) * 5 > quint64(in.device()->bytesAvailable()))
	{ error = QStringLiteral("Invalid legacy city inventory count."); return false; }
	for(quint32 index = 0; index < count; ++index)
	{
		quint8 type = 0, flags = 0; quint16 commodity = 0;
		in >> type;
		const qint64 itemStart = in.device()->pos();
		in >> flags >> commodity;
		if(in.status() != QDataStream::Ok || (flags & 0xc0) || !catalogue.CommodityForRuntimeId(commodity)
			|| !in.device()->seek(itemStart))
		{ error = QStringLiteral("Invalid legacy city inventory commodity."); return false; }
		std::unique_ptr<AuItem> item(AuItem::Create(type));
		if(!item) { error = QStringLiteral("Invalid legacy city inventory item type."); return false; }
		in >> *item;
		if(in.status() != QDataStream::Ok || !item->IsValid() || item->Count() > 0x7fffffffU)
		{ error = QStringLiteral("Malformed legacy city inventory item."); return false; }
		if(item->Count())
		{
			const auto commodityId = item->CommodityId();
			inventory.insert(std::make_pair(commodityId, item.release()));
		}
	}
	if(in.status() != QDataStream::Ok || !in.atEnd())
	{ error = QStringLiteral("Trailing data in legacy city inventory."); return false; }
	output = inventory;
	return true;
}

bool WriteCityInventory(const AuItemHash& inventory, QByteArray& output, QString& error)
{
	error.clear();
	if(inventory.size() > 65535)
	{ error = QStringLiteral("Legacy city inventory has too many stacks."); return false; }
	QByteArray bytes; QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_4_1); out << inventory;
	if(out.status() != QDataStream::Ok || bytes.size() > MaximumStateBytes / 2)
	{ error = QStringLiteral("Legacy city inventory exceeds its size limit."); return false; }
	output = bytes;
	return true;
}

bool IsUpgradeModule(CommodityId historicalId)
{
	// DbCommodity::IsUpgradeModule, AuGlobal13 0x8e70.
	const int offset = int(historicalId.value) - 234;
	return historicalId.value == 319 || (offset >= 0 && offset <= 57
		&& ((Q_UINT64_C(0x282c0003fffffff) >> offset) & 1));
}

bool IsObsoleteCityItem(const AuLegacyCityCatalogue& catalogue, const AuItemHash& inventory,
	const AuItem& item, bool hasBreathableAtmosphere, quint8 ecologicalRole, quint32 storageCapacity)
{
	// AuData13 0xccea0. The modern helper uses quality for obsolescence;
	// this rule uses original technology levels and treats patents separately.
	const auto* commodity = catalogue.CommodityForRuntimeId(item.CommodityId());
	if(!commodity) return true;
	const quint16 historical = commodity->id.value;
	if((!hasBreathableAtmosphere && (historical == 5 || historical == 83))
		|| commodity->descriptor.IsFood(ECOROLE(ecologicalRole))) return false;
	const bool technology = commodity->IsTechnology() || IsUpgradeModule(commodity->id);
	quint64 betterTechnology = 0, twoLevelsBetter = 0;
	for(const auto& entry : inventory)
	{
		const AuItem& stored = *entry.second;
		if(stored.CommodityId() != item.CommodityId()) continue;
		if(stored.TechLevel() > item.TechLevel()) betterTechnology += stored.Count();
		if(stored.TechLevel() > int(item.TechLevel()) + 1)
		{
			twoLevelsBetter += stored.Count();
			if(stored.Count() >= item.Count()) return true;
		}
	}
	if(technology && betterTechnology > item.Count()) return true;
	if(!technology && twoLevelsBetter > storageCapacity / 2) return true;
	return twoLevelsBetter > item.Count();
}

void DiscardObsoleteCityItems(const AuLegacyCityCatalogue& catalogue, AuItemHash& inventory,
	bool hasBreathableAtmosphere, quint8 ecologicalRole, quint32 storageCapacity)
{
	// AuData13 0xc4410 removes each obsolete stack completely, unlike the
	// modern ten-percent discard. Later comparisons use the remaining stock.
	for(auto item = inventory.begin(); item != inventory.end();)
		if(IsObsoleteCityItem(catalogue, inventory, *item->second,
			hasBreathableAtmosphere, ecologicalRole, storageCapacity)) item = inventory.Erase(item);
		else ++item;
}

quint32 TestFitCityCommodity(const AuLegacyCityCatalogue& catalogue, const AuItemHash& inventory,
	const AuItem& item, const StorageCapacities& capacity, bool hasBreathableAtmosphere, quint8 ecologicalRole)
{
	// DaCity::TestFitCommodity 0x2d2640 subtracts equal-or-better stock
	// rather than all stock, permitting replacement of lower TL/quality items.
	const auto* commodity = catalogue.CommodityForRuntimeId(item.CommodityId());
	if(!commodity || !item.Count() || item.Count() > 0x7fffffffU
		|| IsObsoleteCityItem(catalogue, inventory, item, hasBreathableAtmosphere, ecologicalRole, capacity.cargo)) return 0;
	const auto existing = inventory.Find(item);
	if(existing != inventory.cend() && quint64(existing->second->Count()) + item.Count() > 0x7ffffffeU) return 0;
	const quint32 limit = commodity->id.value == 3 ? capacity.electricity : capacity.cargo;
	quint64 better = 0;
	for(const auto& entry : inventory)
	{
		const AuItem& stored = *entry.second;
		if(stored.CommodityId() == item.CommodityId()
			&& (stored.TechLevel() > item.TechLevel()
				|| (stored.TechLevel() == item.TechLevel() && stored.Quality() >= item.Quality()))) better += stored.Count();
	}
	// Retain the original unsigned subtraction when capacity has fallen below
	// existing stock. AddCommodity's subsequent trim enforces the new limit.
	const quint32 freeCapacity = limit - quint32(better);
	return qMin(item.Count(), freeCapacity);
}

bool AddCityCommodity(const AuLegacyCityCatalogue& catalogue, AuItemHash& inventory,
	const AuItem& item, const StorageCapacities& capacity, bool hasBreathableAtmosphere, quint8 ecologicalRole)
{
	if(TestFitCityCommodity(catalogue, inventory, item, capacity, hasBreathableAtmosphere, ecologicalRole) != item.Count()
		|| !item.Count()) return false;
	const auto* commodity = catalogue.CommodityForRuntimeId(item.CommodityId());
	inventory.AddItemLimit(item, commodity->id.value == 3 ? capacity.electricity : capacity.cargo);
	return true;
}
}
