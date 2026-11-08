#ifndef AuCommunicationEvent_H
#define AuCommunicationEvent_H

#include "auatom.h"
#include "audef.h"
#include "aueventlog.h"

class AuCommPacket;
class DaPerson;
class DaVessel;

// The received-message event shared by cities and modern buildings. Keep
// sender validation, channel/range rules and event ownership at the caller.
AUATOM_DLL AuEventLogEntry AuReceivedCommunicationEvent(const AuCommPacket& message,
	POLITICAL_STANCE stance, const DaPerson* person, const DaVessel* directShip,
	const DaVessel* aboardShip);
#endif
