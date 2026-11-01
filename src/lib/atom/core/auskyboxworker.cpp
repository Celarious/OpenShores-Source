#include "auskyboxworker.h"
#include "augleffects.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QThread>
#include <exception>

void AuSkyBoxJob::Cancel(AuGLEffects& re)
{
	cancelled = true;
	std::lock_guard<std::mutex> lock(mutex);
	if(texture) re.glDeleteTextures(1, &texture);
	texture = 0;
}

namespace
{
void DeleteWorkerTextures(AuGLEffects& re)
{
	// AuGLTexture's destructor deliberately leaves GL names alive. These cache
	// objects belong only to this worker, even though their names are shared.
	const char* textureIds[] = {"StarGlow", "StarSpriteDot", "Ring"};
	for(const char* id : textureIds)
	{
		const QString textureId = QString::fromLatin1(id);
		if(AuGLTexture* texture = re.FindTexture(textureId))
		{
			const GLuint name = texture->GlName();
			re.glDeleteTextures(1, &name);
			re.DeleteTexture(STATIC_TEXTURE_ID, textureId);
		}
	}
}

class SkyThread : public QThread
{
	std::shared_ptr<AuSkyBoxJob> m_Job;
	QOpenGLContext* m_Context;
	QOffscreenSurface* m_Surface;
	AuSkyBoxTask m_Task;
	bool m_InitializeShaders;

public:
	SkyThread(const std::shared_ptr<AuSkyBoxJob>& job, QOpenGLContext* context,
		QOffscreenSurface* surface, const AuSkyBoxTask& task, bool initializeShaders)
		: m_Job(job), m_Context(context), m_Surface(surface), m_Task(task), m_InitializeShaders(initializeShaders)
	{
		m_Context->moveToThread(this);
		connect(this, &QThread::finished, m_Surface, &QObject::deleteLater);
		connect(this, &QThread::finished, this, &QObject::deleteLater);
	}

	void run() override
	{
		QElapsedTimer clock;
		clock.start();
		qint64 contextMs = 0, shaderMs = 0, generationMs = 0;
		QString error;
		bool published = false;
		if(!m_Job->cancelled && m_Context->makeCurrent(m_Surface))
		{
			contextMs = clock.elapsed();
			{
				AuGLEffects re;
				if(re.initializeOpenGLFunctions())
				{
					const bool shadersReady = !m_InitializeShaders || re.GLSL().InitializeSkyBox(re);
					shaderMs = clock.elapsed() - contextMs;
					GLuint texture = 0;
					if(shadersReady && !m_Job->cancelled)
					{
						re.glDisable(GL_DEPTH_TEST);
						re.glDisable(GL_CULL_FACE);
						re.glDisable(GL_SCISSOR_TEST);
						re.glActiveTexture(GL_TEXTURE0);
						try
						{
							texture = m_Task(re, m_Job->cancelled, error);
						}
						catch(const std::exception& failure)
						{
							error = QStringLiteral("Skybox generation failed: %1")
								.arg(QString::fromLocal8Bit(failure.what()));
						}
						catch(...)
						{
							error = QStringLiteral("Skybox generation failed.");
						}
					}
					else if(!m_Job->cancelled) error = QStringLiteral("Unable to initialize skybox shaders.");
					generationMs = clock.elapsed() - contextMs - shaderMs;
					DeleteWorkerTextures(re);
					// DeleteSkyBox deselects via AuGLSL's captured GL table, which
					// does not exist on a cache hit that never initialized shaders.
					if(m_InitializeShaders || re.GLSL().FarStarShader().IsLinked()
						|| re.GLSL().GalaxyStarShader().IsLinked() || re.GLSL().GalaxyCloudShader().IsLinked()
						|| re.GLSL().NebulaShader().IsLinked()) re.GLSL().DeleteSkyBox();
					// Retain ownership until cleanup and GPU completion are over.
					// Result, metadata and done become visible together; painting
					// never needs to finish or wait for the worker context.
					if(texture) re.glFinish();
					{
						std::lock_guard<std::mutex> lock(m_Job->mutex);
						if(texture && m_Job->cancelled) re.glDeleteTextures(1, &texture);
						else m_Job->texture = texture;
						m_Job->error = error;
						m_Job->done = true;
					}
					published = true;
				}
				else error = QStringLiteral("Unable to initialize skybox OpenGL functions.");
			}
			m_Context->doneCurrent();
		}
		else if(!m_Job->cancelled) error = QStringLiteral("Unable to activate the skybox context.");
		delete m_Context;
		if(qEnvironmentVariableIsSet("HAZERON_LEGACY_STARTUP_STATS"))
			qInfo() << "Skybox worker: context" << contextMs << "ms, shaders" << shaderMs
				<< "ms, generation" << generationMs << "ms, total" << clock.elapsed() << "ms";
		if(!published)
		{
			std::lock_guard<std::mutex> lock(m_Job->mutex);
			m_Job->error = error;
			m_Job->done = true;
		}
	}
};
}

std::shared_ptr<AuSkyBoxJob> AuStartSkyBoxJob(QOpenGLContext* shareContext,
	const AuSkyBoxTask& task, QString& error, bool initializeShaders)
{
	error.clear();
	if(!QCoreApplication::instance()
		|| QThread::currentThread() != QCoreApplication::instance()->thread()
		|| !shareContext || !shareContext->isValid() || !task)
	{
		error = QStringLiteral("Skybox jobs require a valid sharing context on the GUI thread.");
		return std::shared_ptr<AuSkyBoxJob>();
	}
	// Qt requires offscreen surface creation and destruction on the GUI thread.
	// Establish sharing before the caller's context can be destroyed.
	QOpenGLContext* context = new QOpenGLContext;
	context->setFormat(shareContext->format());
	context->setShareContext(shareContext);
	if(!QOpenGLContext::supportsThreadedOpenGL() || !context->create()
		|| !QOpenGLContext::areSharing(shareContext, context))
	{
		delete context;
		error = QStringLiteral("Unable to create a shared skybox context.");
		return std::shared_ptr<AuSkyBoxJob>();
	}
	QOffscreenSurface* surface = new QOffscreenSurface;
	surface->setFormat(context->format());
	surface->create();
	if(!surface->isValid())
	{
		delete surface;
		delete context;
		error = QStringLiteral("Unable to create the skybox surface.");
		return std::shared_ptr<AuSkyBoxJob>();
	}
	std::shared_ptr<AuSkyBoxJob> job(new AuSkyBoxJob);
	SkyThread* worker = new SkyThread(job, context, surface, task, initializeShaders);
	worker->start(QThread::LowPriority);
	return job;
}
