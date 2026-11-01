#include "ausceneskybox.h"
#include "auskyboxworker.h"
#include "auNebula.h"
#include "augleffects.h"
#include "auglobal.h"
#include "dagalaxy.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QOpenGLContext>
#include <QPointer>
#include <QRunnable>
#include <QSaveFile>
#include <QThread>
#include <QThreadPool>
#include <QtEndian>
#include <atomic>
#include <exception>
#include <limits>

namespace
{
// Separate namespace from the discarded approximate cubemaps. Increment for
// any change to the original sample/shader interpretation or image format.
const quint16 CacheVersion = 1;
const quint32 CacheMagic = 0x534b5945; // SKYE
const quint64 GpuCacheBytes = 512ULL * 1024 * 1024;
const qint64 DiskCacheBytes = 1024LL * 1024 * 1024;

struct Description
{
	GALAXY galaxy;
	quint64 galaxyIdentity;
	AuPoint sector, viewer, centre;
	AuDice dice;
	int particles, stars, colour, seed, type, cloudTexture;
	float a, b, c, d, particleSize;
	double coreRadius, galaxyRadius;
	bool reverse, mipMap;
	QColor galaxyColour;
	unsigned int paneSize;
	QString cacheFile;
	QByteArray digest;
};

struct TextureCleanup
{
	AuGLEffects& re;
	GLuint name;
	explicit TextureCleanup(AuGLEffects& effects) : re(effects), name(0) {}
	~TextureCleanup() { if(name) re.glDeleteTextures(1, &name); }
	GLuint Release() { const GLuint result = name; name = 0; return result; }
};

struct GeometryCleanup
{
	AuGLEffects& re;
	std::shared_ptr<auNebula> sky;
	GeometryCleanup(AuGLEffects& effects, const std::shared_ptr<auNebula>& prepared) : re(effects), sky(prepared) {}
	~GeometryCleanup() { sky->DeleteDeviceObjects(re); }
};

std::shared_ptr<auNebula> PrepareCpu(const Description& d, const std::atomic<bool>& cancelled)
{
	if(cancelled.load()) return std::shared_ptr<auNebula>();
	std::shared_ptr<auNebula> sky(new auNebula(d.particles, d.stars, d.a, d.b, d.c, d.d,
		d.particleSize, d.colour, d.reverse, d.centre, d.seed, d.type, d.galaxyColour));
	sky->m_CoreRadPc = d.coreRadius;
	sky->m_GalaxyRadPc = d.galaxyRadius;
	sky->SetCloudTexture(d.cloudTexture);
	// This private galaxy loads its own immutable density image. Original
	// traversal, sector centre, RNG state, jitter and sample count are retained.
	DaGalaxy galaxy(d.galaxy);
	sky->PrepareCachedGalacticStarField(galaxy, d.sector, d.dice, d.galaxyIdentity);
	if(cancelled.load()) return std::shared_ptr<auNebula>();
	sky->PrepareCachedNebula();
	if(cancelled.load()) return std::shared_ptr<auNebula>();
	sky->PrepareSkyBoxGeometry();
	return sky;
}

bool ReadCube(AuGLEffects& re, const Description& d, GLuint& texture,
	QColor& cloudColour, const std::atomic<bool>& cancelled)
{
	if(d.cacheFile.isEmpty() || cancelled.load()) return false;
	QFile file(d.cacheFile);
	if(!file.open(QIODevice::ReadOnly)) return false;
	QDataStream in(&file);
	in.setVersion(QDataStream::Qt_4_1);
	quint32 magic = 0, paneSize = 0;
	quint16 version = 0;
	bool mipMap = false;
	QColor colour;
	in >> magic >> version >> paneSize >> mipMap >> colour;
	QByteArray digest(32, '\0');
	if(in.readRawData(digest.data(), digest.size()) != digest.size()
		|| in.status() != QDataStream::Ok || magic != CacheMagic || version != CacheVersion
		|| paneSize != d.paneSize || mipMap != d.mipMap || digest != d.digest) return false;
	const quint64 bytes = quint64(paneSize) * paneSize * 4;
	if(!bytes || bytes > quint64(std::numeric_limits<int>::max() - 65536)) return false;
	TextureCleanup cube(re);
	re.glGenTextures(1, &cube.name);
	if(!cube.name) return false;
	re.glBindTexture(GL_TEXTURE_CUBE_MAP, cube.name);
	re.glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	re.glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	re.glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	re.glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	re.glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, mipMap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
	bool valid = true;
	for(int face = 0; face < 6 && valid; ++face)
	{
		quint32 compressedSize = 0;
		in >> compressedSize;
		valid = !cancelled.load() && in.status() == QDataStream::Ok && compressedSize >= 4
			&& compressedSize <= bytes + bytes / 100 + 65536
			&& compressedSize <= quint64(file.bytesAvailable());
		if(!valid) break;
		const QByteArray compressed = file.read(compressedSize);
		valid = compressed.size() == int(compressedSize)
			&& qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(compressed.constData())) == bytes;
		if(!valid) break;
		const QByteArray pixels = qUncompress(compressed);
		valid = quint64(pixels.size()) == bytes;
		if(valid)
		{
			re.glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA8,
				paneSize, paneSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			const int rowsPerUpload = qMax(1, int(4U * 1024U * 1024U / (paneSize * 4U)));
			for(int row = 0; row < int(paneSize); row += rowsPerUpload)
			{
				if(cancelled.load()) { valid = false; break; }
				const int rows = qMin(rowsPerUpload, int(paneSize) - row);
				re.glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, 0, row,
					paneSize, rows, GL_RGBA, GL_UNSIGNED_BYTE, pixels.constData() + row * paneSize * 4);
				QThread::yieldCurrentThread();
			}
		}
		QThread::yieldCurrentThread();
	}
	if(valid && mipMap) re.glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	valid = valid && file.atEnd() && !cancelled.load() && re.glGetError() == GL_NO_ERROR;
	if(!valid) return false;
	texture = cube.Release();
	cloudColour = colour;
	return true;
}

