#include "aulegacyinventory.h"

#include <QList>
#include <QRect>
#include <memory>

namespace
{
const quint32 CarryPositions = 0x0f070000U;
const quint32 BodyPositions = 0x3fffffffU;

bool IsBodyPosition(INVENTORYPOS position)
{
	return position >= IP_ArmorArms && position <= IP_WristRight;
}

AuGearSlot::SLOTTYPE BodyType(INVENTORYPOS position)
{
	switch(position)
	{
	case IP_ArmorHead:
	case IP_ClothingHead:
		return AuGearSlot::Head;
	case IP_ArmorFace:
	case IP_ClothingFace:
		return AuGearSlot::Face;
	case IP_AnkleLeft:
	case IP_AnkleRight:
	case IP_WristLeft:
	case IP_WristRight:
		return AuGearSlot::Limb;
	case IP_Back:
		return AuGearSlot::Body;
	case IP_HandLeft:
	case IP_HandRight:
		return AuGearSlot::Hand;
	case IP_Neck:
		return AuGearSlot::Neck;
	case IP_Ring1Left:
	case IP_Ring1Right:
	case IP_Ring2Left:
	case IP_Ring2Right:
		return AuGearSlot::Digit;
	case IP_WaistBack:
	case IP_WaistFront:
	case IP_WaistLeft:
	case IP_WaistRight:
		return AuGearSlot::Waist;
	default:
		return position >= IP_ArmorArms && position <= IP_ClothingUpperTorso
			? AuGearSlot::BodyWear : AuGearSlot::Invalid;
	}
}

bool CanEquip(const AuItem& item, INVENTORYPOS position)
{
	if(!IsBodyPosition(position) || !item.CanBeCarried())
		return false;
	if(!((item.BitsWear() | CarryPositions) & (quint32(1) << position)))
		return false;
	const QSize capacity = AuLegacyInventory::BodyCapacity(position);
	const QSize footprint = item.PackSize();
	return footprint.width() > 0 && footprint.height() > 0
		&& (capacity.isEmpty() || (footprint.width() <= capacity.width()
			&& footprint.height() <= capacity.height()));
}

quint32 OccupiedPositions(const AuItem& item, INVENTORYPOS position)
{
	if(!IsBodyPosition(position))
		return 0;
	const quint32 bit = quint32(1) << position;
	const quint32 wear = item.BitsWear();
	if((wear & bit) && !(wear & (quint32(1) << IP_EquipOr)))
		return wear & BodyPositions;
	return bit;
}

bool CanPlaceBody(const AuGear& gear, const AuItem& item, INVENTORYPOS position,
	const AuGearPos& ignored = AuGearPos())
{
	if(!CanEquip(item, position))
		return false;
	const quint32 occupied = OccupiedPositions(item, position);
	for(AuGear::const_iterator entry = gear.cbegin(); entry != gear.cend(); ++entry)
		if(AuGearPos(*entry) != ignored
			&& (OccupiedPositions(entry->Item(), entry->LegacyPosition()) & occupied))
			return false;
	return true;
}

bool FitsAt(const AuItem& container, const AuItem& item, const QPoint& cell,
	const AuItem* ignored = 0)
{
	const QSize size = AuLegacyInventory::ContainerSize(container);
	const QSize footprint = item.PackSize();
	if(cell.x() < 0 || cell.y() < 0 || footprint.width() <= 0 || footprint.height() <= 0
		|| footprint.width() > size.width() || footprint.height() > size.height()
		|| cell.x() > size.width() - footprint.width()
		|| cell.y() > size.height() - footprint.height())
		return false;
	const AuItemContent* contents = container.Content();
	if(!contents)
		return false;
	const QRect placed(cell, footprint);
	for(AuItemContent::const_iterator entry = contents->cbegin(); entry != contents->cend(); ++entry)
	{
		if(entry->second == ignored)
			continue;
		const QPoint otherCell = contents->LegacyPosition(entry->first);
		if(otherCell.x() >= 0 && otherCell.y() >= 0
			&& placed.intersects(QRect(otherCell, entry->second->PackSize())))
			return false;
	}
	return true;
}

void NormalizeContents(AuItem& container)
{
	AuItemContent* contents = container.Content();
	if(!contents)
		return;
	const QSize size = AuLegacyInventory::ContainerSize(container);
	QList<QRect> occupied;
	for(AuItemContent::iterator entry = contents->begin(); entry != contents->end(); ++entry)
	{
		NormalizeContents(*entry->second);
		const QPoint cell = contents->LegacyPosition(entry->first);
		const QSize footprint = entry->second->PackSize();
		const QRect rectangle(cell, footprint);
		bool valid = cell.x() >= 0 && cell.y() >= 0
			&& footprint.width() > 0 && footprint.height() > 0
			&& footprint.width() <= size.width() && footprint.height() <= size.height()
			&& cell.x() <= size.width() - footprint.width()
			&& cell.y() <= size.height() - footprint.height();
		for(int index = 0; valid && index < occupied.size(); ++index)
			valid = !rectangle.intersects(occupied.at(index));
		if(valid)
			occupied.append(rectangle);
		else
			contents->SetLegacyPosition(entry->first, QPoint(-1, -1));
	}
	for(AuItemContent::iterator entry = contents->begin(); entry != contents->end(); ++entry)
	{
		const QPoint current = contents->LegacyPosition(entry->first);
		if(current.x() >= 0 && current.y() >= 0)
			continue;
		QPoint cell;
		if(AuLegacyInventory::FindSpace(container, *entry->second, cell, entry->second))
			contents->SetLegacyPosition(entry->first, cell);
	}
}

AuGearPos ParentPosition(const AuGearPos& position)
{
	AuGearPos parent(position.Slot(), position.Which());
	for(int index = 0; index + 1 < position.ContentPath().size(); ++index)
		parent = AuGearPos(parent, position.ContentPath().at(index));
	return parent;
}

bool InsertInto(AuItem& container, const AuItem& item, const QPoint& cell)
{
	AuItemContent* contents = container.Content();
	if(!contents || !FitsAt(container, item, cell))
		return false;
	const AuItemContentId insertedId = contents->UnusedId();
	if(!insertedId.IsValid() || !container.FitItemIntoContainer(item))
		return false;
	if(contents->find(insertedId) == contents->end())
		return false;
	contents->SetLegacyPosition(insertedId, cell);
	return true;
}
}

