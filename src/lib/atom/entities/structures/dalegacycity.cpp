#include "dalegacycity.h"
#include "auatomrenderinfo.h"
#include "auether.h"
#include "auglmodel.h"
#include "auglmodelmaker.h"
#include "aucityreport.h"
#include "aubox.h"
#include "auglobal.h"
#include "augllight.h"
#include "aulegacybd.h"
#include "aulegacybiodome.h"
#include "aulegacybuildinggeometry.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityinventory.h"
#include "aulegacyconstructionmaterials.h"
#include "aulegacycitizenconstruction.h"
#include "aulegacycityproduction.h"
#include "aulegacycitypopulation.h"
#include "aulegacycityinfrastructure.h"
#include "aulegacyfinancetransfer.h"
#include "audice.h"
#include "augear.h"
#include "auitem.h"
#include "auterraininfo.h"
#include "auzoneresource.h"
#include "deprocesscomponent.h"
#include "aulegacyshipequipment.h"
#include "aulegacysccollision.h"
#include "aunoise.h"
#include "autriangle.h"
#include "daanimal.h"
#include "dabd.h"
#include "dacreature.h"
#include "daatomlist.h"
#include "daperson.h"
#include "dadesigner.h"
#include "dgdata.h"
#include "daworldll.h"
#include "daworldringsection.h"
#include "daworldsphere.h"
#include <QDataStream>
#include <QDateTime>
#include <QCryptographicHash>
#include <QFile>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QPainter>
#include <QRunnable>
#include <QThreadPool>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>

namespace
{
AuPoint Point(const QVector3D& p) { return AuPoint(p.x(), p.y(), p.z()); }

bool ExteriorLightsOn(const DaWorldLL& world, const AuLegacyCity::Development& development,
	const AuMatrix4x4& placement)
{
	// DpBuilding::IsDark 0x5a1a10; DpGlobeRoad::IsDark 0x653810
	// switches the whole road on when either endpoint is dark.
	if(development.industryId != 2) return DaAtom::IsDark(world.QuerySunlight(placement.Translation()));
	const double halfLength = development.roadLengthMeters * FeetPerMeter() / 2;
	AuGlobeLLF start, end;
	if(!world.LLOfLoc(end, placement.Project(AuPoint(0, halfLength, 0)))
		|| !world.LLOfLoc(start, placement.Project(AuPoint(0, -halfLength, 0)))) return false;
	return DaAtom::IsDark(world.QuerySunlight(world.LocOfLL(end, development.roadEndAltitudeMSL)))
		|| DaAtom::IsDark(world.QuerySunlight(world.LocOfLL(start, development.roadStartAltitudeMSL)));
}

QColor ExteriorLightColor(bool runway, bool powered)
{
	// DaCity::RenderPostTerrain 0x2b2c20 keeps dim amber lights without
	// electricity. Powered streets are sodium; runway slabs are mercury.
	if(powered) return runway ? QColor(216, 247, 255) : QColor(255, 209, 178);
	QColor color;
	if(runway) color.setRgbF(.5f, .38627452f, .28039217f);
	else color.setRgbF(.5f, .2882353f, .08039216f);
	return color;
}

template<class Shader>
void RenderFlag(Shader& shader, const AuMatrix4x4& model, const AuPoint& ambientUp,
	GLuint texture, float force, float period, const AuGLVAO& vao)
{
	shader.SetAmbientLightUp(ambientUp.x, ambientUp.y, ambientUp.z);
	shader.SetModelView(model);
	shader.SetFlagTexture(texture);
	shader.SetWind(force, period);
	vao.SelectGL(); vao.DrawGL(); vao.DeselectGL();
}

void CreateFlagVAO(AuGLVAO& vao, AuGLEffects& re, float aspectRatio)
{
	// DaCity::CreateFlagVAO, AuAtom13 0x281950: six strips on each
	// face, with the original wave displacement and normal-rotation samples.
	const float x[] = {0, 1.3333334f, 2.6666667f, 4, 5.3333335f, 6.6666665f, 8};
	const float u[] = {0, .16666667f, .33333334f, .5f, .6666667f, .8333333f, 1};
	const float wave[] = {0, .7071f, 1, .7071f, 0, -.7071f, -1};
	const float rotation[] = {1.5707964f, .7853982f, 0, -.7853982f, -1.5707964f, -.7853982f, 0};
	float positions[72 * 3], normals[72 * 3], texCoordWave[72 * 4];
	int count = 0;
	const auto vertex = [&](int column, bool top, float normalY) {
		positions[count * 3] = x[column];
		positions[count * 3 + 1] = 0;
		positions[count * 3 + 2] = top ? 0 : -8 * aspectRatio;
		normals[count * 3] = 0;
		normals[count * 3 + 1] = normalY;
		normals[count * 3 + 2] = 0;
		texCoordWave[count * 4] = u[column];
		texCoordWave[count * 4 + 1] = top ? 1 : 0;
		texCoordWave[count * 4 + 2] = wave[column];
		texCoordWave[count * 4 + 3] = rotation[column];
		++count;
	};
	for(int column = 0; column < 6; ++column)
	{
		vertex(column, false, -1); vertex(column, true, -1); vertex(column + 1, false, -1);
		vertex(column + 1, false, -1); vertex(column, true, -1); vertex(column + 1, true, -1);
	}
	for(int column = 6; column > 0; --column)
	{
		vertex(column, false, 1); vertex(column, true, 1); vertex(column - 1, true, 1);
		vertex(column - 1, true, 1); vertex(column - 1, false, 1); vertex(column, false, 1);
	}
	vao.CreateGL(re, count, 3);
	vao.SelectGL();
	const auto buffer = [&](GLuint attribute, int components, const float* data, size_t bytes) {
		re.glBindBuffer(GL_ARRAY_BUFFER, vao.Buffer(attribute));
		re.glBufferData(GL_ARRAY_BUFFER, bytes, data, GL_STATIC_DRAW);
		re.glVertexAttribPointer(attribute, components, GL_FLOAT, GL_FALSE, 0, nullptr);
	};
	buffer(0, 3, positions, sizeof(positions));
	buffer(1, 3, normals, sizeof(normals));
	buffer(2, 4, texCoordWave, sizeof(texCoordWave));
	re.glBindBuffer(GL_ARRAY_BUFFER, 0);
	vao.DeselectGL();
}

// DpGlobeRoad::Contains, AuAtom13 0x652cf0. The visible pavement's
// eight-foot border is deliberately absent from the placement footprint.
bool RoadContains(const DaWorldLL& world, const AuLegacyCity::Development& road,
	const AuGlobeLLF& start, const AuGlobeLLF& end, const AuGlobeLLF& point, double margin)
{
	float fraction;
	const auto closest = point.CoordOnSegment(start, end, &fraction, world.LLShape());
	const float startAltitude = float(road.roadStartAltitudeMSL), endAltitude = float(road.roadEndAltitudeMSL);
	const float altitude = fraction <= 0 ? startAltitude : fraction >= 1 ? endAltitude
		: (endAltitude - startAltitude) * fraction + startAltitude;
	const AuPoint endpoint = world.LocOfLL(fraction <= 0 ? start : fraction >= 1 ? end : closest, altitude);
	const AuPoint query = world.LocOfLL(point, altitude);
	const float halfWidth = float(road.roadWidthMeters * FeetPerMeter() / 2);
	if(fraction >= 0 && fraction <= 1)
	{
		const float width = float(halfWidth + margin);
		return float(query.Distance2(endpoint)) <= width * width;
	}
	if(margin <= 0) return false;
	const AuPoint first = world.LocOfLL(start, startAltitude), last = world.LocOfLL(end, endAltitude);
	const double beyond = (fraction < 0 ? -double(fraction) : double(fraction) - 1) * first.Distance(last);
	if(beyond > margin) return false;
	AuPoint direction = last - first;
	if(direction.IsZero()) direction.x = 1; else direction.Unit();
	AuPoint side = direction.Cross(-world.GravityDir(endpoint)); side.Unit();
	const AuPoint delta = query - endpoint;
	const double lateral = delta.x * side.x + delta.y * side.y + delta.z * side.z;
	if(std::abs(lateral) <= halfWidth + margin) return true;
	const AuPoint edge = endpoint + side * (lateral > 0 ? halfWidth : -halfWidth);
	return query.Distance2(edge) <= double(float(margin * margin));
}

bool BuildingTouchesRoad(const DaWorldLL& world, const AuLegacyCity::Development& building,
	const AuMatrix4x4& placement, double radius, const AuLegacyCity::Development& road,
	const AuMatrix4x4& roadPlacement)
{
	AuGlobeLLF start, end;
	const double halfLength = road.roadLengthMeters * FeetPerMeter() / 2;
	if(!world.LLOfLoc(start, roadPlacement.Project(AuPoint(0, -halfLength, 0)))
		|| !world.LLOfLoc(end, roadPlacement.Project(AuPoint(0, halfLength, 0)))) return false;
	// DaCity::DevelopmentContaining 0x28c050: an expanded center query,
	// then 32 square-boundary samples with no expansion (0x28c354-0x28c3ad).
	if(RoadContains(world, road, start, end, building.location, radius)) return true;
	for(int i = 0; i < 32; ++i)
	{
		const float angle = float(i) * 6.2831855f * .03125f;
		const float x = std::cos(angle), y = std::sin(angle), scale = qMax(std::abs(x), std::abs(y));
		AuGlobeLLF ll;
		if(world.LLOfLoc(ll, placement.Project(AuPoint(radius * (x / scale), radius * (y / scale), 0)))
			&& RoadContains(world, road, start, end, ll, 0)) return true;
	}
	return false;
}

bool RoadTouchesBuilding(const DaWorldLL& world, const AuLegacyCity::Development& road,
	const AuMatrix4x4& placement, const AuLegacyCity::Development& building,
	const AuMatrix4x4& buildingPlacement, double radius)
{
	// LockTestBuildLocation 0x5264d0 checks center and both edges
	// every fifteen feet, including the final endpoint, against DpBuilding::Contains.
	const double halfLength = road.roadLengthMeters * FeetPerMeter() / 2;
	AuGlobeLLF startLL, endLL;
	world.LLOfLoc(startLL, placement.Project(AuPoint(0, -halfLength, 0)));
	world.LLOfLoc(endLL, placement.Project(AuPoint(0, halfLength, 0)));
	const AuPoint start = world.LocOfLL(startLL, road.roadStartAltitudeMSL);
	const AuPoint end = world.LocOfLL(endLL, road.roadEndAltitudeMSL), direction = end - start;
	const double length = direction.Length(); if(length <= 0) return false;
	const auto inverse = buildingPlacement.Inverse();
	for(double along = 0;; along = qMin(length, along + 15))
	{
		AuPoint center = start + direction * (along / length);
		world.ElevateLocMSL(center, road.roadStartAltitudeMSL
			+ (road.roadEndAltitudeMSL - road.roadStartAltitudeMSL) * along / length);
		AuPoint side = direction.Cross(-world.GravityDir(center)); side.Unit(road.roadWidthMeters * FeetPerMeter() / 2);
		for(int edge : {0, -1, 1})
		{
			AuGlobeLLF ll; if(!world.LLOfLoc(ll, center + side * edge)) continue;
			const AuPoint local = inverse.Project(world.LocOfLL(ll, building.altitudeMSL));
			if(qMax(float(std::abs(local.x)), float(std::abs(local.y))) <= radius) return true;
		}
		if(along >= length) return false;
	}
}

double TerrainLevelRadius(quint8 industry, const AuLegacyCityCatalogue& catalogue, quint8 constructionProcess = 0)
{
	// DpDevelopment::TerrainLevelRadius, AuAtom13 0xfe610. Default BuildRadius
	// uses the construction recipe's meter byte (AuData13 0x2c4e0).
	switch(industry)
	{
	case 20: return 26.24672f;
	case 30: case 32: return 42.650917f;
	case 31: case 50: case 51: return 32.8084f;
	case 66: case 67: return 49.2126f;
	case 113: return 65.6168f;
	case 114: return 9.84252f;
	default: break;
	}
	if(constructionProcess)
	{
		const auto* process = catalogue.ConstructionProcess(AuLegacyCity::ConstructionProcessId(constructionProcess));
		return process && process->industry.value == industry ? double(float(process->buildRadius * FeetPerMeter())) : -1;
	}
	int meters = -1;
	for(const auto& process : catalogue.ConstructionProcesses())
		if(process.industry.value == industry)
		{
			if(meters >= 0 && meters != process.buildRadius) return -1;
			meters = process.buildRadius;
		}
	return meters < 0 ? -1 : double(float(meters * FeetPerMeter()));
}

bool SameTerrainSite(const AuLegacyCity::Development& p, const AuLegacyCity::Development& q)
{
	return p.industryId == q.industryId && p.constructionProcessId == q.constructionProcessId && p.materialId == q.materialId && p.location == q.location
		&& p.altitudeMSL == q.altitudeMSL && p.facingRadians == q.facingRadians
		&& p.constructed == q.constructed && p.demolished == q.demolished && p.roadSlab == q.roadSlab
		&& p.components.isEmpty() == q.components.isEmpty()
		&& p.roadLengthMeters == q.roadLengthMeters && p.roadWidthMeters == q.roadWidthMeters
		&& p.roadStartAltitudeMSL == q.roadStartAltitudeMSL && p.roadEndAltitudeMSL == q.roadEndAltitudeMSL
		&& p.biodomeRadiusMeters == q.biodomeRadiusMeters;
}

bool SameTerrainSites(const QVector<AuLegacyCity::Development>& a, const QVector<AuLegacyCity::Development>& b)
{
	if(a.size() != b.size()) return false;
	for(int i = 0; i < a.size(); ++i)
	{
		if(a[i].id != b[i].id || !SameTerrainSite(a[i], b[i])) return false;
	}
	return true;
}

bool SameBuildingGeometry(const AuLegacyCity::Development& a, const AuLegacyCity::Development& b)
{
	return a.id == b.id && a.designId == b.designId && a.minimumDeck == b.minimumDeck
		&& a.maximumDeck == b.maximumDeck && a.completedLevels == b.completedLevels && SameTerrainSite(a, b);
}

bool SameGeometry(const QVector<AuLegacyCity::Development>& a, const QVector<AuLegacyCity::Development>& b)
{
	if(!SameTerrainSites(a,b)) return false;
	for(int i=0;i<a.size();++i)
		if(a[i].id!=b[i].id || a[i].designId!=b[i].designId || a[i].constructionProcessId!=b[i].constructionProcessId
			|| a[i].minimumDeck!=b[i].minimumDeck || a[i].maximumDeck!=b[i].maximumDeck
			|| a[i].completedLevels!=b[i].completedLevels) return false;
	return true;
}

const AuLegacyBd::Paths::PathableCell* PathCell(const AuLegacyBd::Paths::Graph& graph, const AuPoint& location)
{
	const double spacing = AuLegacyShip::CoordinateSpacing;
	if(!std::isfinite(location.x) || !std::isfinite(location.y) || !std::isfinite(location.z)
		|| std::abs(location.x) > 127*spacing || std::abs(location.y) > 127*spacing) return nullptr;
	const double level = std::floor((location.z + .05) / AuLegacyBd::DeckHeight);
	if(level < -127 || level > 127) return nullptr;
	const int deck = int(level);
	const quint32 key = (quint32(quint16(qint16(deck))) << 16)
		| (quint32(int(std::round(location.x / spacing)) + 128) << 8)
		| quint32(int(std::round(location.y / spacing)) + 128);
	const auto found = graph.PathableCells().constFind(key);
	return found == graph.PathableCells().cend() ? nullptr : &found.value();
}

bool ReadEnvelope(QDataStream& stream, QByteArray& bytes)
{
	quint32 size = 0;
	stream >> size;
	if(stream.status() != QDataStream::Ok || size > AuLegacyCity::MaximumStateBytes
		|| !stream.device() || size > quint64(stream.device()->bytesAvailable()))
	{ stream.setStatus(QDataStream::ReadCorruptData); return false; }
	bytes.resize(int(size));
	if(stream.readRawData(bytes.data(), int(size)) != int(size))
	{ stream.setStatus(QDataStream::ReadPastEnd); return false; }
	return true;
}

void WriteEnvelope(QDataStream& stream, const QByteArray& bytes)
{
	stream << quint32(bytes.size());
	if(!bytes.isEmpty()) stream.writeRawData(bytes.constData(), bytes.size());
}

bool ValidateDesigns(const AuLegacyCity::State& state, QString& error)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue) { error = QStringLiteral("Legacy catalogue has not been initialized."); return false; }
	for(const auto& dev : state.developments)
	{
		if(dev.constructionProcessId)
		{
			const auto* process = catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(dev.constructionProcessId));
			if(!process || process->industry.value != dev.industryId)
			{ error = QStringLiteral("A legacy construction process disagrees with its industry."); return false; }
			if(dev.completedLevels>32 || dev.targetLevels<dev.completedLevels)
			{ error=QStringLiteral("Legacy construction level counts disagree."); return false; }
		}
		for(const auto& component : dev.components)
			if(!catalogue->Commodity(AuLegacyCity::CommodityId(component.commodityId)))
			{ error = QStringLiteral("A legacy construction component is absent from the original catalogue."); return false; }
		if(!dev.designId && (dev.industryId == 1 || dev.industryId == 2 || dev.industryId == 10
			|| dev.industryId == 114 || dev.industryId == 129))
		{
			if((AuLegacyBd::Geometry::MaterialAtlas(dev.materialId).isEmpty() && !(dev.industryId==2 && dev.materialId==23))
				|| (dev.industryId==129 && !AuLegacyCityCatalogue::BiodomeSegmentCount(dev.biodomeRadiusMeters*FeetPerMeter())))
			{ error=QStringLiteral("A procedural legacy development has an invalid material or size."); return false; }
			continue;
		}
		const auto design = AuLegacyBd::Catalog::BuiltIn().Find(dev.designId);
		if(!design || design->header.industryId != dev.industryId
			|| AuLegacyBd::Geometry::MaterialAtlas(dev.materialId).isEmpty())
		{ error = QStringLiteral("Legacy development %1 has an incompatible design, industry or material.").arg(dev.id); return false; }
	}
	AuItemHash inventory;
	AuLegacyCity::ProductionState production;
	AuLegacyCity::AccountingState accounting;
	if(!AuLegacyCity::ReadCityInventory(state.inventory, *catalogue, inventory, error)
		|| !AuLegacyCity::DecodeProductionState(*catalogue, state.manufacturing, production, error)
		|| !AuLegacyCity::DecodeAccounting(state.accounting, accounting, error)) return false;
	for(const auto& process : production.processes)
	{
		const auto development = std::find_if(state.developments.cbegin(), state.developments.cend(), [&process](const AuLegacyCity::Development& value) {
			return value.id == process.developmentId;
		});
		const auto* recipe = catalogue->ManufacturingProcess(AuLegacyCity::ManufacturingProcessId(process.processId));
		if(development == state.developments.cend() || development->demolished || !recipe
			|| recipe->industry.value != development->industryId
			|| process.slot >= AuLegacyCity::ManufacturingSlots(development->industryId, development->completedLevels))
		{ error = QStringLiteral("A legacy manufacturing process disagrees with its development."); return false; }
	}
	return true;
}

const AuLegacyCity::ConstructionProcessRecord* ConstructionRecipe(quint8 industry, quint8 process = 0)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue) return nullptr;
	if(process)
	{
		const auto* recipe = catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(process));
		return recipe && recipe->industry.value == industry ? recipe : nullptr;
	}
	return catalogue->ConstructionProcessForIndustry(AuLegacyCity::IndustryId(industry));
}

int MaximumDesignDeck(quint8 industry, quint16 material)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	return catalogue ? catalogue->BuildingDesignMaximumDeck(AuLegacyCity::IndustryId(industry),AuLegacyCity::CommodityId(material)) : 0;
}

void ConstructedDecks(AuLegacyCity::Development& dev)
{
	// DpBuilding::ConstructedDecks, 0x59cb20. Level counts have different
	// geometry meanings for mines, wells and fixed-shape industrial buildings.
	dev.minimumDeck = dev.industryId == 11 || dev.industryId == 12 ? -5
		: dev.industryId == 50 ? -8 : dev.industryId == 51 ? -1 : 0;
	dev.maximumDeck = qint8(MaximumDesignDeck(dev.industryId, dev.materialId));
	const auto* recipe = ConstructionRecipe(dev.industryId, dev.constructionProcessId);
	if(recipe && recipe->stackLimit == 32) dev.minimumDeck = qint8(-2 * int(dev.completedLevels));
	else if(dev.industryId == 51 && dev.completedLevels <= 1) dev.minimumDeck = 0;
	else if(recipe && recipe->stackLimit > 0 && recipe->stackLimit < 32)
		dev.maximumDeck = qint8(qMin(int(dev.maximumDeck), qMax(0, int(dev.completedLevels) - 1)));
	dev.constructed = dev.completedLevels > 0 && !dev.demolished;
}

void InitializeConstruction(AuLegacyCity::Development& dev, quint32 labor, quint8 environment)
{
	dev.components.clear(); dev.laborRemaining = qMin(labor, quint32(65535));
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue) return;
	const quint32 pavement = dev.industryId == 2 ? catalogue->RoadPavementQuantity(dev.roadLengthMeters*FeetPerMeter(),dev.roadWidthMeters*FeetPerMeter()) : 0;
	const quint32 multiplier=dev.industryId==129 ? quint32(catalogue->BiodomeSegmentCount(dev.biodomeRadiusMeters*FeetPerMeter())) : 1;
	for(const auto& source : catalogue->ConstructionRequirements(AuLegacyCity::ConstructionProcessId(dev.constructionProcessId),
		AuLegacyCity::CommodityId(dev.materialId),environment,pavement,multiplier))
	{
		AuLegacyCity::ConstructionComponent component;
		component.commodityId = source.commodity.value; component.effect = source.effect; component.quantity = source.quantity;
		dev.components.append(component);
	}
}

