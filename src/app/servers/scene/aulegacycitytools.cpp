#include "dtchatchannel.h"
#include "auatominterface.h"
#include "aubox.h"
#include "audbglobal.h"
#include "auether.h"
#include "auglobal.h"
#include "aulegacybd.h"
#include "aulegacybuildinggeometry.h"
#include "aulegacycityprotocol.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityproduction.h"
#include "aulegacystudioprotocol.h"
#include "aulegacyshipequipment.h"
#include "aumatrix4x4.h"
#include "auqueryenvironment.h"
#include "auserverinterface.h"
#include "daatomlist.h"
#include "dgdata.h"
#include "dalegacycity.h"
#include "dadesigner.h"
#include "daperson.h"
#include "dasolarsystem.h"
#include "dasector.h"
#include "dagalaxy.h"
#include "daworldll.h"
#include "dtccpacket.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <limits>
#include <QMutex>
#include <QMutexLocker>

namespace
{
struct AvatarUnlock
{
	AuAtomInterface* target;
	~AvatarUnlock() { target->UnlockAvatar(); }
};

bool ContainsSite(DaWorldLL& world, const AuLegacyCity::Development& dev, const AuPoint& point)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue) return false;
	double width = 0, length = 0;
	if(dev.industryId == 2)
	{
		width = dev.roadWidthMeters * FeetPerMeter() / 2;
		length = dev.roadLengthMeters * FeetPerMeter() / 2;
	}
	else
	{
		if(dev.industryId == 129) width = length = dev.biodomeRadiusMeters * FeetPerMeter();
		else for(const auto& recipe : catalogue->ConstructionProcesses())
			if(recipe.industry.value == dev.industryId) { width = length = recipe.buildRadius * FeetPerMeter(); break; }
	}
	if(width <= 0 || length <= 0) return false;
	const AuPoint center = world.LocOfLL(dev.location, dev.altitudeMSL);
	if(center.Distance2(point) > std::pow(width + length + 100, 2)) return false;
	AuMatrix4x4 placement;
	world.BuildingConstructionTransform(center, dev.facingRadians, nullptr, AuPoint(), placement);
	AuPoint local = placement.UnProject(point);
	if(dev.industryId == 30)
	{
		// Match the labor/runtime field footprint on sloping and curved ground.
		AuGlobeLLF location;
		if(!world.LLOfLoc(location, point)) return false;
		local = placement.UnProject(world.LocOfLL(location, dev.altitudeMSL));
	}
	return dev.industryId == 129 ? local.x*local.x + local.y*local.y <= width*width
		: std::abs(local.x) <= width + 1 && std::abs(local.y) <= length + 1;
}

double DistanceToSite(DaWorldLL& world, const AuLegacyCity::Development& dev, const AuPoint& point)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	const AuPoint center = world.LocOfLL(dev.location, dev.altitudeMSL);
	double width = 0, length = 0;
	if(dev.industryId == 2)
	{ width = dev.roadWidthMeters * FeetPerMeter()/2; length = dev.roadLengthMeters * FeetPerMeter()/2; }
	else if(dev.industryId == 129) width = length = dev.biodomeRadiusMeters * FeetPerMeter();
	else if(catalogue)
		for(const auto& recipe : catalogue->ConstructionProcesses())
			if(recipe.industry.value == dev.industryId) { width = length = recipe.buildRadius * FeetPerMeter(); break; }
	// A tangent projection must not turn a distant antipode into an adjacent site.
	if(center.Distance2(point) > std::pow(width + length + 2000*FeetPerMeter(), 2)) return center.Distance2(point);
	AuMatrix4x4 placement;
	world.BuildingConstructionTransform(center, dev.facingRadians, nullptr, AuPoint(), placement);
	const AuPoint local = placement.UnProject(point);
	if(dev.industryId == 129)
	{ const double gap = qMax(0.0, std::hypot(local.x,local.y)-width); return gap*gap; }
	const double dx = qMax(0.0,std::abs(local.x)-width), dy = qMax(0.0,std::abs(local.y)-length);
	return dx*dx+dy*dy;
}

