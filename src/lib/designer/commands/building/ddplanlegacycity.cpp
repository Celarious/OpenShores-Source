#include "ddplanlegacycity.h"
#include "auatomrenderinfo.h"
#include "audesignerview.h"
#include "augleffects.h"
#include "auglobal.h"
#include "aulegacybd.h"
#include "aulegacybuildinggeometry.h"
#include "aulegacybiodome.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityproduction.h"
#include "aulegacycityconstruction.h"
#include "aulegacycitystate.h"
#include "daperson.h"
#include "dalegacycity.h"
#include "daatomlist.h"
#include "daworldll.h"
#include <QWheelEvent>
#include <QGuiApplication>
#include <QCursor>
#include <QPainter>
#include <QPixmap>
#include <cmath>

namespace
{
void AddLine(QList<AuPointAlpha>& vertices, const AuPoint& a, const AuPoint& b)
{
	vertices.append(AuPointAlpha(a.x, a.y, a.z)); vertices.append(AuPointAlpha(b.x, b.y, b.z));
}

double ClearSiteRadius(const AuLegacyCity::Development& dev)
{
	if(dev.industryId == 129) return dev.biodomeRadiusMeters * FeetPerMeter();
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	const auto* recipe = !catalogue ? nullptr : dev.constructionProcessId
		? catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(dev.constructionProcessId))
		: catalogue->ConstructionProcessForIndustry(AuLegacyCity::IndustryId(dev.industryId));
	return recipe ? recipe->buildRadius * FeetPerMeter() : 0;
}

struct ClearTarget
{
	quint32 city = 0;
	AuLegacyCity::Development development;
	AuMatrix4x4 placement;
	bool allowed = false;
};

bool FindClearTarget(const DaWorldLL& world, const AuPoint& point, ClearTarget& hit)
{
	AuGlobeLLF ll; if(!world.LLOfLoc(ll, point)) return false;
	DaAtomList atoms; world.ListNormalSubatoms(atoms);
	for(const auto* atom : atoms)
		if(const auto* city = dynamic_cast<const DaLegacyCity*>(atom))
		{
			if(city->IsDead()) continue;
			const auto state = city->Snapshot();
			// Original lookup prefers buildings, then roads, then enclosing biodomes.
			for(int kind = 0; kind < 3; ++kind) for(const auto& dev : state.developments)
			{
				if((dev.industryId == 129 ? 2 : dev.industryId == 2 ? 1 : 0) != kind) continue;
				const double radius = ClearSiteRadius(dev);
				const double halfLength = dev.roadLengthMeters * FeetPerMeter() / 2;
				const double halfWidth = dev.roadWidthMeters * FeetPerMeter() / 2;
				const AuPoint center = world.LocOfLL(dev.location, dev.altitudeMSL);
				if(center.Distance2(point) > std::pow(2 * (kind == 1 ? qMax(halfLength, halfWidth) : radius) + 100, 2)) continue;
				AuMatrix4x4 placement;
				world.BuildingConstructionTransform(center, dev.facingRadians, nullptr, AuPoint(), placement);
				const AuPoint local = placement.UnProject(world.LocOfLL(ll, dev.altitudeMSL));
				bool contains = kind == 2 ? center.Distance(world.LocOfLL(ll, dev.altitudeMSL)) <= radius + AuLegacyBd::Biodome::ContainmentMargin
					: std::abs(local.x) <= radius && std::abs(local.y) <= radius;
				if(kind == 1)
				{
					AuGlobeLLF start, end;
					world.LLOfLoc(start, placement.Project(AuPoint(0, -halfLength, 0)));
					world.LLOfLoc(end, placement.Project(AuPoint(0, halfLength, 0)));
					float fraction;
					const auto closest = ll.CoordOnSegment(start, end, &fraction, world.LLShape());
					const double grade = dev.roadStartAltitudeMSL + (dev.roadEndAltitudeMSL - dev.roadStartAltitudeMSL) * qBound(0.f, fraction, 1.f);
					contains = fraction >= 0 && fraction <= 1
						&& world.LocOfLL(ll, grade).Distance2(world.LocOfLL(closest, grade)) <= halfWidth * halfWidth;
				}
				if(!contains) continue;
				hit.city = city->Id().GetID(); hit.development = dev; hit.placement = placement;
				hit.allowed = dev.industryId != 1 || state.developments.size() == 1;
				return true;
			}
		}
	return false;
}

