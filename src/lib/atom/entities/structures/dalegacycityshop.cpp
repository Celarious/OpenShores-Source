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
#include "davessel.h"
#include "dacapsule.h"
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
const AuLegacyCityCatalogue* ShopCatalogue()
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !AuLegacyItemTechnologyActive() || !catalogue) return nullptr;
	for(const quint16 commodity : catalogue->RuntimeCommodityIds())
		if(!g_Au.HasLegacyCommodity(COMMODITYID(commodity))) return nullptr;
	return catalogue;
}

quint32 PersonalDevelopment(const DaLegacyCity& city, const DaPerson& person)
{
	if(person.IsDead() || person.IsInsideSpacecraft() || person.FindParentPlanetInEnv() != city.Parent()) return 0;
	const auto* environment = person.LockEnvironmentQuery();
	const quint32 development = environment->LegacyCityId() == city.Id() ? environment->LegacyDevelopmentId() : 0;
	person.UnlockEnvironmentQuery();
	return development;
}

struct GearWriteLock
{
	DaPerson* person;
	AuGear* gear;
	explicit GearWriteLock(DaPerson* buyer) : person(buyer), gear(buyer ? buyer->LockGearForWrite() : nullptr) {}
	~GearWriteLock() { if(person) person->UnlockGear(); }
};

}

bool DaLegacyCity::RxGoodsSaleMessage(const AuCommPacket* message)
{
	using namespace AuLegacyCity;
	if(!message || (message->Type() != AuCommPacket::Auction && message->Type() != AuCommPacket::ForSale)) return false;
	const auto* catalogue = ShopCatalogue();
	const auto* sector = FindParentSector();
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	auto* system = FindEnvironmentAtom();
	auto* person = system ? dynamic_cast<DaPerson*>(system->FindIdRecursive(message->FromId())) : nullptr;
	if(!catalogue || !world || !sector || !person || person->IsDead()) return true;
	auto* ship = person->IsInsideSpacecraft() ? person->FindParentVesselInEnv() : nullptr;
	const quint32 developmentId = ship ? 0 : PersonalDevelopment(*this, *person);
	const auto range = AuCommStdChannel::RANGE(qMin(int(message->ReceptionRange()), int(AuCommStdChannel::SolarSystem)));
	const auto reply = [&](const QString& text) {
		AuCommCityChat response(text, message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), range, message->Scope());
		g_Au.SendAtomChat(response);
	};
	if(ship ? !ship->InChainOfCommand(person) || !CanAccessSpacecraft(*ship) : !developmentId)
	{
		if(!AuCommStdChannel::IsTradeChannel(message->ChannelName()))
			reply(ship ? tr("An officer must bring the spacecraft onto a city slab, into transporter range, or within station trade range.")
				: tr("Stand at an industry in this city to sell personal items."));
		return true;
	}
	const AuItem* offered = message->Item();
	if(!offered || !offered->IsValid() || !offered->Count() || offered->Count() > quint32(std::numeric_limits<int>::max())) return true;
	const auto* commodity = catalogue->CommodityForRuntimeId(offered->CommodityId());
	if(!commodity) return true;
	if(offered->StoryInstance()) { reply(tr("The city does not buy story items.")); return true; }
	const State before = Snapshot();
	const quint32 sellerEmpire = ship ? ship->EmpireId() : person->EmpireId();
	const auto stance = PoliticalStance(before.controllingEmpire, sellerEmpire);
	const auto& data = g_Au.LockDataForRead();
	const bool restricted = data.EmpireCommerceRestricted(before.controllingEmpire, offered->CommodityId(), stance);
	g_Au.UnlockData();
	if(stance == POL_Enemy || restricted) { reply(tr("Commerce with this city is restricted.")); return true; }
	QWriteLocker lock(&m_CityLock);
	if(m_City.controllingEmpire != before.controllingEmpire) return true;
	const AuSpecies species(m_City.citizenSpecies);
	if(!ship)
	{
		bool wanted = false;
		for(const Development& development : m_City.developments)
			if(development.id == developmentId)
				wanted = IndustryBuysCommodity(*catalogue, development, commodity->id, species.EcoRole());
		if(!wanted) { lock.unlock(); reply(tr("This industry does not buy that item. Try an industry that makes or uses it.")); return true; }
	}
	AuItemHash inventory; ProductionState production; AccountingState accounting; QString error;
	if(!ReadCityInventory(m_City.inventory, *catalogue, inventory, error)
		|| !DecodeProductionState(*catalogue, m_City.manufacturing, production, error)
		|| !DecodeAccounting(m_City.accounting, accounting, error)) return true;
	quint32 price = CachedShopPrice(*catalogue, m_City, inventory, production, *offered, 255);
	// Government-issued supplies can be returned, but never sold for profit.
	if(ship && !ship->IsPrivatelyOwned() && (offered->IsFreeToFriendlyGovShip() || offered->IsPower())) price = 0;
	if(message->Type() == AuCommPacket::ForSale)
	{
		if(message->Price() > price) { lock.unlock(); reply(tr("Your asking price is too high. Auction the items to request our current price.")); return true; }
		price = message->Price(); // Includes the UI's offer-for-free option.
	}
	quint32 quantity = TestFitCityCommodity(*catalogue, inventory, *offered,
		CityStorage(*catalogue, m_City.developments, quint8(world->OrbitZone())), world->HasBreathableAtmosphere(), quint8(species.EcoRole()));
	if(!quantity) { lock.unlock(); reply(tr("We do not want more of that item; our stock is full or better goods are already available.")); return true; }
	if(price)
	{
		quantity = qMin(quantity, quint32(qMin(2000000000.0 / price, qMax(0.0, accounting.accounts.citizens / price))));
		if(!quantity) { lock.unlock(); reply(tr("The citizens cannot afford those items.")); return true; }
	}
	std::unique_ptr<AuItem> bidItem(offered->Clone()); bidItem->SetCount(quantity);
	lock.unlock();
	AuCommBid response(*bidItem, message->EmpireOfOrigin(), price * quantity, message->ChannelName(),
		Id(), Name(), sector->Id(), Galaxy(), range, message->Scope());
	g_Au.SendAtomChat(response);
	return true;
}