quint8 ConstructionEnvironment(const DaWorldLL& world, const AuGlobeLLF& location)
{
	AuTerrainInfo terrain; world.QueryTerrain(terrain, location);
	if(terrain.IsWater() && world.SurfaceAltitudeMSL(location)<=-32) return 4;
	return world.HasBreathableAtmosphere() ? 1 : 2;
}

quint32 SitePreparationLabor(const DaWorldLL& world, const AuLegacyCity::Development& dev)
{
	const auto* recipe = ConstructionRecipe(dev.industryId, dev.constructionProcessId);
	if(!recipe) return 1;
	if(dev.industryId==129) return quint32(AuLegacyCityCatalogue::BiodomeSegmentCount(dev.biodomeRadiusMeters*FeetPerMeter()))*10;
	AuMatrix4x4 placement;
	world.BuildingConstructionTransform(world.LocOfLL(dev.location, dev.altitudeMSL), dev.facingRadians, nullptr, AuPoint(), placement);
	if(dev.industryId == 2)
	{
		// LockAddRoad 0x5253d0: sample at fifteen-foot intervals. Landscapers
		// use area; roads use the cut/fill volume against the endpoint grade.
		const double length = dev.roadLengthMeters*FeetPerMeter(), width = dev.roadWidthMeters*FeetPerMeter();
		double deviation = 0; int samples = 0;
		for(double along=0;;along=qMin(length,along+15))
		{
			AuGlobeLL sample;
			if(world.LLOfLoc(sample,placement.Project(AuPoint(0,along-length/2,0))))
			{
				const double grade = dev.roadStartAltitudeMSL+(dev.roadEndAltitudeMSL-dev.roadStartAltitudeMSL)*along/length;
				deviation += std::abs(grade-world.TerrainAltitudeMSL(sample)); ++samples;
			}
			if(along>=length) break;
		}
		return AuLegacyCityCatalogue::RoadPreparationLabor(AuLegacyCity::ConstructionProcessId(dev.constructionProcessId),
			length,width,deviation/qMax(1,samples));
	}
	unsigned int rocks = 0, shrubs = 0, trees = 0; float water = 0;
	const double radius = recipe->buildRadius * FeetPerMeter();
	world.ResourceDistributionAt(placement, placement.Translation(), AuPoint(radius, radius, 0), rocks, shrubs, trees, water, 10);
	return AuLegacyCityCatalogue::BuiltIn()->SitePreparationLabor(AuLegacyCity::ConstructionProcessId(dev.constructionProcessId),
		dev.completedLevels,rocks,shrubs,trees);
}

int ResourceLevelLimit(const DaWorldLL& world, const AuLegacyCity::Development& dev)
{
	const auto* recipe = ConstructionRecipe(dev.industryId, dev.constructionProcessId);
	if(!recipe) return 0;
	if(recipe->stackLimit < 32) return qMin(32, int(recipe->stackLimit) + 1);
	AuMatrix4x4 placement;
	world.BuildingConstructionTransform(world.LocOfLL(dev.location, dev.altitudeMSL), dev.facingRadians, nullptr, AuPoint(), placement);
	unsigned int rocks = 0, shrubs = 0, trees = 0; float water = 0;
	const double radius = recipe->buildRadius * FeetPerMeter();
	world.ResourceDistributionAt(placement, placement.Translation(), AuPoint(radius, radius, 0), rocks, shrubs, trees, water, 4);
	return AuLegacyCityCatalogue::BuiltIn()->ConstructionResourceLevelLimit(AuLegacyCity::ConstructionProcessId(dev.constructionProcessId),rocks,shrubs,trees);
}

bool ConstructionAuthority(const DaLegacyCity& city, const DaPerson& person, bool owner, QString& error)
{
	if(!g_Au.IsLegacyGeneration() || !city.Parent() || person.FindParentPlanetInEnv() != city.Parent())
	{ error = QStringLiteral("Move onto this Legacy city's world first."); return false; }
	if(owner && city.EmpireId() != person.EmpireId() && !person.HasArchitectPower() && !person.HasProgrammerPower())
	{ error = QStringLiteral("Only the city's empire may change its construction projects."); return false; }
	return true;
}

int ConstructionThreat(DaWorldLL& world, const AuId& cityId, const AuLegacyCity::Development& dev, quint32 empire)
{
	// CheckWildAnimalOrEnemyCreature, AuAtom13 0x50edc0. These industries
	// limit the occupied part of the site; the others use the whole site.
	double distanceLimit=0;
	switch(dev.industryId)
	{
	case 20: distanceLimit=26.24672; break;
	case 30: case 32: distanceLimit=42.650917; break;
	case 31: case 50: case 51: distanceLimit=32.8084; break;
	case 66: case 67: distanceLimit=49.2126; break;
	case 113: distanceLimit=65.6168; break;
	case 114: distanceLimit=9.84252; break;
	default: break;
	}
	const AuPoint center=world.LocOfLL(dev.location,dev.altitudeMSL);
	DaAtomList atoms; world.ListSubatoms(atoms);
	for(auto* atom : atoms)
	{
		auto* creature=dynamic_cast<DaCreature*>(atom);
		if(!creature || (!creature->IsNormal() && !creature->IsTraveling()) || creature->CurrentHitPoints()<=0
			|| creature->IsUnderGround() || !creature->IsOnGround()) continue;
		const auto* environment=creature->LockEnvironmentQuery();
		const bool sameSite=environment->LegacyCityId()==cityId && environment->LegacyDevelopmentId()==dev.id;
		creature->UnlockEnvironmentQuery();
		if(!sameSite || (distanceLimit && creature->Location().Distance2(center)>distanceLimit*distanceLimit)) continue;
		if(dynamic_cast<DaAnimal*>(creature) && !creature->EmpireId()) return 1;
		const auto* person=dynamic_cast<DaPerson*>(creature);
		if(person && person->IsInvisibleOffLine()) continue;
		if(PoliticalStance(creature->EmpireId(),empire)==POL_Enemy) return 2;
	}
	return 0;
}

bool ComponentsComplete(const AuLegacyCity::Development& dev)
{
	for(const auto& component : dev.components)
		if((component.effect == CE_Required || component.effect == CE_RequiredQ)
			&& component.applied < qMax(quint32(1), component.quantity)) return false;
	return true;
}
}

struct DaLegacyCity::Runtime
{
	struct Site
	{
		AuLegacyCity::Development state;
		AuMatrix4x4 placement, inverse;
		AuGlobeLLF roadStart, roadEnd;
		float radius = 0;
		float halfWidth = 0, halfLength = 0;
	};
	struct DrawBatch
	{
		quint32 development = 0;
		AuGLVAO vao;
		GLuint texture = 0;
		bool twoSided = false;
		bool interior = false;
		bool depthOnly = false;
		quint8 exteriorLens = 0; // 1: street light, 2: runway light.
		QVector4D tint = QVector4D(1,1,1,1);
		float selfIllumination = 0;
	};
	struct PreparedDraw
	{
		DrawBatch draw;
		AuModelTexFogVertexData vertices;
		QString textureKey;
		QImage texture;
		bool buildingAtlas = false;
	};
	struct RenderData { QVector<PreparedDraw> draws; };
	struct Building
	{
		AuLegacyCity::Development state;
		AuLegacyBd::Geometry::Mesh mesh;
		QSharedPointer<AuLegacyScGeometry::CollisionWorld> collision;
		AuLegacyBd::Paths::Graph paths;
		QSharedPointer<const RenderData> render;
	};
	struct Job
	{
		std::atomic<bool> cancelled{false}, finished{false};
		quint64 revision = 0;
		AuId worldId;
		QVector<AuLegacyCity::Development> developments;
		QVector<Site> sites;
		QVector<Building> previous, result;
		bool succeeded = false;
	};
	struct BuildTask : QRunnable
	{
		explicit BuildTask(const QSharedPointer<Job>& value) : job(value) {}
		void run() override;
		QSharedPointer<Job> job;
	};
	struct RetireTask : QRunnable
	{
		QSharedPointer<Job> job;
		void run() override { job.reset(); }
	};
	struct Upload
	{
		QSharedPointer<const RenderData> source;
		QVector<DrawBatch> draws;
		int next = 0;
	};
	struct MinePostJob
	{
		std::atomic<bool> cancelled{false}, finished{false};
		AuId worldId;
		quint64 geometryRevision = 0, terrainRevision = 0;
		QVector<AuLegacyCity::Development> mines;
		QVector<AuMatrix4x4> result;
		bool succeeded = false;
	};
	struct MinePostTask : QRunnable
	{
		explicit MinePostTask(const QSharedPointer<MinePostJob>& value) : job(value) {}
		void run() override;
		QSharedPointer<MinePostJob> job;
	};
	struct FlagDraw
	{
		AuGLVAO vao;
		GLuint texture = 0;
		quint32 empire = 0;
	};
	QMutex lock;
	bool asynchronous = false;
	quint64 requestedRevision = 0, builtRevision = 0;
	QSharedPointer<Job> job;
	quint64 revision = 0, drawRevision = 0;
	quint64 sourceRevision = 0;
	quint64 overlayRevision = 0;
	quint32 geometryEmpire = 0;
	QVector<AuLegacyCity::Development> geometryState;
	QVector<Building> buildings;
	QVector<DrawBatch> drawBatches;
	QHash<quint32, QSharedPointer<const RenderData>> drawnModels;
	QHash<quint32, Upload> uploads;
	QVector<FlagDraw> flagDraws;
	QVector<AuMatrix4x4> minePosts;
	QSharedPointer<MinePostJob> minePostJob;
	quint64 minePostRevision = 0, minePostTerrainRevision = 0;
	quint64 siteRevision = 0, terrainRevision = 0, terrainDrawRevision = 0;
	const DaWorldLL* siteWorld = nullptr;
	QVector<AuLegacyCity::Development> siteState;
	QVector<Site> sites, renderedSites;
	~Runtime()
	{
		if(job) job->cancelled.store(true, std::memory_order_release);
		if(minePostJob) minePostJob->cancelled.store(true, std::memory_order_release);
	}
	double AdjustTerrain(const DaWorldLL& world, double altitudeMSL, const AuGlobeLL& ll,
		int& levelSites, bool& hideRocks, int& hideGrass, bool& hideShrubs,
		bool& hideTrees, WORLDTEX_TYPE& pavementType) const;
	bool PavementEdgeDistance(const DaWorldLL& world, const AuGlobeLL& ll, float& dist) const;

	// The old world's development lookup excluded distant sites before
	// InBuildingSite. A tangent-plane projection alone also hits antipodes.
	static bool SiteInRange(const Site& site, const AuPoint& worldLocation)
	{
		const double range = 2 * (site.radius + 32.8084);
		return worldLocation.Distance2(site.placement.Translation()) <= range * range;
	}

	// DpGlobeRoad::AdjustAltMSL, 0x652140. Returns the squared distance from 'll'
	// to the closest point of a road site's segment, clamped to the end points,
	// at the road 'grade' there. 'fraction' is unclamped along the segment.
	static float RoadDistance2(const Site& site, const DaWorldLL& world, const AuGlobeLL& ll, float& fraction, float& grade)
	{
		const auto& dev = site.state;
		const AuGlobeLLF closest = ll.ToLLF().CoordOnSegment(site.roadStart, site.roadEnd, &fraction, world.LLShape());
		grade = float(dev.roadStartAltitudeMSL + (dev.roadEndAltitudeMSL - dev.roadStartAltitudeMSL)
			* qBound(0.f, fraction, 1.f));
		const AuGlobeLLF segmentLL = fraction <= 0 ? site.roadStart : fraction >= 1 ? site.roadEnd : closest;
		return float(world.LocOfLL(ll, grade).Distance2(world.LocOfLL(segmentLL, grade)));
	}

	// Original pavement index a road site paints, or WORLDTEX_Count for none.
	static WORLDTEX_TYPE RoadPavementType(const AuLegacyCity::Development& dev)
	{
		if(!dev.constructed) return WORLDTEX_TYPE(0);
		if(dev.constructionProcessId == 2) return WORLDTEX_TYPE(6);
		if(dev.constructionProcessId == 3) return WORLDTEX_TYPE(4);
		if(dev.constructionProcessId == 4) return WORLDTEX_TYPE(3);
		if(dev.constructionProcessId == 7) return WORLDTEX_TYPE(2);
		return WORLDTEX_Count;
	}

	void EnsureSites(const DaLegacyCity& city, const DaWorldLL& world)
	{
		QReadLocker stateLock(&city.m_CityLock);
		EnsureSites(city.m_City.developments, city.m_CityRevision, world);
	}

	void EnsureSites(const QVector<AuLegacyCity::Development>& developments, quint64 currentRevision, const DaWorldLL& world)
	{
		if(siteWorld == &world && siteRevision == currentRevision) return;
		siteRevision = currentRevision;
		if(siteWorld == &world && SameTerrainSites(siteState, developments)) return;
		siteWorld = &world;
		siteState = developments;
		sites.clear();
		if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
			for(const auto& dev : siteState)
			{
				if(dev.industryId==129) continue; // DpBldBiodome::AdjustAltMSL is a no-op.
				const double radius = TerrainLevelRadius(dev.industryId, *catalogue, dev.constructionProcessId);
				if(radius < 0) continue;
				Site site; site.state = dev;
				site.radius = float(radius) + 2.828427f; // InBuildingSite 0x5a0c90.
				site.halfWidth = dev.industryId == 2 ? dev.roadWidthMeters * FeetPerMeter()/2+8 : site.radius;
				site.halfLength = dev.industryId == 2 ? dev.roadLengthMeters * FeetPerMeter()/2 : site.radius;
				site.radius = qMax(site.halfWidth,site.halfLength);
				world.BuildingConstructionTransform(world.LocOfLL(dev.location, dev.altitudeMSL),
					dev.facingRadians, nullptr, AuPoint(), site.placement);
				site.inverse = site.placement.Inverse();
				if(dev.industryId == 2)
				{
					world.LLOfLoc(site.roadStart, site.placement.Project(AuPoint(0, -site.halfLength, 0)));
					world.LLOfLoc(site.roadEnd, site.placement.Project(AuPoint(0, site.halfLength, 0)));
				}
				sites.append(site);
			}
		++terrainRevision;
	}

	void Ensure(const DaLegacyCity& city)
	{
		const auto* world=dynamic_cast<const DaWorldLL*>(city.Parent());
		QVector<AuLegacyCity::Development> developments;
		{
			QReadLocker stateLock(&city.m_CityLock);
			// Terrain and geometry must describe the same state even if a network
			// update arrives while this snapshot is being taken.
			if(world) EnsureSites(city.m_City.developments, city.m_CityRevision, *world);
			if(sourceRevision == city.m_CityRevision && (builtRevision == requestedRevision
				|| (job && !job->finished.load(std::memory_order_acquire)))) return;
			developments = city.m_City.developments;
			if(sourceRevision != city.m_CityRevision)
			{
				sourceRevision = city.m_CityRevision;
				if(!requestedRevision || geometryEmpire != city.m_City.controllingEmpire
					|| !SameGeometry(geometryState, developments))
				{
					geometryState = developments; geometryEmpire = city.m_City.controllingEmpire;
					++requestedRevision;
					if(job) job->cancelled.store(true, std::memory_order_release);
				}
			}
		}
		if(asynchronous)
		{
			// Poll only. A job owns its snapshots and outlives a departing city;
			// neither render nor environment/collision queries wait for it.
			if(job && job->finished.load(std::memory_order_acquire))
			{
				if(job->revision == requestedRevision && job->succeeded
					&& !job->cancelled.load(std::memory_order_acquire))
				{
					buildings.swap(job->result); builtRevision = requestedRevision; ++revision;
				}
				// Reclaim obsolete meshes/BVHs on a worker too. Transfer the last
				// runtime reference before scheduling, so cleanup cannot race ahead
				// and leave the render thread doing the final deletion.
				auto* retire = new RetireTask;
				retire->job.swap(job);
				QThreadPool::globalInstance()->start(retire, -1);
			}
			if(!job && builtRevision != requestedRevision)
			{
				job.reset(new Job);
				job->revision = requestedRevision; job->developments = developments;
				job->previous = buildings; job->sites = sites;
				if(world) job->worldId = world->Id();
				QThreadPool::globalInstance()->start(new BuildTask(job), 0);
			}
		}
		else if(builtRevision != requestedRevision)
		{
			// Authority still needs collision/path data immediately. Its work is
			// independent of the scene replica and never holds the city state lock.
			const auto previous = buildings;
			Build(developments, world, previous, nullptr, false);
			builtRevision = requestedRevision; ++revision;
		}
		QHash<quint32, const AuLegacyCity::Development*> latest;
		for(const auto& dev : developments) latest.insert(dev.id, &dev);
		for(auto& building : buildings)
		{
			const auto found = latest.constFind(building.state.id);
			if(found != latest.cend() && SameBuildingGeometry(building.state, **found)) building.state = **found;
		}
	}

	static QSharedPointer<const RenderData> PrepareRender(const Building& building,
		QHash<quint16, QImage>& atlases, QHash<QString, QImage>& textures)
	{
		QSharedPointer<RenderData> result(new RenderData);
		const quint16 material = building.state.materialId;
		const quint16 atlasMaterial = building.state.industryId == 2 && material == 23 ? 76 : material;
		for(const auto& batch : building.mesh.batches)
		{
			if(batch.collisionOnly || batch.empireFlag) continue;
			PreparedDraw prepared;
			prepared.draw.development = building.state.id;
			prepared.draw.twoSided = batch.twoSided; prepared.draw.interior = batch.interior;
			prepared.draw.depthOnly = batch.depthOnly;
			prepared.draw.exteriorLens = batch.textureSlot == 37 ? 1 : batch.textureSlot == 38 ? 2 : 0;
			prepared.draw.tint = batch.tint; prepared.draw.selfIllumination = batch.selfIllumination;
			prepared.buildingAtlas = batch.textureFile.isEmpty() && batch.textureSlot >= 0;
			prepared.textureKey = QStringLiteral("legacy-city/%1/%2/%3").arg(material).arg(batch.textureSlot).arg(batch.textureFile);
			if(!textures.contains(prepared.textureKey))
			{
				QImage texture;
				if(!batch.textureFile.isEmpty()) texture.load(batch.textureFile);
				else if(batch.textureSlot < 0)
				{ texture = QImage(1, 1, QImage::Format_ARGB32); texture.fill(Qt::white); }
				else
				{
					if(!atlases.contains(atlasMaterial)) atlases.insert(atlasMaterial, QImage(AuLegacyBd::Geometry::MaterialAtlas(atlasMaterial)));
					texture = atlases.value(atlasMaterial).copy(AuLegacyBd::Geometry::TextureTile(batch.textureSlot));
					if(batch.textureSlot == 16) texture = texture.mirrored(true, false);
					// Lamp lenses use effect 3, not the window alpha cutoff.
					if(batch.textureSlot == 37 || batch.textureSlot == 38) texture = texture.convertToFormat(QImage::Format_RGB32);
				}
				textures.insert(prepared.textureKey, texture);
			}
			prepared.texture = textures.value(prepared.textureKey);
			const QVector2D inset = prepared.buildingAtlas ? AuLegacyBd::Geometry::TextureInset(batch.textureSlot) : QVector2D();
			// Bound each GL upload, including exceptionally large authored rooms.
			const int chunkVertices = 12288;
			for(int first = 0; first + 2 < batch.vertices.size(); first += chunkVertices)
			{
				PreparedDraw chunk = prepared;
				const int end = qMin(first + chunkVertices, batch.vertices.size());
				chunk.vertices.normal.reserve((end - first) * 3);
				chunk.vertices.position.reserve((end - first) * 3);
				chunk.vertices.texCoordShine.reserve((end - first) * 3);
				for(int i = first; i + 2 < end; i += 3)
					// CPU collision uses CCW. The original renderer submits CW.
					for(int corner : {0, 2, 1})
					{
						const auto& vertex = batch.vertices[i + corner];
						for(int axis = 0; axis < 3; ++axis)
						{
							chunk.vertices.normal.append(vertex.normal[axis]);
							chunk.vertices.position.append(vertex.position[axis]);
						}
						chunk.vertices.texCoordShine.append(inset.x() + vertex.uv.x() * (1 - 2 * inset.x()));
						chunk.vertices.texCoordShine.append(inset.y() + vertex.uv.y() * (1 - 2 * inset.y()));
						chunk.vertices.texCoordShine.append(0);
					}
				result->draws.append(std::move(chunk));
			}
		}
		return result;
	}