void ClearOutline(const DaWorldLL& world, const ClearTarget& hit,
	QList<AuPointAlpha>& vertices, QList<AuPointAlpha>& ground, bool perimeterOnly = false)
{
	const auto& dev = hit.development;
	const auto onGround = [&world](AuPoint point) {
		AuGlobeLL ll; return world.LLOfLoc(ll, point) ? world.GetTerrainLocation(ll) : point;
	};
	if(dev.industryId == 2)
	{
		// DpGlobeRoad::RenderCursorHit: edges sampled every fifteen feet.
		const double halfLength = dev.roadLengthMeters * FeetPerMeter() / 2;
		const double halfWidth = dev.roadWidthMeters * FeetPerMeter() / 2;
		AuGlobeLLF startLL, endLL;
		world.LLOfLoc(startLL, hit.placement.Project(AuPoint(0, -halfLength, 0)));
		world.LLOfLoc(endLL, hit.placement.Project(AuPoint(0, halfLength, 0)));
		const AuPoint start = world.LocOfLL(startLL, dev.roadStartAltitudeMSL);
		const AuPoint end = world.LocOfLL(endLL, dev.roadEndAltitudeMSL), direction = end - start;
		const double length = direction.Length(); if(length <= 0) return;
		const bool landscape = dev.constructionProcessId == 5 || dev.constructionProcessId == 74
			|| dev.constructionProcessId == 80 || dev.constructionProcessId == 81;
		AuPoint previousLeft, previousRight, previousCenter, firstLeft, firstRight;
		for(double along = 0;; along = qMin(length, along + 15))
		{
			AuPoint center = start + direction * (along / length);
			AuPoint right = direction.Cross(-world.GravityDir(center));
			if(right.IsZero()) right = hit.placement.ProjectVector(AuPoint(1, 0, 0));
			right.Unit(halfWidth);
			world.ElevateLocMSL(center, dev.roadStartAltitudeMSL + (dev.roadEndAltitudeMSL - dev.roadStartAltitudeMSL) * along / length);
			const AuPoint leftEdge = center - right, rightEdge = center + right;
			if(!along) { firstLeft = leftEdge; firstRight = rightEdge; }
			else
			{
				AddLine(vertices, previousLeft, leftEdge); AddLine(vertices, previousRight, rightEdge);
				if(!perimeterOnly && !landscape && !dev.roadSlab) AddLine(vertices, previousCenter, center);
			}
			if(!perimeterOnly && !landscape && !dev.roadSlab) AddLine(vertices, leftEdge, rightEdge);
			previousLeft = leftEdge; previousRight = rightEdge; previousCenter = center;
			if(along >= length) break;
		}
		if(dev.roadSlab || perimeterOnly)
		{
			AddLine(vertices, firstLeft, firstRight); AddLine(vertices, previousLeft, previousRight);
			if(!perimeterOnly && !landscape) { AddLine(vertices, firstLeft, previousRight); AddLine(vertices, firstRight, previousLeft); }
		}
		return;
	}
	const double radius = ClearSiteRadius(dev);
	const int count = dev.industryId == 129 ? AuLegacyCityCatalogue::BiodomeSegmentCount(radius) : 32;
	AuPoint first, previous, firstTop, previousTop;
	for(int i = 0; i < count; ++i)
	{
		const float angle = float(i) * PIf * 2 / count;
		float x = std::cos(angle), y = std::sin(angle);
		if(dev.industryId != 129) { const float scale = qMax(std::abs(x), std::abs(y)); x /= scale; y /= scale; }
		const AuPoint point = hit.placement.Project(AuPoint(radius * x, radius * y, 0));
		const AuPoint bottom = onGround(point);
		if(!i) first = bottom; else AddLine(dev.industryId == 129 ? ground : vertices, previous, bottom);
		previous = bottom;
		if(dev.industryId == 129 && !perimeterOnly)
		{
			AuPoint top = point; world.ElevateLocMSL(top, dev.altitudeMSL + AuLegacyBd::Biodome::BeamHeight);
			AddLine(vertices, bottom, top);
			if(!i) firstTop = top; else AddLine(vertices, previousTop, top);
			previousTop = top;
		}
	}
	if(count) AddLine(dev.industryId == 129 ? ground : vertices, previous, first);
	if(dev.industryId == 129 && count && !perimeterOnly) AddLine(vertices, previousTop, firstTop);
}

AuLegacyBd::Geometry::Mesh Preview(const AuLegacyCityProtocol::Request& request)
{
	quint32 designId = request.designId;
	if(!designId)
		for(quint32 id = 1; id <= 74; ++id)
		{
			const auto design = AuLegacyBd::Catalog::BuiltIn().Find(id);
			if(design && design->header.industryId == request.industryId) { designId = id; break; }
		}
	const auto design = AuLegacyBd::Catalog::BuiltIn().Find(designId);
	if(design)
	{
		AuLegacyBd::Geometry::Options options; options.materialCommodity = request.material;
		options.minimumDeck = design->MinimumDeck(); options.maximumDeck = design->MaximumDeck();
		if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
			if(const auto* recipe = catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(request.processId)))
			{
				options.maximumDeck = qMin(options.maximumDeck, catalogue->BuildingDesignMaximumDeck(
					AuLegacyCity::IndustryId(request.industryId), AuLegacyCity::CommodityId(request.material)));
				if(recipe->stackLimit == 32) options.minimumDeck = -2 * int(request.levels);
				else if(request.industryId == 51 && request.levels <= 1) options.minimumDeck = 0;
				else if(recipe->stackLimit > 0 && recipe->stackLimit < 32)
					options.maximumDeck = qMin(options.maximumDeck, qMax(0, int(request.levels) - 1));
			}
		options.interior = false;
		return AuLegacyBd::Geometry::Build(*design, options);
	}
	AuLegacyCity::Development dev; dev.industryId = request.industryId; dev.materialId = request.material;
	dev.constructionProcessId = request.processId; dev.completedLevels = 1; dev.constructed = true;
	dev.roadLengthMeters = request.roadLengthMeters; dev.roadWidthMeters = request.roadWidthMeters;
	dev.biodomeRadiusMeters = request.biodomeRadiusMeters;
	return AuLegacyBd::Geometry::BuildProcedural(dev);
}
}

DdPlanLegacyCity::DdPlanLegacyCity(AuDesignerView* view, AuLegacyCityConstruction* controls,
	const AuLegacyCityProtocol::Request& request) : DdCommand(view), m_Interface(controls), m_Request(request)
{
	m_Angle = request.targetX;
	m_Request.targetX = m_Request.targetY = m_Request.targetZ = 0;
}

DdPlanLegacyCity::~DdPlanLegacyCity()
{
	if(m_Interface) m_Interface->PlanningStopped();
}

bool DdPlanLegacyCity::UsesControls(const AuLegacyCityConstruction* controls) const
{
	return m_Interface.data() == controls;
}

void DdPlanLegacyCity::UpdateSelection(const AuLegacyCityProtocol::Request& request)
{
	const bool sameProcess = request.industryId == m_Request.industryId && request.processId == m_Request.processId
		&& request.action == m_Request.action;
	const float heading = m_Request.headingDegrees;
	m_Request = request; m_Request.headingDegrees = heading;
	m_MaximumLevels = -1; m_SiteMaterial = 0; m_SiteDesignId = 0;
	if(!sameProcess) { m_PreviousSiteWorld = 0; m_AvailableProducts.clear(); }
	ChangeLevels(0);
	m_Angle = request.targetX;
	m_Request.targetX = m_Request.targetY = m_Request.targetZ = 0;
	if(!sameProcess) m_HasStart = false;
	m_SiteBlocked = false;
	m_ConflictWorld = 0; m_ConflictingPerimeters.clear();
	m_PreviewValid = false;
}

