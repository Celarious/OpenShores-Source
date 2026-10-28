//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: dpconsolehelm.cpp,v 1.264 2022/04/14 16:43:11 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "dpconsolehelm.h"
//
#include "dacreature.h"
#include "dasc.h"
#include "daperson.h"
#include "auatomrenderinfo.h"
#include "dicontrolinput.h"
#include "dipanelcontrol.h"

namespace
{
bool LegacyShip(const DaAtom& atom)
{
	const DaSc* ship = dynamic_cast<const DaSc*>(&atom);
	return ship && ship->IsLegacyDebug();
}

void RemoveModernFTLControls(DiControlInputArray& controls)
{
	// AuAtom13:18060fc50/18060e130 define the original pilot controls without
	// FTL buttons or keyboard actions; FTL belongs to the engineer console.
	controls.RemoveId(DpStationHelm::CI_FTLSelect);
	controls.RemoveId(DpStationHelm::CI_FTLEngage);
	controls.RemoveId(DpStationHelm::CI_FTLSettingUp);
	controls.RemoveId(DpStationHelm::CI_FTLSettingDown);
}
}

DpConsoleHelm::DpConsoleHelm()
{
}

DpConsoleHelm::~DpConsoleHelm()
{
}

DpConsoleHelm::DpConsoleHelm(const DpConsoleHelm& c) : DpStationHelm(c), DpConsole(c), m_LegacyControls(c.m_LegacyControls)
{
}

DpStation* DpConsoleHelm::Clone() const
{
	return new DpConsoleHelm(*this);
}

void DpConsoleHelm::CreateDeviceObjects(AuAtomRenderInfo& ari, const DaAtom& parent, bool inBuilding)
{
	SetLegacyControls(LegacyShip(parent));
	if(m_LegacyControls && !m_ScreenTex)
		m_ScreenTex = ari.re->CreateTextureFromBitmap(parent.Id().GetID(),
			DisplayTextureName(parent.Id()), ":data/legacy_pc_Console.bmp");
	DpStationHelm::CreateDeviceObjects(ari, parent, inBuilding);
	DpConsole::CreateDeviceObjects(ari, parent, inBuilding);
}

void DpConsoleHelm::SetLegacyControls(bool legacy)
{
	// A design report can arrive after the console's first texture was created.
	if(m_LegacyControls != legacy) m_ScreenTex = 0;
	m_LegacyControls = legacy;
}

QString DpConsoleHelm::DisplayTextureName(const AuId& parentId) const
{
	return m_LegacyControls ? QString("LegacyPCon%1").arg(parentId.GetID())
		: DpStationHelm::DisplayTextureName(parentId);
}

