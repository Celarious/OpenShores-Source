//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: dtclient.h,v 1.18 2022/04/15 17:04:01 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef DtClient_H
#define DtClient_H

#include "auatompos.h"
#include "auclientlib.h"
#include "aucommpacket.h"
#include "auelapsedtimer.h"
#include "augamegeneration.h"
#include "auid.h"
#include "aupose.h"
#include "auspecies.h"
#include <QImage>
#include <QSemaphore>
#include <QSet>
#include <QTcpSocket>
#include <QThread>

class AuMission;

class AUCLIENT_DLL DtClient : public QThread
{
	Q_OBJECT;

public:
	DtClient(QObject* parent=0);
	~DtClient();

	bool Alive() const
	{
		return !m_semAlive.available();
	}

	bool IsConnected() const
	{
		return m_isConnected;
	}

	void Kill()
	{
		if(Alive())
			m_semAlive.release(1);
	}

	void Logout()
	{
		m_semLogout.release();
		Kill();
	}
	void AcknowledgeGenerationViewsReset() { m_semGenerationViewsReset.release(); }

public slots:
	void TxData(QTcpSocket& tcpSocket);

signals:
	void ClientUnitEmpireReceived(quint32 empireId);

	void Connected();
	// The GUI clears views before the network thread changes item metadata.
	void GenerationChanging();
	void GenerationRejected(QString reason);

	void EmpireDataBegin();

	void EmpireDataComplete();

	void EmpireRequested(quint32 empireId);

	void EstDeferred(AuId idSectorOrWorld);
	void InitSucceeded(AuId idUnit);
	void LoggedOut();
	void LoginFailed(quint8 errCode);	// 0=not logged in, 1=bad version

	void BdDesignNeeded(quint32 designId);
	void ItDesignNeeded(quint32 designId);
	void ScDesignNeeded(quint32 designId);

	void RestartNewAvatar();

	void SceneDisconnectedToDefer();
	void SceneDeferred(int);
	void SceneLoginSucceeded();

	void StarMapErased();

	void StarMapAddSector(AuId sectorId);

	void StartNewAvatarError(unsigned char reason);

	void SwitchAvatarTransferred(AuId transferredId);

protected:
	bool m_GenerationNegotiated = false;
	AuGameGenerationIdentity m_CachedLegacyIdentity;
	QByteArray m_CachedLegacyCatalogue;
	QString m_LegacyCatalogueHintPath;
	void SendGenerationCapability(QTcpSocket& tcpSocket);
	bool AcceptGeneration(const AuGameGenerationIdentity& identity, const QByteArray& cataloguePayload);
	bool ReadPacket(const QByteArray& ba, QTcpSocket& tcpSocket);
	void ReconcileAtomsInScene();
	virtual void run();

private slots:
	void NetworkError(QAbstractSocket::SocketError socketError);
	void RxData(QTcpSocket& tcpSocket);

private:
	int			m_BytesRx;				// Total bytes received from client
	int			m_BytesTx;				// Total bytes sent to client
	AuId		m_idDeferAtom;			// Atom id in last defer packet received.
										// It will be a sector when deferred to a manager, a homeworld when deferred to a worker.
	bool		m_isConnected;			// true while connected to a scene server
	AuElapsedTimer	m_LastComm;				// Last reception or transmission of any bytes. Used for keep-alives.
	quint32		m_PacketSize;			// TCP block size
	int			m_PacketSizeBytesLeft;	// Number of packet size bytes we are expecting.
	QSemaphore	m_semAlive;				// Used to know when I'm supposed to die.
	QSemaphore	m_semLogout;			// Send a logout packet before closing the connection.
	QSemaphore	m_semGenerationViewsReset;
	unordered_map<AuId, bool, AuIdHash>	m_SyncLastAtomIds;	// Ids of non-permanent atoms that belong in the scene.
	bool		m_SceneRemovalPending;	// Retry removal after retained child atoms have moved or departed.
	QSet<AuId>	m_LegacyMissingShips;	// Restore boarding/collision if a retained ship reappears in the server scene.
	// Keep server position separate from local boarding prediction between packets in a scene sync.
	struct ServerAvatarPosition
	{
		AuId id;
		AuAtomPos pos;
		AuPoint vec;
		AuPose pose;
		bool atRest = false;
		float headTilt = 0;
		quint16 stationId = 0;
		quint8 seatIndex = 0;
	};
	ServerAvatarPosition m_ServerAvatarPosition;
	bool		m_EmpireDataIncoming;	// true while we are receiving a batch of empire data.
	quint32		m_ClientUnitEmpire;		// client unit empire id
	bool		m_HasClientUnitEmpire;	// true if we have received the client unit's empire record

	QSet<quint32>						m_EmpiresRequested;		// Empires for which we have requested an empire record.
	QMap<AuSpecies, AuElapsedTimer>		m_SpeciesRequested;		// When we last requested each specie record.
};

#endif // DtClient_H
