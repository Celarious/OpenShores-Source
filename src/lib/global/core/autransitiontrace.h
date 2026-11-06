#ifndef AuTransitionTrace_H
#define AuTransitionTrace_H

#include "augloballib.h"
#include <QString>

// Temporary, always-on diagnostics. No file I/O is performed by measured threads.
class AUGLOBAL_DLL AuTransitionTrace
{
public:
	static void Event(const char* stage, const QString& detail);
	explicit AuTransitionTrace(const char* stage, const QString& detail = QString(), int minimumMs = 20);
	~AuTransitionTrace();
	void Finish();
	void SetDetail(const QString& detail);

private:
	AuTransitionTrace(const AuTransitionTrace&) = delete;
	AuTransitionTrace& operator=(const AuTransitionTrace&) = delete;
	quint64 m_Id;
};

#endif
