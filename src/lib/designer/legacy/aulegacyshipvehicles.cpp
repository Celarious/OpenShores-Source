#include "aulegacyshipvehicles.h"
#include "aulegacyshipmesh.h"
#include "au3dsglsl.h"

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QMatrix4x4>
#include <QMutex>
#include <QMutexLocker>
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace AuLegacyShip
{
namespace Rendering
{
namespace
{
struct VehicleAsset
{
	int commodity;
	const char* model;
	double halfX, halfY;
};

// Bounds and resource hashes were checked against AuModelRes13.dll (2016).
const VehicleAsset VehicleAssets[] = {
	{ 6, "mHelicopter.3dsN", 8.319742202758789, 11.813350200653076 },
	{ 7, "mRocketShip.3dsX", 10.208333015441895, 19.750000715255737 },
	{ 8, "mSpacefighter.3dsN", 14.044610023498535, 14.789060592651367 },
	{ 9, "mFlyingAT.3dsN", 18.64858913421631, 13.657800674438477 },
	{ 28, "mFarmEquipment.3dsN", 4.406362771987915, 7.853582143783569 },
	{ 77, "mNeptune.3dsX", 7.353567838668823, 10.512041091918945 },
	{ 82, "mSUV.3dsN", 8.201797008514404, 9.152505874633789 },
	{ 104, "mAT.3dsN", 6.217533111572266, 8.501940727233887 },
	{ 132, "mWoodenBoat.3dsX", 5.0, 16.090107917785645 },
	{ 133, "mMotorBike.3dsN", 1.49844628572464, 4.320924758911133 },
	{ 231, "mSpaceFighter2.3dsN", 12.763139724731445, 13.934858798980713 }
};

const VehicleAsset* FindVehicle(int commodity)
{
	for (const VehicleAsset& asset : VehicleAssets)
		if (asset.commodity == commodity)
			return &asset;
	return nullptr;
}

QVector3D Point(const AuPoint& point)
{
	return QVector3D(float(point.x), float(point.y), float(point.z));
}

Vertex ConvertVertex(const AuPointTexNS& point)
{
	Vertex result;
	result.position = Point(point);
	result.normal = Point(point.m_N).normalized();
	result.uv = QVector2D(point.m_Tx, point.m_Ty);
	return result;
}

struct ResourceModel
{
	QVector<Batch> batches;
	QVector<QVector<Vertex>> screens;
	QVector3D minimum, maximum;
	int vertexCount = 0;
	bool loaded = false;
};

ResourceModel Model(const QString& name)
{
	static QMap<QString, ResourceModel> cache;
	static QMutex cacheMutex;
	{
		QMutexLocker lock(&cacheMutex);
		const auto found = cache.constFind(name);
		if (found != cache.constEnd())
			return found.value();
	}
	ResourceModel result;
	// Building and spacecraft preparation can request these assets concurrently.
	// Return an implicitly shared snapshot rather than a reference into the map;
	// loading outside the lock also leaves cached assets available to previews.
	const auto publish = [&]() -> ResourceModel {
		QMutexLocker lock(&cacheMutex);
		const auto found = cache.constFind(name);
		if (found != cache.constEnd()) return found.value();
		cache.insert(name, result);
		return result;
	};
	Au3dsGLSLModel model;
	if (!QFile::exists(name) || !model.Load(name))
		return publish();
	result.minimum = Point(model.m_Min);
	result.maximum = Point(model.m_Max);
	QString modelTexture;
	for (const Au3dsGLSLMaterial& material : model.m_Materials)
	{
		const QString path = QStringLiteral(":/data/") + QFileInfo(material.TextureFile()).fileName();
		if (!material.TextureFile().isEmpty() && QFile::exists(path))
		{
			modelTexture = path;
			break;
		}
	}
	// LoadModel retains the original format's matrices, precomputed normals,
	// texture coordinates, and Symmetrica inches-to-feet conversion.
	for (Au3dsGLSLObject& object : model.m_Objects)
	{
		Au3dsGLSLTriTexList triangles;
		object.LoadModel(triangles, model.m_Materials, false, model.m_ModelType, GL_ONE_MINUS_SRC_ALPHA);
		QVector<int> faceMaterials(triangles.size(), -1);
		for (auto it = object.m_Materials.constBegin(); it != object.m_Materials.constEnd(); ++it)
		{
			const int material = model.m_Materials.IndexOf(it.key());
			for (quint16 face : it.value())
				if (face < faceMaterials.size())
					faceMaterials[face] = material;
		}
		QMap<int, int> batches;
		for (int i = 0; i < triangles.size(); ++i)
		{
			const int materialIndex = faceMaterials[i];
			if (!batches.contains(materialIndex))
			{
				Batch batch;
				batch.material = Material::Untextured;
				// The original model renderer selects the first available material
				// texture for the entire model, including its glass and light objects.
				batch.textureFile = modelTexture;
				batch.interior = true;
				batch.equipment = true;
				batch.tint.setW(float(triangles.m_GlowColor.alphaF()));
				batch.twoSided = object.Name().contains(QStringLiteral("TwoSided"));
				if (materialIndex >= 0 && materialIndex < model.m_Materials.size())
				{
					const Au3dsGLSLMaterial& material = model.m_Materials[materialIndex];
					const QColor color = material.DiffuseColor(1);
					if (batch.textureFile.isEmpty() && color.isValid())
					{
						batch.tint.setX(float(color.redF()));
						batch.tint.setY(float(color.greenF()));
						batch.tint.setZ(float(color.blueF()));
					}
					batch.twoSided = batch.twoSided || material.IsTwoSided();
				}
				batches.insert(materialIndex, result.batches.size());
				result.batches.append(batch);
			}
			Vertex vertices[] = { ConvertVertex(triangles[i].m_V0), ConvertVertex(triangles[i].m_V1),
				ConvertVertex(triangles[i].m_V2) };
			const QVector3D cross = QVector3D::crossProduct(vertices[1].position - vertices[0].position,
				vertices[2].position - vertices[0].position);
			if (QVector3D::dotProduct(cross, vertices[0].normal + vertices[1].normal + vertices[2].normal) < 0)
				std::swap(vertices[1], vertices[2]);
			for (const Vertex& vertex : vertices)
				result.batches[batches.value(materialIndex)].vertices.append(vertex);
			result.vertexCount += 3;
		}
	}
	if (model.m_Screens)
	{
		for (const Au3dsGLSLScreen& screen : *model.m_Screens)
		{
			const QVector3D points[] = { Point(screen.LocLL()), Point(screen.LocLR()),
				Point(screen.LocUR()), Point(screen.LocUL()) };
			const QVector2D uvs[] = { QVector2D(0, 0), QVector2D(1, 0), QVector2D(1, 1), QVector2D(0, 1) };
			const QVector3D normal = QVector3D::crossProduct(points[1] - points[0], points[2] - points[0]).normalized();
			QVector<Vertex> vertices;
			for (int index : { 0, 1, 2, 0, 2, 3 })
			{
				Vertex vertex;
				vertex.position = points[index] + normal * 0.002f;
				vertex.normal = normal;
				vertex.uv = uvs[index];
				vertices.append(vertex);
			}
			result.screens.append(vertices);
		}
	}
	result.loaded = !result.batches.isEmpty();
	return publish();
}

class Appender
{
public:
	explicit Appender(Mesh& value) : mesh(value)
	{
		for (const Batch& batch : mesh.batches)
			vertices += batch.vertices.size();
		bounded = vertices != 0;
	}
	Mesh& mesh;
	qint64 vertices = 0;
	bool exhausted = false;
	bool bounded = false;

	void Add(const Batch& source, const QMatrix4x4& transform, int deck, const QString& texture = QString())
	{
		if (vertices + source.vertices.size() > 6000000)
		{
			if (!exhausted)
				mesh.warnings.append(QStringLiteral("Equipment exceeds the six-million-vertex preview limit."));
			exhausted = true;
			return;
		}
		Batch batch = source;
		batch.deck = deck;
		if (!texture.isEmpty())
		{
			batch.textureFile = texture;
			batch.tint.setX(1);
			batch.tint.setY(1);
			batch.tint.setZ(1);
		}
		for (Vertex& vertex : batch.vertices)
		{
			vertex.position = transform.map(vertex.position);
			vertex.normal = transform.mapVector(vertex.normal).normalized();
			if (!bounded)
			{
				mesh.minimum = mesh.maximum = vertex.position;
				bounded = true;
			}
			for (int axis = 0; axis < 3; ++axis)
			{
				mesh.minimum[axis] = std::min(mesh.minimum[axis], vertex.position[axis]);
				mesh.maximum[axis] = std::max(mesh.maximum[axis], vertex.position[axis]);
			}
		}
		vertices += batch.vertices.size();
		mesh.batches.append(batch);
	}

	void AddModel(const ResourceModel& model, const QMatrix4x4& transform, int deck, const QString& texture = QString())
	{
		for (const Batch& batch : model.batches)
			Add(batch, transform, deck, texture);
	}

	void Missing(const QString& name)
	{
		const QString warning = QStringLiteral("The original equipment model is unavailable: %1.").arg(name);
		if (!mesh.warnings.contains(warning))
			mesh.warnings.append(warning);
	}
};

QString ConsoleScreen(int type)
{
	switch (type)
	{
	case 1: return QStringLiteral(":/data/txConsoleNav.bmp");
	case 4: return QStringLiteral(":/data/txConsoleSurgery.bmp");
	case 6: return QStringLiteral(":/data/ec_Console.bmp");
	case 7: return QStringLiteral(":/data/txConsoleWeapon.bmp");
	case 12: return QStringLiteral("legacy:pc_Console.bmp");
	case 13: return QStringLiteral(":/data/dc_Console.bmp");
	case 15: return QStringLiteral(":/data/sc_Console.bmp");
	case 17: return QStringLiteral(":/data/txConsoleShield.bmp");
	case 19: return QStringLiteral(":/data/txConsoleTransporter.bmp");
	default: return QString();
	}
}

void Console(Appender& append, const Detail& detail, const QVector3D& location)
{
	const QString seatName = QStringLiteral(":/data/mConsoleSeat.3dsN");
	const ResourceModel seat = Model(seatName);
	if (!seat.loaded)
	{
		append.Missing(seatName);
		return;
	}
	QVector3D offset = (seat.minimum + seat.maximum) * 0.5f;
	offset.setZ(seat.minimum.z());
	QMatrix4x4 transform;
	transform.translate(location);
	transform.rotate(90, 0, 0, 1);
	transform.translate(-offset - QVector3D(1, 0, 0));
	append.AddModel(seat, transform, detail.deck, QStringLiteral(":/data/mConsoleTex.png"));
	if (detail.detailType == 73)
		return;
	const QString panelName = QStringLiteral(":/data/mConsoleScreen.3dsN");
	const ResourceModel panel = Model(panelName);
	if (!panel.loaded)
	{
		append.Missing(panelName);
		return;
	}
	transform.setToIdentity();
	transform.translate(location);
	transform.rotate(90, 0, 0, 1);
	transform.translate(-offset);
	append.AddModel(panel, transform, detail.deck, QStringLiteral(":/data/mConsoleTex.png"));
	for (int i = 0; i < std::min(2, panel.screens.size()); ++i)
	{
		Batch screen;
		screen.material = Material::Untextured;
		screen.equipment = screen.interior = screen.twoSided = true;
		screen.vertices = panel.screens[i];
		screen.selfIllumination = 1;
		append.Add(screen, transform, detail.deck, i == 0 ? ConsoleScreen(detail.detailType)
			: QStringLiteral(":/data/txConsole_Lower.bmp"));
	}
}

void GameTable(Appender& append, const Detail& detail, const QVector3D& location)
{
	const QString tableName = QStringLiteral(":/data/mGameTable.3dsN");
	const QString chairName = QStringLiteral(":/data/mGameChair.3dsN");
	const ResourceModel table = Model(tableName);
	const ResourceModel chair = Model(chairName);
	if (!table.loaded || !chair.loaded)
	{
		append.Missing(!table.loaded ? tableName : chairName);
		return;
	}
	const float angle = detail.detailType == 75 ? 45.0f : 0.0f;
	QMatrix4x4 transform;
	transform.translate(location - QVector3D(0, 0, table.minimum.z() - 0.083f));
	transform.rotate(angle, 0, 0, 1);
	append.AddModel(table, transform, detail.deck, QStringLiteral(":/data/mGameTableTex.png"));
	// RenderGameTable's second chair uses -45 radians in the original client.
	const float facing[] = { angle - 135, angle - 45.0f * 180.0f / 3.14159265358979323846f,
		angle + 45, angle + 135 };
	for (int i = 0; i < 4; ++i)
	{
		const float around = angle + 90 * i;
		transform.setToIdentity();
		transform.translate(location);
		transform.rotate(around, 0, 0, 1);
		transform.translate(4.5f, 4.5f, -chair.minimum.z() + 0.083f);
		transform.rotate(facing[i] - around, 0, 0, 1);
		append.AddModel(chair, transform, detail.deck, QStringLiteral(":/data/mGameTableTex.png"));
	}
}

void TurretGun(Appender& append, const Detail& detail, const QVector3D& location)
{
	const QString name = QStringLiteral(":/data/mTurretGun.3dsN");
	const ResourceModel gun = Model(name);
	if (!gun.loaded)
	{
		append.Missing(name);
		return;
	}
	QVector3D position = location;
	float angle = 90;
	if (detail.detailType == 22)
		position.setZ(position.z() - gun.minimum.z());
	else if (detail.detailType == 33)
		position.setZ(position.z() - gun.maximum.z());
	else
	{
		const int step = detail.detailType == 21 ? 0 : detail.detailType - 33;
		const float offsets[][2] = { { 2, 0 }, { 1.414f, 1.414f }, { 0, 2 }, { -1.414f, 1.414f },
			{ -2, 0 }, { -1.414f, -1.414f }, { 0, -2 }, { 1.414f, -1.414f } };
		position += QVector3D(offsets[step][0], offsets[step][1], 4 - (gun.minimum.z() + gun.maximum.z()) * 0.5f);
		angle = step * 45.0f;
	}
	QMatrix4x4 transform;
	transform.translate(position);
	transform.rotate(angle, 0, 0, 1);
	// The barrel points along model +X. Its opening is below the model
	// origin, so the rotation pivot is not a valid muzzle position.
	QVector3D muzzleMinimum = gun.maximum, muzzleMaximum = gun.minimum;
	for(const auto& batch : gun.batches)
		for(const auto& vertex : batch.vertices)
			if(vertex.position.x() >= gun.maximum.x() - .001f)
				for(int axis = 0; axis < 3; ++axis)
				{
					muzzleMinimum[axis] = std::min(muzzleMinimum[axis], vertex.position[axis]);
					muzzleMaximum[axis] = std::max(muzzleMaximum[axis], vertex.position[axis]);
				}
	const QVector3D muzzle = transform.map((muzzleMinimum + muzzleMaximum) * .5f);
	const int first = append.mesh.batches.size();
	append.AddModel(gun, transform, detail.detailType == 22 ? detail.deck - 1 : detail.deck);
	for(int i = first; i < append.mesh.batches.size(); ++i)
	{
		append.mesh.batches[i].turretId = detail.detailId;
		append.mesh.batches[i].rotationPivot = position;
		append.mesh.batches[i].turretMuzzle = muzzle;
	}
}
}

bool VehicleSupported(int commodity)
{
	return FindVehicle(commodity) != nullptr;
}

QRectF VehicleFootprint(int commodity)
{
	const VehicleAsset* asset = FindVehicle(commodity);
	return asset ? QRectF(-asset->halfY, -asset->halfX, asset->halfY * 2, asset->halfX * 2) : QRectF();
}

bool ResourceEquipmentSupported(int detailType)
{
	return detailType == 73 || detailType == 74 || detailType == 75 || detailType == 21 || detailType == 22
		|| (detailType >= 33 && detailType <= 40) || !ConsoleScreen(detailType).isEmpty();
}

void AppendResourceEquipment(const Design& design, Mesh& mesh)
{
	Appender append(mesh);
	for (const Detail& detail : design.details)
	{
		if (detail.deck < MinimumDeck || detail.deck > MaximumDeck)
			continue;
		// RenderEnvironmentObject renders asset objects regardless of hiddenRaw;
		// the separate procedural RenderInterior pass observes that flag.
		if (append.exhausted)
			continue;
		const QPointF point = DecodePoint(detail.packedXY);
		const QVector3D location(float(point.x()), float(point.y()), float(detail.deck * DeckHeight));
		const int firstDetailBatch = mesh.batches.size();
		if (detail.detailType >= 57 && detail.detailType <= 64)
		{
			const int commodity = detail.commodityIdNormalized ? detail.commodityIdNormalized : detail.commodityIdRaw;
			const VehicleAsset* asset = FindVehicle(commodity);
			if (!asset)
			{
				append.Missing(QStringLiteral("vehicle commodity %1").arg(commodity));
				continue;
			}
			const QString name = QStringLiteral(":/data/") + QString::fromLatin1(asset->model);
			const ResourceModel model = Model(name);
			if (!model.loaded)
			{
				append.Missing(name);
				continue;
			}
			QVector3D offset = (model.minimum + model.maximum) * 0.5f;
			offset.setZ(model.minimum.z());
			QMatrix4x4 transform;
			transform.translate(location);
			// ClosestVehicleParkingSpot (2016) subtracts pi/2 from the slot angle.
			transform.rotate((detail.detailType - 57) * 45.0f - 90.0f, 0, 0, 1);
			transform.translate(-offset);
			append.AddModel(model, transform, detail.deck);
		}
		else if (detail.detailType == 74 || detail.detailType == 75)
			GameTable(append, detail, location);
		else if (detail.detailType == 21 || detail.detailType == 22 || (detail.detailType >= 33 && detail.detailType <= 40))
			TurretGun(append, detail, location);
		else if (ResourceEquipmentSupported(detail.detailType))
		{
			const int first = mesh.batches.size();
			Console(append, detail, location);
			for(int i = first; i < mesh.batches.size(); ++i)
			{
				mesh.batches[i].consoleId = detail.detailId;
				mesh.batches[i].rotationPivot = location;
			}
		}
		for(int i = firstDetailBatch; i < mesh.batches.size(); ++i)
			mesh.batches[i].detailId = detail.detailId;
	}
}
}
}
