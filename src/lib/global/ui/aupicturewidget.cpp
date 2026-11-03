//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: aupicturewidget.cpp,v 1.8 2022/04/11 20:17:31 build Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "aupicturewidget.h"
//
#include <QPainter>
#include <QWheelEvent>

AuPictureWidget::AuPictureWidget(bool autoScale, QWidget* parent) : QWidget(parent)
{
	m_Auto = autoScale;
	m_BackgroundColor = QColor(0,0,0);
	m_LeftButtonMode = 0;
	m_Scale = 1;
#if defined(Q_OS_MACOS)
	m_FitToView = false;
#endif

	setAttribute(Qt::WA_OpaquePaintEvent, true);
	if(!m_Auto)
		setCursor(QCursor(Qt::OpenHandCursor));
}

AuPictureWidget::~AuPictureWidget()
{
}

void AuPictureWidget::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
}

QSize AuPictureWidget::sizeHint() const
{
	return QSize(256,256);
}

void AuPictureWidget::paintEvent(QPaintEvent* pe)
{
	QPainter p(this);

	// Fill background with a dark color.
	QBrush brBack(m_BackgroundColor);
	p.fillRect(rect(), brBack);

	// Draw picture, if we have one.
	if(!m_Image.isNull())
	{
		if(m_Auto)
		{
			p.setRenderHint(QPainter::SmoothPixmapTransform);
			p.drawImage(rect(), m_Image);
		}
		else
		{
			float scale = m_Scale;
#if defined(Q_OS_MACOS)
			scale = ImageScale();
#endif
			// Determine target rectangle for the image.
			QRectF rc(m_Image.rect());
			if(scale != 1)
			{
				// Scale the rectangle.
				rc.setWidth(rc.width() * scale);
				rc.setHeight(rc.height() * scale);
			}

			// Translate the rectangle so the middle pixel is in the middle of the window.
			float offx = width()/2.f - m_MiddlePixel.x() * scale;
			float offy = height()/2.f - m_MiddlePixel.y() * scale;
			rc = rc.translated(offx, offy);

			// Draw the image so it looks as nice as possible.
			p.setRenderHint(QPainter::SmoothPixmapTransform);
			p.drawImage(rc, m_Image);
		}
	}
	p.end();
	QWidget::paintEvent(pe);
}

void AuPictureWidget::mousePressEvent(QMouseEvent* e)
{
	QWidget::mousePressEvent(e);
	if(m_Auto)
		return;
	m_LeftButtonMode = 0;
	m_MouseDownPt = e->pos();
	if(e->button() == Qt::LeftButton)
	{
		setCursor(QCursor(Qt::ClosedHandCursor));
		m_LeftButtonMode = 1;
	}
}

void AuPictureWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
	QWidget::mouseDoubleClickEvent(e);
}

// NOTE: We only get this event while a button is held down.
void AuPictureWidget::mouseMoveEvent(QMouseEvent* e)
{
	QWidget::mouseMoveEvent(e);
	if(m_Auto)
		return;
	if(m_LeftButtonMode == 1)
	{
		float scale = m_Scale;
#if defined(Q_OS_MACOS)
		scale = ImageScale();
#endif
		float x = m_MiddlePixel.x() - (float)(e->pos().x() - m_MouseDownPt.x()) / scale;
		float y = m_MiddlePixel.y() - (float)(e->pos().y() - m_MouseDownPt.y()) / scale;
		m_MiddlePixel.setX(x);
		m_MiddlePixel.setY(y);
		m_MouseDownPt = e->pos();
		update();
	}
}

void AuPictureWidget::mouseReleaseEvent(QMouseEvent* e)
{
	if(!m_Auto)
	{
		setCursor(QCursor(Qt::OpenHandCursor));
		m_LeftButtonMode = 0;
	}
	QWidget::mouseReleaseEvent(e);
}

void AuPictureWidget::wheelEvent(QWheelEvent *e)
{
	if(m_Auto)
	{
		QWidget::wheelEvent(e);
		return;
	}

	float nuScale = m_Scale;
#if(QT_VERSION >= QT_VERSION_CHECK(5,14,0))
	if(e->angleDelta().y() > 0)
		nuScale += .5f;
	else if(e->angleDelta().y() < 0)
		nuScale -= .5f;
#else
	if(e->delta() > 0)
		nuScale += .5f;
	else if(e->delta() < 0)
		nuScale -= .5f;
#endif

	QWidget::wheelEvent(e);
	SetScale(nuScale);
	e->setAccepted(true);
}

void AuPictureWidget::SetScale(float scale)
{
	if(scale < 1 || scale > 10)
		return;
	m_Scale = scale;
#if defined(Q_OS_MACOS)
	if(m_FitToView && scale == 1)
		m_MiddlePixel = QPointF(m_Image.width()/2.f, m_Image.height()/2.f);
#endif
	update();
	emit ScaleChanged(scale);
}

void AuPictureWidget::setVisible(bool visible)
{
	QWidget::setVisible(visible);
}

void AuPictureWidget::SetImage(const QImage& image)
{
	m_Image = image;
	m_MiddlePixel = QPointF(m_Image.width()/2.f, m_Image.height()/2.f);
#if defined(Q_OS_MACOS)
	if(m_FitToView)
		m_Scale = 1;
#endif
	update();
}

#if defined(Q_OS_MACOS)
float AuPictureWidget::ImageScale() const
{
	if(!m_FitToView || m_Image.isNull())
		return m_Scale;
	return m_Scale * qMin(qMax(1, width()) / float(m_Image.width()),
		qMax(1, height()) / float(m_Image.height()));
}

void AuPictureWidget::SetFitToView(bool fit)
{
	m_FitToView = fit;
	m_Scale = 1;
	m_MiddlePixel = QPointF(m_Image.width()/2.f, m_Image.height()/2.f);
	update();
}
#endif

void AuPictureWidget::SetBackgroundColor(const QColor& bgcol)
{
	m_BackgroundColor = bgcol;
	update();
}

