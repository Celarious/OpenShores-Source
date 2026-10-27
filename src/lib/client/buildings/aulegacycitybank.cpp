#include "aulegacycitybank.h"
#include "auapp.h"
#include "auglobal.h"
#include <QGridLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <algorithm>
#include <cmath>

AuLegacyCityBank::AuLegacyCityBank(QWidget* parent) : QWidget(parent)
{
	m_Balances = new QLabel(this); m_Balances->setWordWrap(true);
	m_Message = new QLabel(this); m_Message->setWordWrap(true);
	// The original bank dialogs parse decimal text directly as double.
	m_Amount = new QLineEdit(QStringLiteral("100"), this);
	m_Amount->setAlignment(Qt::AlignRight);
	m_Deposit = new QPushButton(tr("Deposit Funds"), this);
	m_Withdraw = new QPushButton(tr("Withdraw Funds"), this);
	m_Borrow = new QPushButton(tr("Request Loan"), this);
	m_Repay = new QPushButton(tr("Pay Debt"), this);
	m_SetCapital = new QPushButton(tr("Designate Empire Capital"), this);
	m_ClaimRewards = new QPushButton(tr("Redeem Government Debt"), this);
	auto* explanation = new QLabel(tr("Government funds are held separately from citizens' bank accounts. "
		"Deposits and withdrawals transfer money between your personal account and the government treasury. "
		"Government transfers start at 100%1; loans start at 1%1.").arg(g_Au.CurrencySymbol()), this);
	explanation->setWordWrap(true);
	auto* layout = new QGridLayout(this); layout->setMargin(2); layout->setSpacing(4);
	layout->addWidget(m_Balances, 0, 0, 1, 2);
	layout->addWidget(explanation, 1, 0, 1, 2);
	m_AmountLabel = new QLabel(tr("Amount (%1)").arg(g_Au.CurrencySymbol()), this);
	layout->addWidget(m_AmountLabel, 2, 0); layout->addWidget(m_Amount, 2, 1);
	layout->addWidget(m_Borrow, 3, 0); layout->addWidget(m_Repay, 3, 1);
	layout->addWidget(m_Deposit, 4, 0); layout->addWidget(m_Withdraw, 4, 1);
	layout->addWidget(m_SetCapital, 5, 0, 1, 2);
	layout->addWidget(m_ClaimRewards, 6, 0, 1, 2);
	layout->addWidget(m_Message, 7, 0, 1, 2); layout->setRowStretch(8, 1);
	connect(m_Deposit, &QPushButton::clicked, this, [this]() { Submit(AuLegacyCityProtocol::GovernmentDeposit); });
	connect(m_Withdraw, &QPushButton::clicked, this, [this]() { Submit(AuLegacyCityProtocol::GovernmentWithdraw); });
	connect(m_Borrow, &QPushButton::clicked, this, [this]() { Submit(AuLegacyCityProtocol::BankBorrow); });
	connect(m_Repay, &QPushButton::clicked, this, [this]() { Submit(AuLegacyCityProtocol::BankRepay); });
	connect(m_SetCapital, &QPushButton::clicked, this, [this]() { Submit(AuLegacyCityProtocol::SetEmpireCapital); });
	connect(m_ClaimRewards, &QPushButton::clicked, this, [this]() { Submit(AuLegacyCityProtocol::BankClaimReward); });
	connect(m_Amount, &QLineEdit::textChanged, this, [this](const QString&) { UpdateControls(); });
	m_Timer = new QTimer(this); m_Timer->setInterval(1000);
	m_Timeout = new QTimer(this); m_Timeout->setSingleShot(true); m_Timeout->setInterval(60000);
	m_DeferredTimer = new QTimer(this); m_DeferredTimer->setSingleShot(true); m_DeferredTimer->setInterval(250);
	connect(m_Timer, &QTimer::timeout, this, &AuLegacyCityBank::Refresh);
	connect(m_Timeout, &QTimer::timeout, this, [this]() {
		m_PendingToken = 0; m_DeferredTimer->stop(); m_DeferredRequests.clear();
		m_Message->setText(tr("No server response from the bank."));
	});
	connect(m_DeferredTimer, &QTimer::timeout, this, [this]() {
		if(m_PendingToken || m_DeferredRequests.isEmpty()) return;
		const auto request = m_DeferredRequests.takeFirst();
		if(request.cityId == m_CityId && request.developmentId == m_DevelopmentId) SendRequest(request);
	});
	connect(auApp, &AuApp::LegacyCityReplyReceived, this, &AuLegacyCityBank::ReceiveReply);
	connect(auApp, &AuApp::LegacyScTransportReset, this, &AuLegacyCityBank::ResetTransport);
	UpdateControls();
}

