#include "dalegacycity.h"
#include "aucommpacket.h"
#include "audesign.h"
#include "augear.h"
#include "auglobal.h"
#include "auitem.h"
#include "aulegacycityinventory.h"
#include "aulegacycitypopulation.h"
#include "aulegacycityproduction.h"
#include "aulegacycityshop.h"
#include "aulegacyfinancetransfer.h"
#include "aulegacyscstats.h"
#include "auplayerstats.h"
#include "auqueryenvironment.h"
#include "auscdesignexchange.h"
#include "auscreport.h"
#include "auspecies.h"
#include "daperson.h"
#include "dasector.h"
#include "daworldll.h"
#include "dgdata.h"
#include <QMap>
#include <QWriteLocker>

namespace
{
const AuLegacyCityCatalogue* BlueprintCatalogue()
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !AuLegacyItemTechnologyActive() || !catalogue) return nullptr;
	for(const quint16 commodity : catalogue->RuntimeCommodityIds())
		if(!g_Au.HasLegacyCommodity(COMMODITYID(commodity))) return nullptr;
	return catalogue;
}

quint8 StudioIndustry(const DaLegacyCity& city, const DaPerson& person)
{
	if(person.IsInsideSpacecraft() || person.IsDead() || person.FindParentPlanetInEnv() != city.Parent()) return 0;
	const auto* environment = person.LockEnvironmentQuery();
	const bool inCity = environment->LegacyCityId() == city.Id();
	const quint32 selected = environment->LegacyDevelopmentId();
	person.UnlockEnvironmentQuery();
	if(!inCity) return 0;
	for(const auto& development : city.Snapshot().developments)
		if(development.id == selected && !development.demolished && development.completedLevels
			&& (development.industryId == 3 || development.industryId == 112)) return development.industryId;
	return 0;
}

bool CanonicalBlueprint(quint32 id, AuScReport& report)
{
	const auto& designs = g_Au.LockScDesignsForRead();
	const bool ready = designs.ScDesignState(id) == AuDesign::Final && designs.ScDesignReport(id, report)
		&& report.LegacyStats() && report.LegacyStats()->IsValid();
	g_Au.UnlockScDesigns();
	if(!ready) g_Au.ScDesignNeeded(id);
	return ready;
}

bool BlueprintTradeAllowed(quint32 empire, quint32 customer)
{
	const auto& data = g_Au.LockDataForRead();
	const auto stance = data.EmpirePoliticalStance(empire, customer);
	const bool allowed = stance != POL_Enemy && !data.EmpireCommerceRestricted(empire, CID_ScDesignDisk, stance);
	g_Au.UnlockData();
	return allowed;
}

AuItemScDesign BlueprintCopy(const AuScReport& report, const AuItem& blank)
{
	const AuScDesignExchangeEntry exchange(report, AuDesign::Final, 0, true, true, false, 0, POL_Neutral);
	AuItemScDesign disk(exchange, CID_ScDesignDisk, blank.Quality(), blank.EmpireOfOrigin());
	disk.SetTechLevel(blank.TechLevel());
	return disk;
}

struct PersonalGearLock
{
	DaPerson& person;
	AuGear* gear;
	explicit PersonalGearLock(DaPerson& who) : person(who), gear(who.LockGearForWrite()) {}
	~PersonalGearLock() { person.UnlockGear(); }
};
}

