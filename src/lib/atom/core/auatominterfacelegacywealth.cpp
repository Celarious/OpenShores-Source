#include "auatominterface.h"
#include "auglobal.h"
#include "aulegacywealthlevy.h"
#include "daperson.h"
#include "dgdata.h"

void AuAtomInterface::GovLegacyWealthLevy(QByteArray request)
{
	AuLegacyCity::WealthLevyOrder order;
	if(!g_Au.IsLegacyGeneration() || !AuLegacyCity::DecodeWealthLevy(request, order)) return;
	bool authorized = false;
	DaPerson* person = LockAvatar();
	if(person && !person->IsDead())
	{
		// Client-supplied identity is never trusted. The live avatar provides it.
		order.empire = person->EmpireId(); order.issuer = person->Id(); order.issuerName = person->Name();
		const DgData& gd = g_Au.LockDataForRead();
		authorized = order.empire && gd.EmpireCitizenCanSetTax(order.empire, order.issuer);
		
		AuId capital;
		const bool hasCapital = gd.EmpireCapital(order.empire, capital) && !capital.IsNull();
		g_Au.UnlockData();

		if(authorized && !hasCapital)
		{
			person->Thought(tr("Designate an empire capital before levying wealth."));
			authorized = false;
		}
	}
	UnlockAvatar();
	if(authorized) emit LegacyWealthLevy(AuLegacyCity::EncodeWealthLevy(order));
}