	void Build(const QVector<AuLegacyCity::Development>& developments, const DaWorldLL* world,
		const QVector<Building>& previous, const std::atomic<bool>* cancelled, bool prepareRender)
	{
		QHash<quint32, const Building*> old;
		QVector<AuLegacyCity::Development> oldStates;
		for(const auto& building : previous) { old.insert(building.state.id, &building); oldStates.append(building.state); }
		const bool reuseBiodomes = SameTerrainSites(oldStates, developments);
		QHash<quint16, QImage> atlases;
		QHash<QString, QImage> textures;
		buildings.clear();
		for(const auto& dev : developments)
		{
			if(cancelled && cancelled->load(std::memory_order_acquire)) return;
			const auto found = old.constFind(dev.id);
			if(found != old.cend() && SameBuildingGeometry((*found)->state, dev)
				&& (dev.industryId != 129 || reuseBiodomes) && (!prepareRender || (*found)->render))
			{
				Building building = **found; building.state = dev;
				buildings.append(std::move(building)); continue;
			}
			const auto design = AuLegacyBd::Catalog::BuiltIn().Find(dev.designId);
			if(!design && dev.designId) continue; // Loader rejects unknown authored designs.
			Building building;
			building.state = dev;
			AuLegacyBd::Geometry::Options options;
			options.minimumDeck = dev.minimumDeck; options.maximumDeck = dev.maximumDeck;
			options.materialCommodity = dev.materialId; options.constructed = dev.constructed;
			building.mesh = design ? AuLegacyBd::Geometry::Build(*design, options) : AuLegacyBd::Geometry::BuildProcedural(dev);
			if(world && dev.industryId==129)
			{
				AuMatrix4x4 placement;
				world->BuildingConstructionTransform(world->LocOfLL(dev.location,dev.altitudeMSL),dev.facingRadians,nullptr,AuPoint(),placement);
				const auto project=[world,placement,dev](const QVector3D& p,double height) {
					AuGlobeLL ll;
					if(!world->LLOfLoc(ll,placement.Project(Point(p)))) return QVector3D(p.x(),p.y(),float(height));
					const AuPoint result=placement.UnProject(world->LocOfLL(ll,dev.altitudeMSL+height));
					return QVector3D(float(result.x),float(result.y),float(result.z));
				};
				const auto terrain=[this,world,placement,dev](const QVector3D& p) {
					AuGlobeLL ll; if(!world->LLOfLoc(ll,placement.Project(Point(p)))) return 0.0;
					WORLDTEX_TYPE pavement=WORLDTEX_Count; AuTime newest;
					double altitude=world->VirginTerrainAltitudeMSL(ll,pavement,newest);
					int leveled=0,grass=0; bool rocks=false,shrubs=false,trees=false;
					if(pavement==WORLDTEX_Count) altitude=AdjustTerrain(*world,altitude,ll,leveled,rocks,grass,shrubs,trees,pavement);
					return altitude-dev.altitudeMSL;
				};
				building.mesh=AuLegacyBd::Biodome::Build(dev,project,terrain);
			}
			if(world && dev.industryId==2)
			{
				AuMatrix4x4 placement;
				world->BuildingConstructionTransform(world->LocOfLL(dev.location,dev.altitudeMSL),dev.facingRadians,nullptr,AuPoint(),placement);
				bool first=true;
				for(auto& batch : building.mesh.batches) for(auto& vertex : batch.vertices)
				{
					AuGlobeLL ll;
					if(world->LLOfLoc(ll,placement.Project(Point(vertex.position))))
					{
						const AuPoint corrected=placement.UnProject(world->LocOfLL(ll,dev.altitudeMSL+vertex.position.z()));
						vertex.position=QVector3D(float(corrected.x),float(corrected.y),float(corrected.z));
					}
					if(first) { building.mesh.minimum=building.mesh.maximum=vertex.position; first=false; }
					else for(int axis=0;axis<3;++axis)
					{ building.mesh.minimum[axis]=qMin(building.mesh.minimum[axis],vertex.position[axis]); building.mesh.maximum[axis]=qMax(building.mesh.maximum[axis],vertex.position[axis]); }
				}
				for(auto& light : building.mesh.exteriorLights)
				{
					AuGlobeLL ll;
					if(world->LLOfLoc(ll, placement.Project(Point(light.position))))
					{
						const AuPoint corrected = placement.UnProject(world->LocOfLL(ll, dev.altitudeMSL + light.position.z()));
						light.position = QVector3D(float(corrected.x), float(corrected.y), float(corrected.z));
					}
				}
			}
			if(dev.demolished) building.mesh = AuLegacyBd::Geometry::Mesh();
			if(dev.industryId == 2)
			{
				building.mesh.minimum.setX(qMin(building.mesh.minimum.x(),float(-dev.roadWidthMeters*FeetPerMeter()/2)));
				building.mesh.minimum.setY(qMin(building.mesh.minimum.y(),float(-dev.roadLengthMeters*FeetPerMeter()/2)));
				building.mesh.maximum.setX(qMax(building.mesh.maximum.x(),float(dev.roadWidthMeters*FeetPerMeter()/2)));
				building.mesh.maximum.setY(qMax(building.mesh.maximum.y(),float(dev.roadLengthMeters*FeetPerMeter()/2)));
			}
			if(design && dev.constructed)
				if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
				{
					AuLegacyBd::Paths::Options pathOptions;
					pathOptions.minimumDeck = dev.minimumDeck; pathOptions.maximumDeck = dev.maximumDeck;
					pathOptions.siteRadius = TerrainLevelRadius(dev.industryId, *catalogue) + AuLegacyShip::CoordinateSpacing;
					QString error;
					if(!building.paths.Build(*design, pathOptions, error)) building.mesh.warnings.append(error);
				}
			AuLegacyShip::Rendering::Mesh collisionMesh;
			for(const auto& batch : building.mesh.batches)
			{
				// Original building doorways and hatches are always open. Their
				// textured frame quads must never become solid collision panels.
				if(batch.doorId || batch.empireFlag || batch.depthOnly) continue;
				// Match legacy spacecraft: collide with the smooth walking
				// supports, leaving individual rendered risers/treads visual only.
				if(batch.stairs && !batch.collisionOnly) continue;
				AuLegacyShip::Rendering::Batch imported;
				imported.vertices = batch.vertices;
				imported.material = batch.stairs ? AuLegacyShip::Rendering::Material::StairTread
					: AuLegacyShip::Rendering::Material::HullWall;
				collisionMesh.batches.append(imported);
			}
			building.collision.reset(new AuLegacyScGeometry::CollisionWorld(collisionMesh));
			if(prepareRender) building.render = PrepareRender(building, atlases, textures);
			buildings.append(std::move(building));
		}
	}

	void DeleteDraw(AuAtomRenderInfo& ari, const AuId& id)
	{
		for(auto& batch : drawBatches) if(batch.vao.VAO()) batch.vao.DeleteGL();
		for(auto& upload : uploads) for(auto& batch : upload.draws) if(batch.vao.VAO()) batch.vao.DeleteGL();
		for(auto& flag : flagDraws) if(flag.vao.VAO()) flag.vao.DeleteGL();
		flagDraws.clear();
		drawBatches.clear(); drawnModels.clear(); uploads.clear(); drawRevision = 0;
		if(ari.re) ari.re->DeleteTextures(id.GetID());
	}

	void UploadDraw(AuAtomRenderInfo& ari, const AuId& id)
	{
		if(drawRevision == revision && uploads.isEmpty()) return;
		QHash<quint32, QSharedPointer<const RenderData>> wanted;
		for(const auto& building : buildings) wanted.insert(building.state.id, building.render);
		// Retain the old VAOs while a replacement uploads. Discard only removed
		// developments and obsolete partial uploads; textures are shared by key.
		for(auto it = uploads.begin(); it != uploads.end(); )
			if(!wanted.contains(it.key()) || wanted.value(it.key()) != it->source)
			{
				for(auto& batch : it->draws) if(batch.vao.VAO()) batch.vao.DeleteGL();
				it = uploads.erase(it);
			}
			else ++it;
		for(int i = drawBatches.size() - 1; i >= 0; --i)
			if(!wanted.contains(drawBatches[i].development))
			{
				if(drawBatches[i].vao.VAO()) drawBatches[i].vao.DeleteGL();
				drawBatches.removeAt(i);
			}
		for(auto it = drawnModels.begin(); it != drawnModels.end(); )
			if(!wanted.contains(it.key())) it = drawnModels.erase(it); else ++it;

		QElapsedTimer& budget = ari.mm.LegacyUploadBudget();
		for(const auto& building : buildings)
		{
			const auto source = building.render;
			if(!source || drawnModels.value(building.state.id) == source) continue;
			if(!budget.isValid()) budget.start();
			if(budget.nsecsElapsed() >= 4000000) return;
			auto& upload = uploads[building.state.id];
			upload.source = source;
			while(upload.next < source->draws.size())
			{
				if(budget.nsecsElapsed() >= 4000000) return;
				const auto& prepared = source->draws[upload.next];
				DrawBatch draw = prepared.draw;
				draw.texture = ari.re->UseTexture(id.GetID(), prepared.textureKey);
				if(!draw.texture)
				{
					// Original building atlas filters: GL_LINEAR, no mipmaps.
					draw.texture = ari.re->CreateTextureFromBitmap(id.GetID(), prepared.textureKey,
						prepared.texture, GL_LINEAR, prepared.buildingAtlas ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR,
						!prepared.buildingAtlas);
					if(budget.nsecsElapsed() >= 4000000) return;
				}
				draw.vao.CreateForModelTexFog(*ari.re, prepared.vertices, GL_STATIC_DRAW);
				upload.draws.append(draw); ++upload.next;
			}
			for(int i = drawBatches.size() - 1; i >= 0; --i)
				if(drawBatches[i].development == building.state.id)
				{
					if(drawBatches[i].vao.VAO()) drawBatches[i].vao.DeleteGL();
					drawBatches.removeAt(i);
				}
			drawBatches += upload.draws;
			drawnModels.insert(building.state.id, source);
			uploads.remove(building.state.id);
			// Terrain masks follow visible interiors, as in the original VAO.
			std::stable_sort(drawBatches.begin(), drawBatches.end(),
				[](const DrawBatch& a, const DrawBatch& b) { return a.depthOnly < b.depthOnly; });
		}
		drawRevision = revision;
	}
};

void DaLegacyCity::Runtime::BuildTask::run()
{
	// Only roads and biodomes need world projection/terrain during generation.
	// Clone it on the worker under the existing ether lifetime guard. No task
	// retains a live city, runtime or world pointer after that brief snapshot.
	bool needsWorld = false;
	for(const auto& dev : job->developments) needsWorld |= dev.industryId == 2 || dev.industryId == 129;
	AuEther ether("Legacy city geometry worker");
	DaWorldLL* world = nullptr;
	if(needsWorld && !job->cancelled.load(std::memory_order_acquire))
	{
		const AuEther& live = g_Au.LockEtheriumDelete();
		if(const auto* source = dynamic_cast<const DaWorldLL*>(live.FindNormalAtom(job->worldId)))
		{
			if(const auto* designer = source->FindParentDesignerInEnv()) ether.AddAtom(designer->Clone());
			world = static_cast<DaWorldLL*>(source->Clone());
			ether.AddAtom(world);
		}
		g_Au.UnlockEtheriumDelete();
	}
	if((!needsWorld || world) && !job->cancelled.load(std::memory_order_acquire))
	{
		Runtime result;
		result.sites = job->sites;
		result.Build(job->developments, world, job->previous, &job->cancelled, true);
		job->result.swap(result.buildings);
		job->succeeded = !job->cancelled.load(std::memory_order_acquire);
	}
	job->finished.store(true, std::memory_order_release);
}

void DaLegacyCity::Runtime::MinePostTask::run()
{
	// Terrain includes every city's site adjustments. Keep those sites in the
	// private world snapshot, just as the terrain/flora workers do.
	AuEther ether("Legacy mine post worker");
	DaWorldLL* world = nullptr;
	if(!job->cancelled.load(std::memory_order_acquire))
	{
		const AuEther& live = g_Au.LockEtheriumDelete();
		if(const auto* source = dynamic_cast<const DaWorldLL*>(live.FindNormalAtom(job->worldId)))
		{
			const auto* designer = source->FindParentDesignerInEnv();
			if(designer) ether.AddAtom(designer->Clone());
			world = static_cast<DaWorldLL*>(source->Clone());
			ether.AddAtom(world);
			if(designer) world->ChangePos(AuAtomPos(designer->Id(), AuPoint(), AuPoint()));
			DaAtomList children;
			source->ListNormalSubatoms(children);
			bool buildings = false;
			for(const DaAtom* child : children)
			{
				if(job->cancelled.load(std::memory_order_acquire)) break;
				const bool modernBuilding = dynamic_cast<const DaBd*>(child) != nullptr;
				if(modernBuilding || dynamic_cast<const DaLegacyCity*>(child)) ether.AddAtom(child->Clone());
				buildings |= modernBuilding;
			}
			if(buildings) world->BdAdded();
		}
		g_Au.UnlockEtheriumDelete();
	}
	if(world)
	{
		for(const auto& mine : job->mines)
		{
			if(job->cancelled.load(std::memory_order_acquire)) break;
			const auto* recipe = ConstructionRecipe(mine.industryId, mine.constructionProcessId);
			if(!recipe) continue;
			const double radius = float(recipe->buildRadius * FeetPerMeter());
			AuMatrix4x4 placement;
			world->BuildingConstructionTransform(world->LocOfLL(mine.location, mine.altitudeMSL),
				mine.facingRadians, nullptr, AuPoint(), placement);
			// DpBldMine::RenderGreebles 0x591620: site corners follow terrain
			// height and local globe up, independently of the authored floors.
			for(const QPoint& corner : {QPoint(-1, -1), QPoint(-1, 1), QPoint(1, 1), QPoint(1, -1)})
			{
				if(job->cancelled.load(std::memory_order_acquire)) break;
				AuGlobeLL ll;
				if(!world->LLOfLoc(ll, placement.Project(AuPoint(radius * corner.x(), radius * corner.y(), 0)))) continue;
				const AuPoint location = world->LocOfLL(ll, world->TerrainAltitudeMSL(ll));
				AuMatrix4x4 post;
				post.Translate(location);
				post *= AuPoint(0, 0, 1).GetAlignmentTransform(-world->GravityDir(location));
				job->result.append(post);
			}
		}
		job->succeeded = !job->cancelled.load(std::memory_order_acquire);
	}
	job->finished.store(true, std::memory_order_release);
}

DaLegacyCity::DaLegacyCity() : m_Runtime(new Runtime) { m_Runtime->asynchronous = g_Au.IsSceneClient(); }
DaLegacyCity::~DaLegacyCity() = default;
DaLegacyCity::DaLegacyCity(const DaLegacyCity& city) : DaAtom(city), m_Runtime(new Runtime)
{
	QReadLocker lock(&city.m_CityLock); m_City = city.m_City;
	// Scene replicas are separate from solo authority. LoadSubatoms copies a
	// detached row, so its authoritative solo city keeps synchronous queries.
	m_Runtime->asynchronous = g_Au.IsSceneClient() || (g_Au.IsSceneApp() && city.Ether());
}
DaAtom* DaLegacyCity::Clone() const { return new DaLegacyCity(*this); }
AuLegacyCity::State DaLegacyCity::Snapshot() const { QReadLocker lock(&m_CityLock); return m_City; }
bool DaLegacyCity::WriteReportHistory(const AuCityReportHistory& history, QByteArray& bytes, QString& error)
{
	error.clear();
	if(history.IsEmpty() || history.Count() > 240)
	{ error = QStringLiteral("Invalid legacy city report history count."); return false; }
	QByteArray raw;
	QDataStream writer(&raw, QIODevice::WriteOnly); writer.setVersion(QDataStream::Qt_4_1);
	writer << history;
	if(writer.status() != QDataStream::Ok || raw.size() > 128 * 1024 * 1024)
	{ error = QStringLiteral("Legacy city report history exceeds its expanded limit."); return false; }
	// Keep existing small histories byte-identical. Large flag arrays repeat
	// across 48 hours of reports, so compress rather than shorten retention.
	QByteArray stored = raw.size() <= AuLegacyCity::MaximumStateBytes / 2
		? raw : QByteArray("LCRH", 4) + qCompress(raw);
	if(stored.size() > AuLegacyCity::MaximumStateBytes / 2)
	{ error = QStringLiteral("Legacy city report history exceeds its storage envelope."); return false; }
	bytes = stored;
	return true;
}

bool DaLegacyCity::ReadReportHistory(const QByteArray& bytes, AuCityReportHistory& result)
{
	// The shared history assignment appends, so clear it using its existing
	// zero-count reader before publishing or returning an empty result.
	const QByteArray emptyHistory(2, char(0));
	QDataStream reset(emptyHistory); reset.setVersion(QDataStream::Qt_4_1); reset >> result;
	if(bytes.size() < 2 || bytes.size() > AuLegacyCity::MaximumStateBytes / 2) return false;
	QByteArray raw = bytes;
	if(bytes.startsWith("LCRH"))
	{
		if(bytes.size() < 8) return false;
		QDataStream sizeReader(bytes.mid(4, 4));
		quint32 expanded = 0; sizeReader >> expanded;
		if(!expanded || expanded > 128 * 1024 * 1024) return false;
		raw = qUncompress(bytes.mid(4));
		if(quint32(raw.size()) != expanded) return false;
	}
	QDataStream countReader(raw); countReader.setVersion(QDataStream::Qt_4_1);
	quint16 count = 0; countReader >> count;
	if(countReader.status() != QDataStream::Ok || !count || count > 240) return false;
	QDataStream reader(raw); reader.setVersion(QDataStream::Qt_4_1);
	AuCityReportHistory candidate; reader >> candidate;
	// The shared display history applies its normal retention and inventory
	// minimization. Storage retains the full history in raw or compressed form.
	if(reader.status() != QDataStream::Ok || !reader.atEnd() || candidate.IsEmpty() || candidate.Count() > count) return false;
	result = candidate;
	return true;
}
bool DaLegacyCity::ReportHistory(AuCityReportHistory& result) const
{ return ReadReportHistory(Snapshot().reportHistory, result); }
QString DaLegacyCity::Name() const { QReadLocker lock(&m_CityLock); return m_City.name; }
quint32 DaLegacyCity::EmpireId() const { QReadLocker lock(&m_CityLock); return m_City.controllingEmpire; }
quint32 DaLegacyCity::Population() const { QReadLocker lock(&m_CityLock); return quint32(m_City.population); }

int DaLegacyCity::CanSpawnHere(quint32 empireId) const
{
	if(!g_Au.IsLegacyGeneration() || !IsNormal()) return 2;
	const quint32 cityEmpire = EmpireId();
	if(cityEmpire == empireId) return 0;
	const auto stance = PoliticalStance(cityEmpire, empireId);
	return stance == POL_Friend || stance == POL_Empire || stance == POL_Vassal ? 0 : 1;
}

AuPoint DaLegacyCity::SpawnLocation(const AuGlobeLLF* home) const
{
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world) return AuPoint();
	AuPoint origin = Location();
	bool atHome = false;
	if(home)
	{
		QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
		for(const auto& building : m_Runtime->buildings)
		{
			const auto& dev = building.state;
			if(dev.demolished || !dev.completedLevels || dev.location.Longitude() != home->Longitude()
				|| dev.location.Latitude() != home->Latitude()) continue;
			atHome = true;
			AuMatrix4x4 placement;
			world->BuildingConstructionTransform(world->LocOfLL(dev.location, dev.altitudeMSL),
				dev.facingRadians, nullptr, AuPoint(), placement);
			origin = placement.Translation();
			const auto design = AuLegacyBd::Catalog::BuiltIn().Find(dev.designId);
			if(!design) break;
			// DpBuilding::SpawnLocation 0x5f0e10: prefer connected sleeping
			// rooms. Otherwise use another connected interior room.
			QVector<quint16> rooms;
			bool sleeping = false;
			for(const auto& room : design->rooms)
			{
				if(room.deck < dev.minimumDeck || room.deck > dev.maximumDeck) continue;
				const bool preferred = room.roomType == 1 || room.roomType == 4 || room.roomType == 7 || room.roomType == 19;
				if(!preferred && (sleeping || !(room.roomType == 2 || room.roomType == 3 || room.roomType == 5
					|| room.roomType == 6 || room.roomType == 8 || room.roomType == 10 || room.roomType == 11
					|| room.roomType == 14 || (room.roomType >= 16 && room.roomType <= 21)))) continue;
				bool connected = false;
				for(const auto& cell : building.paths.PathableCells())
					if((cell.doorIndex >= 0 || cell.holeIndex >= 0 || cell.railingIndex >= 0)
						&& (cell.room1 == room.roomId || cell.room2 == room.roomId)) { connected = true; break; }
				if(!connected) continue;
				if(preferred && !sleeping) { sleeping = true; rooms.clear(); }
				rooms.append(room.roomId);
			}
			for(int attempt = 0; !rooms.isEmpty() && attempt < 100; ++attempt)
			{
				const quint16 room = rooms.takeAt(g_Au.Dice().Roll(1, rooms.size(), -1));
				QVector<quint32> floors;
				// AuPath::RandomFloorLocation 0xf8bf0 excludes unsupported
				// openings and stair cells, preserving both opening room sides.
				for(auto cell = building.paths.PathableCells().cbegin(); cell != building.paths.PathableCells().cend(); ++cell)
				{
					const bool opening = cell->doorIndex >= 0 || cell->holeIndex >= 0 || cell->railingIndex >= 0;
					if(cell->room1 != room && !(opening && cell->room2 == room)) continue;
					if((!(cell->flags & 8) && (cell->flags & 32)) || (!cell->room2 && (cell->flags & 8))
						|| (cell->flags & 0xff00)) continue;
					floors.append(cell.key());
				}
				if(floors.isEmpty()) continue;
				const quint32 cell = floors.at(g_Au.Dice().Roll(1, floors.size(), -1));
				return placement.Project(AuPoint((int((cell >> 8) & 255) - 128) * AuLegacyShip::CoordinateSpacing,
					(int(cell & 255) - 128) * AuLegacyShip::CoordinateSpacing, qint16(cell >> 16) * AuLegacyBd::DeckHeight + 4));
			}
			break;
		}
	}
	// DaWorldLL::CityRandomSpawnLoc 0x510370 and CitySpawnLoc 0x5104d0.
	// Original vtable slots 0x738/0x730 use GetSurfaceLocation, including
	// the water surface; the home fallback additionally starts four feet up.
	const int dx = g_Au.Dice().Roll(1, 50), dy = g_Au.Dice().Roll(1, 50), dz = g_Au.Dice().Roll(1, 50);
	AuGlobeLLF location;
	if(!world->LLOfLoc(location, origin + AuPoint(dx, dy, dz)))
		world->LLOfLoc(location, origin);
	return atHome ? world->GetSurfaceLocation(location, 4) : world->GetSurfaceLocation(location);
}