// Called between rendering scene frames that contains this object.
void DpConsoleHelm::AnimateDeviceObjects(AuGLEffects& re, const DaVessel& ship, const AuElapsedTimer& last,
		const AuPoint& location, const DaCreature& sceneAtom)
{
	SetLegacyControls(LegacyShip(ship));
	if(m_LegacyControls && !m_ScreenTex)
		m_ScreenTex = re.CreateTextureFromBitmap(ship.Id().GetID(), DisplayTextureName(ship.Id()),
			":data/legacy_pc_Console.bmp");
	DpStationHelm::AnimateDeviceObjects(re, ship, last, location, sceneAtom);	// Primary display screen.
	DpConsole::AnimateDeviceObjects(re, ship, last, sceneAtom);					// Secondary display screen.
	if(LegacyShip(ship))
	{
		// The recovered button occupies the modern beacon/strobe rectangles.
		// Overlay it after native animation so those controls cannot replace it.
		AuGLTexture* texture = re.FindTexture(DisplayTextureName(ship.Id()));
		if(texture)
		{
			static const QImage headlightOn(QStringLiteral(":data/legacy_pc_LightOn.bmp"));
			static const QImage headlightOff(QStringLiteral(":data/legacy_pc_LightOff.bmp"));
			QImage headlight = ship.IsHeadlightOn() ? headlightOn : headlightOff;
			texture->ReplaceSubImage(re, headlight, 146, 90);
			static const QImage on(QStringLiteral(":data/pc_IntLightOn.bmp"));
			static const QImage dim(QStringLiteral(":data/pc_IntLightRed.bmp"));
			static const QImage off(QStringLiteral(":data/pc_IntLightOff.bmp"));
			const INTERIOR_LIGHT light = ship.InteriorLight(QString());
			QImage button = light == INTLIGHT_Bright ? on : light == INTLIGHT_Dim ? dim : off;
			texture->ReplaceSubImage(re, button, 164, 90);
			// Also populate a newly selected legacy texture when the native
			// instrument cache still holds readings from the previous texture.
			static const QImage powerOn(QStringLiteral(":data/pc_PowerPlantOn.bmp"));
			static const QImage powerOff(QStringLiteral(":data/pc_PowerPlantOff.bmp"));
			QImage power = ship.IsPowerPlantOn() ? powerOn : powerOff;
			texture->ReplaceSubImage(re, power, 218, 90);
			static const QImage driveOn(QStringLiteral(":data/pc_ManeuverOn.bmp"));
			static const QImage driveHold(QStringLiteral(":data/pc_ManeuverPark.bmp"));
			static const QImage driveOff(QStringLiteral(":data/pc_ManeuverOff.bmp"));
			const DaVessel::GRAVITY_DRIVE_STATE driveState = ship.GravityDriveState();
			QImage drive = driveState == DaVessel::GD_On ? driveOn : driveState == DaVessel::GD_Hold ? driveHold : driveOff;
			texture->ReplaceSubImage(re, drive, 236, 90);
			static const QImage moored(QStringLiteral(":data/pc_Moored.bmp"));
			static const QImage unmoored(QStringLiteral(":data/pc_Unmoored.bmp"));
			QImage mooring = ship.IsMoored() ? moored : unmoored;
			texture->ReplaceSubImage(re, mooring, 236, 108);
			// DpConsolePilot:18060b0d0 supplies separate crew/hull lock icons
			// at these bottom-origin positions. Hull icons also show open doors.
			static const QImage original(QStringLiteral(":data/legacy_pc_Console.bmp"));
			const DaSc& spacecraft = static_cast<const DaSc&>(ship);
			static const QImage crewLocked(QStringLiteral(":data/pc_CrewDoorsLocked.png"));
			static const QImage crewUnlocked(QStringLiteral(":data/pc_CrewDoorsUnlocked.png"));
			QImage crewButton = spacecraft.LegacyDoorsLocked(false) ? crewLocked : crewUnlocked;
			texture->ReplaceSubImage(re, crewButton, 182, 90);
			static const QImage hullOpen(QStringLiteral(":data/pc_HullDoorsUnlockedO.png"));
			static const QImage hullClosed(QStringLiteral(":data/pc_HullDoorsUnlocked.png"));
			static const QImage hullLockedOpen(QStringLiteral(":data/pc_HullDoorsLockedO.png"));
			static const QImage hullLockedClosed(QStringLiteral(":data/pc_HullDoorsLocked.png"));
			const bool hullLocked = spacecraft.LegacyDoorsLocked(true);
			QImage hullButton = ship.AnyHullDoorOpen() ? (hullLocked ? hullLockedOpen : hullOpen)
				: (hullLocked ? hullLockedClosed : hullClosed);
			texture->ReplaceSubImage(re, hullButton, 200, 90);
			// FTL operation belongs to the original engineer console. Keep the
			// pilot artwork where the modern panel adds FTL and landing gear.
			QImage ftlSlot = original.copy(204, original.height() - 124, 32, 16);
			texture->ReplaceSubImage(re, ftlSlot, 204, 108);
			QImage gearSlot = original.copy(236, original.height() - 88, 16, 16);
			texture->ReplaceSubImage(re, gearSlot, 236, 72);
		}
	}
}

void DpConsoleHelm::EnumControlInputs(DiControlInputArray& controls) const
{
	DpStationHelm::EnumControlInputs(controls);
	if(m_LegacyControls)
	{
		RemoveModernFTLControls(controls);
		controls.RemoveId(CI_BeaconsOnOff);
		controls.RemoveId(CI_StrobesOnOff);
		controls.RemoveId(CI_NavLightsOnOff);
		controls.RemoveId(CI_GearUpDown);
		// Original helm control 13 remains reserved in the modern input enum.
		controls.append(DiControlInput(QObject::tr("Interior Lights On/Dim/Off"), CI_UNUSED0,
			Qt::Key_unknown, Qt::NoModifier, DiControlInput::MT_None, Qt::NoModifier,
			DiControlInput::JT_None, 0, 100, false));
		controls.append(DiControlInput(QObject::tr("Lock/Unlock Crew Doors"), CI_LockCrewDoors,
			Qt::Key_unknown, Qt::NoModifier, DiControlInput::MT_None, Qt::NoModifier,
			DiControlInput::JT_None, 0, 100, false));
		controls.append(DiControlInput(QObject::tr("Lock/Unlock Hull Doors"), CI_LockHullDoors,
			Qt::Key_unknown, Qt::NoModifier, DiControlInput::MT_None, Qt::NoModifier,
			DiControlInput::JT_None, 0, 100, false));
	}
}

