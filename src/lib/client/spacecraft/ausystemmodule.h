//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: ausystemmodule.h,v 1.6 2022/02/19 18:02:29 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef AuSystemModule_H
#define AuSystemModule_H

#include "auclientlib.h"
#include "audialog.h"
#include "dbcommodity.h"

class AuComboBoxValue;
class QComboBox;

namespace AuModuleDialog
{
QString Name(SYSTEM system, bool legacy);
QString Icon(SYSTEM system, bool legacy);
void ListModules(QComboBox* cb, SYSTEM system, bool building, quint16 current, bool legacy);
}

class AUCLIENT_DLL AuSystemModule : public AuDialog
{
	Q_OBJECT;
public:
	// Values
	quint16					m_Module;

private:
	// Widgets
	QComboBox*		m_Modules;

public:
	AuSystemModule(SYSTEM system, bool isBuilding, COMMODITYID currentModule=CID_Invalid, QWidget* parent=0,
		bool legacy=false);
	virtual ~AuSystemModule();

private slots:
	void ModuleChanged(int);

};

#endif // AuSystemModule_H
