#include "audesigneroldship.h"
//
#include "auapp.h"
#include "audesign.h"
#include "auframe.h"
#include "auglobal.h"
#include "auid.h"
#include "model.h"
#include "oldbuildingconvert.h"
#include "oldbuildingedit.h"
#include "oldbuildingspots.h"
#include "dbcommodity.h"
#include "dbindustry.h"
#include "dgdata.h"
#include "oldshipedit.h"
#include "oldshiptypes.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QTextBrowser>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>

namespace
{


QString RoomTypeName(int roomType, bool building)
{
	const OldShip::RoomTypeInfo* info = OldShip::RoomTypeInfoOf(roomType);
	const char* inBuilding = building ? OldShip::RoomTypeBuildingName(roomType) : 0;
	return inBuilding ? QString::fromUtf8(inBuilding) : info && info->name ? QString::fromUtf8(info->name) : QString("Room type %1").arg(roomType);
}

QString DetailTypeName(int detailType, bool building)
{
	const OldShip::DetailTypeInfo* info = OldShip::DetailTypeInfoOf(detailType);
	const char* inBuilding = building ? OldShip::DetailTypeBuildingName(detailType) : 0;
	return inBuilding ? QString::fromUtf8(inBuilding) : info ? QString::fromUtf8(info->name) : QString("Equipment type %1").arg(detailType);
}

const char* const BuildingIndustryKey = "/Designer/OldBuildingIndustry";
const char* const BuildingMaterialKey = "/Designer/OldBuildingMaterial";
const char* const BuildingFileKey = "/Designer/LastBuildingFile";
const char* const ShipFileKey = "/Designer/LastShipFile";

bool IsDesignIndustry(const OldShip::BuildingIndustryInfo& info)
{
	return info.id >= 3 && info.id <= 139 && info.id != 10 && info.id != 114 && OldShip::BuildingSiteRadius(info.id, 0) > 0.0;
}

const int* DetailVariants(int baseType)
{
	static const int singleBerth[8] = { 65, 66, 67, 68, 69, 70, 71, 72 };
	static const int doubleBerth[8] = { 2, 25, 26, 27, 28, 29, 30, 31 };
	static const int hatch[8] = { 8, 77, 78, 79, 80, 81, 82, 83 };
	static const int crewHatch[8] = { 51, 84, 85, 86, 87, 88, 89, 90 };
	static const int hullHatch[8] = { 52, 91, 92, 93, 94, 95, 96, 97 };
	static const int gameTable[8] = { 74, 75, 74, 75, 74, 75, 74, 75 };
	static const int stairsStraight[8] = { 101, 102, 103, 104, 105, 106, 107, 108 };
	static const int stairsTurnLeft[8] = { 109, 110, 111, 112, 113, 114, 115, 116 };
	static const int stairsTurnRight[8] = { 117, 118, 119, 120, 121, 122, 123, 124 };
	static const int stairsUTurnLeft[8] = { 125, 126, 127, 128, 129, 130, 131, 132 };
	static const int stairsUTurnRight[8] = { 133, 134, 135, 136, 137, 138, 139, 140 };
	static const int vehicleLoc[8] = { 57, 58, 59, 60, 61, 62, 63, 64 };
	static int spots[4][8];
	static bool spotsMade = false;
	if(!spotsMade)
	{
		const int bases[4] = { OldShip::SpotGroundBase, OldShip::SpotSpaceBase, OldShip::SpotRocketBase, OldShip::SpotWaterBase };
		for(int k = 0; k < 4; k++)
			for(int i = 0; i < 8; i++)
				spots[k][i] = bases[k] + i;
		spotsMade = true;
	}
	switch(baseType)
	{
	case OldShip::SpotGroundBase: return spots[0];
	case OldShip::SpotSpaceBase: return spots[1];
	case OldShip::SpotRocketBase: return spots[2];
	case OldShip::SpotWaterBase: return spots[3];
	case 65: return singleBerth;
	case 2: return doubleBerth;
	case 8: return hatch;
	case 51: return crewHatch;
	case 52: return hullHatch;
	case 74: return gameTable;
	case 101: return stairsStraight;
	case 109: return stairsTurnLeft;
	case 117: return stairsTurnRight;
	case 125: return stairsUTurnLeft;
	case 133: return stairsUTurnRight;
	case 57: return vehicleLoc;
	}
	return 0;
}

bool SpotAllowed(int industry, OldShip::SpotKind kind)
{
	if(kind == OldShip::Spot_Water && !OldShip::BuildingIsCoastal(industry))
		return false;
	bool known = false;
	DbIndustry rule;
	const DgData& gd = g_Au.LockDataForRead();
	for(DbIndustryArray::const_iterator ii = gd.m_bIndustryArray.cbegin(); ii != gd.m_bIndustryArray.cend(); ii++)
		if((int)(*ii).IndustryId() == industry && (*ii).Structure() == BD_Building)
		{
			rule = *ii;
			known = true;
		}
	g_Au.UnlockData();
	if(!known)
		return true;
	switch(kind)
	{
	case OldShip::Spot_Ground: return rule.ParkGround() >= 0;
	case OldShip::Spot_Space:
	case OldShip::Spot_Rocket: return rule.ParkSpace() >= 0;
	case OldShip::Spot_Water: return rule.ParkWater() >= 0;
	case OldShip::Spot_Spacecraft: return rule.ParkSpacecraft() >= 0;
	default: return false;
	}
}

QString VehicleName(int commodity)
{
	switch(commodity)
	{
	case OldShip::VEHICLE_SpaceFighter: return QObject::tr("Space Fighter F1");
	case OldShip::VEHICLE_SpaceFighter2: return QObject::tr("Space Fighter F2");
	case OldShip::VEHICLE_SpaceTransport: return QObject::tr("Space Transport");
	case OldShip::VEHICLE_Suv: return QObject::tr("SUV");
	case OldShip::VEHICLE_Apc: return QObject::tr("APC");
	case OldShip::VEHICLE_MotorBike: return QObject::tr("Motor Bike");
	}
	return QObject::tr("vehicle");
}

QString NotHere(const QString& why)
{
	return QString("<br><br><i>%1</i>").arg(why);
}

}

