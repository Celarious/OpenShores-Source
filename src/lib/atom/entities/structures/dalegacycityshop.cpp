#include "dalegacycity.h"
#include "aucommpacket.h"
#include "auglobal.h"
#include "augear.h"
#include "auitem.h"
#include "aulegacycityinventory.h"
#include "aulegacycitypopulation.h"
#include "aulegacycityproduction.h"
#include "aulegacycityshop.h"
#include "aulegacyfinancetransfer.h"
#include "auplayerstats.h"
#include "auqueryenvironment.h"
#include "auspecies.h"
#include "daperson.h"
#include "dasector.h"
#include "daworldll.h"
#include "dgdata.h"
#include <QDataStream>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QWriteLocker>
#include <algorithm>
#include <limits>
#include <memory>

namespace
{
bool SalesFilter(AuCommPacket::TYPE type, COMMODITYFILTER& filter)
{
	switch(type)
	{
	case AuCommPacket::SolicitAllSales: case AuCommPacket::SolicitItemSales: filter = FILTER_All; break;
	case AuCommPacket::SolicitAmmoSales: filter = FILTER_Ammo; break;
	case AuCommPacket::SolicitArmorSales: filter = FILTER_ArmorAndClothes; break;
	case AuCommPacket::SolicitConstructionMaterialSales: filter = FILTER_ConstructionMaterials; break;
	case AuCommPacket::SolicitFoodSales: filter = FILTER_Food; break;
	case AuCommPacket::SolicitFuelSales: filter = FILTER_Fuel; break;
	case AuCommPacket::SolicitMedSales: filter = FILTER_MedicalSupplies; break;
	case AuCommPacket::SolicitMiscSales: filter = FILTER_Misc; break;
	case AuCommPacket::SolicitNaturalResourceSales: filter = FILTER_NaturalResources; break;
	case AuCommPacket::SolicitPartsSales: filter = FILTER_SpareParts; break;
	case AuCommPacket::SolicitPatentSales: filter = FILTER_Patents; break;
	case AuCommPacket::SolicitToolSales: filter = FILTER_Tools; break;
	case AuCommPacket::SolicitWeaponSales: filter = FILTER_Weapons; break;
	default: return false;
	}
	return true;
}

quint32 SaleStock(const AuItem& item, const AuLegacyCityCatalogue& catalogue,
	const AuLegacyCity::State& state, bool fauna, bool flora)
{
	const auto* commodity = catalogue.CommodityForRuntimeId(item.CommodityId());
	if(!commodity) return 0;
	// TxForSaleChat, 0x2d6de8: retain life support and a food reserve.
	quint64 reserve = 0;
	if(!fauna && (commodity->id.value == 5 || commodity->id.value == 83)) reserve = quint64(state.population) * 13;
	else if(item.IsFood() && item.FoodNutritionValue()) reserve = quint64(state.population) * (flora ? 1 : 10);
	return item.Count() > reserve ? quint32(item.Count() - reserve) : 0;
}

// DaSentient::AddGearItemNoLock's packing order, applied to a private copy.
// Actor callbacks would refresh environment state and re-enter the city.
bool PackShopItem(AuGear& gear, const AuItem& item)
{
	if(!item.CanBeCarried()) return false;
	for(const AuGearSlot::SLOTTYPE slot : {AuGearSlot::Body, AuGearSlot::Hand, AuGearSlot::Waist})
		for(auto gearItem = gear.begin(); gearItem != gear.end(); ++gearItem)
			if(gearItem->SlotType() == slot && gearItem->Item().FitItemIntoContainer(item)) return true;
	AuGearSlot::SLOTTYPE slot = AuGearSlot::Invalid;
	quint8 which = 0;
	if(!gear.ItemsInSlot(AuGearSlot::Body, &which)) slot = AuGearSlot::Body;
	else if(gear.ItemsInSlot(AuGearSlot::Hand, &which) < 2) slot = AuGearSlot::Hand;
	else if(item.PackSize().width() * item.PackSize().height() <= 15
		&& gear.ItemsInSlot(AuGearSlot::Waist, &which) < 4) slot = AuGearSlot::Waist;
	if(slot == AuGearSlot::Invalid) return false;
	gear.Insert(slot, which, item);
	return true;
}

struct GearWriteLock
{
	DaPerson* person;
	AuGear* gear;
	explicit GearWriteLock(DaPerson* buyer) : person(buyer), gear(buyer ? buyer->LockGearForWrite() : nullptr) {}
	~GearWriteLock() { if(person) person->UnlockGear(); }
};
}

