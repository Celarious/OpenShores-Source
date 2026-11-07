//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: auspacecraftpages.cpp,v 1.6 2022/02/19 18:16:55 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "auspacecraftpages.h"
//
#include "auapp.h"
#include "audamagereport.h"
#include "auframe.h"
#include "auglobal.h"
#include "AuMissionTool.h"
#include "auscexchange.h"
#include "auspacecraftaccount.h"
#include "auspacecraftberths.h"
#include "auspacecraftberthsdialog.h"
#include "auspacecraftbounties.h"
#include "auspacecraftpolicy.h"
#include "auspacecraftshipments.h"
#include "auspacecraftspecs.h"
#include <QIcon>
#include <QKeyEvent>
#include <QStackedLayout>

void AuSpacecraftDock::UpdateTitleShortcut()
{
	AuFrame* frame = auApp->Frame();
	QString control = frame->ControlBinding(AuFrame::CI_ToggleSpacecraftUI);
	if(control.isEmpty())
		SetTitle(OriginalTitle());
	else
		SetTitle(OriginalTitle() + " (" + control + ")");
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

void AuSpacecraftDockFree::UpdateTitleShortcut()
{
	AuFrame* frame = auApp->Frame();
	QString control = frame->ControlBinding(AuFrame::CI_ToggleSpacecraftUI);
	if(control.isEmpty())
		SetTitle(OriginalTitle());
	else
		SetTitle(OriginalTitle() + " (" + control + ")");
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

AuSpacecraftTabs::AuSpacecraftTabs(QWidget *parent) : AuTabColumn(parent)
{
}

AuSpacecraftTabs::~AuSpacecraftTabs()
{
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

AuSpacecraftPages::AuSpacecraftPages(bool freeAvatar, QWidget* parent, bool staged) : QWidget(parent),
	m_FreeAvatar(freeAvatar), m_InitializeStage(0), m_ShipAccount(0), m_ShipBerths(0),
	m_ShipBounties(0), m_ShipDamage(0), m_ShipDesignExchange(0), m_ShipMission(0),
	m_ShipPolicy(0), m_ShipShipments(0), m_ShipSpecs(0)
{
	m_Pages = new QStackedLayout();

	m_Tabs = new AuSpacecraftTabs(this);
	connect(m_Tabs, SIGNAL(CurrentChanged(int)), SLOT(CurrentTabChanged(int)));

	QGridLayout* lay = new QGridLayout;
	lay->addWidget(m_Tabs, 0,0);
	lay->addLayout(m_Pages, 0,1);
	lay->setMargin(2);
	lay->setSpacing(2);
	setLayout(lay);

	if(!staged)
		while(!InitializeNextPage()) {}
}

// A caller may finish all pages during loading while yielding between calls.
bool AuSpacecraftPages::InitializeNextPage()
{
	if(IsInitialized()) return true;
	switch(m_InitializeStage++)
	{
	case 0:
	if(!m_FreeAvatar)
	{
		int tabIndex = m_Tabs->AddTab(QIcon(":data/OrderTakeCommand.png"));
		m_Tabs->SetTabToolTip(tabIndex, tr("Mission"));
		m_ShipMission = new AuMissionTool(this);
		m_Pages->addWidget(m_ShipMission);
	}
	break;

	case 1:
	if(!m_FreeAvatar)
	{
		int tabIndex = m_Tabs->AddTab(QIcon(":data/Captain.png"));
		m_Tabs->SetTabToolTip(tabIndex, tr("Policy"));
		m_ShipPolicy = new AuSpacecraftPolicy(this);
		m_Pages->addWidget(m_ShipPolicy);
	}
	break;

	case 2:
	{
		int tabIndex = m_Tabs->AddTab(QIcon(":data/CommCrew.png"));
		m_Tabs->SetTabToolTip(tabIndex, tr("Roster"));
		m_ShipBerths = new AuSpacecraftBerths(this);
		m_Pages->addWidget(m_ShipBerths);
	}
	break;

	case 3:
	if(!m_FreeAvatar)
	{
		int tabIndex = m_Tabs->AddTab(QIcon(":data/c_Money.png"));
		m_Tabs->SetTabToolTip(tabIndex, tr("Account"));
		m_ShipAccount = new AuSpacecraftAccount(this);
		m_Pages->addWidget(m_ShipAccount);
	}
	break;

	case 4:
	{
		int tabIndex = m_Tabs->AddTab(QIcon(":data/JobBountyHunter.png"));
		m_Tabs->SetTabToolTip(tabIndex, tr("Bounties"));
		m_ShipBounties = new AuSpacecraftBounties(this);
		m_Pages->addWidget(m_ShipBounties);
	}
	break;

	case 5:
	{
		int tabIndex = m_Tabs->AddTab(QIcon(":data/MsgDamageReport.png"));
		m_Tabs->SetTabToolTip(tabIndex, tr("Damage"));
		m_ShipDamage = new AuDamageReport(this);
		m_Pages->addWidget(m_ShipDamage);
	}
	break;

	case 6:
	if(!g_Au.IsLegacyGeneration())
	{
		int tabIndex = m_Tabs->AddTab(QIcon(":data/c_Container.png"));
		m_Tabs->SetTabToolTip(tabIndex, tr("Shipments"));
		m_ShipShipments = new AuSpacecraftShipments(this);
		m_Pages->addWidget(m_ShipShipments);
		for(const AuShipment& shipment : m_PendingShipments)
			m_ShipShipments->Shipment(shipment);
		m_PendingShipments.clear();
	}
	break;

	case 7:
	{
		int tabIndex = m_Tabs->AddTab(QIcon(":data/c_ScDesignDrawing.png"));
		m_Tabs->SetTabToolTip(tabIndex, tr("Specifications"));
		m_ShipSpecs = new AuSpacecraftSpecs(this);
		m_Pages->addWidget(m_ShipSpecs);
	}
	break;

	case 8:
	if(!g_Au.IsLegacyGeneration())
	{
		int tabIndex = m_Tabs->AddTab(QIcon(":data/Exchange.png"));
		m_Tabs->SetTabToolTip(tabIndex, tr("Blueprint Exchange"));
		m_ShipDesignExchange = new AuScExchange(this);
		m_Pages->addWidget(m_ShipDesignExchange);
	}
	break;
	}
	return IsInitialized();
}

AuSpacecraftPages::~AuSpacecraftPages()
{
}

void AuSpacecraftPages::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
}

void AuSpacecraftPages::hideEvent(QHideEvent* event)
{
	QWidget::hideEvent(event);
}

void AuSpacecraftPages::CurrentTabChanged(int idx)
{
	m_Pages->setCurrentIndex(idx);
}

QSize AuSpacecraftPages::sizeHint() const
{
	return QSize(640,300);
}

void AuSpacecraftPages::BrokerShipment(AuShipment shipment)
{
	if(g_Au.IsLegacyGeneration()) return;
	if(m_ShipShipments)
		m_ShipShipments->Shipment(shipment);
	else
		m_PendingShipments.append(shipment);
}

void AuSpacecraftPages::BrokerShipmentsClear()
{
	m_PendingShipments.clear();
	if(m_ShipShipments)
		m_ShipShipments->ShipmentsClear();
}

