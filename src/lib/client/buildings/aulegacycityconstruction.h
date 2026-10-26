#ifndef AuLegacyCityConstruction_H
#define AuLegacyCityConstruction_H

#include "auclientlib.h"
#include "aulegacycityprotocol.h"
#include "audockwidget.h"
#include <QElapsedTimer>
#include <QWidget>

class AuBldConstruct;
class AuLegacyCityBank;
class AuLegacyCityManufacture;
class AuLegacyCityDesignStudio;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QToolButton;
class QPushButton;
class QPixmap;
class QStackedWidget;
class QTimer;
class QTabBar;

// Original F11 city planning owns historical GD selections and commands.
class AUCLIENT_DLL AuLegacyCityConstruction : public QWidget
{
	Q_OBJECT
public:
	explicit AuLegacyCityConstruction(QWidget* parent = nullptr);
	void Submit(AuLegacyCityProtocol::Request request);
	QPixmap PlacementIcon(const AuLegacyCityProtocol::Request& request) const;
	void SetCursorMaterial(quint16 material);
	void SetCursorRoadSlab(bool slab);
	void SetCursorLevels(quint8 levels) { if(m_Planning) m_ActiveRequest.levels = levels; }
	void SetCursorProductChoices(const QVector<quint16>& allowed, quint16 selected);
	void PlanningStopped();
	void ReportPlacementMessage(const QString& message);
	QString PlacementMessage(float& alpha) const;

signals:
	void PlanConstruction(AuLegacyCityProtocol::Request request);
	void StopPlanning();

protected:
	void showEvent(QShowEvent* event) override;
	void hideEvent(QHideEvent* event) override;

private:
	QButtonGroup *m_Categories, *m_Materials;
	QStackedWidget* m_Pages;
	QComboBox *m_Designs, *m_Grading, *m_RoadWidth, *m_Angle, *m_Products;
	QCheckBox* m_MakeProduct;
	QToolButton *m_Clear, *m_Stop;
	QLabel *m_Name, *m_Levels, *m_Terrain, *m_Jobs, *m_Homes;
	QLabel *m_Recipe, *m_ProductIcons, *m_ProductHeading;
	QTimer *m_Timer, *m_Timeout;
	quint8 m_Industry = 0, m_Process = 0;
	quint16 m_Material = 76;
	quint32 m_PendingToken = 0;
	bool m_Loaded = false, m_Planning = false;
	int m_LevelLimit = 1;
	AuLegacyCityProtocol::Request m_ActiveRequest;
	QString m_PlacementMessage;
	QElapsedTimer m_PlacementMessageTime;
	void LoadCatalogue();
	void SelectCategory(int page);
	void SelectProcess(quint8 processId);
	void DescribeRecipe();
	void DescribeProducts();
	void StartPlanning();
	void UpdatePlanningSettings();
	void UpdateControls();
	void ReportMessage(const QString& message);
	void Refresh();
	void ReceiveReply(QByteArray payload);
	AuLegacyCityProtocol::Request BuildRequest() const;
	void ResetTransport();
};

// A legacy target/transport adapter around the existing F10 labor widget.
// Common labor controls, material/modifier presentation and timer stay in
// AuBldConstruct. The target follows the occupied site, as in AuLabor (2016).
class AUCLIENT_DLL AuLegacyCityLabor : public QWidget
{
	Q_OBJECT
public:
	explicit AuLegacyCityLabor(QWidget* parent = nullptr);

protected:
	void showEvent(QShowEvent* event) override;
	void hideEvent(QHideEvent* event) override;

private:
	AuBldConstruct* m_Labor;
	AuLegacyCityManufacture* m_Manufacture;
	AuLegacyCityBank* m_Bank;
	AuLegacyCityDesignStudio* m_Studio;
	QStackedWidget* m_LaborPages;
	QTabBar* m_LaborTabs;
	bool m_DualPages = false;
	QLabel* m_Info;
	QTimer *m_Timer, *m_Timeout;
	QTimer* m_StudioRequestTimer;
	QVector<AuLegacyCityProtocol::Entry> m_Entries;
	int m_Selected = -1;
	quint32 m_PendingToken = 0;
	bool m_CitizenEnabled = false;
	AuLegacyCityProtocol::Request m_PendingRequest, m_DeferredCitizenRequest;
	bool m_HasDeferredCitizenRequest = false;
	QString m_LastMessage;
	AuLegacyCityProtocol::Request m_QueuedStudioRequest;
	bool m_StudioRequestQueued = false, m_StudioStateValid = false;
	quint8 m_StudioLevelCount = 0;
	quint32 m_StudioCityId = 0, m_StudioDevelopmentId = 0, m_AbandonedStudioToken = 0;
	const AuLegacyCityProtocol::Entry* Selected() const;
	void Refresh();
	void Submit(AuLegacyCityProtocol::Request request);
	void Operate(AuLegacyCityProtocol::Action action);
	void ReceiveReply(QByteArray payload);
	void UpdateControls();
	void ResetTransport();
	void ReportMessage(const QString& message);
	void QueueStudioRequest(AuLegacyCityProtocol::Request request);
};

class AUCLIENT_DLL AuLegacyCityLaborDock : public AuDockWidget
{
public:
	explicit AuLegacyCityLaborDock(QWidget* parent);
	void UpdateTitleShortcut() override;
};

class AUCLIENT_DLL AuLegacyCityConstructionDock : public AuDockWidget
{
public:
	explicit AuLegacyCityConstructionDock(QWidget* parent);
	AuLegacyCityConstruction* Content() const { return m_Content; }
	void UpdateTitleShortcut() override;
private:
	AuLegacyCityConstruction* m_Content;
};

#endif
