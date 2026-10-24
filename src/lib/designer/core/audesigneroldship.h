#ifndef AuDesignerOldShip_H
#define AuDesignerOldShip_H

#include "audialog.h"
#include "audockwidget.h"
#include "oldbuilding.h"
#include "oldbuildingedit.h"
#include "oldship.h"
#include "oldshipconvert.h"
#include <QHash>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QVector>
#include <QWidget>

class AuDesign;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTextBrowser;
class QToolButton;
class QUrl;

class AuOldShipPanel : public QWidget
{
	Q_OBJECT;
public:
	enum KIND
	{
		KIND_Spacecraft,
		KIND_Building
	};

	enum TOOL
	{
		TOOL_None,
		TOOL_Room,
		TOOL_EraseCells,
		TOOL_EraseRoom,
		TOOL_RoomNumber,
		TOOL_Detail,
		TOOL_EraseDetail,
		TOOL_EraseDetails,
		TOOL_Roof
	};

private:
	struct ToolDef
	{
		TOOL							tool;
		int								arg;
		QString							icon;
		int								commodity;
	};

	KIND								m_Kind;
	OldShip::Design						m_Design;
	OldBuilding::Design					m_Building;
	OldShip::BuildingRules				m_Rules;
	int									m_Material;
	int									m_RoofClicks;
	OldShip::GridPoint					m_RoofPoint[2];
	QString								m_FileName;
	bool								m_Modified;
	int									m_Revision;
	int									m_ModelRevision;
	OldShip::Vec3						m_Origin;
	QStringList							m_ConvertLog;
	bool								m_Converted;
	QString								m_ReportHtml;
	int									m_ReportRevision;
	QString								m_ReportShown;
	QList<OldShip::Problem>				m_Problems;

	QList<QToolButton*>				m_SpotButtons;
	QVector<ToolDef>					m_Tools;
	TOOL								m_Tool;
	int									m_ToolArg;
	int									m_ToolCommodity;
	QString								m_ToolIcon;
	int									m_Turn;
	int									m_Deck;
	bool								m_Solid;
	int									m_RoomNumber;
	int									m_Brush;
	bool								m_RetypeMode;

	QButtonGroup*						m_ToolGroup;
	QToolButton*						m_RenderMode;
	QToolButton*						m_Gravity;
	QToolButton*						m_GridButton[4];
	QToolButton*						m_RoomMode;
	QComboBox*							m_MaterialBox;
	QComboBox*							m_IndustryBox;
	QPushButton*						m_RoomNumberButton;
	QLabel*								m_BrushLabel;
	QLabel*								m_ArmorLabel;
	QLabel*								m_DeckLabel;
	QCheckBox*							m_Streamline;
	QSpinBox*							m_MinQuality;
	QToolButton*						m_SaveButton;
	QLabel*								m_Status;
	QTextBrowser*						m_Report;

public:
	AuOldShipPanel(KIND kind, QWidget* parent=0);
	virtual ~AuOldShipPanel();

	bool IsBuilding() const
	{
		return m_Kind == KIND_Building;
	}

	OldBuilding::Design Building() const;

	int Industry() const
	{
		return m_Building.industry;
	}

	int Material() const
	{
		return m_Material;
	}

	const OldShip::BuildingRules* Rules() const
	{
		return IsBuilding() ? &m_Rules : 0;
	}

	int ToolRoofType() const
	{
		return m_Tool == TOOL_Roof ? m_ToolArg : 0;
	}

	int RoofClicks() const
	{
		return m_RoofClicks;
	}

	const OldShip::GridPoint& RoofPoint(int which) const
	{
		return m_RoofPoint[which & 1];
	}

	bool ViewCancel();

	static QString BuildingFileFilter();
	static bool IsBuildingFileName(const QString& path);

	static bool LoadBuildingFile(const QString& path, OldBuilding::Design& building, QString& why);

	void SetBuilding(const OldBuilding::Design& building, const QString& fileName);

	bool ConfirmLoseChanges();
	bool ConfirmPathsForFinalize();
	QString RunPathCheck(OldShip::PathReportCounts& counts);

	bool IsModified() const
	{
		return m_Modified;
	}

	bool Save(bool askName);

	void UpdateTitle();

	const OldShip::Design& Design() const
	{
		return m_Design;
	}

	const QString& FileName() const
	{
		return m_FileName;
	}

	int Brush() const
	{
		return m_Brush;
	}

	int Deck() const
	{
		return m_Deck;
	}

	int GridLines() const;

	bool IsSolid() const
	{
		return m_Solid;
	}

	bool HasModel() const
	{
		return m_ModelRevision >= 0;
	}

	bool IsModelStale() const
	{
		return m_ModelRevision != m_Revision;
	}

	const QList<OldShip::Problem>& Problems() const
	{
		return m_Problems;
	}

	bool IsReportCurrent() const
	{
		return m_ReportRevision == m_Revision;
	}

	const OldShip::Vec3& Origin() const
	{
		return m_Origin;
	}

