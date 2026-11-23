#include "aubuildgridcontrols.h"
#include "auclientglobal.h"
#include "auframe.h"
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>

AuBuildGridControls::AuBuildGridControls(const QString& gridToolTip, QWidget* parent) : QWidget(parent)
{
	m_Grid = new QCheckBox(tr("Grid"), this);
	m_Grid->setToolTip(gridToolTip);
	m_Size = new QComboBox(this);
	m_Size->setFixedWidth(50);
	m_Size->setEditable(true);
	m_Size->addItems(QStringList() << "1" << "2" << "4" << "5" << "8" << "10" << "16" << "20" << "32" << "50");
	m_Size->setToolTip(tr("Distance between grid lines in meters."));
	auto* layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_Grid);
	layout->addWidget(m_Size);
	layout->addWidget(new QLabel(tr("m"), this));
	layout->addStretch(1);

	connect(m_Grid, &QCheckBox::toggled, this, [this](bool enabled) {
		g_AuClient.SetBuildGrid(enabled);
		m_Size->setEnabled(enabled);
	});
	connect(m_Size, &QComboBox::editTextChanged, this, [](const QString& text) {
		bool ok;
		const double metres = text.toDouble(&ok);
		if(ok && metres > 0) g_AuClient.SetBuildGridSize(metres);
	});
	connect(m_Grid, &QCheckBox::clicked, this, &AuFrame::FocusCurrentView, Qt::QueuedConnection);
	connect(m_Size, QOverload<int>::of(&QComboBox::activated), this, &AuFrame::FocusCurrentView, Qt::QueuedConnection);
}

void AuBuildGridControls::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
	// The other generation's construction panel may have changed the shared settings.
	const QSignalBlocker blockGrid(m_Grid), blockSize(m_Size);
	m_Grid->setChecked(g_AuClient.BuildGrid());
	m_Size->setEnabled(m_Grid->isChecked());
	m_Size->setEditText(QString::number(g_AuClient.BuildGridSizeInMeters()));
}
