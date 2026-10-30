#ifndef AuLegacyCityManufacture_H
#define AuLegacyCityManufacture_H

#include "auclientlib.h"
#include "aulegacycityprotocol.h"
#include <QWidget>

class QLabel;
class QTimer;
class QVBoxLayout;
class AuLegacyManufacturePanel;

// F10 manufacturing pages share city-owned snapshots, component presentation
// and transport. The labor window owns their tabs and occupied-site selection.
class AUCLIENT_DLL AuLegacyCityManufacture : public QWidget
{
	Q_OBJECT
public:
	explicit AuLegacyCityManufacture(QWidget* parent = nullptr);
	void SetTarget(quint32 cityId, quint32 developmentId);
	void Refresh();
	bool HasIndustry() const { return m_HasIndustry; }
	bool HasSpacecraftFactory() const { return m_HasSpacecraftFactory; }
	QWidget* SpacecraftPage() const { return m_SpacecraftPage; }

signals:
	void AvailabilityChanged(bool available);

protected:
	void showEvent(QShowEvent* event) override;
	void hideEvent(QHideEvent* event) override;
	bool eventFilter(QObject* object, QEvent* event) override;

private:
	quint32 m_CityId = 0, m_DevelopmentId = 0, m_PendingToken = 0;
	bool m_HasIndustry = false, m_HasSpacecraftFactory = false, m_RefreshQueued = false;
	QVector<AuLegacyCityProtocol::Request> m_DeferredRequests;
	QTimer *m_Timer, *m_Timeout, *m_DeferredTimer;
	QLabel* m_Message;
	QWidget* m_SpacecraftPage;
	QLabel* m_SpacecraftMessage;
	AuLegacyManufacturePanel* m_SpacecraftPanel;
	QVBoxLayout* m_PanelLayout;
	QVector<AuLegacyManufacturePanel*> m_Panels;
	void Submit(AuLegacyCityProtocol::Request request);
	void SendRequest(AuLegacyCityProtocol::Request request);
	void ReceiveReply(QByteArray payload);
	void ResetTransport();
	void SetAvailable(bool available, bool spacecraftFactory = false);
	void SetMessage(const QString& message);
};

#endif
