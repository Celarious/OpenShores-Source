#include "aulegacyscmodel.h"
#include "augleffects.h"
#include "auscdesign.h"
#include "auscparts.h"
#include "auglobal.h"
#include "dgdata.h"
#include <QThreadPool>
#include <QRunnable>
#include <QElapsedTimer>
#include <memory>
#include <cmath>

namespace
{
bool FullyOpaque(const QImage& image)
{
	if(image.isNull()) return false;
	if(!image.hasAlphaChannel()) return true;
	const QImage pixels = image.convertToFormat(QImage::Format_ARGB32);
	for(int row = 0; row < pixels.height(); ++row)
	{
		const QRgb* scan = reinterpret_cast<const QRgb*>(pixels.constScanLine(row));
		for(int column = 0; column < pixels.width(); ++column)
			if(qAlpha(scan[column]) != 255) return false;
	}
	return true;
}

class LegacyScBuildTask : public QRunnable
{
	QSharedPointer<AuLegacyScModel> m_Model;
public:
	explicit LegacyScBuildTask(const QSharedPointer<AuLegacyScModel>& model) : m_Model(model) {}
	void run() override { m_Model->Prepare(); }
};
}

AuLegacyScModel::AuLegacyScModel(const AuScDesign& design)
	: m_DesignId(design.Id()), m_TextureMap(design.LegacyTextureMap()), m_Revision(design.LegacyDeckPlan()),
	  m_SourceParts(&design.Parts()), m_PartCount(design.Parts().size())
{
}

AuLegacyScModel::~AuLegacyScModel()
{
	// Shared ship references can outlive the cache. Only the last reference
	// releases geometry, and only while the original context is current.
	if(m_Context && QOpenGLContext::currentContext() == m_Context.data()) DeleteGL();
}

bool AuLegacyScModel::Matches(const AuScDesign& design) const
{
	// Generated legacy blueprints are final assemblies. A replaced record gets
	// a new immutable deck-plan token; upgrading geometry assigns a new ID.
	return !m_Cancelled.load(std::memory_order_acquire) && m_DesignId == design.Id()
		&& m_Revision == design.LegacyDeckPlan() && m_TextureMap == design.LegacyTextureMap()
		&& m_SourceParts == &design.Parts() && m_PartCount == design.Parts().size();
}

void AuLegacyScModel::Start(const QSharedPointer<AuLegacyScModel>& model)
{
	QThreadPool::globalInstance()->start(new LegacyScBuildTask(model), 0);
}

bool AuLegacyScModel::IsBusy() const
{
	const int state = m_State.load(std::memory_order_acquire);
	return !m_Cancelled.load(std::memory_order_acquire) && state != 2 && !m_UploadFailed && !m_Ready;
}

quint64 AuLegacyScModel::VertexBytes() const
{
	return m_State.load(std::memory_order_acquire) == 1 ? m_VertexBytes : 0;
}

