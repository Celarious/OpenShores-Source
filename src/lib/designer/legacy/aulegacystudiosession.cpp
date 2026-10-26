#include "aulegacystudiosession.h"
#include "aulegacyshipviewer.h"
#include "auapp.h"
#include "auframe.h"
#include "auview.h"
#include "dadesignbase.h"
#include "daperson.h"
#include <QMessageBox>
#include <QStringList>
#include <QTimer>
#include <algorithm>

namespace
{
bool MutatesStudio(AuLegacyStudioProtocol::Action action)
{
	return action != AuLegacyStudioProtocol::Snapshot && action != AuLegacyStudioProtocol::Library;
}
}

AuLegacyStudioSession::AuLegacyStudioSession(AuLegacyShipViewer* viewer, quint32 environmentId, quint32 personId)
	: QObject(viewer), m_Viewer(viewer), m_Timer(new QTimer(this)),
	m_EnvironmentId(environmentId), m_PersonId(personId)
{
	m_Viewer->SetStudioCommandHandler([this](AuLegacyStudioProtocol::Request request) { Queue(request); });
	m_Viewer->SetStudioConnected(false);
	m_Viewer->SetStudioBurnAvailable(false);
	m_Viewer->SetStudioLibraryDesignId(0);
	m_Viewer->ShowStudioMessage(tr("Receiving the studio's working design..."));
	m_Timer->setInterval(40);
	connect(m_Timer, &QTimer::timeout, this, [this]() { Pump(); });
	connect(auApp, &AuApp::LegacyStudioReplyReceived, this, [this](QByteArray bytes) { Receive(bytes); });
	connect(auApp, &AuApp::LegacyScTransportReset, this, [this]() {
		if(m_Stopped) return;
		m_Connected = false; m_NeedSnapshot = true;
		m_LastSend.invalidate(); m_WarnedTimeout = false;
		UpdateControls();
		m_Viewer->ShowStudioMessage(tr("Disconnected. This is the last confirmed studio design. "
			"Unconfirmed edits remain queued until the connection returns."));
	});
	m_Timer->start();
	Pump();
}

bool AuLegacyStudioSession::SceneReady() const
{
	if(!auApp->IsSceneConnected() || !auApp->Frame() || !auApp->Frame()->View()) return false;
	AuView* view = auApp->Frame()->View();
	DaPerson* person = nullptr; view->LockSceneAtom(&person);
	const DaDesignBase* environment = person ? person->FindParentDesignBaseInEnv() : nullptr;
	const bool ready = person && person->Id().GetID() == m_PersonId && !person->IsInTransition()
		&& environment && environment->Id().GetID() == m_EnvironmentId;
	view->UnlockSceneAtom();
	return ready;
}

void AuLegacyStudioSession::UpdateControls()
{
	m_Viewer->SetStudioConnected(m_Connected && !m_Stopped);
	if(!m_Connected || m_Stopped) m_Viewer->SetStudioBurnAvailable(false);
	// Polls never block export. On a disconnect the last confirmed design
	// remains exportable by its owner, and ConfirmExit handles queued edits.
	bool busy = m_HasPending && MutatesStudio(m_Pending.action);
	for(const auto& request : m_Queue) busy = busy || MutatesStudio(request.action);
	m_Viewer->SetStudioBusy(m_Connected && busy);
}

void AuLegacyStudioSession::Queue(AuLegacyStudioProtocol::Request request)
{
	using namespace AuLegacyStudioProtocol;
	if(m_Stopped || !m_Connected || !m_HasDocument || !SceneReady())
	{
		m_Viewer->ShowStudioMessage(tr("Wait for the studio connection before editing."));
		return;
	}
	if(request.action == Snapshot || m_Queue.size() >= 4096)
	{
		m_Viewer->ShowStudioMessage(tr("Wait for the studio to finish applying your edits."));
		return;
	}
	if(request.action == Library)
	{
		if(m_HasPending && m_Pending.action == Library) return;
		for(const auto& queued : m_Queue) if(queued.action == Library) return;
	}
	request.environmentId = m_EnvironmentId;
	// Validate locally before queueing; a malformed tool request must not
	// become an indefinitely retried network operation.
	request.token = 1; request.operationId = 1;
	if(EncodeRequest(request).isEmpty())
	{
		m_Viewer->ShowStudioMessage(tr("The studio request exceeds the supported tool, name, or document limits."));
		return;
	}
	request.token = 0; request.operationId = 0;
	m_Queue.enqueue(request);
	UpdateControls();
	Pump();
}