AuOldShipPanel::AuOldShipPanel(KIND kind, QWidget* parent) : QWidget(parent)
{
	m_Kind = kind;
	const bool bd = kind == KIND_Building;
	m_Building.industry = bd ? g_Au.Settings().readNumEntry(BuildingIndustryKey, 20) : 0;
	if(bd && (!OldShip::BuildingIndustryInfoOf(m_Building.industry) || !IsDesignIndustry(*OldShip::BuildingIndustryInfoOf(m_Building.industry))))
		m_Building.industry = 20;
	m_Rules.industry = m_Building.industry;
	m_Material = bd ? g_Au.Settings().readNumEntry(BuildingMaterialKey, OldShip::BM_Stone) : 0;
	if(bd && m_Material != OldShip::BM_Wood && m_Material != OldShip::BM_Lumber && m_Material != OldShip::BM_Metal
			&& m_Material != OldShip::BM_Magmium && m_Material != OldShip::BM_Vulcium)
		m_Material = OldShip::BM_Stone;
	m_RoofClicks = 0;
	m_RoofPoint[0].x = m_RoofPoint[0].y = m_RoofPoint[1].x = m_RoofPoint[1].y = 0;
	m_MaterialBox = 0;
	m_IndustryBox = 0;
	m_Modified = false;
	m_Revision = 0;
	m_ModelRevision = -1;
	m_Converted = false;
	m_ReportRevision = -1;
	m_Tool = TOOL_None;
	m_ToolArg = 0;
	m_ToolCommodity = 0;
	m_Turn = 0;
	m_Deck = 0;
	m_Solid = false;
	m_RoomNumber = 0;
	m_Brush = 0;
	m_RetypeMode = false;

	m_ToolGroup = new QButtonGroup(this);
	m_ToolGroup->setExclusive(true);
	connect(m_ToolGroup, SIGNAL(buttonClicked(int)), SLOT(ToolPicked(int)));

	QGridLayout* tools = new QGridLayout;
	tools->setMargin(0);
	tools->setSpacing(2);

	QToolButton* newButton = Button(":data/dp_New.png", tr("<b>New Design</b><br>Start a new spacecraft design."));
	connect(newButton, SIGNAL(clicked()), SIGNAL(NewRequested()));
	tools->addWidget(newButton, 0, 0);

	QToolButton* loadButton = Button(":data/dp_Load.png", tr("<b>Load Design</b><br>Edit an existing spacecraft design. "
				"Reads a .ship file of the old designer."));
	connect(loadButton, SIGNAL(clicked()), SIGNAL(LoadRequested()));
	tools->addWidget(loadButton, 0, 1);

	m_SaveButton = Button(":data/dp_Save.png", tr("<b>Save Design</b><br>Save the spacecraft design. "
				"Writes a .ship file of the old designer, which Load Design and the studio's File, Open read again. "
				"The same as File, Save: the first time it asks for a file name; File, Save As asks for another."));
	connect(m_SaveButton, SIGNAL(clicked()), SLOT(SaveShip()));
	tools->addWidget(m_SaveButton, 0, 2);

	m_RenderMode = Button(":data/dp_RenderMode.png", tr("<b>Render Mode</b><br>Switch view between line drawing or solid "
				"rendering.<br>The line drawing can be passed through. The solid rendering is the ship as it will be, "
				"with its walls and floors in the way; it follows the design a moment after each change."));
	m_RenderMode->setCheckable(true);
	connect(m_RenderMode, SIGNAL(clicked(bool)), SLOT(RenderModeClicked(bool)));
	tools->addWidget(m_RenderMode, 1, 0);

	m_Gravity = Button(":data/dp_Gravity.png", tr("<b>Gravity</b><br>Switch gravity on and off.<br>With gravity on you "
				"walk the decks of the solid rendering; off, you float."));
	m_Gravity->setCheckable(true);
	m_Gravity->setChecked(g_Au.DesignerGravityOn());
	connect(m_Gravity, SIGNAL(clicked(bool)), SLOT(GravityClicked(bool)));
	tools->addWidget(m_Gravity, 1, 1);

	QToolButton* deckAbove = Button(":data/dp_DeckAbove.png", tr("<b>Move Grid Up</b><br>Move the design grid to the next deck aloft."));
	connect(deckAbove, SIGNAL(clicked()), SLOT(DeckAbove()));
	tools->addWidget(deckAbove, 2, 0);

	QToolButton* deckBelow = Button(":data/dp_DeckBelow.png", tr("<b>Move Grid Down</b><br>Move the design grid to the next deck below."));
	connect(deckBelow, SIGNAL(clicked()), SLOT(DeckBelow()));
	tools->addWidget(deckBelow, 3, 0);

	static const char* gridIcon[4] = { ":data/dp_Grid0.png", ":data/dp_Grid45.png", ":data/dp_Grid90.png", ":data/dp_Grid135.png" };
	const QString gridTip[4] =
	{
		tr("<b>Lateral Grid Lines</b><br>Enable grid lines on the beam."),
		tr("<b>Diagonal Grid Lines</b><br>Enable diagonal grid lines to the port quarter."),
		tr("<b>Longitudinal Grid Lines</b><br>Enable grid lines fore and aft."),
		tr("<b>Diagonal Grid Lines</b><br>Enable diagonal grid lines to the starboard quarter.")
	};
	static const int gridRow[4] = { 2, 3, 2, 3 }, gridCol[4] = { 2, 1, 1, 2 };
	for(int i = 0; i < 4; i++)
	{
		m_GridButton[i] = Button(gridIcon[i], gridTip[i]);
		m_GridButton[i]->setCheckable(true);
		m_GridButton[i]->setChecked(i == 0 || i == 2);
		connect(m_GridButton[i], SIGNAL(clicked()), SLOT(GridToggled()));
		tools->addWidget(m_GridButton[i], gridRow[i], gridCol[i]);
	}

	QToolButton* eraseDeck = Button(":data/dp_DelDeck.png", tr("<b>Erase Current Deck</b><br>Erase the current deck from the "
				"spacecraft design. All rooms and equipment on the current deck will be erased."));
	connect(eraseDeck, SIGNAL(clicked()), SLOT(EraseDeck()));
	tools->addWidget(eraseDeck, 2, 4);

	QToolButton* moveButton = Button(":data/dp_Move.png", tr("<b>Move/Copy Rooms And Equipment</b><br>Moves rooms and equipment "
				"of the entire design or the current deck.<br>Copies rooms and equipment of the current deck."));
	connect(moveButton, SIGNAL(clicked()), SLOT(MoveCopy()));
	tools->addWidget(moveButton, 3, 4);

	QHBoxLayout* topRow = new QHBoxLayout;
	topRow->setMargin(0);
	topRow->setSpacing(4);
	topRow->addLayout(tools);
	topRow->addWidget(CreateRoomsGroup(), 1);

	QHBoxLayout* bottomRow = new QHBoxLayout;
	bottomRow->setMargin(0);
	bottomRow->setSpacing(2);
	QToolButton* armorUp = Button(":data/dp_ArmorIncrease.png", tr("<b>Increase Hull Armor</b><br>Increase the hull armor level. "
				"Improves defense against physical damage and increases spacecraft mass."));
	connect(armorUp, SIGNAL(clicked()), SLOT(ArmorIncrease()));
	bottomRow->addWidget(armorUp);

	QToolButton* armorDown = Button(":data/dp_ArmorDecrease.png", tr("<b>Decrease Hull Armor</b><br>Decrease the hull armor level. "
				"Reduces defense against physical damage and decreases spacecraft mass."));
	connect(armorDown, SIGNAL(clicked()), SLOT(ArmorDecrease()));
	bottomRow->addWidget(armorDown);

	m_ArmorLabel = new QLabel(this);
	m_ArmorLabel->setToolTip(tr("<b>Hull Armor</b><br>The hull armor level, 0 to 10. It also decides the look of the hull walls "
				"and which rooms have windows."));
	bottomRow->addWidget(m_ArmorLabel);
	bottomRow->addSpacing(6);

	m_Streamline = new QCheckBox(tr("Streamline Hull"), this);
	m_Streamline->setToolTip(tr("<b>Streamline Hull</b><br>Hull streamlining enables greater velocity when maneuvering within an "
				"atmosphere. Streamlining increases spacecraft mass by 10%."));
	connect(m_Streamline, SIGNAL(clicked(bool)), SLOT(StreamlineClicked(bool)));
	bottomRow->addWidget(m_Streamline);
	bottomRow->addStretch(1);

	m_MinQuality = new QSpinBox(this);
	m_MinQuality->setRange(1, 255);
	m_MinQuality->setValue(255);
	m_MinQuality->setToolTip(tr("<b>Quality</b><br>Applies a minimum value to the quality of the design. Configures the design for "
				"manufacturing at a higher quality than the design requires due to its armor, equipment, and size."));
	connect(m_MinQuality, SIGNAL(valueChanged(int)), SLOT(MinQualityChanged(int)));
	bottomRow->addWidget(m_MinQuality);

	if(bd)
	{
		armorUp->hide();
		armorDown->hide();
		m_ArmorLabel->hide();
		m_Streamline->hide();
		m_MinQuality->hide();

		m_MaterialBox = new QComboBox(this);
		m_MaterialBox->addItem(tr("Wood (logs)"), (int)OldShip::BM_Wood);
		m_MaterialBox->addItem(tr("Lumber"), (int)OldShip::BM_Lumber);
		m_MaterialBox->addItem(tr("Stone"), (int)OldShip::BM_Stone);
		m_MaterialBox->addItem(tr("Metal"), (int)OldShip::BM_Metal);
		m_MaterialBox->addItem(tr("Magmium"), (int)OldShip::BM_Magmium);
		m_MaterialBox->addItem(tr("Vulcium"), (int)OldShip::BM_Vulcium);
		m_MaterialBox->setCurrentIndex(qMax(0, m_MaterialBox->findData(m_Material)));
		m_MaterialBox->setFocusPolicy(Qt::NoFocus);
		m_MaterialBox->setToolTip(tr("<html><b>Building Material</b><br>What the building is built of. It decides the look of the "
					"building: the shape of its walls and its textures.<br><br><i>An old design was drawn anew for whatever its "
					"building was built of, so the material is not kept in the design file: it is asked for here.</i></html>"));
		connect(m_MaterialBox, SIGNAL(activated(int)), SLOT(MaterialChanged(int)));
		bottomRow->insertWidget(0, m_MaterialBox);

		m_IndustryBox = new QComboBox(this);
		QMap<QString, int> byName;
		for(int i = 0; i < OldShip::BuildingIndustryCount(); i++)
			if(IsDesignIndustry(OldShip::BuildingIndustryAt(i)) && !byName.contains(QString::fromUtf8(OldShip::BuildingIndustryAt(i).name)))
				byName.insert(QString::fromUtf8(OldShip::BuildingIndustryAt(i).name), OldShip::BuildingIndustryAt(i).id);
		for(QMap<QString, int>::const_iterator ni = byName.constBegin(); ni != byName.constEnd(); ni++)
			m_IndustryBox->addItem(ni.key(), ni.value());
		m_IndustryBox->setMaxVisibleItems(24);
		m_IndustryBox->setFocusPolicy(Qt::NoFocus);
		m_IndustryBox->setToolTip(tr("<html><b>Industry</b><br>What the building is for. The industry decides how big the building "
					"site is, how many storeys the building can have and whether it can have basements; the design grid shows the "
					"site. A design file is named for its industry: a House design is saved as a <i>.House</i> file.</html>"));
		connect(m_IndustryBox, SIGNAL(activated(int)), SLOT(IndustryChanged(int)));
		bottomRow->insertWidget(1, m_IndustryBox, 1);
		SelectIndustry(m_Building.industry);
	}

	QHBoxLayout* extraRow = new QHBoxLayout;
	extraRow->setMargin(0);
	extraRow->setSpacing(2);

	if(bd)
	{
		newButton->setToolTip(tr("<html><b>New Design</b><br>Start a new building design, for the industry and of the material "
					"chosen below.</html>"));
		loadButton->setToolTip(tr("<html><b>Load Design</b><br>Edit an existing building design. Reads a building design file of the "
					"old designer: a file named for its industry, like <i>Town Hall.Capitol</i>.</html>"));
		m_SaveButton->setToolTip(tr("<html><b>Save Design</b><br>Save the building design. Writes a building design file of the old "
					"designer, named for its industry, which Load Design and the studio's File, Open read again. The same as File, "
					"Save: the first time it asks for a file name; File, Save As asks for another.</html>"));
		m_RenderMode->setToolTip(tr("<html><b>Render Mode</b><br>Switch view between line drawing or solid rendering.<br>The line "
					"drawing can be passed through. The solid rendering is the building as it will be, with its walls and floors in "
					"the way; it follows the design a moment after each change.</html>"));
		m_Gravity->setToolTip(tr("<html><b>Gravity</b><br>Switch gravity on and off.<br>With gravity on you walk the floors of the "
					"solid rendering; off, you float.</html>"));
		deckAbove->setToolTip(tr("<html><b>Move Grid Up</b><br>Move the design grid to the next storey up.</html>"));
		deckBelow->setToolTip(tr("<html><b>Move Grid Down</b><br>Move the design grid to the next storey down. Storeys below 0 are "
					"basements, for the industries that have them.</html>"));
		const QString bdGridTip[4] =
		{
			tr("<html><b>East-West Grid Lines</b><br>Enable grid lines running east and west.</html>"),
			tr("<html><b>Diagonal Grid Lines</b><br>Enable diagonal grid lines running southwest to northeast.</html>"),
			tr("<html><b>North-South Grid Lines</b><br>Enable grid lines running north and south.</html>"),
			tr("<html><b>Diagonal Grid Lines</b><br>Enable diagonal grid lines running northwest to southeast.</html>")
		};
		for(int i = 0; i < 4; i++)
			m_GridButton[i]->setToolTip(bdGridTip[i]);
		eraseDeck->setToolTip(tr("<html><b>Erase Current Storey</b><br>Erase the current storey from the building design. All rooms, "
					"roofs and equipment on the current storey will be erased.</html>"));
		moveButton->setToolTip(tr("<html><b>Move/Copy Rooms And Equipment</b><br>Moves rooms and equipment of the entire design or "
					"the current storey.<br>Copies rooms and equipment of the current storey.</html>"));
	}
	if(!bd)
	{
		QPushButton* checkPaths = new QPushButton(QIcon(":data/dp_Check.png"), tr("Check Paths"), this);
		checkPaths->setToolTip(tr("<html><b>Check Paths</b><br>Tests paths through the design. Displays a report of the "
					"results.</html>"));
		connect(checkPaths, SIGNAL(clicked()), SLOT(CheckPaths()));
		extraRow->addWidget(checkPaths);

		QPushButton* systemModules = new QPushButton(QIcon(":data/UpgradeModule.png"), tr("System Modules"), this);
		systemModules->setToolTip(tr("<html><b>System Modules</b><br>Configure modules for the FTL drive, sensors, shields, weapon "
					"systems, transporters and surgery units: which weapon or other bay equipment each weapon system has, for "
					"example. All weapon bay rooms with the same room number are one weapon system. The modules are saved with "
					"the old design.</html>"));
		connect(systemModules, SIGNAL(clicked()), SLOT(SystemModules()));
		extraRow->addWidget(systemModules);
	}
	QPushButton* finalize = new QPushButton(QIcon(":data/ScFinish.png"), tr("Finalize"), this);
	finalize->setToolTip(tr("<html><b>Finalize</b><br>Makes the old design a blueprint of the new kind. Finalize it as is: its "
				"hull volume is worked out, then the blueprint's properties and the Finalize dialog follow. Or move it to the new "
				"editor to change it further. The old design is kept.</html>"));
	connect(finalize, SIGNAL(clicked()), SIGNAL(FinalizeRequested()));
	extraRow->addWidget(finalize);
	extraRow->addSpacing(6);

	extraRow->addStretch(1);

	m_DeckLabel = new QLabel(this);
	extraRow->addWidget(m_DeckLabel);

	QWidget* palette = new QWidget(this);
	QVBoxLayout* paletteLay = new QVBoxLayout(palette);
	paletteLay->setMargin(0);
	paletteLay->setSpacing(2);
	paletteLay->addLayout(topRow);
	paletteLay->addWidget(CreateEquipmentGroup());
	paletteLay->addLayout(bottomRow);
	paletteLay->addLayout(extraRow);

	m_Status = new QLabel(this);
	m_Status->setWordWrap(true);
	m_Status->setMinimumHeight(fontMetrics().height() * 3 + 4);
	m_Status->setAlignment(Qt::AlignLeft | Qt::AlignTop);

	m_Report = new QTextBrowser(this);
	m_Report->setReadOnly(true);
	m_Report->setOpenLinks(false);
	connect(m_Report, SIGNAL(anchorClicked(const QUrl&)), SLOT(ReportLinkClicked(const QUrl&)));

	QVBoxLayout* lay = new QVBoxLayout(this);
	lay->setMargin(2);
	lay->setSpacing(2);
	lay->addWidget(palette);
	lay->addWidget(m_Status);
	lay->addWidget(m_Report, 1);

	UpdateControls();
	UpdateReport();
}

