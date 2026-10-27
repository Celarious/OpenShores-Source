#include "dalegacycity.h"
#include "auglobal.h"
#include "auid.h"
#include "aulegacycitypopulation.h"
#include "aulegacycityprotocol.h"
#include "aulegacyfinancetransfer.h"
#include "auqueryenvironment.h"
#include "auspecies.h"
#include "daperson.h"
#include "dgdata.h"
#include <QWriteLocker>
#include <algorithm>
#include <cmath>

void DaLegacyCity::ReceiveTaxLevy(double amount)
{
	if(!(amount > 0) || !std::isfinite(amount)) return;
	const quint32 empire = EmpireId();
	AuId capital;
	const DgData& gd = g_Au.LockDataForRead();
	const bool hasCapital = gd.EmpireCapital(empire, capital) && !capital.IsNull();
	g_Au.UnlockData();
	QWriteLocker lock(&m_CityLock);
	AuLegacyCity::AccountingState accounting;
	QString error;
	if(m_City.controllingEmpire != empire || !AuLegacyCity::DecodeAccounting(m_City.accounting, accounting, error)) return;
	accounting.taxes.hasCapital = hasCapital;
	if(!accounting.taxes.hasCapital) return;
	accounting.accounts.Credit(AuLegacyCity::GovernmentIncome::TaxLevy, amount, accounting.taxes.hasCapital);
	QByteArray encoded;
	if(!AuLegacyCity::EncodeAccounting(accounting, encoded, error)) return;
	m_City.accounting = encoded;
	++m_CityRevision;
	lock.unlock();
	SetModified();
}