bool DaLegacyCity::RxSpacecraftBlueprintMessage(const AuCommPacket* message)
{
	using namespace AuLegacyCity;
	if(!message) return false;
	const auto* offered = dynamic_cast<const AuItemScDesign*>(message->Item());
	const auto* bid = dynamic_cast<const AuCommBid*>(message);
	const auto* barter = dynamic_cast<const AuCommBidBarter*>(message);
	const auto* auction = dynamic_cast<const AuCommAuction*>(message);
	const auto* sale = dynamic_cast<const AuCommForSale*>(message);
	const bool stockRequest = message->Type() == AuCommPacket::SolicitAllSales
		|| message->Type() == AuCommPacket::SolicitMiscSales
		|| (message->Type() == AuCommPacket::SolicitItemSales && message->CommodityId() == CID_ScDesignDisk);
	const bool designOffer = offered && offered->CommodityId() == CID_ScDesignDisk && (bid || barter || auction || sale);
	if(!stockRequest && !designOffer) return false;
	const auto* catalogue = BlueprintCatalogue();
	auto* system = FindEnvironmentAtom();
	auto* person = system ? dynamic_cast<DaPerson*>(system->FindIdRecursive(message->FromId())) : nullptr;
	const auto* sector = FindParentSector();
	if(!catalogue || !person || !sector || IsDead()) return designOffer;
	const quint8 industry = StudioIndustry(*this, *person);
	if(!industry) return designOffer;
	const auto before = Snapshot();
	if(!before.controllingEmpire || !BlueprintTradeAllowed(before.controllingEmpire, person->EmpireId())) return designOffer;
	const auto range = AuCommStdChannel::RANGE(qMin(int(message->ReceptionRange()), int(AuCommStdChannel::SolarSystem)));
	const auto reply = [&](const QString& text) {
		AuCommCityChat response(text, message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), range, message->Scope());
		g_Au.SendAtomChat(response);
	};
	QList<quint32> published; QString error;
	if(!g_Au.ListLegacySpacecraftPublications(before.controllingEmpire, published, error))
	{ reply(error); return designOffer; }
	if(stockRequest)
	{
		QMap<QPair<QString, quint32>, AuScReport> designs;
		for(const quint32 id : published)
		{
			AuScReport report;
			if(CanonicalBlueprint(id, report)) designs.insert(qMakePair(report.DesignName(), id), report);
		}
		AuItemHash inventory; ProductionState production;
		QWriteLocker lock(&m_CityLock);
		if(m_City.controllingEmpire != before.controllingEmpire
			|| !ReadCityInventory(m_City.inventory, *catalogue, inventory, error)
			|| !DecodeProductionState(*catalogue, m_City.manufacturing, production, error)) return false;
		const auto blank = inventory.find(CID_BlankDisk);
		if(blank == inventory.end() || blank->first != CID_BlankDisk || !blank->second->Count())
		{ lock.unlock(); reply(tr("We have no blank disks in stock. We cannot sell published spacecraft blueprints without blank disks.")); return false; }
		const quint32 price = CachedShopPrice(*catalogue, m_City, inventory, production, *blank->second, industry);
		QList<AuItemScDesign> copies;
		for(const auto& report : designs) copies.append(BlueprintCopy(report, *blank->second));
		lock.unlock();
		if(copies.isEmpty()) reply(tr("No published spacecraft blueprints are available in our empire."));
		else
		{
			reply(tr("Published spacecraft blueprints for sale."));
			for(const auto& disk : copies)
			{
				AuCommForSale response(disk, disk.EmpireNameOfOrigin(), price, message->ChannelName(), Id(), Name(),
					sector->Id(), Galaxy(), range, message->Scope());
				g_Au.SendAtomChat(response);
			}
		}
		reply(tr("To publish a spacecraft blueprint, sell us a reusable blueprint disk that you designed."));
		return false; // The same request also lists ordinary studio stock.
	}
	if(!offered || offered->Count() != 1 || offered->StoryInstance() || !offered->DesignId()) return true;
	AuScReport report;
	if(!CanonicalBlueprint(offered->DesignId(), report))
	{ reply(tr("That finalized Legacy spacecraft blueprint is not loaded. Try again in a moment.")); return true; }
	if(bid || barter)
	{
		if((bid ? !g_Au.HasPendingBid(bid) : !g_Au.HasPendingBidBarter(barter))
			|| !published.contains(offered->DesignId())) return true;
		const AuItem* payment = barter ? barter->OfferItem() : nullptr;
		if(barter && (!payment || !payment->Count() || payment->StoryInstance() || !payment->IsValid())) return true;
		AuLegacyFinanceTransfer finance(person);
		const double balance = person->AccountBalance();
		PersonalGearLock gear(*person);
		if(!gear.gear) return true;
		QWriteLocker lock(&m_CityLock);
		AuItemHash inventory; ProductionState production; AccountingState accounting;
		if(m_City.controllingEmpire != before.controllingEmpire
			|| !ReadCityInventory(m_City.inventory, *catalogue, inventory, error)
			|| !DecodeProductionState(*catalogue, m_City.manufacturing, production, error)
			|| !DecodeAccounting(m_City.accounting, accounting, error)) return true;
		auto blank = inventory.find(CID_BlankDisk);
		for(; blank != inventory.end() && blank->first == CID_BlankDisk; ++blank)
			if(blank->second->Count() && blank->second->Quality() == offered->Quality()
				&& blank->second->TechLevel() == offered->TechLevel()
				&& blank->second->EmpireOfOrigin() == offered->EmpireOfOrigin()) break;
		if(blank == inventory.end() || blank->first != CID_BlankDisk)
		{ lock.unlock(); reply(tr("That blank disk is no longer in stock. Request a new blueprint offer.")); return true; }
		// The original list quotes the blank disk, while acceptance checks the
		// blueprint commodity's SellCost. Preserve both sides of that behavior.
		const quint32 price = CachedShopPrice(*catalogue, m_City, inventory, production, *offered, industry);
		if(!price || (bid ? bid->Price() < price || bid->Price() > balance : payment->TotalCost() < price))
		{ lock.unlock(); reply(tr("Your offer or personal account does not cover the blueprint price.")); return true; }
		const AuItemScDesign disk = BlueprintCopy(report, *blank->second);
		AuGear packed(*gear.gear);
		if(barter)
		{
			auto* world = dynamic_cast<DaWorldLL*>(Parent());
			if(!world || packed.CountAvailableItems(*payment) < payment->Count()) return true;
			const auto capacity = CityStorage(*catalogue, m_City.developments, quint8(world->OrbitZone()));
			const AuSpecies species(m_City.citizenSpecies);
			if(TestFitCityCommodity(*catalogue, inventory, *payment, capacity, world->HasBreathableAtmosphere(),
				quint8(species.EcoRole())) < payment->Count())
			{ lock.unlock(); reply(tr("Our city has no storage space for your offer.")); return true; }
			packed.RemoveItems(*payment);
			if(!AddCityCommodity(*catalogue, inventory, *payment, capacity, world->HasBreathableAtmosphere(),
				quint8(species.EcoRole()))) return true;
			// AddCityCommodity can replace inventory nodes, including blank disks.
			blank = inventory.find(CID_BlankDisk);
			for(; blank != inventory.end() && blank->first == CID_BlankDisk; ++blank)
				if(blank->second->Count() && blank->second->Quality() == disk.Quality()
					&& blank->second->TechLevel() == disk.TechLevel()
					&& blank->second->EmpireOfOrigin() == disk.EmpireOfOrigin()) break;
			if(blank == inventory.end() || blank->first != CID_BlankDisk) return true;
		}
		if(!PackPersonalItem(packed, disk))
		{ lock.unlock(); reply(tr("There is no room in your gear for the blueprint disk.")); return true; }
		blank->second->Consume(1);
		if(!blank->second->Count()) inventory.Erase(blank);
		if(bid) accounting.accounts.Credit(CitizenIncome::Sales, bid->Price(), accounting.taxes);
		QByteArray stock, ledger;
		if(!WriteCityInventory(inventory, stock, error) || !EncodeAccounting(accounting, ledger, error)) return true;
		m_City.inventory = stock; m_City.accounting = ledger; *gear.gear = packed; ++m_CityRevision;
		lock.unlock();
		person->UpdateGearWeight(); if(bid) person->DebitAccount(bid->Price());
		person->Stats().ItemPossessed(disk); person->Stats().ItemBoughtAsInventory(disk);
		g_Au.BidRevoked(message->FromId(), message->ChannelName()); SetModified();
		if(bid)
		{
			AuCommAccept response(person->Id(), person->Name(), disk, disk.EmpireNameOfOrigin(), bid->Price(),
				message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), range, message->Scope());
			g_Au.SendAtomChat(response);
		}
		else
		{
			AuCommAcceptBarter response(person->Id(), person->Name(), disk, disk.EmpireNameOfOrigin(), *payment,
				barter->OfferEmpireOfOrigin(), message->ChannelName(), Id(), Name(), sector->Id(), Galaxy(), range, message->Scope());
			g_Au.SendAtomChat(response);
		}
		return true;
	}
	if(offered->Exchange().UseOnce()) { reply(tr("We do not buy restricted blueprints for publication.")); return true; }
	if(report.ArchitectId() != person->Id()) { reply(tr("Only the architect may publish this spacecraft blueprint.")); return true; }
	if(published.contains(offered->DesignId())) { reply(tr("That blueprint is already in our library.")); return true; }
	const auto* personalGear = person->LockGearForRead();
	const bool carried = personalGear && personalGear->FindItemPos(*offered).IsValid();
	person->UnlockGear();
	if(!carried) return true;
	QWriteLocker lock(&m_CityLock);
	AuItemHash inventory; ProductionState production; AccountingState accounting;
	if(!ReadCityInventory(m_City.inventory, *catalogue, inventory, error)
		|| !DecodeProductionState(*catalogue, m_City.manufacturing, production, error)
		|| !DecodeAccounting(m_City.accounting, accounting, error)) return true;
	const quint32 maximum = CachedShopPrice(*catalogue, m_City, inventory, production, *offered, 255);
	const quint32 price = sale ? sale->Price() : maximum;
	if(price > maximum || price > accounting.accounts.citizens)
	{ lock.unlock(); reply(tr("We cannot afford that spacecraft blueprint at the asking price.")); return true; }
	lock.unlock();
	AuCommBid response(*offered, offered->EmpireNameOfOrigin(), price, message->ChannelName(), Id(), Name(),
		sector->Id(), Galaxy(), range, message->Scope()); g_Au.SendAtomChat(response);
	return true;
}

