#include "aucommunicationevent.h"
#include "aucommpacket.h"
#include "auglobal.h"
#include "daperson.h"
#include "davessel.h"
#include "dgdata.h"

AuEventLogEntry AuReceivedCommunicationEvent(const AuCommPacket& message,
	POLITICAL_STANCE stance, const DaPerson* person, const DaVessel* directShip,
	const DaVessel* aboardShip)
{
	AuEventLogEntry::TYPE type = AuEventLogEntry::NeutralComm;
	switch(stance)
	{
	case POL_Friend: type = AuEventLogEntry::FriendComm; break;
	case POL_Enemy: type = AuEventLogEntry::EnemyComm; break;
	case POL_Empire: case POL_Vassal: type = AuEventLogEntry::EmpireComm; break;
	default: break;
	}
	QString text;
	if(directShip) text = DaAtom::tr("%1: %2").arg(directShip->NameWithEmpire()).arg(message.Text());
	else if(!person) return AuEventLogEntry();
	else if(aboardShip) text = DaAtom::tr("%1 <i>(%2)</i> aboard %3: %4")
		.arg(person->Name()).arg(person->Id().StringId()).arg(aboardShip->NameWithEmpire()).arg(message.Text());
	else
	{
		QString empireName;
		const DgData& data = g_Au.LockDataForRead();
		data.EmpireName(person->EmpireId(), empireName);
		g_Au.UnlockData();
		text = DaAtom::tr("%1 %2 <i>(%3)</i>: %4")
			.arg(empireName).arg(person->Name()).arg(person->Id().StringId()).arg(message.Text());
	}
	return AuEventLogEntry(type, text);
}
