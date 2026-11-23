#ifndef AuBuildGridControls_H
#define AuBuildGridControls_H

#include "auclientlib.h"
#include <QWidget>

class QCheckBox;
class QComboBox;

class AUCLIENT_DLL AuBuildGridControls : public QWidget
{
public:
	AuBuildGridControls(const QString& gridToolTip, QWidget* parent);

protected:
	void showEvent(QShowEvent* event) override;

private:
	QCheckBox* m_Grid;
	QComboBox* m_Size;
};

#endif