void DdPlanLegacyCity::SetCursor(const AuMatrix4x4& camera, DaPerson* person, const QPoint& pixel)
{
	if(person && m_Request.action == AuLegacyCityProtocol::Build && m_Request.industryId != 2 && m_Request.industryId != 129)
	{
		AuPoint point;
		if(const auto* world = m_pWnd->FindTerrainLocationAtPixel(person, pixel, point)) UpdateSite(*world, point);
		else
		{
			m_MaximumLevels = 0; m_SiteMaterial = 0; m_SiteBlocked = false;
			m_ConflictWorld = 0; m_ConflictingPerimeters.clear();
		}
	}
	auto cursor = m_Request;
	if(m_SiteMaterial) cursor.material = m_SiteMaterial;
	const QPixmap icon = m_Interface ? m_Interface->PlacementIcon(cursor) : QPixmap();
	if(!person || !g_Au.IsLegacyGeneration() || icon.isNull())
	{ DdCommand::SetCursor(camera, person, pixel); return; }
	// 2016: DdPlanGlobeRoad::SetCursor (0x140327ba0) uses the native
	// road bitmap, doubled to 32 pixels for slabs, with a centered hotspot.
	if(m_Request.action == AuLegacyCityProtocol::Build && m_Request.industryId == 2)
	{
		m_pWnd->setCursor(QCursor(m_Request.roadSlab ? icon.scaledToWidth(32) : icon)); return;
	}
	// Building 0x140321130, biodome 0x14031bd50 and clear 0x14031d920.
	const bool building = m_Request.action == AuLegacyCityProtocol::Build && m_Request.industryId != 129;
	QPixmap image(building ? 64 : 32, 32); image.fill(Qt::transparent);
	QPainter painter(&image); painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.drawPixmap(QRectF(0, 0, 32, 32), icon, QRectF());
	if(building && m_Request.levels)
	{
		painter.setFont(QFont(QStringLiteral("Arial"), 12, QFont::Bold));
		const int count = m_MaximumLevels < 0 ? m_Request.levels : qMin(int(m_Request.levels), m_MaximumLevels);
		const QString levels = QString::number(count);
		painter.setPen(Qt::black); painter.drawText(QRect(1, 1, 32, 32), Qt::AlignLeft | Qt::AlignTop, levels);
		painter.setPen(count && !m_SiteBlocked ? QColor(25, 229, 25) : QColor(229, 25, 25));
		painter.drawText(QRect(0, 0, 31, 31), Qt::AlignLeft | Qt::AlignTop, levels);
	}
	if(building && m_Request.citizenEnabled)
		if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
			if(const auto* recipe = catalogue->ManufacturingProcess(AuLegacyCity::ManufacturingProcessId(m_Request.manufacturingRecipe)))
				if(const auto* product = catalogue->Commodity(recipe->commodity))
					painter.drawPixmap(QRectF(32, 0, 32, 32), QPixmap(product->descriptor.BitmapName()), QRectF());
	painter.end(); m_pWnd->setCursor(QCursor(image));
}

void DdPlanLegacyCity::ChangeMaterial(int direction)
{
	if(m_Request.industryId == 2) return;
	const quint16 materials[] = {86, 48, 76, 51, 146, 145};
	for(int i = 0; i < 6; ++i)
		if(materials[i] == m_Request.material)
		{
			m_Request.material = materials[(i + direction + 6) % 6]; m_PreviewValid = false;
			ChangeLevels(0);
			if(m_Interface) m_Interface->SetCursorMaterial(m_Request.material); return;
		}
}

void DdPlanLegacyCity::ChangeRotation(int direction, bool fine)
{
	if(m_Request.industryId == 2)
	{
		m_Request.roadSlab = !m_Request.roadSlab;
		if(m_Interface) m_Interface->SetCursorRoadSlab(m_Request.roadSlab); return;
	}
	double degrees = m_Request.headingDegrees + direction * (fine ? 1 : (m_Angle > 0 ? m_Angle : 45));
	while(degrees > 180) degrees -= 360;
	while(degrees < -180) degrees += 360;
	m_Request.headingDegrees = float(degrees);
}

void DdPlanLegacyCity::ChangeLevels(int direction)
{
	if(m_Request.action != AuLegacyCityProtocol::Build || m_Request.industryId == 2 || m_Request.industryId == 129) return;
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn(); if(!catalogue) return;
	const int limit = catalogue->ConstructionLevelLimit(AuLegacyCity::ConstructionProcessId(m_Request.processId),
		AuLegacyCity::CommodityId(m_Request.material));
	const int maximum = qMax(1, m_MaximumLevels < 0 ? limit : m_MaximumLevels);
	int levels = int(m_Request.levels) + direction;
	// Original DdPlanGlobeBuilding wraps when stepping past either end.
	// A settings change only clamps the existing selection to its new limit.
	if(direction) { if(levels < 1) levels = maximum; else if(levels > maximum) levels = 1; }
	else levels = qBound(1, levels, maximum);
	m_Request.levels = quint8(levels);
	m_PreviewValid = false;
	if(m_Interface) m_Interface->SetCursorLevels(m_Request.levels);
}

