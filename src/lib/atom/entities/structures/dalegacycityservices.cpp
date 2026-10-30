#include "dalegacycity.h"
#include "auatompos.h"
#include "aucommpacket.h"
#include "auglobal.h"
#include "auitem.h"
#include "aulegacycityinventory.h"
#include "aulegacycitypopulation.h"
#include "aulegacycityproduction.h"
#include "aulegacycityshop.h"
#include "aulegacyfinancetransfer.h"
#include "auplayerstats.h"
#include "auqueryenvironment.h"
#include "dasector.h"
#include "daatomlist.h"
#include "daperson.h"
#include "davessel.h"
#include "daworld.h"
#include "daworldll.h"
#include "dgdata.h"
#include <QReadLocker>
#include <QWriteLocker>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace
{
const AuLegacyCityCatalogue* ServiceCatalogue()
{
	if(!g_Au.IsLegacyGeneration() || !AuLegacyItemTechnologyActive()) return nullptr;
	const AuLegacyCityCatalogue* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue) return nullptr;
	for(const quint16 commodity : catalogue->RuntimeCommodityIds())
		if(!g_Au.HasLegacyCommodity(COMMODITYID(commodity))) return nullptr;
	return catalogue;
}

bool HasService(const AuLegacyCity::State& state, quint8 industry, quint8 minimumTechnology = 0)
{
	for(const AuLegacyCity::Development& development : state.developments)
		if(!development.demolished && development.completedLevels && development.industryId == industry
			&& development.technology >= minimumTechnology) return true;
	return false;
}

// The stored ship's world location identifies its hangar in the 2016 world
// methods. Ground ships use their projected CG, independently of their height.
quint32 SiteAt(const DaWorldLL& world, const AuLegacyCity::State& state, const AuPoint& location,
	quint8 industry, const AuLegacyCityCatalogue& catalogue, bool slabOnly = false)
{
	AuGlobeLLF ll;
	if(!world.LLOfLoc(ll, location)) return 0;
	for(const AuLegacyCity::Development& development : state.developments)
	{
		if(development.industryId != industry || development.demolished || !development.completedLevels
			|| (slabOnly && !development.roadSlab)) continue;
		AuMatrix4x4 placement;
		world.BuildingConstructionTransform(world.LocOfLL(development.location, development.altitudeMSL),
			development.facingRadians, nullptr, AuPoint(), placement);
		const AuPoint local = placement.UnProject(world.LocOfLL(ll, development.altitudeMSL));
		const AuLegacyCity::ConstructionProcessRecord* recipe = catalogue.ConstructionProcessForIndustry(
			AuLegacyCity::IndustryId(development.industryId));
		const double halfLength = industry == 2 ? development.roadLengthMeters * FeetPerMeter() / 2
			: recipe ? recipe->buildRadius * FeetPerMeter() : 0;
		const double halfWidth = industry == 2 ? development.roadWidthMeters * FeetPerMeter() / 2 : halfLength;
		if(halfLength > 0 && world.LocOfLL(ll, development.altitudeMSL).Distance2(placement.Translation())
			< std::pow(2 * (qMax(halfWidth, halfLength) + 32.8084), 2)
			&& std::abs(local.x) <= halfWidth && std::abs(local.y) <= halfLength)
			return development.id;
	}
	return 0;
}

bool GroundService(const DaLegacyCity& city, const DaVessel& ship)
{
	const DaWorldLL* world = dynamic_cast<const DaWorldLL*>(city.Parent());
	const AuLegacyCityCatalogue* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!world || !catalogue || !ship.IsOnGround() || ship.FindParentPlanetInEnv() != world) return false;
	AuPoint location;
	return ship.GetLocationRelativeToParent(location, world)
		&& SiteAt(*world, city.Snapshot(), location, 2, *catalogue, true);
}

bool StationService(const DaLegacyCity& city, const DaVessel& ship)
{
	if(ship.IsInAtmosphere() || !city.HasSpacecraftService(111)
		|| ship.FindEnvironmentAtom() != city.FindEnvironmentAtom()) return false;
	bool hasStation = false;
	return ship.StationInTradeRange(ship.RoughRadius(), hasStation) != nullptr;
}

bool FreeService(const DaVessel& ship, quint32 empire)
{
	const POLITICAL_STANCE stance = PoliticalStance(empire, ship.EmpireId());
	return !ship.IsPrivatelyOwned() && (stance == POL_Empire || stance == POL_Friend || stance == POL_Vassal);
}

quint32 EligibleCount(const AuItemHash& inventory, COMMODITYID commodity, quint8 technology, quint8 quality)
{
	quint64 total = 0;
	for(auto item = inventory.find(commodity); item != inventory.cend() && item->first == commodity; ++item)
		if(item->second->TechLevel() >= technology && item->second->Quality() >= quality) total += item->second->Count();
	return quint32(qMin(total, quint64(std::numeric_limits<quint32>::max())));
}

quint32 ConsumeParts(AuItemHash& inventory, COMMODITYID commodity, quint8 technology, quint8 quality, quint32 wanted)
{
	const quint32 total = qMin(wanted, EligibleCount(inventory, commodity, technology, quality));
	quint32 remaining = total;
	for(auto item = inventory.find(commodity); item != inventory.end() && item->first == commodity && remaining;)
	{
		if(item->second->TechLevel() < technology || item->second->Quality() < quality) { ++item; continue; }
		const quint32 count = qMin(remaining, item->second->Count());
		item->second->Consume(count); remaining -= count;
		if(!item->second->Count()) { auto removed = item++; inventory.Erase(removed); }
		else ++item;
	}
	return total;
}

}

bool DaLegacyCity::HasSpacecraftService(quint8 industry, quint8 minimumTechnology) const
{
	return g_Au.IsLegacyGeneration() && !IsDead() && HasService(Snapshot(), industry, minimumTechnology);
}

