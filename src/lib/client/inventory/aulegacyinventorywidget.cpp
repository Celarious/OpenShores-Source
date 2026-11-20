#include "aulegacyinventorywidget.h"
#include "aulegacyinventory.h"
#include "auglobal.h"
#include <QApplication>
#include <QHelpEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRegion>
#include <QToolTip>

namespace
{
const int CellPixels = 16;
const char* const BodyImages[30] =
{
	0, 0, 0, 0, "cs_BoxHelmet.png", 0, 0, "cs_BoxArmor.png",
	0, 0, 0, 0, "cs_BoxClothing.png", 0,
	"cs_BoxAnkle.png", "cs_BoxAnkle.png", "cs_GridBack.png",
	"cs_GridHandLeft.png", "cs_GridHandRightUp.png", "cs_BoxNeck.png",
	"cs_BoxRing.png", "cs_BoxRing.png", "cs_BoxRing.png", "cs_BoxRing.png",
	"cs_Grid3x3.png", "cs_Grid3x2.png", "cs_Grid2x3.png", "cs_Grid2x3.png",
	"cs_BoxWrist.png", "cs_BoxWrist.png"
};

QString Resource(const char* name)
{
	return QStringLiteral(":/legacy/inventory/") + QLatin1String(name);
}

}

AuLegacyInventoryWidget::AuLegacyInventoryWidget(QWidget* parent) : QWidget(parent),
	m_Male(true), m_Dragging(false)
{
	setMouseTracking(true);
	Rebuild();
}

QRect AuLegacyInventoryWidget::BodyRectangle(int position) const
{
	// Inclusive QRect coordinates from the original male/female initializers
	// 00a63b90 / 00a647d0. Hidden armor subpositions have no body rectangle.
	static const int male[30][4] =
	{
		{0,0,-1,-1},{0,0,-1,-1},{0,0,-1,-1},{0,0,-1,-1},{66,5,90,29},
		{0,0,-1,-1},{0,0,-1,-1},{50,110,74,134},{0,0,-1,-1},{0,0,-1,-1},
		{0,0,-1,-1},{0,0,-1,-1},{50,140,74,164},{0,0,-1,-1},
		{151,268,168,285},{63,262,80,279},{158,58,194,118},{156,168,192,228},
		{6,27,42,87},{75,41,92,58},{154,232,171,249},{6,6,23,23},
		{175,232,192,249},{27,6,44,23},{23,173,47,197},{23,225,47,241},
		{49,199,65,223},{5,199,21,223},{164,147,181,164},{23,91,40,108}
	};
	static const int female[30][4] =
	{
		{0,0,-1,-1},{0,0,-1,-1},{0,0,-1,-1},{0,0,-1,-1},{46,5,70,29},
		{0,0,-1,-1},{0,0,-1,-1},{29,73,53,97},{0,0,-1,-1},{0,0,-1,-1},
		{0,0,-1,-1},{0,0,-1,-1},{24,103,48,127},{0,0,-1,-1},
		{108,273,125,290},{53,258,70,275},{6,6,42,66},{149,183,185,243},
		{10,178,46,238},{49,50,66,67},{147,248,164,265},{10,243,27,260},
		{168,248,185,265},{31,243,48,260},{151,82,175,106},{151,134,175,150},
		{177,108,193,132},{133,108,149,132},{148,161,165,178},{27,156,44,173}
	};
	if(position < 0 || position >= 30)
		return QRect();
	const int* coordinates = m_Male ? male[position] : female[position];
	return QRect(QPoint(coordinates[0], coordinates[1]), QPoint(coordinates[2], coordinates[3]));
}

void AuLegacyInventoryWidget::SetGear(const AuGear& gear, bool male)
{
	m_Gear = gear;
	m_Male = male;
	if(!m_Gear.ItemAt(m_Selection))
		m_Selection = AuGearPos();
	const AuItem* container = m_Gear.ItemAt(m_Container);
	if(!container || !container->Content())
		m_Container = AuGearPos();
	Rebuild();
	update();
}

