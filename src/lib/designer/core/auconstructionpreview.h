#ifndef AuConstructionPreview_H
#define AuConstructionPreview_H

#include <QLabel>

#if defined(Q_OS_MACOS)
#include <QPainter>

// A passive preview: wheel events continue to the construction scroll area.
class AuConstructionPreview : public QLabel
{
public:
	explicit AuConstructionPreview(QWidget* parent = nullptr) : QLabel(parent)
	{
		setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		setMaximumHeight(256);
	}

	QSize sizeHint() const override
	{
		return pixmap(Qt::ReturnByValue).isNull() ? QSize(0, 0) : QSize(256, 192);
	}

	QSize minimumSizeHint() const override
	{
		return pixmap(Qt::ReturnByValue).isNull() ? QSize(0, 0) : QSize(0, 96);
	}

protected:
	void paintEvent(QPaintEvent*) override
	{
		const QPixmap image = pixmap(Qt::ReturnByValue);
		if(image.isNull())
			return;
		QRectF target(QPointF(0, 0), QSizeF(image.size()));
		target.setSize(target.size().scaled(contentsRect().size(), Qt::KeepAspectRatio));
		target.moveCenter(QRectF(contentsRect()).center());
		QPainter painter(this);
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
		painter.drawPixmap(target, image, image.rect());
	}
};
#endif

#endif