bool DaLegacyCity::CanAccessSpacecraft(const DaVessel& ship) const
{
	if(!g_Au.IsLegacyGeneration() || IsDead() || ship.IsDead() || ship.InDesignEnvironment()) return false;
	if(GroundService(*this, ship) || StationService(*this, ship)) return true;
	const DaWorldLL* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world || ship.FindParentPlanetInEnv() != world) return false;
	AuPoint location;
	if(!ship.GetLocationRelativeToParent(location, world)) return false;
	const double range = ship.MaxTransporterRange();
	if(range > 0)
	{
		const AuQueryEnvironment* environment = ship.LockEnvironmentQuery();
		const bool inCity = environment->LegacyCityId() == Id();
		ship.UnlockEnvironmentQuery();
		if(inCity) return true;
	}
	return range > 0 && location.Distance2(Transform().Translation()) < range * range;
}

bool DaLegacyCity::IsSpacecraftOnSlab(const DaVessel& ship) const
{
	return g_Au.IsLegacyGeneration() && GroundService(*this, ship);
}

// Return the real cost (or -1 when unaffordable). The same reservation and
// calculation serve accepted human offers and automatic mission repairs.
int DaLegacyCity::RepairSpacecraft(DaVessel& ship, const AuSysId* system, quint8 minimumQuality,
	quint32 maximumPrice, bool perform, quint16* seconds)
{
	using namespace AuLegacyCity;
	if(seconds) *seconds = 0;
	const AuLegacyCityCatalogue* catalogue = ServiceCatalogue();
	if(!catalogue || !g_Au.IsSceneWorker() || !CanAccessSpacecraft(ship)
		|| (!GroundService(*this, ship) && !StationService(*this, ship))) return 0;
	const quint8 technology = ship.LegacyMinimumRepairTechnology();
	if(!HasSpacecraftService(110, technology)) return 0;
	const quint32 empire = EmpireId();
	const POLITICAL_STANCE stance = PoliticalStance(empire, ship.EmpireId());
	const bool subsidized = FreeService(ship, empire);
	const DgData& gd = g_Au.LockDataForRead();
	const bool restricted = gd.EmpireCommerceRestricted(empire, CID_Sunlight, stance);
	g_Au.UnlockData();
	if(restricted) return 0;
	AuLegacyFinanceTransfer finance(&ship);
	const bool automaticStation = ship.IsSpaceStation() && !ship.IsPrivatelyOwned() && ship.EmpireId() == empire;
	if(perform && !automaticStation && !ship.IsReadyForRepairs()) return 0;
	quint32 hits = 0; AuRepairParts required;
	if(system) ship.ListRepairsNeeded(*system, hits, required);
	else ship.ListRepairsNeeded(hits, required);
	QWriteLocker lock(&m_CityLock);
	if(m_City.controllingEmpire != empire || !HasService(m_City, 110, technology)) return 0;
	AuItemHash inventory; AccountingState accounting; QString error;
	if(!ReadCityInventory(m_City.inventory, *catalogue, inventory, error)
		|| !DecodeAccounting(m_City.accounting, accounting, error)) return 0;
	quint64 cost = hits, duration = (quint64(hits) + 3) / 4;
	AuRepairParts supplied;
	for(auto part = required.cbegin(); part != required.cend(); ++part)
	{
		const quint32 available = qMin(part.value(), EligibleCount(inventory, part.key(), technology, minimumQuality));
		if(!available) continue;
		auto best = inventory.FindBest(part.key());
		if(best == inventory.end()) continue;
		// Original RepairCostPossible0x2b3b60 uses the best stored Q and
		// the ship's minimum installed TL for price, then one dollar of labor.
		cost += quint64(g_Au.CommodityQualityCost(part.key(), best->second->Quality(), technology) * available) + available;
		duration += qMax(quint64(5), (quint64(available) + 3) / 4);
		supplied.insert(part.key(), available);
	}
	if(!hits && supplied.isEmpty()) return 0;
	duration = qMin(duration, quint64(65535));
	if(seconds) *seconds = quint16(duration);
	if(subsidized) cost = 0;
	const quint32 price = quint32(qMin(cost, quint64(std::numeric_limits<int>::max())));
	if(!perform) return int(price);
	if(price > maximumPrice || price > ship.AccountBalance()) return -1;
	for(auto part = supplied.cbegin(); part != supplied.cend(); ++part)
		ConsumeParts(inventory, part.key(), technology, minimumQuality, part.value());
	accounting.accounts.Credit(CitizenIncome::Sales, price, accounting.taxes);
	QByteArray inventoryBytes, accountingBytes;
	if(!WriteCityInventory(inventory, inventoryBytes, error) || !EncodeAccounting(accounting, accountingBytes, error)) return 0;
	m_City.inventory = inventoryBytes; m_City.accounting = accountingBytes; ++m_CityRevision;
	lock.unlock();
	ship.DebitAccount(price);
	ship.ApplyServiceRepairs(supplied, system);
	if(!automaticStation) ship.AddRepairTime(unsigned(duration));
	ship.SetModified(); SetModified();
	return qMax(1, int(price));
}

