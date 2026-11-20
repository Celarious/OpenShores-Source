//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: auchooseindustry.cpp,v 1.2 2022/02/19 18:10:03 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "auchooseindustry.h"
#include "aulegacycityconstruction.h"
#include "aulegacycityconstructionlayout.h"
//
#include <dgdata.h>
#include <QComboBox>
#include <QInputDialog>
#include <QLabel>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

AuChooseIndustry::AuChooseIndustry(const QString& caption, INDUSTRYID currentIndustry, bool withBuildings, QWidget* parent) : AuDialog(parent)
{
	m_IndustryId = IID_None;

	// Make a list of industry names, sorted alphabetically.
	QMap<QString, INDUSTRYID> industries;
	const DgData& gd = g_Au.LockDataForRead();
	for(int iid = IID_TownSquare; iid < IID_Count; iid++)
	{
		if(withBuildings)
		{
			// Skip industries with no buildings.
			if(iid == IID_TownSquare ||
					iid == IID_BaseFlag ||
					iid == IID_Road ||
					iid == IID_Park)
				continue;
		}
		if(gd.m_bIndustryArray[iid].m_id)
			industries.insert(gd.IndustryName((INDUSTRYID)iid), (INDUSTRYID)iid);
	}
	g_Au.UnlockData();
	Populate(caption, currentIndustry, industries, false);
}

AuChooseIndustry::AuChooseIndustry(const QString& caption, INDUSTRYID currentIndustry,
	const QMap<QString, INDUSTRYID>& industries, QWidget* parent) : AuDialog(parent), m_IndustryId(IID_None)
{
	Populate(caption, currentIndustry, industries, true);
}

void AuChooseIndustry::Populate(const QString& caption, INDUSTRYID currentIndustry,
	const QMap<QString, INDUSTRYID>& industries, bool legacy)
{
	setAuDlgTitle(tr("Choose Building"));
	setAuDlgIcon(QIcon(QStringLiteral(":data/bHouse.png")));

	QLabel* lb = new QLabel(caption, this);

	QListWidgetItem* curItem = 0;
	m_Industries = new QListWidget(this);
	for(QMap<QString, INDUSTRYID>::const_iterator ii = industries.constBegin(); ii != industries.constEnd(); ii++)
	{
		QString icon;
		if(legacy)
			icon = AuLegacyCityConstruction::IconPath(QString::fromLatin1(AuLegacyCityConstructionLayout::IndustryIcon(*ii)));
		else icon = QString::fromLatin1(DbIndustry::BitmapName(*ii));
		QListWidgetItem* item = new QListWidgetItem(QIcon(icon), ii.key());
		item->setData(Qt::UserRole, *ii);

		if(*ii == currentIndustry)
			curItem = item;

		m_Industries->addItem(item);
	}
	if(curItem)
		m_Industries->setCurrentItem(curItem);
	connect(m_Industries, SIGNAL(itemClicked(QListWidgetItem*)), SLOT(IndustryClicked(QListWidgetItem*)));
	connect(m_Industries, SIGNAL(itemActivated(QListWidgetItem*)), SLOT(IndustryClicked(QListWidgetItem*)));

	QVBoxLayout* lay = new QVBoxLayout;
	lay->addWidget(lb);
	lay->addWidget(m_Industries);
	lay->setSpacing(2);
	lay->setMargin(2);

	setAuDlgLayout(lay);
}

AuChooseIndustry::~AuChooseIndustry()
{
}

void AuChooseIndustry::IndustryClicked(QListWidgetItem* item)
{
	if(item)
	{
		m_IndustryId = (INDUSTRYID)item->data(Qt::UserRole).toInt();
		accept();
	}
	else
		m_IndustryId = IID_None;
}