void AuLegacyInventoryWidget::Rebuild()
{
	m_Regions.clear();
	m_Grid = QRect();
	QList<AuGearPos> displayed;
	for(int index = 0; index < 30; ++index)
	{
		QRect rectangle = BodyRectangle(index);
		if(!rectangle.isValid())
			continue;
		Region region;
		region.rectangle = rectangle;
		region.position = AuLegacyInventory::ItemAtBodyPosition(m_Gear, INVENTORYPOS(index));
		region.bodyPosition = index;
		region.overflow = false;
		m_Regions.append(region);
		if(region.position.IsValid())
			displayed.append(region.position);
	}
	int bottom = 318;
	int width = 202;
	// Items from an older save that exceed the recovered geometry remain usable.
	// Also expose equipment whose original body subposition has no visible box.
	for(AuGear::const_iterator it = m_Gear.cbegin(); it != m_Gear.cend(); ++it)
	{
		AuGearPos position(*it);
		if(displayed.contains(position))
			continue;
		Region region;
		region.rectangle = QRect(4, bottom, 198, 28);
		region.position = position;
		region.bodyPosition = -1;
		region.overflow = true;
		m_Regions.append(region);
		bottom += 30;
	}
	const AuItem* container = m_Gear.ItemAt(m_Container);
	if(container && container->Content())
	{
		const QSize capacity = AuLegacyInventory::ContainerSize(*container);
		if(capacity.width() > 0 && capacity.height() > 0)
		{
			m_Grid = QRect(QPoint(4, bottom + 2), QSize(capacity.width() * CellPixels, capacity.height() * CellPixels));
			width = qMax(width, m_Grid.right() + 5);
			bottom = m_Grid.bottom() + 5;
		}
		const AuItemContent& contents = *container->Content();
		for(AuItemContent::const_iterator it = contents.cbegin(); it != contents.cend(); ++it)
		{
			Region region;
			region.position = AuGearPos(m_Container, it->first);
			region.bodyPosition = -1;
			const QPoint cell = contents.LegacyPosition(it->first);
			const QSize footprint = it->second->PackSize();
			region.overflow = cell.x() < 0 || cell.y() < 0 || !m_Grid.isValid()
				|| cell.x() + footprint.width() > capacity.width() || cell.y() + footprint.height() > capacity.height();
			if(region.overflow)
			{
				region.rectangle = QRect(4, bottom, 198, 28);
				bottom += 30;
			}
			else
				region.rectangle = QRect(m_Grid.topLeft() + QPoint(cell.x() * CellPixels, cell.y() * CellPixels),
					QSize(footprint.width() * CellPixels, footprint.height() * CellPixels));
			m_Regions.append(region);
		}
	}
	// Reserve the largest historical container even with no pack selected.
	QSize crate(12, 20);
	if(g_Au.IsLegacyGeneration() && g_Au.IsCommodityAvailable(CID_Crate))
		crate = g_Au.Commodity(CID_Crate).m_capacity;
	m_LogicalSize = QSize(qMax(width, crate.width() * CellPixels + 8),
		qMax(bottom, 318 + crate.height() * CellPixels + 6));
	setMinimumSize(m_LogicalSize);
	emit CanvasMinimumChanged();
}

AuGearPos AuLegacyInventoryWidget::ItemAt(const QPoint& point) const
{
	for(int index = 0; index < m_Regions.size(); ++index)
		if(m_Regions[index].rectangle.contains(point))
			return m_Regions[index].position;
	return AuGearPos();
}

QPoint AuLegacyInventoryWidget::ContainerDropCell(const QPoint& point) const
{
	const QPoint offset = point - m_Grid.topLeft();
	QPoint cell(offset.x() / CellPixels, offset.y() / CellPixels);
	const AuGearPos target = ItemAt(point);
	if(!target.IsValid() || target == m_PressedItem)
		cell -= m_DragOffset;
	return cell;
}