void DaLegacyCity::AcceptGoodsSale(DaPerson& person, const AuCommAccept& message)
{
	using namespace AuLegacyCity;
	if(person.IsInsideSpacecraft()) { AcceptSpacecraftCargo(person, message); return; }
	const auto* catalogue = ShopCatalogue();
	const auto* sector = FindParentSector();
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	const quint32 developmentId = PersonalDevelopment(*this, person);
	const AuItem* sold = message.Item();
	if(!catalogue || !sector || !world || !developmentId || IsDead() || !sold || !sold->IsValid()
		|| !sold->Count() || sold->StoryInstance() || !g_Au.HasPendingBid(&message)) return;
	const auto* commodity = catalogue->CommodityForRuntimeId(sold->CommodityId());
	if(!commodity) return;
	const auto reply = [&](const QString& text) {
		AuCommCityChat response(text, message.ChannelName(), Id(), Name(), sector->Id(), Galaxy(),
			message.ReceptionRange(), message.Scope()); g_Au.SendAtomChat(response);
	};
	const State before = Snapshot();
	const auto stance = PoliticalStance(before.controllingEmpire, person.EmpireId());
	const auto& data = g_Au.LockDataForRead();
	const bool restricted = data.EmpireCommerceRestricted(before.controllingEmpire, sold->CommodityId(), stance);
	g_Au.UnlockData();
	if(stance == POL_Enemy || restricted) { reply(tr("Commerce with this city is restricted.")); return; }
	AuLegacyFinanceTransfer finance(&person);
	GearWriteLock gearLock(&person);
	if(!gearLock.gear) return;
	AuGear consumed(*gearLock.gear);
	if(consumed.ConsumeItemsForSale(*sold) != sold->Count())
	{ reply(tr("You no longer have the offered items in your gear.")); return; }
	QWriteLocker lock(&m_CityLock);
	if(m_City.controllingEmpire != before.controllingEmpire || !g_Au.HasPendingBid(&message)) return;
	const AuSpecies species(m_City.citizenSpecies);
	bool wanted = false;
	for(const Development& development : m_City.developments)
		if(development.id == developmentId)
			wanted = IndustryBuysCommodity(*catalogue, development, commodity->id, species.EcoRole());
	if(!wanted) { lock.unlock(); reply(tr("This industry no longer accepts those items.")); return; }
	AuItemHash inventory; AccountingState accounting; QString error;
	if(!ReadCityInventory(m_City.inventory, *catalogue, inventory, error)
		|| !DecodeAccounting(m_City.accounting, accounting, error)) return;
	if(message.Price() && message.Price() > accounting.accounts.citizens)
	{ lock.unlock(); reply(tr("The citizens can no longer afford this bid. Request a new bid.")); return; }
	if(!AddCityCommodity(*catalogue, inventory, *sold, CityStorage(*catalogue, m_City.developments, quint8(world->OrbitZone())),
		world->HasBreathableAtmosphere(), quint8(species.EcoRole())))
	{ lock.unlock(); reply(tr("The city can no longer store these items. Request a new bid.")); return; }
	accounting.accounts.Debit(CitizenExpense::Purchases, message.Price());
	QByteArray inventoryBytes, accountingBytes;
	if(!WriteCityInventory(inventory, inventoryBytes, error) || !EncodeAccounting(accounting, accountingBytes, error)) return;
	m_City.inventory = inventoryBytes; m_City.accounting = accountingBytes; ++m_CityRevision;
	*gearLock.gear = consumed;
	g_Au.BidRevoked(Id(), message.ChannelName());
	lock.unlock();
	person.UpdateGearWeight(); person.CreditAccount(message.Price());
	person.Stats().ItemSoldAsInventory(*sold); person.IncreaseSkill(AuSkill::Trade, 1);
	person.CargoDelivered(Id(), *sold);
	SetModified();
	reply(message.Price() ? tr("Items sold to the city.") : tr("Items given to the city."));
}

