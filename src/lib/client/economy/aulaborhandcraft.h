//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: aulaborhandcraft.h,v 1.8 2022/05/04 22:04:30 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef AuLaborHandCraft_H
#define AuLaborHandCraft_H

#include "auclientlib.h"
#include "audialog.h"
#include "audockwidget.h"
#include "auelapsedtimer.h"

class DaPerson;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTimer;

class AUCLIENT_DLL AuHandCraftPanel : public QWidget
{
	Q_OBJECT;
private:
	int									m_MPID;					// manufacturing process id
	QString								m_Category;
	QList<int>							m_Plan;
	AuElapsedTimer						m_FlagAfterEmpire;

	// Widgets
	QLineEdit*							m_Search;
	QListWidget*						m_Categories;
	QListWidget*						m_Recipes;
	QListWidget*						m_Needs;
	QGroupBox*							m_NeedsBox;
	QPushButton*						m_Make;
	QLabel*								m_Note;

public:
	AuHandCraftPanel(QWidget* parent=0);
	virtual ~AuHandCraftPanel();

	void UpdateContents(DaPerson* person);

protected slots:
	void CategoryChanged();
	void GoPushed();
	void RecipeChanged();
	void SearchChanged();

signals:
	void Go(int mpid);
	void Refresh();
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AUCLIENT_DLL AuLaborHandCraft : public QWidget
{
	Q_OBJECT;
private:
	QTimer*								m_timer;

	// Widgets
	AuHandCraftPanel*					m_Panel;

public:
	AuLaborHandCraft(QWidget* parent=0);
	virtual ~AuLaborHandCraft();

protected:
	virtual void showEvent(QShowEvent* event);

protected slots:
	void UpdateContents();
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AUCLIENT_DLL AuHandCraftDock : public AuDockWidget
{
public:
	AuLaborHandCraft*					m_HandCraft;

public:
	AuHandCraftDock(QWidget* parent);

	virtual void SetAllowedAreas(Qt::DockWidgetAreas areas);
};

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

class AUCLIENT_DLL AuHandCraftDialog : public AuDialog
{
	Q_OBJECT;
	AuLaborHandCraft*					m_HandCraft;

public:
	AuHandCraftDialog(QWidget* parent=0);
	virtual ~AuHandCraftDialog();

	virtual QSize sizeHint() const;

public slots:
	virtual void setVisible(bool visible);

protected:
	void RestoreGeometry();
	void SaveGeometry();
};

#endif // AuLaborHandCraft_H