int DaLegacyCity::TransferSpacecraftCargo(DaVessel& ship, COMMODITYID commodity, int count, bool buy,
	bool mission, const AuItem* exact, quint32 maximumPrice)
{
	using namespace AuLegacyCity;
	const AuLegacyCityCatalogue* catalogue = ServiceCatalogue();
	DaWorldLL* world = dynamic_cast<DaWorldLL*>(Parent());
	if(!catalogue || !world || !g_Au.IsSceneWorker() || count <= 0 || !CanAccessSpacecraft(ship)) return 0;
	const CommodityRecord* descriptor = catalogue->CommodityForRuntimeId(quint16(commodity));
	if(!descriptor || !descriptor->descriptor.IsBrokerItem()) return 0;
	const quint32 empire = EmpireId();
	const POLITICAL_STANCE stance = PoliticalStance(empire, ship.EmpireId());
	const bool subsidized = FreeService(ship, empire);
	const DgData& gd = g_Au.LockDataForRead();
	const bool restricted = gd.EmpireCommerceRestricted(empire, commodity, stance);
	g_Au.UnlockData();
	if(restricted || stance == POL_Enemy) return 0;
	AuLegacyFinanceTransfer finance(&ship);
	AuItemHash shipInventory;
	if(!buy)
	{
		if(commodity == CID_Hydrogen)
		{
			std::unique_ptr<AuItem> fuel(AuItem::Create(commodity, ship.FuelQuality(), ship.EmpireId(), qMin(count, int(ship.FuelQuantity()))));
			if(fuel && fuel->Count()) shipInventory.AddItem(*fuel);
		}
		else if(commodity == CID_Electricity)
		{
			std::unique_ptr<AuItem> power(AuItem::Create(commodity, 255, 0, qMin(count, int(ship.CapacitorLevel()))));
			if(power && power->Count()) shipInventory.AddItem(*power);
		}
		else { shipInventory = ship.LockCargoForRead(); ship.UnlockCargo(); }
	}
	QWriteLocker lock(&m_CityLock);
	if(m_City.controllingEmpire != empire) return 0;
	AuItemHash inventory; ProductionState production; AccountingState accounting; QString error;
	if(!ReadCityInventory(m_City.inventory, *catalogue, inventory, error)
		|| !DecodeProductionState(*catalogue, m_City.manufacturing, production, error)
		|| !DecodeAccounting(m_City.accounting, accounting, error)) return 0;
	AuItemHash& source = buy ? inventory : shipInventory;
	auto item = exact ? source.Find(*exact) : source.FindBest(commodity);
	if(item == source.end()) return 0;
	std::unique_ptr<AuItem> cargo(item->second->Clone());
	if(cargo->StoryInstance()) return 0;
	const bool free = buy && cargo->IsFreeToFriendlyGovShip() && subsidized;
	if(!buy && !ship.IsPrivatelyOwned() && cargo->IsFreeToFriendlyGovShip()) return 0;
	quint32 unitPrice = free ? 0 : buy ? CachedShopPrice(*catalogue, m_City, inventory, production, *cargo, 111)
		: CachedShopPrice(*catalogue, m_City, inventory, production, *cargo, 255);
	if(mission && !free)
	{
		const int marginType = ship.MissionOrderPriceMarginType();
		const double limit = marginType == 0 ? cargo->UnitCost() * ship.MissionOrderPriceMarginValue() / 100
			: ship.MissionOrderPriceMarginValue();
		if(marginType != 2 && (buy ? unitPrice > limit : unitPrice < limit)) return 0;
	}
	quint32 quantity = qMin(quint32(count), cargo->Count());
	if(buy) quantity = qMin(quantity, SaleStock(*cargo, *catalogue, m_City, world->CanHaveFauna(), world->CanHaveFlora()));
	const double availableMoney = buy ? ship.AccountBalance() : accounting.accounts.citizens;
	if(unitPrice) quantity = qMin(quantity, quint32(qMin(double(std::numeric_limits<quint32>::max()), std::max(0.0, availableMoney / unitPrice))));
	cargo->SetCount(quantity);
	if(buy)
	{
		if(!ship.CanAddCargoTrim(*cargo)) return 0;
		quantity = cargo->Count();
	}
	else
	{
		const StorageCapacities capacity = CityStorage(*catalogue, m_City.developments, quint8(world->OrbitZone()));
		const AuSpecies species(m_City.citizenSpecies);
		quantity = TestFitCityCommodity(*catalogue, inventory, *cargo, capacity, world->HasBreathableAtmosphere(), quint8(species.EcoRole()));
		cargo->SetCount(quantity);
	}
	if(!quantity || (buy && quint64(quantity) * unitPrice > maximumPrice)) return 0;
	double price = double(quantity) * unitPrice;
	if(!buy && !mission && exact)
	{
		// An accepted auction pays its agreed bid in full. Requoting cannot
		// silently reduce the seller's payment or delivered quantity.
		if(quantity != exact->Count() || price < maximumPrice || availableMoney < maximumPrice) return 0;
		price = maximumPrice;
	}
	if(buy)
	{
		if(!inventory.TakeItem(*cargo, quantity)) return 0;
		accounting.accounts.Credit(CitizenIncome::Sales, price, accounting.taxes);
	}
	else
	{
		const StorageCapacities capacity = CityStorage(*catalogue, m_City.developments, quint8(world->OrbitZone()));
		const AuSpecies species(m_City.citizenSpecies);
		if(!AddCityCommodity(*catalogue, inventory, *cargo, capacity, world->HasBreathableAtmosphere(), quint8(species.EcoRole()))) return 0;
		accounting.accounts.Debit(CitizenExpense::Purchases, price);
	}
	QByteArray inventoryBytes, accountingBytes;
	if(!WriteCityInventory(inventory, inventoryBytes, error) || !EncodeAccounting(accounting, accountingBytes, error)) return 0;
	if(!buy && !ship.RemoveCargo(*cargo, quantity)) return 0;
	m_City.inventory = inventoryBytes; m_City.accounting = accountingBytes; ++m_CityRevision;
	lock.unlock();
	if(buy) { ship.DebitAccount(price); ship.AddCargo(*cargo); }
	else ship.CreditAccountSales(price);
	ship.SetModified(); SetModified();
	return int(quantity);
}