AuOldShipPanel::~AuOldShipPanel()
{
}

void AuOldShipPanel::ReportLinkClicked(const QUrl& link)
{
	const QString what = link.toString();
	if(!what.startsWith(QString("problem:")))
		return;
	const int at = what.mid(8).toInt();
	if(at < 0 || at >= m_Problems.size())
		return;
	const OldShip::Problem& problem = m_Problems[at];
	if(problem.deck > -OldShip::DeckLimit && problem.deck < OldShip::DeckLimit)
		m_Deck = problem.deck;
	m_RoofClicks = 0;
	m_Status->setText(problem.text.isEmpty() ? problem.line : problem.text);
	UpdateControls();
	emit LookRequested(problem.at.x, problem.at.y, m_Deck);
}

QToolButton* AuOldShipPanel::Button(const QString& icon, const QString& toolTip, bool available)
{
	QToolButton* button = new QToolButton(this);
	button->setIcon(QIcon(icon));
	button->setIconSize(QSize(16, 16));
	button->setAutoRaise(true);
	QString tip(toolTip);
	tip.remove("<html>");
	tip.remove("</html>");
	button->setToolTip("<html>" + tip + "</html>");
	button->setEnabled(available);
	button->setFocusPolicy(Qt::NoFocus);
	return button;
}

QToolButton* AuOldShipPanel::AddTool(TOOL tool, int arg, const QString& icon, const QString& toolTip, int commodity)
{
	const bool bd = IsBuilding();
	bool available = tool != TOOL_Detail || (bd ? OldShip::DetailTypeBuildingName(arg) != 0 : OldShip::Editor::CanPlace(arg));
	QString tip(toolTip);
	if(tool == TOOL_Room || tool == TOOL_Roof)
	{
		const OldShip::RoomTypeInfo* info = OldShip::RoomTypeInfoOf(arg);
		if(bd && OldShip::RoomTypeBuildingName(arg))
			tip = QString("<b>%1</b><br>%2").arg(QString::fromUtf8(OldShip::RoomTypeBuildingName(arg)))
					.arg(QString::fromUtf8(OldShip::RoomTypeBuildingDesc(arg)));
		else if(info)
			tip = QString("<b>%1</b><br>%2").arg(QString::fromUtf8(info->name)).arg(QString::fromUtf8(info->desc));
		if(tool == TOOL_Roof)
			tip += arg == 24 ? tr("<br>Three clicks on the grid of the storey above the rooms it covers: the roof's middle, the end "
						"of one radius, and how far it reaches the other way. Escape starts over.")
					: tr("<br>Three clicks on the grid of the storey above the rooms it covers: the two ends of one eave, and how far "
						"the roof reaches from it. The ridge runs square to the first eave. Escape starts over.");
	}
	else if(tool == TOOL_Detail)
	{
		const OldShip::DetailTypeInfo* info = OldShip::DetailTypeInfoOf(arg);
		if(bd && OldShip::DetailTypeBuildingName(arg))
			tip = QString("<b>%1</b><br>%2").arg(QString::fromUtf8(OldShip::DetailTypeBuildingName(arg)))
					.arg(QString::fromUtf8(OldShip::DetailTypeBuildingDesc(arg)));
		else if(info)
			tip = QString("<b>%1</b><br>%2").arg(QString::fromUtf8(info->name)).arg(QString::fromUtf8(info->desc));
		if(bd && (arg == 100 || arg == 142))
			tip += tr("<br>It stands outside, on the ground. A building has one: a second takes the first one's place.");
		if(commodity)
			tip += tr("<br>This one is for the <b>%1</b>.").arg(VehicleName(commodity));
		if(OldShip::DetailType::IsStairs(arg))
			tip += tr("<br>The stairs stand at one corner of the floor they cover; the outline under the mouse shows it. "
					"They take the place of equipment standing there.");
		if(DetailVariants(arg))
			tip += tr("<br>Turn it with the left and right arrow keys before placing it.");
	}
	if(!available)
		tip += NotHere(bd ? tr("Not for buildings.") : tr("Not for spacecraft."));

	QToolButton* button = Button(icon, tip, available);
	button->setCheckable(true);
	ToolDef def;
	def.tool = tool;
	def.arg = arg;
	def.icon = icon;
	def.commodity = commodity;
	m_ToolGroup->addButton(button, m_Tools.size());
	m_Tools.append(def);
	return button;
}

QWidget* AuOldShipPanel::CreateRoomsGroup()
{
	QGroupBox* group = new QGroupBox(tr("Rooms"), this);
	group->setFlat(true);
	QGridLayout* lay = new QGridLayout(group);
	lay->setMargin(12);
	lay->setSpacing(0);
	lay->setColumnMinimumWidth(8, 6);
	lay->setColumnStretch(8, 1);

	m_RoomMode = Button(":data/dp_RoomMode0.png", tr("<b>Room Mode</b><br>Switches between painting room cells and changing "
				"the type of a whole room: when it is on, a click with a room button selected changes the room clicked to "
				"that kind of room."));
	m_RoomMode->setCheckable(true);
	connect(m_RoomMode, SIGNAL(toggled(bool)), SLOT(RoomModeToggled(bool)));
	lay->addWidget(m_RoomMode, 0, 0);

	lay->addWidget(AddTool(TOOL_EraseCells, 0, ":data/dp_DelRoomCells.png", tr("<b>Erase Room Cells</b><br>Erases room cells from "
					"the current deck of the design.")), 1, 0);
	lay->addWidget(AddTool(TOOL_EraseRoom, 0, ":data/dp_DelRoom.png", tr("<b>Erase Room</b><br>Erases entire rooms from the "
					"current deck when you click in them.")), 2, 0);

	struct RoomButton { int row, col, type; const char* icon; };
	static const RoomButton rooms[] =
	{
		{ 0, 1, 1, "dp_Barracks" }, { 0, 2, 2, "dp_Bridge" }, { 0, 3, 3, "dp_Brig" }, { 0, 4, 7, "dp_CrewCabin" },
		{ 0, 5, 8, "dp_EngineRoom" }, { 0, 6, 13, "dp_LandingBay" }, { 0, 7, 9, "dp_Fuel" },
		{ 1, 1, 10, "dp_Galley" }, { 1, 2, 11, "dp_GravityWell" }, { 1, 3, 6, "dp_Corridor" }, { 1, 4, 5, "dp_Cargo" },
		{ 1, 5, 4, "dp_CaptCabin" }, { 1, 6, 15, "dp_OpenToBelow" }, { 1, 7, 14, "dp_Lounge" },
		{ 2, 1, 16, "dp_SensorRoom" }, { 2, 2, 17, "dp_ShieldRoom" }, { 2, 3, 18, "dp_SickBay" }, { 2, 4, 19, "dp_Stateroom" },
		{ 2, 5, 20, "dp_TransporterRoom" }, { 2, 6, 21, "dp_WeaponBay" }
	};
	for(size_t i = 0; i < sizeof(rooms) / sizeof(rooms[0]); i++)
	{
		if(IsBuilding() && !OldShip::RoomTypeBuildingName(rooms[i].type))
			continue;
		lay->addWidget(AddTool(TOOL_Room, rooms[i].type, QString(":data/%1.png").arg(rooms[i].icon), QString()), rooms[i].row,
				rooms[i].col);
	}
	if(IsBuilding())
	{
		lay->addWidget(AddTool(TOOL_Roof, 22, ":data/dp_RoofGable.png", QString()), 0, 4);
		lay->addWidget(AddTool(TOOL_Roof, 23, ":data/dp_RoofHip.png", QString()), 1, 5);
		lay->addWidget(AddTool(TOOL_Roof, 24, ":data/dp_RoofRound.png", QString()), 2, 7);
	}

	QToolButton* numberUp = Button(":data/dp_RoomIdUp.png", tr("<html><b>Increase Room Number</b><br>Increases the room number. "
				"Room number is used to distinguish between rooms of the same type on the same deck.</html>"));
	numberUp->setAutoRepeat(true);
	connect(numberUp, SIGNAL(clicked()), SLOT(RoomNumberUp()));
	lay->addWidget(numberUp, 0, 9);

	m_RoomNumberButton = new QPushButton("0", group);
	m_RoomNumberButton->setMinimumWidth(20);
	m_RoomNumberButton->setMaximumWidth(32);
	m_RoomNumberButton->setCheckable(true);
	m_RoomNumberButton->setFocusPolicy(Qt::NoFocus);
	m_RoomNumberButton->setToolTip(tr("<html><b>Room Number</b><br>Room number is used to distinguish between rooms of the same "
				"type on the same deck.<br><br>Push this button to change the room number of an existing room to the number "
				"shown: then click the room. The room number cannot change if this would create two rooms of the same type "
				"with the same number on a deck.</html>"));
	connect(m_RoomNumberButton, SIGNAL(clicked()), SLOT(RoomNumberPressed()));
	lay->addWidget(m_RoomNumberButton, 1, 9);

	QToolButton* numberDown = Button(":data/dp_RoomIdDown.png", tr("<html><b>Decrease Room Number</b><br>Decreases the room number. "
				"Room number is used to distinguish between rooms of the same type on the same deck.</html>"));
	numberDown->setAutoRepeat(true);
	connect(numberDown, SIGNAL(clicked()), SLOT(RoomNumberDown()));
	lay->addWidget(numberDown, 2, 9);

	return group;
}