void DdPlanLegacyCity::ChangeProduct(int direction)
{
	if(!m_Request.citizenEnabled || !m_Interface) return;
	QVector<quint16> choices = m_AvailableProducts;
	if(choices.isEmpty() && m_Request.industryId != 50 && m_Request.industryId != 51 && m_Request.industryId != 89)
		if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
			for(const auto& recipe : catalogue->ManufacturingProcesses())
				if(recipe.industry.value == m_Request.industryId) choices.append(recipe.id.value);
	if(choices.isEmpty()) return;
	const int current = choices.indexOf(m_Request.manufacturingRecipe);
	m_Request.manufacturingRecipe = choices[(qMax(0, current) + direction + choices.size()) % choices.size()];
	m_Interface->SetCursorProductChoices(choices, m_Request.manufacturingRecipe);
}

void DdPlanLegacyCity::UpdateSite(const DaWorldLL& world, const AuPoint& point)
{
	m_ConflictWorld = world.Id().GetID(); m_ConflictingPerimeters.clear();
	QList<AuPointAlpha> conflictGround;
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	const auto* recipe = catalogue ? catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(m_Request.processId)) : nullptr;
	if(!recipe) return;
	AuLegacyCity::Development site;
	site.industryId = m_Request.industryId; site.materialId = m_Request.material;
	site.designId = m_Request.designId;
	if(!world.LLOfLoc(site.location, point)) return;
	site.altitudeMSL = world.SurfaceAltitudeMSL(site.location);
	if(world.Hydrographics() && site.altitudeMSL > -32 && site.altitudeMSL < 1) site.altitudeMSL = 1;
	site.facingRadians = m_Request.headingDegrees * DEG2RAD - world.BuildingConstructionNorthAngle(point, nullptr);
	const int previousExistingLevels = m_ExistingLevels;
	m_ExistingLevels = 0;
	m_SiteBlocked = false;
	quint32 extensionCity = 0, extensionDevelopment = 0;
	ClearTarget hit;
	if(FindClearTarget(world, point, hit) && !hit.development.demolished
		&& hit.development.industryId == site.industryId && site.industryId != 1)
	{
		site = hit.development; m_ExistingLevels = site.completedLevels;
		extensionCity = hit.city; extensionDevelopment = site.id;
		m_SiteBlocked = !site.components.isEmpty();
		if(m_SiteBlocked) ClearOutline(world, hit, m_ConflictingPerimeters, conflictGround, true);
	}
	const bool siteChanged = m_PreviousSiteWorld != world.Id().GetID()
		|| m_PreviousSiteLocation.Latitude() != site.location.Latitude()
		|| m_PreviousSiteLocation.Longitude() != site.location.Longitude();
	AuMatrix4x4 placement;
	const AuPoint center = world.LocOfLL(site.location, site.altitudeMSL);
	world.BuildingConstructionTransform(center, site.facingRadians, nullptr, AuPoint(), placement);
	unsigned int rocks = 0, shrubs = 0, trees = 0; float water = 0;
	const double radius = recipe->buildRadius * FeetPerMeter();
	DaAtomList cities; world.ListNormalSubatoms(cities);
	for(const auto* atom : cities)
		if(const auto* city = dynamic_cast<const DaLegacyCity*>(atom))
		{
			if(city->IsDead()) continue;
			const auto state = city->Snapshot();
			for(const auto& existing : state.developments)
			{
				if(existing.demolished || (city->Id().GetID() == extensionCity && existing.id == extensionDevelopment)) continue;
				if(existing.industryId == 2 && (existing.constructionProcessId == 5 || existing.constructionProcessId == 74
					|| existing.constructionProcessId == 80 || existing.constructionProcessId == 81)) continue;
				AuMatrix4x4 otherPlacement;
				world.BuildingConstructionTransform(world.LocOfLL(existing.location, existing.altitudeMSL),
					existing.facingRadians, nullptr, AuPoint(), otherPlacement);
				bool interferes = false;
				if(existing.industryId == 129)
				{
					// Being wholly inside a dome is allowed; intersecting its wall
					// is not. Keep the original containment margin and airport rule.
					const double domeRadius = existing.biodomeRadiusMeters * FeetPerMeter();
					const auto toDome = otherPlacement.Inverse() * placement;
					double outerRadius = 0;
					for(int x : {-1, 1}) for(int y : {-1, 1})
					{
						const AuPoint corner = toDome.Project(AuPoint(x * radius, y * radius, 0));
						outerRadius = qMax(outerRadius, std::hypot(corner.x, corner.y));
					}
					const AuPoint domeCenter = toDome.UnProject(AuPoint());
					const double innerRadius = std::hypot(qMax(0.0, std::abs(domeCenter.x) - radius), qMax(0.0, std::abs(domeCenter.y) - radius));
					const bool inside = outerRadius <= domeRadius - AuLegacyBd::Biodome::ContainmentMargin;
					const bool outside = innerRadius >= domeRadius + AuLegacyBd::Biodome::ContainmentMargin;
					interferes = (!inside && !outside) || (inside && site.industryId == 111);
				}
				else interferes = DaLegacyCity::ConstructionSitesOverlap(world, site, placement, existing, otherPlacement);
				if(interferes)
				{
					m_SiteBlocked = true;
					ClearTarget conflict;
					conflict.development = existing; conflict.placement = otherPlacement;
					ClearOutline(world, conflict, m_ConflictingPerimeters, conflictGround, true);
				}
			}
		}
	m_ConflictingPerimeters.append(conflictGround);
	// Original SetCursor 0x140321130 counts at most four resources and
	// subtracts existing levels. Keep desired levels separate from this cap.
	if(recipe->stackLimit >= 32 || site.industryId == 89)
		world.ResourceDistributionAt(placement, center, AuPoint(radius, radius, 0), rocks, shrubs, trees, water, 4);
	const int maximum = qMax(0, qMin(catalogue->ConstructionLevelLimit(recipe->id,
		AuLegacyCity::CommodityId(site.materialId), world.OrbitZone() != OZONE_Inferno),
		catalogue->ConstructionResourceLevelLimit(recipe->id, rocks, shrubs, trees)) - m_ExistingLevels);
	if(maximum != m_MaximumLevels || m_SiteMaterial != site.materialId || previousExistingLevels != m_ExistingLevels
		|| m_SiteDesignId != site.designId)
		m_PreviewValid = false;
	m_MaximumLevels = maximum; m_SiteMaterial = site.materialId;
	m_SiteDesignId = site.designId;
	m_SiteCenter = center; m_SiteFacing = site.facingRadians;
	m_SiteMessage = maximum ? QString() : QObject::tr("No more levels can be built at this site.");
	if(m_SiteBlocked) m_SiteMessage = extensionDevelopment && !site.components.isEmpty()
		? QObject::tr("Finish the current construction before adding more levels.")
		: QObject::tr("The construction site overlaps an existing development.");
	if(m_Request.citizenEnabled && (site.industryId == 50 || site.industryId == 51 || site.industryId == 89))
	{
		// Recovered cursor rules differ from F10: a well can always select
		// water here. Natural gas tests the same outcrop as crude oil.
		m_AvailableProducts.clear(); quint16 preferred = 0, fallback = 0;
		bool currentAvailable = false, currentFallback = false;
		for(const auto& product : catalogue->ManufacturingProcesses())
		{
			if(product.industry.value != site.industryId) continue;
			const quint16 commodity = product.commodity.value;
			const bool research = AuLegacyCity::IsProductionTechnology(commodity);
			const bool defaultProduct = site.industryId == 89 ? product.id.value == 171
				: commodity == (site.industryId == 50 ? 76 : 83);
			bool available = research;
			if(!research && site.industryId == 89)
				available = product.id.value == 171 || (water > 0 && product.id.value == (world.OrbitZone() == OZONE_Inferno ? 252 : world.OrbitZone() == OZONE_Frigid ? 253 : 170));
			else if(!research)
				available = (site.industryId == 51 && commodity == 83) || world.TestRockResource(placement, center,
					AuPoint(radius, radius, radius), COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(
						site.industryId == 51 && commodity == 57 ? 23 : commodity))));
			if(available)
			{
				m_AvailableProducts.append(product.id.value);
				if(!research) { if(defaultProduct) fallback = product.id.value; else preferred = product.id.value; }
			}
			if(product.id.value == m_Request.manufacturingRecipe)
			{ currentAvailable = available; currentFallback = defaultProduct; }
		}
		if(!currentAvailable || (currentFallback && siteChanged))
			if(preferred || fallback) m_Request.manufacturingRecipe = preferred ? preferred : fallback;
		if(m_Interface) m_Interface->SetCursorProductChoices(m_AvailableProducts, m_Request.manufacturingRecipe);
	}
	else if(m_Interface && !m_AvailableProducts.isEmpty())
	{
		m_AvailableProducts.clear();
		for(const auto& product : catalogue->ManufacturingProcesses())
			if(product.industry.value == site.industryId) m_AvailableProducts.append(product.id.value);
		m_Interface->SetCursorProductChoices(m_AvailableProducts, m_Request.manufacturingRecipe);
		m_AvailableProducts.clear();
	}
	m_PreviousSiteWorld = world.Id().GetID(); m_PreviousSiteLocation = site.location;
}

