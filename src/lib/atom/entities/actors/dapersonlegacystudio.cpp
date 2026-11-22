#include "daperson.h"
#include "augear.h"
#include "auglobal.h"
#include "auitem.h"
#include "aulegacycityshop.h"
#include "auscdesignexchange.h"
#include <memory>

bool DaPerson::BurnLegacyStudioMedia(const AuScDesignExchangeEntry* spacecraft, const AuBdDesignExchangeEntry* building,
	QString& message, const std::function<bool(QString&)>& finalize)
{
	// The immutable descriptors are published only after all legacy commodity
	// metadata is initialized. Check both activation and readiness before the
	// first inventory read, item clone, quality roll, or pack-size lookup.
	if(!g_Au.IsLegacyGeneration() || !AuLegacyItemTechnologyActive() || !g_Au.HasLegacyCommodity(CID_BlankDisk)
		|| (building ? (!g_Au.HasLegacyCommodity(CID_Paper) || !g_Au.HasLegacyCommodity(CID_BdDesignDisk)
			|| !g_Au.HasLegacyCommodity(CID_BdDesignDrawing)) : (!spacecraft || !g_Au.HasLegacyCommodity(CID_ScDesignDisk))))
	{ message = tr("Legacy commodity metadata is not ready."); return false; }
	AuGear* gear = LockGearForWrite();
	AuGearPos position = gear->FindBest(CID_BlankDisk);
	if(building && !position.IsValid()) position = gear->FindBest(CID_Paper);
	const AuItem* blank = gear->ItemAt(position);
	if(!blank || !blank->Count())
	{ UnlockGear(); message = building ? tr("I need a blank disk or paper for this building design.") : tr("I need a blank disk to burn this design."); return false; }
	std::unique_ptr<AuItem> media(blank->Clone());
	if(!media) { UnlockGear(); message = tr("The blank disk could not be prepared."); return false; }
	media->SetCount(1);
	AuGear consumed(*gear);
	AuItem* selected = consumed.ItemAt(position);
	if(!selected) { UnlockGear(); message = tr("The blank disk is no longer available."); return false; }
	if(selected->Count() > 1) selected->SetCount(selected->Count() - 1);
	else consumed.Erase(position);
	const bool paper = media->CommodityId() == CID_Paper;
	std::unique_ptr<AuItem> disk;
	if(building) disk.reset(new AuItemBdDesign(*building, paper ? CID_BdDesignDrawing : CID_BdDesignDisk, media->Quality(), media->EmpireOfOrigin()));
	else disk.reset(new AuItemScDesign(*spacecraft, CID_ScDesignDisk, media->Quality(), media->EmpireOfOrigin()));
	disk->SetTechLevel(media->TechLevel());
	AuGear completed(consumed);
	if(!AuLegacyCity::PackPersonalItem(completed, *disk))
	{ UnlockGear(); message = tr("I need room in my gear for the design disk."); return false; }
	if(finalize && !finalize(message)) { UnlockGear(); return false; }
	const bool functioned = media->UseItem();
	*gear = functioned ? completed : consumed;
	UnlockGear();
	SetModified();
	message = functioned ? (paper ? tr("%1 inscribed on paper.") : tr("%1 burned to disk successfully.")).arg(building ? building->DesignName() : spacecraft->DesignName())
		: paper ? tr("The drawing failed. Try another sheet of paper.") : tr("The blank disk was defective; it failed during the save. I should try again with another blank disk.");
	return true;
}
