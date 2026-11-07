#ifndef AuLegacyStudioSession_H
#define AuLegacyStudioSession_H

#include "aulegacystudioprotocol.h"
#include <QObject>
#include <QElapsedTimer>
#include <QQueue>

class AuLegacyShipViewer;
class QTimer;

// Edits are displayed locally and sent in ordered batches. The confirmed copy
// is kept separately so remote changes can be combined with unsent local edits.
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
	bool m_FlushEdits = false, m_Waiting = false;
	bool m_HasPendingDesign = false;
	int m_RetryAfterMs = 5000;
	QElapsedTimer m_LastSend, m_LastPoll, m_BatchWait;
	AuLegacyShip::Design m_Design, m_PendingDesign;
	AuLegacyStudioProtocol::Request m_Pending;
	QQueue<AuLegacyStudioProtocol::Request> m_Queue;
	QString m_Members;

	bool SceneReady() const;
	bool HasUnconfirmed() const;
	bool HasLocalEdits() const;
	bool Flush(bool wait);
	void Queue(AuLegacyStudioProtocol::Request request);
	void Pump();
	void Receive(QByteArray payload);
	void UpdateControls();
};

#endif
