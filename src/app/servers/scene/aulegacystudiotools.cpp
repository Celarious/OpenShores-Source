#include "dtchatchannel.h"
#include "auatominterface.h"
#include "auserverinterface.h"
#include "auether.h"
#include "aulegacystudioprotocol.h"
#include "aulegacyspacecraftcatalog.h"
#include "aulegacystudiolibrary.h"
#include "aulegacyshipsave.h"
#include "auscdesignexchange.h"
#include "auscdesign.h"
#include "dgdata.h"
#include "dadesigner.h"
#include "daperson.h"
#include "dtccpacket.h"

void DtChatChannel::ReadLegacyStudioPacket(const QByteArray& payload)
{
	using namespace AuLegacyStudioProtocol;
	Request request; QString error;
	if(!LegacyCityAuthorized() || !DecodeRequest(payload, request, error))
	{
		Disconnect();
		return;
	}
	Reply reply; reply.token = request.token; reply.environmentId = request.environmentId;
	if(m_LastLegacyStudioRequest.isValid() && m_LastLegacyStudioRequest.elapsed() < 20)
	{
		reply.retry = true;
		reply.message = tr("Wait a moment before sending another studio request.");
	}
	else
	{
		m_LastLegacyStudioRequest.restart();
		DaPerson* person = m_I->LockAvatar();
		bool avatarLocked = true;
		DaDesigner* environment = person ? person->FindParentDesignerInEnv() : nullptr;
		if(!person || person->IsDead() || !environment || !environment->IsNormal()
			|| environment->IsDead() || !environment->IsLegacyStudio()
			|| environment->Id().GetID() != request.environmentId)
			reply.message = tr("You are no longer in this legacy studio.");
		else if(request.action != Saved && request.action != BurnMedia && request.action != Library && request.action != LoadDesign)
			environment->ProcessLegacyStudioRequest(*person, request, reply);
		else
		{
			// Capture an authorized immutable snapshot, then release both the
			// avatar deletion protection and environment lock for database work.
			Request snapshot = request; snapshot.action = Snapshot;
			if(request.action != Library) { snapshot.revision = 0; snapshot.generation = 0; }
			environment->ProcessLegacyStudioRequest(*person, snapshot, reply);
			const bool duplicate = request.action != Library && request.operationId <= reply.lastOperation;
			const bool current = request.action == Library || (request.generation == reply.generation && request.revision == reply.revision);
			const bool allowed = request.action == BurnMedia ? reply.canBurn : reply.ownerId == person->Id().GetID();
			const AuId actorId = person->Id();
			AuLegacyShip::Design prepared;
			const quint32 savedLibraryId = reply.libraryDesignId;
			if(duplicate && (request.action == BurnMedia || reply.ownerId == actorId.GetID()))
			{ m_I->UnlockAvatar(); avatarLocked = false; person = nullptr; }
			else if(!current || !allowed || (request.action != Library && !DecodeDocument(reply.document, prepared, error)))
			{
				reply.accepted = false;
				reply.message = !current ? tr("The shared design changed. Review it and try again.")
					: !allowed ? tr("Only the owner may save a library design; burning also requires a saved, valid design.") : error;
			}
			else
			{
				bool reservationRetry = false;
				const bool reserved = request.action != Library;
				if(reserved && !environment->BeginLegacyStudioOperation(*person, request, reservationRetry, error))
				{
					Request refresh = request; refresh.action = Snapshot;
					environment->ProcessLegacyStudioRequest(*person, refresh, reply);
					reply.accepted = false; reply.retry = reservationRetry; reply.message = error;
				}
				else
				{
					AuEther* studioEther = environment->Ether();
					if(request.action == Saved)
					{
						if(!request.name.isEmpty()) prepared.header.name = request.name;
						prepared.header.state = 0; prepared.header.versionStateRaw = 0x20;
					}
					m_I->UnlockAvatar(); avatarLocked = false; person = nullptr;
					quint32 libraryId = savedLibraryId;
					AuScDesignExchangeEntry exchange;
					QVector<LibraryEntry> library;
					bool libraryTruncated = false;
					bool stored = false;
					if(request.action == Saved)
					{
						AuLegacyShip::Design normalized;
						stored = AuLegacyShip::PrepareForSave(prepared, normalized, error);
						if(stored) { prepared = normalized; stored = SaveLegacyStudioDesign(prepared, libraryId, error); }
					}
					else if(request.action == BurnMedia) stored = PrepareLegacyStudioMedia(savedLibraryId, exchange, error);
					else if(request.action == Library) stored = ListLegacyStudioLibrary(actorId, library, libraryTruncated, error);
					else
					{
						libraryId = request.designId;
						stored = LoadLegacyStudioLibrary(libraryId, actorId, prepared, error);
					}
					person = m_I->LockAvatar();
					avatarLocked = true;
					environment = person ? person->FindParentDesignerInEnv() : nullptr;
					if(!LegacyCityAuthorized() || !person || person->Id() != actorId || person->IsDead()
						|| !environment || !environment->IsNormal()
						|| environment->IsDead() || !environment->IsLegacyStudio()
						|| environment->Id().GetID() != request.environmentId)
					{
						reply = Reply(); reply.token = request.token; reply.environmentId = request.environmentId;
						reply.message = tr("Your studio changed before the operation completed.");
					}
					else if(request.action == Library)
					{
						environment->ProcessLegacyStudioRequest(*person, request, reply);
						if(stored && reply.accepted)
						{ reply.hasLibrary = true; reply.library = library; reply.libraryTruncated = libraryTruncated; }
						else { reply.accepted = false; if(!stored) reply.message = error; }
					}
					else if(!stored)
					{
						Request refresh = request; refresh.action = Snapshot;
						environment->ProcessLegacyStudioRequest(*person, refresh, reply);
						reply.accepted = false; reply.message = error;
					}
					else
					{
						bool mediaChanged = false;
						environment->ProcessLegacyStudioRequest(*person, request, reply,
							[&](AuLegacyShip::Design& candidate, quint32& committedId, QString& message) {
								if(request.action == Saved || request.action == LoadDesign)
								{ candidate = prepared; committedId = libraryId; return true; }
								if(committedId != savedLibraryId) { message = tr("The saved library design changed."); return false; }
								DgData& data = g_Au.LockScDesignsForWrite();
								const auto runtime = data.m_ScDesigns->find(exchange.DesignId());
								if(runtime == data.m_ScDesigns->end() || runtime->second.IsDeleted() || !runtime->second.LegacyStats())
								{ g_Au.UnlockScDesigns(); message = tr("The legacy runtime blueprint is unavailable."); return false; }
								mediaChanged = person->BurnLegacyStudioMedia(exchange, message);
								if(mediaChanged)
								{
									runtime->second.SetState(AuDesign::Final);
									g_Au.ScDesignChanged(exchange.DesignId());
								}
								g_Au.UnlockScDesigns();
								return mediaChanged;
							});
						if(mediaChanged)
						{
							// Weight refresh may query the environment; do it after
							// releasing the studio write lock used by the callback.
							person->LockGearForWrite(); person->UpdateGearWeight(); person->UnlockGear();
						}
					}
					if(reserved && studioEther)
					{
						// LockAvatar retains the global deletion protection even
						// when the avatar switched or its lookup returned null.
						if(auto* original = dynamic_cast<DaDesigner*>(studioEther->FindNormalAtom(AuId(request.environmentId))))
							original->EndLegacyStudioOperation(actorId, request.operationId);
					}
				}
			}
		}
		if(person && request.action == BurnMedia && !reply.retry && !reply.message.isEmpty())
			person->Thought(reply.message);
		// UnlockAvatar always pairs the most recent LockAvatar, including a
		// null result. The duplicate branch already released its first lock.
		if(avatarLocked) m_I->UnlockAvatar();
	}
	const QByteArray response = EncodeReply(reply);
	if(response.isEmpty()) return;
	QByteArray packet(1, char(DtChatChannelPacket::PD_LegacyStudio));
	packet.append(response); SendPacket(packet);
}