	int Revision() const
	{
		return m_Revision;
	}

	TOOL Tool() const
	{
		return m_Tool;
	}

	int ToolDetailType() const;

	int ToolCommodity() const
	{
		return m_Tool == TOOL_Detail ? m_ToolCommodity : 0;
	}

	const QString& ToolIcon() const
	{
		return m_ToolIcon;
	}

	void ViewHover(double gx, double gy);
	void ViewStroke(double gx, double gy, bool pressed);
	bool ViewTurn(int step);

	static bool IsLogLineNoteworthy(const QString& line);

	static bool LoadShipFile(const QString& path, OldShip::Design& ship, QString& why);

	void NewDesign();

	void SetConvertLog(const QStringList& log, bool converted, const OldShip::Vec3& origin);

	void SetModelGone();

	void SetReport(const QString& html, int revision, const QList<OldShip::Problem>& problems=QList<OldShip::Problem>());

	void SetGravityShown(bool on);

	void ShowLines();

	void SetDesign(const OldShip::Design& design, const QString& fileName);

protected:
	QToolButton* AddTool(TOOL tool, int arg, const QString& icon, const QString& toolTip, int commodity=0);
	bool Apply(double gx, double gy, QString& why);
	QToolButton* Button(const QString& icon, const QString& toolTip, bool available=true);
	QWidget* CreateEquipmentGroup();
	QWidget* CreateRoomsGroup();
	QString DeckWord(bool capital) const;
	QString DetailName(int detailType) const;
	int DetailVariant(int baseType, int turn) const;
	QString FileFilter() const;
	QString RoomName(int roomType) const;
	void RoofClick(double gx, double gy);
	void SelectIndustry(int industry);
	void UpdateSpotTools();
	void SetIndustry(int industry);
	void DesignChanged();
	void SelectTool(TOOL tool, int arg, const QString& icon, int commodity=0);
	void UpdateControls();
	void UpdateReport();

protected slots:
	void ArmorDecrease();
	void ArmorIncrease();
	void CheckPaths();
	void BrushDown();
	void BrushUp();
	void DeckAbove();
	void DeckBelow();
	void EraseDeck();
	void GravityClicked(bool on);
	void GridToggled();
	void IndustryChanged(int index);
	void MaterialChanged(int index);
	void MinQualityChanged(int quality);
	void MoveCopy();
	void RenderModeClicked(bool solid);
	void ReportLinkClicked(const QUrl& link);
	void RoomModeToggled(bool on);
	void RoomNumberDown();
	void RoomNumberPressed();
	void RoomNumberUp();
	void SaveShip();
	void StreamlineClicked(bool on);
	void SystemModules();
	void ToolPicked(int id);

signals:
	void DesignEdited();
	void FinalizeRequested();
	void GravityToggled(bool on);
	void LoadRequested();
	void LookRequested(double x, double y, int deck);
	void NewRequested();
	void RenderModeToggled(bool solid);
};

class AuOldShipDock : public AuDockWidget
{
	Q_OBJECT;
	AuOldShipPanel*						m_Panel;

public:
	AuOldShipDock(QWidget* parent, AuOldShipPanel::KIND kind=AuOldShipPanel::KIND_Spacecraft);

	AuOldShipPanel* Panel() const
	{
		return m_Panel;
	}

	virtual void SetAllowedAreas(Qt::DockWidgetAreas areas);
};

class AuOldShipMoveDlg : public AuDialog
{
	Q_OBJECT;
	QRadioButton*						m_MoveDesign;
	QRadioButton*						m_MoveDeck;
	QRadioButton*						m_CopyDeck;
	QSpinBox*							m_X;
	QSpinBox*							m_Y;
	QSpinBox*							m_Decks;
	QSpinBox*							m_Repeat;

public:
	enum WHAT
	{
		MoveDesign,
		MoveDeck,
		CopyDeck
	};

	AuOldShipMoveDlg(int deck, QWidget* parent=0, bool building=false);

	WHAT What() const;
	int X() const;
	int Y() const;
	int Decks() const;
	int Repeat() const;
};

class AuOldShipModulesDlg : public AuDialog
{
	Q_OBJECT;
	QComboBox*							m_Ftl;
	QComboBox*							m_Sensor;
	QComboBox*							m_Shield;
	QMap<int, QComboBox*>				m_Bays;
	QMap<int, QComboBox*>				m_Transporters;
	QMap<int, QComboBox*>				m_Surgery;

public:
	AuOldShipModulesDlg(const OldShip::Design& design, QWidget* parent=0);

	bool Apply(OldShip::Design& design) const;
};

class AuOldShipReportDlg : public AuDialog
{
	Q_OBJECT;
public:
	AuOldShipReportDlg(const QString& title, const QString& html, QWidget* parent=0);
};

class AuOldShipLogDlg : public AuDialog
{
	Q_OBJECT;
public:
	AuOldShipLogDlg(const QString& headline, const QStringList& log, QWidget* parent=0, bool building=false);

	static QString LogHtml(const QStringList& log);
};

#endif
