//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: aubio.cpp,v 1.13 2022/02/19 18:08:24 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "aubio.h"
//
#include "aubioaccount.h"
#include "aubioavatar.h"
#include "aubiobiology.h"
#include "aubiobounties.h"
#include "aubiodomain.h"
#include "aubiojobs.h"
#include "aubioskills.h"
#include "auapp.h"
#include "auglobal.h"
#include "auframe.h"
#include <QGridLayout>
#include <QIcon>
#include <QStackedLayout>
#include <QTabBar>

void AuBioDock::UpdateTitleShortcut()
{
	AuFrame* frame = auApp->Frame();
	QString control = frame->ControlBinding(AuFrame::CI_ToggleCharacterUI);
	if(control.isEmpty())
		SetTitle(OriginalTitle());
	else
		SetTitle(OriginalTitle() + " (" + control + ")");
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

AuBioPages::AuBioPages(QWidget* parent, bool staged) : QWidget(parent),
	m_AccountPage(0), m_AvatarPage(0), m_BiologyPage(0), m_BountiesPage(0),
	m_DomainPage(0), m_JobsPage(0), m_SkillsPage(0), m_InitializePage(0)
{
	m_Pages = new QStackedLayout;
	m_Tabs = new QTabBar(this);
	connect(m_Tabs, SIGNAL(currentChanged(int)), m_Pages, SLOT(setCurrentIndex(int)));

	QGridLayout* lay = new QGridLayout();
	lay->addWidget(m_Tabs, 0, 0);
	lay->addLayout(m_Pages, 1, 0);
	lay->setMargin(2);
	lay->setSpacing(2);
	setLayout(lay);

	// Scene loading drives one page at a time; other callers retain eager setup.
	if(!staged)
		while(!InitializeNextPage()) {}
}

bool AuBioPages::InitializeNextPage()
{
	if(IsInitialized()) return true;
	const int pageIndex = m_Pages->count();
	int itab;
	switch(m_InitializePage)
	{
	case 0:
	{
		m_AvatarPage = new AuBioAvatar(this);
		m_Pages->insertWidget(pageIndex, m_AvatarPage);

		QIcon avatarIcon(":data/c_VacSuitHelmet.png");
		itab = m_Tabs->insertTab(pageIndex, avatarIcon, tr("Avatar"));
		m_Tabs->setTabToolTip(itab, tr("<html><b>Avatar</b><br>"
				"Shows information about your avatar."
				"</html>"));
		break;
	}
	case 1:
	{
		m_SkillsPage = new AuBioSkills(this);
		m_Pages->insertWidget(pageIndex, m_SkillsPage);

		QIcon skillsIcon(":data/Brain.png");
		itab = m_Tabs->insertTab(pageIndex, skillsIcon, tr("Skills"));
		m_Tabs->setTabToolTip(itab, tr("<html><b>Skills</b><br>"
				"Shows skills learned by your avatar."
				"</html>"));
		break;
	}
	case 2:
	{
		m_BiologyPage = new AuBioBiology(this);
		m_Pages->insertWidget(pageIndex, m_BiologyPage);

		QIcon biologyIcon(":data/DNA.png");
		itab = m_Tabs->insertTab(pageIndex, biologyIcon, tr("Biology"));
		m_Tabs->setTabToolTip(itab, tr("<html><b>Biology</b><br>"
				"Shows information about your avatar's physical body."
				"</html>"));
		break;
	}
	case 3:
	{
		if(g_Au.IsLegacyGeneration()) break;
		m_DomainPage = new AuBioDomain(this);
		m_Pages->insertWidget(pageIndex, m_DomainPage);

		QIcon sysIcon(":data/AvatarDomain.png");
		itab = m_Tabs->insertTab(pageIndex, sysIcon, tr("Land"));
		m_Tabs->setTabToolTip(itab, tr("<html><b>Land</b><br>"
				"Shows the land of your avatar."
				"</html>"));
		break;
	}
	case 4:
	{
		m_JobsPage = new AuBioJobs(this);
		m_Pages->insertWidget(pageIndex, m_JobsPage);

		QIcon jobsIcon(":data/OrderRead.png");
		itab = m_Tabs->insertTab(pageIndex, jobsIcon, tr("Stories"));
		m_Tabs->setTabToolTip(itab, tr("<html><b>Stories</b><br>"
				"Shows information about your active stories and jobs."
				"</html>"));
		break;
	}
	case 5:
	{
		m_BountiesPage = new AuBioBounties(this);
		m_Pages->insertWidget(pageIndex, m_BountiesPage);

		QIcon bountyIcon(":data/JobBountyHunter.png");
		itab = m_Tabs->insertTab(pageIndex, bountyIcon, tr("Bounties"));
		m_Tabs->setTabToolTip(itab, tr("<html><b>Bounties</b><br>"
				"Shows rewards for killing your avatar."
				"</html>"));
		break;
	}
	case 6:
	{
		if(g_Au.IsLegacyGeneration()) break;
		m_AccountPage = new AuBioAccount(this);
		m_Pages->insertWidget(pageIndex, m_AccountPage);

		QIcon accountIcon(":data/Login.png");
		itab = m_Tabs->insertTab(pageIndex, accountIcon, tr("Account"));
		m_Tabs->setTabToolTip(itab, tr("<html><b>Account</b><br>"
				"Manages your login account. "
				"</html>"));
		break;
	}
	}
	++m_InitializePage;
	return IsInitialized();
}

AuBioPages::~AuBioPages()
{
}
