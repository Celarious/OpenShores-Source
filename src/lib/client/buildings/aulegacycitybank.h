#ifndef AuLegacyCityBank_H
#define AuLegacyCityBank_H

#include "auclientlib.h"
#include "aulegacycityprotocol.h"
#include <QWidget>

class QLineEdit;
class QLabel;
class QPushButton;
class QTimer;

class AUCLIENT_DLL AuLegacyCityBank : public QWidget
{
	Q_OBJECT
public:
	explicit AuLegacyCityBank(QWidget* parent = nullptr);
	void SetTarget(quint32 cityId, quint32 developmentId, bool capitol);
	void Refresh();

protected:
	void showEvent(QShowEvent* event) override;
	void hideEvent(QHideEvent* event) override;

private:
	quint32 m_CityId = 0, m_DevelopmentId = 0, m_PendingToken = 0;
	bool m_Capitol = false, m_RefreshQueued = false, m_HasState = false;
	AuLegacyCityProtocol::Reply m_State;
	QVector<AuLegacyCityProtocol::Request> m_DeferredRequests;
	QLabel *m_Balances, *m_Message, *m_AmountLabel;
	QLineEdit* m_Amount;
	QPushButton *m_Deposit, *m_Withdraw, *m_Borrow, *m_Repay, *m_SetCapital, *m_ClaimRewards;
	QTimer *m_Timer, *m_Timeout, *m_DeferredTimer;
	void Submit(AuLegacyCityProtocol::Action action);
	void SendRequest(const AuLegacyCityProtocol::Request& request);
	void ReceiveReply(QByteArray payload);
	void UpdateControls();
	void ResetTransport();
};

#endif
