#include "aulegacyplacementmessage.h"
#include "auatomrenderinfo.h"
#include "augleffects.h"
#include <QOpenGLWidget>

const int PopupLifetime = 5000; // Time in ms, 5000 = popup vanishes after 5 seconds
const float FadeOutSpeed = 1000.f; // This means how fast before the text reaches 50% opacity, 2000.f = 2 seconds 

void AuLegacyPlacementMessage::Show(const QString& message)
{
	m_Text = message;
	m_Time.start();
}

void AuLegacyPlacementMessage::Render(AuAtomRenderInfo& info) const
{
	if(!m_Time.isValid() || m_Text.isEmpty() || !info.glw || !info.re) return;
	const qint64 elapsed = m_Time.elapsed();
	if(elapsed >= PopupLifetime) return;
	const float alpha = 1.f / (1.f + elapsed / FadeOutSpeed);
	// Follow the cursor for the original five-second lifetime. Place the
	// wrapped label above it so the error does not obscure the site pointer.
	const QPoint pixel = info.glw->mapFromGlobal(QCursor::pos()) - QPoint(0, 32);
	QFont font; font.setPointSize(10); font.setBold(true);
	auto& effects = *info.re;
	const bool blend = effects.glIsEnabled(GL_BLEND);
	GLint source, destination, sourceAlpha, destinationAlpha;
	effects.glGetIntegerv(GL_BLEND_SRC_RGB, &source); effects.glGetIntegerv(GL_BLEND_DST_RGB, &destination);
	effects.glGetIntegerv(GL_BLEND_SRC_ALPHA, &sourceAlpha); effects.glGetIntegerv(GL_BLEND_DST_ALPHA, &destinationAlpha);
	effects.glEnable(GL_BLEND); effects.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	QColor color; color.setRgbF(0, 0, 0, alpha);
	effects.RenderText(pixel.x(), pixel.y(), m_Text, font, color, info.glw->width(), info.glw->height(),
		Qt::AlignBottom | Qt::AlignHCenter | Qt::TextWordWrap, 200);
	color.setRgbF(.9, .35, .1, alpha);
	effects.RenderText(pixel.x() - 1, pixel.y() - 1, m_Text, font, color, info.glw->width(), info.glw->height(),
		Qt::AlignBottom | Qt::AlignHCenter | Qt::TextWordWrap, 200);
	effects.glBlendFuncSeparate(source, destination, sourceAlpha, destinationAlpha);
	if(!blend) effects.glDisable(GL_BLEND);
}