void EconomyRequest(const AuLegacyCityProtocol::Request& request, AuLegacyCityProtocol::Reply& reply,
	DaPerson& person, DaWorldLL& world, AuEther& ether, const AuPoint& origin)
{
	using namespace AuLegacyCityProtocol;
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue) { reply.message = QObject::tr("The legacy economy catalogue is unavailable."); return; }
	// Production snapshots include a tool inventory. Require the complete,
	// active descriptors before either the operation or the reply decodes it.
	if(!g_Au.IsLegacyGeneration() || !catalogue->IsValid())
	{ reply.message = QObject::tr("Legacy commodity metadata is not ready."); return; }
	for(const quint16 commodity : catalogue->RuntimeCommodityIds())
		if(!g_Au.HasLegacyCommodity(COMMODITYID(commodity)))
		{ reply.message = QObject::tr("Legacy commodity metadata is not ready."); return; }
	quint32 cityId = request.cityId, developmentId = request.developmentId;
	if(request.action == EconomyList && !cityId && !developmentId)
	{
		const auto* environment = person.LockEnvironmentQuery();
		cityId = environment->LegacyCityId().GetID(); developmentId = environment->LegacyDevelopmentId();
		person.UnlockEnvironmentQuery();
	}
	auto* city = dynamic_cast<DaLegacyCity*>(ether.FindNormalAtom(AuId(cityId)));
	if(!city || city->IsDead() || city->Parent() != &world
		|| (request.action == SetIndustryProcess && (!person.EmpireId() || person.EmpireId() != city->EmpireId())))
	{
		if(request.action != EconomyList) reply.message = QObject::tr("You must govern this city to configure its industry.");
		return;
	}
	auto state = city->Snapshot();
	const AuLegacyCity::Development* selected = nullptr;
	for(const auto& development : state.developments)
		if(development.id == developmentId && !development.demolished) { selected = &development; break; }
	// A saved or stale UI target cannot operate a city from another site.
	if(!selected || !ContainsSite(world, *selected, origin))
	{ if(request.action != EconomyList) reply.message = QObject::tr("Stand at this industry to operate it."); return; }
	reply.selectedCity = cityId; reply.selectedDevelopment = developmentId;
	if(request.action == SetIndustryProcess)
		reply.accepted = city->SetIndustryProcess(person, developmentId, request.processSlot,
			request.manufacturingRecipe, request.minimumTechnology, request.citizenEnabled, reply.message);
	else if(request.action == FetchIndustryMaterials || request.action == RunIndustryProcess)
		reply.accepted = city->OperateIndustryProcess(person, developmentId, request.processSlot, true,
			request.action == RunIndustryProcess, reply.message);
	// TEMPORARY ARCHITECT MANUFACTURING TEST: the city method rechecks active architect powers.
	else if(request.action == TestingFinishIndustryProcess)
		reply.accepted = city->SkipIndustryProcessWaitForTesting(person, developmentId, request.processSlot, reply.message);
	else reply.accepted = true;
	state = city->Snapshot();
	AuLegacyCity::ProductionState production, visible;
	QString error;
	if(!AuLegacyCity::DecodeProductionState(*catalogue, state.manufacturing, production, error))
	{ reply.message = error; return; }
	for(const auto& process : production.processes)
		if(process.developmentId == developmentId
			&& (person.EmpireId() == state.controllingEmpire || process.enabled)) visible.processes.append(process);
	// This selected-development presentation never contains city inventory,
	// accounts, another development's recipes or the process tool reserve.
	if(!AuLegacyCity::EncodeProductionState(*catalogue, visible, reply.production, error))
	{ reply.message = error; return; }
	for(const auto& development : state.developments)
		if(development.id == developmentId)
		{
			reply.economyIndustry = development.industryId; reply.economyTechnology = development.technology;
			reply.economyLevels = development.completedLevels; reply.economyEmployees = development.employees;
			if(development.industryId == 50 || development.industryId == 51 || development.industryId == 89)
			{
				const auto* construction = catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(development.constructionProcessId));
				if(!construction) construction = catalogue->ConstructionProcessForIndustry(AuLegacyCity::IndustryId(development.industryId));
				const double radius = construction ? construction->buildRadius * FeetPerMeter() : 0;
				const float rockRadius = float(radius);
				AuMatrix4x4 placement;
				world.BuildingConstructionTransform(world.LocOfLL(development.location, development.altitudeMSL),
					development.facingRadians, nullptr, AuPoint(), placement);
				const AuPoint center = placement.Translation();
				float water = 0;
				if(development.industryId == 89)
				{
					unsigned int rocks = 0, shrubs = 0, trees = 0;
					world.ResourceDistributionAt(placement, center, AuPoint(radius, radius, 0), rocks, shrubs, trees, water, 0);
				}
				const quint8 zone = quint8(world.OrbitZone());
				const auto choices = AuLegacyCity::ProductionRecipeChoices(*catalogue, development.industryId,
					[&](quint16 commodity) {
						// QueryWellResources supplies water throughout a liquid-water world.
						if(development.industryId == 51 && commodity == 83) return world.Hydrographics() && zone != 0 && zone != 4;
						return world.TestRockResource(placement, center, AuPoint(rockRadius, rockRadius, 0),
							COMMODITYID(catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId(commodity))));
					}, water != 0, zone);
				reply.hasRecipeChoices = true;
				for(const auto id : choices) reply.recipeChoices.append(id.value);
			}
			break;
		}
	reply.hasEconomy = true;
}

void BankRequest(const AuLegacyCityProtocol::Request& request, AuLegacyCityProtocol::Reply& reply,
	DaPerson& person, DaWorldLL& world, AuEther& ether)
{
	AuLegacyCityProtocol::Request selected = request;
	if(selected.action == AuLegacyCityProtocol::BankState && !selected.cityId && !selected.developmentId)
	{
		const auto* environment = person.LockEnvironmentQuery();
		selected.cityId = environment->LegacyCityId().GetID();
		selected.developmentId = environment->LegacyDevelopmentId();
		person.UnlockEnvironmentQuery();
	}
	auto* city = dynamic_cast<DaLegacyCity*>(ether.FindNormalAtom(AuId(selected.cityId)));
	if(!city || city->IsDead() || city->Parent() != &world)
	{
		if(selected.action != AuLegacyCityProtocol::BankState)
			reply.message = QObject::tr("This bank is no longer on your current world. Refresh the bank window.");
		return;
	}
	city->OperateBank(person, selected, reply);
}