QWidget* AuOldShipPanel::CreateEquipmentGroup()
{
	QGroupBox* group = new QGroupBox(tr("Equipment"), this);
	group->setFlat(true);
	QGridLayout* lay = new QGridLayout(group);
	lay->setMargin(12);
	lay->setSpacing(0);
	lay->setColumnMinimumWidth(13, 6);
	lay->setColumnStretch(13, 1);

	lay->addWidget(AddTool(TOOL_EraseDetail, 0, ":data/dp_DelDetail.png", tr("<b>Erase Equipment</b><br>Erase equipment one unit "
					"at a time.")), 0, 0);
	lay->addWidget(AddTool(TOOL_EraseDetails, 0, ":data/dp_DelDetails.png", tr("<b>Erase Equipment in Area</b><br>Erase equipment "
					"in a brush-sized area. Only equipment that is the same type as the equipment at the click location is "
					"erased.")), 1, 0);

	struct DetailButton { int row, col, type; const char* icon; int commodity; };
	static const DetailButton details[] =
	{
		{ 0, 1, 73, "dp_CmdConsole" }, { 0, 2, 12, "dp_PilotConsole" }, { 0, 3, 6, "dp_EngineerConsole" },
		{ 0, 4, 17, "dp_ShieldConsole" }, { 0, 5, 15, "dp_SensorConsole" }, { 0, 6, 1, "dp_NavConsole" },
		{ 0, 7, 7, "dp_FireControlConsole" }, { 0, 8, 13, "dp_PowerConsole" }, { 0, 9, 4, "dp_SurgeryConsole" },
		{ 0, 10, 19, "dp_TransporterConsole" }, { 0, 11, 22, "dp_TurretTopBottom" }, { 0, 12, 21, "dp_TurretSide" },
		{ 1, 2, 10, "dp_ManeuverDrive" }, { 1, 3, 14, "dp_PowerPlant" }, { 1, 4, 18, "dp_Shield" }, { 1, 5, 16, "dp_Sensor" },
		{ 1, 6, 24, "dp_WormholeDrive" }, { 1, 7, 23, "dp_Weapon" }, { 1, 8, 32, "dp_LifeSupport" },
		{ 1, 9, 11, "dp_OperatingTable" }, { 1, 10, 20, "dp_TransporterPad" }, { 1, 11, 65, "dp_Berth0" },
		{ 1, 12, 2, "dp_Berth2" },
		{ 2, 1, 50, "dp_HullDoor" }, { 2, 2, 49, "dp_CrewDoor" }, { 2, 3, 5, "dp_Door" }, { 2, 4, 3, "dp_CargoDoor" },
		{ 2, 5, 54, "dp_LargeCrewDoor" }, { 2, 6, 53, "dp_LargeDoor" }, { 2, 7, 52, "dp_HullHatch" },
		{ 2, 8, 51, "dp_CrewHatch" }, { 2, 9, 8, "dp_Hatch" }, { 2, 10, 55, "dp_WallOpening" }, { 2, 11, 9, "dp_HullOpening" },
		{ 2, 12, 56, "dp_Railing" },
		{ 3, 6, 141, "ScTroop" }, { 3, 8, 101, "dp_StairsStraight" }, { 3, 9, 109, "dp_StairsTurnLeft" },
		{ 3, 10, 117, "dp_StairsTurnRight" }, { 3, 11, 125, "dp_StairsUTurnLeft" }, { 3, 12, 133, "dp_StairsUTurnRight" },
		{ 4, 0, 76, "dp_PivotPoint" }, { 4, 1, 98, "dp_ContrailEmitter" }, { 4, 2, 99, "dp_WeaponEmitter" },
		{ 4, 4, 74, "dp_GameTable" },
		{ 4, 6, 57, "c_CommercialAircraft", OldShip::VEHICLE_SpaceTransport }, { 4, 7, 57, "c_SpaceFighter", OldShip::VEHICLE_SpaceFighter },
		{ 4, 8, 57, "c_SpaceFighter2", OldShip::VEHICLE_SpaceFighter2 }, { 4, 10, 57, "c_APC", OldShip::VEHICLE_Apc },
		{ 4, 11, 57, "c_MotorBike", OldShip::VEHICLE_MotorBike }, { 4, 12, 57, "c_Suv", OldShip::VEHICLE_Suv }
	};
	for(size_t i = 0; i < sizeof(details) / sizeof(details[0]); i++)
	{
		if(IsBuilding() && !OldShip::DetailTypeBuildingName(details[i].type))
			continue;
		lay->addWidget(AddTool(TOOL_Detail, details[i].type, QString(":data/%1.png").arg(details[i].icon), QString(),
					details[i].commodity), details[i].row, details[i].col);
	}
	if(IsBuilding())
	{
		lay->addWidget(AddTool(TOOL_Detail, 100, ":data/dp_FlagPole.png", QString()), 4, 0);
		lay->addWidget(AddTool(TOOL_Detail, 142, ":data/dp_LightPole.png", QString()), 4, 1);
		for(int k = 0; k < OldShip::Spot_Count; k++)
		{
			const OldShip::SpotKind kind = (OldShip::SpotKind)k;
			QToolButton* button = AddTool(TOOL_Detail, OldShip::SpotBase(kind), QString::fromUtf8(OldShip::SpotIcon(kind)),
					QString());
			button->setProperty("spotKind", k);
			m_SpotButtons.append(button);
			lay->addWidget(button, 5, k);
		}
		UpdateSpotTools();
	}

	QToolButton* brushUp = Button(":data/dp_BrushSzUp.png", tr("<b>Increase Brush Size</b><br>Increase the brush size for drawing "
				"and erasing equipment."));
	brushUp->setAutoRepeat(true);
	connect(brushUp, SIGNAL(clicked()), SLOT(BrushUp()));
	lay->addWidget(brushUp, 0, 14);

	m_BrushLabel = new QLabel("0", group);
	m_BrushLabel->setAlignment(Qt::AlignCenter);
	m_BrushLabel->setToolTip(tr("<html><b>Brush Size</b><br>Size of brush used for drawing and erasing equipment.<br>When drawing, "
				"equipment fills the brush area in the room at the click location. Only certain types of equipment fills an "
				"area of a room.<br>When erasing, only equipment that is the same type as the equipment at the click location "
				"is erased.</html>"));
	lay->addWidget(m_BrushLabel, 1, 13, 1, 2);

	QToolButton* brushDown = Button(":data/dp_BrushSzDown.png", tr("<b>Decrease Brush Size</b><br>Decrease the brush size for "
				"drawing and erasing equipment."));
	brushDown->setAutoRepeat(true);
	connect(brushDown, SIGNAL(clicked()), SLOT(BrushDown()));
	lay->addWidget(brushDown, 2, 14);

	return group;
}

bool AuOldShipPanel::IsLogLineNoteworthy(const QString& line)
{
	return line.startsWith("WARNING") || line.startsWith("TOO BIG") || line.startsWith("FAILED") || line.contains("refused");
}

int AuOldShipPanel::GridLines() const
{
	int gridLines = 0;
	for(int i = 0; i < 4; i++)
		if(m_GridButton[i]->isChecked())
			gridLines |= 1 << i;
	return gridLines;
}

int AuOldShipPanel::ToolDetailType() const
{
	return m_Tool == TOOL_Detail ? DetailVariant(m_ToolArg, m_Turn) : -1;
}

QString AuOldShipPanel::RoomName(int roomType) const
{
	return RoomTypeName(roomType, IsBuilding());
}

QString AuOldShipPanel::DetailName(int detailType) const
{
	return DetailTypeName(detailType, IsBuilding());
}

QString AuOldShipPanel::DeckWord(bool capital) const
{
	if(IsBuilding())
		return capital ? tr("Storey") : tr("storey");
	return capital ? tr("Deck") : tr("deck");
}

OldBuilding::Design AuOldShipPanel::Building() const
{
	OldBuilding::Design building(m_Building);
	OldShip::EditToBuilding(m_Design, building);
	return building;
}

QString AuOldShipPanel::BuildingFileFilter()
{
	QStringList endings;
	for(int i = 0; i < OldShip::BuildingIndustryCount(); i++)
	{
		const OldShip::BuildingIndustryInfo& info = OldShip::BuildingIndustryAt(i);
		const QString ending = QString("*.%1").arg(QString::fromStdString(OldShip::BuildingFileExtension(info.id)));
		if(info.id >= 3 && info.id <= 139 && info.id != 10 && info.id != 114 && !endings.contains(ending))
			endings.append(ending);
	}
	return QString("Old Building Designs (%1)").arg(endings.join(" "));
}

bool AuOldShipPanel::IsBuildingFileName(const QString& path)
{
	const QString ending = QFileInfo(path).suffix();
	return !ending.isEmpty() && OldShip::BuildingIndustryOfExtension(ending.toStdString()) >= 3;
}

QString AuOldShipPanel::FileFilter() const
{
	if(!IsBuilding())
		return QString("Old Spacecraft Designs (*.ship);;All Files (*.*)");
	const OldShip::BuildingIndustryInfo* info = OldShip::BuildingIndustryInfoOf(m_Building.industry);
	const QString ending = QString::fromStdString(OldShip::BuildingFileExtension(m_Building.industry));
	QString filter;
	if(info && !ending.isEmpty())
		filter = QString("%1 Designs (*.%2);;").arg(QString::fromUtf8(info->name)).arg(ending);
	return filter + BuildingFileFilter() + QString(";;All Files (*.*)");
}

bool AuOldShipPanel::LoadBuildingFile(const QString& path, OldBuilding::Design& building, QString& why)
{
	QFile file(path);
	if(!file.open(QIODevice::ReadOnly))
	{
		why = tr("The file cannot be opened.");
		return false;
	}
	const QByteArray bytes = file.readAll();
	file.close();

	std::vector<uint8_t> data(bytes.constData(), bytes.constData() + bytes.size());
	std::string reason;
	OldBuilding::Design read;
	if(!read.Parse(data, reason))
	{
		why = tr("It is not a building design of the old designer (%1).").arg(QString::fromStdString(reason));
		return false;
	}
	building = read;
	return true;
}

void AuOldShipPanel::SelectIndustry(int industry)
{
	if(!m_IndustryBox)
		return;
	int at = m_IndustryBox->findData(industry);
	if(at < 0)
	{
		const OldShip::BuildingIndustryInfo* info = OldShip::BuildingIndustryInfoOf(industry);
		m_IndustryBox->addItem(info ? tr("%1 (no building site)").arg(QString::fromUtf8(info->name)) : tr("Industry %1").arg(industry),
				industry);
		at = m_IndustryBox->count() - 1;
	}
	m_IndustryBox->setCurrentIndex(at);
}

void AuOldShipPanel::SetIndustry(int industry)
{
	m_Building.industry = industry;
	m_Rules.industry = industry;
	m_RoofClicks = 0;
	g_Au.Settings().writeEntry(BuildingIndustryKey, industry);
	SelectIndustry(industry);
	UpdateSpotTools();
}

void AuOldShipPanel::UpdateSpotTools()
{
	for(int i = 0; i < m_SpotButtons.size(); i++)
	{
		const OldShip::SpotKind kind = (OldShip::SpotKind)m_SpotButtons[i]->property("spotKind").toInt();
		const bool allowed = SpotAllowed(m_Building.industry, kind);
		m_SpotButtons[i]->setEnabled(allowed);
		QString tip = QString("<html><b>%1</b><br>%2").arg(QString::fromUtf8(OldShip::SpotName(OldShip::SpotBase(kind))))
				.arg(QString::fromUtf8(OldShip::SpotDesc(OldShip::SpotBase(kind))));
		if(kind != OldShip::Spot_Spacecraft)
			tip += tr("<br>Turn it with the left and right arrow keys before placing it.");
		if(!allowed)
			tip += tr("<br><i>Not for this industry.</i>");
		m_SpotButtons[i]->setToolTip(tip + "</html>");
	}
}

void AuOldShipPanel::IndustryChanged(int index)
{
	if(!m_IndustryBox || index < 0)
		return;
	const int industry = m_IndustryBox->itemData(index).toInt();
	if(industry == m_Building.industry)
		return;
	SetIndustry(industry);
	std::vector<std::string> faults;
	OldShip::Editor(m_Design, Rules()).BuildingFaults(faults);
	const double radius = OldShip::BuildingSiteRadius(industry, m_Deck);
	QString said = tr("%1: the site reaches %2 ft from its middle each way, the storeys go from %3 to %4.")
			.arg(m_IndustryBox->itemText(index)).arg(radius, 0, 'f', 0).arg(m_Rules.MinDeck()).arg(m_Rules.MaxDeck());
	if(!faults.empty())
		said += QString(" <font color=\"#ff8080\">%1</font>").arg(QString::fromStdString(faults[0]).toHtmlEscaped());
	m_Status->setText(said);
	DesignChanged();
}

void AuOldShipPanel::MaterialChanged(int index)
{
	if(!m_MaterialBox || index < 0)
		return;
	const int material = m_MaterialBox->itemData(index).toInt();
	if(material == m_Material)
		return;
	m_Material = material;
	g_Au.Settings().writeEntry(BuildingMaterialKey, material);
	m_Status->setText(tr("The building is of %1. The solid rendering shows it.").arg(OldShip::BuildingMaterialName(material)));
	m_Revision++;
	UpdateControls();
	UpdateReport();
	emit DesignEdited();
}

bool AuOldShipPanel::LoadShipFile(const QString& path, OldShip::Design& ship, QString& why)
{
	QFile file(path);
	if(!file.open(QIODevice::ReadOnly))
	{
		why = tr("The file cannot be opened.");
		return false;
	}
	const QByteArray bytes = file.readAll();
	file.close();

	std::vector<uint8_t> data(bytes.constData(), bytes.constData() + bytes.size());
	std::string reason;
	OldShip::Design read;
	if(!read.Parse(data, reason))
	{
		why = tr("It is not a spacecraft design of the old designer (%1).").arg(QString::fromStdString(reason));
		return false;
	}
	ship = read;
	return true;
}

