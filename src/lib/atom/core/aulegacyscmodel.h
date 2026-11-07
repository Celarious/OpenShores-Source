#ifndef AULEGACYSCMODEL_H
#define AULEGACYSCMODEL_H

#include "auatom.h"
#include "auglsl.h"
#include "aulegacyscgeometry.h"
#include "aulegacyscnpcocclusion.h"
#include <QPointer>
#include <QSharedPointer>
#include <QOpenGLContext>
#include <atomic>

class AuScDesign;
class AuGLEffects;
class AuLegacyScDeckPlan;
class QElapsedTimer;

struct AuLegacyScDraw
{
	AuScVAO vao;
	GLuint texture = 0;
	GLuint unlitTexture = 0;
	GLuint effectTextures[4] = {};
	GLuint onEffectTextures[4] = {};
	int effectFirstFrame = -1;
	int effectFrameCount = 0;
	QString roomName;
	QVector2D textureInset;
	float opacity = 1;
	bool interior = false;
	bool hullSurface = false;
	bool cutout = false;
	bool translucent = false;
	quint16 doorId = 0;
	quint16 turretId = 0;
	quint16 weaponBayId = 0;
	quint16 transporterId = 0;
	quint8 doorState = 0;
};

// One immutable design's shared geometry. GPU operations and drawing belong
// to the model maker's render thread/context; workers retain no ship pointers.
class AUATOM_DLL AuLegacyScModel
{
public:
	AuLegacyScModel(const AuScDesign& design);
	~AuLegacyScModel();
	bool Matches(const AuScDesign& design) const;
	static void Start(const QSharedPointer<AuLegacyScModel>& model);
	void Prepare(); // CPU-only worker entry.
	bool Upload(AuGLEffects& re, QElapsedTimer& frameBudget); // Resumable GL work.
	void Cancel() { m_Cancelled.store(true, std::memory_order_release); }
	void DeleteGL(); // Current owning context required; also cancels preparation.
	bool IsBusy() const;
	bool Ready() const { return m_Ready && m_Context && !m_Cancelled.load(std::memory_order_acquire); }
	quint64 VertexBytes() const;
	QVector<AuLegacyScDraw>& Draws() { return m_Draws; }
	bool NpcOccluded(const AuBox& bounds, const AuPoint& eye, const QSet<quint16>& closedDoors) const
		{ return Ready() && m_NpcOcclusion.Occluded(bounds, eye, closedDoors); }

private:
	struct Material
	{
		AuLegacyScGeometry::TextureBinding binding;
		QImage image, unlit;
		GLuint texture = 0, unlitTexture = 0;
		bool transparent = false;
		bool mipMap = false, unlitMipMap = false;
	};
	struct PreparedDraw
	{
		AuLegacyScDraw draw;
		int material = 0;
		AuScVertexDataMap vertices;
	};
	quint32 m_DesignId;
	QByteArray m_TextureMap;
	QSharedPointer<const AuLegacyScDeckPlan> m_Revision;
	const void* m_SourceParts;
	int m_PartCount;
	std::atomic<bool> m_Cancelled{false};
	std::atomic<int> m_State{0}; // 0 preparing, 1 prepared, 2 failed. Publishes CPU data.
	QString m_Error;
	QVector<Material> m_Materials;
	QMap<int, QImage> m_Effects;
	QVector<PreparedDraw> m_PreparedDraws;
	AuLegacyScNpcOcclusion m_NpcOcclusion;
	quint64 m_VertexBytes = 0;
	// Render-thread state, accessed only after acquire of m_State.
	QPointer<QOpenGLContext> m_Context;
	QVector<AuLegacyScDraw> m_Draws;
	int m_NextMaterial = 0, m_NextDraw = 0;
	bool m_Ready = false, m_UploadFailed = false;
};

#endif