void DpConsoleHelm::ProcessControlInput(const DiControlInputArray& controls, DaPerson* person, DaVessel& ship,
	CONTROL_CONTEXT context, const AuPoint& location)
{
	if(!LegacyShip(ship))
	{
		DpStationHelm::ProcessControlInput(controls, person, ship, context, location);
		return;
	}
	if(!ship.IsNormal()) return;
	DiControlInputArray active = controls;
	RemoveModernFTLControls(active);
	active.RemoveId(CI_BeaconsOnOff);
	active.RemoveId(CI_StrobesOnOff);
	active.RemoveId(CI_NavLightsOnOff);
	active.RemoveId(CI_GearUpDown);
	if(context != CONTROL_Client)
	{
		DaSc& spacecraft = static_cast<DaSc&>(ship);
		if(active.HasId(CI_LockCrewDoors)) spacecraft.ToggleLegacyDoorLocks(false);
		if(active.HasId(CI_LockHullDoors)) spacecraft.ToggleLegacyDoorLocks(true);
	}
	active.RemoveId(CI_LockCrewDoors);
	active.RemoveId(CI_LockHullDoors);
	if(active.HasId(CI_UNUSED0) && !ship.IsNew())
	{
		if(context != CONTROL_Client)
		{
			const INTERIOR_LIGHT light = ship.InteriorLight(QString());
			ship.InteriorLightToggle(light == INTLIGHT_Bright ? INTLIGHT_Dim
				: light == INTLIGHT_Dim ? INTLIGHT_Dark : INTLIGHT_Bright);
			ship.SetModified();
		}
	}
	active.RemoveId(CI_UNUSED0);
	DpStationHelm::ProcessControlInput(active, person, ship, context, location);
}

void DpConsoleHelm::Refresh(const DpStation* station, const DaAtom& parent)
{
	SetLegacyControls(LegacyShip(parent));
	DpStationHelm::Refresh(station, parent);
}

void DpConsoleHelm::Rx(QDataStream& in, DaAtom& capsule)
{
	SetLegacyControls(LegacyShip(capsule));
	DpStationHelm::Rx(in, capsule);
}

void DpConsoleHelm::SampleControlPanelInput(qint16 panelId, const AuPoint& panelLoc,
	DiControlInputArray& controls, bool continuous)
{
	DpStationHelm::SampleControlPanelInput(panelId, panelLoc, controls, continuous);
	if(!m_LegacyControls || panelId) return;
	controls.RemoveId(CI_BeaconsOnOff);
	controls.RemoveId(CI_StrobesOnOff);
	controls.RemoveId(CI_NavLightsOnOff);
	controls.RemoveId(CI_GearUpDown);
	controls.RemoveId(CI_HeadlightOnOff);
	controls.RemoveId(CI_CloseHullDoors);
	RemoveModernFTLControls(controls);
	double width, height;
	PanelDimensions(width, height);
	if(width <= 0 || height <= 0) return;
	const QPoint pixel(int(panelLoc.x * 256 / width), int(panelLoc.y * 128 / height));
	const DiPanelControl legacy[] = {
		DiPanelControl(CI_HeadlightOnOff, QRect(146, 90, 16, 16)),
		DiPanelControl(CI_UNUSED0, QRect(164, 90, 16, 16)),
		// DpConsolePilot::SampleControlPanelInput, AuAtom13:18060fc50.
		DiPanelControl(CI_LockCrewDoors, QRect(182, 90, 16, 16)),
		DiPanelControl(CI_LockHullDoors, QRect(200, 90, 16, 16)),
		DiPanelControl(CI_CloseHullDoors, QRect(200, 83, 16, 7))
	};
	controls.SampleControls(panelId, pixel, continuous, legacy, sizeof(legacy) / sizeof(legacy[0]));
}

bool DpConsoleHelm::Tick(const AuElapsedTimer& last, const AuElapsedTimer& now, DaPerson* person)
{
	SetLegacyControls(person->Parent() && LegacyShip(*person->Parent()));
	return DpStationHelm::Tick(last, now, person);
}

void DpConsoleHelm::Render(const AuMatrix4x4& projection, const AuMatrix4x4& farProjection, const AuMatrix4x4& tModel,
		const DaPerson& sceneAtom, const DaCreature* seatedCreature, AuAtomRenderInfo& ari)
{
	AuMatrix4x4 tPanel(tModel);
	if(seatedCreature)
		tPanel.Translate(0,0,-seatedCreature->GroundHeightAboveCG()/seatedCreature->GrowthScale());
	DpStationHelm::Render(projection, farProjection, tPanel, sceneAtom, seatedCreature, ari);
	DpConsole::RenderConsole(projection, tPanel, ari, m_ScreenTex);
}

void DpConsoleHelm::RenderMini(const AuMatrix4x4& projection, const AuMatrix4x4& tModel, const DaCreature* seatedCreature,
		AuAtomRenderInfo& ari)
{
	AuMatrix4x4 tPanel(tModel);
	if(seatedCreature)
		tPanel.Translate(0,0,-seatedCreature->GroundHeightAboveCG()/seatedCreature->GrowthScale());
	DpConsole::RenderConsole(projection, tPanel, ari, m_ScreenTex);
}

void DpConsoleHelm::PanelDimensions(double& width, double& height) const
{
	DpConsole::PanelDimensions(0, width, height);
}

void DpConsoleHelm::EnumControlPanels(AuQuadArray &aControlPanels, const AuMatrix4x4 &tModel, QVector<qint16>& idPanels,
		const DaCreature& seatedCreature) const
{
	AuMatrix4x4 tPanel(tModel);
	tPanel.Translate(0,0,-seatedCreature.GroundHeightAboveCG()/seatedCreature.GrowthScale());
	DpConsole::EnumControlPanels(aControlPanels, tPanel, idPanels);
}