void WriteCube(AuGLEffects& re, const Description& d, GLuint texture,
	const QColor& cloudColour, const std::atomic<bool>& cancelled)
{
	if(d.cacheFile.isEmpty() || cancelled.load()) return;
	QDir folder = QFileInfo(d.cacheFile).absoluteDir();
	if(!folder.mkpath(QStringLiteral("."))) return;
	QSaveFile file(d.cacheFile);
	if(!file.open(QIODevice::WriteOnly)) return;
	QDataStream out(&file);
	out.setVersion(QDataStream::Qt_4_1);
	out << CacheMagic << CacheVersion << quint32(d.paneSize) << d.mipMap << cloudColour;
	if(out.writeRawData(d.digest.constData(), d.digest.size()) != d.digest.size()) return;
	const quint64 bytes = quint64(d.paneSize) * d.paneSize * 4;
	if(!bytes || bytes > quint64(std::numeric_limits<int>::max() - 65536)) return;
	QByteArray pixels(int(bytes), '\0');
	re.glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
	for(int face = 0; face < 6; ++face)
	{
		if(cancelled.load()) return;
		re.glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		if(re.glGetError() != GL_NO_ERROR) return;
		const QByteArray compressed = qCompress(pixels, 1);
		out << quint32(compressed.size());
		if(file.write(compressed) != compressed.size() || out.status() != QDataStream::Ok) return;
		QThread::yieldCurrentThread();
	}
	if(cancelled.load() || !file.commit()) return;
	const QFileInfoList files = folder.entryInfoList(QStringList(QStringLiteral("SceneSkyExact-v*-*.bin")),
		QDir::Files, QDir::Time);
	qint64 retained = 0;
	int count = 0;
	for(const QFileInfo& item : files)
	{
		retained += item.size();
		if((++count > 8 || retained > DiskCacheBytes) && item.absoluteFilePath() != file.fileName())
			QFile::remove(item.absoluteFilePath());
	}
}
}

class AuSceneSkyBox
{
public:
	Description description;
	GLuint texture = 0;
	QColor cloudColour;
	quint64 used = 0;
	std::shared_ptr<AuSkyBoxJob> job;
	std::atomic<bool> cancelled{false};
	std::atomic<bool> cpuReady{false};
	std::shared_ptr<auNebula> fallback;
	int nextFace = 0;
	bool started = false;
	bool fallbackStarted = false;
	bool failed = false;
	QString workerError;
};