void DaLegacyCity::AcceptSpacecraftRepair(DaPerson& person, const AuCommAcceptRepair& message)
{
	DaVessel* ship = person.FindParentVesselInEnv();
	const DaSector* sector = FindParentSector();
	if(!ServiceCatalogue() || !sector || !ship || !person.IsInsideSpacecraft()
		|| !ship->Is(message.RepairShipId()) || !ship->InChainOfCommand(&person)) return;
	const auto reply = [&](const QString& text) {
		AuCommCityChat response(text, message.ChannelName(), Id(), Name(), sector->Id(), Galaxy(),
			message.ReceptionRange(), message.Scope()); g_Au.SendAtomChat(response);
	};
	if(!g_Au.HasPendingRepairOffer(&message)) { reply(tr("The repair offer has expired.")); return; }
	if(ship->RepairSecondsRemaining()) { reply(tr("The spacecraft is already receiving repairs.")); return; }
	if(ship->IsMoving() || ship->SensorsWorking() || ship->ShieldsWorkingAndOn() || ship->IsAnyFTLDriveWorking()
		|| ship->GravityDriveWorking() || ship->PowerPlantWorking())
	{ reply(tr("Stop the spacecraft and turn off its sensors, shields, FTL, maneuver drive and power plant for repairs.")); return; }
	const AuSysId target(message.SysId());
	quint16 seconds = 0;
	const int result = RepairSpacecraft(*ship,
		message.Type() == AuCommPacket::AcceptRepairSystem ? &target : nullptr,
		message.MinQ(), message.Price(), true, &seconds);
	if(result < 0) { reply(tr("The spacecraft cannot afford those repairs. Request a new offer.")); return; }
	if(!result) { reply(tr("No repairs can be performed with the available materials and repair facilities.")); return; }
	g_Au.BidRevoked(Id(), message.ChannelName());
	reply(tr("Spacecraft repairs will be completed in %1 seconds.").arg(seconds));
	person.Stats().SpacecraftRepairsBought(ship);
	person.IncreaseSkill(AuSkill::Command, 1);
}

void DaLegacyCity::AcceptSpacecraftCargo(DaPerson& person, const AuCommAccept& message)
{
	DaVessel* ship = person.FindParentVesselInEnv();
	if(!message.Item() || !message.Item()->IsValid() || !ship || !person.IsInsideSpacecraft()
		|| !ship->InChainOfCommand(&person) || !g_Au.HasPendingBid(&message)
		|| message.Item()->Count() > quint32(std::numeric_limits<int>::max())) return;
	const int delivered = TransferSpacecraftCargo(*ship, message.Item()->CommodityId(), int(message.Item()->Count()),
		false, false, message.Item(), message.Price());
	if(delivered) g_Au.BidRevoked(Id(), message.ChannelName());
	const DaSector* sector = FindParentSector();
	if(!sector) return;
	AuCommCityChat response(delivered ? tr("Cargo sold to the city.") : tr("The cargo sale could not be completed. Request a new bid."),
		message.ChannelName(), Id(), Name(), sector->Id(), Galaxy(), message.ReceptionRange(), message.Scope());
	g_Au.SendAtomChat(response);
}

