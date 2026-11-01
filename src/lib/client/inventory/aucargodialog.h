//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: aucargodialog.h,v 1.8 2022/02/19 17:52:59 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef AuCargoDialog_H
#define AuCargoDialog_H

#include "auclientlib.h"
#include <audockwidget.h>
#include <auglobal.h>

class AuCargoTransfer;
class AuOrdnanceTransfer;
class QStackedLayout;
class QTabBar;

class AUCLIENT_DLL AuCargoDialog : public QWidget
{
	Q_OBJECT;
private:
	// Widgets
	AuCargoTransfer*		m_CargoPage;
	AuOrdnanceTransfer*		m_OrdPage;
	QStackedLayout*			m_Pages;
	QTabBar*				m_Tabs;
	int m_InitializePage;

public:
	AuCargoDialog(bool freeAvatar, QWidget* parent=0, bool staged=false);
	virtual ~AuCargoDialog();
	bool InitializeNextPage();
	bool IsInitialized() const { return m_InitializePage >= 4; }

	void OrdnanceReceived(const QByteArray& ba);

	void TransferCitizensAvailable(const QByteArray& ba);
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AUCLIENT_DLL AuCargoDock : public AuDockWidget
{
public:
	AuCargoDialog*						m_Cargo;

public:
	AuCargoDock(QWidget* parent, bool staged=false) : AuDockWidget(":data/c_Crate.png", tr("<b>Cargo</b>"), tr("Cargo"), parent)
	{
		m_Cargo = new AuCargoDialog(false, this, staged);
		setWidget(m_Cargo);

		int dockSides = g_Au.Settings().readNumEntry("Dock/CargoAreas", (int)allowedAreas());
		setAllowedAreas((Qt::DockWidgetAreas)dockSides);
	}

	virtual void SetAllowedAreas(Qt::DockWidgetAreas areas)
	{
		setAllowedAreas(areas);
		g_Au.Settings().writeEntry("Dock/CargoAreas", (int)allowedAreas());
	}

	inline void TransferCitizensAvailable(const QByteArray& ba)
	{
		m_Cargo->TransferCitizensAvailable(ba);
	}

	virtual void UpdateTitleShortcut();
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AUCLIENT_DLL AuCargoDockFree : public AuDockWidget
{
public:
	AuCargoDialog*						m_Cargo;

public:
	AuCargoDockFree(QWidget* parent, bool staged=false) : AuDockWidget(":data/c_Crate.png", tr("<b>Cargo</b>"), tr("Cargo"), parent)
	{
		m_Cargo = new AuCargoDialog(true, this, staged);
		setWidget(m_Cargo);

		int dockSides = g_Au.Settings().readNumEntry("Dock/CargoAreas", (int)allowedAreas());
		setAllowedAreas((Qt::DockWidgetAreas)dockSides);
	}

	virtual void SetAllowedAreas(Qt::DockWidgetAreas areas)
	{
		setAllowedAreas(areas);
		g_Au.Settings().writeEntry("Dock/CargoAreas", (int)allowedAreas());
	}

	inline void TransferCitizensAvailable(const QByteArray& ba)
	{
		m_Cargo->TransferCitizensAvailable(ba);
	}

	virtual void UpdateTitleShortcut();
};

#endif // AuCargoDialog_H