namespace AuLegacyInventory
{
// A container may not be moved into itself or one of its descendants.
bool IsAncestorPosition(const AuGearPos& source, const AuGearPos& destination)
{
	if(source.Slot() != destination.Slot() || source.Which() != destination.Which()
		|| source.ContentPath().size() > destination.ContentPath().size())
		return false;
	for(int index = 0; index < source.ContentPath().size(); ++index)
		if(source.ContentPath().at(index) != destination.ContentPath().at(index))
			return false;
	return true;
}

QSize BodyCapacity(INVENTORYPOS position)
{
	if(position >= IP_ArmorArms && position <= IP_ClothingUpperTorso)
		return QSize(0, 0);
	switch(position)
	{
	case IP_Back:
	case IP_HandLeft:
	case IP_HandRight:
		return QSize(12, 20);
	case IP_WaistBack:
		return QSize(3, 3);
	case IP_WaistFront:
		return QSize(3, 2);
	case IP_WaistLeft:
	case IP_WaistRight:
		return QSize(2, 3);
	default:
		return IsBodyPosition(position) ? QSize(1, 1) : QSize(0, 0);
	}
}

QSize ContainerSize(const AuItem& container)
{
	if(!container.Content())
		return QSize(0, 0);
	const QSize size = container.Commodity().m_capacity;
	return size.width() > 0 && size.height() > 0 ? size : QSize(0, 0);
}

AuGearPos ItemAtBodyPosition(const AuGear& gear, INVENTORYPOS position)
{
	if(!IsBodyPosition(position))
		return AuGearPos();
	const quint32 bit = quint32(1) << position;
	for(AuGear::const_iterator entry = gear.cbegin(); entry != gear.cend(); ++entry)
		if(OccupiedPositions(entry->Item(), entry->LegacyPosition()) & bit)
			return AuGearPos(*entry);
	return AuGearPos();
}

bool FindSpace(const AuItem& container, const AuItem& item, QPoint& cell, const AuItem* ignored)
{
	if(!item.CanBeCarried())
		return false;
	const QSize size = ContainerSize(container);
	const QSize footprint = item.PackSize();
	if(footprint.width() <= 0 || footprint.height() <= 0
		|| footprint.width() > size.width() || footprint.height() > size.height())
		return false;
	// FUN_008b5580 scans rows then columns without rotating the footprint.
	for(int y = 0; y <= size.height() - footprint.height(); ++y)
		for(int x = 0; x <= size.width() - footprint.width(); ++x)
			if(FitsAt(container, item, QPoint(x, y), ignored))
			{
				cell = QPoint(x, y);
				return true;
			}
	return false;
}

void NormalizeContainer(AuItem& item)
{
	NormalizeContents(item);
}

void Normalize(AuGear& gear)
{
	quint32 occupied = 0;
	for(AuGear::iterator entry = gear.begin(); entry != gear.end(); ++entry)
	{
		NormalizeContents(entry->Item());
		const INVENTORYPOS position = entry->LegacyPosition();
		const quint32 mask = OccupiedPositions(entry->Item(), position);
		if(!CanEquip(entry->Item(), position) || BodyType(position) != entry->SlotType()
			|| (mask & occupied))
			entry->SetLegacyPosition(IP_NumInventoryPos);
		else
			occupied |= mask;
	}
	for(AuGear::iterator entry = gear.begin(); entry != gear.end(); ++entry)
	{
		if(IsBodyPosition(entry->LegacyPosition()))
			continue;
		for(int index = IP_ArmorArms; index <= IP_WristRight; ++index)
		{
			const INVENTORYPOS position = INVENTORYPOS(index);
			const quint32 mask = OccupiedPositions(entry->Item(), position);
			if(BodyType(position) == entry->SlotType() && CanEquip(entry->Item(), position)
				&& !(mask & occupied))
			{
				entry->SetLegacyPosition(position);
				occupied |= mask;
				break;
			}
		}
	}
}

bool AddItem(AuGear& gear, const AuItem& item)
{
	const QSize footprint = item.PackSize();
	if(!item.CanBeCarried() || footprint.width() <= 0 || footprint.height() <= 0
		|| footprint.width() > 12 || footprint.height() > 20)
		return false;
	AuGear working(gear);
	Normalize(working);
	// FUN_0074ff70: packs on the back, in the right hand, then in the left hand.
	const INVENTORYPOS containers[] = {IP_Back, IP_HandRight, IP_HandLeft};
	for(unsigned int index = 0; index < sizeof(containers) / sizeof(containers[0]); ++index)
	{
		AuItem* container = working.ItemAt(ItemAtBodyPosition(working, containers[index]));
		QPoint cell;
		if(container && FindSpace(*container, item, cell) && InsertInto(*container, item, cell))
		{
			Normalize(working);
			gear = working;
			return true;
		}
	}
	const auto place = [&](INVENTORYPOS position) -> bool
	{
		if(!CanPlaceBody(working, item, position))
			return false;
		const AuGearSlot::SLOTTYPE type = BodyType(position);
		const quint8 which = working.NextWhich(type);
		if(which > 15)
			return false;
		working.append(AuGearSlot(type, which, item));
		working.last().SetLegacyPosition(position);
		Normalize(working);
		gear = working;
		return true;
	};
	const INVENTORYPOS carried[] = {IP_Back, IP_HandLeft, IP_HandRight};
	for(unsigned int index = 0; index < sizeof(carried) / sizeof(carried[0]); ++index)
	{
		if(!ItemAtBodyPosition(working, carried[index]).IsValid())
			return place(carried[index]);
	}
	if(footprint.width() > 3 || footprint.height() > 3)
		return false;
	INVENTORYPOS waist = IP_WaistBack;
	if(ItemAtBodyPosition(working, waist).IsValid())
	{
		if(footprint.width() > 2)
			return false;
		waist = IP_WaistLeft;
		if(ItemAtBodyPosition(working, waist).IsValid())
		{
			waist = IP_WaistRight;
			if(ItemAtBodyPosition(working, waist).IsValid())
				return false;
		}
	}
	// Preserve the original preference and its early-return quirk: the front
	// is considered only after a back/side waist candidate has been found.
	if(footprint.height() < 3 && !ItemAtBodyPosition(working, IP_WaistFront).IsValid())
		waist = IP_WaistFront;
	return place(waist);
}

bool MoveToType(AuGear& gear, const AuGearPos& source, AuGearSlot::SLOTTYPE type)
{
	AuGear working(gear);
	Normalize(working);
	if(!working.ItemAt(source))
		return false;
	for(int index = IP_ArmorArms; index <= IP_WristRight; ++index)
	{
		const INVENTORYPOS position = INVENTORYPOS(index);
		if(BodyType(position) != type || ItemAtBodyPosition(working, position).IsValid())
			continue;
		if(MoveItem(working, source, position, AuGearPos(), QPoint(-1, -1)))
		{
			gear = working;
			return true;
		}
	}
	return false;
}

bool MoveItem(AuGear& gear, const AuGearPos& source, INVENTORYPOS bodyPosition,
	const AuGearPos& containerPosition, const QPoint& cell)
{
	if(!source.IsGear() || (!IsBodyPosition(bodyPosition) && bodyPosition != IP_NumInventoryPos))
		return false;
	AuGear working(gear);
	Normalize(working);
	const AuItem* sourceItem = working.ItemAt(source);
	if(!sourceItem)
		return false;
	if(IsBodyPosition(bodyPosition))
	{
		const AuGearPos destination = ItemAtBodyPosition(working, bodyPosition);
		if(destination.IsValid())
		{
			if(IsAncestorPosition(source, destination) || !working.MoveItem(source, destination))
				return false;
		}
		else
		{
			std::unique_ptr<AuItem> moved(sourceItem->Clone());
			working.Erase(source);
			if(!CanPlaceBody(working, *moved, bodyPosition))
				return false;
			const AuGearSlot::SLOTTYPE type = BodyType(bodyPosition);
			const quint8 which = working.NextWhich(type);
			if(which > 15)
				return false;
			working.append(AuGearSlot(type, which, *moved));
			working.last().SetLegacyPosition(bodyPosition);
		}
	}
	else
	{
		if(!containerPosition.IsGear() || IsAncestorPosition(source, containerPosition))
			return false;
		AuItem* container = working.ItemAt(containerPosition);
		if(!container || !container->Content())
			return false;
		const QSize capacity = ContainerSize(*container);
		if(cell.x() < 0 || cell.y() < 0 || cell.x() >= capacity.width()
			|| cell.y() >= capacity.height())
			return false;
		AuItemContent* contents = container->Content();
		for(AuItemContent::const_iterator entry = contents->cbegin(); entry != contents->cend(); ++entry)
		{
			const QPoint otherCell = contents->LegacyPosition(entry->first);
			if(entry->second == sourceItem || otherCell.x() < 0 || otherCell.y() < 0
				|| !QRect(otherCell, entry->second->PackSize()).contains(cell))
				continue;
			const AuGearPos destination(containerPosition, entry->first);
			if(IsAncestorPosition(source, destination) || !working.MoveItem(source, destination))
				return false;
			Normalize(working);
			gear = working;
			return true;
		}
		if(!source.IsTopLevel() && ParentPosition(source) == containerPosition)
		{
			// Coordinate changes retain the content id used by item selection and actions.
			if(!FitsAt(*container, *sourceItem, cell, sourceItem))
				return false;
			container->Content()->SetLegacyPosition(source.ContentPath().last(), cell);
		}
		else
		{
			if(!InsertInto(*container, *sourceItem, cell))
				return false;
			working.Erase(source);
		}
	}
	Normalize(working);
	gear = working;
	return true;
}
}