namespace
{
class CpuTask : public QRunnable
{
	std::shared_ptr<AuSceneSkyBox> m_Entry;
public:
	explicit CpuTask(const std::shared_ptr<AuSceneSkyBox>& entry) : m_Entry(entry) {}
	void run() override
	{
		try
		{
			m_Entry->fallback = PrepareCpu(m_Entry->description, m_Entry->cancelled);
		}
		catch(const std::exception& error)
		{
			qWarning() << "Scene skybox CPU preparation failed:" << error.what();
		}
		catch(...)
		{
			qWarning() << "Scene skybox CPU preparation failed.";
		}
		m_Entry->cpuReady.store(true, std::memory_order_release);
	}
};

void StartFallback(const std::shared_ptr<AuSceneSkyBox>& entry, const QString& error)
{
	qWarning().noquote() << "Scene skybox uses incremental GL fallback:" << error;
	entry->fallbackStarted = true;
	QThreadPool::globalInstance()->start(new CpuTask(entry), -1);
}

void DeleteFallback(AuSceneSkyBox& entry, AuGLEffects* re)
{
	if(entry.cpuReady.load(std::memory_order_acquire) && entry.fallback)
	{
		if(re) entry.fallback->DeleteDeviceObjects(*re);
		entry.fallback.reset();
	}
}

void Harvest(AuSceneSkyBox& entry)
{
	if(!entry.job) return;
	const std::shared_ptr<AuSkyBoxJob> job = entry.job;
	std::unique_lock<std::mutex> lock(job->mutex, std::try_to_lock);
	if(!lock.owns_lock() || !job->done) return;
	entry.texture = job->texture;
	job->texture = 0;
	const bool cancelled = job->cancelled.load();
	entry.workerError = job->error;
	lock.unlock();
	entry.job.reset();
	if(cancelled) entry.started = false;
}
}

struct AuSceneSkyBoxCache::State
{
	bool diskCache;
	QPointer<QOpenGLContext> context;
	QMap<QByteArray, std::shared_ptr<AuSceneSkyBox>> entries;
	quint64 clock = 0;
	explicit State(bool enabled) : diskCache(enabled) {}
};

AuSceneSkyBoxCache::AuSceneSkyBoxCache(bool diskCache) : m_State(new State(diskCache)) {}
AuSceneSkyBoxCache::~AuSceneSkyBoxCache() { Clear(nullptr); }

void AuSceneSkyBoxCache::Clear(AuGLEffects* re)
{
	// VAOs are context-local; only texture names can be deleted from another
	// context in the same share group. Normal teardown uses the owning context.
	const bool ownerCurrent = re && m_State->context && m_State->context == QOpenGLContext::currentContext();
	for(auto it = m_State->entries.begin(); it != m_State->entries.end(); ++it)
	{
		AuSceneSkyBox& entry = *it.value();
		entry.cancelled = true;
		if(entry.job)
		{
			if(re) entry.job->Cancel(*re);
			else entry.job->cancelled = true;
		}
		if(entry.texture && re) re->glDeleteTextures(1, &entry.texture);
		entry.texture = 0;
		DeleteFallback(entry, ownerCurrent ? re : nullptr);
	}
	m_State->entries.clear();
	m_State->context.clear();
}