void StudioRequest(const AuLegacyCityProtocol::Request& request, AuLegacyCityProtocol::Reply& reply,
	DaPerson& person, DaWorldLL& world, AuEther& ether, const AuPoint& origin)
{
	using namespace AuLegacyCityProtocol;
	// Check and reserve a studio together, including requests from other players.
	static QMutex studioLock;
	QMutexLocker lock(&studioLock);
	const auto* city = dynamic_cast<const DaLegacyCity*>(ether.FindNormalAtom(AuId(request.cityId)));
	if(!city || city->IsDead() || city->Parent() != &world)
	{ reply.message = QObject::tr("This design studio is no longer on your current world."); return; }
	const auto state = city->Snapshot();
	const AuLegacyCity::Development* selected = nullptr;
	for(const auto& development : state.developments)
		if(development.id == request.developmentId && !development.demolished)
		{ selected = &development; break; }
	if(!selected || selected->industryId != 3 || !selected->completedLevels
		|| !ContainsSite(world, *selected, origin))
	{ reply.message = QObject::tr("Stand at a completed Design Studio to enter a designer."); return; }
	reply.selectedCity = request.cityId; reply.selectedDevelopment = request.developmentId;
	reply.hasStudio = true; reply.studioCount = qMin(int(selected->completedLevels), 16);
	DaDesigner* selectedStudio = nullptr;
	bool createdStudio = false;
	DaAtomList atoms; world.ListNormalSubatoms(atoms);
	for(auto* atom : atoms)
		if(auto* studio = dynamic_cast<DaDesigner*>(atom))
		{
			const quint16 index = studio->StudioId() ^ PrivateStudioIdBase;
			if(index < reply.studioCount && studio->FindPersonRecursive()
				&& ContainsSite(world, *selected, studio->Location()))
			{
				reply.occupiedStudios |= quint16(1u << index);
				if(index == request.studioIndex) selectedStudio = studio;
			}
		}
	if(request.action == StudioState) { reply.accepted = true; return; }
	if(person.Parent() != &world)
	{ reply.message = QObject::tr("Dismount and exit your vehicle or spacecraft before entering a design studio."); return; }
	if(request.studioIndex >= reply.studioCount)
	{ reply.message = QObject::tr("That studio level is not completed."); return; }
	if(selectedStudio)
	{
		const QString password = selectedStudio->EntryPassword();
		// The original blank password means private, including another blank.
		if(password.isEmpty() || request.studioPassword != password)
		{ reply.message = QObject::tr("This studio is private or its password does not match."); return; }
		DaAtomList members; selectedStudio->FindPeopleRecursive(members);
		if(members.size() >= AuLegacyStudioProtocol::MaximumMembers)
		{ reply.message = QObject::tr("This design studio is full."); return; }
		QString error;
		const DaPerson* first = selectedStudio->FindPersonRecursive();
		if(!first || !selectedStudio->InitializeLegacyStudio(*first, error))
		{ reply.message = error; return; }
	}
	else
	{
		if(!g_AuDb.HasAtomIds())
		{ reply.message = QObject::tr("The server is restocking atom IDs. Try again shortly."); return; }
		std::unique_ptr<DaDesigner> studio(new DaDesigner(PrivateStudioIdBase | request.studioIndex));
		studio->Create();
		if(studio->Id().IsNull())
		{ reply.message = QObject::tr("The server could not allocate a studio ID."); return; }
		QString error;
		if(!studio->InitializeLegacyStudio(person, error)) { reply.message = error; return; }
		studio->ConnectSignals();
		studio->SetPosAndOrient(person.Pos()); // All participants leave at the original studio entrance.
		selectedStudio = studio.release();
		createdStudio = true;
		ether.AddAtom(selectedStudio);
		g_Au.AtomAdded(selectedStudio->Id());
	}
	person.ClearMovementInput();
	person.SetDesignPassword(request.studioPassword);
	person.DismountAllRiders();
	AuMatrix4x4 inside; inside.Translate(0, 0, person.GroundHeightAboveCG());
	person.ForcePosStop(AuAtomPos(selectedStudio->Id(), inside));
	person.SetModified();
	// The shared working document remains separate from modern AuScDesign.
	// Each participant's editor uses the dedicated studio protocol.
	if(createdStudio) { selectedStudio->SetNormal(); selectedStudio->TickTockRecursive(); }
	reply.occupiedStudios |= quint16(1u << request.studioIndex);
	reply.accepted = true;
}

