#ifndef AuLegacyStudioSession_H
#define AuLegacyStudioSession_H

#include "aulegacystudioprotocol.h"
#include <QObject>
#include <QElapsedTimer>
#include <QQueue>

class AuLegacyShipViewer;
class QTimer;

// One connected editor's ordered command stream. The working document and edit
// order belong to the server; retries retain their operation ID for deduplication.
class AuLegacyStudioSession : public QObject
{
public:
	AuLegacyStudioSession(AuLegacyShipViewer* viewer, quint32 environmentId, quint32 personId);
	bool ConfirmExit();
	void Stop();

private:
	AuLegacyShipViewer* m_Viewer;
	QTimer* m_Timer;
	quint32 m_EnvironmentId, m_PersonId, m_Generation = 0;
	quint64 m_Revision = 0, m_LastOperation = 0;
	bool m_Stopped = false, m_Connected = false, m_HasDocument = false, m_HasPending = false;
	bool m_NeedSnapshot = true, m_WarnedTimeout = false;
	int m_RetryAfterMs = 5000;
	QElapsedTimer m_LastSend, m_LastPoll;
	AuLegacyShip::Design m_Design;
	AuLegacyStudioProtocol::Request m_Pending;
	QQueue<AuLegacyStudioProtocol::Request> m_Queue;
	QString m_Members;

	bool SceneReady() const;
	void Queue(AuLegacyStudioProtocol::Request request);
	void Pump();
	void Receive(QByteArray payload);
	void UpdateControls();
};

#endif