bool AuSceneSkyBoxCache::Prepare(AuGLEffects& re, const DaGalaxy& galaxy, const AuId& systemId,
	const AuPoint& viewerInSectors, auNebula& sky, std::shared_ptr<AuSceneSkyBox>& handle)
{
	QOpenGLContext* context = QOpenGLContext::currentContext();
	if(!context) return false;
	if(m_State->context != context)
	{
		const bool sharing = m_State->context && QOpenGLContext::areSharing(m_State->context, context);
		Clear(sharing ? &re : nullptr);
	}
	m_State->context = context;
	QByteArray signature;
	QDataStream out(&signature, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_4_1);
	out << CacheVersion;
	AuWriteGameGenerationIdentity(out, g_Au.GenerationIdentity());
	out << quint32(galaxy.Galaxy()) << galaxy.Id() << galaxy.GenSeed();
	const QByteArray galaxyDigest = QCryptographicHash::hash(signature, QCryptographicHash::Sha256);
	Description d;
	d.galaxy = galaxy.Galaxy();
	d.galaxyIdentity = qFromBigEndian<quint64>(reinterpret_cast<const uchar*>(galaxyDigest.constData()));
	d.sector = sky.m_GalacticSector; d.dice = sky.m_GalacticDice;
	d.viewer = viewerInSectors; d.centre = sky.m_Center;
	d.particles = sky.m_iParticles; d.stars = sky.m_iStars;
	d.colour = sky.m_iColor; d.reverse = sky.m_bReverseColor;
	d.seed = sky.m_Seed; d.type = sky.m_Type; d.cloudTexture = sky.CloudTexture();
	d.a = sky.m_fA; d.b = sky.m_fB; d.c = sky.m_fC; d.d = sky.m_fD;
	d.particleSize = sky.m_fParticleSize;
	d.coreRadius = sky.m_CoreRadPc; d.galaxyRadius = sky.m_GalaxyRadPc;
	d.galaxyColour = sky.m_GalaxyColor;
	d.paneSize = g_Au.SkyBoxPaneSize(); d.mipMap = g_Au.SkyBoxMipMap();
	AuDice signatureDice(d.dice);
	out << systemId << d.viewer << d.sector << signatureDice.Random() << d.centre
		<< d.particles << d.stars << d.colour << d.reverse << d.seed << d.type << d.cloudTexture
		<< d.a << d.b << d.c << d.d << d.particleSize << d.coreRadius << d.galaxyRadius
		<< d.galaxyColour << quint32(d.paneSize) << d.mipMap;
	d.digest = QCryptographicHash::hash(signature, QCryptographicHash::Sha256);
	if(m_State->diskCache) d.cacheFile = QDir(g_Au.CacheGame()).absoluteFilePath(
		QStringLiteral("SceneSkyExact-v%1-%2.bin").arg(CacheVersion).arg(QString::fromLatin1(d.digest.toHex())));
	auto found = m_State->entries.find(d.digest);
	if(found == m_State->entries.end())
	{
		std::shared_ptr<AuSceneSkyBox> entry(new AuSceneSkyBox);
		entry->description = d;
		found = m_State->entries.insert(d.digest, entry);
	}
	const std::shared_ptr<AuSceneSkyBox> entry = found.value();
	handle = entry;
	entry->used = ++m_State->clock;
	sky.m_img = 0;
	bool busy = false;
	for(auto it = m_State->entries.begin(); it != m_State->entries.end(); ++it)
	{
		const std::shared_ptr<AuSceneSkyBox>& other = it.value();
		Harvest(*other);
		if(other != entry && !other->texture)
		{
			other->cancelled = true;
			if(other->job) other->job->cancelled = true;
			DeleteFallback(*other, &re);
		}
		if(other != entry && (other->job || (other->fallbackStarted && !other->cpuReady.load(std::memory_order_acquire))))
			busy = true;
	}
	if(entry->cancelled && !entry->job && (!entry->fallbackStarted || entry->cpuReady.load(std::memory_order_acquire)))
	{
		DeleteFallback(*entry, &re);
		entry->cancelled = false;
		entry->cpuReady = false;
		entry->fallbackStarted = false;
		entry->started = false;
		entry->nextFace = 0;
		entry->failed = false;
	}
	if(!entry->started && !entry->texture && !busy)
	{
		entry->started = true;
		QString error;
		entry->job = AuStartSkyBoxJob(context, [entry](AuGLEffects& worker, const std::atomic<bool>& cancelled, QString& error) {
			QElapsedTimer clock;
			clock.start();
			GLuint texture = 0;
			if(ReadCube(worker, entry->description, texture, entry->cloudColour, cancelled))
			{
				if(qEnvironmentVariableIsSet("HAZERON_LEGACY_STARTUP_STATS"))
					qInfo() << "Scene skybox: cubemap cache hit," << clock.elapsed() << "ms";
				return texture;
			}
			clock.restart();
			if(!worker.GLSL().InitializeSkyBox(worker))
			{
				error = QStringLiteral("Unable to initialize scene skybox shaders.");
				return GLuint(0);
			}
			const qint64 shaderMs = clock.restart();
			std::shared_ptr<auNebula> prepared = PrepareCpu(entry->description, cancelled);
			if(!prepared) return GLuint(0);
			GeometryCleanup cleanup(worker, prepared);
			const qint64 cpuMs = clock.restart();
			prepared->SetSkyBoxWorkSliced(true);
			while(!cancelled.load() && !prepared->UploadSkyBoxGeometry(worker, true)) QThread::yieldCurrentThread();
			const qint64 uploadMs = clock.restart();
			int face = 0;
			for(; face < 6 && !cancelled.load(); ++face)
			{
				if(!prepared->CreateSkyBoxFace(worker, entry->description.viewer,
					entry->description.paneSize, entry->description.mipMap, face)
					|| worker.glGetError() != GL_NO_ERROR) break;
				QThread::yieldCurrentThread();
			}
			const qint64 facesMs = clock.restart();
			if(face == 6 && !cancelled.load())
			{
				entry->cloudColour = prepared->CloudColor();
				texture = prepared->m_img;
				// Cache storage is best effort; retain a successfully rendered cube
				// even when allocating its compression buffer fails.
				try { WriteCube(worker, entry->description, texture, entry->cloudColour, cancelled); }
				catch(...) { qWarning() << "Unable to store the completed scene skybox in the disk cache."; }
			}
			else if(!cancelled.load()) error = QStringLiteral("Unable to render a scene skybox face.");
			if(qEnvironmentVariableIsSet("HAZERON_LEGACY_STARTUP_STATS"))
				qInfo() << "Scene skybox: shaders" << shaderMs << "ms, CPU" << cpuMs
					<< "ms, upload" << uploadMs << "ms, faces" << facesMs << "ms, cache store" << clock.elapsed()
					<< "ms, original samples" << prepared->m_vStars.size();
			if(texture) prepared->m_img = 0;
			return texture;
		}, error, false);
		if(!entry->job) StartFallback(entry, error);
	}
	if(entry->started && !entry->job && !entry->texture && !entry->fallbackStarted && !entry->cancelled)
		StartFallback(entry, entry->workerError);
	if(entry->fallbackStarted && entry->cpuReady.load(std::memory_order_acquire)
		&& !entry->texture && !entry->failed && !entry->cancelled)
	{
		if(!entry->fallback) entry->failed = true;
		else if(entry->fallback->UploadSkyBoxGeometry(re, true))
		{
			GLint viewport[4];
			re.glGetIntegerv(GL_VIEWPORT, viewport);
			const bool ok = entry->fallback->CreateSkyBoxFace(re, d.viewer, d.paneSize, d.mipMap, entry->nextFace)
				&& re.glGetError() == GL_NO_ERROR;
			re.glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
			if(!ok)
			{
				qWarning() << "Unable to render a scene skybox face in the incremental fallback.";
				entry->failed = true;
				DeleteFallback(*entry, &re);
			}
			else if(++entry->nextFace == 6)
			{
				entry->cloudColour = entry->fallback->CloudColor();
				entry->texture = entry->fallback->m_img;
				entry->fallback->m_img = 0;
				DeleteFallback(*entry, &re);
			}
		}
	}
	if(!entry->texture) return entry->failed;
	sky.m_img = entry->texture;
	sky.m_CloudColor = entry->cloudColour;
	// Evict only unborrowed completed textures; sectors retain their handles.
	quint64 bytes = 0;
	for(auto it = m_State->entries.begin(); it != m_State->entries.end(); ++it)
		if(it.value()->texture) bytes += quint64(it.value()->description.paneSize) * it.value()->description.paneSize * 24
			* (it.value()->description.mipMap ? 4 : 3) / 3;
	while(bytes > GpuCacheBytes || m_State->entries.size() > 8)
	{
		auto oldest = m_State->entries.end();
		for(auto it = m_State->entries.begin(); it != m_State->entries.end(); ++it)
			if(it.value().use_count() == 1 && !it.value()->job
				&& (!it.value()->fallbackStarted || it.value()->cpuReady.load(std::memory_order_acquire))
				&& (oldest == m_State->entries.end() || it.value()->used < oldest.value()->used)) oldest = it;
		if(oldest == m_State->entries.end()) break;
		AuSceneSkyBox& removed = *oldest.value();
		if(removed.texture)
		{
			bytes -= quint64(removed.description.paneSize) * removed.description.paneSize * 24
				* (removed.description.mipMap ? 4 : 3) / 3;
			re.glDeleteTextures(1, &removed.texture);
		}
		DeleteFallback(removed, &re);
		m_State->entries.erase(oldest);
	}
	return true;
}
