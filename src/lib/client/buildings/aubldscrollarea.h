#ifndef AuBldScrollArea_H
#define AuBldScrollArea_H

#include <QEvent>
#include <QLayout>
#include <QScrollArea>
#include <QScrollBar>

// F10 lists may scroll vertically, but a complete studio/process panel must
// fit in the viewport. Propagate this minimum to the containing dock layout.
class AuBldScrollArea : public QScrollArea
{
public:
	explicit AuBldScrollArea(QWidget* parent = nullptr) : QScrollArea(parent) {}

protected:
	bool eventFilter(QObject* watched, QEvent* event) override
	{
		const bool handled = QScrollArea::eventFilter(watched, event);
		if(watched == widget() && (event->type() == QEvent::LayoutRequest
			|| event->type() == QEvent::Show || event->type() == QEvent::FontChange
			|| event->type() == QEvent::StyleChange))
			UpdateMinimumSize();
		return handled;
	}

	bool event(QEvent* event) override
	{
		const bool handled = QScrollArea::event(event);
		if(event->type() == QEvent::Show || event->type() == QEvent::FontChange
			|| event->type() == QEvent::StyleChange)
			UpdateMinimumSize();
		return handled;
	}

private:
	void UpdateMinimumSize()
	{
		if(!widget() || !widget()->layout()) return;
		QLayout* contentLayout = widget()->layout();
		const QSize contentMinimum = contentLayout->minimumSize().expandedTo(widget()->minimumSize());
		int panelHeight = 0;
		for(int index = 0; index < contentLayout->count(); ++index)
		{
			QLayoutItem* item = contentLayout->itemAt(index);
			if(!item->isEmpty()) panelHeight = qMax(panelHeight, item->minimumSize().height());
		}
		const QMargins margins = contentLayout->contentsMargins();
		const QSize required = QScrollArea::minimumSizeHint().expandedTo(QSize(
			contentMinimum.width() + 2 * frameWidth() + verticalScrollBar()->sizeHint().width(),
			panelHeight + margins.top() + margins.bottom() + 2 * frameWidth()));
		if(minimumSize() != required) setMinimumSize(required);
	}
};

#endif // AuBldScrollArea_H
