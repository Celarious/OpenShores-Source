//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: augovern.cpp,v 1.5 2022/02/19 18:15:04 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "augovern.h"
//
#include "auapp.h"
#include <auglobal.h>
#include "augovannouncements.h"
#include "augovcities.h"
#include "augovcommerce.h"
#include "augovdiplomacy.h"
#include "augoverntabs.h"
#include "augovpatents.h"
#include "augovpolicy.h"
#include "auempirecitizens.h"
#include "augovtaxes.h"
#include "auframe.h"
#include <QIcon>
#include <QKeyEvent>
#include <QStackedLayout>
#include <QTimer>

void AuGovernDock::UpdateTitleShortcut()
{
	AuFrame* frame = auApp->Frame();
	QString control = frame->ControlBinding(AuFrame::CI_ToggleGovernUI);
	if(control.isEmpty())
		SetTitle(OriginalTitle());
	else
		SetTitle(OriginalTitle() + " (" + control + ")");
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

AuGovern::AuGovern(QWidget* parent, bool staged) : QWidget(parent),
	m_Announcements(0), m_Citizens(0), m_Commerce(0), m_Diplomacy(0), m_Places(0),
	m_Patents(0), m_Policy(0), m_Taxes(0), m_PatentsTabIndex(-1), m_InitializePage(0)
{
	m_Pages = new QStackedLayout();

	AuGovernTabs* tabs = m_Tabs = new AuGovernTabs(this);
	connect(tabs, SIGNAL(CurrentChanged(int)), SLOT(CurrentTabChanged(int)));

	QGridLayout* lay = new QGridLayout;
	lay->addWidget(tabs, 0,0);
	lay->addLayout(m_Pages, 0,1);
	lay->setMargin(2);
	lay->setSpacing(2);
	setLayout(lay);

	if(!staged)
		while(!InitializeNextPage()) {}
}

bool AuGovern::InitializeNextPage()
{
	if(IsInitialized()) return true;
	AuGovernTabs* tabs = m_Tabs;
	switch(m_InitializePage)
	{
	case 0:
	{
		int tabIndex = tabs->AddTab(QIcon(":data/Speaker.png"));
		tabs->SetTabToolTip(tabIndex, tr("Announcements"));
		m_Announcements = new AuGovAnnouncements(this);
		m_Pages->addWidget(m_Announcements);
		break;
	}
	case 1:
	{
		int tabIndex = tabs->AddTab(QIcon(":data/CommGovernment.png"));
		tabs->SetTabToolTip(tabIndex, tr("Policy"));
		m_Policy = new AuGovPolicy(this);
		m_Pages->addWidget(m_Policy);
		break;
	}
	case 2:
	{
		int tabIndex = tabs->AddTab(QIcon(":data/CommDiplomacy.png"));
		tabs->SetTabToolTip(tabIndex, tr("Diplomacy"));
		m_Diplomacy = new AuGovDiplomacy(this);
		m_Pages->addWidget(m_Diplomacy);
		break;
	}
	case 3:
	{
		int tabIndex = tabs->AddTab(QIcon(":data/GovCommerce.png"));
		tabs->SetTabToolTip(tabIndex, tr("Commerce"));
		m_Commerce = new AuGovCommerce(this);
		m_Pages->addWidget(m_Commerce);
		break;
	}
	case 4:
	{
		int tabIndex = tabs->AddTab(QIcon(":data/c_Patent.png"));
		m_PatentsTabIndex = tabIndex;
		tabs->SetTabToolTip(tabIndex, tr("Patents"));
		m_Patents = new AuGovPatents(this);
		m_Pages->addWidget(m_Patents);
		break;
	}
	case 5:
	{
		int tabIndex = tabs->AddTab(QIcon(":data/CommCrew.png"));
		tabs->SetTabToolTip(tabIndex, tr("Citizens"));
		m_Citizens = new AuEmpireCitizens(this);
		m_Pages->addWidget(m_Citizens);
		break;
	}
	case 6:
	{
		int tabIndex = tabs->AddTab(QIcon(":data/City.png"));
		tabs->SetTabToolTip(tabIndex, tr("Cities and Bases"));
		m_Places = new AuGovCities(this);
		m_Pages->addWidget(m_Places);
		break;
	}
	case 7:
	{
		int tabIndex = tabs->AddTab(QIcon(":data/GovTaxes.png"));
		tabs->SetTabToolTip(tabIndex, tr("Taxes"));
		m_Taxes = new AuGovTaxes(this);
		m_Pages->addWidget(m_Taxes);
		break;
	}
	}
	++m_InitializePage;
	if(IsInitialized()) UpdatePatentPage();
	return IsInitialized();
}

AuGovern::~AuGovern()
{
}

void AuGovern::UpdateDatabase()
{
	if(!IsInitialized()) return;
	UpdatePatentPage();
	AuFrame* frame = auApp->Frame();
	if(frame && frame->GetAndResetEmpireDataReceived())
	{
		//qDebug() << "AuGovern::UpdateDatabase";
		m_Announcements->UpdateEmpireRecord();

		m_Commerce->UpdateEmpireRecord();

		m_Places->UpdateEmpireRecord();

		m_Diplomacy->EmpireDataComplete();

		m_Patents->UpdateEmpireRecord();

		m_Policy->UpdateEmpireRecord();

		m_Citizens->UpdateEmpireRecord();

		m_Taxes->UpdateEmpireRecord();
	}
}

void AuGovern::UpdatePatentPage()
{
	if(!IsInitialized()) return;
	const int pageIndex = m_Pages->indexOf(m_Patents);
	if(g_Au.IsLegacyGeneration() && pageIndex >= 0)
	{
		m_Pages->removeWidget(m_Patents);
		m_Patents->hide();
		m_Tabs->RemoveTab(pageIndex);
	}
	else if(!g_Au.IsLegacyGeneration() && pageIndex < 0)
	{
		m_Pages->insertWidget(m_PatentsTabIndex, m_Patents);
		m_Tabs->InsertTab(m_PatentsTabIndex, QIcon(":data/c_Patent.png"));
		m_Tabs->SetTabToolTip(m_PatentsTabIndex, tr("Patents"));
	}
}

void AuGovern::showEvent(QShowEvent* event)
{
	UpdatePatentPage();
	QWidget::showEvent(event);
}

void AuGovern::CurrentTabChanged(int idx)
{
	if(!IsInitialized()) return;
	m_Pages->setCurrentIndex(idx);
}

QSize AuGovern::sizeHint() const
{
	return QSize(640,300);
}