bool DdPlanLegacyCity::OnKeyDown(QKeyEvent* event)
{
	switch(event->key())
	{
	case Qt::Key_Left:
		if(event->modifiers() & Qt::ControlModifier) ChangeProduct(-1);
		else ChangeRotation(1, event->modifiers() & Qt::ShiftModifier);
		return true;
	case Qt::Key_Right:
		if(event->modifiers() & Qt::ControlModifier) ChangeProduct(1);
		else ChangeRotation(-1, event->modifiers() & Qt::ShiftModifier);
		return true;
	case Qt::Key_Up: if(m_Request.industryId == 2) ChangeRotation(1, false); else ChangeMaterial(1); return true;
	case Qt::Key_Down: if(m_Request.industryId == 2) ChangeRotation(-1, false); else ChangeMaterial(-1); return true;
	case Qt::Key_Plus: case Qt::Key_Equal: ChangeLevels(1); return true;
	case Qt::Key_Minus: ChangeLevels(-1); return true;
	case Qt::Key_Space: m_HasStart = false; return true;
	default: return DdCommand::OnKeyDown(event);
	}
}

void DdPlanLegacyCity::OnMouseWheel(QWheelEvent* event)
{
	const int delta = event->angleDelta().y(); if(!delta) return;
	if(m_Request.industryId == 2) ChangeRotation(delta > 0 ? 1 : -1, false);
	else ChangeLevels(delta > 0 ? 1 : -1);
}

void DdPlanLegacyCity::ConstrainRoad(const DaWorldLL& world, AuPoint& point, bool constrain) const
{
	if(!m_HasStart) return;
	if(constrain && m_Angle > 0)
	{
		AuMatrix4x4 basis; world.BuildingConstructionTransform(m_Start, 0, nullptr, AuPoint(), basis);
		AuPoint delta = basis.Inverse().ProjectVector(point - m_Start);
		const double length = std::hypot(delta.x, delta.y);
		const double angle = std::round(std::atan2(delta.y, delta.x) * RAD2DEG / m_Angle) * m_Angle * DEG2RAD;
		point = basis.Project(AuPoint(std::cos(angle) * length, std::sin(angle) * length, 0));
	}
	AuGlobeLLF location;
	if(world.LLOfLoc(location, point))
		point = world.LocOfLL(location, m_Request.roadSlab ? world.AltitudeMSL(m_Start) : world.SurfaceAltitudeMSL(location));
}