void AuLegacyScModel::Prepare()
{
	bool success = false;
	{
		std::unique_ptr<AuScDesign> snapshot;
		if(gp_AuGlobal && !m_Cancelled.load(std::memory_order_acquire))
		{
			const DgData& data = g_Au.LockScDesignsForRead();
			const auto found = data.m_ScDesigns->find(m_DesignId);
			if(found != data.m_ScDesigns->cend() && Matches(found->second))
				snapshot.reset(new AuScDesign(found->second));
			g_Au.UnlockScDesigns();
		}
		const AuScDesign* design = snapshot.get();
		QString& error = m_Error;
		const auto build = [&]() -> bool {
			if(!design || m_Cancelled.load(std::memory_order_acquire))
			{
				error = QStringLiteral("Legacy blueprint changed before model preparation.");
				return false;
			}
			QVector<AuLegacyScGeometry::TextureBinding> bindings;
			if(!AuLegacyScGeometry::ReadTextureMap(design->LegacyTextureMap(), bindings, error)) return false;
			const int side = bindings.first().packedTile.width();
			QHash<quint32, int> tiles;
			QMap<quint64, QVector<QMap<float, AuScTriRenderList>>> triangles;
			QHash<quint64, QString> roomNames;
			for(int index = 0; index < bindings.size(); ++index)
			{
				const auto& binding = bindings[index];
				const quint32 key = (quint32(binding.paletteIndex) << 16)
					| (quint32(binding.packedTile.x()) << 8) | quint32(binding.packedTile.y());
				tiles.insert(key, index);
			}
			QColor colors[AuScFace::THEME_Count];
			for(QColor& color : colors) color = Qt::white;
			bool oldForcedInteriorGlow = true;
			for(const AuScPart* part : design->Parts())
				if(part->IsHullMesh() && (part->GroupName() == QStringLiteral("legacy/debug/static/v7")
					|| part->GroupName() == QStringLiteral("legacy/debug/static/v8")
					|| part->GroupName() == QStringLiteral("legacy/debug/static/v9")
					|| part->GroupName() == QStringLiteral("legacy/debug/static/v10")
					|| part->GroupName() == QStringLiteral("legacy/debug/static/v11")
					|| part->GroupName() == QStringLiteral("legacy/debug/static/v12")))
				{
					oldForcedInteriorGlow = false;
					break;
				}
			int count = 0;
			for(auto partIterator = design->Parts().cbegin(); partIterator != design->Parts().cend(); ++partIterator)
			{
				if(m_Cancelled.load(std::memory_order_acquire)) return false;
				const AuScPart* part = *partIterator;
				const bool isTurret = part->IsTurret() && part->GroupName().startsWith(QStringLiteral("legacy/turret/detail/"));
				if(part->IsOmitted() || (!part->IsVisibleInFinalModel() && !isTurret)) continue;
				const bool isDoor = part->IsDoor();
				const QString roomName = part->GroupName();
				int effectFirstFrame = -1;
				quint16 weaponBayId = 0;
				quint16 transporterId = 0;
				const QStringList fields = roomName.split('/');
				const int effectField = fields.indexOf(QStringLiteral("effect"));
				if(effectField >= 0)
				{
					bool valid = false;
					effectFirstFrame = fields.value(effectField + 1).toInt(&valid);
					if(!valid || (effectFirstFrame != 0 && effectFirstFrame != 8 && effectFirstFrame != 16
						&& effectFirstFrame != 20 && effectFirstFrame != 24 && effectFirstFrame != 27 && effectFirstFrame != 31
						&& effectFirstFrame != 40 && effectFirstFrame != 47 && effectFirstFrame != 51 && effectFirstFrame != 55))
					{
						error = QStringLiteral("Legacy mesh has invalid equipment effect metadata.");
						return false;
					}
				}
				const int bayField = fields.indexOf(QStringLiteral("bay"));
				if(bayField >= 0)
				{
					bool valid = false;
					const int id = fields.value(bayField + 1).toInt(&valid);
					if(!valid || id < 1 || id > 256 || effectFirstFrame != 51)
					{
						error = QStringLiteral("Legacy mesh has invalid weapon bay effect metadata.");
						return false;
					}
					weaponBayId = quint16(id);
				}
				const int transporterField = fields.indexOf(QStringLiteral("transporter"));
				if(transporterField >= 0)
				{
					bool valid = false;
					const int id = fields.value(transporterField + 1).toInt(&valid);
					if(!valid || id < 1 || id > 65535 || effectFirstFrame != 47)
					{
						error = QStringLiteral("Legacy mesh has invalid transporter effect metadata.");
						return false;
					}
					transporterId = quint16(id);
				}
				const bool partInterior = roomName.endsWith(QStringLiteral("/interior/1"))
					|| (!roomName.endsWith(QStringLiteral("/interior/0"))
						&& (part->IsRoomMesh() || (!part->IsHullMesh() && !isDoor)));
				const quint8 frames[] = {AuScDoor::Closed, AuScDoor::Open50, AuScDoor::Open};
				for(int frame = 0; frame < (isDoor ? 3 : 1); ++frame)
				{
					const quint32 stateKey = isDoor ? (quint32(partIterator.key()) << 8) | frames[frame]
						: isTurret ? (quint32(partIterator.key()) << 8) | AuScTurret::Gun : 0;
					AuScTriRenderList opaque, transparent;
					if(isDoor) part->RenderTrianglesState(opaque, transparent, colors, frames[frame]);
					else if(isTurret) part->RenderTrianglesState(opaque, transparent, colors, AuScTurret::Gun);
					else part->RenderTriangles(opaque, transparent, colors);
					opaque.Insert(transparent);
					for(const AuScTriRender& original : opaque)
					{
						// Hull faces are combined in the final static hull part, so their
						// effect comes from the native face rather than equipment metadata.
						const int triangleEffectFrame = original.Effect() == EFF_Hull ? 31 : effectFirstFrame;
						if(++count > 1000000)
						{
							error = QStringLiteral("Legacy render geometry exceeds its triangle limit.");
							return false;
						}
						const float u = (original.V0().m_Tx + original.V1().m_Tx + original.V2().m_Tx) / 3;
						const float v = (original.V0().m_Ty + original.V1().m_Ty + original.V2().m_Ty) / 3;
						if(!std::isfinite(u) || !std::isfinite(v) || u < 0 || u >= 1 || v <= 0 || v > 1)
						{
							error = QStringLiteral("Legacy mesh has invalid texture coordinates.");
							return false;
						}
						const int x = int(std::floor(u * 256 / side)) * side;
						const int y = int(std::floor((1 - v) * 256 / side)) * side;
						const quint32 key = (quint32(original.TextureIndex()) << 16) | (quint32(x) << 8) | quint32(y);
						const auto tile = tiles.constFind(key);
						if(tile == tiles.constEnd())
						{
							error = QStringLiteral("Legacy mesh texture coordinates do not match its material map.");
							return false;
						}
						const auto& binding = bindings.at(tile.value());
						// Hull doors use exterior materials on the outside and crew/interior
						// materials on the inside. Keep those lighting passes independent.
						const bool hullDoorMaterial = (binding.material >= 8 && binding.material <= 10)
							|| (binding.material >= 19 && binding.material <= 24);
						const bool interior = isDoor ? !hullDoorMaterial : partInterior;
						// Low 25 bits retain door state and lighting; upper bits separate
						// effect batches, including berth sides from their plain tops.
						const quint64 lightingKey = (quint64(stateKey) << 1) | quint64(interior)
							| (quint64(triangleEffectFrame + 1) << 25) | (quint64(weaponBayId) << 32)
							| (quint64(transporterId) << 41)
							| (quint64(!interior && (part->IsHullMesh() || (isDoor && hullDoorMaterial))) << 57);
						if(!triangles.contains(lightingKey))
						{
							triangles.insert(lightingKey, QVector<QMap<float, AuScTriRenderList>>(bindings.size()));
							roomNames.insert(lightingKey, roomName);
						}
						float opacity = original.Opacity();
						// The palette adapter forced partly transparent images into the
						// blend pass. Direct sampling classifies each texel instead.
						if(opacity == .999f && binding.tint.w() == 1) opacity = 1;
						if(!std::isfinite(opacity) || opacity < 0 || opacity > 1)
						{
							error = QStringLiteral("Legacy mesh has invalid surface opacity.");
							return false;
						}
						const auto vertex = [&](const AuScPoint& point) {
							AuScPoint result(point);
							result.m_Tx = (point.m_Tx * 256 - binding.packedTile.x() - .5f) / (side - 1);
							result.m_Ty = 1 - ((1 - point.m_Ty) * 256 - binding.packedTile.y() - .5f) / (side - 1);
							if(triangleEffectFrame >= 0)
							{
								const int width = triangleEffectFrame == 0 ? 128 : triangleEffectFrame == 40 ? 64 : 32;
								const int height = triangleEffectFrame == 24 || triangleEffectFrame == 40 ? 128 : 32;
								result.SetEffectCoord((.5f + point.EffectTx() * (width - 1)) / width,
									(.5f + point.EffectTy() * (height - 1)) / height);
							}
							return result;
						};
						AuScTri triangle(vertex(original.V0()), vertex(original.V1()), vertex(original.V2()));
						// Earlier runtime blueprints forced a minimum glow on interior
						// faces. It would keep those rooms bright with their lights off.
						const float selfIllumination = oldForcedInteriorGlow && interior && original.SelfIllumination() == .25f
							? 0.f : original.SelfIllumination();
						AuScTriRender rendered(triangle, 0, QColor(0, 0, 0, 0), original.Reflectivity(), original.Shine(),
							selfIllumination, 1, original.Effect());
						triangles[lightingKey][tile.value()][opacity].insert(original.Effect(), rendered);
					}
				}
			}
			m_Materials.resize(bindings.size());
			for(int index = 0; index < bindings.size(); ++index)
			{
				if(m_Cancelled.load(std::memory_order_acquire)) return false;
				bool used = false;
				for(auto states = triangles.cbegin(); states != triangles.cend(); ++states)
					if(!states.value().at(index).isEmpty()) { used = true; break; }
				if(!used) continue;
				Material& material = m_Materials[index];
				material.binding = bindings[index];
				const auto& binding = material.binding;
				material.image = AuLegacyScGeometry::OriginalTexture(binding, true, &material.transparent);
				if(material.image.isNull())
				{
					error = QStringLiteral("A bundled legacy texture is unavailable: %1.").arg(binding.resource);
					return false;
				}
				if(binding.resource.isEmpty() && binding.material >= 0)
				{
					material.unlit = AuLegacyScGeometry::OriginalTexture(binding, false, nullptr);
					if(material.unlit.isNull())
					{
						error = QStringLiteral("The bundled unlit legacy spacecraft texture is unavailable.");
						return false;
					}
				}
				const bool opaque = FullyOpaque(material.image);
				const bool unlitOpaque = material.unlit.isNull() || FullyOpaque(material.unlit);
				// The original atlas disabled mipmaps even for opaque floor tiles.
				// Filter those tiles independently, retaining cutout alpha coverage.
				material.mipMap = !binding.cutout || opaque;
				material.unlitMipMap = !binding.cutout || unlitOpaque;
				const bool occluderMaterial = binding.tint.w() == 1 && binding.material != 26 && binding.material != 28
					&& opaque && unlitOpaque;
				for(auto states = triangles.cbegin(); states != triangles.cend(); ++states)
				{
					for(auto group = states.value().at(index).cbegin(); group != states.value().at(index).cend(); ++group)
					{
						PreparedDraw prepared;
						AuLegacyScDraw& draw = prepared.draw;
						prepared.material = index;
						draw.roomName = roomNames.value(states.key());
						if(draw.roomName.startsWith(QStringLiteral("legacy/turret/detail/")))
							draw.turretId = quint16((states.key() >> 9) & 0xffff);
						else draw.doorId = quint16((states.key() >> 9) & 0xffff);
						draw.doorState = quint8((states.key() >> 1) & 0xff);
						draw.interior = states.key() & 1;
						draw.hullSurface = (states.key() >> 57) & 1;
						draw.textureInset = binding.textureInset;
						draw.cutout = binding.cutout;
						draw.opacity = group.key();
						draw.translucent = draw.opacity < .999f || material.transparent;
						draw.effectFirstFrame = int((states.key() >> 25) & 0x7f) - 1;
						draw.weaponBayId = quint16((states.key() >> 32) & 0x1ff);
						draw.transporterId = quint16((states.key() >> 41) & 0xffff);
						if(draw.effectFirstFrame >= 0)
						{
							draw.effectFrameCount = draw.effectFirstFrame == 24 || draw.effectFirstFrame == 40 ? 3 : 4;
							const auto prepareEffect = [&](int frame) -> bool {
								if(m_Effects.contains(frame)) return true;
								const QImage image = AuLegacyScGeometry::OriginalEffectTexture(frame);
								if(image.isNull())
								{
									error = QStringLiteral("A bundled legacy equipment effect texture is unavailable.");
									return false;
								}
								m_Effects.insert(frame, image);
								return true;
							};
							for(int frame = 0; frame < draw.effectFrameCount; ++frame)
							{
								if(!prepareEffect(draw.effectFirstFrame + frame)) return false;
								if(draw.effectFirstFrame == 16 && !prepareEffect(12 + frame)) return false;
								if(draw.effectFirstFrame == 31 && !prepareEffect(62 - frame)) return false;
								if(draw.effectFirstFrame == 47 && !prepareEffect(43 + frame)) return false;
							}
						}
						// Only fully opaque, static surfaces or closed door panels can
						// block NPCs. Cutout atlas flags alone do not imply transparency.
						if(occluderMaterial && draw.opacity == 1 && !draw.turretId
							&& (!draw.doorId || draw.doorState == AuScDoor::Closed))
							m_NpcOcclusion.Add(group.value(), draw.doorId);
						AuScVAO::Prepare(group.value(), prepared.vertices);
						for(const AuScVertexData& vao : prepared.vertices)
							m_VertexBytes += quint64(vao.Vertices()) * 24 * sizeof(GLfloat);
						m_PreparedDraws.append(prepared);
					}
				}
			}
			m_NpcOcclusion.Build();
			return !m_PreparedDraws.isEmpty();
		};
		success = build();
		if(!success)
		{
			m_PreparedDraws.clear();
			m_Materials.clear();
			m_Effects.clear();
			if(!m_Cancelled.load(std::memory_order_acquire)) qWarning() << "Legacy model preparation:" << m_DesignId << error;
		}
	}
	// Publish only after snapshot/temporary destruction.
	m_State.store(success ? 1 : 2, std::memory_order_release);
}