bool AuOldShipPanel::ConfirmLoseChanges()
{
	if(!m_Modified)
		return true;
	const QString file = m_FileName.isEmpty() ? (IsBuilding() ? tr("It has not been saved as a building design file yet.")
				: tr("It has not been saved as a .ship file yet."))
			: tr("Its file is %1.").arg(QDir::toNativeSeparators(m_FileName));
	QMessageBox mb(g_Au.ApplicationName(), (IsBuilding() ? tr("<html>The old building design was changed, and the changes are not saved. "
					"%1<br><br>Save it now?</html>")
				: tr("<html>The old spacecraft design was changed, and the changes are not saved. "
					"%1<br><br>Save it now?</html>")).arg(file.toHtmlEscaped()),
			QMessageBox::Question, QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel, auApp->Frame());
	const int answer = auApp->Frame()->ExecMessageBox(mb);
	if(answer == QMessageBox::Yes)
		return Save(false);
	return answer == QMessageBox::No;
}

void AuOldShipPanel::UpdateTitle()
{
	if(!auApp || !auApp->Frame())
		return;
	const std::string& designName = IsBuilding() ? m_Building.name : m_Design.name;
	QString name = QDir::toNativeSeparators(m_FileName);
	if(name.isEmpty())
		name = !designName.empty() ? QString::fromUtf8(designName.c_str()) : IsBuilding() ? tr("New Old Building") : tr("New Old Ship");
	auApp->Frame()->setWindowTitle(QString("%1 %2 %3 %4%5").arg(g_Au.ApplicationName()).arg(g_Au.MiddleDotSymbol())
			.arg(IsBuilding() ? tr("Old Building") : tr("Old Ship")).arg(name).arg(m_Modified ? QString(" *") : QString()));
}

void AuOldShipPanel::NewDesign()
{
	m_Design = OldShip::Design();
	const int industry = m_Building.industry;
	m_Building = OldBuilding::Design();
	m_Building.industry = industry;
	AuId registration;
	do
	{
		registration = AuId(g_Au.Dice().Random());
	}
	while(registration < AuId::FirstUnreserved());
	m_Design.name = registration.StringId().toUtf8().constData();
	m_Building.name = m_Design.name;
	m_RoofClicks = 0;
	m_FileName = "";
	m_Modified = false;
	m_Revision++;
	m_ModelRevision = -1;
	m_Origin = OldShip::Vec3();
	m_ConvertLog.clear();
	m_Converted = false;
	m_ReportHtml.clear();
	m_ReportRevision = -1;
	m_Problems.clear();
	m_Deck = 0;
	m_Status->setText(tr("A new design. Pick a room button and paint cells on the grid, in the main view."));
	UpdateControls();
	UpdateReport();
	UpdateTitle();
}

void AuOldShipPanel::SetBuilding(const OldBuilding::Design& building, const QString& fileName)
{
	m_Building = building;
	m_Building.rooms.clear();
	m_Building.details.clear();
	m_Rules.industry = m_Building.industry;
	g_Au.Settings().writeEntry(BuildingIndustryKey, m_Building.industry);
	SelectIndustry(m_Building.industry);
	OldShip::Design design;
	OldShip::BuildingToEdit(building, design);
	SetDesign(design, fileName);
}

void AuOldShipPanel::SetDesign(const OldShip::Design& design, const QString& fileName)
{
	m_Design = design;
	m_RoofClicks = 0;
	m_FileName = fileName;
	m_Modified = false;
	m_Revision++;
	m_ModelRevision = -1;
	m_Origin = OldShip::Vec3();
	m_ConvertLog.clear();
	m_Converted = false;
	m_ReportHtml.clear();
	m_ReportRevision = -1;
	m_Problems.clear();

	bool hasDeck0 = false;
	for(size_t i = 0; i < m_Design.rooms.size(); i++)
		if(m_Design.rooms[i].deck == 0)
			hasDeck0 = true;
	m_Deck = hasDeck0 ? 0 : m_Design.minDeck;
	m_Status->setText(QString());
	UpdateControls();
	UpdateReport();
	UpdateTitle();
}

void AuOldShipPanel::SetConvertLog(const QStringList& log, bool converted, const OldShip::Vec3& origin)
{
	m_ConvertLog = log;
	m_Converted = converted;
	if(converted)
	{
		m_ModelRevision = m_Revision;
		m_Origin = origin;
	}
	UpdateControls();
	UpdateReport();
}

void AuOldShipPanel::SetModelGone()
{
	m_ModelRevision = -1;
	m_Origin = OldShip::Vec3();
	UpdateReport();
}

void AuOldShipPanel::SetReport(const QString& html, int revision, const QList<OldShip::Problem>& problems)
{
	m_ReportHtml = html;
	m_ReportRevision = revision;
	m_Problems = problems;
	UpdateReport();
}

void AuOldShipPanel::SetGravityShown(bool on)
{
	m_Gravity->setChecked(on);
}

void AuOldShipPanel::ShowLines()
{
	m_Solid = false;
	m_RenderMode->setChecked(false);
}

void AuOldShipPanel::RenderModeClicked(bool solid)
{
	m_Solid = solid;
	m_Status->setText(solid ? tr("Solid rendering") : tr("Line rendering"));
	emit RenderModeToggled(solid);
}

void AuOldShipPanel::GravityClicked(bool on)
{
	emit GravityToggled(on);
}

void AuOldShipPanel::DesignChanged()
{
	const bool wasModified = m_Modified;
	m_Modified = true;
	m_Revision++;
	UpdateControls();
	UpdateReport();
	if(!wasModified)
		UpdateTitle();
	emit DesignEdited();
}

void AuOldShipPanel::UpdateControls()
{
	m_ArmorLabel->setText(tr("Armor %1").arg(m_Design.armor));
	m_Streamline->blockSignals(true);
	m_Streamline->setChecked(m_Design.streamline != 0);
	m_Streamline->blockSignals(false);
	m_MinQuality->blockSignals(true);
	m_MinQuality->setValue(qBound(1, m_Design.minQuality, 255));
	m_MinQuality->blockSignals(false);
	m_RoomNumberButton->setText(QString::number(m_RoomNumber));
	m_RoomNumberButton->setChecked(m_Tool == TOOL_RoomNumber);
	m_BrushLabel->setText(QString::number(m_Brush));
	m_RenderMode->setChecked(m_Solid);
	if(IsBuilding())
		m_DeckLabel->setText(tr("Storey %1 (design has storeys %2 to %3; a %4 can have %5 to %6)").arg(m_Deck).arg(m_Design.minDeck)
				.arg(m_Design.maxDeck).arg(m_IndustryBox ? m_IndustryBox->currentText() : QString()).arg(m_Rules.MinDeck()).arg(m_Rules.MaxDeck()));
	else
		m_DeckLabel->setText(tr("Deck %1 (design has decks %2 to %3)").arg(m_Deck).arg(m_Design.minDeck).arg(m_Design.maxDeck));
}

void AuOldShipPanel::UpdateReport()
{
	QString html;
	if(m_ReportHtml.isEmpty())
		html = QString("<html>%1</html>").arg(m_Design.rooms.empty() ? tr("No rooms yet. Pick a room button and paint cells on the grid.")
				: tr("Working out the report..."));
	else
	{
		QString top;
		if(m_Modified)
			top += m_FileName.isEmpty() ? (IsBuilding() ? tr("<i>Changed, not saved as a building design file yet: File, Save.</i><br>")
						: tr("<i>Changed, not saved as a .ship file yet: File, Save.</i><br>"))
					: tr("<i>Changed since it was saved: File, Save.</i><br>");
		html = m_ReportHtml;
		html.replace(QString("<html>"), QString("<html>") + top);
	}
	if(html == m_ReportShown)
		return;
	const int at = m_Report->verticalScrollBar()->value();
	m_Report->setHtml(html);
	m_Report->verticalScrollBar()->setValue(at);
	m_ReportShown = html;
}

void AuOldShipPanel::SelectTool(TOOL tool, int arg, const QString& icon, int commodity)
{
	m_Tool = tool;
	m_ToolArg = arg;
	m_ToolCommodity = commodity;
	if(!icon.isEmpty() || tool != TOOL_Room)
		m_ToolIcon = icon;
	m_Turn = 0;
	m_RoofClicks = 0;
	switch(tool)
	{
	case TOOL_Room:
		m_Status->setText(m_RetypeMode ? tr("Click a room to make it a %1.").arg(RoomName(arg))
				: tr("Paint cells of %1 %2. Hold the button down to keep painting.").arg(RoomName(arg)).arg(m_RoomNumber));
		break;
	case TOOL_Roof:
		m_Status->setText(arg == 24 ? tr("%1: click the middle of the roof, on the storey above the rooms it is to cover.").arg(RoomName(arg))
				: tr("%1: click one end of an eave, on the storey above the rooms it is to cover.").arg(RoomName(arg)));
		break;
	case TOOL_EraseCells:
		m_Status->setText(tr("Erase room cells. A room cannot be cut in two: erase it from the outside in."));
		break;
	case TOOL_EraseRoom:
		m_Status->setText(tr("Click a room to erase it, with the equipment in it."));
		break;
	case TOOL_RoomNumber:
		m_Status->setText(tr("Click a room to give it the room number %1.").arg(m_RoomNumber));
		break;
	case TOOL_Detail:
		if(commodity)
			m_Status->setText(tr("Place: %1 for the %2. The left and right arrow keys turn it.").arg(DetailName(arg))
					.arg(VehicleName(commodity)));
		else
			m_Status->setText(DetailVariants(arg) ? tr("Place: %1. The left and right arrow keys turn it.").arg(DetailName(arg))
					: tr("Place: %1.").arg(DetailName(arg)));
		break;
	case TOOL_EraseDetail:
	case TOOL_EraseDetails:
		m_Status->setText(tool == TOOL_EraseDetail ? tr("Click a piece of equipment to erase it.")
				: tr("Click a piece of equipment to erase it and the others of its kind within the brush size."));
		break;
	default:
		break;
	}
	UpdateControls();
}

void AuOldShipPanel::ToolPicked(int id)
{
	if(id < 0 || id >= m_Tools.size())
		return;
	SelectTool(m_Tools[id].tool, m_Tools[id].arg, m_Tools[id].icon, m_Tools[id].commodity);
}

void AuOldShipPanel::RoomNumberPressed()
{
	QAbstractButton* down = m_ToolGroup->checkedButton();
	if(down)
	{
		m_ToolGroup->setExclusive(false);
		down->setChecked(false);
		m_ToolGroup->setExclusive(true);
	}
	SelectTool(TOOL_RoomNumber, 0, QString());
}

void AuOldShipPanel::RoomModeToggled(bool on)
{
	m_RetypeMode = on;
	m_RoomMode->setIcon(QIcon(on ? ":data/dp_RoomMode1.png" : ":data/dp_RoomMode0.png"));
	if(m_Tool == TOOL_Room)
		SelectTool(m_Tool, m_ToolArg, QString(), m_ToolCommodity);
}

int AuOldShipPanel::DetailVariant(int baseType, int turn) const
{
	const int* variants = DetailVariants(baseType);
	if(OldShip::DetailType::IsStairs(baseType))
		turn &= 6;
	return variants ? variants[turn & 7] : baseType;
}