void DaLegacyCity::AcceptSpacecraftBlueprint(DaPerson& person, const AuCommAccept& message)
{
	using namespace AuLegacyCity;
	const auto* catalogue = BlueprintCatalogue();
	const auto* offered = dynamic_cast<const AuItemScDesign*>(message.Item());
	const auto* sector = FindParentSector();
	if(!catalogue || !offered || offered->CommodityId() != CID_ScDesignDisk || offered->Count() != 1
		|| offered->StoryInstance() || offered->Exchange().UseOnce() || !sector || !StudioIndustry(*this, person)
		|| !g_Au.HasPendingBid(&message)) return;
	AuScReport report;
	if(!CanonicalBlueprint(offered->DesignId(), report) || report.ArchitectId() != person.Id()) return;
	const auto before = Snapshot();
	if(!before.controllingEmpire || !BlueprintTradeAllowed(before.controllingEmpire, person.EmpireId())) return;
	AuLegacyFinanceTransfer finance(&person);
	PersonalGearLock gear(person);
	if(!gear.gear) return;
	AuGear consumed(*gear.gear);
	const auto position = consumed.FindItemPos(*offered);
	auto* disk = consumed.ItemAt(position);
	if(!position.IsValid() || !disk || !disk->Count()) return;
	if(disk->Count() > 1) disk->SetCount(disk->Count() - 1); else consumed.Erase(position);
	QWriteLocker lock(&m_CityLock);
	AccountingState accounting; QString error;
	if(m_City.controllingEmpire != before.controllingEmpire || !DecodeAccounting(m_City.accounting, accounting, error)
		|| message.Price() > accounting.accounts.citizens) return;
	accounting.accounts.Debit(CitizenExpense::Purchases, message.Price());
	QByteArray ledger;
	if(!EncodeAccounting(accounting, ledger, error)) return;
	// Unique publication is committed before the already-prepared local state.
	// This SQL-only hook takes no game-data locks; failure consumes nothing.
	if(!g_Au.SetLegacySpacecraftPublication(before.controllingEmpire, offered->DesignId(), true, error))
	{
		lock.unlock();
		AuCommCityChat response(error, message.ChannelName(), Id(), Name(), sector->Id(), Galaxy(),
			message.ReceptionRange(), message.Scope()); g_Au.SendAtomChat(response); return;
	}
	m_City.accounting = ledger; *gear.gear = consumed; ++m_CityRevision;
	lock.unlock();
	person.UpdateGearWeight(); person.CreditAccount(message.Price());
	g_Au.BidRevoked(Id(), message.ChannelName()); SetModified();
	AuCommCityMessage response(AuCommCityMessage::PublishCongrats, message.ChannelName(), Id(), Name(), sector->Id(),
		Galaxy(), message.ReceptionRange(), message.Scope()); g_Au.SendAtomChat(response);
}