void AuLegacyInventoryWidget::Select(const AuGearPos& position)
{
	m_Selection = position;
	const AuItem* item = m_Gear.ItemAt(position);
	if(item && item->Content())
		m_Container = position;
	emit SelectionChanged(position);
	Rebuild();
	update();
}

void AuLegacyInventoryWidget::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	painter.fillRect(rect(), Qt::black);
	painter.drawPixmap(0, 0, QPixmap(Resource(m_Male ? "cs_InventoryBGMale.png" : "cs_InventoryBGFemale.png")));
	painter.drawPixmap(m_Male ? QPoint(26, 204) : QPoint(155, 113), QPixmap(Resource("cs_InventoryBelt.png")));
	painter.setPen(QColor(0, 131, 227));
	if(m_Grid.isValid())
	{
		// FUN_00554a70: cyan bevel around purple bevelled 16-pixel cells.
		const QRect border = m_Grid.adjusted(-2, -2, 2, 2);
		painter.setPen(QColor(0, 192, 192));
		painter.drawLine(border.topLeft(), border.topRight());
		painter.drawLine(border.topLeft(), border.bottomLeft());
		painter.setPen(QColor(0, 255, 255));
		painter.drawLine(border.bottomLeft(), border.bottomRight());
		painter.drawLine(border.topRight(), border.bottomRight());
		for(int x = m_Grid.left(); x <= m_Grid.right(); x += CellPixels)
		{
			painter.setPen(QColor(62, 22, 79));
			painter.drawLine(x, m_Grid.top(), x, m_Grid.bottom());
			painter.setPen(QColor(125, 45, 159));
			painter.drawLine(x + CellPixels - 1, m_Grid.top(), x + CellPixels - 1, m_Grid.bottom());
		}
		for(int y = m_Grid.top(); y <= m_Grid.bottom(); y += CellPixels)
		{
			painter.setPen(QColor(62, 22, 79));
			painter.drawLine(m_Grid.left(), y, m_Grid.right(), y);
			painter.setPen(QColor(125, 45, 159));
			painter.drawLine(m_Grid.left(), y + CellPixels - 1, m_Grid.right(), y + CellPixels - 1);
		}
		painter.setPen(QColor(0, 131, 227));
	}
	for(int index = 0; index < m_Regions.size(); ++index)
	{
		const Region& region = m_Regions[index];
		if(region.bodyPosition >= 0)
		{
			const char* imageName = BodyImages[region.bodyPosition];
			if(region.bodyPosition == IP_HandRight && !m_Male)
				imageName = "cs_GridHandRight.png";
			painter.drawPixmap(region.rectangle, QPixmap(Resource(imageName)));
		}
		const AuItem* item = m_Gear.ItemAt(region.position);
		if(item)
		{
			QRect imageRectangle = region.rectangle.adjusted(1, 1, -1, -1);
			if(region.bodyPosition < 0 && !region.overflow)
				painter.fillRect(imageRectangle, Qt::black);
			if(region.overflow)
			{
				imageRectangle.setWidth(26);
				painter.drawText(region.rectangle.adjusted(32, 0, -2, 0), Qt::AlignVCenter,
					fontMetrics().elidedText(item->ListName(), Qt::ElideRight, 162));
			}
			painter.drawPixmap(imageRectangle, QPixmap(item->BitmapName()));
			if(region.position == m_Selection)
			{
				painter.setPen(QColor(174, 215, 255));
				painter.drawRect(region.rectangle.adjusted(0, 0, -1, -1));
				painter.setPen(QColor(0, 131, 227));
			}
		}
	}
	if(m_Dragging && m_Grid.contains(m_DragPoint))
	{
		const AuItem* dragged = m_Gear.ItemAt(m_PressedItem);
		if(dragged)
		{
			const QPoint cell = ContainerDropCell(m_DragPoint);
			const QSize footprint = dragged->PackSize();
			const QPoint origin = m_Grid.topLeft() + QPoint(cell.x() * CellPixels, cell.y() * CellPixels);
			const QRect previewRect = QRect(origin, QSize(footprint.width() * CellPixels, footprint.height() * CellPixels));
			bool isBlocked = !m_Grid.contains(previewRect);
			painter.save();
			painter.setClipRect(m_Grid);
			QColor rectColor = isBlocked ? QColor(Qt::red) : QColor(Qt::cyan);
			painter.setPen(rectColor);
			rectColor.setAlpha(48);
			painter.setBrush(rectColor);
			for(int y = 0; y < footprint.height(); ++y)
				for(int x = 0; x < footprint.width(); ++x)
					painter.drawRect(QRect(origin + QPoint(x * CellPixels, y * CellPixels),
						QSize(CellPixels, CellPixels)).adjusted(0, 0, -1, -1));
			painter.restore();
		}
	}
}