bool DaLegacyCity::IsInScene(const DaPerson* sceneAtom, const AuPoint& cameraEnvironmentLocation) const
{
	Q_UNUSED(cameraEnvironmentLocation);
	return sceneAtom && sceneAtom->IsNearPlanet()
		&& FindParentWorldNotSatelliteInEnv() == sceneAtom->FindParentWorldNotSatelliteInEnv();
}

double DaLegacyCity::AdjustSiteTerrain(const DaWorldLL& world, double altitudeMSL, const AuGlobeLL& ll,
	int& levelSites, bool& hideRocks, int& hideGrass, bool& hideShrubs,
	bool& hideTrees, WORLDTEX_TYPE& pavementType) const
{
	if(!g_Au.IsLegacyGeneration() || IsDead()) return altitudeMSL;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->EnsureSites(*this, world);
	return m_Runtime->AdjustTerrain(world,altitudeMSL,ll,levelSites,hideRocks,hideGrass,hideShrubs,hideTrees,pavementType);
}

bool DaLegacyCity::SitePavementEdgeDistance(const DaWorldLL& world, const AuGlobeLL& ll, float& dist) const
{
	if(!g_Au.IsLegacyGeneration() || IsDead()) return false;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->EnsureSites(*this, world);
	return m_Runtime->PavementEdgeDistance(world,ll,dist);
}

bool DaLegacyCity::Runtime::PavementEdgeDistance(const DaWorldLL& world, const AuGlobeLL& ll, float& dist) const
{
	// Mirrors the pavement rules of AdjustTerrain. Overlapping sites are a union,
	// so the nearest edge wins.
	bool found = false;
	for(const auto& site : sites)
	{
		const auto& dev = site.state;
		const AuPoint worldLocation = world.LocOfLL(ll, dev.altitudeMSL);
		if(!SiteInRange(site, worldLocation)) continue;
		float siteDist;
		if(dev.industryId == 2)
		{
			if(RoadPavementType(dev) == WORLDTEX_Count) continue;
			float fraction, grade;
			const float distance2 = RoadDistance2(site, world, ll, fraction, grade);
			// Pavement stops square at the end points.
			siteDist = DaWorldLL::PavementStripEdgeDistance(distance2, fraction, 2 * site.halfLength, site.halfWidth);
		}
		else
		{
			if(AuLegacyBd::Geometry::SiteTextureSlot(dev.materialId, dev.constructed && dev.components.isEmpty()) < 0) continue;
			const AuPoint p = site.inverse.Project(worldLocation);
			siteDist = std::max(float(std::abs(p.x))-site.halfWidth, float(std::abs(p.y))-site.halfLength);
		}
		if(!found || siteDist < dist) dist = siteDist;
		found = true;
	}
	return found;
}

double DaLegacyCity::Runtime::AdjustTerrain(const DaWorldLL& world, double altitudeMSL, const AuGlobeLL& ll,
	int& levelSites, bool& hideRocks, int& hideGrass, bool& hideShrubs,
	bool& hideTrees, WORLDTEX_TYPE& pavementType) const
{
	for(const auto& site : sites)
	{
		// The original projects at the site's altitude, not the incoming terrain
		// altitude. This keeps its square boundary stable on slopes and globes.
		const auto& dev = site.state;
		const AuPoint worldLocation = world.LocOfLL(ll, dev.altitudeMSL);
		if(!SiteInRange(site, worldLocation)) continue;
		if(dev.industryId == 2)
		{
			// Use the original LL segment query, including endpoint blend caps
			// and the eight-foot side margin.
			float fraction, grade;
			const float distance2 = RoadDistance2(site, world, ll, fraction, grade);
			const float width = site.halfWidth;
			const bool landscaping = dev.constructionProcessId == 74 || dev.constructionProcessId == 80 || dev.constructionProcessId == 81;
			const bool endpoint = fraction <= 0 || fraction >= 1;
			if(endpoint || distance2 > width * width)
			{
				if(levelSites || landscaping) continue;
				const float blendRadius = width + 32.8084f;
				if(distance2 >= blendRadius * blendRadius) continue;
				const float blend = endpoint ? std::sqrt(distance2) / blendRadius
					: (std::sqrt(distance2) - width) / 32.8084f;
				altitudeMSL = AuNoiseF::Cosine_Interpolate(grade, float(altitudeMSL), blend);
				continue;
			}
			if(landscaping)
			{
				if(dev.constructionProcessId == 80) hideGrass = 1;
				else if(!hideGrass) hideGrass = 2;
				if(dev.constructionProcessId != 74) hideShrubs = hideTrees = true;
			}
			else
			{
				altitudeMSL = levelSites ? (altitudeMSL * levelSites + grade) / (levelSites + 1) : grade;
				++levelSites;
				if(dev.constructionProcessId != 5)
				{
					hideGrass = 1;
					hideRocks = hideShrubs = hideTrees = true;
				}
			}
			const WORLDTEX_TYPE roadPavement = RoadPavementType(dev);
			if(roadPavement != WORLDTEX_Count) pavementType = roadPavement;
			continue;
		}
		const AuPoint p = site.inverse.Project(worldLocation);
		const double siteAltitude = dev.altitudeMSL;
		const float margin = std::max(float(std::abs(p.x))-site.halfWidth, float(std::abs(p.y))-site.halfLength);
		if(margin >= 32.8084f) continue;
		if(margin > 0)
		{
			// DpBuilding::AdjustAltMSL 0x597380: a level site takes priority
			// over all neighboring blend margins, including other cities.
			if(!levelSites)
				altitudeMSL = AuNoiseF::Cosine_Interpolate(float(altitudeMSL), float(siteAltitude),
					1.f - margin / 32.8084f);
			continue;
		}
		altitudeMSL = siteAltitude;
		++levelSites;
		hideGrass = 1;
		// DoesHideRocks/Shrubs/Trees 0x5a00d0/0x5a0180/0x5a0260.
		// Military bases and guard towers override all three with false.
		const int industry = dev.industryId;
		if(industry != 11 && industry != 12 && industry != 128)
		{
			if(industry != 50 && industry != 51 && industry != 113 && industry != 114) hideRocks = true;
			if(industry != 20 && industry != 31 && industry != 51 && industry != 66
				&& industry != 67 && industry != 113 && industry != 114) hideShrubs = true;
			if(industry != 20 && industry != 30 && industry != 31 && industry != 32
				&& industry != 113 && industry != 114) hideTrees = true;
		}
		const int slot = AuLegacyBd::Geometry::SiteTextureSlot(dev.materialId, dev.constructed && dev.components.isEmpty());
		if(slot >= 0) pavementType = WORLDTEX_TYPE(slot);
	}
	return altitudeMSL;
}

void DaLegacyCity::UpdateSiteTerrain(AuAtomRenderInfo& ari, bool removed)
{
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	if(!world) return;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->EnsureSites(*this, *world);
	removed = removed || IsDead();
	{
		QReadLocker stateLock(&m_CityLock);
		if(removed || m_Runtime->overlayRevision != m_CityRevision)
		{
			m_Runtime->overlayRevision = m_CityRevision;
			// Construction can change levels without changing site terrain.
			// Refresh only building sprites, including while standing still.
			world->InvalidateBuildingOverlaySprites();
		}
	}
	if(!removed && m_Runtime->terrainDrawRevision == m_Runtime->terrainRevision) return;
	// Invalidate old sites as well, so moved/removed developments restore their
	// natural terrain and flora. Release the city lock before terrain work.
	QVector<Runtime::Site> dirty;
	QHash<quint32, const Runtime::Site*> previous, current;
	for(const auto& site : m_Runtime->renderedSites) previous.insert(site.state.id, &site);
	if(!removed) for(const auto& site : m_Runtime->sites) current.insert(site.state.id, &site);
	for(auto it = previous.cbegin(); it != previous.cend(); ++it)
	{
		const auto found = current.constFind(it.key());
		if(found == current.cend() || !SameTerrainSite((*it)->state, (*found)->state)) dirty.append(**it);
	}
	for(auto it = current.cbegin(); it != current.cend(); ++it)
	{
		const auto found = previous.constFind(it.key());
		if(found == previous.cend()) dirty.append(**it);
		else if(!SameTerrainSite((*it)->state, (*found)->state))
		{
			// One unchanged footprint covers both old and new surface states.
			if((*it)->state.location != (*found)->state.location || (*it)->state.altitudeMSL != (*found)->state.altitudeMSL
				|| (*it)->state.facingRadians != (*found)->state.facingRadians || (*it)->radius != (*found)->radius)
				dirty.append(**it);
		}
	}
	m_Runtime->renderedSites = removed ? QVector<Runtime::Site>() : m_Runtime->sites;
	m_Runtime->terrainDrawRevision = removed ? 0 : m_Runtime->terrainRevision;
	lock.unlock();
	for(const auto& site : dirty)
		world->BuildingChanged(site.placement, (site.radius + 32.8084) * std::sqrt(2.0), ari.mm);
}

bool DaLegacyCity::SetState(const AuLegacyCity::State& state, QString& error)
{
	if(!g_Au.IsLegacyGeneration()) { error = QStringLiteral("Legacy cities require a Legacy universe."); return false; }
	QByteArray checked;
	if(!AuLegacyCity::EncodeState(state, checked, error) || !ValidateDesigns(state, error)) return false;
	{
		QWriteLocker lock(&m_CityLock);
		const qint64 lastVisit = m_City.lastVisit;
		m_City = state; m_City.lastVisit = qMax(m_City.lastVisit, lastVisit); ++m_CityRevision;
	}
	SetModified();
	return true;
}

bool DaLegacyCity::ConstructionSitesOverlap(const DaWorldLL& world,
	const AuLegacyCity::Development& site, const AuMatrix4x4& placement,
	const AuLegacyCity::Development& existing, const AuMatrix4x4& otherPlacement)
{
	const auto* recipe = ConstructionRecipe(site.industryId, site.constructionProcessId);
	const auto* otherRecipe = ConstructionRecipe(existing.industryId, existing.constructionProcessId);
	if(!recipe || !otherRecipe) return false;
	const double radius = site.industryId == 129 ? site.biodomeRadiusMeters * FeetPerMeter() : recipe->buildRadius * FeetPerMeter();
	const double otherRadius = existing.industryId == 129 ? existing.biodomeRadiusMeters * FeetPerMeter() : otherRecipe->buildRadius * FeetPerMeter();
	if(site.industryId != 2 && site.industryId != 129 && existing.industryId == 2)
		return BuildingTouchesRoad(world, site, placement, radius, existing, otherPlacement);
	if(site.industryId == 2 && existing.industryId != 2 && existing.industryId != 129)
		return RoadTouchesBuilding(world, site, placement, existing, otherPlacement, otherRadius);
	const AuPoint point = otherPlacement.UnProject(placement.Translation());
	const double ownX = site.industryId == 2 ? site.roadWidthMeters * FeetPerMeter() / 2 : radius;
	const double ownY = site.industryId == 2 ? site.roadLengthMeters * FeetPerMeter() / 2 : radius;
	const double otherX = existing.industryId == 2 ? existing.roadWidthMeters * FeetPerMeter() / 2 : otherRadius;
	const double otherY = existing.industryId == 2 ? existing.roadLengthMeters * FeetPerMeter() / 2 : otherRadius;
	const double angle = site.facingRadians - existing.facingRadians;
	const double cosine = std::cos(angle), sine = std::sin(angle);
	const double rx = ownX * std::abs(cosine) + ownY * std::abs(sine);
	const double ry = ownX * std::abs(sine) + ownY * std::abs(cosine);
	// The same four separating axes and edge tolerance as BeginConstruction.
	return std::abs(point.x) < otherX + rx - .05
		&& std::abs(point.y) < otherY + ry - .05
		&& std::abs(point.x * cosine + point.y * sine) < ownX + otherX * std::abs(cosine) + otherY * std::abs(sine) - .05
		&& std::abs(-point.x * sine + point.y * cosine) < ownY + otherX * std::abs(sine) + otherY * std::abs(cosine) - .05;
}

int DaLegacyCity::ConstructionLevelLimit(quint8 industry, quint16 material)
{
	const auto* recipe = ConstructionRecipe(industry);
	if(!recipe) return 0;
	return AuLegacyCityCatalogue::BuiltIn()->ConstructionLevelLimit(recipe->id,AuLegacyCity::CommodityId(material));
}

bool DaLegacyCity::BeginConstruction(DaPerson& person, quint8 industry, quint8 process, quint32 designId,
	quint16 material, const AuGlobeLLF& location, double heading, quint8 levels, QString& error,
	double roadLength, double roadWidth, bool roadSlab, double biodomeRadius,
	quint16 manufacturingRecipe, bool manufactureCitizen)
{
	if(!ConstructionAuthority(*this, person, true, error)) return false;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	const auto* recipe = ConstructionRecipe(industry, process);
	const auto* manufacturing = AuLegacyCityCatalogue::BuiltIn()->ManufacturingProcess(AuLegacyCity::ManufacturingProcessId(manufacturingRecipe));
	if(manufacturingRecipe && (!manufacturing || manufacturing->industry.value != industry))
	{ error = QStringLiteral("The selected manufacturing recipe belongs to a different industry."); return false; }
	// Road pavement comes from its process, independently of the building
	// material retained in the construction window.
	if(industry==2 && recipe) material=recipe->id.value==3 ? 23 : 76;
	if(!world || !recipe || !levels || levels > AuLegacyCityCatalogue::BuiltIn()->ConstructionLevelLimit(recipe->id,AuLegacyCity::CommodityId(material),world->OrbitZone()!=OZONE_Inferno)
		|| !std::isfinite(heading) || !std::isfinite(location.Latitude()) || !std::isfinite(location.Longitude())
		|| std::abs(location.Latitude()) > PIf/2 || std::abs(location.Longitude()) > PIf
		|| !std::isfinite(roadLength) || !std::isfinite(roadWidth)
		|| roadLength < 1 || roadLength > 1000 || roadWidth < 1 || roadWidth > 100
		|| !std::isfinite(biodomeRadius) || biodomeRadius<1 || biodomeRadius>1000)
	{ error = QStringLiteral("Invalid original construction recipe, material, location or level count."); return false; }
	if(industry != 1 && industry != 2 && industry != 10 && industry != 114 && industry != 129)
	{
		if(!designId) designId = AuLegacyBd::Catalog::BuiltIn().DefaultForIndustry(industry);
		const auto design = AuLegacyBd::Catalog::BuiltIn().Find(designId);
		if(!design || design->header.industryId != industry)
		{ error = industry == 35 ? QStringLiteral("The original Dairy blueprint has not been recovered.")
			: QStringLiteral("Select an original public blueprint for this industry."); return false; }
	}
	else designId = 0;
	AuLegacyCity::Development dev;
	dev.designId = designId; dev.industryId = industry; dev.materialId = material;
	dev.location = location; dev.facingRadians = heading; dev.constructionProcessId = recipe->id.value;
	const double naturalAltitude=world->SurfaceAltitudeMSL(location);
	dev.altitudeMSL = naturalAltitude; dev.completedLevels = 0; dev.targetLevels = levels;
	if(world->Hydrographics() && dev.altitudeMSL>-32 && dev.altitudeMSL<1) dev.altitudeMSL=1;
	dev.roadLengthMeters = float(roadLength); dev.roadWidthMeters = float(roadWidth);
	dev.roadSlab = roadSlab;
	dev.biodomeRadiusMeters=float(biodomeRadius);
	if(industry==129 && !AuLegacyCityCatalogue::BiodomeSegmentCount(dev.biodomeRadiusMeters*FeetPerMeter()))
	{ error=QStringLiteral("The biodome circumference must contain 6 to 100 original fifty-meter segments."); return false; }
	AuTerrainInfo terrain; world->QueryTerrain(terrain, location);
	const double radius = industry==129 ? biodomeRadius*FeetPerMeter() : recipe->buildRadius * FeetPerMeter();
	if(world->IsForbiddenZone(location,dev.altitudeMSL,radius) || dev.altitudeMSL>world->AtmosphereHeightMSL())
	{ error=QStringLiteral("This site is outside the world's permitted construction terrain."); return false; }
	if(industry==62 && naturalAltitude>=0 && (std::abs(location.Latitude())>world->PolarLatitude() || world->OrbitZone()==OZONE_Frigid))
	{ error=QStringLiteral("Hydroelectric power plants require unfrozen water outside the polar region."); return false; }
	AuMatrix4x4 placement;
	world->BuildingConstructionTransform(world->LocOfLL(location, dev.altitudeMSL), heading, nullptr, AuPoint(), placement);
	if(industry == 2)
	{
		AuGlobeLLF start, end;
		if(!world->LLOfLoc(start,placement.Project(AuPoint(0,-roadLength*FeetPerMeter()/2,0)))
			|| !world->LLOfLoc(end,placement.Project(AuPoint(0,roadLength*FeetPerMeter()/2,0))))
		{ error = QStringLiteral("The road endpoints must remain on this world's surface."); return false; }
		dev.roadStartAltitudeMSL = world->SurfaceAltitudeMSL(start);
		dev.roadEndAltitudeMSL = roadSlab ? dev.roadStartAltitudeMSL : world->SurfaceAltitudeMSL(end);
		if(world->Hydrographics())
		{
			if(dev.roadStartAltitudeMSL>-32 && dev.roadStartAltitudeMSL<1) dev.roadStartAltitudeMSL=1;
			if(dev.roadEndAltitudeMSL>-32 && dev.roadEndAltitudeMSL<1) dev.roadEndAltitudeMSL=1;
		}
	}
	unsigned int rocks = 0, shrubs = 0, trees = 0; float water = 0;
	world->ResourceDistributionAt(placement, placement.Translation(), AuPoint(radius,radius,0), rocks,shrubs,trees,water,4);
	// Original terrain categories from LockTestBuildLocation 0x5264d0.
	const int category = recipe->terrain;
	bool terrainOk = true;
	const bool seaBottom = terrain.IsWater() && dev.altitudeMSL <= -32;
	if(seaBottom && -dev.altitudeMSL<(MaximumDesignDeck(industry,material)+4)*AuLegacyBd::DeckHeight)
	{ error=QStringLiteral("The water is too shallow for this original building's required underwater clearance."); return false; }
	if(category == 1 || category == 2 || category == 3) terrainOk = seaBottom || (!terrain.IsWater() && water <= .1f);
	else if(category == 4) terrainOk = terrain.m_byWater && water >= .3f;
	else if(category == 5) terrainOk = terrain.m_byWater && water >= .4f;
	else if(category == 7) terrainOk = (seaBottom || (!terrain.IsWater() && water <= .25f)) && trees > 0;
	else if(category == 8) terrainOk = (seaBottom || (!terrain.IsWater() && water <= .25f)) && rocks > 0;
	else if(category == 9) terrainOk = terrain.m_byWater && water >= .9f;
	else if(category == 10) terrainOk = terrain.m_byWater && water >= 1.f;
	else if(category == 11 || category == 13 || category == 14) terrainOk = seaBottom || !terrain.IsWater();
	if(!terrainOk || levels > ResourceLevelLimit(*world, dev))
	{ error = QStringLiteral("This terrain or its natural resources cannot support the selected construction and levels."); return false; }
	const AuPoint center = placement.Translation();
	// LockTestBuildLocation 0x5264d0 permits actors, vehicles and spacecraft
	// within a new construction site; only development overlaps are tested here.
	DaAtomList peers; world->ListNormalSubatoms(peers);
	bool adjacent = industry == 1 || category == 15 || category == 16;
	int baseFlagsInZone=0;
	const int resourceZone=world->ResourceZone(location.Longitude());
	for(auto* peer : peers)
	{
		if(auto* city = dynamic_cast<DaLegacyCity*>(peer))
		{
			const auto otherCity=city->Snapshot();
			if(industry==10 && world->ResourceZone(city->Transform().Translation())==resourceZone)
				for(const auto& existing : otherCity.developments) if(existing.industryId==10)
					baseFlagsInZone+=qMax(existing.completedLevels,existing.targetLevels);
			for(const auto& existing : otherCity.developments)
			{
				if(existing.demolished) continue;
				const bool landscapingRoad = existing.industryId == 2 && (existing.constructionProcessId == 5
					|| existing.constructionProcessId == 74 || existing.constructionProcessId == 80 || existing.constructionProcessId == 81);
				if(landscapingRoad && industry != 129) continue; // Original FIND_SkipLandscapers.
				const auto* otherRecipe = ConstructionRecipe(existing.industryId, existing.constructionProcessId);
				if(!otherRecipe) continue;
				AuMatrix4x4 otherPlacement;
				world->BuildingConstructionTransform(world->LocOfLL(existing.location,existing.altitudeMSL),existing.facingRadians,nullptr,AuPoint(),otherPlacement);
				const AuPoint p = otherPlacement.UnProject(center);
				const double ownX = industry == 2 ? roadWidth * FeetPerMeter()/2 : radius;
				const double ownY = industry == 2 ? roadLength * FeetPerMeter()/2 : radius;
				const double otherX = existing.industryId == 2 ? existing.roadWidthMeters*FeetPerMeter()/2
					: existing.industryId==129 ? existing.biodomeRadiusMeters*FeetPerMeter() : otherRecipe->buildRadius*FeetPerMeter();
				const double otherY = existing.industryId == 2 ? existing.roadLengthMeters*FeetPerMeter()/2 : otherX;
				const double angle = heading - existing.facingRadians;
				const double cosine=std::cos(angle),sine=std::sin(angle);
				const double rx = ownX*std::abs(cosine) + ownY*std::abs(sine);
				const double ry = ownX*std::abs(sine) + ownY*std::abs(cosine);
				const bool overlapping = ConstructionSitesOverlap(*world, dev, placement, existing, otherPlacement);
				const bool inMargin = std::abs(p.x) <= otherX+rx+32.8084 && std::abs(p.y) <= otherY+ry+32.8084;
				bool domeCompatible=false;
				if(industry==129 || existing.industryId==129)
				{
					const bool constructingDome=industry==129;
					const double domeRadius=constructingDome ? radius : otherX;
					const double rectangleX=constructingDome ? otherX : ownX;
					const double rectangleY=constructingDome ? otherY : ownY;
					const AuMatrix4x4 toDome=constructingDome ? placement.Inverse()*otherPlacement : otherPlacement.Inverse()*placement;
					double outerRadius=0;
					for(int x : {-1,1}) for(int y : {-1,1})
					{
						const AuPoint corner=toDome.Project(AuPoint(x*rectangleX,y*rectangleY,0));
						outerRadius=qMax(outerRadius,std::hypot(corner.x,corner.y));
					}
					const AuPoint domeInRectangle=toDome.UnProject(AuPoint());
					double innerRadius=std::hypot(qMax(0.0,std::abs(domeInRectangle.x)-rectangleX),qMax(0.0,std::abs(domeInRectangle.y)-rectangleY));
					if(industry==129 && existing.industryId==129)
					{
						const double centerDistance=std::hypot(domeInRectangle.x,domeInRectangle.y);
						outerRadius=centerDistance+otherX; innerRadius=qMax(0.0,centerDistance-otherX);
					}
					const bool completelyInside=outerRadius<=domeRadius-AuLegacyBd::Biodome::ContainmentMargin;
					const bool completelyOutside=innerRadius>=domeRadius+AuLegacyBd::Biodome::ContainmentMargin;
					if(!completelyInside && !completelyOutside)
					{ error=QStringLiteral("A biodome's outer wall would intersect an existing development."); return false; }
					if(!constructingDome && industry==111 && completelyInside)
					{ error=QStringLiteral("Airport terminals cannot be enclosed by an existing biodome."); return false; }
					domeCompatible=true;
				}
				if((overlapping && city->EmpireId()!=EmpireId()) || (industry == 1 && inMargin))
				{ error = QStringLiteral("The founding site or road intrudes on another city development."); return false; }
				if(overlapping && !domeCompatible && !(industry == 2 && (existing.industryId == 2 || existing.industryId == 1)))
				{ error = QStringLiteral("The construction site overlaps an existing development."); return false; }
				if(city==this && inMargin)
				{
					const bool town=existing.industryId==1;
					// New roads use the original unfinished-road-inclusive lookup
					// (FINDDEV 0x10); buildings still require a finished road (0x1a).
					const bool road=existing.industryId==2 && (industry==2 || existing.constructed) && !landscapingRoad;
					// LockTestForAdjacentFinishedRoad 0x527fe0 explicitly tries
					// the town square before a finished non-landscaping road.
					if(category==3) adjacent=adjacent || (road && existing.roadSlab);
					else if(category==13) adjacent=adjacent || town || road || existing.industryId==industry;
					else if(category==14 || category==9 || category==10) adjacent=adjacent || !landscapingRoad;
					else adjacent=adjacent || town || road;
				}
			}
		}
	}
	if(industry==10)
	{
		// MaxBaseFlagsInResourceZone 0x4b74f0 uses the original population
		// ceiling, not the later modern-world population formula.
		const auto* sphere=dynamic_cast<const DaWorldSphere*>(world);
		const int populationMaximum=sphere ? sphere->Radius100km()*100 : 1000;
		const auto* flagIndustry=AuLegacyCityCatalogue::BuiltIn()->Industry(AuLegacyCity::IndustryId(10));
		const int troops=flagIndustry ? std::abs(int(flagIndustry->employees)) : 0;
		if(!troops || baseFlagsInZone>=(troops-1+populationMaximum/30)/troops+2)
		{ error=QStringLiteral("This resource zone has reached its original military flag limit."); return false; }
	}
	if(!adjacent)
	{
		error = industry == 2 ? QStringLiteral("Build beside a town square or road.")
			: QStringLiteral("Build beside a completed city development or road.");
		return false;
	}
	dev.selectedManufacturingRecipe = manufacturingRecipe;
	dev.manufacturingCitizenEnabled = manufactureCitizen;
	ConstructedDecks(dev);
	InitializeConstruction(dev, SitePreparationLabor(*world,dev), ConstructionEnvironment(*world,location));
	QWriteLocker lock(&m_CityLock);
	if(m_City.developments.size() >= AuLegacyCity::MaximumDevelopments)
	{ error = QStringLiteral("The city has reached its development limit."); return false; }
	int flagSites=industry==1 || industry==10 ? 1 : 0;
	for(const auto& existing : m_City.developments)
	{
		if(existing.industryId==1 || existing.industryId==10) ++flagSites;
		dev.id = qMax(dev.id,existing.id);
		if(industry == 1 && existing.industryId == 1)
		{ error = QStringLiteral("This city already has a town square."); return false; }
	}
	if(flagSites>4096) { error=QStringLiteral("The city has reached its flag limit."); return false; }
	if(dev.id == std::numeric_limits<quint32>::max()) { error = QStringLiteral("The city has exhausted development identities."); return false; }
	++dev.id;
	AuLegacyCity::State candidate=m_City;
	if(industry == 1)
	{
		if(!person.EmpireId()) { error = QStringLiteral("Join an empire before founding a city."); return false; }
		candidate.citizenSpecies = person.Species().Key();
		// Validate the entire publishable city before consuming the flag.
		dev.completedLevels = dev.targetLevels = 1; dev.laborRemaining=0; dev.components.clear(); ConstructedDecks(dev);
		AuLegacyCity::Flag cityFlag; cityFlag.empire=person.EmpireId(); cityFlag.location=dev.location;
		cityFlag.level=30; cityFlag.capturedAt=QDateTime::currentMSecsSinceEpoch(); candidate.flags.append(cityFlag);
	}
	candidate.developments.append(dev);
	QByteArray checked;
	if(!AuLegacyCity::EncodeState(candidate,checked,error) || !ValidateDesigns(candidate,error)) return false;
	if(industry == 1)
	{
		AuGear* gear = person.LockGearForWrite(); AuItemHash items; gear->CombineItemsForSale(items);
		const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
		const auto flagId = COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(1)));
		QScopedPointer<AuItem> flag;
		for(const auto& entry : items) if(entry.second->CommodityId()==flagId && entry.second->Count())
		{ flag.reset(entry.second->Clone()); flag->SetCount(1); break; }
		if(!flag) { person.UnlockGear(); error=QStringLiteral("An empire flag in your inventory is required to found a city."); return false; }
		gear->RemoveItems(*flag); person.UnlockGear();
	}
	candidate.lastVisit = qMax(candidate.lastVisit, m_City.lastVisit);
	m_City=std::move(candidate); ++m_CityRevision;
	lock.unlock(); SetModified();
	if(industry==1) { person.LockGearForWrite(); person.UpdateGearWeight(); person.UnlockGear(); }
	return true;
}