bool DaLegacyCity::RxSpacecraftServiceMessage(const AuCommPacket* message)
{
	using namespace AuLegacyCity;
	if(!message) return false;
	const bool repair = message->Type() == AuCommPacket::RequestRepairAll || message->Type() == AuCommPacket::RequestRepairSystem;
	const bool upgrade = message->Type() == AuCommPacket::RequestUpgradeSystem || message->Type() == AuCommPacket::AcceptUpgradeInstall;
	const bool auction = message->Type() == AuCommPacket::Auction;
	const bool exterminate = message->Type() == AuCommPacket::RequestExterminator;
	COMMODITYFILTER filter = FILTER_All;
	const bool sales = SalesFilter(message->Type(), filter);
	const AuCommBid* bid = dynamic_cast<const AuCommBid*>(message);
	if(!repair && !upgrade && !sales && !bid && !auction && !exterminate) return false;
	DaAtom* system = FindEnvironmentAtom();
	DaPerson* person = system ? dynamic_cast<DaPerson*>(system->FindIdRecursive(message->FromId())) : nullptr;
	DaVessel* ship = person ? person->FindParentVesselInEnv() : nullptr;
	if(!person || !ship || !person->IsInsideSpacecraft()) return false; // Ground shoppers use the existing shop path.
	const AuLegacyCityCatalogue* catalogue = ServiceCatalogue();
	const DaSector* sector = FindParentSector();
	if(!catalogue || !g_Au.IsSceneWorker() || !sector || person->IsDead()) return true;
	const auto reply = [&](const QString& text) {
		AuCommCityChat response(text, message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(),
			message->ReceptionRange(), message->Scope()); g_Au.SendAtomChat(response);
	};
	if(!ship->InChainOfCommand(person)) { reply(tr("Only an officer in the spacecraft's chain of command may arrange ship services.")); return true; }
	if(!CanAccessSpacecraft(*ship))
	{
		if(!AuCommStdChannel::IsTradeChannel(message->ChannelName()))
			reply(tr("Move onto a city road slab, within transporter range, or near a friendly station to receive city services."));
		return true;
	}
	if(exterminate)
	{
		if(!HasSpacecraftService(110)) return true;
		AuLegacyFinanceTransfer finance(ship);
		const int price = ship->IsPrivatelyOwned() ? 100 : 0;
		if(price > ship->AccountBalance()) { reply(tr("The spacecraft does not have 100 to pay the exterminator.")); return true; }
		QWriteLocker lock(&m_CityLock); AccountingState accounting; QString error; QByteArray encoded;
		if(!DecodeAccounting(m_City.accounting, accounting, error)) return true;
		accounting.accounts.Credit(CitizenIncome::Sales, price, accounting.taxes);
		if(!EncodeAccounting(accounting, encoded, error)) return true;
		m_City.accounting = encoded; ++m_CityRevision; lock.unlock();
		ship->DebitAccount(price); SetModified();
		const int result = ship->ExterminateRats();
		QString text;
		if(result & 1) text += tr("Rat nest was removed. ");
		if(result & 2) text += tr("Vermin were exterminated. ");
		if(result & 4) text += tr("Food was cleaned up. ");
		text += tr("Extermination complete.");
		if(price) text += tr(" 100 was deducted from the spacecraft's fund.");
		reply(text); return true;
	}
	if(auction)
	{
		if(!message->Item() || !message->Item()->IsValid() || !message->Item()->Count() || !message->Item()->IsBrokerItem()) return true;
		if(message->Item()->StoryInstance()) { reply(tr("The city does not buy story items.")); return true; }
		const State state = Snapshot(); AuItemHash inventory; ProductionState production; AccountingState accounting; QString error;
		if(!ReadCityInventory(state.inventory, *catalogue, inventory, error)
			|| !DecodeProductionState(*catalogue, state.manufacturing, production, error)
			|| !DecodeAccounting(state.accounting, accounting, error)) return true;
		const DaWorldLL* world = dynamic_cast<const DaWorldLL*>(Parent());
		if(!world) return true;
		const POLITICAL_STANCE stance = PoliticalStance(state.controllingEmpire, ship->EmpireId());
		const DgData& gd = g_Au.LockDataForRead();
		const bool restricted = gd.EmpireCommerceRestricted(state.controllingEmpire, message->Item()->CommodityId(),
			stance); g_Au.UnlockData();
		if(restricted || (!ship->IsPrivatelyOwned() && message->Item()->IsFreeToFriendlyGovShip())) return true;
		quint32 price;
		{ QWriteLocker lock(&m_CityLock); price = CachedShopPrice(*catalogue, state, inventory, production, *message->Item(), 255); }
		std::unique_ptr<AuItem> offered(message->Item()->Clone());
		const AuSpecies species(state.citizenSpecies);
		quint32 quantity = TestFitCityCommodity(*catalogue, inventory, *offered,
			CityStorage(*catalogue, state.developments, quint8(world->OrbitZone())), world->HasBreathableAtmosphere(), quint8(species.EcoRole()));
		quantity = qMin(quantity, quint32(qMin(double(std::numeric_limits<quint32>::max()), qMax(0.0, accounting.accounts.citizens / price))));
		if(!quantity) return true;
		offered->SetCount(quantity);
		AuCommBid response(*offered, message->EmpireOfOrigin(), quint32(qMin(quint64(0xffffffffu), quint64(price) * quantity)),
			message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), message->ReceptionRange(), message->Scope());
		g_Au.SendAtomChat(response); return true;
	}
	if(repair)
	{
		const AuSysId target(message->SysId());
		const AuSysId* selected = message->Type() == AuCommPacket::RequestRepairSystem ? &target : nullptr;
		quint16 duration = 0;
		const quint32 price = quint32(RepairSpacecraft(*ship, selected, message->MinQ(), 0xffffffffu, false, &duration));
		if(!duration) { reply(tr("No repairs are available. Check damage, repair shop technology and city parts.")); return true; }
		AuItemHash inventory; QString error; const State state = Snapshot();
		if(!ReadCityInventory(state.inventory, *catalogue, inventory, error)) return true;
		quint32 hits = 0; AuRepairParts required;
		if(selected) ship->ListRepairsNeeded(*selected, hits, required); else ship->ListRepairsNeeded(hits, required);
		QList<COMMODITYID> missing, empty;
		for(auto part = required.cbegin(); part != required.cend(); ++part)
			if(EligibleCount(inventory, part.key(), ship->LegacyMinimumRepairTechnology(), message->MinQ()) < part.value()) missing.append(part.key());
		if(selected)
		{
			AuCommOfferRepairSystem response(message->System(), message->SystemWhich(), ship->SystemName(target), empty, empty,
				missing, empty, ship->Id(), ship->NameWithEmpire(), price, duration, message->MinQ(), 1,
				message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), message->ReceptionRange(), message->Scope());
			g_Au.SendAtomChat(response);
		}
		else
		{
			AuCommOfferRepairAll response(empty, empty, missing, empty, ship->Id(), ship->NameWithEmpire(), price, duration,
				message->MinQ(), 1, message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), message->ReceptionRange(), message->Scope());
			g_Au.SendAtomChat(response);
		}
		return true;
	}
	if(upgrade)
	{
		if(!HasSpacecraftService(110)) { reply(tr("This city needs a completed spacecraft repair shop.")); return true; }
		const quint32 empire = EmpireId();
		const POLITICAL_STANCE stance = PoliticalStance(empire, ship->EmpireId());
		const bool subsidized = FreeService(*ship, empire);
		const DgData& gd = g_Au.LockDataForRead();
		const bool restricted = gd.EmpireCommerceRestricted(empire,
			COMMODITYID(catalogue->RuntimeCommodityId(CommodityId(87))), stance);
		g_Au.UnlockData();
		if(restricted) { reply(tr("The city empire prohibits spacecraft upgrades.")); return true; }
		const AuSysId target(message->SysId());
		const quint32 quantity = ship->SystemModuleCount(target);
		if(!quantity) { reply(tr("That system has no replaceable upgrade modules.")); return true; }
		const bool installationReachable = GroundService(*this, *ship) || StationService(*this, *ship);
		AuLegacyFinanceTransfer finance(ship);
		QWriteLocker lock(&m_CityLock);
		AuItemHash inventory; ProductionState production; AccountingState accounting; QString error;
		if(!ReadCityInventory(m_City.inventory, *catalogue, inventory, error)
			|| !DecodeProductionState(*catalogue, m_City.manufacturing, production, error)
			|| !DecodeAccounting(m_City.accounting, accounting, error)) return true;
		if(message->Type() == AuCommPacket::RequestUpgradeSystem)
		{
			struct Offer { AuItemHash item; quint32 price; quint16 seconds; };
			QMap<QString, Offer> offers;
			for(const auto& item : inventory)
			{
				const int work = ship->SystemModuleInstallTime(target, item.second->CommodityId());
				if(work <= 0 || item.second->Count() < quantity || !HasService(m_City, 110, item.second->TechLevel())) continue;
				Offer offer; offer.item.AddItemCount(*item.second, quantity);
				offer.price = subsidized ? 0 : quint32(qMin(quint64(0xffffffffu),
					quint64(CachedShopPrice(*catalogue, m_City, inventory, production, *item.second, 110)) * quantity * 2));
				offer.seconds = quint16(qMin(quint64(65535), qMax(quint64(10), (quint64(work) * quantity + 3) / 4)));
				offers.insert(item.second->ListDesc(), offer);
			}
			lock.unlock();
			if(offers.isEmpty()) { reply(tr("No suitable upgrade modules are available at this repair shop's technology.")); return true; }
			for(auto offer = offers.cbegin(); offer != offers.cend(); ++offer)
			{
				AuCommOfferUpgradeInstall response(*offer->item.begin()->second, QString(), message->SystemWhich(), offer->price,
					offer->seconds, 1, message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), message->ReceptionRange(), message->Scope());
				g_Au.SendAtomChat(response);
			}
			return true;
		}
		const AuCommAcceptUpgradeInstall* accepted = dynamic_cast<const AuCommAcceptUpgradeInstall*>(message);
		if(!accepted || !accepted->Item()) return true;
		auto item = inventory.Find(*accepted->Item());
		if(item == inventory.end() || item->second->Count() < quantity || !HasService(m_City, 110, item->second->TechLevel())) return true;
		const int work = ship->SystemModuleInstallTime(target, item->second->CommodityId());
		if(work <= 0) return true;
		const quint32 price = subsidized ? 0 : quint32(qMin(quint64(0xffffffffu),
			quint64(CachedShopPrice(*catalogue, m_City, inventory, production, *item->second, 110)) * quantity * 2));
		const quint16 duration = quint16(qMin(quint64(65535), qMax(quint64(10), (quint64(work) * quantity + 3) / 4)));
		if(price > accepted->Price() || price > ship->AccountBalance()) { lock.unlock(); reply(tr("The spacecraft cannot afford the current upgrade price.")); return true; }
		if(!installationReachable) { lock.unlock(); reply(tr("Move onto a city road slab or near a friendly station for installation.")); return true; }
		if(ship->RepairSecondsRemaining() || ship->IsMoving() || ship->SensorsWorking() || ship->ShieldsWorkingAndOn()
			|| ship->IsAnyFTLDriveWorking() || ship->GravityDriveWorking() || ship->PowerPlantWorking())
		{ lock.unlock(); reply(tr("Rig the spacecraft for repairs before installing an upgrade.")); return true; }
		std::unique_ptr<AuItem> installed(item->second->Clone());
		if(!inventory.TakeItem(*installed, quantity)) return true;
		accounting.accounts.Credit(CitizenIncome::Sales, price, accounting.taxes);
		QByteArray inventoryBytes, accountingBytes;
		if(!WriteCityInventory(inventory, inventoryBytes, error) || !EncodeAccounting(accounting, accountingBytes, error)) return true;
		// InstallSystemModule updates the shared ship systems and the legacy
		// runtime report, including the independently persisted installed TL.
		if(!ship->InstallSystemModule(target, *installed, duration)) { lock.unlock(); reply(tr("The system could not accept that module.")); return true; }
		m_City.inventory = inventoryBytes; m_City.accounting = accountingBytes; ++m_CityRevision;
		lock.unlock(); ship->DebitAccount(price); ship->SetModified(); SetModified();
		g_Au.BidRevoked(Id(), message->ChannelName());
		reply(tr("Spacecraft upgrade installation will finish in %1 seconds.").arg(duration));
		return true;
	}
	if(bid)
	{
		if(!bid->Item() || !bid->Item()->IsValid() || !g_Au.HasPendingBid(bid)) return true;
		const int delivered = TransferSpacecraftCargo(*ship, bid->Item()->CommodityId(), int(qMin(bid->Item()->Count(), quint32(std::numeric_limits<int>::max()))),
			true, false, bid->Item(), bid->Price());
		if(!delivered) { reply(tr("The cargo purchase could not be completed. Request the city's stock again.")); return true; }
		std::unique_ptr<AuItem> bought(bid->Item()->Clone()); bought->SetCount(quint32(delivered));
		g_Au.BidRevoked(bid->FromId(), bid->ChannelName());
		AuCommAccept response(person->Id(), person->Name(), *bought, bid->EmpireOfOrigin(), bid->Price(),
			message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), message->ReceptionRange(), message->Scope());
		g_Au.SendAtomChat(response);
		return true;
	}
	const State state = Snapshot(); AuItemHash inventory; ProductionState production; QString error;
	if(!ReadCityInventory(state.inventory, *catalogue, inventory, error)
		|| !DecodeProductionState(*catalogue, state.manufacturing, production, error)) return true;
	DbCommodityArray descriptors;
	for(const quint16 id : catalogue->RuntimeCommodityIds())
		if(const DbCommodity* descriptor = catalogue->RuntimeCommodity(id)) descriptors.append(*descriptor);
	QList<COMMODITYID> filtered;
	if(message->Type() == AuCommPacket::SolicitItemSales) filtered.append(message->CommodityId());
	else descriptors.ListFilterCommodities(filtered, filter);
	const DaWorldLL* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world) return true;
	QList<AuCommForSale> offers;
	for(const auto& item : inventory)
	{
		if(!item.second->IsBrokerItem() || !filtered.contains(item.first)) continue;
		const quint32 available = SaleStock(*item.second, *catalogue, state, world->CanHaveFauna(), world->CanHaveFlora());
		if(!available) continue;
		const POLITICAL_STANCE stance = PoliticalStance(state.controllingEmpire, ship->EmpireId());
		const DgData& gd = g_Au.LockDataForRead();
		const bool restricted = gd.EmpireCommerceRestricted(state.controllingEmpire, item.first,
			stance); g_Au.UnlockData();
		if(restricted) continue;
		std::unique_ptr<AuItem> offered(item.second->Clone()); offered->SetCount(available);
		quint32 price;
		{ QWriteLocker lock(&m_CityLock); price = CachedShopPrice(*catalogue, state, inventory, production, *offered, 111); }
		if(offered->IsFreeToFriendlyGovShip() && FreeService(*ship, state.controllingEmpire)) price = 0;
		offers.append(AuCommForSale(*offered, QString(), price, message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(),
			message->ReceptionRange(), message->Scope()));
	}
	for(const AuCommForSale& offer : offers) g_Au.SendAtomChat(offer);
	if(offers.isEmpty() && !AuCommStdChannel::IsTradeChannel(message->ChannelName())) reply(tr("No matching cargo is available for sale."));
	return true;
}