bool AuOldShipPanel::ViewTurn(int step)
{
	if(m_Tool != TOOL_Detail || !DetailVariants(m_ToolArg))
		return false;
	if(OldShip::DetailType::IsStairs(m_ToolArg))
		m_Turn = ((m_Turn & 6) + step * 2 + 8) & 6;
	else
		m_Turn = (m_Turn + step + 8) & 7;
	if(m_ToolArg == 74)
		m_Status->setText(m_Turn & 1 ? tr("Game table, turned 45 degrees.") : tr("Game table, square to the grid."));
	else
		m_Status->setText(tr("%1, turned %2 degrees.").arg(DetailName(m_ToolArg)).arg(m_Turn * 45));
	return true;
}

bool AuOldShipPanel::ViewCancel()
{
	if(m_Tool != TOOL_Roof || !m_RoofClicks)
		return false;
	m_RoofClicks = 0;
	m_Status->setText(tr("%1: started over. Click its first point.").arg(RoomName(m_ToolArg)));
	return true;
}

void AuOldShipPanel::RoofClick(double gx, double gy)
{
	const QString red("<font color=\"#ff8080\">%1</font>");
	const bool round = m_ToolArg == 24;
	if(m_Deck < 0)
	{
		m_Status->setText(red.arg(tr("A roof cannot be below the ground.")));
		return;
	}
	OldShip::GridPoint point;
	if(m_RoofClicks == 0)
	{
		point.x = (int)floor(gx + 0.5);
		point.y = (int)floor(gy + 0.5);
		if(!m_Rules.ValidGrid(point.x, point.y, m_Deck))
		{
			m_Status->setText(red.arg(tr("That is beyond the edge of the building site.")));
			return;
		}
		m_RoofPoint[0] = point;
		m_RoofClicks = 1;
		m_Status->setText(round ? tr("Now the end of one radius: along the grid or a diagonal from the middle. Escape starts over.")
				: tr("Now the other end of that eave: along the grid or a diagonal from the first. Escape starts over."));
		return;
	}
	if(m_RoofClicks == 1)
	{
		if(!OldShip::RoofSecondPoint(m_RoofPoint[0], gx, gy, point))
			return;
		if(!m_Rules.ValidGrid(point.x, point.y, m_Deck))
		{
			m_Status->setText(red.arg(tr("That is beyond the edge of the building site.")));
			return;
		}
		m_RoofPoint[1] = point;
		m_RoofClicks = 2;
		m_Status->setText(round ? tr("Now how far the roof reaches the other way, square to that radius. Escape starts over.")
				: tr("Now how far the roof reaches from that eave, to either side of it. Escape starts over."));
		return;
	}
	if(!OldShip::RoofThirdPoint(m_RoofPoint[0], m_RoofPoint[1], gx, gy, point))
		return;
	const OldShip::Design before(m_Design);
	OldShip::Editor editor(m_Design, Rules());
	std::string reason;
	if(!editor.AddRoof(m_Deck, m_RoofPoint[0], m_RoofPoint[1], point, m_ToolArg, reason))
	{
		m_Status->setText(red.arg(QString::fromStdString(reason).toHtmlEscaped()));
		return;
	}
	m_RoofClicks = 0;
	const int lost = (int)before.details.size() - (int)m_Design.details.size();
	m_Status->setText(lost > 0 ? tr("%1 made. %2 pieces of equipment no longer had a valid place and were removed.").arg(RoomName(m_ToolArg)).arg(lost)
			: tr("%1 made. Erase Room erases it.").arg(RoomName(m_ToolArg)));
	DesignChanged();
}

bool AuOldShipPanel::Apply(double gx, double gy, QString& why)
{
	const int deck = m_Deck;
	const int gridLines = GridLines();

	const OldShip::Design before(m_Design);
	OldShip::Editor editor(m_Design, Rules());
	std::string reason;
	bool done = false;
	switch(m_Tool)
	{
	case TOOL_Room:
		if(m_RetypeMode)
			done = editor.ChangeRoomType(deck, gx, gy, m_ToolArg, reason);
		else
			done = editor.PaintCell(deck, OldShip::FillShapeAt(gx, gy, gridLines), m_ToolArg, m_RoomNumber, reason);
		break;
	case TOOL_EraseCells:
		done = editor.EraseCells(deck, OldShip::FillShapeAt(gx, gy, gridLines), reason);
		break;
	case TOOL_EraseRoom:
		done = editor.EraseRoom(deck, OldShip::FillShapeAt(gx, gy, gridLines), reason);
		break;
	case TOOL_RoomNumber:
		done = editor.ChangeRoomNumber(deck, gx, gy, m_RoomNumber, reason);
		break;
	case TOOL_Detail:
		done = editor.AddDetail(deck, gx, gy, DetailVariant(m_ToolArg, m_Turn), m_Brush, reason, m_ToolCommodity);
		break;
	case TOOL_EraseDetail:
		done = editor.EraseDetail(deck, gx, gy, 0, reason);
		break;
	case TOOL_EraseDetails:
		done = editor.EraseDetail(deck, gx, gy, m_Brush, reason);
		break;
	default:
		reason = "Pick a room or equipment button first.";
		break;
	}
	if(!done)
	{
		why = QString::fromStdString(reason);
		return false;
	}


	const int lost = (int)before.details.size() - (int)m_Design.details.size();
	if(m_Tool == TOOL_Detail && lost >= 0)
		m_Status->setText(tr("Placed. %1 pieces of equipment that stood there were replaced.").arg(lost + 1));
	else if(lost > 0 && m_Tool != TOOL_EraseDetail && m_Tool != TOOL_EraseDetails)
		m_Status->setText(tr("Done. %1 pieces of equipment no longer had a valid place and were removed.").arg(lost));
	else
		m_Status->setText(QString());
	DesignChanged();
	return true;
}

void AuOldShipPanel::ViewStroke(double gx, double gy, bool pressed)
{
	if(!pressed && (m_Tool == TOOL_RoomNumber || m_Tool == TOOL_EraseRoom || m_Tool == TOOL_None || m_Tool == TOOL_Roof
				|| (m_Tool == TOOL_Room && m_RetypeMode)))
		return;
	if(m_Tool == TOOL_Roof)
	{
		RoofClick(gx, gy);
		return;
	}
	QString why;
	if(!Apply(gx, gy, why) && pressed)
		m_Status->setText(QString("<font color=\"#ff8080\">%1</font>").arg(why.toHtmlEscaped()));
}

void AuOldShipPanel::ViewHover(double gx, double gy)
{
	OldShip::Editor editor(m_Design, Rules());
	const int room = editor.RoomAt(m_Deck, gx, gy);
	QString what = tr("%1, %2").arg(floor(gx + 0.5)).arg(floor(gy + 0.5));
	if(room >= 0)
		what += QString("  %1 %2").arg(RoomName(m_Design.rooms[room].type)).arg(m_Design.rooms[room].style + 1);
	const int x = (int)floor(gx + 0.5), y = (int)floor(gy + 0.5);
	for(size_t i = 0; i < m_Design.details.size(); i++)
		if(m_Design.details[i].deck == m_Deck && m_Design.details[i].x == x && m_Design.details[i].y == y)
			what += QString("  %1").arg(DetailName(m_Design.details[i].type));
	if(IsBuilding() && !m_Rules.ValidGrid(gx, gy, m_Deck))
		what += tr("  beyond the site");
	const QString text = tr("%1 %2  (%3)").arg(DeckWord(true)).arg(m_Deck).arg(what);
	if(m_DeckLabel->text() != text)
		m_DeckLabel->setText(text);
}

void AuOldShipPanel::GridToggled()
{
	const int gridLines = GridLines();
	const int families = (gridLines & 1) + ((gridLines >> 1) & 1) + ((gridLines >> 2) & 1) + ((gridLines >> 3) & 1);
	if(families < 2)
		m_Status->setText(tr("Enable at least two kinds of grid lines to make cells."));
}

void AuOldShipPanel::DeckAbove()
{
	if(m_Deck + 1 >= OldShip::DeckLimit)
		return;
	m_Deck++;
	m_RoofClicks = 0;
	UpdateControls();
}

void AuOldShipPanel::DeckBelow()
{
	if(m_Deck - 1 <= -OldShip::DeckLimit)
		return;
	m_Deck--;
	m_RoofClicks = 0;
	UpdateControls();
}

void AuOldShipPanel::EraseDeck()
{
	QMessageBox mb(g_Au.ApplicationName(), tr("All rooms and details on the current working deck will be erased."),
			QMessageBox::Warning, QMessageBox::Ok, QMessageBox::Cancel, QMessageBox::NoButton, auApp->Frame());
	if(auApp->Frame()->ExecMessageBox(mb) != QMessageBox::Ok)
		return;

	const OldShip::Design before(m_Design);
	OldShip::Editor editor(m_Design, Rules());
	std::string reason;
	if(!editor.EraseDeck(m_Deck, reason))
	{
		m_Status->setText(QString::fromStdString(reason));
		return;
	}
	m_Status->setText(tr("%1 %2 erased.").arg(DeckWord(true)).arg(m_Deck));
	DesignChanged();
}

void AuOldShipPanel::ArmorIncrease()
{
	const OldShip::Design before(m_Design);
	OldShip::Editor editor(m_Design, Rules());
	std::string reason;
	if(!editor.ArmorIncrease(reason))
	{
		m_Status->setText(QString::fromStdString(reason));
		return;
	}
	m_Status->setText(tr("Hull armor %1.").arg(m_Design.armor));
	DesignChanged();
}

void AuOldShipPanel::ArmorDecrease()
{
	const OldShip::Design before(m_Design);
	OldShip::Editor editor(m_Design, Rules());
	std::string reason;
	if(!editor.ArmorDecrease(reason))
	{
		m_Status->setText(QString::fromStdString(reason));
		return;
	}
	m_Status->setText(tr("Hull armor %1.").arg(m_Design.armor));
	DesignChanged();
}

void AuOldShipPanel::StreamlineClicked(bool on)
{
	OldShip::Editor editor(m_Design, Rules());
	editor.SetStreamline(on);
	DesignChanged();
}

void AuOldShipPanel::SystemModules()
{
	AuOldShipModulesDlg dlg(m_Design, auApp->Frame());
	if(auApp->Frame()->ExecDialog(dlg) != QDialog::Accepted)
		return;
	OldShip::Design changed(m_Design);
	if(!dlg.Apply(changed))
		return;
	m_Design = changed;
	m_Status->setText(tr("System modules set."));
	DesignChanged();
}

void AuOldShipPanel::MinQualityChanged(int quality)
{
	if(quality == m_Design.minQuality)
		return;
	OldShip::Editor editor(m_Design, Rules());
	editor.SetMinQuality(quality);
	DesignChanged();
}