bool DaLegacyCity::ExtendConstruction(DaPerson& person, quint32 id, quint8 levels, QString& error,
	quint16 manufacturingRecipe, bool manufactureCitizen)
{
	if(!ConstructionAuthority(*this,person,true,error)) return false;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	auto state = Snapshot();
	for(auto& dev : state.developments) if(dev.id == id)
	{
		const auto* recipe=ConstructionRecipe(dev.industryId,dev.constructionProcessId);
		const auto* manufacturing = AuLegacyCityCatalogue::BuiltIn()->ManufacturingProcess(AuLegacyCity::ManufacturingProcessId(manufacturingRecipe));
		if(manufacturingRecipe && (!manufacturing || manufacturing->industry.value != dev.industryId))
		{ error = QStringLiteral("The selected manufacturing recipe belongs to a different industry."); return false; }
		if(!world || !levels || dev.demolished || !dev.components.isEmpty() || !dev.constructed
			|| !recipe || int(dev.completedLevels)+levels > AuLegacyCityCatalogue::BuiltIn()->ConstructionLevelLimit(recipe->id,
				AuLegacyCity::CommodityId(dev.materialId),world->OrbitZone()!=OZONE_Inferno)
			|| int(dev.completedLevels)+levels > ResourceLevelLimit(*world,dev))
		{ error = QStringLiteral("Finish current work first; extensions must fit the original material and resource limits."); return false; }
		const quint8 expectedProcess=dev.constructionProcessId;
		dev.constructionProcessId=recipe->id.value;
		dev.targetLevels = quint8(dev.completedLevels+levels);
		dev.selectedManufacturingRecipe = manufacturingRecipe;
		dev.manufacturingCitizenEnabled = manufactureCitizen;
		InitializeConstruction(dev,SitePreparationLabor(*world,dev),ConstructionEnvironment(*world,dev.location));
		return ReplaceFinishedDevelopment(dev,expectedProcess,dev.completedLevels,error);
	}
	error = QStringLiteral("The development no longer exists."); return false;
}

bool DaLegacyCity::ReplaceFinishedDevelopment(const AuLegacyCity::Development& replacement,
	quint8 expectedProcess, quint8 expectedLevels, QString& error)
{
	QWriteLocker lock(&m_CityLock);
	auto candidate=m_City;
	for(auto& current : candidate.developments) if(current.id==replacement.id)
	{
		if(!current.components.isEmpty() || !current.constructed || current.constructionProcessId!=expectedProcess
			|| current.completedLevels!=expectedLevels || current.designId!=replacement.designId
			|| current.industryId!=replacement.industryId || (current.industryId!=2 && current.materialId!=replacement.materialId))
		{ error=QStringLiteral("This development changed while the request was being prepared. Refresh its construction status."); return false; }
		const bool citizenWorker=current.citizenWorker;
		current=replacement; current.citizenWorker=citizenWorker;
		QByteArray checked;
		if(!AuLegacyCity::EncodeState(candidate,checked,error) || !ValidateDesigns(candidate,error)) return false;
		candidate.lastVisit = qMax(candidate.lastVisit, m_City.lastVisit);
		m_City=std::move(candidate); ++m_CityRevision;
		lock.unlock(); SetModified(); return true;
	}
	error=QStringLiteral("The development no longer exists."); return false;
}

bool DaLegacyCity::SetConstructionHalted(DaPerson& person, quint32 id, bool halted, QString& error)
{
	if(!ConstructionAuthority(*this,person,true,error)) return false;
	QWriteLocker lock(&m_CityLock);
	for(auto& dev : m_City.developments) if(dev.id == id && !dev.components.isEmpty())
	{ dev.halted = halted; ++m_CityRevision; lock.unlock(); SetModified(); return true; }
	error = QStringLiteral("There is no construction to halt or resume."); return false;
}

bool DaLegacyCity::EmployConstructionWorker(DaPerson& person, quint32 id, bool enabled, QString& error)
{
	if(!ConstructionAuthority(*this,person,true,error)) return false;
	QWriteLocker lock(&m_CityLock);
	for(auto& dev : m_City.developments) if(dev.id == id)
	{ dev.citizenWorker = enabled; ++m_CityRevision; lock.unlock(); SetModified(); return true; }
	error = QStringLiteral("The development no longer exists."); return false;
}

bool DaLegacyCity::DemolishDevelopment(DaPerson& person, quint32 id, bool clear, QString& error)
{
	Q_UNUSED(clear);
	if(!ConstructionAuthority(*this,person,true,error)) return false;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	QWriteLocker lock(&m_CityLock);
	auto state = m_City;
	for(int i=0;i<state.developments.size();++i) if(state.developments[i].id == id)
	{
		auto& dev = state.developments[i];
		if(dev.industryId == 1 && state.developments.size() != 1)
		{ error = QStringLiteral("Remove all other buildings and roads before the town square."); return false; }
		if(dev.industryId == 1 || dev.industryId == 10)
			for(int j=state.flags.size()-1;j>=0;--j) if(state.flags[j].location==dev.location) state.flags.removeAt(j);
		// DeleteConstruction 0x514bf0 deletes the development, with no synthetic
		// rubble phase and no refund of materials already applied.
		state.developments.removeAt(i);
		AuLegacyCity::ProductionState production;
		const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
		if(!catalogue || !AuLegacyCity::DecodeProductionState(*catalogue, state.manufacturing, production, error)) return false;
		// DeleteBuilding 0x28add0 calls ClearManufacturingProcesses, which
		// destroys their applied materials without the recipe-change refund.
		production.processes.erase(std::remove_if(production.processes.begin(), production.processes.end(), [id](const AuLegacyCity::ProductionProcess& process) {
			return process.developmentId == id;
		}), production.processes.end());
		if(!AuLegacyCity::EncodeProductionState(*catalogue, production, state.manufacturing, error)) return false;
		state.lastEmployment = 0;
		QByteArray checked;
		if(!AuLegacyCity::EncodeState(state,checked,error)) return false;
		state.lastVisit = qMax(state.lastVisit, m_City.lastVisit);
		m_City=std::move(state); ++m_CityRevision;
		lock.unlock(); SetModified();
		if(world) world->InvalidateEnvironmentRecursive();
		return true;
	}
	error = QStringLiteral("The development no longer exists."); return false;
}

bool DaLegacyCity::FetchConstructionMaterials(DaPerson& person, quint32 id, QString& error)
{ return OperateConstruction(&person,id,true,false,error); }
bool DaLegacyCity::ContributeConstructionLabor(DaPerson& person, quint32 id, QString& error, bool* accepted)
{ return OperateConstruction(&person,id,true,true,error,accepted); }

bool DaLegacyCity::ChangeRoadConstruction(DaPerson& person, quint32 id, quint8 process, QString& error)
{
	if(!ConstructionAuthority(*this,person,true,error)) return false;
	if(process!=2 && process!=3 && process!=4 && process!=5 && process!=7)
	{ error=QStringLiteral("Existing roads may be changed to dirt, asphalt, concrete, pressurized pavement or grading."); return false; }
	auto* world=dynamic_cast<DaWorldLL*>(Parent()); if(!world) return false;
	if(process==3 && world->OrbitZone()==OZONE_Inferno)
	{ error=QStringLiteral("Asphalt roads cannot be constructed in an Inferno orbit zone."); return false; }
	auto state=Snapshot();
	for(auto& dev : state.developments) if(dev.id==id)
	{
		// DpGlobeRoad::ChangeConstruction 0x652b20 leaves unfinished projects
		// intact and does not allow the grade-only development to be repaved.
		if(dev.industryId!=2 || !dev.components.isEmpty() || !dev.constructed)
		{ error=QStringLiteral("Finish this road's current construction before changing its pavement."); return false; }
		if(dev.constructionProcessId==5)
		{ error=QStringLiteral("A graded area cannot be repaved; build a road on its prepared terrain."); return false; }
		if(dev.constructionProcessId==process)
		{ error=QStringLiteral("This road already uses that construction process."); return false; }
		const quint8 expectedProcess=dev.constructionProcessId, expectedLevels=dev.completedLevels;
		dev.constructionProcessId=process; dev.materialId=process==3 ? 23 : 76;
		dev.completedLevels=0; dev.targetLevels=1; dev.constructed=false; dev.halted=false;
		const quint32 pavement=AuLegacyCityCatalogue::RoadPavementQuantity(dev.roadLengthMeters*FeetPerMeter(),dev.roadWidthMeters*FeetPerMeter());
		InitializeConstruction(dev,qMin(quint32(65535),(pavement+1)/2+4),ConstructionEnvironment(*world,dev.location));
		return ReplaceFinishedDevelopment(dev,expectedProcess,expectedLevels,error);
	}
	error=QStringLiteral("The road no longer exists."); return false;
}

AuLegacyCity::CitizenConstructionFacts DaLegacyCity::GatherCitizenConstructionFacts(
	const AuLegacyCity::State& state, const AuLegacyCity::Development& development, qint64 now, bool materialsOnly) const
{
	AuLegacyCity::CitizenConstructionFacts facts;
	facts.developmentId = development.id;
	facts.completedLevels = development.completedLevels;
	facts.processId = development.constructionProcessId;
	facts.now = now;
	auto* world = dynamic_cast<DaWorldLL*>(Parent());
	if(!world || development.demolished || development.components.isEmpty() || development.halted
		|| (!materialsOnly && !development.citizenWorker))
	{ facts.blocked = true; return facts; }
	facts.blocked = !materialsOnly && ConstructionThreat(*world, Id(), development, state.controllingEmpire) != 0;
	facts.ringworld = OnRingworld();
	facts.environment = ConstructionEnvironment(*world, development.location);
	AuGlobeLL cityLL;
	if(world->LLOfLoc(cityLL, Location()))
		world->QueryNaturalResources(facts.natural, world->ResourceZone(cityLL.Longitude()));
	// DpDevelopment::OperatedByCitizens 0x651540 fetches only after current
	// site/material labor is exhausted. Ground items precede natural/stock.
	if(materialsOnly || (!development.laborRemaining && !facts.blocked))
		facts.groundItems = AuLegacyConstructionMaterials::ItemsAtSite(*world, Id(), development);
	quint32 possibleLabor = 4;
	for(const auto& component : development.components)
		if(component.effect == CE_DecreasesTime) possibleLabor += component.commodityId == 3 ? 1 : 5;
	if(!materialsOnly && development.completedLevels + 1 < development.targetLevels && development.laborRemaining <= possibleLabor)
	{
		auto next = development;
		++next.completedLevels;
		facts.nextPreparation = SitePreparationLabor(*world, next);
	}
	return facts;
}

bool DaLegacyCity::ApplyCitizenConstruction(AuLegacyCity::State& state, AuLegacyCity::Development& development,
	AuItemHash& inventory, AuLegacyCity::ProductionState& production,
	const AuLegacyCity::CitizenConstructionFacts& facts, bool& physicalChanged,
	AuLegacyConstructionMaterials::Batch& materials, bool performLabor) const
{
	physicalChanged = false;
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue || (performLabor && (facts.blocked || !development.citizenWorker)) || development.id != facts.developmentId
		|| development.completedLevels != facts.completedLevels || development.constructionProcessId != facts.processId
		|| development.demolished || development.halted || development.components.isEmpty()) return false;
	// A completed military base adds a flag. Reject work before consuming
	// world materials when the bounded state cannot accept that completion.
	if(performLabor && development.industryId == 10 && state.flags.size() >= 4096) return false;
	AuLegacyCity::ProductionProcess components;
	components.buildingTechnology = 0;
	components.laborNeeded = development.laborRemaining;
	for(const auto& source : development.components)
	{
		AuLegacyCity::ProductionComponent component;
		component.commodityId = source.commodityId;
		component.effect = source.effect;
		component.quality = source.quality;
		component.required = source.quantity;
		component.applied = source.applied;
		components.components.append(component);
	}
	bool changed = false;
	if(!performLabor || !components.laborNeeded)
	{
		changed = materials.Fetch(facts.groundItems, *catalogue, components);
		changed |= AuLegacyCity::FetchConstructionMaterials(*catalogue, components, inventory, production,
			facts.natural, facts.ringworld);
	}
	components.laborNeeded = qMin(65535U, components.laborNeeded);
	if(performLabor && components.laborNeeded)
	{
		const auto electricity = COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(3)));
		const bool hasPower = facts.ringworld || inventory.RawCount(electricity);
		bool missingTool = false;
		for(const auto& component : components.components)
			if(component.effect == CE_Required && !component.required && !component.applied)
				if(component.commodityId == 3 ? !hasPower
					: !AuLegacyCity::HasProductionTool(*catalogue, component.commodityId, production, inventory)) missingTool = true;
		if(!missingTool)
		{
			quint32 labor = 4;
			for(auto& component : components.components)
			{
				if(component.effect == CE_DecreasesTime)
				{
					const bool tool = component.commodityId == 3 ? hasPower
						: AuLegacyCity::UseProductionTool(*catalogue, component.commodityId, production, inventory);
					component.applied = tool ? 1 : 0;
					if(tool) labor += component.commodityId == 3 ? 1 : 5;
				}
				else if(component.effect == CE_Required && !component.required && !component.applied)
					component.applied = component.commodityId == 3 ? quint32(hasPower)
						: quint32(AuLegacyCity::UseProductionTool(*catalogue, component.commodityId, production, inventory));
			}
			components.laborNeeded -= qMin(components.laborNeeded, labor);
			changed = true;
		}
	}
	const auto laborBefore = development.laborRemaining;
	development.laborRemaining = qMin(65535U, components.laborNeeded);
	for(int index = 0; index < development.components.size(); ++index)
	{
		auto& destination = development.components[index];
		const auto& source = components.components[index];
		changed |= destination.quality != source.quality || destination.applied != source.applied;
		destination.quality = source.quality;
		destination.applied = source.applied;
	}
	if(performLabor && !development.laborRemaining && ComponentsComplete(development))
	{
		++development.completedLevels;
		ConstructedDecks(development);
		if(development.completedLevels < development.targetLevels)
			InitializeConstruction(development, facts.nextPreparation, facts.environment);
		else development.components.clear();
		if(development.industryId == 10)
		{
			AuLegacyCity::Flag flag;
			flag.empire = state.controllingEmpire;
			flag.location = development.location;
			flag.level = 30;
			flag.capturedAt = facts.now;
			state.flags.append(flag);
		}
		state.lastEmployment = 0;
		changed = physicalChanged = true;
	}
	return changed || laborBefore != development.laborRemaining;
}

