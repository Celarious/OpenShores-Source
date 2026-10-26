//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: aublddesignstudio.cpp,v 1.8 2022/02/19 18:09:18 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "aublddesignstudio.h"
#include "aubldscrollarea.h"
//
#include "auapp.h"
#include "auframe.h"
#include "auview.h"
#include "dabd.h"
#include "dadesignbase.h"
#include "daperson.h"
#include "daworldll.h"
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>

AuBldDesignStudioPanel::AuBldDesignStudioPanel(int panelIndex, QWidget* parent, bool legacy) : QWidget(parent)
{
	m_PanelIndex = panelIndex;
	m_Legacy = legacy;

	// Make the controls.
	QString text = QString(tr("<b>Studio ")) + QChar('A' + panelIndex) + "</b>";
	QLabel* lb0 = new QLabel(text, this);

	QLabel* lb1 = new QLabel(tr("Vacant"), this);
	lb1->setObjectName("Status");
	lb1->setToolTip(tr("Shows occupied status of the studio."));
	lb1->setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);

	QLineEdit* le0 = new QLineEdit("");
	le0->setObjectName("Password");
	le0->setToolTip(tr("<html>Enter studio password.<br>"
				"If the studio is <b>Vacant</b>, this sets the password that others must enter to join you in the studio. "
				"If the studio is <b>Occupied</b>, this must match the password that was entered by the first person to enter "
				"the studio.</html>"));

	QPushButton* pb2;
	QPushButton* pbBuilding = 0;
	if(m_Legacy)
	{
		// A blank password makes the studio private, including against other
		// blank passwords. A matching nonblank password permits joining.
		le0->setMaxLength(128);
		le0->setToolTip(tr("<html>Enter studio password.<br>If the studio is <b>Vacant</b>, this sets the password "
			"that others must enter to join you. If it is <b>Occupied</b>, enter the owner's password.<br>"
			"A blank password prevents anyone from joining, even with another blank password.</html>"));
		pbBuilding = new QPushButton(QIcon(":data/c_BuildingDesignDrawing.png"), tr("Enter Building Designer"), this);
		pbBuilding->setEnabled(false);
		pbBuilding->setToolTip(tr("The legacy building designer is not available yet."));
		pb2 = new QPushButton(QIcon(":data/c_DesignDisk.png"), tr("Enter Spacecraft Designer"), this);
		pb2->setObjectName("LegacyEnter");
		pb2->setToolTip(tr("Enters the studio for designing legacy spacecraft."));
		connect(pb2, SIGNAL(clicked(bool)), SLOT(EnterLegacyDesignerPushed()));
		connect(le0, &QLineEdit::textChanged, this, [this](const QString&) { SetOccupied(m_Occupied); });
	}
	else
	{
		pb2 = new QPushButton(QIcon(":data/SoHDesignerIcon.png"), tr("Enter Designer"), this);
		pb2->setToolTip(tr("<html>Enters the studio for designing spacecraft and buildings. "
					"</html>"));
		connect(pb2, SIGNAL(clicked(bool)), SLOT(EnterDesignerPushed()));
	}

	// Build the layout.
	QGridLayout* lay = new QGridLayout;
	int row=0;
	lay->addWidget(lb0, row,0);		// Studio title
	lay->addWidget(lb1, row,1);		// Status
	lay->addWidget(le0, row,2);		// Password edit
	if(pbBuilding)
	{
		lay->addWidget(pbBuilding, row,3);
		lay->addWidget(pb2, row+1,3);
	}
	else
		lay->addWidget(pb2, row,3);		// Enter Designer
	//row++;
	//lay->addWidget(pb0, row,3);		// Enter Old-style Building Designer
	//row++;
	//lay->addWidget(pb1, row,3);		// Enter Old-style Spacecraft Designer
	lay->setColumnMinimumWidth(1, 120);
	lay->setColumnMinimumWidth(2, 120);
	lay->setColumnStretch(0,1);
	lay->setColumnStretch(1,1);
	lay->setSpacing(2);
	lay->setMargin(2);
	setLayout(lay);
}

