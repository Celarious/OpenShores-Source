//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: aublddesignstudio.h,v 1.7 2022/02/19 17:51:42 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef AuBldDesignStudio_H
#define AuBldDesignStudio_H

#include "auclientlib.h"
#include <QWidget>

class DaAtom;
class QScrollArea;
class QTimer;

class AUCLIENT_DLL AuBldDesignStudioPanel : public QWidget
{
	Q_OBJECT;
	bool		m_CitizenOk;	// is citizen worker enabled?
	int			m_MPID;			// manufacturing process id
	int			m_PanelIndex;
	bool		m_Legacy;
	bool		m_Occupied = false;

public:
	AuBldDesignStudioPanel(int panelIndex=0, QWidget* parent=0, bool legacy=false);
	virtual ~AuBldDesignStudioPanel();

	void UpdateContents(const DaAtom* pde);
	void SetOccupied(bool occupied);

protected slots:
	void EnterDesignerPushed();
	void EnterLegacyDesignerPushed();

signals:
	void Enter(int panelIndex, QString password);
	void LegacyEnter(int panelIndex, QString password);
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AUCLIENT_DLL AuBldDesignStudio : public QWidget
{
	Q_OBJECT;
private:
	QTimer*		m_timer;

	// Widgets
	AuBldDesignStudioPanel*				m_Panels[16];			// Up to 16 design studio panels at a building.
	QScrollArea*						m_PanelScroller;

public:
	AuBldDesignStudio(QWidget* parent=0);
	virtual ~AuBldDesignStudio();

protected:
	virtual void hideEvent(QHideEvent* event);
	virtual void showEvent(QShowEvent* event);

protected slots:
	void UpdateContents();
};

class AUCLIENT_DLL AuLegacyCityDesignStudio : public QWidget
{
	Q_OBJECT;
	quint32 m_CityId;
	quint32 m_DevelopmentId;
	int m_StudioCount;
	AuBldDesignStudioPanel* m_Panels[16];

public:
	AuLegacyCityDesignStudio(QWidget* parent=0);
	void SetTarget(quint32 cityId, quint32 developmentId, int completedLevels);
	void SetOccupied(int studioIndex, bool occupied);

protected slots:
	void EnterDesignerPushed(int studioIndex, QString password);

signals:
	void Enter(quint32 cityId, quint32 developmentId, int studioIndex, QString password);
};

#endif // AuBldDesignStudio_H