void AuLegacyCityBank::SetTarget(quint32 cityId, quint32 developmentId, bool capitol)
{
	if(m_CityId == cityId && m_DevelopmentId == developmentId && m_Capitol == capitol) return;
	m_CityId = cityId; m_DevelopmentId = developmentId; m_Capitol = capitol;
	ResetTransport(); if(cityId && developmentId) Refresh();
}

void AuLegacyCityBank::Submit(AuLegacyCityProtocol::Action action)
{
	if(!m_CityId || !m_DevelopmentId || !g_Au.IsLegacyGeneration()
		|| auApp->IsSingleUserMode() || !auApp->IsSceneConnected()) return;
	AuLegacyCityProtocol::Request request;
	request.action = action; request.cityId = m_CityId; request.developmentId = m_DevelopmentId;
	if(action != AuLegacyCityProtocol::BankState && action != AuLegacyCityProtocol::SetEmpireCapital
		&& action != AuLegacyCityProtocol::BankClaimReward)
	{
		bool valid = false;
		request.money = m_Amount->text().toDouble(&valid);
		if(!valid || !std::isfinite(request.money) || request.money <= 0)
		{ m_Message->setText(tr("Enter a positive amount of money.")); return; }
	}
	if(m_PendingToken || m_DeferredTimer->isActive() || !m_DeferredRequests.isEmpty())
	{
		if(action == AuLegacyCityProtocol::BankState) return;
		// Keep the amount and target from the click while the current request
		// finishes. A status poll must not disable buttons or swallow actions.
		if(m_DeferredRequests.size() < 32) m_DeferredRequests.append(request);
		else m_Message->setText(tr("Wait for the queued bank requests to finish."));
		return;
	}
	SendRequest(request);
}

void AuLegacyCityBank::SendRequest(const AuLegacyCityProtocol::Request& request)
{
	m_PendingToken = auApp->RequestLegacyCityTool(request);
	if(m_PendingToken) m_Timeout->start();
	else
	{
		m_DeferredRequests.clear();
		m_Message->setText(tr("The bank request could not be sent."));
	}
}

void AuLegacyCityBank::Refresh()
{
	if(!m_CityId || !m_DevelopmentId || m_PendingToken || m_RefreshQueued
		|| m_DeferredTimer->isActive() || !m_DeferredRequests.isEmpty()) return;
	// Share the transport with the occupied-site and manufacture polls.
	m_RefreshQueued = true;
	QTimer::singleShot(500, this, [this]() {
		m_RefreshQueued = false; Submit(AuLegacyCityProtocol::BankState);
	});
}

void AuLegacyCityBank::ReceiveReply(QByteArray payload)
{
	AuLegacyCityProtocol::Reply reply;
	if(!AuLegacyCityProtocol::DecodeReply(payload, reply) || !m_PendingToken || reply.token != m_PendingToken) return;
	m_PendingToken = 0; m_Timeout->stop();
	if(reply.hasBank && reply.selectedCity == m_CityId && reply.selectedDevelopment == m_DevelopmentId)
	{ m_State = reply; m_HasState = true; }
	if(reply.action != AuLegacyCityProtocol::BankState || !reply.hasBank) m_Message->setText(reply.message);
	UpdateControls();
	if(!m_DeferredRequests.isEmpty()) m_DeferredTimer->start();
}

