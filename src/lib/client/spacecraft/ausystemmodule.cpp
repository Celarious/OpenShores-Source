//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: ausystemmodule.cpp,v 1.3 2022/02/19 18:17:06 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "ausystemmodule.h"
//
#include "auglobal.h"
#include "aulegacycityconstruction.h"
#include "aulegacyshipcommoditydata.h"
#include "aulegacyshipmoduleoptions.h"
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>

AuSystemModule::AuSystemModule(SYSTEM system, bool isBuilding, COMMODITYID currentModule, QWidget* parent,
		bool legacy) :
	AuDialog(parent)
{
	m_Module = currentModule;

	setAuDlgIcon(QIcon(QStringLiteral(":data/Design.png")));
	setAuDlgTitle(QString("%1 Module").arg(AuModuleDialog::Name(system, legacy)));

	QLabel* lb = new QLabel(this);
	lb->setPixmap(QPixmap(AuModuleDialog::Icon(system, legacy)));

	m_Modules = new QComboBox(this);
	m_Modules->setToolTip(AuModuleDialog::Name(system, legacy));
	AuModuleDialog::ListModules(m_Modules, system, isBuilding, m_Module, legacy);
	connect(m_Modules, SIGNAL(activated(int)), SLOT(ModuleChanged(int)));

	QPushButton* pbok = new QPushButton(tr("Place Equipment"));
	pbok->setAutoDefault(true);
	pbok->setDefault(true);
	connect(pbok, SIGNAL(clicked()), SLOT(accept()));
	pbok->setToolTip(tr("Place equipment using the selected system module."));

	QGridLayout* lay = new QGridLayout;
	int row=0;
	lay->addWidget(lb, row,0);
	lay->addWidget(m_Modules, row,1);
	row++;
	lay->addWidget(pbok, row,0, 1,2);
	setAuDlgLayout(lay);
}

AuSystemModule::~AuSystemModule()
{
}

QString AuModuleDialog::Name(SYSTEM system, bool legacy)
{
	if(legacy)
	{
		switch(system)
		{
		case SYS_CommandAndControl: return QStringLiteral("Control Stations");
		case SYS_Transporter: return QStringLiteral("Transporter Room");
		case SYS_Weapon: return QStringLiteral("Weapon Bay");
		default: break;
		}
	}
	return ScSystemName(system);
}

QString AuModuleDialog::Icon(SYSTEM system, bool legacy)
{
	if(!legacy) return SystemIcon(system);
	const QString name = system == SYS_Hull ? QStringLiteral("dp_ArmorIncrease.png")
		: SystemIcon(system).section(QLatin1Char('/'), -1);
	return QStringLiteral(":/data/legacy_") + name;
}

void AuModuleDialog::ListModules(QComboBox* cb, SYSTEM system, bool building, quint16 current, bool legacy)
{
	if(legacy)
	{
		quint16 id = 0;
		for(const auto& commodity : AuLegacyShip::LegacyCommodity::Table)
		{
			if(AuLegacyShip::IsDesignerModule(system, id, building))
			{
				cb->addItem(QIcon(AuLegacyCityConstruction::IconPath(QString::fromLatin1(commodity.bitmap))),
					QString::fromLatin1(commodity.name), int(id));
			}
			++id;
		}
	}
	else
	{
		const DbCommodityArray& commodities = g_Au.m_CommodityArray;
		for(const DbCommodity& commodity : commodities)
			if(building ? DbCommodity::IsBdModule(system, commodity.m_id) : DbCommodity::IsScModule(system, commodity.m_id))
				cb->addItem(QIcon(commodity.BitmapName()), commodity.m_name, commodity.m_id);
	}

	int curIndex = cb->findData(int(current));
	if(curIndex >= 0)
		cb->setCurrentIndex(curIndex);
}

void AuSystemModule::ModuleChanged(int index)
{
	m_Module = m_Modules->itemData(index).toInt();
}
