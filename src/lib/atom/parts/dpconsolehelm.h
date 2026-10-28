//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: dpconsolehelm.h,v 1.74 2022/04/14 16:41:45 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef DpConsoleHelm_H
#define DpConsoleHelm_H

#include "auatom.h"
#include "dpconsole.h"
#include "dpstationhelm.h"

class AUATOM_DLL DpConsoleHelm : public DpStationHelm, public DpConsole
{
private:
	bool m_LegacyControls = false;
	void SetLegacyControls(bool legacy);

public:
	// Reuse the original wire IDs reserved by the modern helm enum.
	enum LEGACY_CONTROLINPUT
	{
		CI_LockCrewDoors = CI_UNUSED1,
		CI_LockHullDoors = CI_UNUSED2,
	};

	DpConsoleHelm();
	virtual ~DpConsoleHelm();
	DpConsoleHelm(const DpConsoleHelm& c);

public:
	virtual void AnimateDeviceObjects(AuGLEffects& re, const DaVessel& ship, const AuElapsedTimer& last, const AuPoint& location,
			const DaCreature& sceneAtom);

	virtual DpStation* Clone() const;

	virtual void CreateDeviceObjects(AuAtomRenderInfo& ari, const DaAtom& parent, bool inBuilding);

	virtual void EnumControlPanels(AuQuadArray &aControlPanels, const AuMatrix4x4 &tModel, QVector<qint16>& idPanels,
			const DaCreature& seatedCreature) const;

	virtual void EnumControlInputs(DiControlInputArray& controls) const;

	virtual void ProcessControlInput(const DiControlInputArray& controls, DaPerson* person, DaVessel& ship,
		CONTROL_CONTEXT context, const AuPoint& location);

	virtual void Refresh(const DpStation* station, const DaAtom& parent);

	virtual void Rx(QDataStream& in, DaAtom& capsule);

	virtual void SampleControlPanelInput(qint16 panelId, const AuPoint& panelLoc,
		DiControlInputArray& controls, bool continuous);

	virtual bool Tick(const AuElapsedTimer& last, const AuElapsedTimer& now, DaPerson* person);

	// 'sceneAtom' is the atom for whom the scene is being constructed.
	// 'seatedCreature' is the creature seated at this console, if any; may be 0.
	virtual void Render(const AuMatrix4x4& projection, const AuMatrix4x4& farProjection, const AuMatrix4x4& tModel,
			const DaPerson& sceneAtom, const DaCreature* seatedCreature, AuAtomRenderInfo& ari);

	virtual void RenderMini(const AuMatrix4x4& projection, const AuMatrix4x4& tModel, const DaCreature* seatedCreature,
			AuAtomRenderInfo& ari);

	virtual AuStation::TYPE Type() const
	{
		return AuStation::Console_Helm;
	}

protected:
	virtual QString DisplayTextureName(const AuId& parentId) const;
	virtual void PanelDimensions(double& width, double& height) const;
};

#endif // DpConsoleHelm_H