bool DaLegacyCity::OperateBank(DaPerson& person, const AuLegacyCityProtocol::Request& request,
	AuLegacyCityProtocol::Reply& reply)
{
	using namespace AuLegacyCity;
	using namespace AuLegacyCityProtocol;
	AuLegacyFinanceTransfer transfer(&person);
	if(!IsBank(request.action) || !std::isfinite(request.money) || request.money < 0) return false;
	const auto* environment = person.LockEnvironmentQuery();
	const bool atSite = environment->LegacyCityId() == Id()
		&& environment->LegacyDevelopmentId() == request.developmentId;
	person.UnlockEnvironmentQuery();
	if(!atSite) { return false; }
	const State snapshot = Snapshot();
	const Development* selected = nullptr;
	for(const Development& development : snapshot.developments)
		if(development.id == request.developmentId && !development.demolished && development.completedLevels)
		{ selected = &development; break; }
	if(!selected || (selected->industryId != 100 && selected->industryId != 123))
	{ reply.message = tr("This development is not a completed bank or capitol."); return false; }
	const bool inBank = selected->industryId == 100;
	const bool inCapitol = selected->industryId == 123;
	const quint32 empire = snapshot.controllingEmpire;
	const bool ownEmpire = empire && person.EmpireId() == empire;
	AuIdList places;
	places.append(Id());
	person.ChainOfPlaces(places);
	AuId capital;
	const DgData& gd = g_Au.LockDataForRead();
	const bool hasCapital = gd.EmpireCapital(empire, capital) && !capital.IsNull();
	const bool governmentAuthorized = ownEmpire && gd.EmpireCitizenCanAccessGovtAccount(empire, person.Id(), places);
	const bool capitalAuthorized = ownEmpire && gd.EmpireCitizenCanSetCapital(empire, person.Id());
	g_Au.UnlockData();
	if(request.action == SetEmpireCapital)
	{
		if(!inCapitol || !capitalAuthorized)
		{ reply.message = tr("Stand at a capitol of your empire and obtain authority to designate its capital."); return false; }
		if(snapshot.citizenSpecies.isEmpty())
		{ reply.message = tr("The city's citizen species is not available."); return false; }
	}
	else if(request.action != BankState && !inBank)
	{ return false; }
	const bool deposit = request.action == GovernmentDeposit;
	const bool withdraw = request.action == GovernmentWithdraw;
	const bool borrow = request.action == BankBorrow;
	const bool repay = request.action == BankRepay;
	const bool claim = request.action == BankClaimReward;
	const bool mutation = deposit || withdraw || borrow || repay || claim;
	if(mutation && !claim && request.money <= 0)
	{ reply.message = tr("Enter a positive amount of money."); return false; }
	if((deposit || withdraw) && !governmentAuthorized)
	{ reply.message = tr("Only government officials in this city's chain of command may access its treasury."); return false; }
	if(claim && !ownEmpire)
	{ reply.message = tr("Only a bank of my empire can redeem government debt."); return false; }
	const double rewardOwed = person.GetReward();
	if(claim && rewardOwed < 1)
	{ reply.message = tr("The government of my empire does not owe a debt to me."); return false; }
	const double personalBalance = person.AccountBalance();
	const double personalLoan = person.LoanBalance();
	const double creditAvailable = std::max(0.0, person.CreditLimit() - personalLoan);
	if((deposit || repay) && personalBalance < request.money)
	{ reply.message = tr("My personal account does not contain that much money."); return false; }
	if(borrow && request.money > creditAvailable)
	{ reply.message = tr("You have insufficient credit available to borrow that much money."); return false; }
	if(repay && request.money > personalLoan)
	{ reply.message = tr("You do not owe that much money to the bank."); return false; }
	AccountingState accounting;
	QString error;
	QWriteLocker lock(&m_CityLock);
	if(m_City.controllingEmpire != empire)
	{ reply.message = tr("The city's government changed. Refresh the bank window."); return false; }
	bool exists = false;
	for(const Development& development : m_City.developments)
		if(development.id == request.developmentId && !development.demolished && development.completedLevels
			&& development.industryId == selected->industryId) { exists = true; break; }
	if(!exists || !DecodeAccounting(m_City.accounting, accounting, error))
	{ reply.message = error.isEmpty() ? tr("The bank is no longer available.") : error; return false; }
	// Capital designation is published through the empire registry. Refresh
	// its presence before using the treasury rather than waiting for the next
	// population cycle to update the accounting cache.
	const bool capitalRecognitionChanged = accounting.taxes.hasCapital != hasCapital;
	accounting.taxes.hasCapital = hasCapital;
	AuSpecies capitalSpecies;
	QString capitalName;
	if(request.action == SetEmpireCapital)
	{
		if(m_City.citizenSpecies.isEmpty())
		{ reply.message = tr("The city's citizen species is not available."); return false; }
		capitalSpecies = AuSpecies(m_City.citizenSpecies);
		capitalName = m_City.name;
	}
	if(withdraw && accounting.accounts.government < request.money)
	{ reply.message = tr("The government treasury does not contain that much money."); return false; }
	if(borrow && accounting.accounts.LoanFundsAvailable(m_City.population) < request.money)
	{ reply.message = tr("Bank reserve policies prevent a loan of that size at this time."); return false; }
	// The original caller still debits player cash when no capital exists.
	if(deposit) accounting.accounts.Credit(GovernmentIncome::Deposit, request.money, accounting.taxes.hasCapital);
	if(withdraw) accounting.accounts.Debit(GovernmentExpense::Withdrawal, request.money);
	if(borrow) accounting.accounts.Debit(CitizenExpense::Loans, request.money);
	if(repay) accounting.accounts.Credit(CitizenIncome::Loans, request.money, accounting.taxes);
	double rewardPaid = 0, rewardNet = 0;
	if(claim)
	{
		rewardPaid = std::min(rewardOwed, accounting.accounts.government);
		if(rewardPaid <= 0) { reply.message = tr("The government treasury in this city has no funds."); return false; }
		accounting.accounts.Debit(GovernmentExpense::Rewards, rewardPaid);
		if(accounting.taxes.income > 0)
		{
			// Original reward redemption uses single precision for tax and net,
			// retained in the old-city section of RedeemGovtDebt's modern source.
			const float incomeTax = float(rewardPaid * (accounting.taxes.income / 100.0f));
			rewardNet = float(rewardPaid - incomeTax);
			accounting.accounts.Credit(GovernmentIncome::IncomeTax, incomeTax, accounting.taxes.hasCapital);
		}
		else rewardNet = rewardPaid;
	}
	QByteArray encoded;
	if(!EncodeAccounting(accounting, encoded, error)) { reply.message = error; return false; }
	if(mutation || capitalRecognitionChanged)
	{
		m_City.accounting = encoded;
		++m_CityRevision;
	}
	if(mutation)
	{
		reply.accepted = true;
		reply.message = tr("Bank transaction completed.");
	}
	reply.hasBank = true;
	reply.selectedCity = Id().GetID();
	reply.selectedDevelopment = request.developmentId;
	reply.bankCitizenBalance = accounting.accounts.citizens;
	reply.bankGovernmentBalance = governmentAuthorized ? accounting.accounts.government : 0;
	reply.bankLoanFunds = inBank ? accounting.accounts.LoanFundsAvailable(m_City.population) : 0;
	reply.bankGovernmentAuthorized = governmentAuthorized;
	reply.bankCapitalAuthorized = inCapitol && capitalAuthorized;
	reply.bankHasCapital = accounting.taxes.hasCapital;
	reply.bankIsCapital = hasCapital && capital == Id();
	if(governmentAuthorized) reply.bankAccounting = encoded;
	lock.unlock();
	if(request.action == SetEmpireCapital)
	{
		// Publish only after current government/site validation succeeds. The
		// signal can query the city, so it belongs outside its write lock.
		emit GovSetCapital(empire, Id(), capitalSpecies, AuId(), 1);
		reply.accepted = true;
		reply.message = tr("Empire capital designation requested for %1.").arg(capitalName);
	}
	// Account setters mark the actor modified and can emit logging/notices.
	// The actor transfer guard spans the city commit and personal callbacks;
	// release the city lock before entering those callbacks.
	if(deposit || repay) person.DebitAccount(request.money);
	if(withdraw || borrow) person.CreditAccount(request.money);
	if(borrow) person.LoanBorrow(request.money);
	if(repay)
	{
		person.LoanPay(request.money);
		if(person.LoanBalance() < 1) person.LoanPay(person.LoanBalance());
	}
	if(claim)
	{
		person.ReduceReward(rewardPaid);
		person.CreditAccount(rewardNet);
	}
	reply.bankPersonalBalance = person.AccountBalance();
	reply.bankPersonalLoan = person.LoanBalance();
	reply.bankPersonalReward = person.GetReward();
	reply.bankCreditAvailable = std::max(0.0, person.CreditLimit() - reply.bankPersonalLoan);
	if(mutation || capitalRecognitionChanged) SetModified();
	if(request.action == BankState) reply.accepted = true;
	return true;
}