void AuLegacyInventoryWidget::mousePressEvent(QMouseEvent* event)
{
	m_PressPoint = event->pos();
	m_DragPoint = event->pos();
	const QPoint point = event->pos();
	m_PressedItem = ItemAt(point);
	m_Dragging = false;
	m_DragOffset = QPoint();
	// Keep the open container stable until release so its cells remain targets
	// while dragging a nested container.
	m_Selection = m_PressedItem;
	emit SelectionChanged(m_Selection);
	if(m_Grid.contains(point) && m_PressedItem.IsValid())
	{
		for(int index = 0; index < m_Regions.size(); ++index)
			if(m_Regions[index].position == m_PressedItem && !m_Regions[index].overflow)
			{
				QPoint offset = point - m_Regions[index].rectangle.topLeft();
				m_DragOffset = QPoint(offset.x() / CellPixels, offset.y() / CellPixels);
				break;
			}
	}
	update();
}

void AuLegacyInventoryWidget::mouseMoveEvent(QMouseEvent* event)
{
	m_DragPoint = event->pos();
	if((event->buttons() & Qt::LeftButton) && m_PressedItem.IsValid()
		&& (event->pos() - m_PressPoint).manhattanLength() >= QApplication::startDragDistance())
	{
		m_Dragging = true;
		setCursor(Qt::ClosedHandCursor);
	}
	if(m_Dragging)
		update();
}

void AuLegacyInventoryWidget::mouseReleaseEvent(QMouseEvent* event)
{
	unsetCursor();
	const QPoint point = event->pos();
	if(event->button() == Qt::LeftButton && m_Dragging)
	{
		if(!visibleRegion().contains(event->pos()))
			emit DropRequested();
		else if(m_Grid.contains(point))
		{
			emit MoveRequested(m_PressedItem, IP_NumInventoryPos, m_Container,
				ContainerDropCell(point));
		}
		else
			for(int index = 0; index < m_Regions.size(); ++index)
				if(m_Regions[index].bodyPosition >= 0 && m_Regions[index].rectangle.contains(point))
				{
					emit MoveRequested(m_PressedItem, quint8(m_Regions[index].bodyPosition), AuGearPos(), QPoint(-1, -1));
					break;
				}
	}
	else
	{
		Select(ItemAt(point));
		if(event->button() == Qt::RightButton)
			emit ContextRequested();
	}
	m_Dragging = false;
	m_PressedItem = AuGearPos();
	update();
}

void AuLegacyInventoryWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
	Select(ItemAt(event->pos()));
	if(event->button() == Qt::LeftButton)
		emit UseRequested();
}

bool AuLegacyInventoryWidget::event(QEvent* event)
{
	if(event->type() == QEvent::ToolTip)
	{
		QHelpEvent* help = static_cast<QHelpEvent*>(event);
		const AuItem* item = m_Gear.ItemAt(ItemAt(help->pos()));
		if(item)
			QToolTip::showText(help->globalPos(), item->HtmlToolTip(), this);
		else
			QToolTip::hideText();
		return true;
	}
	return QWidget::event(event);
}
