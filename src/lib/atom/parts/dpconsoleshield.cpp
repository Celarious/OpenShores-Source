//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: dpconsoleshield.cpp,v 1.71 2022/04/14 16:43:31 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "dpconsoleshield.h"
//
#include "dacreature.h"
#include "daperson.h"
#include "dalegacycity.h"
#include "dasc.h"
#include "ausound.h"
#include "dicontrolinput.h"

DpConsoleShield::DpConsoleShield()
{
}

DpConsoleShield::~DpConsoleShield()
{
}

DpConsoleShield::DpConsoleShield(const DpConsoleShield& c) : DpStationShield(c), DpConsole(c)
{
}

DpStation* DpConsoleShield::Clone() const
{
	return new DpConsoleShield(*this);
}

void DpConsoleShield::ProcessControlInput(const DiControlInputArray& controls, DaPerson* person, DaVessel& ship,
		CONTROL_CONTEXT context, const AuPoint& location)
{
	const DaSc* legacy = dynamic_cast<const DaSc*>(&ship);
	if(!legacy || !legacy->IsLegacyDebug())
	{
		DpStationShield::ProcessControlInput(controls, person, ship, context, location);
		return;
	}
	if(ship.IsNew()) return;
	DiControlInputArray active = controls;
	if(active.HasId(CI_PowerOnOff))
	{
		// AuAtom13:180620990 toggles the original shield switch between
		// on and off; the later three-state Down mode did not exist.
		if(context != CONTROL_Client)
			ship.SetShieldState(ship.ShieldState() == DaVessel::SS_Up ? DaVessel::SS_Off : DaVessel::SS_Up);
		if(context != CONTROL_Server)
		{
			AuEffect click(person->ButtonSoundLoc(), AuPoint(0, 0), AuPoint(0, 0), SOUND_Bleep7, 1,
				ship.ShieldState() == DaVessel::SS_Up ? .9f : 1.f);
			person->AddEffect(click);
		}
		active.RemoveId(CI_PowerOnOff);
	}
	DpStationShield::ProcessControlInput(active, person, ship, context, location);
}

void DpConsoleShield::ProcessControlInput(const DiControlInputArray& controls, DaPerson* person, DaLegacyCity& city,
		CONTROL_CONTEXT context, const AuPoint& location)
{
	Q_UNUSED(location);
	// The original building console has one switch, not six directional shields.
	if(!controls.HasId(CI_PowerOnOff)) return;
	if(context != CONTROL_Client) city.ToggleStationShield(*person);
	if(context != CONTROL_Server)
	{
		AuEffect click(person->ButtonSoundLoc(), AuPoint(), AuPoint(), SOUND_Bleep7, 1, 1);
		person->AddEffect(click);
	}
}

void DpConsoleShield::CreateDeviceObjects(AuAtomRenderInfo& ari, const DaAtom& parent, bool inBuilding)
{
	DpStationShield::CreateDeviceObjects(ari, parent, inBuilding);
	DpConsole::CreateDeviceObjects(ari, parent, inBuilding);
}

// Called between rendering scene frames that contains this object.
void DpConsoleShield::AnimateDeviceObjects(AuGLEffects& re, const DaVessel& ship, const AuElapsedTimer& last,
		const AuPoint& location, const DaCreature& sceneAtom)
{
	DpStationShield::AnimateDeviceObjects(re, ship, last, location, sceneAtom);	// Primary display screen.
	DpConsole::AnimateDeviceObjects(re, ship, last, sceneAtom);					// Secondary display screen.
}

void DpConsoleShield::Render(const AuMatrix4x4& projection, const AuMatrix4x4& farProjection, const AuMatrix4x4& tModel,
		const DaPerson& sceneAtom, const DaCreature* seatedCreature, AuAtomRenderInfo& ari)
{
	AuMatrix4x4 tPanel(tModel);
	if(seatedCreature)
		tPanel.Translate(0,0,-seatedCreature->GroundHeightAboveCG()/seatedCreature->GrowthScale());
	DpStationShield::Render(projection, farProjection, tPanel, sceneAtom, seatedCreature, ari);
	DpConsole::RenderConsole(projection, tPanel, ari, m_ScreenTex);
}

void DpConsoleShield::RenderMini(const AuMatrix4x4& projection, const AuMatrix4x4& tModel, const DaCreature* seatedCreature,
		AuAtomRenderInfo& ari)
{
	AuMatrix4x4 tPanel(tModel);
	if(seatedCreature)
		tPanel.Translate(0,0,-seatedCreature->GroundHeightAboveCG()/seatedCreature->GrowthScale());
	DpConsole::RenderConsole(projection, tPanel, ari, m_ScreenTex);
}

void DpConsoleShield::PanelDimensions(double& width, double& height) const
{
	DpConsole::PanelDimensions(0, width, height);
}

void DpConsoleShield::EnumControlPanels(AuQuadArray &aControlPanels, const AuMatrix4x4 &tModel, QVector<qint16>& idPanels,
		const DaCreature& seatedCreature) const
{
	AuMatrix4x4 tPanel(tModel);
	tPanel.Translate(0,0,-seatedCreature.GroundHeightAboveCG()/seatedCreature.GrowthScale());
	DpConsole::EnumControlPanels(aControlPanels, tPanel, idPanels);
}