DaVessel* DaLegacyCity::FindShipInHangar(const DaPerson& person, quint32* count) const
{
	if(count) *count = 0;
	const DaWorldLL* world = dynamic_cast<const DaWorldLL*>(Parent());
	const AuLegacyCityCatalogue* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!g_Au.IsLegacyGeneration() || !world || !catalogue || person.FindParentPlanetInEnv() != world) return nullptr;
	const AuQueryEnvironment* environment = person.LockEnvironmentQuery();
	const quint32 development = environment->LegacyCityId() == Id() ? environment->LegacyDevelopmentId() : 0;
	person.UnlockEnvironmentQuery();
	const AuLegacyCity::State state = Snapshot();
	bool validHangar = false;
	for(const AuLegacyCity::Development& site : state.developments)
		if(site.id == development && site.industryId == 126 && site.completedLevels && !site.demolished) validHangar = true;
	if(!validHangar) return nullptr;
	DaVessel* owned = nullptr;
	DaAtomList children; world->ListSubatoms(children);
	for(DaAtom* atom : children)
	{
		DaVessel* ship = dynamic_cast<DaVessel*>(atom);
		if(!ship || !ship->IsDormant() || !ship->IsMoored()) continue;
		AuPoint location;
		if(!ship->GetLocationRelativeToParent(location, world)
			|| SiteAt(*world, state, location, 126, *catalogue) != development) continue;
		if(count) ++*count;
		if(ship->CaptainId() == person.Id()) owned = ship;
	}
	return owned;
}