void AuLegacyStudioSession::Pump()
{
	using namespace AuLegacyStudioProtocol;
	if(m_Stopped) return;
	if(!SceneReady())
	{
		if(m_Connected)
		{
			m_Connected = false; m_NeedSnapshot = true;
			UpdateControls();
			m_Viewer->ShowStudioMessage(tr("The studio connection is unavailable. This is the last confirmed design."));
		}
		return;
	}
	if(m_LastSend.isValid() && m_LastSend.elapsed() < 40) return;
	if(m_HasPending)
	{
		if(m_LastSend.isValid() && m_LastSend.elapsed() < m_RetryAfterMs) return;
		// An acknowledgement can be lost after the command was applied. The
		// same operation ID lets the server return its state without a repeat.
		const quint32 token = auApp->RequestLegacyStudio(m_Pending);
		if(token) { m_Pending.token = token; m_LastSend.restart(); m_RetryAfterMs = 5000; }
		if(!m_WarnedTimeout)
		{
			m_WarnedTimeout = true;
			m_Viewer->ShowStudioMessage(tr("Waiting for the studio server. Your edits remain queued."));
		}
		return;
	}
	Request request;
	if(m_Connected && !m_Queue.isEmpty())
	{
		request = m_Queue.dequeue();
		if(MutatesStudio(request.action) && request.generation != m_Generation)
		{
			m_Viewer->ShowStudioMessage(tr("The owner loaded a different design. Edits for the previous design were not applied."));
			UpdateControls();
			return;
		}
		request.operationId = MutatesStudio(request.action) ? ++m_LastOperation : 0;
		// New/Load and Save retain the exact revision shown in their dialogs.
		if(request.action == Edit || request.action == Password || request.action == Library) request.revision = m_Revision;
	}
	else
	{
		if(!m_NeedSnapshot && m_LastPoll.isValid() && m_LastPoll.elapsed() < 500) return;
		request.action = Snapshot;
		request.environmentId = m_EnvironmentId;
		request.generation = m_Generation;
		request.revision = m_NeedSnapshot ? 0 : m_Revision;
		m_LastPoll.restart();
	}
	m_Pending = request;
	m_Pending.environmentId = m_EnvironmentId;
	m_Pending.token = auApp->RequestLegacyStudio(m_Pending);
	m_HasPending = m_Pending.token != 0;
	if(m_HasPending) { m_LastSend.restart(); m_WarnedTimeout = false; m_RetryAfterMs = 5000; }
	else
	{
		// A connection can disappear after SceneReady. Keep a mutation and
		// its operation ID for retry even when no packet was queued yet.
		m_HasPending = MutatesStudio(request.action);
		m_LastSend.restart();
		m_Viewer->ShowStudioMessage(tr("The studio request could not be sent."));
		if(!auApp->IsSceneConnected()) m_Connected = false;
	}
	UpdateControls();
}

void AuLegacyStudioSession::Receive(QByteArray payload)
{
	using namespace AuLegacyStudioProtocol;
	if(m_Stopped || !m_HasPending) return;
	Reply reply;
	if(!DecodeReply(payload, reply) || reply.token != m_Pending.token || reply.environmentId != m_EnvironmentId) return;
	if(reply.retry)
	{
		m_RetryAfterMs = 200; m_LastSend.restart();
		return;
	}
	const Action action = m_Pending.action;
	m_HasPending = false;
	m_LastOperation = std::max(m_LastOperation, reply.lastOperation);
	if(!reply.generation || !reply.revision)
	{
		m_Connected = false;
		UpdateControls();
		if(!reply.message.isEmpty()) m_Viewer->ShowStudioMessage(reply.message);
		return;
	}
	if(reply.hasDocument)
	{
		AuLegacyShip::Design incoming; QString error;
		if(!DecodeDocument(reply.document, incoming, error))
		{
			m_Connected = false; m_NeedSnapshot = true;
			UpdateControls(); m_Viewer->ShowStudioMessage(error);
			return;
		}
		m_Design = std::move(incoming); m_HasDocument = true;
	}
	if(!m_HasDocument)
	{
		m_NeedSnapshot = true;
		UpdateControls();
		return;
	}
	int discarded = 0;
	if(m_Generation && reply.generation != m_Generation)
	{
		QQueue<Request> current;
		while(!m_Queue.isEmpty())
		{
			const Request queued = m_Queue.dequeue();
			if(!MutatesStudio(queued.action) || queued.generation == reply.generation) current.enqueue(queued);
			else ++discarded;
		}
		m_Queue = current;
	}
	m_Generation = reply.generation; m_Revision = reply.revision;
	m_NeedSnapshot = false; m_Connected = SceneReady();
	m_Viewer->ApplyStudioState(m_Design, reply.ownerId == m_PersonId, reply.modified, m_Generation, m_Revision);
	m_Viewer->SetStudioBurnAvailable(m_Connected && reply.canBurn);
	m_Viewer->SetStudioLibraryDesignId(reply.libraryDesignId);
	if(reply.hasLibrary) m_Viewer->ApplyStudioLibrary(reply.library, reply.libraryTruncated);
	// Apply the authoritative dirty state before releasing a Save/Exit wait.
	UpdateControls();
	QStringList names;
	for(const auto& member : reply.members)
		names.append(member.id == reply.ownerId ? tr("%1 (owner)").arg(member.name) : member.name);
	const QString members = names.join(QStringLiteral(", "));
	if(members != m_Members)
	{
		m_Members = members;
		m_Viewer->ShowStudioMessage(tr("In studio: %1").arg(members));
	}
	if(!reply.message.isEmpty()) m_Viewer->ShowStudioMessage(reply.message);
	else if(!reply.accepted && action != Snapshot)
		m_Viewer->ShowStudioMessage(tr("The studio could not apply that change."));
	else if(discarded)
		m_Viewer->ShowStudioMessage(tr("The owner loaded a different design. %1 queued edits for the previous design were not applied.").arg(discarded));
}

bool AuLegacyStudioSession::ConfirmExit()
{
	bool unconfirmed = m_HasPending && MutatesStudio(m_Pending.action);
	for(const auto& request : m_Queue) unconfirmed = unconfirmed || MutatesStudio(request.action);
	if(m_Connected || !unconfirmed) return true;
	return QMessageBox::question(m_Viewer, tr("Leave Design Studio"),
		tr("Some edits have not been confirmed by the server. Leave the studio anyway? "
			"Queued edits will be abandoned."), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
}

void AuLegacyStudioSession::Stop()
{
	m_Stopped = true; m_Timer->stop();
	m_Queue.clear(); m_HasPending = false;
}