void AuLegacyCityBank::UpdateControls()
{
	const auto money = [](double value) { return QStringLiteral("%L1%2").arg(value, 0, 'f', 0).arg(g_Au.CurrencySymbol()); };
	QString balances = tr("Personal account: %1<br>Loan debt: %2<br>Credit available: %3")
		.arg(money(m_State.bankPersonalBalance), money(m_State.bankPersonalLoan), money(m_State.bankCreditAvailable));
	balances += tr("<br>Government debt owed to you: %1").arg(money(m_State.bankPersonalReward));
	if(m_State.hasBank && m_State.bankGovernmentAuthorized)
		balances += tr("<br>Citizen bank accounts: %1<br>Government treasury: %2")
			.arg(money(m_State.bankCitizenBalance), money(m_State.bankGovernmentBalance));
	if(m_State.hasBank && !m_State.bankHasCapital)
		balances += tr("<br>This city has not recognized an empire capital. Government deposits are discarded until it does.");
	if(m_Capitol && m_State.bankIsCapital) balances += tr("<br>This city is the empire capital.");
	m_Balances->setText(balances);
	// Pending transport is not bank state. Keep the last authoritative
	// eligibility while polling, and allow actions before the first snapshot.
	const bool ready = m_CityId && m_DevelopmentId && g_Au.IsLegacyGeneration()
		&& !auApp->IsSingleUserMode() && auApp->IsSceneConnected();
	bool amountValid = false;
	const double amount = m_Amount->text().toDouble(&amountValid);
	amountValid = amountValid && std::isfinite(amount) && amount > 0;
	m_Amount->setVisible(!m_Capitol);
	m_AmountLabel->setVisible(!m_Capitol);
	m_Deposit->setVisible(!m_Capitol); m_Withdraw->setVisible(!m_Capitol);
	m_Borrow->setVisible(!m_Capitol); m_Repay->setVisible(!m_Capitol);
	m_Deposit->setEnabled(ready && amountValid && amount >= 100 && (!m_HasState || (m_State.bankGovernmentAuthorized && amount <= m_State.bankPersonalBalance)));
	m_Withdraw->setEnabled(ready && amountValid && amount >= 100 && (!m_HasState || (m_State.bankGovernmentAuthorized && amount <= m_State.bankGovernmentBalance)));
	m_Borrow->setEnabled(ready && amountValid && amount >= 1 && (!m_HasState || amount <= std::min(m_State.bankLoanFunds, m_State.bankCreditAvailable)));
	m_Repay->setEnabled(ready && amountValid && (!m_HasState || amount <= std::min(m_State.bankPersonalBalance, m_State.bankPersonalLoan)));
	m_SetCapital->setVisible(m_Capitol);
	m_SetCapital->setEnabled(ready && (!m_HasState || (m_State.bankCapitalAuthorized && !m_State.bankIsCapital)));
	m_ClaimRewards->setVisible(!m_Capitol);
	m_ClaimRewards->setEnabled(ready && (!m_HasState || m_State.bankPersonalReward > 0));
}

void AuLegacyCityBank::ResetTransport()
{
	m_PendingToken = 0; m_Timeout->stop(); m_DeferredTimer->stop(); m_DeferredRequests.clear();
	m_HasState = false; m_State = AuLegacyCityProtocol::Reply(); m_Message->clear(); UpdateControls();
}

void AuLegacyCityBank::showEvent(QShowEvent* event)
{ QWidget::showEvent(event); m_Timer->start(); Refresh(); }

void AuLegacyCityBank::hideEvent(QHideEvent* event)
{ m_Timer->stop(); QWidget::hideEvent(event); }