void DdPlanLegacyCity::OnButtonDown(QMouseEvent* event)
{
	if(event->button() != Qt::LeftButton || !m_Interface || !g_Au.IsLegacyGeneration()) return;
	DaPerson* person = nullptr; m_pWnd->LockSceneAtom(&person);
	AuPoint point; DaWorldLL* world = person ? m_pWnd->FindTerrainLocationAtPixel(person, event->pos(), point) : nullptr;
	AuLegacyCityProtocol::Request request = m_Request;
	bool submit = false;
	if(world)
	{
		if(request.action == AuLegacyCityProtocol::Build && request.industryId != 2 && request.industryId != 129)
		{
			UpdateSite(*world, point);
			if(!m_MaximumLevels || m_SiteBlocked) { m_pWnd->UnlockSceneAtom(); return; }
			request = m_Request;
			request.levels = quint8(qMin(int(request.levels), m_MaximumLevels));
		}
		if(request.action == AuLegacyCityProtocol::ClearLand)
		{
			ClearTarget hit;
			if(!FindClearTarget(*world, point, hit) || !hit.allowed) { m_pWnd->UnlockSceneAtom(); return; }
			request.cityId = hit.city; request.developmentId = hit.development.id;
		}
		if((request.industryId == 2 || request.industryId == 129) && request.action == AuLegacyCityProtocol::Build && !m_HasStart)
		{ m_Start = point; m_StartWorld = world->Id().GetID(); m_HasStart = true; }
		else
		{
			if((request.industryId == 2 || request.industryId == 129) && m_HasStart)
			{
				if(m_StartWorld != world->Id().GetID()) { m_HasStart = false; m_pWnd->UnlockSceneAtom(); return; }
				if(request.industryId == 2) ConstrainRoad(*world, point, event->modifiers() & Qt::ShiftModifier);
				else
				{
					AuGlobeLLF radiusLocation;
					if(world->LLOfLoc(radiusLocation, point)) point = world->LocOfLL(radiusLocation, world->AltitudeMSL(m_Start));
				}
				AuPoint middle = request.industryId == 129 ? m_Start : (m_Start + point) / 2.;
				AuGlobeLLF center;
				if(world->LLOfLoc(center, middle))
					middle = world->LocOfLL(center, world->SurfaceAltitudeMSL(center));
				AuMatrix4x4 basis; world->BuildingConstructionTransform(middle, 0, nullptr, AuPoint(), basis);
				const AuPoint delta = basis.Inverse().ProjectVector(point - m_Start);
				request.headingDegrees = float(std::atan2(-delta.x, delta.y) * RAD2DEG);
				const float sizeMeters = float((point - m_Start).Length() / FeetPerMeter());
				if(request.industryId == 129) request.biodomeRadiusMeters = sizeMeters;
				else request.roadLengthMeters = sizeMeters;
				point = middle; m_HasStart = false;
			}
			AuGlobeLLF location;
			if(world->LLOfLoc(location, point))
			{
				if(request.action == AuLegacyCityProtocol::Build && request.industryId != 2 && request.industryId != 129)
					request.headingDegrees = float(std::remainder(request.headingDegrees - world->BuildingConstructionNorthAngle(point, nullptr) * RAD2DEG, 360.));
				request.worldId = world->Id().GetID(); request.latitudeRadians = location.Latitude();
				request.longitudeRadians = location.Longitude(); request.explicitLocation = true; submit = true;
			}
		}
	}
	m_pWnd->UnlockSceneAtom();
	if(submit && m_Interface) m_Interface->Submit(request);
}

void DdPlanLegacyCity::DeleteDeviceObjects(AuGLEffects& effects)
{
	Q_UNUSED(effects); if(m_PreviewVAO.VAO()) m_PreviewVAO.DeleteGL();
}