// Called with m_CityLock held. Quotes remain stable for twenty minutes,
// including repeated requests, as in the recovered DaCity::PriceOffer.
quint32 DaLegacyCity::CachedShopPrice(const AuLegacyCityCatalogue& catalogue, const AuLegacyCity::State& state,
	const AuItemHash& inventory, const AuLegacyCity::ProductionState& production, const AuItem& item, quint8 industry)
{
	QByteArray key; QDataStream out(&key, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_4_1);
	out << quint16(item.CommodityId()) << item.Quality() << item.TechLevel() << item.EmpireOfOrigin() << industry;
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	for(auto quote = m_ShopPrices.begin(); quote != m_ShopPrices.end();)
		if(now - quote.value().second >= 20 * 60000) quote = m_ShopPrices.erase(quote); else ++quote;
	auto quote = m_ShopPrices.find(key);
	if(quote != m_ShopPrices.end()) { quote.value().second = now; return quote.value().first; }
	const quint32 price = AuLegacyCity::ShopUnitPrice(catalogue, state, inventory, production, item, industry);
	m_ShopPrices.insert(key, qMakePair(price, now));
	return price;
}

void DaLegacyCity::RxChatMessage(const AuCommPacket* message, quint32&, bool)
{
	using namespace AuLegacyCity;
	if(!message || !g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !AuLegacyItemTechnologyActive() || IsDead()) return;
	COMMODITYFILTER filter = FILTER_All;
	const bool solicit = SalesFilter(message->Type(), filter);
	const auto* bid = dynamic_cast<const AuCommBid*>(message);
	if(!solicit && !bid) return;
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	const auto* sector = FindParentSector();
	if(!catalogue || !world || !sector) return;
	// The generation bootstrap must install and activate descriptors before
	// an inventory stream can create its first item, including Legacy aliases.
	for(const quint16 commodity : catalogue->RuntimeCommodityIds())
		if(!g_Au.HasLegacyCommodity(COMMODITYID(commodity))) return;
	auto* person = dynamic_cast<DaPerson*>(world->FindIdRecursive(message->FromId()));
	if(!person || person->IsDead() || person->IsInsideSpacecraft() || person->FindParentPlanetInEnv() != world) return;
	AuLegacyFinanceTransfer transfer(person);
	const auto* environment = person->LockEnvironmentQuery();
	const bool inCity = environment->LegacyCityId() == Id();
	const quint32 developmentId = environment->LegacyDevelopmentId();
	person->UnlockEnvironmentQuery();
	if(!inCity || !developmentId) return;
	if(bid && (!bid->Item() || !bid->Item()->Count() || !bid->Item()->IsValid() || !g_Au.HasPendingBid(bid))) return;
	const State before = Snapshot();
	const AuSpecies citizens(before.citizenSpecies);
	const ECOROLE diet = citizens.IsNull() ? ECOROLE(0) : citizens.EcoRole();
	const bool fauna = world->CanHaveFauna(), flora = world->CanHaveFlora();
	const double personalBalance = person->AccountBalance();
	const POLITICAL_STANCE stance = PoliticalStance(before.controllingEmpire, person->EmpireId());
	if(before.controllingEmpire && stance == POL_Enemy) return;
	QSet<quint16> restricted;
	QHash<quint32, QString> origins;
	AuId capital;
	const DgData& gd = g_Au.LockDataForRead();
	const bool hasCapital = gd.EmpireCapital(before.controllingEmpire, capital) && !capital.IsNull();
	for(const quint16 commodity : catalogue->RuntimeCommodityIds())
		if(gd.EmpireCommerceRestricted(before.controllingEmpire, COMMODITYID(commodity), stance)) restricted.insert(commodity);
	g_Au.UnlockData();
	AuCommStdChannel::RANGE range = message->ReceptionRange();
	if(range > AuCommStdChannel::SolarSystem) range = AuCommStdChannel::SolarSystem;
	const QString cityName = Name();
	const auto reply = [&](const QString& text) {
		AuCommCityChat response(text, message->ChannelName(), Id(), cityName, sector->Id(), Galaxy(), range, message->Scope());
		g_Au.SendAtomChat(response);
	};
	// Normal gear actions can query city environment state while holding this
	// lock. Preserve that order, and defer callbacks until the city is unlocked.
	GearWriteLock gearLock(bid ? person : nullptr);
	QWriteLocker lock(&m_CityLock);
	if(before.controllingEmpire != m_City.controllingEmpire || before.citizenSpecies != m_City.citizenSpecies) return;
	const Development* selected = nullptr;
	for(const Development& development : m_City.developments)
		if(development.id == developmentId && development.completedLevels && !development.demolished)
		{ selected = &development; break; }
	if(!selected || !HasShop(*catalogue, selected->industryId)) return;
	AuItemHash inventory; ProductionState production; AccountingState accounting; QString error;
	if(!ReadCityInventory(m_City.inventory, *catalogue, inventory, error)
		|| !DecodeProductionState(*catalogue, m_City.manufacturing, production, error)
		|| !DecodeAccounting(m_City.accounting, accounting, error)) return;
	const auto saleable = [&](const AuItem& item) {
		const auto* commodity = catalogue->CommodityForRuntimeId(item.CommodityId());
		return commodity && item.CanBeCarried() && !restricted.contains(quint16(item.CommodityId()))
			&& IndustrySellsCommodity(*catalogue, selected->industryId, commodity->id, diet);
	};
	if(bid)
	{
		const AuItem& requested = *bid->Item();
		auto item = inventory.Find(requested);
		if(item == inventory.end() || !saleable(*item->second))
		{ lock.unlock(); reply(tr("This shop does not have that item for sale.")); return; }
		if(SaleStock(*item->second, *catalogue, m_City, fauna, flora) < requested.Count())
		{ lock.unlock(); reply(tr("We no longer have that quantity in stock.")); return; }
		const quint32 price = CachedShopPrice(*catalogue, m_City, inventory, production, *item->second, selected->industryId);
		if(!price) { lock.unlock(); reply(tr("A price is not available for that item.")); return; }
		if(quint64(price) * requested.Count() > bid->Price())
		{ lock.unlock(); reply(tr("Your offer is too low. Request a new sale offer for the current price.")); return; }
		if(bid->Price() > personalBalance)
		{ lock.unlock(); reply(tr("Your personal account does not have enough money.")); return; }
		std::unique_ptr<AuItem> bought(item->second->Clone()); bought->SetCount(1);
		// Stage the gear along with city cargo/accounts so failure cannot leave
		// free merchandise. The original purchase fills gear only as far as it fits.
		if(!gearLock.gear) return;
		AuGear packedGear = *gearLock.gear;
		quint32 delivered = 0;
		for(; delivered < requested.Count(); ++delivered)
			if(!PackShopItem(packedGear, *bought)) break;
		if(!delivered)
		{ lock.unlock(); reply(tr("Your gear has no room for that item.")); return; }
		bought->SetCount(delivered);
		const quint32 charge = delivered == requested.Count() ? bid->Price()
			: quint32((quint64(bid->Price()) * delivered) / requested.Count());
		item->second->Consume(delivered);
		if(!item->second->Count()) inventory.Erase(item);
		accounting.taxes.hasCapital = hasCapital;
		accounting.accounts.Credit(CitizenIncome::Sales, double(charge), accounting.taxes);
		QByteArray inventoryBytes, accountingBytes;
		if(!WriteCityInventory(inventory, inventoryBytes, error) || !EncodeAccounting(accounting, accountingBytes, error))
		{
			lock.unlock(); reply(tr("The purchase could not be completed. Request the shop's stock again.")); return;
		}
		m_City.inventory = inventoryBytes; m_City.accounting = accountingBytes; ++m_CityRevision;
		*gearLock.gear = packedGear;
		lock.unlock();
		person->UpdateGearWeight();
		person->DebitAccount(charge);
		person->Stats().ItemPossessed(*bought);
		person->Stats().ItemBoughtAsInventory(*bought);
		g_Au.BidRevoked(bid->FromId(), bid->ChannelName());
		SetModified();
		AuCommAccept accepted(person->Id(), person->Name(), *bought, bid->EmpireOfOrigin(), charge,
			message->ChannelName(), Id(), cityName, sector->Id(), Galaxy(), range, message->Scope());
		g_Au.SendAtomChat(accepted);
		if(delivered < requested.Count()) reply(tr("Your gear had room for only %L1 of the items.").arg(delivered));
		return;
	}

	DbCommodityArray activeCommodities;
	for(const quint16 id : catalogue->RuntimeCommodityIds())
		if(const DbCommodity* descriptor = catalogue->RuntimeCommodity(id)) activeCommodities.append(*descriptor);
	QList<COMMODITYID> filtered;
	const bool specific = message->Type() == AuCommPacket::SolicitItemSales;
	if(specific) filtered.append(message->CommodityId());
	else activeCommodities.ListFilterCommodities(filtered, filter);
	QMap<QString, QPair<AuItemHash, quint32>> offers;
	for(const auto& entry : inventory)
	{
		const AuItem& item = *entry.second;
		if(!saleable(item) || !filtered.contains(item.CommodityId())) continue;
		const quint32 available = SaleStock(item, *catalogue, m_City, fauna, flora);
		if(!available) continue;
		QString key = item.ListName();
		if(specific) key += QStringLiteral("/%1/%2/%3").arg(item.TechLevel()).arg(item.Quality()).arg(item.EmpireOfOrigin());
		auto previous = offers.find(key);
		if(previous != offers.end() && !previous.value().first.empty()
			&& !item.IsBetter(*previous.value().first.begin()->second)) continue;
		AuItemHash offer;
		std::unique_ptr<AuItem> copy(item.Clone()); copy->SetCount(available);
		offer.insert(std::make_pair(copy->CommodityId(), copy.get())); copy.release();
		const quint32 price = CachedShopPrice(*catalogue, m_City, inventory, production, item, selected->industryId);
		if(!price) continue;
		offers.insert(key, qMakePair(offer, price));
	}
	lock.unlock();
	if(offers.isEmpty())
	{ if(!AuCommStdChannel::IsTradeChannel(message->ChannelName())) reply(tr("We have no matching goods for sale.")); return; }
	QString channel = message->ChannelName();
	if(AuCommStdChannel::IsTradeChannel(channel))
	{
		channel = QStringLiteral("%1 %2-%3").arg(cityName).arg(Id().StringId()).arg(person->Id().StringId());
		AuCommCityMessage notice(AuCommCityMessage::ItemsOnPrivateChannel, message->ChannelName(), Id(), cityName,
			sector->Id(), Galaxy(), range, message->Scope());
		g_Au.SendAtomChat(notice);
	}
	g_Au.OpenChannel(channel, AuCommStdChannel::Everyone, Id(), cityName, sector->Id(), Galaxy(), person->Id(), false);
	for(auto offer = offers.cbegin(); offer != offers.cend(); ++offer)
	{
		const AuItem& item = *offer.value().first.begin()->second;
		QString origin;
		if(!origins.contains(item.EmpireOfOrigin()))
		{
			const DgData& data = g_Au.LockDataForRead(); data.EmpireName(item.EmpireOfOrigin(), origin); g_Au.UnlockData();
			origins.insert(item.EmpireOfOrigin(), origin);
		}
		AuCommForSale response(item, origins.value(item.EmpireOfOrigin()), offer.value().second, channel,
			Id(), cityName, sector->Id(), Galaxy(), range, AuCommStdChannel::Everyone);
		g_Au.SendAtomChat(response);
	}
}