bool DaLegacyCity::OperateConstruction(DaPerson* person, quint32 id, bool fetch, bool labor, QString& error, bool* accepted)
{
	AuLegacyFinanceTransfer transfer(person);
	if(accepted) *accepted=false;
	if(person && !ConstructionAuthority(*this,*person,false,error)) return false;
	auto* world = dynamic_cast<DaWorldLL*>(Parent()); if(!world) return false;
	const auto before = Snapshot();
	auto original = std::find_if(before.developments.cbegin(),before.developments.cend(),[id](const AuLegacyCity::Development& dev){return dev.id==id;});
	if(original == before.developments.cend()) { error = QStringLiteral("The development no longer exists."); return false; }
	if(person)
	{
		AuMatrix4x4 actor;
		if(!person->GetTransformRelativeToParent(actor,world)) return false;
		AuMatrix4x4 site;
		world->BuildingConstructionTransform(world->LocOfLL(original->location,original->altitudeMSL),original->facingRadians,nullptr,AuPoint(),site);
		AuGlobeLL actorLL;
		if(!world->LLOfLoc(actorLL,actor.Translation())) return false;
		const AuPoint local=site.UnProject(world->LocOfLL(actorLL,original->altitudeMSL));
		const double actorAltitude=world->AltitudeMSL(actor.Translation());
		const auto* recipe=ConstructionRecipe(original->industryId,original->constructionProcessId);
		const double halfWidth=original->industryId==2 ? original->roadWidthMeters*FeetPerMeter()/2
			: original->industryId==129 ? original->biodomeRadiusMeters*FeetPerMeter()+AuLegacyBd::Biodome::ContainmentMargin
			: recipe ? recipe->buildRadius*FeetPerMeter()+2.828427 : 0;
		const double halfLength=original->industryId==2 ? original->roadLengthMeters*FeetPerMeter()/2 : halfWidth;
		const double baseAltitude=original->industryId==2 ? original->roadStartAltitudeMSL+(original->roadEndAltitudeMSL-original->roadStartAltitudeMSL)
			*qBound(0.0,(local.y+halfLength)/(2*halfLength),1.0) : original->altitudeMSL;
		const double height=actorAltitude-baseAltitude;
		if(std::abs(local.x)>halfWidth || std::abs(local.y)>halfLength || (original->industryId==129 && std::hypot(local.x,local.y)>halfWidth)
			|| height<(original->minimumDeck-1)*AuLegacyBd::DeckHeight
			|| height>(original->industryId==129 ? AuLegacyBd::Biodome::RoofHeight(original->biodomeRadiusMeters*FeetPerMeter(),std::hypot(local.x,local.y))
				: (original->maximumDeck+2)*AuLegacyBd::DeckHeight))
		{ error.clear(); return false; } // Leaving the site stops continuous work silently.
	}
	const int threat=ConstructionThreat(*world,Id(),*original,person ? person->EmpireId() : EmpireId());
	if(threat)
	{ error=threat==1 ? QStringLiteral("STOP! Wild Animal Present") : QStringLiteral("STOP! Enemy Unit Present"); return false; }
	quint32 nextPreparation=1, possibleLabor=4;
	for(const auto& component : original->components)
		if(component.effect==CE_DecreasesTime) possibleLabor+=component.commodityId==3 ? 1 : AuItem::GetLaborBoost();
	if(original->completedLevels+1<original->targetLevels && original->laborRemaining<=possibleLabor)
	{
		// CompleteConstruction 0x510830 repeats the original preparation
		// calculation before ContinueConstruction starts each queued level.
		// Only sample when this effort could finish the current level.
		auto next=*original; ++next.completedLevels; nextPreparation=SitePreparationLabor(*world,next);
	}
	AuGlobeLL cityLL;
	if(!world->LLOfLoc(cityLL,Location())) return false;
	AuZoneResource natural; world->QueryNaturalResources(natural,world->ResourceZone(cityLL.Longitude()));
	const quint8 environment = ConstructionEnvironment(*world,original->location);
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn(); if(!catalogue) return false;
	const bool ringworld = OnRingworld();
	quint16 completionRecipe = 0;
	if(labor && AuLegacyCity::ManufacturingSlots(original->industryId, original->completedLevels + 1)
		> AuLegacyCity::ManufacturingSlots(original->industryId, original->completedLevels))
	{
		AuMatrix4x4 placement;
		const auto center = world->LocOfLL(original->location, original->altitudeMSL);
		world->BuildingConstructionTransform(center, original->facingRadians, nullptr, AuPoint(), placement);
		const auto* construction = ConstructionRecipe(original->industryId, original->constructionProcessId);
		const double radius = construction ? construction->buildRadius * FeetPerMeter() : 0;
		completionRecipe = AuLegacyCity::DefaultProductionRecipe(*catalogue, original->industryId, [&](quint16 historical) {
			return world->TestRockResource(placement, center, AuPoint(radius, radius, radius),
				COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(historical))));
		});
	}
	const auto groundItems=fetch ? AuLegacyConstructionMaterials::ItemsAtSite(*world,Id(),*original) : QVector<DaItem*>();
	QHash<quint16,AuGearPos> ready;
	if(person && labor)
		for(const auto& component : original->components)
			if(component.commodityId != 3 && (component.effect == CE_DecreasesTime
				|| (component.effect == CE_Required && !component.quantity)))
				ready.insert(component.commodityId,person->FindReadyItem(
					COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(component.commodityId)))));
	AuLegacyConstructionMaterials::Batch materials;
	QVector<AuGearPos> wornGear;
	double salary=0,salaryReward=0;
	bool contributed=false;
	QWriteLocker lock(&m_CityLock);
	AuLegacyCity::State state=m_City;
	auto found = std::find_if(state.developments.begin(),state.developments.end(),[id](const AuLegacyCity::Development& dev){return dev.id==id;});
	if(found == state.developments.end() || found->components.isEmpty() || found->demolished)
	{ return false; }
	if(found->halted) { return false; }
	// Completing a military base appends its flag. Reject before touching
	// loose world cargo when that append would exceed the state codec bound.
	if(labor && found->industryId == 10 && state.flags.size() >= 4096)
	{ error = QStringLiteral("This city has reached its flag capacity; this military base cannot be completed."); return false; }
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	AuItemHash inventory;
	AuLegacyCity::ProductionState production;
	AuLegacyCity::AccountingState accounting;
	if(!AuLegacyCity::ReadCityInventory(state.inventory,*catalogue,inventory,error)
		|| !AuLegacyCity::DecodeProductionState(*catalogue,state.manufacturing,production,error)
		|| !AuLegacyCity::DecodeAccounting(state.accounting,accounting,error)) return false;
	if(found->completedLevels!=original->completedLevels || found->constructionProcessId!=original->constructionProcessId
		|| (found->completedLevels+1<found->targetLevels && found->laborRemaining<=possibleLabor && original->laborRemaining>possibleLabor))
	{ return false; }
	if(accepted) *accepted=true;
	// Keep the one-second cadence independently of clock-second boundaries.
	// Allow a quarter second of arrival jitter, but advance the nominal effort
	// time by a full second so repeated early packets cannot speed up work.
	const qint64 previousLabor = person && labor ? m_ConstructionLaborTimes.value(person->Id().GetID(), 0) : 0;
	if(person && labor && now + 250 < previousLabor + 1000)
	{ error.clear(); return false; }
	auto& dev = *found; bool changed = false, physicalChanged = false;
	AuLegacyCity::ProductionProcess components;
	components.laborNeeded=dev.laborRemaining;
	for(const auto& source : dev.components)
	{
		AuLegacyCity::ProductionComponent component;
		component.commodityId=source.commodityId; component.effect=source.effect;
		component.quality=source.quality; component.required=source.quantity; component.applied=source.applied;
		components.components.append(component);
	}
	if(fetch)
	{
		// ManufactureFetch(...,99) -> 0x516200: loose items/direct contents,
		// then city-zone natural resources, ringworld power, vehicles and stock.
		// Carried cargo is not a source; personal equipment supplies labor tools.
		materials.Fetch(groundItems,*catalogue,components);
		AuLegacyCity::FetchConstructionMaterials(*catalogue,components,inventory,production,natural,ringworld);
	}
	components.laborNeeded=qMin(65535U,components.laborNeeded);
	if(labor && components.laborNeeded)
	{
		bool missingTool = false; int boost = 0;
		const bool hasPower=ringworld || inventory.RawCount(COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(3))));
		// Verify every unsatisfied required modifier before wearing any tool.
		// DpDevelopment::ApplyConstructionLabor, AuAtom13 0x64b390.
		for(const auto& component : components.components)
		{
			if(component.effect != CE_Required || component.required || component.applied) continue;
			const bool available = component.commodityId == 3 ? hasPower : ready.value(component.commodityId).IsValid()
				|| AuLegacyCity::HasProductionTool(*catalogue,component.commodityId,production,inventory);
			if(!available) missingTool = true;
		}
		if(!missingTool)
		for(auto& component : components.components)
		{
			if((component.effect != CE_Required || component.required) && component.effect != CE_DecreasesTime) continue;
			if(component.effect == CE_Required && component.applied) continue;
			bool tool=false;
			if(component.commodityId == 3) tool=hasPower;
			else if(ready.value(component.commodityId).IsValid())
			{ tool=true; wornGear.append(ready.value(component.commodityId)); }
			else tool=AuLegacyCity::UseProductionTool(*catalogue,component.commodityId,production,inventory);
			component.applied=tool ? 1 : 0;
			if(component.effect == CE_DecreasesTime && component.applied)
				boost+=component.commodityId == 3 ? 1 : AuItem::GetLaborBoost();
		}
		if(!missingTool)
		{
			components.laborNeeded-=qMin(components.laborNeeded,quint32(4+boost));
			contributed=true;
			if(person)
			{
				dev.lastPlayerLabor=now;
				// DaWorldLL::ApplyConstructionLabor 0x50df40 pays one crono
				// per successful effort, from public works or the citizen bank.
				const bool governmentWorks=dev.industryId == 2 || (dev.industryId >= 10 && dev.industryId <= 12)
					|| dev.industryId == 123 || dev.industryId == 128;
				const float balance=float(governmentWorks ? accounting.accounts.government : accounting.accounts.citizens);
				const float gross=qMin(1.0f,balance);
				salaryReward=double(1.0f-gross);
				if(governmentWorks) accounting.accounts.Debit(AuLegacyCity::GovernmentExpense::Works,double(gross));
				else accounting.accounts.Debit(AuLegacyCity::CitizenExpense::Salary,double(gross));
				const float tax=(accounting.taxes.income/100.0f)*gross;
				accounting.accounts.Credit(AuLegacyCity::GovernmentIncome::IncomeTax,double(tax),accounting.taxes.hasCapital);
				salary=double(gross-tax);
			}
		}
	}
	dev.laborRemaining=qMin(65535U,components.laborNeeded);
	for(int index=0;index<dev.components.size();++index)
	{
		dev.components[index].quality=components.components[index].quality;
		dev.components[index].applied=components.components[index].applied;
	}
	if(labor && !dev.laborRemaining && ComponentsComplete(dev))
	{
		const auto priorProcessCount = AuLegacyCity::ManufacturingSlots(dev.industryId, dev.completedLevels);
		++dev.completedLevels; ConstructedDecks(dev); changed = physicalChanged = true;
		if(dev.completedLevels < dev.targetLevels) InitializeConstruction(dev,nextPreparation,environment);
		else dev.components.clear();
		// CompleteConstruction 0x510830 creates newly available process slots
		// immediately, including the first mine/well's site-specific default.
		for(quint16 slot = priorProcessCount; slot < AuLegacyCity::ManufacturingSlots(dev.industryId, dev.completedLevels); ++slot)
		{
			if(std::any_of(production.processes.cbegin(), production.processes.cend(), [&dev, slot](const AuLegacyCity::ProductionProcess& item) {
				return item.developmentId == dev.id && item.slot == slot; })) continue;
			quint16 recipeId = completionRecipe;
			const auto first = std::find_if(production.processes.cbegin(), production.processes.cend(), [&dev](const AuLegacyCity::ProductionProcess& item) {
				return item.developmentId == dev.id && item.slot == 0; });
			if(dev.selectedManufacturingRecipe) recipeId = dev.selectedManufacturingRecipe;
			else if(first != production.processes.cend()) recipeId = first->processId;
			const auto* recipe = catalogue->ManufacturingProcess(AuLegacyCity::ManufacturingProcessId(recipeId));
			if(!recipe) continue;
			AuLegacyCity::ProductionProcess process;
			process.developmentId = dev.id; process.slot = slot; process.enabled = dev.manufacturingCitizenEnabled;
			if(AuLegacyCity::InitializeProductionProcess(*catalogue, process, recipeId,
				AuLegacyCity::TechnologyOfProduct(dev.industryId, dev.technology, recipe->commodity.value, state.developments)))
				production.processes.append(process);
		}
		if(dev.industryId == 10)
		{
			AuLegacyCity::Flag flag; flag.empire=state.controllingEmpire; flag.location=dev.location;
			flag.level=30; flag.capturedAt=now; state.flags.append(flag);
		}
		state.lastEmployment=0;
	}
	QByteArray checked;
	if(!AuLegacyCity::WriteCityInventory(inventory,state.inventory,error)
		|| !AuLegacyCity::EncodeProductionState(*catalogue,production,state.manufacturing,error)
		|| !AuLegacyCity::EncodeAccounting(accounting,state.accounting,error)
		|| !AuLegacyCity::EncodeState(state,checked,error)) return false;
	QByteArray previous;
	if(!AuLegacyCity::EncodeState(m_City,previous,error)) return false;
	changed=previous!=checked;
	if(!materials.Commit(error)) return false;
	if(changed)
	{
		m_City=state; ++m_CityRevision;
		if(person && contributed)
		{
			m_ConstructionLaborTimes.insert(person->Id().GetID(), qMax(now, previousLabor + 1000));
			if(m_ConstructionLaborTimes.size()>4096)
				for(auto it=m_ConstructionLaborTimes.begin();it!=m_ConstructionLaborTimes.end();)
					if(now-it.value()>60000) it=m_ConstructionLaborTimes.erase(it); else ++it;
		}
	}
	lock.unlock();
	materials.Finalize();
	if(person)
	{
		for(const auto& position : wornGear) person->UseGearItem(position,IPS_Item);
		if(salaryReward>0) person->Reward(salaryReward,REW_Salary);
		if(salary>0) person->CreditAccount(salary);
	}
	if(changed) SetModified();
	if(physicalChanged) world->InvalidateEnvironmentRecursive();
	return changed;
}

bool DaLegacyCity::TestRoadConnection(const DaLegacyCity& other) const
{
	if(this == &other) return true;
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world || other.Parent() != world) return false;
	const auto mine = Snapshot(), theirs = other.Snapshot();
	const auto eligible = [](const AuLegacyCity::Development& road) {
		// FINDDEV 0x1a excludes unfinished roads and landscaping, but includes
		// both road strips and slabs (DpRoad::TestFindFilter 0x655fe0).
		return road.industryId == 2 && !road.demolished && road.components.isEmpty()
			&& road.constructionProcessId != 5 && road.constructionProcessId != 74
			&& road.constructionProcessId != 80 && road.constructionProcessId != 81;
	};
	const auto ends = [world](const AuLegacyCity::Development& road, AuGlobeLLF& first, AuGlobeLLF& last) {
		AuMatrix4x4 placement;
		world->BuildingConstructionTransform(world->LocOfLL(road.location, road.altitudeMSL), road.facingRadians, nullptr, AuPoint(), placement);
		const double halfLength = road.roadLengthMeters * FeetPerMeter() / 2;
		return world->LLOfLoc(first, placement.Project(AuPoint(0, -halfLength, 0)))
			&& world->LLOfLoc(last, placement.Project(AuPoint(0, halfLength, 0)));
	};
	for(const auto& road : mine.developments)
	{
		if(!eligible(road)) continue;
		AuGlobeLLF first, last;
		if(!ends(road, first, last)) continue;
		for(const auto& target : theirs.developments)
		{
			if(!eligible(target)) continue;
			AuGlobeLLF targetFirst, targetLast;
			if(ends(target, targetFirst, targetLast)
				&& (RoadContains(*world, target, targetFirst, targetLast, first, 0)
					|| RoadContains(*world, target, targetFirst, targetLast, last, 0))) return true;
		}
	}
	return false;
}

void DaLegacyCity::Tock(const AuTime& last, const AuTime& now, const AuScripts& scripts, DtTockTransactions& transactions)
{
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker()) return;
	DaAtom::Tock(last,now,scripts,transactions);
	if(IsDead() || !last.IsNewPeriod(now,1000,Id().GetID())) return;
	TockEconomy(last.MSecsSinceEpoch(), QDateTime::currentMSecsSinceEpoch());
}

void DaLegacyCity::SetName(const QString& name)
{
	if(!g_Au.IsLegacyGeneration()) return;
	QWriteLocker lock(&m_CityLock);
	if(m_City.name == name) return;
	auto candidate = m_City;
	candidate.name = name;
	QByteArray checked; QString error;
	if(!AuLegacyCity::EncodeState(candidate, checked, error)) { qWarning() << error; return; }
	// A rename changes only the name. Publishing an earlier full snapshot here
	// would overwrite concurrent bank, levy, import or population transactions.
	m_City.name = name;
	++m_CityRevision;
	lock.unlock(); SetModified();
}

bool DaLegacyCity::ApplyPayload(const QByteArray& bytes, QString& error)
{
	AuLegacyCity::State state;
	if(!g_Au.IsLegacyGeneration() || !AuLegacyCity::DecodeState(bytes, state, error) || !ValidateDesigns(state, error)) return false;
	QWriteLocker lock(&m_CityLock);
	state.lastVisit = qMax(state.lastVisit, m_City.lastVisit);
	m_City = state; ++m_CityRevision;
	return true;
}

bool DaLegacyCity::ValidateStateData(const QByteArray& bytes, QString& error)
{
	AuLegacyCity::State state;
	return AuLegacyCity::DecodeState(bytes, state, error) && ValidateDesigns(state, error);
}

void DaLegacyCity::operator>>(QDataStream& out) const
{
	DaAtom::operator>>(out);
	QByteArray bytes; QString error;
	if(!AuLegacyCity::EncodeState(Snapshot(), bytes, error)) out.setStatus(QDataStream::WriteFailed);
	WriteEnvelope(out, bytes);
}
void DaLegacyCity::operator<<(QDataStream& in)
{
	DaAtom::operator<<(in);
	QByteArray bytes; QString error;
	if(!ReadEnvelope(in, bytes) || !ApplyPayload(bytes, error)) in.setStatus(QDataStream::ReadCorruptData);
}
void DaLegacyCity::TxUpdate(QDataStream& out, const AuTime& lastUpdate,
	const AuElapsedTimer& lastTx, const DaPerson& sceneAtom)
{
	DaAtom::TxUpdate(out, lastUpdate, lastTx, sceneAtom);
	auto state = Snapshot();
	// Public physical state is replicated. Accounts, inventory and report history
	// remain on the authority; later report requests must authorize their reader.
	state.inventory.clear(); state.manufacturing.clear(); state.accounting.clear(); state.reportHistory.clear();
	QByteArray bytes; QString error;
	if(!AuLegacyCity::EncodeState(state, bytes, error)) out.setStatus(QDataStream::WriteFailed);
	WriteEnvelope(out, bytes);
}
void DaLegacyCity::RxUpdate(QDataStream& in, bool firstUpdate)
{
	Q_UNUSED(firstUpdate);
	QByteArray bytes; QString error;
	if(!ReadEnvelope(in, bytes) || !ApplyPayload(bytes, error)) in.setStatus(QDataStream::ReadCorruptData);
}
bool DaLegacyCity::Refresh(DaAtom* counterpart, AuAtomRenderInfo* ari)
{
	auto* city = dynamic_cast<DaLegacyCity*>(counterpart);
	if(!city) return false;
	DaAtom::Refresh(counterpart, ari);
	auto state = city->Snapshot();
	{ QWriteLocker lock(&m_CityLock); state.lastVisit = qMax(state.lastVisit, m_City.lastVisit); m_City = state; ++m_CityRevision; }
	if(ari)
	{
		{
			QMutexLocker lock(&m_Runtime->lock);
			if(!m_Runtime->asynchronous)
			{
				m_Runtime->asynchronous = true;
				m_Runtime->builtRevision = 0; // Synchronous results have no prepared graphics.
			}
		}
		UpdateSiteTerrain(*ari);
	}
	return false;
}

void DaLegacyCity::Insert(QSqlDatabase& db) const { Store(db, true); }
void DaLegacyCity::Update(QSqlDatabase& db) const { Store(db, false); }
void DaLegacyCity::Store(QSqlDatabase& db, bool insert) const
{
	if(IsDead() || !g_Au.IsLegacyGeneration()) return;
	const auto state = Snapshot(); QByteArray bytes; QString error;
	if(!AuLegacyCity::EncodeState(state, bytes, error)) { qWarning() << error; return; }
	const AuAtomPos pos(Pos()); const auto loc = pos.Translation(), rot = pos.Rotation();
	QSqlQuery query(db);
	query.prepare(insert
		? QStringLiteral("INSERT INTO a_LegacyCity(id,idp,locX,locY,locZ,rotX,rotY,rotZ,timeCreate,timeModified,timeTock,timeDeath,name,controllingEmpire,allegianceEmpire,population,satisfaction,stateVersion,stateData,reportHistory) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)")
		: QStringLiteral("UPDATE a_LegacyCity SET idp=?,locX=?,locY=?,locZ=?,rotX=?,rotY=?,rotZ=?,timeModified=?,timeTock=?,timeDeath=?,name=?,controllingEmpire=?,allegianceEmpire=?,population=?,satisfaction=?,stateVersion=?,stateData=?,reportHistory=? WHERE id=?"));
	if(insert) query.addBindValue(Id().GetID());
	query.addBindValue(pos.ParentId().GetID());
	for(double v : {loc.x, loc.y, loc.z, rot.x, rot.y, rot.z}) query.addBindValue(FuzzyClamp(v));
	if(insert) query.addBindValue(CreatedTime().UTC());
	query.addBindValue(ModifyTime().UTC()); query.addBindValue(TockTime().UTC()); query.addBindValue(DeathTime().UTC());
	query.addBindValue(state.name); query.addBindValue(state.controllingEmpire); query.addBindValue(state.allegianceEmpire);
	query.addBindValue(state.population); query.addBindValue(state.satisfaction); query.addBindValue(AuLegacyCity::StateVersion);
	query.addBindValue(bytes);
	// A null QByteArray binds as SQL NULL, but reportHistory requires a non-null bytea.
	query.addBindValue(state.reportHistory.isNull() ? QByteArray("") : state.reportHistory);
	if(!insert) query.addBindValue(Id().GetID());
	if(!query.exec()) qWarning() << "Legacy city persistence failed:" << Id() << query.lastError().text();
	else if(!insert && !query.numRowsAffected()) Insert(db);
}