void DdPlanLegacyCity::RenderCursor(const AuMatrix4x4& projection, const AuMatrix4x4& camera,
	DaPerson* person, const QPoint& pixel, AuAtomRenderInfo& info, bool snapped)
{
	Q_UNUSED(snapped);
	if(!person || !g_Au.IsLegacyGeneration() || !info.re || !info.glw) return;
	AuPoint point; auto* world = m_pWnd->FindTerrainLocationAtPixel(person, pixel, point);
	auto* environment = person->FindEnvironmentAtom(); if(!world || !environment) return;
	AuMatrix4x4 scene, terrain, placement;
	if(!person->GetTransformRelativeToParent(scene, environment) || !world->GetTransformRelativeToParent(terrain, environment)) return;
	AuLegacyCityProtocol::Request preview = m_Request;
	if(m_MaximumLevels >= 0 && preview.industryId != 2 && preview.industryId != 129)
		preview.levels = quint8(m_ExistingLevels + qMin(int(preview.levels), m_MaximumLevels));
	if(m_SiteMaterial) { preview.material = m_SiteMaterial; preview.designId = m_SiteDesignId; }
	AuPoint roadEnd;
	const bool roadDrag = m_Request.industryId == 2 && m_HasStart && m_StartWorld == world->Id().GetID();
	if(preview.action == AuLegacyCityProtocol::Build && preview.industryId != 2 && preview.industryId != 129)
	{
		// SetCursor samples this site before drawing each frame. Reuse its
		// effective placement and limits instead of sampling the resources twice.
		point = m_SiteCenter; preview.headingDegrees = float(m_SiteFacing * RAD2DEG);
	}
	if(m_Request.industryId == 129 && m_HasStart && m_StartWorld == world->Id().GetID())
	{
		AuGlobeLLF radiusLocation;
		if(world->LLOfLoc(radiusLocation, point)) point = world->LocOfLL(radiusLocation, world->AltitudeMSL(m_Start));
		preview.biodomeRadiusMeters = float((point - m_Start).Length() / FeetPerMeter()); point = m_Start;
	}
	if(roadDrag)
	{
		ConstrainRoad(*world, point, QGuiApplication::keyboardModifiers() & Qt::ShiftModifier);
		roadEnd = point;
		AuPoint middle = (point + m_Start) / 2.;
		// Match the submitted site's center instead of placing the preview at
		// the chord midpoint beneath a curved world's surface.
		AuGlobeLLF center;
		if(world->LLOfLoc(center, middle)) middle = world->LocOfLL(center, world->SurfaceAltitudeMSL(center));
		// BuildingConstructionTransform appends to its matrix. Keep this
		// heading basis separate so the final placement is applied only once.
		AuMatrix4x4 basis;
		world->BuildingConstructionTransform(middle, 0, nullptr, AuPoint(), basis);
		const AuPoint delta = basis.Inverse().ProjectVector(point - m_Start);
		preview.headingDegrees = float(std::atan2(-delta.x, delta.y) * RAD2DEG);
		preview.roadLengthMeters = float((point - m_Start).Length() / FeetPerMeter()); point = middle;
	}
	world->BuildingConstructionTransform(point, preview.headingDegrees * DEG2RAD, nullptr, AuPoint(), placement);
	// Road edges below are world-relative: rebuilding a horizontal rectangle
	// at the moving midpoint changes the apparent start height on uneven land.
	const AuMatrix4x4 worldModel = camera * scene.Inverse() * terrain;
	AuMatrix4x4 model = roadDrag ? worldModel : worldModel * placement;
	QList<AuPointAlpha> vertices, ground;
	bool clearAllowed = true;
	if(m_Request.action == AuLegacyCityProtocol::ClearLand)
	{
		ClearTarget hit; if(!FindClearTarget(*world, point, hit)) return;
		ClearOutline(*world, hit, vertices, ground); clearAllowed = hit.allowed;
		model = worldModel;
	}
	else if(m_Request.industryId == 2)
	{
		const double width = preview.roadWidthMeters * FeetPerMeter() / 2;
		if(roadDrag)
		{
			// Follow the recovered road cursor: integrate from the saved first
			// endpoint, interpolating grade and projecting onto world curvature.
			const AuPoint direction = roadEnd - m_Start;
			const int segments = qMax(1, int(std::ceil(direction.Length() / (10 * FeetPerMeter()))));
			const double startAltitude = world->AltitudeMSL(m_Start), endAltitude = world->AltitudeMSL(roadEnd);
			AuPoint previousLeft, previousRight, firstLeft, firstRight;
			for(int segment = 0; segment <= segments; ++segment)
			{
				const double fraction = double(segment) / segments;
				AuPoint center = segment == 0 ? m_Start : segment == segments ? roadEnd : m_Start + direction * fraction;
				if(segment > 0 && segment < segments)
					world->ElevateLocMSL(center, startAltitude + (endAltitude - startAltitude) * fraction);
				const AuPoint up = -world->GravityDir(center);
				AuPoint right = direction.Cross(up);
				if(right.IsZero()) up.ArbitraryAxis(right);
				right.Unit(width);
				const AuPoint leftEdge = center - right, rightEdge = center + right;
				if(segment == 0)
				{
					firstLeft = leftEdge; firstRight = rightEdge; AddLine(vertices, leftEdge, rightEdge);
				}
				else
				{
					AddLine(vertices, previousLeft, leftEdge); AddLine(vertices, previousRight, rightEdge);
					if(segment == segments) AddLine(vertices, leftEdge, rightEdge);
				}
				previousLeft = leftEdge; previousRight = rightEdge;
			}
			if(preview.roadSlab) { AddLine(vertices, firstLeft, previousRight); AddLine(vertices, firstRight, previousLeft); }
		}
		else
		{
			// DdPlanGlobeRoad::RenderCursor (0x140324250): a 32-point circle
			// at half the selected road width, with one diameter for slabs.
			model = worldModel;
			const AuPoint up = -world->GravityDir(point);
			AuPoint axisX; up.ArbitraryAxis(axisX);
			const AuPoint axisY = up.Cross(axisX);
			AuPoint first, previous, opposite;
			for(int i = 0; i < 32; ++i)
			{
				const float angle = float(i) * PIf * 2 / 32;
				const AuPoint edge = point + axisX * (width * std::cos(angle)) + axisY * (width * std::sin(angle));
				if(!i) first = edge; else AddLine(vertices, previous, edge);
				if(i == 16) opposite = edge;
				previous = edge;
			}
			AddLine(vertices, previous, first);
			if(preview.roadSlab) AddLine(vertices, first, opposite);
		}
	}
	else if(m_Request.industryId == 129)
	{
		const double radius = preview.biodomeRadiusMeters * FeetPerMeter();
		const double edge = AuLegacyBd::Biodome::RoofHeight(radius, radius);
		for(int i = 0; i < 64; ++i)
		{
			const double a = i * PI / 32., b = (i + 1) * PI / 32.;
			AddLine(vertices, AuPoint(std::cos(a)*radius, std::sin(a)*radius, 1), AuPoint(std::cos(b)*radius, std::sin(b)*radius, 1));
			AddLine(vertices, AuPoint(std::cos(a)*radius, std::sin(a)*radius, edge), AuPoint(std::cos(b)*radius, std::sin(b)*radius, edge));
			if(i < 32)
			{
				const double x = std::cos(a)*radius, y = std::cos(b)*radius;
				const double h1 = AuLegacyBd::Biodome::RoofHeight(radius, std::abs(x));
				const double h2 = AuLegacyBd::Biodome::RoofHeight(radius, std::abs(y));
				AddLine(vertices, AuPoint(x, 0, h1), AuPoint(y, 0, h2));
				AddLine(vertices, AuPoint(0, x, h1), AuPoint(0, y, h2));
			}
		}
	}
	else if(!m_PreviewValid)
	{
		if(m_PreviewVAO.VAO()) m_PreviewVAO.DeleteGL();
		m_Preview.clear(); const auto mesh = Preview(preview);
		// Decode/build the authored geometry once per material selection, never
		// once per rendered frame. Rotation only changes the model matrix.
		for(const auto& batch : mesh.batches)
		{
			if(batch.interior || batch.collisionOnly) continue;
			for(int i = 0; i + 2 < batch.vertices.size(); i += 3)
			{
				const auto& a = batch.vertices[i].position; const auto& b = batch.vertices[i + 1].position;
				const auto& c = batch.vertices[i + 2].position;
				const AuPoint p(a.x(), a.y(), a.z()), q(b.x(), b.y(), b.z()), r(c.x(), c.y(), c.z());
				AddLine(m_Preview, p, q); AddLine(m_Preview, q, r); AddLine(m_Preview, r, p);
			}
		}
		if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
			if(const auto* recipe = catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId{preview.processId}))
			{
				const double radius = recipe->buildRadius * FeetPerMeter();
				const AuPoint a(-radius, -radius, 1), b(radius, -radius, 1), c(radius, radius, 1), d(-radius, radius, 1);
				AddLine(m_Preview, a, b); AddLine(m_Preview, b, c); AddLine(m_Preview, c, d); AddLine(m_Preview, d, a);
				// Retain the recovered front-marker radii for arrowhead sizing.
				double frontRadius = radius;
				switch(preview.industryId)
				{
				case 30: case 32: frontRadius = double(42.650917f); break;
				case 31: frontRadius = double(32.8084f); break;
				case 113: frontRadius = double(65.6168f); break;
				case 114: frontRadius = double(9.84252f); break;
				default: break;
				}
				// The tip meets the full site's +Y perimeter, including farms and parks.
				const AuPoint tip(0, radius, 1);
				AddLine(m_Preview, AuPoint(0, 0, 1), tip);
				const double arrowLength = qMin(qMin(frontRadius, 10 * FeetPerMeter()) * .25, 3 * FeetPerMeter());
				AddLine(m_Preview, tip, AuPoint(-arrowLength * .5, radius - arrowLength, 1));
				AddLine(m_Preview, tip, AuPoint(arrowLength * .5, radius - arrowLength, 1));
			}
		m_PreviewValid = true;
	}
	auto& effects = *info.re;
	GLboolean depthTest, depthWrite;
	effects.glGetBooleanv(GL_DEPTH_TEST, &depthTest);
	effects.glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
	// Original building/road placement cursors draw over terrain and scenery.
	// Preserve the caller's state so this overlay cannot affect later passes.
	effects.glDisable(GL_DEPTH_TEST); effects.glDepthMask(GL_FALSE);
	effects.GLSL().LineShader().Select(); effects.GLSL().LineShader().SetProjectionModelView(projection * model);
	if(m_Request.action == AuLegacyCityProtocol::ClearLand)
		effects.GLSL().LineShader().SetColor(.9f, clearAllowed ? .35f : .1f, .1f, .9f);
	else
	{
		const bool blocked = m_MaximumLevels == 0 || m_SiteBlocked;
		effects.GLSL().LineShader().SetColor(blocked ? .9f : .1f, blocked ? .1f : .9f, .1f, .9f);
	}
	effects.glEnable(GL_BLEND); effects.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if(!ground.isEmpty())
	{
		AuGLVAO vao; vao.CreateForSprites(effects, ground, GL_STREAM_DRAW);
		if(vao.VAO()) { vao.SelectGL(); vao.DrawGL(GL_LINES); vao.DeselectGL(); vao.DeleteGL(); }
	}
	effects.glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	if(m_Request.action != AuLegacyCityProtocol::ClearLand && m_Request.industryId != 2 && m_Request.industryId != 129)
	{
		if(!m_PreviewVAO.VAO()) m_PreviewVAO.CreateForSprites(effects, m_Preview, GL_STATIC_DRAW);
		if(m_PreviewVAO.VAO()) { m_PreviewVAO.SelectGL(); m_PreviewVAO.DrawGL(GL_LINES); m_PreviewVAO.DeselectGL(); }
	}
	else
	{
		AuGLVAO vao; vao.CreateForSprites(effects, vertices, GL_STREAM_DRAW);
		if(vao.VAO()) { vao.SelectGL(); vao.DrawGL(GL_LINES); vao.DeselectGL(); vao.DeleteGL(); }
	}
	if(m_Request.action == AuLegacyCityProtocol::Build && m_ConflictWorld == world->Id().GetID()
		&& !m_ConflictingPerimeters.isEmpty())
	{
		// Site outlines are already in world coordinates, unlike the proposed
		// building mesh. Draw their perimeters in amber over the scenery.
		effects.GLSL().LineShader().SetProjectionModelView(projection * worldModel);
		effects.GLSL().LineShader().SetColor(1.f, .7f, .05f, 1.f);
		effects.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		AuGLVAO vao; vao.CreateForSprites(effects, m_ConflictingPerimeters, GL_STREAM_DRAW);
		if(vao.VAO()) { vao.SelectGL(); vao.DrawGL(GL_LINES); vao.DeselectGL(); vao.DeleteGL(); }
	}
	effects.glDisable(GL_BLEND); effects.GLSL().LineShader().Deselect();
	effects.glDepthMask(depthWrite);
	if(depthTest) effects.glEnable(GL_DEPTH_TEST);
	if(m_Request.action == AuLegacyCityProtocol::ClearLand) return;
	QFont font; font.setPixelSize(14);
	if(m_Request.industryId != 2 && m_Request.industryId != 129)
	{
		if(!m_SiteMessage.isEmpty()) effects.RenderText(pixel.x() + 16, pixel.y() + 24, m_SiteMessage,
			font, QColor(255, 100, 70), info.glw->width(), info.glw->height());
		return;
	}
	const QString text = m_Request.industryId == 2 ? (m_HasStart ? QObject::tr("Click road end — %1 m").arg(preview.roadLengthMeters, 0, 'f', 1) : QObject::tr("Click road start"))
		: (m_HasStart ? QObject::tr("Click biodome radius — %1 m").arg(preview.biodomeRadiusMeters, 0, 'f', 1) : QObject::tr("Click biodome center"));
	effects.RenderText(pixel.x() + 16, pixel.y() + 24, text, font, QColor(255, 230, 100), info.glw->width(), info.glw->height());
}