void AuOldShipPanel::MoveCopy()
{
	AuOldShipMoveDlg dlg(m_Deck, auApp->Frame(), IsBuilding());
	if(auApp->Frame()->ExecDialog(dlg) != QDialog::Accepted)
		return;

	const OldShip::Design before(m_Design);
	OldShip::Editor editor(m_Design, Rules());
	std::string reason;
	QString done;
	if(dlg.What() == AuOldShipMoveDlg::CopyDeck)
	{
		const int copies = editor.Copy(m_Deck, dlg.X(), dlg.Y(), dlg.Decks(), dlg.Repeat(), reason);
		if(copies)
			done = copies == dlg.Repeat() ? tr("%1 %2 copied %3 times.").arg(DeckWord(true)).arg(m_Deck).arg(copies)
					: tr("%1 %2 copied %3 times; the next copy had no room.").arg(DeckWord(true)).arg(m_Deck).arg(copies);
	}
	else if(editor.Move(dlg.What() == AuOldShipMoveDlg::MoveDesign, m_Deck, dlg.X(), dlg.Y(), dlg.Decks(), reason))
	{
		done = dlg.What() == AuOldShipMoveDlg::MoveDesign ? tr("The design was moved.")
				: tr("%1 %2 was moved.").arg(DeckWord(true)).arg(m_Deck);
		m_Deck = qBound(-OldShip::DeckLimit + 1, m_Deck + dlg.Decks(), OldShip::DeckLimit - 1);
	}
	if(done.isEmpty())
	{
		m_Status->setText(QString("<font color=\"#ff8080\">%1</font>").arg(QString::fromStdString(reason).toHtmlEscaped()));
		return;
	}
	const int lost = (int)before.details.size() - (int)m_Design.details.size();
	if(dlg.What() != AuOldShipMoveDlg::CopyDeck && lost > 0)
		done += tr(" %1 pieces of equipment no longer had a valid place and were removed.").arg(lost);
	m_Status->setText(done);
	DesignChanged();
}

void AuOldShipPanel::RoomNumberUp()
{
	m_RoomNumber = (m_RoomNumber + 1) & 255;
	UpdateControls();
}

void AuOldShipPanel::RoomNumberDown()
{
	m_RoomNumber = (m_RoomNumber + 255) & 255;
	UpdateControls();
}

void AuOldShipPanel::BrushUp()
{
	if(m_Brush < OldShip::BrushMax)
		m_Brush++;
	UpdateControls();
}

void AuOldShipPanel::BrushDown()
{
	if(m_Brush > 0)
		m_Brush--;
	UpdateControls();
}

void AuOldShipPanel::SaveShip()
{
	Save(false);
}

bool AuOldShipPanel::Save(bool askName)
{
	const char* const lastFileKey = IsBuilding() ? BuildingFileKey : ShipFileKey;
	const QString ending = IsBuilding() ? QString::fromStdString(OldShip::BuildingFileExtension(m_Building.industry)) : QString("ship");
	QString saveFile(m_FileName);
	if(IsBuilding() && !saveFile.isEmpty() && IsBuildingFileName(saveFile) && !ending.isEmpty()
			&& QFileInfo(saveFile).suffix().compare(ending, Qt::CaseInsensitive) != 0)
		askName = true;
	if(askName || saveFile.isEmpty())
	{
		QString shipFileName(m_FileName);
		if(shipFileName.isEmpty())
		{
			shipFileName = g_Au.Settings().readEntry(lastFileKey, "");
			if(shipFileName.isEmpty())
				shipFileName = QDir::currentPath();
			else
				shipFileName = QFileInfo(shipFileName).absolutePath();
		}
		else if(IsBuilding() && !ending.isEmpty() && QFileInfo(shipFileName).suffix().compare(ending, Qt::CaseInsensitive) != 0)
			shipFileName = QFileInfo(shipFileName).absolutePath() + "/" + QFileInfo(shipFileName).completeBaseName() + "." + ending;
		saveFile = QFileDialog::getSaveFileName(this, IsBuilding() ? tr("Old Building Design File") : tr("Old Spacecraft Design File"),
				shipFileName, FileFilter(), 0,
				(g_Au.UseNativeFileDialog() ? QFileDialog::Options() : QFileDialog::Options(QFileDialog::DontUseNativeDialog))
					| QFileDialog::HideNameFilterDetails);
		if(saveFile.isEmpty())
			return false;
		if(QFileInfo(saveFile).suffix().isEmpty() && !ending.isEmpty())
			saveFile += "." + ending;
	}

	std::vector<uint8_t> data;
	if(IsBuilding())
		Building().Write(data);
	else
		m_Design.Write(data);
	QFile file(saveFile);
	bool written = file.open(QIODevice::WriteOnly);
	if(written)
	{
		written = data.empty() || file.write((const char*)&data[0], (qint64)data.size()) == (qint64)data.size();
		file.close();
	}
	if(!written)
	{
		QMessageBox mb(g_Au.ApplicationName(), tr("The old design file could not be written: %1")
					.arg(QDir::toNativeSeparators(saveFile)),
				QMessageBox::Warning, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton, auApp->Frame());
		auApp->Frame()->ExecMessageBox(mb);
		return false;
	}
	g_Au.Settings().writeEntry(lastFileKey, saveFile);
	m_FileName = saveFile;
	m_Modified = false;
	m_Status->setText(tr("Saved %1.").arg(QDir::toNativeSeparators(saveFile)));
	UpdateReport();
	UpdateTitle();
	return true;
}

AuOldShipDock::AuOldShipDock(QWidget* parent, AuOldShipPanel::KIND kind) :
	AuDockWidget(kind == AuOldShipPanel::KIND_Building ? ":data/City.png" : ":data/c_Spacecraft.png",
			kind == AuOldShipPanel::KIND_Building ? tr("<b>Old Building Design</b>") : tr("<b>Old Spacecraft Design</b>"),
			kind == AuOldShipPanel::KIND_Building ? tr("Old Building Design") : tr("Old Spacecraft Design"), parent)
{
	m_Panel = new AuOldShipPanel(kind, this);
	setWidget(m_Panel);

	int dockSides = g_Au.Settings().readNumEntry(m_Panel->IsBuilding() ? "Dock/DesignerOldBuildingAreas" : "Dock/DesignerOldShipAreas",
			(int)allowedAreas());
	setAllowedAreas((Qt::DockWidgetAreas)dockSides);
}

void AuOldShipDock::SetAllowedAreas(Qt::DockWidgetAreas areas)
{
	setAllowedAreas(areas);
	g_Au.Settings().writeEntry(m_Panel->IsBuilding() ? "Dock/DesignerOldBuildingAreas" : "Dock/DesignerOldShipAreas", (int)allowedAreas());
}

AuOldShipMoveDlg::AuOldShipMoveDlg(int deck, QWidget* parent, bool building) : AuDialog(parent)
{
	setAuDlgTitle(tr("Move/Copy Rooms And Equipment"));
	setAuDlgIcon(QIcon(":data/dp_Move.png"));

	m_MoveDesign = new QRadioButton(tr("Move the entire design"), this);
	m_MoveDeck = new QRadioButton(building ? tr("Move storey %1").arg(deck) : tr("Move deck %1").arg(deck), this);
	m_CopyDeck = new QRadioButton(building ? tr("Copy storey %1 to another storey").arg(deck) : tr("Copy deck %1 to another deck").arg(deck), this);
	m_MoveDesign->setChecked(true);

	m_X = new QSpinBox(this);
	m_X->setRange(-120, 120);
	m_X->setToolTip(building ? tr("Cells east. Negative: west.") : tr("Cells to starboard. Negative: to port."));
	m_Y = new QSpinBox(this);
	m_Y->setRange(-120, 120);
	m_Y->setToolTip(building ? tr("Cells north. Negative: south.") : tr("Cells forward. Negative: aft."));
	m_Decks = new QSpinBox(this);
	m_Decks->setRange(-84, 84);
	m_Decks->setToolTip(building ? tr("Storeys up. Negative: down. A storey can only go to a storey that is empty; a copy must go to another storey.")
			: tr("Decks up. Negative: down. A deck can only go to a deck that is empty; a copy must go to another deck."));
	m_Repeat = new QSpinBox(this);
	m_Repeat->setRange(1, 84);
	m_Repeat->setToolTip(tr("How many copies to make, each as far again from the one before."));
	m_Repeat->setEnabled(false);
	connect(m_CopyDeck, SIGNAL(toggled(bool)), m_Repeat, SLOT(setEnabled(bool)));

	QGridLayout* grid = new QGridLayout;
	grid->addWidget(m_MoveDesign, 0, 0, 1, 2);
	grid->addWidget(m_MoveDeck, 1, 0, 1, 2);
	grid->addWidget(m_CopyDeck, 2, 0, 1, 2);
	grid->addWidget(new QLabel(building ? tr("Cells east") : tr("Cells to starboard"), this), 3, 0);
	grid->addWidget(m_X, 3, 1);
	grid->addWidget(new QLabel(building ? tr("Cells north") : tr("Cells forward"), this), 4, 0);
	grid->addWidget(m_Y, 4, 1);
	grid->addWidget(new QLabel(building ? tr("Storeys up") : tr("Decks up"), this), 5, 0);
	grid->addWidget(m_Decks, 5, 1);
	grid->addWidget(new QLabel(tr("Copies"), this), 6, 0);
	grid->addWidget(m_Repeat, 6, 1);

	QPushButton* okButton = new QPushButton(tr("OK"), this);
	okButton->setDefault(true);
	connect(okButton, SIGNAL(clicked()), SLOT(accept()));
	QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);
	connect(cancelButton, SIGNAL(clicked()), SLOT(reject()));

	QHBoxLayout* buttons = new QHBoxLayout;
	buttons->addStretch(1);
	buttons->addWidget(okButton);
	buttons->addWidget(cancelButton);

	QVBoxLayout* lay = new QVBoxLayout;
	lay->setMargin(4);
	lay->setSpacing(4);
	lay->addLayout(grid);
	lay->addLayout(buttons);
	setAuDlgLayout(lay);
}

static QComboBox* OldShipModuleBox(SYSTEM system, int current, COMMODITYID plainest, QWidget* parent)
{
	QComboBox* box = new QComboBox(parent);
	for(int cid = 0; cid < CID_Count; cid++)
		if(DbCommodity::IsScModule(system, (COMMODITYID)cid))
			box->addItem(g_Au.CommodityName((COMMODITYID)cid), cid);
	int i = box->findData(current);
	if(i < 0)
		i = box->findData((int)plainest);
	box->setCurrentIndex(qMax(i, 0));
	return box;
}

static bool OldShipSetModuleFor(std::vector<std::pair<int, int> >& modules, int number, int cid)
{
	for(size_t i = 0; i < modules.size(); i++)
		if(modules[i].first == number)
		{
			if(modules[i].second == cid)
				return false;
			modules[i].second = cid;
			return true;
		}
	modules.push_back(std::make_pair(number, cid));
	return true;
}

static int OldShipModuleFor(const std::vector<std::pair<int, int> >& modules, int number)
{
	for(size_t i = 0; i < modules.size(); i++)
		if(modules[i].first == number)
			return modules[i].second;
	return 0;
}

