#include "dalegacycity.h"
#include "auglobal.h"
#include "aulegacycitypopulation.h"
#include "aulegacywealthlevy.h"
#include "dasolarsystem.h"
#include "daworldll.h"
#include "dgdata.h"
#include <QDateTime>
#include <QReadLocker>
#include <QWriteLocker>
#include <algorithm>
#include <cmath>

namespace
{
bool HasBank(const AuLegacyCity::State& state)
{
	for(const auto& development : state.developments)
		if(development.industryId == 100 && development.completedLevels && !development.demolished) return true;
	return false;
}
}

bool DaLegacyCity::ReceiveConfiscatedWealth(quint32 empire, double amount)
{
	using namespace AuLegacyCity;
	if(!std::isfinite(amount) || amount < 1) return false;
	AuId capital;
	const DgData& gd = g_Au.LockDataForRead();
	const bool hasCapital = gd.EmpireCapital(empire, capital) && !capital.IsNull();
	g_Au.UnlockData();
	QWriteLocker lock(&m_CityLock);
	AccountingState accounting; QString error;
	if(m_City.controllingEmpire != empire || !HasBank(m_City)
		|| !DecodeAccounting(m_City.accounting, accounting, error)) return false;
	accounting.taxes.hasCapital = hasCapital;
	if(!accounting.taxes.hasCapital) return false;
	accounting.accounts.Credit(GovernmentIncome::Confiscated, amount, accounting.taxes.hasCapital);
	QByteArray bytes;
	if(!EncodeAccounting(accounting, bytes, error)) return false;
	m_City.accounting = bytes; ++m_CityRevision;
	lock.unlock(); SetModified(); return true;
}

void DaLegacyCity::ApplyWealthLevy(const AuLegacyCity::WealthLevyOrder& order)
{
	using namespace AuLegacyCity;
	if(!g_Au.IsLegacyGeneration()) return;
	const bool inArena = InArena();
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	AuIdList places; places.append(Id()); ChainOfPlaces(places);
	for(;;)
	{
		State candidate; quint64 revision = 0;
		{ QReadLocker read(&m_CityLock); candidate = m_City; revision = m_CityRevision; }
		AccountingState accounting; QString error;
		if(!DecodeAccounting(candidate.accounting, accounting, error)) return;
		AuId capital;
		const DgData& data = g_Au.LockDataForRead();
		accounting.taxes.hasCapital = data.EmpireCapital(candidate.controllingEmpire, capital) && !capital.IsNull();
		g_Au.UnlockData();
		WealthLevyContext context;
		context.population = candidate.population; context.loyalPopulation = candidate.loyalPopulation;
		context.fraction = float(order.percent) / 100.0f;
		context.hasCapital = accounting.taxes.hasCapital; context.hasBank = HasBank(candidate);
		context.occupied = candidate.allegianceEmpire != candidate.controllingEmpire;
		context.underAttack = candidate.lastAttack && (now - candidate.lastAttack) / 1000 <= 771;
		context.inArena = inArena; context.empireMatches = candidate.controllingEmpire == order.empire;
		context.deferFinalWithdrawal = true;
		Accounts preview = accounting.accounts;
		const auto predicted = LevyCitizenWealth(preview, context);
		if(!predicted.applied) return;
		quint32 pirateEmpire = 0; AuIdList governors;
		if(predicted.revolution)
		{
			const DgData& gd = g_Au.LockDataForRead();
			if(candidate.allegianceEmpire == order.empire) pirateEmpire = gd.SelectLegacyPirateEmpire();
			gd.EmpireDomainGovernors(order.empire, places, governors);
			g_Au.UnlockData();
		}
		context.hasGovernors = !governors.isEmpty();
		const auto result = LevyCitizenWealth(accounting.accounts, context);
		quint32 surrenderedTo = 0; bool controlChanged = false;
		if(result.revolution)
		{
			// 0x2803b0 changes native allegiance before Surrender 0x2d0680.
			// An occupied city restores its former allegiance instead.
			if(candidate.allegianceEmpire == order.empire) candidate.allegianceEmpire = pirateEmpire;
			surrenderedTo = candidate.allegianceEmpire;
			controlChanged = ApplyCitySurrender(candidate, accounting.accounts, order.empire, surrenderedTo);
		}
		candidate.loyalPopulation = result.loyalPopulation;
		const double withdrawn = accounting.accounts.citizens * double(context.fraction);
		if(withdrawn >= 1) accounting.accounts.Debit(CitizenExpense::Withdrawn, withdrawn);
		if(!EncodeAccounting(accounting, candidate.accounting, error)) return;
		QWriteLocker lock(&m_CityLock);
		if(m_CityRevision != revision) continue;
		candidate.lastVisit = qMax(candidate.lastVisit, m_City.lastVisit);
		m_City = candidate; ++m_CityRevision;
		lock.unlock(); SetModified();
		if(result.revolution)
		{
			if(controlChanged)
			{
				emit CityCaptured(order.empire, surrenderedTo, Id());
				if(auto* world = dynamic_cast<DaWorldLL*>(Parent())) world->InvalidateEnvironmentRecursive();
				if(auto* system = FindParentSolarSystem())
					if(!system->IsExploredBy(surrenderedTo)) system->ExploredBy(surrenderedTo);
			}
			if(result.bounty >= 1)
				emit BountyPosted(Id(), Name(), order.issuer,
					tr("Death Sentence for Political Corruption and Graft in the %1 incident.").arg(order.statement), result.bounty, 0);
			const DgData& data = g_Au.LockDataForRead();
			const quint32 player = data.PlayerId(), confirmation = data.ConfirmationNumber();
			g_Au.UnlockData();
			g_Au.SendBulkMail(player, confirmation, Id(), Name(), governors, AuMailMsg::MSG_CityDistressReport,
				tr("Revolution!"), tr("Citizens revolted against the wealth tax ordered by %1. The city restored its allegiance and refused to surrender wealth.<br><br>%2")
					.arg(order.issuerName.toHtmlEscaped(), order.statement.toHtmlEscaped()),
				AuId(), QString(), AuId(), QString(), Galaxy(), AuPoint());
		}
		return;
	}
}