AuBldDesignStudioPanel::~AuBldDesignStudioPanel()
{
}

void AuBldDesignStudioPanel::UpdateContents(const DaAtom* pde)
{
	QLabel* lb1 = findChild<QLabel*>(tr("Status"));
	if(!lb1)
		return;

	if(pde)
	{
		// Presense of design environment implies it is occupied.
		// It disappears when the last person leaves.
		lb1->setText(tr("Occupied"));
	}
	else
	{
		// Absense of design environment implies it is vacant.
		lb1->setText(tr("Vacant"));
	}
}

void AuBldDesignStudioPanel::EnterDesignerPushed()
{
	QLineEdit* le0 = findChild<QLineEdit*>(tr("Password"));
	if(le0)
	{
		QString password = le0->text();
		emit Enter(m_PanelIndex, password);
	}
}

void AuBldDesignStudioPanel::SetOccupied(bool occupied)
{
	m_Occupied = occupied;
	QLabel* status = findChild<QLabel*>("Status");
	if(status)
		status->setText(occupied ? tr("Occupied") : tr("Vacant"));
	if(m_Legacy)
	{
		QPushButton* enter = findChild<QPushButton*>("LegacyEnter");
		const QLineEdit* password = findChild<QLineEdit*>("Password");
		if(enter)
			enter->setEnabled(!occupied || (password && !password->text().isEmpty()));
	}
}

void AuBldDesignStudioPanel::EnterLegacyDesignerPushed()
{
	const QLineEdit* password = findChild<QLineEdit*>("Password");
	if(password && (!m_Occupied || !password->text().isEmpty()))
		emit LegacyEnter(m_PanelIndex, password->text());
}

//-+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

AuBldDesignStudio::AuBldDesignStudio(QWidget* parent) : QWidget(parent)
{
	m_timer = 0;

	// Make the controls.
	// The most any building has is sixteen design studios.
	// Make a panel for each potential studio. UpdateContents will hide the ones it doesn't need.
	for(int pidx=0; pidx<16; pidx++)
	{
		m_Panels[pidx] = new AuBldDesignStudioPanel(pidx, this);
		connect(m_Panels[pidx], SIGNAL(Enter(int, QString)), auApp, SLOT(EnterStudio(int, QString)));
		m_Panels[pidx]->hide();	// Hide this now or it forces the parent AuLabor dialog to be too big.
	}

	// Build the layout.
	QWidget* panels = new QWidget;
	QVBoxLayout* lay = new QVBoxLayout;
	for(int pidx=0; pidx<16; pidx++)
		lay->addWidget(m_Panels[pidx]);
	lay->addStretch(1);
	lay->setSpacing(2);
	lay->setMargin(2);
	panels->setLayout(lay);

	m_PanelScroller = new AuBldScrollArea;
	m_PanelScroller->setWidget(panels);
	m_PanelScroller->setWidgetResizable(true);

	QVBoxLayout* flay = new QVBoxLayout;
	flay->addWidget(m_PanelScroller);
	flay->setSpacing(2);
	flay->setMargin(0);
	setLayout(flay);
}

AuBldDesignStudio::~AuBldDesignStudio()
{
}

void AuBldDesignStudio::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);

	// Create a timer to keep this page updated.
	if(!m_timer)
	{
		m_timer = new QTimer(this);
		connect(m_timer, SIGNAL(timeout()), SLOT(UpdateContents()));
	}
	if(!m_timer->isActive())
		m_timer->start(500);
	m_PanelScroller->show();
}