bool AuLegacyScModel::Upload(AuGLEffects& re, QElapsedTimer& budget)
{
	if(m_Cancelled.load(std::memory_order_acquire) || m_State.load(std::memory_order_acquire) != 1 || m_UploadFailed) return false;
	if(m_Ready) return Ready();
	if(!m_Context) m_Context = QOpenGLContext::currentContext();
	if(!m_Context || m_Context.data() != QOpenGLContext::currentContext()) return false;
	if(!budget.isValid()) budget.start();
	if(budget.nsecsElapsed() >= 4000000) return false;
	const auto texture = [&](const QString& key, const QImage& image, bool mipMap, bool clampEdges) -> GLuint {
		const QString textureKey = mipMap ? key + QStringLiteral("/mipmapped") : key;
		GLuint result = re.UseTexture(STATIC_TEXTURE_ID, textureKey);
		if(result) return result;
		result = re.CreateTextureFromBitmap(STATIC_TEXTURE_ID, textureKey, image, GL_LINEAR, GL_LINEAR,
			mipMap, clampEdges ? 1 : 0);
		if(mipMap) re.SetTextureAnisotropy(result);
		return result;
	};
	bool failed = false;
	while(m_NextMaterial < m_Materials.size() && !failed)
	{
		Material& material = m_Materials[m_NextMaterial++];
		if(material.image.isNull()) continue; // Unused material map entry.
		material.texture = texture(AuLegacyScGeometry::OriginalTextureKey(material.binding), material.image,
			material.mipMap, material.binding.cutout);
		material.unlitTexture = material.unlit.isNull() ? material.texture
			: texture(AuLegacyScGeometry::OriginalTextureKey(material.binding, false), material.unlit,
				material.unlitMipMap, material.binding.cutout);
		failed = !material.texture || !material.unlitTexture;
		material.image = QImage();
		material.unlit = QImage();
		if(budget.nsecsElapsed() >= 4000000) break;
	}
	while(m_NextMaterial == m_Materials.size() && m_NextDraw < m_PreparedDraws.size() && !failed
		&& budget.nsecsElapsed() < 4000000)
	{
		PreparedDraw& prepared = m_PreparedDraws[m_NextDraw++];
		AuLegacyScDraw draw = prepared.draw;
		const Material& material = m_Materials[prepared.material];
		draw.texture = material.texture;
		draw.unlitTexture = material.unlitTexture;
		for(int frame = 0; frame < draw.effectFrameCount; ++frame)
		{
			const auto effect = [&](int sourceFrame) -> GLuint {
				return texture(QStringLiteral("LegacyEffect/%1").arg(sourceFrame), m_Effects.value(sourceFrame), false, true);
			};
			draw.effectTextures[frame] = effect(draw.effectFirstFrame + frame);
			if(draw.effectFirstFrame == 16) draw.onEffectTextures[frame] = effect(12 + frame);
			if(draw.effectFirstFrame == 31) draw.onEffectTextures[frame] = effect(62 - frame);
			if(draw.effectFirstFrame == 47) draw.onEffectTextures[frame] = effect(43 + frame);
			failed = failed || !draw.effectTextures[frame] || ((draw.effectFirstFrame == 16 || draw.effectFirstFrame == 31
				|| draw.effectFirstFrame == 47) && !draw.onEffectTextures[frame]);
		}
		if(!failed) draw.vao.Create(re, prepared.vertices, GL_STATIC_DRAW);
		failed = failed || !draw.vao.VAO();
		m_Draws.append(draw); // Cache owns any partial GPU objects for cleanup.
		prepared.vertices.clear();
	}
	if(failed)
	{
		m_UploadFailed = true;
		// Retain the failed entry to avoid retrying an allocation failure every
		// frame. Invalidation/context teardown still cancels it through DeleteGL.
		for(AuLegacyScDraw& draw : m_Draws) draw.vao.DeleteGL();
		m_Draws.clear();
		m_PreparedDraws.clear();
		m_Materials.clear();
		m_Effects.clear();
		qWarning() << "Legacy model upload:" << m_DesignId << "unable to create textures or geometry";
		return false;
	}
	m_Ready = m_NextDraw == m_PreparedDraws.size();
	if(m_Ready)
	{
		m_PreparedDraws.clear();
		m_Materials.clear();
		m_Effects.clear();
	}
	return m_Ready;
}

void AuLegacyScModel::DeleteGL()
{
	Cancel();
	if(m_Context && m_Context.data() == QOpenGLContext::currentContext())
		for(AuLegacyScDraw& draw : m_Draws) draw.vao.DeleteGL();
	m_Draws.clear();
	m_Context.clear();
	m_Ready = false;
}
