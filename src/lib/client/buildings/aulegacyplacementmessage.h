#ifndef AuLegacyPlacementMessage_H
#define AuLegacyPlacementMessage_H

#include "auclientlib.h"
#include <QElapsedTimer>

class AuAtomRenderInfo;

class AUCLIENT_DLL AuLegacyPlacementMessage
{
public:
	void Show(const QString& message);
	void Render(AuAtomRenderInfo& info) const;

private:
	QString m_Text;
	QElapsedTimer m_Time;
};

#endif