// Called with m_CityLock held. Selling quotes remain stable for twenty minutes,
// including repeated requests, as in the recovered DaCity::PriceOffer.
quint32 DaLegacyCity::CachedShopPrice(const AuLegacyCityCatalogue& catalogue, const AuLegacyCity::State& state,
	const AuItemHash& inventory, const AuLegacyCity::ProductionState& production, const AuItem& item, quint8 industry)
{
	// Buying quotes follow current supply. Repeated sales must not refresh an
	// old high bid indefinitely as they fill the city's stores.
	if(industry == 255)
	{
		const quint32 demandCost = AuLegacyCity::ShopDemandCost(catalogue, state, inventory, production, item);
		return demandCost ? qMax(1u, quint32(quint64(demandCost) * 80 / 100)) : 0;
	}
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

void DaLegacyCity::RxChatMessage(const AuCommPacket* message, quint32& countSupplied, bool)
{
	using namespace AuLegacyCity;
	const auto* catalogue = ShopCatalogue();
	if(!message || !catalogue || IsDead()) return;
	LogReceivedCommunication(message);
	if(RxCivicChat(message) || RxStatusChat(message) || RxPersonnelChat(message, countSupplied) || RxSpacecraftBlueprintMessage(message)
		|| RxGoodsSaleMessage(message) || RxSpacecraftServiceMessage(message)) return;
	COMMODITYFILTER filter = FILTER_All;
	const bool solicit = SalesFilter(message->Type(), filter);
	const auto* bid = dynamic_cast<const AuCommBid*>(message);
	const auto* beam = dynamic_cast<const AuCommBeamOver*>(message);
	if(!solicit && !bid && !beam) return;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	const auto* sector = FindParentSector();
	if(!catalogue || !world || !sector) return;
	DaAtom* system = world->FindEnvironmentAtom();
	auto* person = system ? dynamic_cast<DaPerson*>(system->FindIdRecursive(message->FromId())) : nullptr;
	if(!person || person->IsDead()) return;
	if(beam)
	{
		if(!EmpireId() || person->EmpireId() != EmpireId()) return;
		int result = -1;
		for(int attempt = 0; attempt < 100; ++attempt)
		{
			result = BeamUp(*person, int(beam->Count()));
			if(result != 0) break;
		}
		if(result == -1) return; // No authored airport pads within range.
		const QString text = DaCapsule::BeamUpReply(person->IsAboardVehicle(), int(beam->Count()), result, Name());
		AuCommCityChat response(text, message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(),
			AuCommStdChannel::Hail, message->Scope());
		g_Au.SendAtomChat(response);
		return;
	}
	if(person->IsInsideSpacecraft() || person->FindParentPlanetInEnv() != world) return;
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
			if(!PackPersonalItem(packedGear, *bought)) break;
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