bool DaLegacyCity::Revolt(const QString& reason)
{
	using namespace AuLegacyCity;
	if(!g_Au.IsLegacyGeneration()) return false;
	for(;;)
	{
		State candidate; quint64 revision = 0;
		{ QReadLocker read(&m_CityLock); candidate = m_City; revision = m_CityRevision; }
		AccountingState accounting; QString error;
		if(!DecodeAccounting(candidate.accounting, accounting, error)) return false;
		const quint32 oldEmpire = candidate.controllingEmpire;
		if(candidate.allegianceEmpire == oldEmpire)
		{
			const DgData& gd = g_Au.LockDataForRead();
			candidate.allegianceEmpire = gd.SelectLegacyPirateEmpire();
			g_Au.UnlockData();
		}
		const quint32 newEmpire = candidate.allegianceEmpire;
		const bool captured = ApplyCitySurrender(candidate, accounting.accounts, oldEmpire, newEmpire);
		candidate.lastVisit = QDateTime::currentMSecsSinceEpoch();
		if(!EncodeAccounting(accounting, candidate.accounting, error)) return false;
		QWriteLocker lock(&m_CityLock);
		if(m_CityRevision != revision) continue;
		candidate.lastVisit = qMax(candidate.lastVisit, m_City.lastVisit);
		m_City = candidate; ++m_CityRevision;
		lock.unlock(); SetModified();
		NotifyRevolt(oldEmpire, newEmpire, captured, reason);
		return true;
	}
}

void DaLegacyCity::NotifyRevolt(quint32 oldEmpire, quint32 newEmpire, bool captured, const QString& reason)
{
	// Caller has already committed city ownership and the bank run. No city
	// state mutation belongs in this notification phase of CityCycle.
	if(captured)
	{
		emit CityCaptured(oldEmpire, newEmpire, Id());
		if(auto* world = dynamic_cast<DaWorldLL*>(Parent())) world->InvalidateEnvironmentRecursive();
		if(auto* system = FindParentSolarSystem())
			if(!system->IsExploredBy(newEmpire)) system->ExploredBy(newEmpire);
	}
	AuIdList places, governors; places.append(Id()); ChainOfPlaces(places);
	const DgData& data = g_Au.LockDataForRead();
	data.EmpireDomainGovernors(oldEmpire, places, governors);
	const quint32 player = data.PlayerId(), confirmation = data.ConfirmationNumber();
	g_Au.UnlockData();
	g_Au.SendBulkMail(player, confirmation, Id(), Name(), governors, AuMailMsg::MSG_CityDistressReport,
		tr("Revolution!"), tr("<b>REVOLUTION</b><br>%1<br><br>An interim ruling council was formed. The city declared its allegiance to another empire. Vive la revolution!").arg(reason),
		AuId(), QString(), AuId(), QString(), Galaxy(), AuPoint());
}
