//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: audefaultspacecraft.h,v 1.7 2022/02/19 17:55:27 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#ifndef AuDefaultSpacecraft_H
#define AuDefaultSpacecraft_H

#include "auclientlib.h"
#include "audialog.h"
#include <QPair>
#include <QVector>

class QComboBox;
class QLabel;

class AUCLIENT_DLL AuDefaultSpacecraft : public AuDialog
{
	Q_OBJECT;
public:
	enum Purpose { ModernProduction, LegacyProduction, LegacyPurchase };
private:
	Purpose m_Purpose;
	quint32								m_DesignId;			// Design selected to buy, if accepted.
	QString								m_DesignName;		// Design selected to buy, if accepted.
	quint8								m_DesignQ;
	QVector<QPair<quint32, QString>> m_AdditionalDesigns;

	// Widgets
	QComboBox*							m_MakeWhat;

public:
	AuDefaultSpacecraft(QWidget* parent=0, Purpose purpose=ModernProduction);
	virtual ~AuDefaultSpacecraft();
	void SetAdditionalDesigns(const QVector<QPair<quint32, QString>>& designs) { m_AdditionalDesigns = designs; }

	inline quint32 DesignId() const
	{
		return m_DesignId;
	}

	inline const QString& DesignName() const
	{
		return m_DesignName;
	}

	inline quint8 DesignQ() const
	{
		return m_DesignQ;
	}

public slots:
	virtual void setVisible(bool visible);

protected:
	void ListDesigns();
	void RestoreGeometry();
	void SaveGeometry();

protected slots:
	void MakeOne();
};

#endif // AuDefaultSpacecraft_H