void AuBldDesignStudio::hideEvent(QHideEvent* event)
{
	QWidget::hideEvent(event);

	// Kill the timer. It will be recreated during the next showEvent.
	if(m_timer && m_timer->isActive())
		m_timer->stop();

	// Hide my panels now or they force the parent AuLabor dialog to be too big even though this page isn't visible.
	for(int pidx=0; pidx<16; pidx++)
		m_Panels[pidx]->hide();
	m_PanelScroller->hide();
}

void AuBldDesignStudio::UpdateContents()
{
	int dscount = 0;	// Count of studios present.

	// Lock the root and find the scene atom.
	AuView* pView = auApp->Frame()->View();
	DaPerson* person;
	pView->LockSceneAtom(&person);
	if(person && person->InIndustry() == IID_DesignStudio)
	{
		DaWorldLL* llworld = dynamic_cast<DaWorldLL*>(person->FindParentPlanetInEnv());
		if(llworld)
		{
			const DaBd* bd = llworld->FindBd(person->BdId());
			if(bd)
			{
				dscount = bd->Auditoriums();
				if(dscount > 16)
					dscount = 16;	// Max is 16.
				for(int pidx=0; pidx<dscount; pidx++)
				{
					const DaAtom* pde = llworld->DesignEnvironment(person->InIndustryLL(), pidx);
					m_Panels[pidx]->show();
					m_Panels[pidx]->UpdateContents(pde);
				}
			}
		}
	}
	pView->UnlockSceneAtom();

	// Hide panels that are not needed.
	for(int pidx=dscount; pidx<16; pidx++)
		m_Panels[pidx]->hide();
}

AuLegacyCityDesignStudio::AuLegacyCityDesignStudio(QWidget* parent) : QWidget(parent),
	m_CityId(0), m_DevelopmentId(0), m_StudioCount(0)
{
	QWidget* panels = new QWidget;
	QVBoxLayout* layout = new QVBoxLayout;
	for(int pidx=0; pidx<16; pidx++)
	{
		m_Panels[pidx] = new AuBldDesignStudioPanel(pidx, panels, true);
		connect(m_Panels[pidx], SIGNAL(LegacyEnter(int, QString)), SLOT(EnterDesignerPushed(int, QString)));
		layout->addWidget(m_Panels[pidx]);
		m_Panels[pidx]->hide();
	}
	layout->addStretch(1);
	layout->setSpacing(2);
	layout->setMargin(2);
	panels->setLayout(layout);

	QScrollArea* scroller = new AuBldScrollArea(this);
	scroller->setWidget(panels);
	scroller->setWidgetResizable(true);
	QVBoxLayout* pageLayout = new QVBoxLayout;
	pageLayout->addWidget(scroller);
	pageLayout->setSpacing(2);
	pageLayout->setMargin(0);
	setLayout(pageLayout);
}

void AuLegacyCityDesignStudio::SetTarget(quint32 cityId, quint32 developmentId, int completedLevels)
{
	const bool changed = cityId != m_CityId || developmentId != m_DevelopmentId;
	m_CityId = cityId;
	m_DevelopmentId = developmentId;
	m_StudioCount = cityId && developmentId ? qBound(0, completedLevels, 16) : 0;
	for(int pidx=0; pidx<16; pidx++)
	{
		if(changed)
		{
			QLineEdit* password = m_Panels[pidx]->findChild<QLineEdit*>("Password");
			if(password)
				password->clear();
			m_Panels[pidx]->SetOccupied(false);
		}
		m_Panels[pidx]->setVisible(pidx < m_StudioCount);
	}
}

void AuLegacyCityDesignStudio::SetOccupied(int studioIndex, bool occupied)
{
	if(studioIndex >= 0 && studioIndex < m_StudioCount)
		m_Panels[studioIndex]->SetOccupied(occupied);
}

void AuLegacyCityDesignStudio::EnterDesignerPushed(int studioIndex, QString password)
{
	if(m_CityId && m_DevelopmentId && studioIndex >= 0 && studioIndex < m_StudioCount)
		emit Enter(m_CityId, m_DevelopmentId, studioIndex, password);
}
