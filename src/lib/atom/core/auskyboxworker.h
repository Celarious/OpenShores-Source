#ifndef AUSKYBOXWORKER_H
#define AUSKYBOXWORKER_H

#include "auatom.h"
#include "augl.h"
#include <QString>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

class AuGLEffects;
class QOpenGLContext;

// Only completed, shared texture names leave the worker context. Call Cancel
// before abandoning a job; its GL deletion must use the same share group.
struct AUATOM_DLL AuSkyBoxJob
{
	std::atomic<bool> cancelled{false};
	std::mutex mutex;
	GLuint texture = 0;
	bool done = false;
	QString error;

	void Cancel(AuGLEffects& re);
};

// The task owns its intermediate GL objects and returns one completed cube, or
// zero on failure. No widget, view or live scene atom may be used by the task.
typedef std::function<GLuint(AuGLEffects&, const std::atomic<bool>&, QString&)> AuSkyBoxTask;

// Call on the GUI thread while shareContext is alive. A null result reports a
// context/surface failure so callers can use an incremental fallback.
// Cache readers can defer shader initialization until their task has a miss.
AUATOM_DLL std::shared_ptr<AuSkyBoxJob> AuStartSkyBoxJob(QOpenGLContext* shareContext,
	const AuSkyBoxTask& task, QString& error, bool initializeShaders = true);

#endif
