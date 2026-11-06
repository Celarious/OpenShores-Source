#include "autransitiontrace.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QStringList>
#include <atomic>

namespace
{
struct TraceOperation
{
	const char* stage = nullptr;
	QString detail;
	quintptr thread = 0;
	qint64 started = 0, reported = 0;
	int minimumMs = 20;
};

qint64 TraceNow()
{
	QElapsedTimer timer;
	timer.start();
	return timer.msecsSinceReference();
}

QString TraceLine(const char* kind, quint64 id, const TraceOperation& operation, qint64 now)
{
	QString detail = operation.detail;
	detail.replace('\n', ' '); detail.replace('\r', ' ');
	return QString("t_ms=%1 thread=%2 %3 id=%4 stage=%5 elapsed_ms=%6 %7\n")
		.arg(now).arg(qulonglong(operation.thread), 0, 16).arg(QString::fromLatin1(kind))
		.arg(id).arg(QString::fromLatin1(operation.stage)).arg(now - operation.started).arg(detail);
}

class TransitionTraceWriter : public QThread
{
public:
	TransitionTraceWriter()
	{
		setObjectName(QStringLiteral("TransitionTrace"));
		start();
	}
	~TransitionTraceWriter() override
	{
		m_Stop.store(true);
		wait(); // Only at process shutdown, never on a scene transition.
	}

	quint64 Begin(const char* stage, const QString& detail, int minimumMs)
	{
		TraceOperation operation;
		operation.stage = stage; operation.detail = detail;
		operation.thread = quintptr(QThread::currentThreadId());
		operation.started = TraceNow(); operation.minimumMs = minimumMs;
		QMutexLocker lock(&m_Lock);
		const quint64 id = ++m_NextId;
		m_Active.insert(id, operation);
		return id;
	}
	void Detail(quint64 id, const QString& detail)
	{
		QMutexLocker lock(&m_Lock);
		auto found = m_Active.find(id);
		if(found != m_Active.end()) found->detail = detail;
	}
	void End(quint64 id)
	{
		const qint64 now = TraceNow();
		QMutexLocker lock(&m_Lock);
		auto found = m_Active.find(id);
		if(found == m_Active.end()) return;
		if(now - found->started >= found->minimumMs || found->reported)
			Append(TraceLine("SLOW", id, *found, now));
		m_Active.erase(found);
	}
	void Event(const char* stage, const QString& detail)
	{
		TraceOperation operation;
		operation.stage = stage; operation.detail = detail;
		operation.thread = quintptr(QThread::currentThreadId());
		operation.started = TraceNow();
		QMutexLocker lock(&m_Lock);
		Append(TraceLine("EVENT", ++m_NextId, operation, operation.started));
	}

protected:
	void run() override
	{
		const QString path = QDir(QDir::tempPath()).filePath(QString("hazeron-transitions-%1.log")
			.arg(QCoreApplication::applicationPid()));
		QFile file(path);
		if(!file.open(QIODevice::WriteOnly | QIODevice::Append))
		{
			qWarning() << "Transition trace cannot open" << path << file.errorString();
			return;
		}
		qWarning() << "Transition trace:" << path;
		file.write(QString("\nSESSION utc=%1 pid=%2 slow_ms=20 watchdog_ms=250 repeat_ms=1000\n"
			"Nested durations overlap; do not add them. WAIT means still in the named operation.\n")
			.arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate))
			.arg(QCoreApplication::applicationPid()).toUtf8());
		for(;;)
		{
			QStringList lines;
			const qint64 now = TraceNow();
			const bool stopping = m_Stop.load();
			{
				QMutexLocker lock(&m_Lock);
				lines.swap(m_Lines);
				if(m_Dropped)
				{
					lines.append(QString("t_ms=%1 DROPPED events=%2\n").arg(now).arg(m_Dropped));
					m_Dropped = 0;
				}
				for(auto it = m_Active.begin(); it != m_Active.end(); ++it)
					if(now - it->started >= 250 && (!it->reported || now - it->reported >= 1000))
					{
						lines.append(TraceLine("WAIT", it.key(), *it, now));
						it->reported = now;
					}
			}
			// Never acquire game locks or write files while holding the trace mutex.
			for(const QString& line : lines) file.write(line.toUtf8());
			file.flush();
			if(stopping) break;
			msleep(100);
		}
	}

private:
	void Append(const QString& line)
	{
		if(m_Lines.size() < 4096) m_Lines.append(line);
		else ++m_Dropped;
	}
	QMutex m_Lock;
	QHash<quint64, TraceOperation> m_Active;
	QStringList m_Lines;
	quint64 m_NextId = 0, m_Dropped = 0;
	std::atomic<bool> m_Stop{false};
};

TransitionTraceWriter& TraceWriter()
{
	static TransitionTraceWriter writer;
	return writer;
}
}

AuTransitionTrace::AuTransitionTrace(const char* stage, const QString& detail, int minimumMs) : m_Id(0)
{
	m_Id = TraceWriter().Begin(stage, detail, minimumMs);
}

AuTransitionTrace::~AuTransitionTrace() { Finish(); }

void AuTransitionTrace::Finish()
{
	if(m_Id) { TraceWriter().End(m_Id); m_Id = 0; }
}

void AuTransitionTrace::SetDetail(const QString& detail)
{
	if(m_Id) TraceWriter().Detail(m_Id, detail);
}

void AuTransitionTrace::Event(const char* stage, const QString& detail)
{
	TraceWriter().Event(stage, detail);
}