bool DaLegacyCity::IsHangarShipValid(const DaVessel& ship) const
{
	const DaWorldLL* world = dynamic_cast<const DaWorldLL*>(Parent());
	const AuLegacyCityCatalogue* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!g_Au.IsLegacyGeneration() || !world || !catalogue || ship.Parent() != world
		|| !ship.IsDormant() || !ship.IsMoored()) return false;
	AuPoint location;
	return ship.GetLocationRelativeToParent(location, world) && SiteAt(*world, Snapshot(), location, 126, *catalogue);
}

bool DaLegacyCity::HangarSpacecraft(DaPerson& person, const AuId& shipId, bool launch, bool takeCaptain, QString& message)
{
	DaWorld* world = person.FindParentPlanetInEnv();
	const AuQueryEnvironment* environment = person.LockEnvironmentQuery();
	const bool inCity = environment->LegacyCityId() == Id();
	const quint32 selectedDevelopment = environment->LegacyDevelopmentId();
	person.UnlockEnvironmentQuery();
	bool completedHangar = false;
	const AuLegacyCity::State state = Snapshot();
	for(const AuLegacyCity::Development& site : state.developments)
		if(site.id == selectedDevelopment && site.industryId == 126 && site.completedLevels && !site.demolished) completedHangar = true;
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !world || world != Parent()
		|| !inCity || person.InIndustry() != IID_Hangar || !completedHangar)
	{ message = tr("Stand inside a completed hangar to store or launch spacecraft."); return false; }
	if(PoliticalStance(EmpireId(), person.EmpireId()) == POL_Enemy)
	{ message = tr("This enemy city will not permit spacecraft hangar operations."); return false; }
	DaVessel* stored = FindShipInHangar(person);
	if(launch)
	{
		if(!stored) { message = tr("No spacecraft belonging to me was found in this hangar."); return false; }
		AuPoint location; person.GetLocationRelativeToParent(location, world);
		location = world->FindOrbitSpawnLoc(location, AuId(), stored->RoughRadius(), world->AtmosphereRadius() + ParkAltAboveAtm(), true);
		AuMatrix4x4 placement; placement.Translate(location);
		placement *= AuPoint(0, 0, 1).GetAlignmentTransform(-world->GravityDir(location));
		stored->RemoveBerthOccupant(stored->CaptainId());
		if(takeCaptain)
		{
			stored->OccupyBerth(AuRosterEntry::Captain, person.Id(), person.Name());
			person.SetSpawnShip(stored->Id(), stored->Name(), AuRosterEntry::Captain, stored->IsPrivatelyOwned());
			person.SetModified();
		}
		stored->ChangePosAndOrient(AuAtomPos(world->Id(), placement));
		stored->Reactivate(); stored->InvalidateEnvironment(); stored->SetModified();
		message = tr("%1 launched successfully into orbit.").arg(stored->NameWithEmpire());
		if(takeCaptain) message += tr(" Captain's berth assigned. Recall home to go aboard.");
		return true;
	}
	if(stored) { message = tr("This hangar already holds a mothballed spacecraft of mine."); return false; }
	DaVessel* ship = dynamic_cast<DaVessel*>(world->FindIdRecursive(shipId));
	if(!ship || ship->IsDormant() || ship->IsDead() || ship->InDesignEnvironment())
	{ message = tr("The spacecraft to be stored could not be found."); return false; }
	if(ship->IsSpaceStation() || !ship->IsOnGround() || !ship->IsReadyForRepairs())
	{ message = tr("The spacecraft must be on the ground and rigged for repairs."); return false; }
	if(const DaCreature* creature = ship->FindCreatureBlockingMothball())
	{ message = tr("%1 cannot be mothballed while %2 is aboard.").arg(ship->NameWithEmpire()).arg(creature->Name()); return false; }
	const POLITICAL_STANCE stance = PoliticalStance(ship->EmpireId(), person.EmpireId());
	const bool newShip = ship->IsNew() && (stance == POL_Empire || stance == POL_Friend || stance == POL_Vassal);
	if(!newShip && (ship->IsNew() || (ship->HasOfficers() && !ship->InChainOfCommand(&person))))
	{ message = tr("I do not have authority to mothball that spacecraft."); return false; }
	ship->Mothball(&person, world);
	message = tr("%1 mothballed successfully in this hangar.").arg(ship->NameWithEmpire());
	return true;
}