void ListBuildings(DaWorldLL& world, const AuPoint& origin, AuLegacyCityProtocol::Reply& reply)
{
	reply.hasList = true;
	DaAtomList atoms; world.ListNormalSubatoms(atoms);
	QVector<DaLegacyCity*> cities;
	for(DaAtom* atom : atoms)
		if(auto* city = dynamic_cast<DaLegacyCity*>(atom)) cities.append(city);
	const bool hasSite = reply.action == AuLegacyCityProtocol::ConstructionList
		&& reply.selectedCity && reply.selectedDevelopment;
	std::sort(cities.begin(), cities.end(), [&reply, hasSite](const DaLegacyCity* a, const DaLegacyCity* b) {
		if(hasSite && (a->Id().GetID() == reply.selectedCity) != (b->Id().GetID() == reply.selectedCity))
			return a->Id().GetID() == reply.selectedCity;
		return a->Id() < b->Id();
	});
	double closestSite = std::numeric_limits<double>::max();
	quint64 replyBytes = 128 + reply.message.toUtf8().size();
	for(const auto* city : cities)
	{
		auto state = city->Snapshot();
		if(hasSite && city->Id().GetID() == reply.selectedCity)
			std::stable_sort(state.developments.begin(), state.developments.end(), [&reply](const AuLegacyCity::Development& a,
				const AuLegacyCity::Development& b) { return a.id == reply.selectedDevelopment && b.id != reply.selectedDevelopment; });
		for(const auto& dev : state.developments)
		{
			if(!AuLegacyCityProtocol::IsGameplay(reply.action) && !dev.designId) continue;
			if(world.LocOfLL(dev.location, dev.altitudeMSL).Distance2(origin) > std::pow(2000 * FeetPerMeter(), 2)) continue;
			if(reply.entries.size() == AuLegacyCityProtocol::MaximumEntries) { reply.truncated = true; return; }
			if(!hasSite && reply.action == AuLegacyCityProtocol::ConstructionList
				&& ContainsSite(world, dev, origin))
			{
				const double distance = world.LocOfLL(dev.location, dev.altitudeMSL).Distance2(origin);
				if(distance < closestSite)
				{
					closestSite = distance; reply.selectedCity = city->Id().GetID(); reply.selectedDevelopment = dev.id;
				}
			}
			AuLegacyCityProtocol::Entry e;
			e.cityId = city->Id().GetID(); e.developmentId = dev.id; e.designId = dev.designId;
			e.material = dev.materialId; e.minimumDeck = dev.minimumDeck; e.maximumDeck = dev.maximumDeck;
			e.constructed = dev.constructed;
			e.elevationMeters = float((dev.altitudeMSL - world.SurfaceAltitudeMSL(dev.location)) / FeetPerMeter());
			e.headingDegrees = float(dev.facingRadians * RAD2DEG);
			e.name = state.name;
			e.industryId = dev.industryId; e.processId = dev.constructionProcessId;
			e.completedLevels = dev.completedLevels; e.targetLevels = dev.targetLevels;
			e.halted = dev.halted; e.citizenEnabled = dev.citizenWorker;
			e.levelLaborRemaining = dev.laborRemaining;
			e.components = dev.components;
			if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
			{
				QStringList requirements;
				for(const auto& component : dev.components)
				{
					if(component.applied >= qMax(quint32(1),component.quantity)) continue;
					const auto* commodity = catalogue->Commodity(AuLegacyCity::CommodityId(component.commodityId));
					const QString name = commodity ? commodity->descriptor.m_name : QString::number(component.commodityId);
					requirements.append(component.quantity ? QStringLiteral("%1 %2").arg(component.quantity-component.applied).arg(name)
						: QStringLiteral("Tool: %1").arg(name));
				}
				e.materials = requirements.join(QStringLiteral(", "));
				// A reply remains encodable even for very large original bills.
				while(e.materials.toUtf8().size() > 1024) e.materials.chop(1);
			}
			// Leave room for the envelope and fixed fields as well as bounded
			// strings/components, even if a restored city has unusually large bills.
			const quint64 entryBytes = 128 + e.name.toUtf8().size() + e.materials.toUtf8().size()
				+ quint64(e.components.size()) * 16;
			if(replyBytes + entryBytes > AuLegacyCityProtocol::MaximumReplyBytes)
			{ reply.truncated = true; return; }
			replyBytes += entryBytes;
			reply.entries.append(e);
		}
	}
}

bool PlacementClear(DaWorldLL& world, const AuLegacyCity::Development& dev,
	const AuLegacyBd::Geometry::Mesh& mesh, const DaAtom* ignore)
{
	AuMatrix4x4 transform;
	world.BuildingConstructionTransform(world.LocOfLL(dev.location, dev.altitudeMSL), dev.facingRadians,
		nullptr, AuPoint(), transform);
	AuBox bounds(AuPoint(mesh.minimum.x(), mesh.minimum.y(), mesh.minimum.z()),
		AuPoint(mesh.maximum.x(), mesh.maximum.y(), mesh.maximum.z()));
	bounds.Transform(transform);
	DaAtomList peers; world.ListNormalSubatoms(peers);
	for(DaAtom* peer : peers)
	{
		if(peer == ignore || peer->IsEthereal()) continue;
		AuBox occupied;
		if(!peer->RoughExtents(occupied)) continue;
		occupied.Transform(peer->Transform());
		if(bounds.DoesIntersect(occupied)) return false;
	}
	return true;
}
}

void DtChatChannel::ReadLegacyCityPacket(const QByteArray& payload)
{
	using namespace AuLegacyCityProtocol;
	Request request;
	if(!LegacyCityAuthorized() || !DecodeRequest(payload, request)) { Disconnect(); return; }
	Reply reply; reply.token = request.token; reply.action = request.action;
	if(!IsGameplay(request.action) && !LegacyScAuthorized(true)) reply.message = tr("Enable architect or programmer powers to use Legacy city test tools.");
	else if(m_LastLegacyCityRequest.isValid() && m_LastLegacyCityRequest.elapsed() < 200)
	{
		reply.message = tr("Wait a moment before sending another city request.");
		// A harmless transport retry must not switch off automatic site work.
		reply.accepted = request.action == Contribute;
	}
	else
	{
		m_LastLegacyCityRequest.restart();
		if(IsGameplay(request.action)) ProcessLegacyConstructionRequest(request, reply);
		else ProcessLegacyCityRequest(request, reply);
	}
	const QByteArray response = EncodeReply(reply);
	if(response.isEmpty()) return;
	QByteArray packet(1, char(DtChatChannelPacket::PD_LegacyCity)); packet.append(response); SendPacket(packet);
}