AuOldShipModulesDlg::AuOldShipModulesDlg(const OldShip::Design& design, QWidget* parent) : AuDialog(parent)
{
	setAuDlgTitle(tr("System Modules"));
	setAuDlgIcon(QIcon(":data/UpgradeModule.png"));

	QLabel* intro = new QLabel(tr("Choose the module each system uses. All weapon bay rooms with the same room number are "
				"one weapon system, worked by a fire control station; a transporter room's number tells it from the others."), this);
	intro->setWordWrap(true);

	QGridLayout* grid = new QGridLayout;
	int row = 0;
	m_Ftl = new QComboBox(this);
	m_Ftl->addItem(tr("Spaceship (no FTL drive)"), (int)CID_Invalid);
	for(int cid = 0; cid < CID_Count; cid++)
		if(DbCommodity::IsScModule(SYS_FTLDrive, (COMMODITYID)cid))
			m_Ftl->addItem(tr("Starship: %1").arg(g_Au.CommodityName((COMMODITYID)cid)), cid);
	int ftl = m_Ftl->findData(design.modules[1]);
	if(ftl < 0)
		ftl = design.modules[1] ? qMin(1, m_Ftl->count() - 1) : 0;
	m_Ftl->setCurrentIndex(ftl);
	m_Ftl->setToolTip(tr("<html><b>FTL Drive</b><br>A Spaceship has no FTL drive. A Starship's FTL drive, of the kind "
				"chosen here, is put on its first engineer station, which it must have; the drive's hull volume comes from "
				"the engine rooms.</html>"));
	grid->addWidget(new QLabel(tr("FTL Drive"), this), row, 0);
	grid->addWidget(m_Ftl, row++, 1);
	m_Sensor = OldShipModuleBox(SYS_Sensor, design.modules[6], CID_SensorModule, this);
	grid->addWidget(new QLabel(tr("Sensors"), this), row, 0);
	grid->addWidget(m_Sensor, row++, 1);
	m_Shield = OldShipModuleBox(SYS_Shield, design.modules[7], CID_EnergyShieldModule, this);
	grid->addWidget(new QLabel(tr("Shields"), this), row, 0);
	grid->addWidget(m_Shield, row++, 1);

	QMap<int, bool> bays, transporters;
	for(size_t i = 0; i < design.rooms.size(); i++)
	{
		if(design.rooms[i].type == 21)
			bays.insert(design.rooms[i].style, true);
		else if(design.rooms[i].type == 20)
			transporters.insert(design.rooms[i].style, true);
	}
	for(QMap<int, bool>::const_iterator bi = bays.constBegin(); bi != bays.constEnd(); bi++)
	{
		QComboBox* box = OldShipModuleBox(SYS_Weapon, OldShipModuleFor(design.bayModules, bi.key()), CID_BallisticWeaponBayModule, this);
		m_Bays.insert(bi.key(), box);
		grid->addWidget(new QLabel(tr("Weapon Bay %1").arg(bi.key() + 1), this), row, 0);
		grid->addWidget(box, row++, 1);
	}
	if(bays.isEmpty())
		grid->addWidget(new QLabel(tr("No weapon bay rooms."), this), row++, 0, 1, 2);
	for(QMap<int, bool>::const_iterator ti = transporters.constBegin(); ti != transporters.constEnd(); ti++)
	{
		QComboBox* box = OldShipModuleBox(SYS_Transporter, OldShipModuleFor(design.transporterModules, ti.key()),
				CID_PersonnelTransporterModule, this);
		m_Transporters.insert(ti.key(), box);
		grid->addWidget(new QLabel(tr("Transporter Room %1").arg(ti.key() + 1), this), row, 0);
		grid->addWidget(box, row++, 1);
	}
	int units = 0;
	for(size_t i = 0; i < design.details.size(); i++)
	{
		if(design.details[i].type != 11)
			continue;
		QComboBox* box = OldShipModuleBox(SYS_SurgeryUnit, design.details[i].extra, CID_SurgeryMedicalModule, this);
		m_Surgery.insert((int)i, box);
		grid->addWidget(new QLabel(tr("Surgery Unit %1, Deck %2").arg(++units).arg(design.details[i].deck), this), row, 0);
		grid->addWidget(box, row++, 1);
	}

	QPushButton* okButton = new QPushButton(tr("OK"), this);
	okButton->setDefault(true);
	connect(okButton, SIGNAL(clicked()), SLOT(accept()));
	QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);
	connect(cancelButton, SIGNAL(clicked()), SLOT(reject()));

	QHBoxLayout* buttons = new QHBoxLayout;
	buttons->addStretch(1);
	buttons->addWidget(okButton);
	buttons->addWidget(cancelButton);

	QVBoxLayout* lay = new QVBoxLayout;
	lay->setMargin(4);
	lay->setSpacing(4);
	lay->addWidget(intro);
	lay->addLayout(grid);
	lay->addLayout(buttons);
	setAuDlgLayout(lay);
}

bool AuOldShipModulesDlg::Apply(OldShip::Design& design) const
{
	bool changed = false;
	const int ftl = m_Ftl->currentData().toInt();
	if(design.modules[1] != ftl)
	{
		design.modules[1] = ftl;
		changed = true;
	}
	const int sensor = m_Sensor->currentData().toInt();
	if(design.modules[6] != sensor)
	{
		design.modules[6] = sensor;
		changed = true;
	}
	const int shield = m_Shield->currentData().toInt();
	if(design.modules[7] != shield)
	{
		design.modules[7] = shield;
		changed = true;
	}
	for(QMap<int, QComboBox*>::const_iterator bi = m_Bays.constBegin(); bi != m_Bays.constEnd(); bi++)
		changed = OldShipSetModuleFor(design.bayModules, bi.key(), (*bi)->currentData().toInt()) || changed;
	for(QMap<int, QComboBox*>::const_iterator ti = m_Transporters.constBegin(); ti != m_Transporters.constEnd(); ti++)
		changed = OldShipSetModuleFor(design.transporterModules, ti.key(), (*ti)->currentData().toInt()) || changed;
	for(QMap<int, QComboBox*>::const_iterator si = m_Surgery.constBegin(); si != m_Surgery.constEnd(); si++)
	{
		const int cid = (*si)->currentData().toInt();
		if(si.key() < (int)design.details.size() && design.details[si.key()].extra != cid)
		{
			design.details[si.key()].extra = cid;
			changed = true;
		}
	}
	return changed;
}

AuOldShipMoveDlg::WHAT AuOldShipMoveDlg::What() const
{
	return m_CopyDeck->isChecked() ? CopyDeck : m_MoveDeck->isChecked() ? MoveDeck : MoveDesign;
}

int AuOldShipMoveDlg::X() const
{
	return m_X->value();
}

int AuOldShipMoveDlg::Y() const
{
	return m_Y->value();
}

int AuOldShipMoveDlg::Decks() const
{
	return m_Decks->value();
}

int AuOldShipMoveDlg::Repeat() const
{
	return m_Repeat->value();
}

void AuOldShipPanel::CheckPaths()
{
	if(m_Design.rooms.empty())
	{
		m_Status->setText(tr("The design has no rooms to check."));
		return;
	}
	OldShip::PathReportCounts counts;
	const QString html = RunPathCheck(counts);
	AuOldShipReportDlg dlg(tr("Check Paths Report"), html, auApp->Frame());
	auApp->Frame()->ExecDialog(dlg);
}

QString AuOldShipPanel::RunPathCheck(OldShip::PathReportCounts& counts)
{
	QProgressDialog progress(tr("Checking paths..."), tr("Cancel"), 0, 1, this);
	progress.setWindowTitle(tr("Check Paths Report"));
	progress.setWindowModality(Qt::WindowModal);
	progress.setMinimumDuration(500);
	const QString html = OldShip::PathReportHtml(m_Design, [&progress](const QString& label, int value, int maximum) -> bool
	{
		if(progress.labelText() != label)
			progress.setLabelText(label);
		if(progress.maximum() != maximum)
			progress.setMaximum(maximum);
		progress.setValue(value);
		QCoreApplication::processEvents();
		return !progress.wasCanceled();
	}, &counts);
	progress.close();
	const QString legend = tr("<span style=\"color: rgb(255,0,0);\">Path Warning</span> Identifies something that may prevent "
			"access to parts of the ship by players and AI.<br><span style=\"color: rgb(255,255,0);\">Security Warning</span> "
			"Identifies something that may be unsecure.<br><br>");
	return legend + html;
}

bool AuOldShipPanel::ConfirmPathsForFinalize()
{
	if(IsBuilding() || m_Design.rooms.empty())
		return true;
	OldShip::PathReportCounts counts;
	const QString html = RunPathCheck(counts);
	if(!counts.complete)
	{
		m_Status->setText(tr("Finalize canceled."));
		return false;
	}
	if(!counts.pathWarnings && !counts.securityWarnings)
		return true;
	QString found;
	if(counts.pathWarnings && counts.securityWarnings)
		found = tr("Check Paths found %1 path warnings and %2 security warnings.").arg(counts.pathWarnings).arg(counts.securityWarnings);
	else if(counts.pathWarnings)
		found = tr("Check Paths found %1 path warnings.").arg(counts.pathWarnings);
	else
		found = tr("Check Paths found %1 security warnings.").arg(counts.securityWarnings);
	QMessageBox mb(g_Au.ApplicationName(), tr("<html>%1<br><br>Path warnings can leave parts of the ship out of reach of players "
				"and AI.</html>").arg(found),
			QMessageBox::Warning, QMessageBox::NoButton, QMessageBox::NoButton, QMessageBox::NoButton, auApp->Frame());
	QAbstractButton* anyway = mb.addButton(tr("Finalize Anyway"), QMessageBox::AcceptRole);
	QAbstractButton* show = mb.addButton(tr("Show Report"), QMessageBox::ActionRole);
	mb.addButton(QMessageBox::Cancel);
	mb.setDefaultButton(qobject_cast<QPushButton*>(show));
	auApp->Frame()->ExecMessageBox(mb);
	if(mb.clickedButton() == anyway)
		return true;
	if(mb.clickedButton() == show)
	{
		AuOldShipReportDlg dlg(tr("Check Paths Report"), html, auApp->Frame());
		auApp->Frame()->ExecDialog(dlg);
	}
	m_Status->setText(tr("Finalize canceled."));
	return false;
}

AuOldShipReportDlg::AuOldShipReportDlg(const QString& title, const QString& html, QWidget* parent) : AuDialog(parent)
{
	setAuDlgTitle(title);
	setAuDlgIcon(QIcon(":data/dp_Check.png"));

	QTextBrowser* text = new QTextBrowser(this);
	text->setReadOnly(true);
	text->setHtml("<html>" + html + "</html>");
	text->setMinimumSize(560, 360);

	QPushButton* okButton = new QPushButton(tr("OK"), this);
	okButton->setDefault(true);
	connect(okButton, SIGNAL(clicked()), SLOT(accept()));

	QHBoxLayout* buttons = new QHBoxLayout;
	buttons->addStretch(1);
	buttons->addWidget(okButton);

	QVBoxLayout* lay = new QVBoxLayout;
	lay->setMargin(4);
	lay->setSpacing(4);
	lay->addWidget(text, 1);
	lay->addLayout(buttons);
	setAuDlgLayout(lay);
}

AuOldShipLogDlg::AuOldShipLogDlg(const QString& headline, const QStringList& log, QWidget* parent, bool building) : AuDialog(parent)
{
	setAuDlgTitle(building ? tr("Old Building Design") : tr("Old Spacecraft Design"));
	setAuDlgIcon(QIcon(building ? ":data/City.png" : ":data/c_Spacecraft.png"));

	QLabel* label = new QLabel(headline, this);
	label->setWordWrap(true);

	QTextBrowser* text = new QTextBrowser(this);
	text->setReadOnly(true);
	text->setHtml("<html>" + LogHtml(log) + "</html>");
	text->setMinimumSize(560, 320);

	QPushButton* okButton = new QPushButton(tr("OK"), this);
	okButton->setDefault(true);
	connect(okButton, SIGNAL(clicked()), SLOT(accept()));

	QHBoxLayout* buttons = new QHBoxLayout;
	buttons->addStretch(1);
	buttons->addWidget(okButton);

	QVBoxLayout* lay = new QVBoxLayout;
	lay->setMargin(4);
	lay->setSpacing(4);
	lay->addWidget(label);
	lay->addWidget(text, 1);
	lay->addLayout(buttons);
	setAuDlgLayout(lay);
}

QString AuOldShipLogDlg::LogHtml(const QStringList& log)
{
	QString html;
	for(int i = 0; i < log.size(); i++)
	{
		const QString line = log[i].toHtmlEscaped();
		if(AuOldShipPanel::IsLogLineNoteworthy(log[i]) || log[i].startsWith("NOT DONE"))
			html += QString("<font color=\"#e0c040\">%1</font><br>").arg(line);
		else
			html += line + "<br>";
	}
	return html;
}