bool DaLegacyCity::LoadSubatoms(AuEther& ether, QSqlDatabase& db, const AuId& world, QString* error)
{
	if(!g_Au.IsLegacyGeneration()) { if(error) *error = QStringLiteral("Legacy city loader called for a Modern universe."); return false; }
	QSqlQuery query(db);
	query.prepare(QStringLiteral("SELECT id,locX,locY,locZ,rotX,rotY,rotZ,timeCreate,timeModified,timeTock,timeDeath,stateVersion,stateData,name,controllingEmpire,allegianceEmpire,population,satisfaction,reportHistory FROM a_LegacyCity WHERE idp=?"));
	query.addBindValue(world.GetID());
	if(!query.exec()) { if(error) *error = query.lastError().text(); return false; }
	QVector<QSharedPointer<DaLegacyCity>> pending;
	while(query.next())
	{
		QSharedPointer<DaLegacyCity> city(new DaLegacyCity);
		QString failure;
		if(!AuLegacyCity::SupportedStateVersion(query.value(11).toUInt())
			|| !city->ApplyPayload(query.value(12).toByteArray(), failure))
		{ if(error) *error = QStringLiteral("Invalid saved legacy city %1: %2").arg(query.value(0).toString(), failure); return false; }
		const auto state = city->Snapshot();
		if(state.name != query.value(13).toString() || state.controllingEmpire != query.value(14).toUInt()
			|| state.allegianceEmpire != query.value(15).toUInt() || state.population != query.value(16).toInt()
			|| state.satisfaction != query.value(17).toInt() || state.reportHistory != query.value(18).toByteArray())
		{ if(error) *error = QStringLiteral("Legacy city indexed summary disagrees with its state envelope."); return false; }
		city->SetId(AuId(query.value(0).toUInt()));
		city->SetPosAndOrient(AuAtomPos(world, query.value(1).toDouble(), query.value(2).toDouble(), query.value(3).toDouble(),
			query.value(4).toDouble(), query.value(5).toDouble(), query.value(6).toDouble()));
		city->SetCreatedTime(query.value(7).toDateTime().toUTC()); city->SetModifyTime(query.value(8).toDateTime().toUTC());
		city->SetTockTime(query.value(9).toDateTime().toUTC()); city->SetDeathTime(query.value(10).toDateTime().toUTC());
		// The original loader used the parent world's creation time when no
		// visit had ever been recorded, and clamped future saved visits to now.
		const qint64 now = AuTime::CurrentMSecsSinceEpoch();
		{
			QWriteLocker lock(&city->m_CityLock);
			// DaCity's constructor 0x26b710 initializes these caches, and
			// LoadSubatoms 0x29c8b0 restores neither them nor the vehicle list.
			// ApplyPayload/Refresh deliberately retain them during live transfers.
			AuLegacyCity::ProductionState production;
			AuLegacyCity::AccountingState accounting;
			if(!AuLegacyCity::DecodeProductionState(*AuLegacyCityCatalogue::BuiltIn(), city->m_City.manufacturing, production, failure)
				|| !AuLegacyCity::DecodeAccounting(city->m_City.accounting, accounting, failure))
			{ if(error) *error = QStringLiteral("Invalid saved legacy city economy: %1").arg(failure); return false; }
			production.vehicles.clear();
			accounting.taxes = AuLegacyCity::TaxRates();
			accounting.powerRequested = 0;
			accounting.tributeSentToday = false;
			accounting.unemployed = std::numeric_limits<quint32>::max();
			accounting.foodPerMeal = 0;
			city->m_City.lastEmployment = 0;
			// Original development streams omit the constructor's employee cache.
			for(auto& development : city->m_City.developments) development.employees = 0;
			// The durable meal timestamp is clamped to the load time, matching
			// the original loader; funded work and ledger activity survive.
			city->m_City.lastPopulationCycle = qMin(city->m_City.lastPopulationCycle, now);
			accounting.lastPopulationCycle = city->m_City.lastPopulationCycle;
			if(!AuLegacyCity::EncodeProductionState(*AuLegacyCityCatalogue::BuiltIn(), production, city->m_City.manufacturing, failure)
				|| !AuLegacyCity::EncodeAccounting(accounting, city->m_City.accounting, failure))
			{ if(error) *error = QStringLiteral("Could not initialize legacy city economy: %1").arg(failure); return false; }
			if(!city->m_City.lastVisit)
			{
				if(const DaAtom* parent = ether.FindAtom(world))
					city->m_City.lastVisit = parent->CreatedTime().MSecsSinceEpoch();
			}
			else if(city->m_City.lastVisit > now) city->m_City.lastVisit = now;
		}
		pending.append(city);
	}
	// Ownership passes to the ether only after every row has validated.
	for(auto& item : pending)
	{
		auto* city = new DaLegacyCity(*item);
		city->ConnectSignals(); ether.AddAtom(city); city->SetNormal();
	}
	return true;
}

bool DaLegacyCity::RoughExtents(AuBox& extents) const
{
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world) return false;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	AuPoint minimum, maximum; bool any = false;
	const auto worldToCity = Transform().Inverse();
	for(const auto& building : m_Runtime->buildings)
	{
		AuMatrix4x4 transform;
		world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
			building.state.facingRadians, nullptr, AuPoint(), transform);
		AuBox box(Point(building.mesh.minimum), Point(building.mesh.maximum)); box.Transform(worldToCity * transform);
		AuPoint a, b; box.GetMinMax(a, b);
		minimum = any ? minimum.Min(a) : a; maximum = any ? maximum.Max(b) : b; any = true;
	}
	if(any) extents.SetFromMinMax(minimum, maximum);
	return any;
}

bool DaLegacyCity::IsPathBlocked(const AuPoint& from, const AuPoint& to) const
{
	AuPoint hit, normal;
	return const_cast<DaLegacyCity*>(this)->IsPathBlocked(from, to, hit, normal, nullptr);
}

bool DaLegacyCity::IsSightTarget(const DaAtom* observer) const
{ return g_Au.IsLegacyGeneration() && observer != this; }

DaAtom* DaLegacyCity::SightTarget(const DaAtom* observer, const AuPoint& eye,
	const AuMatrix4x4& model, DaAtom* best, AuPoint& sighted, unsigned int& part,
	bool& shield, AuPoint* surfaceNormal) const
{
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world || !IsSightTarget(observer)) return best;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	const auto worldToObserver = model * Transform().Inverse();
	for(const auto& building : m_Runtime->buildings)
	{
		AuMatrix4x4 placement;
		world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
			building.state.facingRadians, nullptr, AuPoint(), placement);
		const auto transform = worldToObserver * placement;
		const auto source = transform.UnProject(eye);
		const auto target = transform.UnProject(sighted);
		const AuBox box(Point(building.mesh.minimum), Point(building.mesh.maximum));
		AuPoint first, second;
		if(!box.IsPointInside(source) && !box.IsPointInside(target)
			&& !box.SegmentIntersection(source, target, first, second)) continue;
		for(const auto& batch : building.mesh.batches)
		{
			// Sight uses visible triangles; walking ramps are movement proxies.
			if(batch.collisionOnly || batch.doorId || batch.empireFlag || batch.depthOnly) continue;
			for(int i=0; i+2<batch.vertices.size(); i+=3)
			{
				const AuTriangle triangle(Point(batch.vertices[i].position), Point(batch.vertices[i+1].position), Point(batch.vertices[i+2].position));
				AuPoint hit;
				if(triangle.SegmentIntersection(hit, source, target) != 3) continue;
				const auto location = transform.Project(hit);
				if(location.Distance2(eye) >= sighted.Distance2(eye)) continue;
				sighted = location; part = building.state.id; shield = false; best = const_cast<DaLegacyCity*>(this);
				if(surfaceNormal) *surfaceNormal = transform.ProjectVector(Point(batch.vertices[i].normal));
			}
		}
	}
	return best;
}

bool DaLegacyCity::IsPathBlocked(const AuPoint& from, const AuPoint& to, AuPoint& hit,
	AuPoint& normal, const DaAtom* actor)
{
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world || !g_Au.IsLegacyGeneration()) return false;
	const auto* creature = dynamic_cast<const DaCreature*>(actor);
	// A gravity query can rebuild actor environment and re-enter the city.
	const AuPoint gravityUp = creature ? creature->GravityNormal() : AuPoint();
	const double standingHeight = creature ? creature->GroundHeightAboveCG() : 0;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	bool blocked = false; double closest = from.Distance2(to);
	for(const auto& building : m_Runtime->buildings)
	{
		if(!building.collision->IsValid()) continue;
		AuMatrix4x4 transform;
		world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
			building.state.facingRadians, nullptr, AuPoint(), transform);
		AuBox body(AuPoint(-.01,-.01,-.01), AuPoint(.01,.01,.01));
		if(actor)
		{
			if(creature) creature->LegacyCollisionExtents(body); else actor->RoughExtents(body);
			AuMatrix4x4 actorToWorld; actor->GetTransformRelativeToParent(actorToWorld, world);
			const auto actorToBuilding = transform.Inverse() * actorToWorld;
			if(creature) body = AuLegacyScGeometry::MovementBodyOffsets(body, actorToBuilding.ProjectVector(gravityUp));
			else for(auto& corner : body.m_fCorners) corner = actorToBuilding.ProjectVector(corner);
		}
		AuPoint localHit, localNormal;
		if(building.collision->IsBodyPathBlocked(transform.UnProject(from), transform.UnProject(to), body, localHit, localNormal))
		{
			// Native checks also see the short straight chord between updates.
			// Accept the same swept stair-to-floor route used by local movement.
			if(creature && from.Distance2(to) <= 6.561679790026246 * 6.561679790026246)
			{
				AuPoint replayNormal;
				const auto replay = building.collision->ResolveDestination(transform.UnProject(from), transform.UnProject(to),
					body, transform.UnProjectVector(gravityUp), standingHeight, true, replayNormal,
					false, nullptr, false, nullptr, true);
				if(replay.Distance2(transform.UnProject(to)) < 1e-10) continue;
			}
			const auto candidate = transform.Project(localHit);
			if(candidate.Distance2(from) <= closest)
			{ hit = candidate; normal = transform.ProjectVector(localNormal); closest = candidate.Distance2(from); blocked = true; }
		}
	}
	return blocked;
}

bool DaLegacyCity::FilterDestination(const DaAtom& actor, const AuPoint& from, AuPoint& to, const AuBox& anatomy) const
{
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world || !g_Au.IsLegacyGeneration()) return false;
	AuMatrix4x4 actorToWorld; if(!actor.GetTransformRelativeToParent(actorToWorld, world)) return false;
	const auto* creature = dynamic_cast<const DaCreature*>(&actor);
	// Match the legacy spacecraft's walking eligibility. A creature in flight,
	// a jump or a ladder transition must retain its requested vertical movement.
	const bool followGround = creature && !creature->IsJumping()
		&& !creature->IsFlying() && !creature->IsLadderPose() && !creature->InLadderWell()
		&& !creature->IsInGravityWell() && creature->IsTouchingGround();
	// DaCreature::CGHeight already subtracts CGOffset.z, so this is the
	// insertion-point-to-ground height expected by the collision resolver.
	const double standingHeight = actor.GroundHeightAboveCG();
	// These environment reads can rebuild the query and re-enter this city's
	// QueryLegacyEnvironment, so resolve them before taking the runtime lock.
	const AuPoint gravityUp = actor.GravityNormal();
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	bool changed = false;
	for(const auto& building : m_Runtime->buildings)
	{
		if(!building.collision->IsValid()) continue;
		AuMatrix4x4 transform;
		world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
			building.state.facingRadians, nullptr, AuPoint(), transform);
		const auto bodyTransform = transform.Inverse() * actorToWorld;
		const auto up = bodyTransform.ProjectVector(gravityUp);
		AuBox body(anatomy);
		if(creature) body = AuLegacyScGeometry::MovementBodyOffsets(body, up);
		else for(auto& corner : body.m_fCorners) corner = bodyTransform.ProjectVector(corner);
		AuPoint normal;
		const auto result = building.collision->ResolveDestination(transform.UnProject(from), transform.UnProject(to),
			body, up, standingHeight, followGround, normal, false, nullptr, false, nullptr, true);
		const auto destination = transform.Project(result);
		if(destination.Distance2(to) > 1e-16) { changed = true; to = destination; }
	}
	return changed;
}

void DaLegacyCity::QueryLegacyEnvironment(const DaAtom& actor, const AuPoint& location, AuQueryEnvironment& query) const
{
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world || !g_Au.IsLegacyGeneration()) return;
	// EnclosureNeeded 0x28f190 depends on the city's origin and world, without
	// a power gate. Resolve these world properties before taking the mesh lock.
	const bool undersea=world->Hydrographics() && world->AltitudeMSL(Transform().Translation())<=-32;
	const int enclosure=undersea ? 2 : world->HasBreathableAtmosphere() ? 0 : 1;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	AuMatrix4x4 actorToWorld; if(!actor.GetTransformRelativeToParent(actorToWorld, world)) return;
	for(const auto& building : m_Runtime->buildings)
	{
		AuMatrix4x4 transform;
		world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
			building.state.facingRadians, nullptr, AuPoint(), transform);
		const auto local = transform.UnProject(location);
		const auto& dev=building.state;
		const auto* recipe=ConstructionRecipe(dev.industryId,dev.constructionProcessId);
		const double radius=dev.industryId==129 ? dev.biodomeRadiusMeters*FeetPerMeter() : recipe ? recipe->buildRadius*FeetPerMeter() : 0;
		const double halfWidth=dev.industryId==2 ? dev.roadWidthMeters*FeetPerMeter()/2 : radius;
		const double halfLength=dev.industryId==2 ? dev.roadLengthMeters*FeetPerMeter()/2 : radius;
		const AuPoint surfaceLocal=transform.UnProject(world->LocOfLL(query.m_GlobeLL,dev.altitudeMSL));
		const double gradeAltitude=dev.industryId==2 ? dev.roadStartAltitudeMSL+(dev.roadEndAltitudeMSL-dev.roadStartAltitudeMSL)
			*qBound(0.0,(surfaceLocal.y+halfLength)/(2*halfLength),1.0) : dev.altitudeMSL;
		const bool curvedSite=dev.industryId==2 || dev.industryId==129;
		const AuPoint& siteLocal=curvedSite ? surfaceLocal : local;
		const double siteHeight=curvedSite ? query.m_fAltitudeMSL-gradeAltitude : local.z;
		const bool close=location.Distance2(transform.Translation())<std::pow(2*(qMax(halfWidth,halfLength)+32.8084),2);
		if(close && !dev.demolished && (dev.industryId!=129 || query.m_LegacyDevelopmentId==0)
			&& std::abs(siteLocal.x)<=halfWidth+2.828427 && std::abs(siteLocal.y)<=halfLength+2.828427
			&& (dev.industryId!=129 || std::hypot(siteLocal.x,siteLocal.y)<=radius+AuLegacyBd::Biodome::ContainmentMargin)
			&& siteHeight>=(dev.minimumDeck-1)*AuLegacyBd::DeckHeight
			&& siteHeight<(dev.industryId==129 ? AuLegacyBd::Biodome::RoofHeight(radius,std::hypot(siteLocal.x,siteLocal.y))
				: (dev.maximumDeck+2)*AuLegacyBd::DeckHeight))
		{
			query.m_Environment|=EB_LegacyCity;
			if(dev.industryId==2) query.m_Environment|=EB_Road;
			query.m_LegacyCityId=Id(); query.m_LegacyDevelopmentId=dev.id; query.m_CityEmpire=EmpireId();
			query.m_InIndustryLL=dev.location;
			if(const auto* catalogue=AuLegacyCityCatalogue::BuiltIn())
				query.m_InIndustry=INDUSTRYID(catalogue->RuntimeIndustryId(AuLegacyCity::IndustryId(dev.industryId)));
		}
		// InsideEnclosure 0x29b6a0: completed pressurized roads use a five-
		// meter margin; completed ordinary building sites use half a meter.
		// Biodomes own their separate curved roof volume and are excluded.
		const double pressureMargin=dev.industryId==2 ? 16.4041994750656 : 1.64041994750656;
		const bool pressureSite=dev.constructed && !dev.demolished && dev.industryId!=129
			&& (dev.industryId!=2 || dev.constructionProcessId==7);
		const bool withinPressureHeight=query.m_fAltitudeMSL-gradeAltitude<=48;
		const bool cityPressure=enclosure && pressureSite && withinPressureHeight && std::abs(surfaceLocal.x)<=halfWidth+pressureMargin
			&& std::abs(surfaceLocal.y)<=halfLength+pressureMargin;
		const bool domePressure=dev.industryId==129 && dev.constructed && !dev.demolished
			&& std::hypot(surfaceLocal.x,surfaceLocal.y)<radius-AuLegacyBd::Biodome::ContainmentMargin
			&& query.m_fAltitudeMSL-dev.altitudeMSL<AuLegacyBd::Biodome::RoofHeight(radius,std::hypot(surfaceLocal.x,surfaceLocal.y));
		if(close && (cityPressure || domePressure))
		{
			query.m_EnclosureId=Id(); query.m_AtmType=ATMTYPE_Standard; query.m_fAirDensity=.5f;
			query.m_Environment|=EB_Breathable;
			if(enclosure==2 || (domePressure && undersea)) query.m_Environment&=~(EB_OnWater|EB_InWater|EB_MouthUnderwater);
		}
		if(!building.collision->IsValid()) continue;
		const auto up = transform.UnProjectVector(-world->GravityDir(location));
		AuPoint floorNormal;
		const double height = building.collision->HeightAboveFloor(local, up, floorNormal);
		// HeightAboveFloor rays from the insertion point, and the creature's
		// ground height already includes its CG offset. No second shift applies.
		// The native floor query subtracts one inch, matching the collision
		// resolver's support clearance and precise IsTouchingGround test.
		const double fall = height - actor.GroundHeightAboveCG() - 1./12.;
		if(!std::isfinite(height) || height >= 100000) continue;
		const AuLegacyBd::Geometry::FloorState* room = nullptr;
		for(const auto& floor : building.mesh.floors)
			if(floor.boundary.containsPoint(QPointF(local.x, local.y), Qt::OddEvenFill)
				&& local.z >= floor.deck * AuLegacyBd::DeckHeight - .25
				&& local.z < (floor.deck + 1) * AuLegacyBd::DeckHeight) { room = &floor; break; }
		if(!room && fall > query.m_FallDist + d_GroundTolerance) continue;
		query.m_FallDist = fall;
		query.m_Environment |= EB_LegacyCityFloor;
		query.m_fCeilingDist = std::min(double(query.m_fCeilingDist), building.collision->HeightOfCeiling(local, up));
		query.m_terNormal = actorToWorld.UnProjectVector(transform.ProjectVector(floorNormal));
		if(fall < d_GroundTolerance) query.m_OnGround = room ? OG_HardFloor : OG_Pavement;
		const auto* cell = PathCell(building.paths, local);
		if(cell && (cell->stairs || (cell->flags & 0xff00))) query.m_Environment |= EB_Stairs;
		// ConnectBelow carries the well into its bottom landing as flag 4.
		if(cell && (cell->flags & 4)) query.m_Environment |= EB_GravityWell;
		if(room && room->roomType != 9 && (room->roomType < 22 || room->roomType > 24))
		{
			query.m_Environment |= EB_LegacyCity;
			// DpBuilding::FloorHt/DeckBelow report weightlessness throughout
			// authored gravity-well rooms, independently of city electricity.
			if(room->roomType == 11) query.m_Environment |= EB_GravityWell;
			if(cell && cell->type != 0 && cell->type != 1 && cell->type != 15 && cell->type != 18)
				query.m_Environment |= EB_Pathable;
			query.m_LegacyCityId = Id(); query.m_LegacyDevelopmentId = building.state.id;
			query.m_LegacyRoomType = room->roomType; query.m_CityEmpire = EmpireId();
			query.m_InIndustryLL = building.state.location;
			if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
				query.m_InIndustry = static_cast<INDUSTRYID>(catalogue->RuntimeIndustryId(AuLegacyCity::IndustryId{building.state.industryId}));
		}
	}
}

bool DaLegacyCity::Route(quint32 development, const AuPoint& from, const AuPoint& to,
	QVector<AuLegacyBd::Paths::RoutePoint>& points, bool useGroundLevelDoors) const
{
	points.clear();
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world || !g_Au.IsLegacyGeneration()) return false;
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	for(const auto& building : m_Runtime->buildings)
		if(building.state.id == development)
		{
			AuMatrix4x4 transform;
			world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
				building.state.facingRadians, nullptr, AuPoint(), transform);
			if(!building.paths.Route(transform.UnProject(from), transform.UnProject(to), points, useGroundLevelDoors)) return false;
			for(auto& point : points) point.location = transform.Project(point.location);
			return true;
		}
	return false;
}