bool DtChatChannel::LegacyCityAuthorized()
{
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || !m_idPlayer || !m_confirmNum || UnitId().IsNull()) return false;
	AuId units[6]; ACCOUNT account; AuTime expiry;
	if(!g_Au.IsPlayerLoggedIn(m_idPlayer, m_confirmNum, units, account, expiry)) return false;
	for(const AuId& unit : units) if(unit == UnitId()) return true;
	return false;
}

void DtChatChannel::ProcessLegacyConstructionRequest(const AuLegacyCityProtocol::Request& request,
	AuLegacyCityProtocol::Reply& reply)
{
	using namespace AuLegacyCityProtocol;
	// An occupied-site query with no active avatar/world clears F10's target.
	// A transport-rate retry never reaches here and leaves that target intact.
	if(request.action == ConstructionList) reply.hasList = true;
	if(!LegacyCityAuthorized()) { reply.message = tr("Your session changed. Reconnect before building."); return; }
	DaPerson* person = m_I->LockAvatar(); AvatarUnlock unlock{m_I};
	if(!person || person->IsDead()) { reply.message = tr("An active avatar is required."); return; }
	auto* world = dynamic_cast<DaWorldLL*>(person->FindParentPlanetInEnv());
	AuEther* ether = person->Ether(); AuMatrix4x4 avatarTransform;
	if(!world || !ether || !person->GetTransformRelativeToParent(avatarTransform, world))
	{ reply.message = tr("Move onto a planet or ringworld to work on construction."); return; }
	const AuPoint origin = avatarTransform.Translation();
	if(IsEconomy(request.action))
	{ EconomyRequest(request, reply, *person, *world, *ether, origin); return; }
	if(IsBank(request.action))
	{ BankRequest(request, reply, *person, *world, *ether); return; }
	if(IsStudio(request.action))
	{ StudioRequest(request, reply, *person, *world, *ether, origin); return; }
	if(request.action == ConstructionList)
	{
		// Use the same environment target as F10. This also prefers the actual
		// building over its enclosing biodome and keeps it within a bounded list.
		const auto* environment = person->LockEnvironmentQuery();
		reply.selectedCity = environment->LegacyCityId().GetID();
		reply.selectedDevelopment = environment->LegacyDevelopmentId();
		person->UnlockEnvironmentQuery();
		reply.accepted = true; ListBuildings(*world, origin, reply); return;
	}
	const bool personalLabor = request.action == Contribute || request.action == FetchMaterials;
	if(!personalLabor && !person->EmpireId())
	{ reply.message = tr("Join an empire before founding or developing a city."); return; }

	AuGlobeLLF location;
	AuPoint site = origin;
	if(request.explicitLocation)
	{
		if(request.worldId != world->Id().GetID()) { reply.message = tr("The selected site is on another world."); return; }
		location = AuGlobeLLF(request.longitudeRadians, request.latitudeRadians);
		site = world->LocOfLL(location, world->SurfaceAltitudeMSL(location));
	}
	else
	{
		AuPoint up = -world->GravityDir(origin); up.Unit();
		AuPoint forward = avatarTransform.ProjectVector(AuPoint(0, 1, 0));
		forward -= up * forward.Dot(up);
		if(forward.IsZero()) up.ArbitraryAxis(forward);
		forward.Unit(); site += forward * (request.distanceMeters * FeetPerMeter());
		if(!world->LLOfLoc(location, site)) { reply.message = tr("Select a site within this world's terrain."); return; }
		site = world->LocOfLL(location, world->SurfaceAltitudeMSL(location));
	}
	if((request.action == Build || (request.action == ClearLand && request.explicitLocation))
		&& site.Distance2(origin) > std::pow(500 * FeetPerMeter(), 2))
	{ reply.message = tr("Move within 500 meters of the construction site."); return; }

	DaLegacyCity* city = nullptr;
	AuLegacyCity::State state;
	if(request.cityId)
	{
		city = dynamic_cast<DaLegacyCity*>(ether->FindNormalAtom(AuId(request.cityId)));
		if(!city || city->Parent() != world || city->IsDead())
		{ reply.message = tr("That city is no longer on your current world. Refresh the list."); return; }
		state = city->Snapshot();
		if(!personalLabor && state.controllingEmpire != person->EmpireId())
		{ reply.message = tr("That city belongs to another empire."); return; }
	}

	DaAtomList peers; world->ListNormalSubatoms(peers);
	const bool founding = request.action == Build && request.industryId == 1;
	std::unique_ptr<DaLegacyCity> created;
	if(founding)
	{
		if(!IsNameValid(request.cityName)) { reply.message = tr("Enter a valid city name (1 to 30 characters)."); return; }
		for(const DaAtom* atom : peers)
			if(const auto* other = dynamic_cast<const DaLegacyCity*>(atom))
				if(other->Name().compare(request.cityName, Qt::CaseInsensitive) == 0)
				{ reply.message = tr("This world already has a city with that name."); return; }
		AuIdList places; world->ChainOfPlaces(places);
		const DgData& gd = g_Au.LockDataForRead();
		const bool authority = gd.IsEmpirePlaceInDomain(person->EmpireId(), places, person->Id());
		g_Au.UnlockData();
		if(!authority && world->ClaimedByPersonId() != person->Id())
		{ reply.message = tr("You lack authority to found a city on this world."); return; }
		if(!g_AuDb.HasAtomIds()) { reply.message = tr("The server is restocking atom IDs. Try again shortly."); return; }
		created.reset(new DaLegacyCity); created->Create();
		if(created->Id().IsNull()) { reply.message = tr("The server could not allocate a city ID."); return; }
		AuMatrix4x4 placement;
		world->BuildingConstructionTransform(site, request.headingDegrees * DEG2RAD, nullptr, AuPoint(), placement);
		created->SetPosAndOrient(AuAtomPos(world->Id(), placement));
		state = AuLegacyCity::State(); state.name = request.cityName;
		state.controllingEmpire = state.allegianceEmpire = person->EmpireId();
		state.lastEmployment = state.lastPopulationCycle = AuTime::CurrentMSecsSinceEpoch();
		QString error;
		if(!created->SetState(state, error)) { reply.message = error; return; }
		// Resolve the parent for validation while retaining private ownership.
		// The atom is published to the ether only after founding succeeds.
		created->SetEther(ether);
		city = created.get();
	}
	else if(!city && (request.action == Build || (request.action == ClearLand && request.explicitLocation)))
	{
		// Ownership is selected from the existing city network. BeginConstruction
		// checks the actual road/site adjacency; distance alone grants no territory.
		double closest = std::numeric_limits<double>::max();
		for(DaAtom* atom : peers)
			if(auto* candidate = dynamic_cast<DaLegacyCity*>(atom))
			{
				if(candidate->IsDead() || candidate->EmpireId() != person->EmpireId()) continue;
				const auto candidateState = candidate->Snapshot();
				for(const auto& dev : candidateState.developments)
				{
					const double distance = DistanceToSite(*world, dev, site);
					if(distance < closest) { closest = distance; city = candidate; state = candidateState; }
				}
			}
	}
	if(!city) { reply.message = tr("Found a city with a town square first, then extend its roads and buildings."); return; }
	if(!founding && !personalLabor
		&& !person->HasArchitectPower() && !person->HasProgrammerPower())
	{
		AuIdList places; world->ChainOfPlaces(places); places.prepend(city->Id());
		const DgData& gd = g_Au.LockDataForRead();
		const bool authority = gd.IsEmpirePlaceInDomain(person->EmpireId(),places,person->Id());
		g_Au.UnlockData();
		if(!authority) { reply.message = tr("You must be in this city's chain of command to change its construction."); return; }
	}

	quint32 developmentId = request.developmentId;
	if(request.action == Build || (request.action == ClearLand && request.explicitLocation && !request.developmentId))
	{
		// Clicking an existing building requests an extension. Its stored design,
		// material, orientation and recipe remain authoritative.
		// Match placement's FIND_SkipLandscapers filter; clearing can still select them.
		developmentId = 0;
		for(const auto& dev : state.developments)
			if((request.action != Build || !dev.IsLandscaper()) && ContainsSite(*world, dev, site))
			{
				if(request.action == Build && (request.industryId == 2 || founding)) continue;
				if(request.action == Build && (request.industryId == 129 || dev.industryId == 129)) continue;
				if(request.action == Build && request.industryId != dev.industryId)
				{ reply.message = tr("An extension must use the existing building's industry."); return; }
				developmentId = dev.id; break;
			}
	}
	else
	{
		bool found = false;
		for(const auto& dev : state.developments) if(dev.id == developmentId)
		{
			found = true;
			if(world->LocOfLL(dev.location, dev.altitudeMSL).Distance2(origin) > std::pow(2000 * FeetPerMeter(), 2))
			{ reply.message = tr("Move within 2 km of the selected development."); return; }
			if(request.action == Contribute || request.action == FetchMaterials)
			{
				// The runtime checks vertical reach against road grade or the
				// constructed decks/dome, rather than the site's tangent plane.
				if(!ContainsSite(*world, dev, origin))
					return; // Leaving the site stops continuous work without a Thoughts message.
			}
			break;
		}
		if(!found) { reply.message = tr("That development no longer exists. Refresh the list."); return; }
	}

	QString error; bool changed = false;
	switch(request.action)
	{
	case Build:
		if(developmentId && !founding) changed = city->ExtendConstruction(*person, developmentId, request.levels, error,
			request.manufacturingRecipe, request.citizenEnabled);
		else changed = city->BeginConstruction(*person, request.industryId, request.processId, request.designId,
			request.material, location, request.headingDegrees * DEG2RAD, request.levels, error,
			request.roadLengthMeters, request.roadWidthMeters, request.roadSlab, request.biodomeRadiusMeters,
			request.manufacturingRecipe, request.citizenEnabled);
		if(changed && !developmentId) developmentId = city->Snapshot().developments.constLast().id;
		break;
	case Extend: changed = city->ExtendConstruction(*person, developmentId, request.levels, error,
		request.manufacturingRecipe, request.citizenEnabled); break;
	case Halt: case Resume: changed = city->SetConstructionHalted(*person, developmentId, request.action == Halt, error); break;
	case Contribute: changed = city->ContributeConstructionLabor(*person, developmentId, error, &reply.accepted); break;
	case FetchMaterials: changed = city->FetchConstructionMaterials(*person, developmentId, error); break;
	case EmployCitizen: changed = city->EmployConstructionWorker(*person, developmentId, request.citizenEnabled, error); break;
	case ChangeRoad: changed = city->ChangeRoadConstruction(*person, developmentId, request.processId, error); break;
	case Demolish: case ClearLand:
		if(!developmentId) { error = tr("Select an existing development to clear."); break; }
		changed = city->DemolishDevelopment(*person, developmentId, request.action == ClearLand, error); break;
	default: return;
	}
	if(!changed)
	{
		reply.message = error; ListBuildings(*world, origin, reply); return;
	}
	reply.accepted = true;
	if(request.action == Demolish || request.action == ClearLand)
	{
		for(const auto& removed : state.developments) if(removed.id == developmentId)
			for(DaAtom* actor : peers)
			{
				AuPoint actorLocation;
				if(actor->CanSetInMotion() && actor->GetLocationRelativeToParent(actorLocation, world)
					&& ContainsSite(*world, removed, actorLocation))
				{ actor->SetInMotion(); actor->SetModified(); }
			}
	}
	if(created)
	{
		created->ConnectSignals(); city = created.release(); ether->AddAtom(city);
		city->SetNormal(); city->SetModified(); g_Au.AtomAdded(city->Id());
		if(!world->HasWorkerSpecies()) { world->SetWorkerSpecies(person->Species()); world->SetModified(); }
		if(auto* system = person->FindParentSolarSystemInEnv())
		{
			const auto* sector = system->FindParentSector();
			const auto* galaxy = system->FindParentGalaxy();
			if(sector && galaxy)
			{
				AuPoint systemLocation; system->GetLocationRelativeToParent(systemLocation, galaxy);
				emit CityCreated(person->EmpireId(), quint8(CITY_City_Obsolete), city->Id(), request.cityName,
					person->Id(), person->Name(), world->Id(), world->Name(), system->Id(), system->Name(), systemLocation,
					sector->Id(), sector->SectorName(), galaxy->Id(), quint8(galaxy->Galaxy()));
			}
		}
	}
	reply.selectedCity = city->Id().GetID(); reply.selectedDevelopment = developmentId;
	if(city->Snapshot().developments.isEmpty())
	{
		city->Kill();
		if(city->IsDead())
		{
			emit city->CityDecayed(state.controllingEmpire,city->Id());
			world->TrashSubatom(city->Id()); reply.selectedCity = reply.selectedDevelopment = 0;
		}
	}
	if(request.action == Build || request.action == Extend || request.action == Demolish
		|| request.action == ClearLand || request.action == ChangeRoad) world->InvalidateEnvironmentRecursive();
	g_AuDb.PostCriticalUpdates();
	reply.message = error;
	ListBuildings(*world, origin, reply);
}

