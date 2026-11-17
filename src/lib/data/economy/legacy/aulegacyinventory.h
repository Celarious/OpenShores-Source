#ifndef AuLegacyInventory_H
#define AuLegacyInventory_H

#include "augear.h"
#include <QPoint>
#include <QSize>

// The 2011 layout uses the existing gear and item identities. Coordinates are
// additional placement data; moving an item within a pack does not rename it.
namespace AuLegacyInventory
{
AUDATA_DLL QSize BodyCapacity(INVENTORYPOS position);
AUDATA_DLL QSize ContainerSize(const AuItem& container);
AUDATA_DLL AuGearPos ItemAtBodyPosition(const AuGear& gear, INVENTORYPOS position);
AUDATA_DLL bool IsAncestorPosition(const AuGearPos& source, const AuGearPos& destination);
AUDATA_DLL bool FindSpace(const AuItem& container, const AuItem& item, QPoint& cell,
                         const AuItem* ignored = 0);

// Assign missing layout data to older saves without discarding items that no
// longer fit. The UI also exposes these unplaced items so they can be moved.
AUDATA_DLL void Normalize(AuGear& gear);
AUDATA_DLL void NormalizeContainer(AuItem& item);
AUDATA_DLL bool AddItem(AuGear& gear, const AuItem& item);
AUDATA_DLL bool MoveToType(AuGear& gear, const AuGearPos& source, AuGearSlot::SLOTTYPE type);
AUDATA_DLL bool MoveItem(AuGear& gear, const AuGearPos& source, INVENTORYPOS bodyPosition,
                       const AuGearPos& containerPosition, const QPoint& cell);
}

#endif