void DaLegacyCity::PaintOnSurfaceMap(const DaWorldLL& world, QPainter& painter, unsigned int width, unsigned int height) const
{
	if(!g_Au.IsLegacyGeneration() || IsDead() || !width || !height) return;
	const auto state = Snapshot();
	const auto mapPoint = [width, height](const AuGlobeLLF& location) {
		return QPoint(int((location.Longitude() + PIf) * width / (2 * PIf)),
			int((PIf / 2 - location.Latitude()) * height / PIf));
	};
	// DaCity::PaintOnSurfaceMap (2016 0x2a3a90): roads first, then all
	// building dots, followed by the airport, town square and military dots.
	for(const auto& development : state.developments)
	{
		if(development.demolished || development.industryId != 2) continue;
		const quint8 process = development.constructionProcessId;
		if(process == 5 || process == 74 || process == 80 || process == 81) continue;
		painter.setPen(QPen(development.roadSlab ? QColor(0, 0, 0, 255) : QColor(32, 32, 32, 100),
			development.roadSlab ? 5.f : 0.f));
		AuMatrix4x4 placement;
		world.BuildingConstructionTransform(world.LocOfLL(development.location, development.altitudeMSL),
			development.facingRadians, nullptr, AuPoint(), placement);
		const double halfLength = development.roadLengthMeters * FeetPerMeter() / 2;
		AuGlobeLLF start, end;
		if(!world.LLOfLoc(start, placement.Project(AuPoint(0, -halfLength, 0)))
			|| !world.LLOfLoc(end, placement.Project(AuPoint(0, halfLength, 0)))) continue;
		const QPoint a = mapPoint(start), b = mapPoint(end);
		if(std::abs(end.Longitude() - start.Longitude()) < PIf)
			painter.drawLine(a, b);
		else if(a.x() < b.x())
		{
			const int length = a.x() + int(width) - b.x();
			const float fraction = length > 0 ? float(a.x()) / length : 0;
			const int seamY = int(a.y() + fraction * (b.y() - a.y()));
			painter.drawLine(a, QPoint(0, seamY));
			painter.drawLine(b, QPoint(int(width), seamY));
		}
		else
		{
			const int length = b.x() + int(width) - a.x();
			const float fraction = length > 0 ? float(b.x()) / length : 0;
			const int seamY = int(b.y() + fraction * (b.y() - a.y()));
			painter.drawLine(b, QPoint(0, seamY));
			painter.drawLine(a, QPoint(int(width), seamY));
		}
	}
	painter.setPen(QPen(QColor(255, 255, 32, 100), 0.f));
	for(const auto& development : state.developments)
		if(!development.demolished && development.industryId != 2)
			painter.drawPoint(mapPoint(development.location));
	for(const auto& development : state.developments)
	{
		if(development.demolished) continue;
		QColor color;
		switch(development.industryId)
		{
		case 111: color = QColor(32, 255, 32, 255); break; // Airport terminal.
		case 1: color = QColor(192, 32, 255, 255); break; // Town square.
		case 10: case 11: case 12: case 128: color = QColor(255, 128, 32, 255); break;
		default: continue;
		}
		painter.setPen(QPen(color, 0.f));
		painter.drawPoint(mapPoint(development.location));
	}
}

void DaLegacyCity::DeleteDeviceObjects(AuAtomRenderInfo& ari)
{
	UpdateSiteTerrain(ari, true);
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->DeleteDraw(ari, Id());
	DaAtom::DeleteDeviceObjects(ari);
}

void DaLegacyCity::GetRenderLightsRecursive(const DaPerson& person, const AuMatrix4x4& model,
	QList<AuGLSun>& suns, QList<AuGLLight>& lights) const
{
	DaAtom::GetRenderLightsRecursive(person, model, suns, lights);
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	if(!world || !g_Au.IsLegacyGeneration()) return;
	bool powered;
	{ QReadLocker stateLock(&m_CityLock); powered = m_City.capacitor > 0; }
	powered = powered || world->AtomType() == ATOM_WorldRingSection;
	const AuMatrix4x4 worldModel = model * Transform().Inverse();
	const double maximumDistance = 1000 * FeetPerMeter();
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	for(const auto& building : m_Runtime->buildings)
	{
		if(building.mesh.exteriorLights.isEmpty()) continue;
		AuMatrix4x4 placement;
		world->BuildingConstructionTransform(world->LocOfLL(building.state.location, building.state.altitudeMSL),
			building.state.facingRadians, nullptr, AuPoint(), placement);
		if(!ExteriorLightsOn(*world, building.state, placement)) continue;
		const AuMatrix4x4 lightModel = worldModel * placement;
		for(const auto& light : building.mesh.exteriorLights)
		{
			const AuPoint location = lightModel.Project(Point(light.position));
			if(location.Length2() > maximumDistance * maximumDistance) continue;
			const float radius = float((light.runway ? 2 : 10) * FeetPerMeter());
			// Use the existing nearest-local-light selection, so an entire city's
			// fixtures do not displace the scene person's lamp or vehicle lights.
			lights.append(AuGLLight(Id(), AuGLLight::Item, .005f, 1.f / (radius * radius),
				ExteriorLightColor(light.runway, powered), location));
		}
	}
}

void DaLegacyCity::RenderEnvironmentObject(const AuMatrix4x4& projection, AuAtomRenderInfo& ari,
	AuModelMatrixHash& matrices, const DaPerson* sceneAtom, AuRenderLastMap& last,
	RENDERPASS pass, DaAtomList& sc, DaAtomList& scFog, DaAtomList& scGlobe,
	DaAtomList& scGlobeFog, DaAtomList& scRing, DaAtomList& scRingFog)
{
	Q_UNUSED(last); Q_UNUSED(sc); Q_UNUSED(scFog); Q_UNUSED(scGlobe);
	Q_UNUSED(scGlobeFog); Q_UNUSED(scRing); Q_UNUSED(scRingFog);
	if(pass == PASS_Near) RenderCity(projection, ari, matrices, sceneAtom, false);
}

void DaLegacyCity::RenderPreTerrain(const AuMatrix4x4& projection, AuAtomRenderInfo& ari,
	AuModelMatrixHash& matrices, const DaPerson* sceneAtom)
{
	RenderCity(projection, ari, matrices, sceneAtom, true);
}

void DaLegacyCity::RenderCity(const AuMatrix4x4& projection, AuAtomRenderInfo& ari,
	AuModelMatrixHash& matrices, const DaPerson* sceneAtom, bool beforeTerrain)
{
	if(!ari.re || !g_Au.IsLegacyGeneration()) return;
	const auto* world = dynamic_cast<const DaWorldLL*>(Parent());
	const auto model = matrices.constFind(Id());
	if(!world || model == matrices.cend()) return;
	UpdateSiteTerrain(ari);
	QVector<AuLegacyCity::Flag> flags;
	bool powered;
	{ QReadLocker stateLock(&m_CityLock); flags = m_City.flags; powered = m_City.capacitor > 0; }
	powered = powered || world->AtomType() == ATOM_WorldRingSection;
	// DaCity::EnclosureNeeded 0x28f190, also used by CreateDeviceObjects.
	const bool enclosureNeeded = (world->Hydrographics() && world->AltitudeMSL(Transform().Translation()) <= -32)
		|| !world->HasBreathableAtmosphere();
	QMutexLocker lock(&m_Runtime->lock); m_Runtime->Ensure(*this);
	if(m_Runtime->minePostJob)
	{
		const auto pending = m_Runtime->minePostJob;
		if(pending->geometryRevision != m_Runtime->revision || pending->terrainRevision != m_Runtime->terrainRevision)
			pending->cancelled.store(true, std::memory_order_release);
		if(pending->finished.load(std::memory_order_acquire))
		{
			if(pending->succeeded && !pending->cancelled.load(std::memory_order_acquire))
			{
				m_Runtime->minePosts.swap(pending->result);
				m_Runtime->minePostRevision = pending->geometryRevision;
				m_Runtime->minePostTerrainRevision = pending->terrainRevision;
			}
			m_Runtime->minePostJob.reset();
		}
	}
	if(m_Runtime->minePostRevision != m_Runtime->revision
		|| m_Runtime->minePostTerrainRevision != m_Runtime->terrainRevision)
	{
		if(!m_Runtime->minePostJob)
		{
			QSharedPointer<Runtime::MinePostJob> pending(new Runtime::MinePostJob);
			pending->worldId = world->Id();
			pending->geometryRevision = m_Runtime->revision;
			pending->terrainRevision = m_Runtime->terrainRevision;
			for(const auto& building : m_Runtime->buildings)
				if(building.state.industryId == 50 && !building.state.demolished) pending->mines.append(building.state);
			if(pending->mines.isEmpty())
			{
				m_Runtime->minePosts.clear();
				m_Runtime->minePostRevision = pending->geometryRevision;
				m_Runtime->minePostTerrainRevision = pending->terrainRevision;
			}
			else
			{
				m_Runtime->minePostJob = pending;
				QThreadPool::globalInstance()->start(new Runtime::MinePostTask(pending), 0);
			}
		}
	}
	m_Runtime->UploadDraw(ari, Id());
	auto& shader = ari.re->GLSL().ModelTexColorShader();
	shader.Select(projection, *ari.re);
	float ambientR, ambientG, ambientB, fogDensity, fogR, fogG, fogB;
	bool sunsBlocked;
	world->SceneRenderParameters(sceneAtom, ambientR, ambientG, ambientB, fogDensity,
		fogR, fogG, fogB, ari.IsViewTopDown(), sunsBlocked);
	shader.SetAmbientLight(ambientR, ambientG, ambientB);
	const AuMatrix4x4 worldModel = model->Matrix() * Transform().Inverse();
	shader.SetDirectTextureGlobe(worldModel.Translation(),
		world->AtomType() == ATOM_WorldGlobe || world->AtomType() == ATOM_WorldGasGiant);
	if(sunsBlocked)
		for(int sun = 0; sun < MAX_SUNS; ++sun)
		{
			const AuGLSun& light = ari.re->Sun(sun);
			shader.SetSun(sun, false, light.m_Attenuation, light.m_ColorR, light.m_ColorG,
				light.m_ColorB, light.m_LocX, light.m_LocY, light.m_LocZ, light.m_DiskRadius);
		}
	const bool cullingEnabled = ari.re->glIsEnabled(GL_CULL_FACE);
	GLint previousFrontFace = GL_CW;
	ari.re->glGetIntegerv(GL_FRONT_FACE, &previousFrontFace);
	// All uploaded building batches use the original clockwise convention.
	ari.re->glFrontFace(GL_CW);
	GLboolean colorMask[4];
	ari.re->glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
	const bool depthClampEnabled = ari.re->glIsEnabled(GL_DEPTH_CLAMP);
	for(const auto& draw : m_Runtime->drawBatches)
	{
		if(draw.interior != beforeTerrain) continue;
		const auto found = std::find_if(m_Runtime->buildings.cbegin(), m_Runtime->buildings.cend(),
			[&draw](const Runtime::Building& b) { return b.state.id == draw.development; });
		if(found == m_Runtime->buildings.cend() || !draw.vao.VAO()) continue;
		AuMatrix4x4 transform;
		world->BuildingConstructionTransform(world->LocOfLL(found->state.location, found->state.altitudeMSL),
			found->state.facingRadians, nullptr, AuPoint(), transform);
		const AuMatrix4x4 buildingModel = worldModel * transform;
		shader.SetModelView(buildingModel); shader.SetTexture(draw.texture);
		AuPoint ambientUp = buildingModel.ProjectVector(0, 0, 1);
		ambientUp.Unit();
		shader.SetAmbientLightUp(ambientUp.x, ambientUp.y, ambientUp.z);
		shader.SetColor(draw.tint.x(), draw.tint.y(), draw.tint.z(), draw.tint.w());
		if(draw.exteriorLens && ExteriorLightsOn(*world, found->state, transform))
		{
			const QColor color = ExteriorLightColor(draw.exteriorLens == 2, powered);
			shader.SetGlow(color.redF(), color.greenF(), color.blueF());
		}
		else shader.SetGlow(draw.selfIllumination, draw.selfIllumination, draw.selfIllumination);
		if(draw.twoSided && cullingEnabled) ari.re->glDisable(GL_CULL_FACE);
		if(draw.depthOnly)
		{
			ari.re->glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			// Keep the terrain mask when the camera's near plane crosses it
			// before crossing the terrain immediately beneath the entrance.
			if(!depthClampEnabled) ari.re->glEnable(GL_DEPTH_CLAMP);
		}
		draw.vao.SelectGL(); draw.vao.DrawGL(); draw.vao.DeselectGL();
		if(draw.depthOnly)
		{
			if(!depthClampEnabled) ari.re->glDisable(GL_DEPTH_CLAMP);
			ari.re->glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
		}
		if(draw.twoSided && cullingEnabled) ari.re->glEnable(GL_CULL_FACE);
	}
	if(sunsBlocked) shader.InvalidateSuns();
	shader.Deselect();
	if(beforeTerrain)
	{
		ari.re->glFrontFace(GLenum(previousFrontFace));
		return;
	}

	for(int i = flags.size(); i < m_Runtime->flagDraws.size(); ++i)
		if(m_Runtime->flagDraws[i].vao.VAO()) m_Runtime->flagDraws[i].vao.DeleteGL();
	m_Runtime->flagDraws.resize(flags.size());
	const float seconds = AuTime::CurrentMinCycle() * 60.f;
	const auto* ring = dynamic_cast<const DaWorldRingSection*>(world);
	if(!m_Runtime->minePosts.isEmpty())
	{
		AuGLModelInstance* post = ari.LoadModelThreaded(QStringLiteral(":data/mFencePost.3ds"), false, 1);
		if(post && (post->HasGLModel() || post->GLModel(*ari.re)))
		{
			float shadowAngles[MAX_SUNS] = {};
			if(ring) for(int sun = 0; sun < MAX_SUNS; ++sun)
				shadowAngles[sun] = ring->ShadowPanelSunAngle(ari.re->Sun(sun).m_ShadowAngle);
			for(const auto& placement : m_Runtime->minePosts)
			{
				const AuMatrix4x4 postModel = worldModel * placement;
				const double depth = postModel.Translation().z;
				if(depth >= 0 || depth < -g_Au.TreeViewRangeFt()) continue;
				AuPoint ambientUp = postModel.ProjectVector(0, 0, 1); ambientUp.Unit();
				if(ring) post->RenderFogRingworld(projection, postModel, *ari.re, shadowAngles,
					ambientR, ambientG, ambientB, ambientUp, false, fogDensity, fogR, fogG, fogB);
				else post->RenderFogGlobe(projection, postModel, *ari.re, worldModel.Translation(),
					ambientR, ambientG, ambientB, ambientUp, false, fogDensity, fogR, fogG, fogB);
			}
		}
	}
	const auto renderFlags = [&]() {
		for(int i = 0; i < flags.size(); ++i)
		{
			const auto& flag = flags[i];
			auto& draw = m_Runtime->flagDraws[i];
			const auto building = std::find_if(m_Runtime->buildings.cbegin(), m_Runtime->buildings.cend(),
				[&flag](const Runtime::Building& b) { return b.state.location == flag.location; });
			if(building == m_Runtime->buildings.cend() || !building->state.constructed || building->state.demolished) continue;
			if(!draw.vao.VAO() || draw.empire != flag.empire)
			{
				if(draw.vao.VAO()) draw.vao.DeleteGL();
				draw.texture = 0;
				QImage image;
				const DgData& gd = g_Au.LockDataForRead(); gd.EmpireFlag(flag.empire, image); g_Au.UnlockData();
				if(image.isNull()) continue; // Retry when the empire image arrives.
				draw.empire = flag.empire;
				draw.texture = ari.re->CreateTextureFromBitmap(Id().GetID(),
					QStringLiteral("Flag:%1").arg(flag.empire), image, GL_LINEAR, GL_LINEAR, false, 1);
				CreateFlagVAO(draw.vao, *ari.re, float(image.height()) / image.width());
			}
			if(!draw.texture) continue;
			// DaCity::CreateDeviceObjects 0x2813b0. Use the flag's own
			// weather and level; building heading does not turn the cloth.
			const auto& dev = building->state;
			AuMatrix4x4 placement;
			world->BuildingConstructionTransform(world->LocOfLL(dev.location, dev.altitudeMSL),
				dev.facingRadians, nullptr, AuPoint(), placement);
			const AuPoint base = placement.Translation();
			const AuPoint up = -world->GravityDir(base);
			const AuPoint location = base + up * 3.0 + (up * 27.0) * double(float(flag.level) / 30.f);
			float direction = 0, force = 1, period = 1;
			if(!enclosureNeeded)
			{
				const QPointF wind = world->WindAt(flag.location);
				direction = float(std::atan2(wind.y(), wind.x()) + double(1.5707964f));
				force = qMin(std::sqrt(float(wind.x() * wind.x() + wind.y() * wind.y())) / 100.f, 1.f);
				period = std::sin(seconds / (5.f - force * 4.75f) * 3.1415927f * 2.f);
			}
			AuMatrix4x4 flagModel = worldModel;
			flagModel.Translate(location);
			flagModel.Rotate(AuPoint::CalculateRotation(up));
			flagModel.RotateZ(direction);
			AuPoint ambientUp = flagModel.ProjectVector(-world->GravityDir(location));
			if(ring)
				RenderFlag(ari.re->GLSL().RingworldFlagShader(), flagModel, ambientUp, draw.texture, force, period, draw.vao);
			else
				RenderFlag(ari.re->GLSL().FlagShader(), flagModel, ambientUp, draw.texture, force, period, draw.vao);
		}
	};
	if(!flags.isEmpty())
	{
		if(ring)
		{
			auto& flagShader = ari.re->GLSL().RingworldFlagShader();
			flagShader.Select(projection, *ari.re);
			flagShader.SetAmbientLight(ambientR, ambientG, ambientB);
			flagShader.SetFog(fogDensity, fogR, fogG, fogB);
			for(int sun = 0; sun < MAX_SUNS; ++sun)
			{
				const AuGLSun& light = ari.re->Sun(sun);
				flagShader.SetSun(sun, light.m_Enabled && !sunsBlocked, light.m_Attenuation,
					light.m_ColorR, light.m_ColorG, light.m_ColorB, light.m_LocX, light.m_LocY, light.m_LocZ,
					light.m_DiskRadius, ring->ShadowPanelSunAngle(light.m_ShadowAngle), light.m_ShadowCount,
					light.m_XVec.x, light.m_XVec.y, light.m_XVec.z, light.m_YVec.x, light.m_YVec.y, light.m_YVec.z);
			}
			renderFlags();
			flagShader.Deselect();
		}
		else
		{
			auto& flagShader = ari.re->GLSL().FlagShader();
			flagShader.Select(projection, *ari.re);
			flagShader.SetAmbientLight(ambientR, ambientG, ambientB);
			flagShader.SetFog(fogDensity, fogR, fogG, fogB);
			const AuPoint globe = worldModel.Translation();
			flagShader.SetGlobeLocation(globe.x, globe.y, globe.z);
			for(int sun = 0; sun < MAX_SUNS; ++sun)
			{
				const AuGLSun& light = ari.re->Sun(sun);
				flagShader.SetSun(sun, light.m_Enabled && !sunsBlocked, light.m_Attenuation,
					light.m_ColorR, light.m_ColorG, light.m_ColorB, light.m_LocX, light.m_LocY, light.m_LocZ, light.m_DiskRadius);
			}
			renderFlags();
			flagShader.Deselect();
		}
	}
	ari.re->glFrontFace(GLenum(previousFrontFace));
}

bool DaLegacyCity::RegisterPublicDesigns(QSqlDatabase& db, QString& error)
{
	if(!g_Au.IsLegacyGeneration()) return true;
	for(quint32 id=1; id<=74; ++id)
	{
		const auto design = AuLegacyBd::Catalog::BuiltIn().Find(id);
		QFile asset(QStringLiteral(":/legacy/cities/designs/B%1").arg(id));
		if(!design || !asset.open(QIODevice::ReadOnly))
		{ error = QStringLiteral("Missing packaged legacy building design %1.").arg(id); return false; }
		const auto bytes = asset.readAll();
		QSqlQuery write(db);
		write.prepare(QStringLiteral("INSERT INTO n_LegacyBdDesign(id,sourceId,contentHash,sourceData,name,industry,publicDefault) VALUES(?,?,?,?,?,?,true) ON CONFLICT(id) DO NOTHING"));
		write.addBindValue(id); write.addBindValue(design->header.designId); write.addBindValue(QString::fromLatin1(design->sourceHash.toHex()));
		write.addBindValue(bytes); write.addBindValue(design->header.name); write.addBindValue(design->header.industryId);
		if(!write.exec()) { error = write.lastError().text(); return false; }
		QSqlQuery read(db);
		read.prepare(QStringLiteral("SELECT sourceId,contentHash,sourceData,name,industry,publicDefault FROM n_LegacyBdDesign WHERE id=?"));
		read.addBindValue(id);
		if(!read.exec() || !read.next() || read.value(0).toUInt() != id
			|| read.value(1).toString().toLatin1() != design->sourceHash.toHex()
			|| QCryptographicHash::hash(read.value(2).toByteArray(), QCryptographicHash::Sha256) != design->sourceHash
			|| read.value(3).toString() != design->header.name || read.value(4).toUInt() != design->header.industryId
			|| !read.value(5).toBool())
		{ error = QStringLiteral("Stored legacy building design %1 conflicts with the packaged original.").arg(id); return false; }
	}
	return true;
}
