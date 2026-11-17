#ifndef AuLegacyInventoryWidget_H
#define AuLegacyInventoryWidget_H

#include "auclientlib.h"
#include "augear.h"
#include <QRect>
#include <QWidget>

class AUCLIENT_DLL AuLegacyInventoryWidget : public QWidget
{
	Q_OBJECT
public:
	explicit AuLegacyInventoryWidget(QWidget* parent = 0);
	void SetGear(const AuGear& gear, bool male);

signals:
	void SelectionChanged(AuGearPos position);
	void UseRequested();
	void ContextRequested();
	void DropRequested();
	void MoveRequested(AuGearPos source, quint8 bodyPosition, AuGearPos containerPosition, QPoint cell);

protected:
	void paintEvent(QPaintEvent* event);
	void mousePressEvent(QMouseEvent* event);
	void mouseMoveEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent* event);
	void mouseDoubleClickEvent(QMouseEvent* event);
	bool event(QEvent* event);

private:
	struct Region
	{
		QRect rectangle;
		AuGearPos position;
		int bodyPosition;
		bool overflow;
	};
	AuGear m_Gear;
	AuGearPos m_Selection;
	AuGearPos m_Container;
	AuGearPos m_PressedItem;
	QPoint m_PressPoint;
	QPoint m_DragOffset;
	QList<Region> m_Regions;
	QRect m_Grid;
	bool m_Male;
	bool m_Dragging;

	void Rebuild();
	void Select(const AuGearPos& position);
	AuGearPos ItemAt(const QPoint& point) const;
	QRect BodyRectangle(int position) const;
};

#endif
