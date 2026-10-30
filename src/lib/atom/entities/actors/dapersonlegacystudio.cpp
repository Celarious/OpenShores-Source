#include "daperson.h"
#include "augear.h"
#include "auglobal.h"
#include "auitem.h"
#include "aulegacycityshop.h"
#include "auscdesignexchange.h"
#include <memory>

bool DaPerson::BurnLegacyStudioMedia(const AuScDesignExchangeEntry& exchange, QString& message)
{
	// The immutable descriptors are published only after all legacy commodity
	// metadata is initialized. Check both activation and readiness before the
	// first inventory read, item clone, quality roll, or pack-size lookup.
	if(!g_Au.IsLegacyGeneration() || !AuLegacyItemTechnologyActive() || !g_Au.HasLegacyCommodity(CID_BlankDisk)
		|| !g_Au.HasLegacyCommodity(CID_ScDesignDisk))
	{ message = tr("Legacy commodity metadata is not ready."); return false; }
	AuGear* gear = LockGearForWrite();
	const AuGearPos position = gear->FindBest(CID_BlankDisk);
	const AuItem* blank = gear->ItemAt(position);
	if(!blank || !blank->Count())
	{ UnlockGear(); message = tr("I need a blank disk to burn this design."); return false; }
	std::unique_ptr<AuItem> media(blank->Clone());
	if(!media) { UnlockGear(); message = tr("The blank disk could not be prepared."); return false; }
	media->SetCount(1);
	AuGear consumed(*gear);
	AuItem* selected = consumed.ItemAt(position);
	if(!selected) { UnlockGear(); message = tr("The blank disk is no longer available."); return false; }
	if(selected->Count() > 1) selected->SetCount(selected->Count() - 1);
	else consumed.Erase(position);
	AuItemScDesign disk(exchange, CID_ScDesignDisk, media->Quality(), media->EmpireOfOrigin());
	disk.SetTechLevel(media->TechLevel());
	AuGear completed(consumed);
	if(!AuLegacyCity::PackPersonalItem(completed, disk))
	{ UnlockGear(); message = tr("I need room in my gear for the design disk."); return false; }
	const bool functioned = media->UseItem();
	*gear = functioned ? completed : consumed;
	UnlockGear();
	SetModified();
	message = functioned ? tr("%1 burned to disk successfully.").arg(exchange.DesignName())
		: tr("The blank disk was defective; it failed during the save. I should try again with another blank disk.");
	return true;
}