void DaLegacyCity::SupplySpaceStation(DaVessel& ship)
{
	using namespace AuLegacyCity;
	const AuLegacyCityCatalogue* catalogue = ServiceCatalogue();
	const DaWorldLL* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!catalogue || !world || !g_Au.IsSceneWorker() || !ship.IsSpaceStation() || ship.IsPrivatelyOwned()
		|| ship.EmpireId() != EmpireId() || !HasSpacecraftService(111) || !CanAccessSpacecraft(ship)) return;
	const int food = ship.FoodWantedByStation(), supplies = ship.SuppliesWantedByStation(), munitions = ship.MunitionsWantedByStation();
	const quint32 fuel = ship.FuelWanted();
	AuItemHash aboard; aboard = ship.LockCargoForRead(); ship.UnlockCargo();
	QList<COMMODITYID> weapons;
	const AuShipSystemArray& systems = ship.LockSystems();
	for(const AuShipSystem& system : systems)
		if(system.GetSystemType() == SYS_Weapon || system.GetSystemType() == SYS_Turret) weapons.append(system.Module());
	ship.UnlockSystems();
	QSet<COMMODITYID> ammoNeeded;
	for(const quint16 commodity : catalogue->RuntimeCommodityIds())
	{
		const COMMODITYID ammo = COMMODITYID(commodity);
		const DbCommodity* descriptor = catalogue->RuntimeCommodity(commodity);
		if(!descriptor || (!descriptor->IsAmmo() && !DbCommodity::IsEnergyWeapon(ammo))) continue;
		for(const COMMODITYID weapon : weapons)
			if(DbCommodity::IsAmmunitionForWeapon(weapon, ammo)) ammoNeeded.insert(ammo);
	}
	AuItemHash exported;
	QWriteLocker lock(&m_CityLock);
	AuItemHash inventory; QString error;
	if(!HasService(m_City, 111) || m_City.controllingEmpire != ship.EmpireId()
		|| !ReadCityInventory(m_City.inventory, *catalogue, inventory, error)) return;
	// Shared original city-to-station transfer: ten percent per commodity,
	// station category quotas and the city's environmental life-support reserve.
	inventory.ExportToStation(m_City.population, exported, 10, quint32(qMax(0, food)),
		quint32(qMax(0, supplies)), quint32(qMax(0, munitions)), world->CanHaveFlora(), world->CanHaveFauna());
	auto hydrogen = inventory.FindBest(CID_Hydrogen);
	if(fuel && hydrogen != inventory.end())
	{
		const quint32 amount = qMin(fuel, hydrogen->second->Count());
		std::unique_ptr<AuItem> reserved(hydrogen->second->Clone()); reserved->SetCount(amount);
		exported.AddItem(*reserved);
		inventory.TakeItem(*reserved, amount);
	}
	const quint32 rounds = ammoNeeded.isEmpty() ? 0 : (ship.CargoCapacityVolume() / 2) / quint32(ammoNeeded.size());
	for(const COMMODITYID ammo : ammoNeeded)
	{
		const quint32 desired = DbCommodity::IsEnergyWeapon(ammo) ? quint32(ship.Turrets()) : rounds;
		const quint64 current = quint64(aboard.RawCount(ammo)) + exported.RawCount(ammo);
		if(current >= desired) continue;
		auto item = inventory.FindBest(ammo);
		if(item == inventory.end()) continue;
		const quint32 amount = qMin(quint32(desired - current), item->second->Count());
		std::unique_ptr<AuItem> reserved(item->second->Clone()); reserved->SetCount(amount);
		exported.AddItem(*reserved);
		inventory.TakeItem(*reserved, amount);
	}
	QByteArray bytes;
	if(!WriteCityInventory(inventory, bytes, error)) return;
	m_City.inventory = bytes; ++m_CityRevision;
	lock.unlock();
	for(const auto& item : exported) ship.AddCargo(*item.second);
	SetModified(); ship.SetModified();
	BoardSpacecraftPersonnel(ship, ROLE_Crew, ship.CrewBerthsAvailable());
}