void DtChatChannel::ProcessLegacyCityRequest(const AuLegacyCityProtocol::Request& request, AuLegacyCityProtocol::Reply& reply)
{
	using namespace AuLegacyCityProtocol;
	if(request.action == RetiredDoors)
	{
		reply.message = tr("Legacy building doorways and hatches always remain open."); return;
	}
	QSharedPointer<const AuLegacyBd::Design> design;
	AuLegacyBd::Geometry::Mesh mesh;
	if(request.action == Spawn || request.action == Update)
	{
		design = AuLegacyBd::Catalog::BuiltIn().Find(request.designId);
		if(!design) { reply.message = tr("The selected original public blueprint is unavailable."); return; }
		AuLegacyBd::Geometry::Options options;
		options.minimumDeck = request.minimumDeck; options.maximumDeck = request.maximumDeck;
		options.materialCommodity = request.material; options.constructed = request.constructed;
		mesh = AuLegacyBd::Geometry::Build(*design, options);
		if(!mesh.TriangleCount()) { reply.message = tr("These deck limits do not produce building geometry."); return; }
	}
	// Geometry preparation above does not hold the scene deletion lock. Recheck
	// session ownership and active powers before accessing or changing any atom.
	if(!LegacyScAuthorized(false)) { reply.message = tr("Your session changed. Refresh after reconnecting."); return; }
	DaPerson* person = m_I->LockAvatar(); AvatarUnlock unlock{m_I};
	if(!person || !(person->HasArchitectPower() || person->HasProgrammerPower()))
	{ reply.message = tr("Enable architect or programmer powers first."); return; }
	auto* world = dynamic_cast<DaWorldLL*>(person->FindParentPlanetInEnv());
	AuEther* ether = person->Ether(); AuMatrix4x4 avatarTransform;
	if(!world || !ether || !person->GetTransformRelativeToParent(avatarTransform, world))
	{ reply.message = tr("Move onto a planet or ringworld before testing Legacy cities."); return; }
	const AuPoint origin = avatarTransform.Translation();
	if(request.action == List) { ListBuildings(*world, origin, reply); return; }

	DaLegacyCity* city = nullptr;
	AuLegacyCity::State state;
	int developmentIndex = -1;
	if(request.action != Spawn)
	{
		city = dynamic_cast<DaLegacyCity*>(ether->FindNormalAtom(AuId(request.cityId)));
		if(!city || city->Parent() != world) { reply.message = tr("That Legacy city is no longer on your current world. Refresh the list."); return; }
		state = city->Snapshot();
		for(int i = 0; i < state.developments.size(); ++i)
			if(state.developments[i].id == request.developmentId) { developmentIndex = i; break; }
		if(developmentIndex < 0) { reply.message = tr("That development no longer exists. Refresh the list."); return; }
		const auto& dev = state.developments[developmentIndex];
		if(world->LocOfLL(dev.location, dev.altitudeMSL).Distance2(origin) > std::pow(2000 * FeetPerMeter(), 2))
		{ reply.message = tr("Move within 2 km of the selected building."); return; }
		reply.selectedCity = city->Id().GetID(); reply.selectedDevelopment = dev.id;
	}

	if(request.action == Spawn)
	{
		AuPoint up = -world->GravityDir(origin); up.Unit();
		AuPoint forward = avatarTransform.ProjectVector(AuPoint(0, 1, 0));
		forward -= up * forward.Dot(up);
		if(forward.IsZero()) up.ArbitraryAxis(forward);
		forward.Unit();
		const AuPoint position = origin + forward * (request.distanceMeters * FeetPerMeter());
		AuLegacyCity::Development dev; dev.id = 1; dev.designId = request.designId;
		if(!world->LLOfLoc(dev.location, position)) { reply.message = tr("The requested site is outside this world's terrain. Move closer to the surface."); return; }
		dev.industryId = design->header.industryId; dev.materialId = request.material;
		dev.minimumDeck = request.minimumDeck; dev.maximumDeck = request.maximumDeck;
		dev.completedLevels = quint8(int(dev.maximumDeck) - int(dev.minimumDeck) + 1);
		dev.constructed = request.constructed;
		dev.altitudeMSL = world->SurfaceAltitudeMSL(dev.location) + request.elevationMeters * FeetPerMeter();
		dev.facingRadians = request.headingDegrees * DEG2RAD;
		if(!PlacementClear(*world, dev, mesh, nullptr))
		{ reply.message = tr("The test building would overlap an actor, vehicle, ship or city. Increase the distance or choose an open site."); return; }
		if(!g_AuDb.HasAtomIds()) { reply.message = tr("The server is restocking atom IDs. Try again shortly."); return; }
		std::unique_ptr<DaLegacyCity> created(new DaLegacyCity);
		created->Create();
		if(created->Id().IsNull()) { reply.message = tr("The server could not allocate a city ID."); return; }
		AuMatrix4x4 placement;
		world->BuildingConstructionTransform(world->LocOfLL(dev.location, dev.altitudeMSL), dev.facingRadians,
			nullptr, AuPoint(), placement);
		created->SetPosAndOrient(AuAtomPos(world->Id(), placement));
		state.name = tr("Legacy test B%1").arg(request.designId);
		state.controllingEmpire = state.allegianceEmpire = person->EmpireId();
		state.developments.append(dev);
		QString error;
		if(!created->SetState(state, error)) { reply.message = error; return; }
		created->ConnectSignals(); city = created.release(); ether->AddAtom(city);
		city->SetNormal(); city->SetModified(); g_Au.AtomAdded(city->Id());
		reply.selectedCity = city->Id().GetID(); reply.selectedDevelopment = dev.id;
		reply.message = tr("Created B%1 as city %2. %3 triangles.")
			.arg(request.designId).arg(city->Id().StringId()).arg(mesh.TriangleCount());
	}
	else if(request.action == Remove)
	{
		state.developments.removeAt(developmentIndex);
		if(state.developments.isEmpty())
		{
			city->Kill();
			if(!city->IsDead()) { reply.message = tr("The city could not be removed while it contains a player."); return; }
			world->TrashSubatom(city->Id());
		}
		else { QString error; if(!city->SetState(state, error)) { reply.message = error; return; } }
		reply.selectedCity = reply.selectedDevelopment = 0;
		reply.message = tr("Removed the selected Legacy test building.");
	}
	else if(request.action == Route)
	{
		const auto& dev = state.developments[developmentIndex];
		AuMatrix4x4 placement;
		world->BuildingConstructionTransform(world->LocOfLL(dev.location, dev.altitudeMSL), dev.facingRadians,
			nullptr, AuPoint(), placement);
		QVector<AuLegacyBd::Paths::RoutePoint> points;
		if(!city->Route(dev.id, origin, placement.Project(AuPoint(request.targetX, request.targetY, request.targetZ)), points))
			reply.message = tr("No accessible route from your avatar to that local point. Check floor limits and walls.");
		else reply.message = tr("Route found: %1 waypoints in the current route leg. Target: (%2, %3, %4) feet in the building.")
			.arg(points.size()).arg(request.targetX).arg(request.targetY).arg(request.targetZ);
	}
	else
	{
		auto& dev = state.developments[developmentIndex];
		if(request.action == Update)
		{
			if(request.designId != dev.designId) { reply.message = tr("Select the building's existing blueprint before applying settings. Spawn a new building to test another blueprint."); return; }
			dev.materialId = request.material; dev.minimumDeck = request.minimumDeck; dev.maximumDeck = request.maximumDeck;
			dev.completedLevels = quint8(int(dev.maximumDeck) - int(dev.minimumDeck) + 1); dev.constructed = request.constructed;
			dev.facingRadians = request.headingDegrees * DEG2RAD;
			dev.altitudeMSL = world->SurfaceAltitudeMSL(dev.location) + request.elevationMeters * FeetPerMeter();
			if(!PlacementClear(*world, dev, mesh, city)) { reply.message = tr("These test settings would overlap another actor or structure."); return; }
			reply.message = tr("Updated the test building's material, constructed decks and placement settings.");
		}
		QString error; if(!city->SetState(state, error)) { reply.message = error; return; }
	}
	if(request.action != Route)
	{
		world->InvalidateEnvironmentRecursive();
		g_AuDb.PostCriticalUpdates();
	}
	ListBuildings(*world, origin, reply);
}
