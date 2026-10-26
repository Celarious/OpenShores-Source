#include "aulegacyshipviewer.h"
#include "aulegacystudioprotocol.h"
#include "aulegacyship.h"
#include "aulegacyshipedit.h"
#include "aulegacyshipdetails.h"
#include "aulegacyshipequipment.h"
#include "aulegacyshipgeometry.h"
#include "aulegacyshipicons.h"
#include "aulegacyshippathreport.h"
#include "aulegacyshiprender.h"
#include "aulegacyshipsave.h"
#include "aulegacyshipstatsreport.h"
#include "aulegacyshipvehicles.h"
#include "auapp.h"
#include "audialog.h"
#include "audesignerframe.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QEventLoop>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMargins>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QSet>
#include <QSettings>
#include <QScrollBar>
#include <QScrollArea>
#include <QShowEvent>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStringList>
#include <QStyle>
#include <QTableWidget>
#include <QTextDocument>
#include <QTextBrowser>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>
#include <utility>

namespace
{
// DaDesignPlane::MoveUp/MoveDown reject destination decks >=42 / <=-42.
// Grid movement is independent of the narrower spacecraft room-paint limits.
constexpr int MinimumGridDeck = -41;
constexpr int MaximumGridDeck = 41;

struct RoomChoice
{
	int type;
	const char* name;
	const char* icon;
};

const RoomChoice RoomChoices[] = {
	{ 1, "Barracks", "dp_Barracks" }, { 2, "Bridge", "dp_Bridge" },
	{ 3, "Brig", "dp_Brig" }, { 4, "Officer Cabin", "dp_CaptCabin" },
	{ 5, "Hold", "dp_Cargo" }, { 6, "Hall", "dp_Corridor" },
	{ 7, "Crew Cabin", "dp_CrewCabin" }, { 8, "Engine Room", "dp_EngineRoom" },
	{ 9, "Fuel Cell", "dp_Fuel" }, { 10, "Galley", "dp_Galley" },
	{ 11, "Gravity Well", "dp_GravityWell" }, { 13, "Vehicle Bay", "dp_LandingBay" },
	{ 14, "Lounge", "dp_Lounge" }, { 15, "Open to Deck Below", "dp_OpenToBelow" },
	{ 16, "Sensor Room", "dp_SensorRoom" }, { 17, "Shield Room", "dp_ShieldRoom" },
	{ 18, "Sickbay", "dp_SickBay" }, { 19, "Stateroom", "dp_Stateroom" },
	{ 20, "Transporter Room", "dp_TransporterRoom" }, { 21, "Weapon Bay", "dp_WeaponBay" }
};

struct EquipmentChoice
{
	int type;
	quint16 commodity;
	const char* name;
	const char* icon;
	int row;
	int column;
};

// Original AuView equipment slots and AuTabDesign vehicle commodity IDs.
const EquipmentChoice EquipmentChoices[] = {
	{73, 0, "Captain's chair", "dp_CmdConsole", 0, 1},
	{12, 0, "Helm", "dp_PilotConsole", 0, 2},
	{6, 0, "Engineer station", "dp_EngineerConsole", 0, 3},
	{17, 0, "Shield station", "dp_ShieldConsole", 0, 4},
	{15, 0, "Sensor station", "dp_SensorConsole", 0, 5},
	{1, 0, "Navigator station", "dp_NavConsole", 0, 6},
	{7, 0, "Fire control station", "dp_FireControlConsole", 0, 7},
	{13, 0, "Power relay station", "dp_PowerConsole", 0, 8},
	{4, 0, "Medic station", "dp_SurgeryConsole", 0, 9},
	{19, 0, "Transporter station", "dp_TransporterConsole", 0, 10},
	{22, 0, "Top/bottom turret", "dp_TurretTopBottom", 0, 11},
	{21, 0, "Side turret", "dp_TurretSide", 0, 12},
	{10, 0, "Maneuver drive", "dp_ManeuverDrive", 1, 2},
	{14, 0, "Power plant", "dp_PowerPlant", 1, 3},
	{18, 0, "Shields", "dp_Shield", 1, 4},
	{16, 0, "Sensors", "dp_Sensor", 1, 5},
	{24, 0, "FTL drive", "dp_WormholeDrive", 1, 6},
	{23, 0, "Weapon system", "dp_Weapon", 1, 7},
	{32, 0, "Life support", "dp_LifeSupport", 1, 8},
	{11, 0, "Surgery unit", "dp_OperatingTable", 1, 9},
	{20, 0, "Transporter pad", "dp_TransporterPad", 1, 10},
	{65, 0, "Single berth", "dp_Berth0", 1, 11},
	{2, 0, "Double berth", "dp_Berth2", 1, 12},
	{50, 0, "Hull door", "dp_HullDoor", 2, 1},
	{49, 0, "Crew door", "dp_CrewDoor", 2, 2},
	{5, 0, "Door", "dp_Door", 2, 3},
	{3, 0, "Large hull door", "dp_CargoDoor", 2, 4},
	{54, 0, "Large crew door", "dp_LargeCrewDoor", 2, 5},
	{53, 0, "Large door", "dp_LargeDoor", 2, 6},
	{52, 0, "Hull hatch", "dp_HullHatch", 2, 7},
	{51, 0, "Crew hatch", "dp_CrewHatch", 2, 8},
	{8, 0, "Hatch", "dp_Hatch", 2, 9},
	{55, 0, "Wall opening", "dp_WallOpening", 2, 10},
	{9, 0, "Hull opening", "dp_HullOpening", 2, 11},
	{56, 0, "Railing", "dp_Railing", 2, 12},
	{141, 0, "Troop station", "Troop", 3, 6},
	{101, 0, "Straight stairs", "dp_StairsStraight", 3, 8},
	{109, 0, "Stairs turning left", "dp_StairsTurnLeft", 3, 9},
	{117, 0, "Stairs turning right", "dp_StairsTurnRight", 3, 10},
	{125, 0, "Stairs U-turn left", "dp_StairsUTurnLeft", 3, 11},
	{133, 0, "Stairs U-turn right", "dp_StairsUTurnRight", 3, 12},
	{76, 0, "Pivot point", "dp_PivotPoint", 4, 0},
	{98, 0, "Contrail emitter", "dp_ContrailEmitter", 4, 1},
	{99, 0, "Weapon emitter", "dp_WeaponEmitter", 4, 2},
	{74, 0, "Game table", "dp_GameTable", 4, 4},
	{57, 9, "Space transport", "c_CommercialAircraft", 4, 6},
	{57, 8, "Space fighter F1", "c_SpaceFighter", 4, 7},
	{57, 231, "Space fighter F2", "c_SpaceFighter2", 4, 8},
	{57, 104, "APC", "c_APC", 4, 10},
	{57, 133, "Motorbike", "c_MotorBike", 4, 11},
	{57, 82, "SUV", "c_Suv", 4, 12}
};

bool EquipmentUsesBrush(int type)
{
	return type == 10 || type == 14 || type == 16 || type == 18
		|| type == 23 || type == 24 || type == 32;
}

bool EquipmentCanRotate(int type)
{
	const int base = AuLegacyShip::CanonicalDetailType(type);
	return base == 2 || base == 8 || base == 51 || base == 52 || base == 57
		|| base == 65 || base == 74 || (base >= 101 && base <= 133);
}

// Borders follow grid cells so resizing/docking preserves the original layout.
class PaletteGroups : public QWidget
{
public:
	explicit PaletteGroups(QWidget* parent) : QWidget(parent) {}
	QVector<QRect> groups;

protected:
	void paintEvent(QPaintEvent* event) override
	{
		QWidget::paintEvent(event);
		const QGridLayout* grid = static_cast<QGridLayout*>(layout());
		if (!grid) return;
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(QPen(QColor(255, 0, 0, 210), 1.0));
		painter.setBrush(Qt::NoBrush);
		for (const QRect& group : groups)
		{
			const QRectF bounds = grid->cellRect(group.top(), group.left())
				.united(grid->cellRect(group.bottom(), group.right()));
			painter.drawRoundedRect(bounds.adjusted(-1.0, -1.0, 1.0, 1.0), 4.0, 4.0);
		}
	}
};

QWidget* DesignerDockTitle(QDockWidget* dock, const QIcon& icon)
{
	QWidget* title = new QWidget(dock);
	title->setObjectName(QStringLiteral("LegacyDockTitle"));
	QHBoxLayout* layout = new QHBoxLayout(title);
	layout->setContentsMargins(4, 2, 2, 2);
	layout->setSpacing(4);
	QLabel* image = new QLabel(title);
	image->setPixmap(icon.pixmap(16, 16));
	image->setFixedSize(16, 16);
	image->setAttribute(Qt::WA_TransparentForMouseEvents);
	layout->addWidget(image);
	QLabel* label = new QLabel(dock->windowTitle(), title);
	label->setAttribute(Qt::WA_TransparentForMouseEvents);
	layout->addWidget(label, 1);
	QObject::connect(dock, &QWidget::windowTitleChanged, label, &QLabel::setText);
	const auto button = [&](QStyle::StandardPixmap symbol, const QString& text)
	{
		QToolButton* control = new QToolButton(title);
		control->setIcon(dock->style()->standardIcon(symbol));
		control->setToolTip(text);
		control->setAccessibleName(text);
		control->setAutoRaise(true);
		control->setFocusPolicy(Qt::NoFocus);
		control->setFixedSize(18, 18);
		layout->addWidget(control);
		return control;
	};
	QToolButton* floating = button(QStyle::SP_TitleBarNormalButton, QStringLiteral("Dock/float tools"));
	QObject::connect(floating, &QToolButton::clicked, dock, [dock]() { dock->setFloating(!dock->isFloating()); });
	QToolButton* close = button(QStyle::SP_TitleBarCloseButton, QStringLiteral("Hide tools"));
	QObject::connect(close, &QToolButton::clicked, dock, &QWidget::close);
	// Unhandled title-bar mouse events propagate to QDockWidget for dragging
	// and double-click docking; only the two controls consume their clicks.
	return title;
}

QString RoomName(int type)
{
	for (const RoomChoice& choice : RoomChoices)
		if (choice.type == type)
			return QString::fromLatin1(choice.name);
	// Original GD names for records outside the spacecraft paint palette.
	switch (type)
	{
	case 0: return QStringLiteral("None");
	case 12: return QString();
	case 22: return QStringLiteral("Roof, Gable");
	case 23: return QStringLiteral("Roof, Hip");
	case 24: return QStringLiteral("Roof, Round");
	}
	return QStringLiteral("Type %1").arg(type);
}

bool ValidLegacyLibraryName(const QString& name)
{
	// AuDesignSave and its IsTextValid helper in the 2016 executable.
	if(name.isEmpty() || name.size() > 30 || name.simplified() != name) return false;
	const QString reserved = QStringLiteral("/:!|<>&\\");
	for(const auto character : name)
		if(!character.isPrint() || reserved.contains(character)) return false;
	return true;
}

}

class AuLegacyShipViewer::Impl
{
public:
	enum ToolMode { HandMode, PaintMode, EraseMode, DeleteRoomMode, EquipmentMode, DeleteEquipmentMode, DeleteEquipmentAreaMode };

	explicit Impl(AuLegacyShipViewer* owner) : window(owner),
		settings(QSettings::NativeFormat, QSettings::UserScope,
			QStringLiteral("Hazeron Freehold"), QStringLiteral("Shores of Hazeron"))
	{
		renderView = new AuLegacyShipRender(window);

		newAction = MakeAction("New spacecraft", "dp_New", "Start a new spacecraft (Ctrl+N).");
		newAction->setShortcut(QKeySequence::New);
		QObject::connect(newAction, &QAction::triggered, window, &AuLegacyShipViewer::NewDesign);
		openAction = MakeAction("Open .ship...", "dp_Load", "Open a spacecraft design (Ctrl+O).");
		openAction->setShortcut(QKeySequence::Open);
		QObject::connect(openAction, &QAction::triggered, window, &AuLegacyShipViewer::ChooseFile);
		saveAction = MakeAction("Save spacecraft", "dp_Save", "Save spacecraft to a .ship file (Ctrl+S).");
		saveAction->setShortcut(QKeySequence::Save);
		QObject::connect(saveAction, &QAction::triggered, window, [this]() { SaveDesign(); });
		passwordAction = MakeAction("Studio Password...", "dp_Password", "Change the password used by other people to join this studio.");
		passwordAction->setToolTip(QStringLiteral("Change the password used by other people to join this studio."));
		passwordAction->setVisible(false);
		passwordAction->setEnabled(false);
		QObject::connect(passwordAction, &QAction::triggered, window, [this]() { ChangeStudioPassword(); });
		burnAction = MakeAction("Burn CD", "dp_BurnCD",
			"<b>Burn CD</b><br>Store the spacecraft design onto a disk. You must have a blank data disk in your inventory.");
		burnAction->setEnabled(false);
		QObject::connect(burnAction, &QAction::triggered, window, [this]() { BurnStudioMedia(); });
		QAction* fitAction = MakeAction("Show design plane", "dp_ShowDesignPlane", "Move the scene camera to view the current design plane (F).");
		fitAction->setShortcut(QKeySequence(QStringLiteral("F")));
		QObject::connect(fitAction, &QAction::triggered, window, [this]() { FitDeck(); });
		renderAction = MakeAction("Render Mode", "dp_RenderMode",
			"<b>Render Mode</b><br>Switch view between line drawing or solid rendering.");
		renderAction->setCheckable(true);
		QObject::connect(renderAction, &QAction::toggled, window, [this, fitAction](bool solid) {
			fitAction->setEnabled(!solid);
			SetRenderMode(solid);
		});
		gravityAction = MakeAction("Gravity", "dp_Gravity",
			"Toggle gravity on the design plane outside the spacecraft. Ship interiors retain their own gravity; gravity wells and ladders allow vertical travel.");
		gravityAction->setCheckable(true);
		QObject::connect(gravityAction, &QAction::toggled, window, [this](bool enabled) {
			renderView->SetGravity(enabled);
			ShowRenderHelp();
		});
		checkPathsAction = MakeAction("Check paths", "dp_Check",
			"Check paths from berths to stations, turrets and the outside.");
		checkPathsAction->setEnabled(false);
		QObject::connect(checkPathsAction, &QAction::triggered, window, [this]() { CheckPaths(); });
		gridAction = MakeAction("Grid", "dp_ShowDesignPlane", "Show the selected grid line families.");
		gridAction->setCheckable(true);
		gridAction->setChecked(true);
		QObject::connect(gridAction, &QAction::toggled, window, [this](bool visible) {
			renderView->SetGrid(visible, gridMask);
		});

		tools = new QActionGroup(window);
		tools->setExclusive(true);
		eraseAction = MakeAction("Erase room cells", "dp_DelRoomCells", "Erase cells from the current deck.");
		eraseAction->setCheckable(true);
		tools->addAction(eraseAction);
		QObject::connect(eraseAction, &QAction::triggered, window, [this]() { SetToolMode(EraseMode); });
		deleteRoomAction = MakeAction("Erase room", "dp_DelRoom", "Erase the whole room under the cursor on the current deck.");
		deleteRoomAction->setCheckable(true);
		tools->addAction(deleteRoomAction);
		QObject::connect(deleteRoomAction, &QAction::triggered, window, [this]() { SetToolMode(DeleteRoomMode); });
		deleteDeckAction = MakeAction("Erase current deck", "dp_DelDeck", "Erase all rooms and equipment on the current deck.");
		QObject::connect(deleteDeckAction, &QAction::triggered, window, [this]() { DeleteCurrentDeck(); });
		for (const RoomChoice& choice : RoomChoices)
		{
			QAction* action = MakeAction(choice.name, choice.icon,
				qPrintable(QStringLiteral("Paint %1 cells on the current deck. Room Number distinguishes rooms of the same type.").arg(QString::fromLatin1(choice.name))));
			action->setCheckable(true);
			tools->addAction(action);
			roomActions.insert(choice.type, action);
			const int type = choice.type;
			QObject::connect(action, &QAction::triggered, window, [this, type]() {
				selectedRoomType = type;
				SetToolMode(PaintMode);
			});
		}

		roomNumber = new QSpinBox(window);
		roomNumber->setRange(1, 256);
		roomNumber->setWrapping(true);
		roomNumber->setButtonSymbols(QAbstractSpinBox::NoButtons);
		roomNumber->setAlignment(Qt::AlignCenter);
		roomNumber->setAccessibleName(QStringLiteral("Room number"));
		roomNumber->setToolTip(QStringLiteral("Room Number: distinguishes rooms of the same type on the same deck. Values wrap from 256 to 1."));
		deckUpAction = MakeAction("Raise design grid", "dp_DeckAbove", "Move the design grid up one deck.");
		deckDownAction = MakeAction("Lower design grid", "dp_DeckBelow", "Move the design grid down one deck.");
		QObject::connect(deckUpAction, &QAction::triggered, window, [this]() { ChangeDeck(1); });
		QObject::connect(deckDownAction, &QAction::triggered, window, [this]() { ChangeDeck(-1); });
		const char* lineIcons[] = { "dp_Grid0", "dp_Grid45", "dp_Grid90", "dp_Grid135" };
		const char* lineTips[] = { "Lateral grid lines (on the beam).", "Diagonal grid lines (to the port quarter).",
			"Longitudinal grid lines (fore and aft).", "Diagonal grid lines (to the starboard quarter)." };
		for (int i = 0; i < 4; ++i)
		{
			lineActions[i] = MakeAction(lineTips[i], lineIcons[i], lineTips[i]);
			lineActions[i]->setCheckable(true);
			lineActions[i]->setChecked((5 & (1 << i)) != 0);
			QObject::connect(lineActions[i], &QAction::toggled, window, [this](bool) { UpdateGrid(); });
		}

		closeAction = new QAction(QStringLiteral("Exit Designer"), window);
		closeAction->setToolTip(QStringLiteral("Leave the designer, with a prompt to save any changes."));
		closeAction->setShortcut(QKeySequence::Close);
		closeAction->setAutoRepeat(false);
		QAction* backAction = new QAction(QStringLiteral("Back"), window);
		backAction->setShortcut(QKeySequence(Qt::Key_Escape));
		backAction->setAutoRepeat(false);
		window->addAction(backAction);
		QObject::connect(backAction, &QAction::triggered, window, [this]() {
			SetToolMode(HandMode);
			renderView->UseHandCursor();
			renderView->setFocus();
		});
		QAction* interfaceAction = new QAction(QStringLiteral("Show or hide interface"), window);
		interfaceAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Escape));
		interfaceAction->setAutoRepeat(false);
		window->addAction(interfaceAction);
		QObject::connect(interfaceAction, &QAction::triggered, window, [this]() {
			interfaceVisible = !interfaceVisible;
			if(!interfaceVisible) toolsWereVisible = toolDock->isVisible();
			renderView->SetInterfaceVisible(interfaceVisible);
			toolDock->setVisible(interfaceVisible && toolsWereVisible);
			if(!interfaceVisible) renderView->setFocus();
		});
		for (QAction* action : { newAction, openAction, saveAction, fitAction, closeAction })
			window->addAction(action);
		QObject::connect(closeAction, &QAction::triggered, window, [this]() {
			if (exitHandler)
			{
				const std::function<void()> handler = exitHandler;
				QTimer::singleShot(0, window, [handler]() { handler(); });
			}
			else
				window->close();
		});

		QWidget* central = new QWidget(window);
		QVBoxLayout* layout = new QVBoxLayout(central);
		layout->setContentsMargins(6, 4, 6, 4);
		layout->addWidget(renderView, 1);
		window->setCentralWidget(central);

		toolDock = new QDockWidget(QStringLiteral("Spacecraft Design (F6)"), window);
		toolDock->setObjectName(QStringLiteral("LegacySpacecraftTools"));
		const QIcon designerIcon = AuLegacyShipIcon(QStringLiteral("c_DesignDisk"));
		toolDock->setWindowIcon(designerIcon);
		toolDock->setTitleBarWidget(DesignerDockTitle(toolDock, designerIcon));
		toolDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
		toolDock->setStyleSheet(QStringLiteral(
			"QDockWidget { color: #d5e3fa; background: #000000; font-size: 12px; }"
			"QDockWidget::title { background: #07152d; border: 1px solid #315986; padding: 4px; }"
			"QWidget#LegacyDockTitle { background: #07152d; border: 1px solid #315986; }"
			"QWidget#LegacyPalette { background: #000000; color: #d5e3fa; }"
			"QToolButton { background: transparent; color: #d5e3fa; border: 1px solid transparent; border-radius: 2px; padding: 0; }"
			"QToolButton:hover { border-color: #668fc8; background: #10233e; }"
			"QToolButton:checked { border-color: #8da7d0; background: #1e3459; }"
			"QToolButton:pressed { background: #294c7b; }"
			"QToolButton:disabled { color: #606979; }"
			"QSpinBox, QComboBox { background: #102442; color: #d5e3fa; border: 1px solid #40618d; padding: 2px; }"
			"QComboBox QAbstractItemView { background: #0a1c3b; color: #d5e3fa; selection-background-color: #294c7b; }"
			"QLabel, QCheckBox { color: #d5e3fa; background: transparent; }"
			"QTextBrowser { background: #081a40; color: #b8cef5; border: 0; padding: 0; font-size: 11px; }"));
		QWidget* panel = new QWidget(toolDock);
		panel->setObjectName(QStringLiteral("LegacyPalette"));
		QVBoxLayout* panelLayout = new QVBoxLayout(panel);
		panelLayout->setContentsMargins(4, 4, 4, 4);
		panelLayout->setSpacing(4);
		editPanel = new QWidget(panel);
		editPanel->setObjectName(QStringLiteral("LegacyPalette"));
		BuildPalette(editPanel, newAction, openAction, saveAction);
		panelLayout->addWidget(editPanel);
		statsBrowser = new QTextBrowser(panel);
		statsBrowser->document()->setDocumentMargin(4);
		statsBrowser->setObjectName(QStringLiteral("LegacyShipStats"));
		statsBrowser->setAccessibleName(QStringLiteral("Spacecraft design statistics"));
		statsBrowser->setOpenLinks(false);
		statsBrowser->setOpenExternalLinks(false);
		statsTimer = new QTimer(window);
		statsTimer->setSingleShot(true);
		statsTimer->setInterval(100);
		QObject::connect(statsTimer, &QTimer::timeout, window, [this]() { RefreshStats(); });
		renderView->tipsChanged = [this](const QString& text) {
			if(!renderAction->isChecked()) return;
			if(text.isEmpty()) ShowRenderHelp();
			else SetFeedback(text);
		};
		designTips = QStringLiteral(
			"<b>NEW SPACECRAFT DESIGN TIPS</b><br>"
			"- All the buttons on this window have tooltips.<br>"
			"- Start by painting the floor of a room, such as the Bridge.<br>"
			"- Move the design grid up and down using the deck buttons.<br>"
			"- Combine the four grid directions to change the cell shape.<br>"
			"- Room Number distinguishes rooms of the same type on the same deck. Change it before painting a separate room.<br>"
			"- Left click or drag to paint. Select Erase Room Cells to erase.<br>"
			"- Erase Room removes the whole room under the cursor; Erase Current Deck removes all rooms and equipment on the selected deck.<br>"
			"- Select equipment, then left click or drag to place it. Its tooltip lists the room restrictions.<br>"
			"- Left/right arrow keys rotate berths, vehicles, hatches and stairs before placement.<br>"
			"- The brush arrows resize machinery placement and Erase Equipment Area.<br>"
			"- Fly around the design using your Avatar and Scene control bindings. Use the Scene camera bindings for overhead and other views.<br>"
			"- F frames the active design plane; moving the grid between decks preserves your player position.<br>"
			"- Render Mode switches between 3D wireframe and solid rendering at the same player position.<br>"
			"- W/A/S/D move, the mouse looks around, and Shift runs. Q toggles continuous forward movement.<br>"
			"- Return or right-click switches between avatar input and the free hand cursor. Hold Ctrl to use the hand temporarily. Use the hand to edit the active design plane in wireframe mode.<br>"
			"- C/Z move up/down when flying, in a gravity well, or on a ladder. Space jumps or brakes in zero gravity.<br>"
			"- E enters/exits a console or engages/releases a nearby ladder.<br>"
			"- On ladders, W climbs toward your look direction (up/down); S reverses it. C/Z climb directly. Shift does not increase climb speed.<br>"
			"- Hold Caps Lock to crouch; X toggles prone/standing. These use your Avatar bindings.<br>"
			"- Escape clears the selected tool and returns to the hand cursor. Shift+Escape hides or shows the interface.<br>"
			"- Use the hand to click a nearby door or hatch. Ctrl+P opens the nearest closed door; Ctrl+C closes the nearest open door.<br>"
			"- Regular and crew doors open on approach. Hull and large doors require manual operation.<br>"
			"- Gravity toggles the studio floor outside the ship. Ship interiors have their own gravity.<br>"
			"- Home returns to a clear location on the selected deck if you get lost.<br><br>"
			"<b>CURRENT EDITOR</b><br>"
			"Rooms and equipment can be edited on the active plane in the 3D wireframe. Other decks remain visible and dimmed. Unavailable tools remain disabled.<br><br>"
			"<b>WHEN FINISHED</b><br>"
			"Use Exit Designer or Ctrl+W to exit.<br>"
			"Use Save or Ctrl+S to save your work. New, Open and Exit offer to save changes.<br>"
			"The sidebar designer button or your Design Parts window binding hides or shows this panel. "
			"Drag its title to dock it elsewhere or float it beside the 3D view.");
		statsBrowser->setMinimumHeight(130);
		panelLayout->addWidget(statsBrowser, 1);
		QScrollArea* panelScroll = new QScrollArea(toolDock);
		panelScroll->setFrameShape(QFrame::NoFrame);
		panelScroll->setWidgetResizable(true);
		panelScroll->setWidget(panel);
		toolDock->setWidget(panelScroll);
		toolDock->setMinimumWidth(380);
		window->addDockWidget(Qt::RightDockWidgetArea, toolDock);
		window->resizeDocks(QList<QDockWidget*>() << toolDock, QList<int>() << 380, Qt::Horizontal);
		showTools = toolDock->toggleViewAction();
		showTools->setText(QStringLiteral("Spacecraft Design tools"));
		showTools->setIcon(designerIcon);
		toolDock->addAction(showTools);
		toolDock->addAction(closeAction);
		toolDock->addAction(passwordAction);
		toolDock->setContextMenuPolicy(Qt::ActionsContextMenu);
		toolDock->addAction(backAction);
		toolDock->addAction(interfaceAction);
		window->addAction(showTools);
		RefreshControlLabels();

		renderView->cellEntered = [this](const QPointF& point) { EditAt(point); };
		renderView->rotateEquipment = [this](int steps) {
			if (toolMode != EquipmentMode || !EquipmentCanRotate(selectedEquipmentType)) return;
			selectedEquipmentType = AuLegacyShip::RotateDetailType(selectedEquipmentType, steps);
			SetToolMode(EquipmentMode);
		};
		renderView->SetGrid(gridAction->isChecked(), gridMask);
	}

	~Impl()
	{
		statsTimer->stop();
		QObject::disconnect(statsTimer, nullptr, window, nullptr);
		renderView->tipsChanged = {};
		renderView->cellEntered = std::function<void(const QPointF&)>();
		renderView->rotateEquipment = std::function<void(int)>();
	}

	void RefreshControlLabels()
	{
		AuDesignerFrame* frame = qobject_cast<AuDesignerFrame*>(auApp->Frame());
		if(!frame) return;
		const QString binding = frame->ControlBinding(AuFrame::CI_ToggleDesignPartsUI);
		toolDock->setWindowTitle(QStringLiteral("Spacecraft Design")
			+ (binding.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(binding)));
	}

	QAction* MakeAction(const char* text, const char* icon, const char* tip)
	{
		QAction* action = new QAction(AuLegacyShipIcon(QString::fromLatin1(icon)), QString::fromLatin1(text), window);
		action->setToolTip(QString::fromLatin1(tip));
		return action;
	}

	QAction* UnavailableAction(const char* text, const char* icon, const char* reason)
	{
		QAction* action = MakeAction(text, icon, reason);
		action->setToolTip(QStringLiteral("%1\n%2").arg(QString::fromLatin1(text), QString::fromLatin1(reason)));
		action->setEnabled(false);
		return action;
	}

	QToolButton* AddButton(QGridLayout* layout, QAction* action, int row, int column)
	{
		QToolButton* button = new QToolButton(layout->parentWidget());
		button->setDefaultAction(action);
		button->setAccessibleName(action->text());
		button->setIconSize(QSize(16, 16));
		button->setFixedSize(22, 22);
		button->setToolButtonStyle(Qt::ToolButtonIconOnly);
		layout->addWidget(button, row, column, Qt::AlignCenter);
		return button;
	}

	void BuildPalette(QWidget* parent, QAction* newAction, QAction* openAction, QAction* saveAction)
	{
		QGridLayout* outer = new QGridLayout(parent);
		outer->setContentsMargins(0, 0, 0, 0);
		outer->setSpacing(2);
		AddButton(outer, newAction, 0, 1);
		AddButton(outer, openAction, 0, 2);
		AddButton(outer, saveAction, 0, 3);
		passwordButton = AddButton(outer, passwordAction, 0, 4);
		passwordButton->hide();
		AddButton(outer, renderAction, 1, 0);
		AddButton(outer, gravityAction, 1, 1);
		AddButton(outer, checkPathsAction, 1, 2);
		burnButton = AddButton(outer, burnAction, 1, 3);
		burnButton->hide();
		AddButton(outer, deckUpAction, 2, 0);
		AddButton(outer, lineActions[2], 2, 1);
		AddButton(outer, lineActions[0], 2, 2);
		AddButton(outer, deleteDeckAction, 2, 4);
		AddButton(outer, deckDownAction, 3, 0);
		AddButton(outer, lineActions[1], 3, 1);
		AddButton(outer, lineActions[3], 3, 2);
		moveAction = MakeAction("Move/Copy Rooms And Equipment", "dp_Move",
			"<b>Move/Copy Rooms And Equipment</b><br>Moves rooms and equipment of the entire design or the current deck.<br>Copies rooms and equipment of the current deck.");
		QObject::connect(moveAction, &QAction::triggered, window, [this]() { MoveCopyDesign(); });
		AddButton(outer, moveAction, 3, 4);

		PaletteGroups* roomPalette = new PaletteGroups(parent);
		roomPalette->groups << QRect(0, 0, 8, 3) << QRect(9, 0, 1, 3);
		QGridLayout* rooms = new QGridLayout(roomPalette);
		rooms->setContentsMargins(2, 2, 2, 2);
		rooms->setSpacing(2);
		rooms->setColumnMinimumWidth(8, 6);
		rooms->setColumnStretch(8, 1);
		QAction* drawMode = MakeAction("Room mode: draw", "dp_RoomMode0", "Draw room cells using the selected room type. Changing a whole room's type is not available yet.");
		QObject::connect(drawMode, &QAction::triggered, window, [this]() { SetToolMode(PaintMode); });
		AddButton(rooms, drawMode, 0, 0);
		AddButton(rooms, eraseAction, 1, 0);
		AddButton(rooms, deleteRoomAction, 2, 0);
		const int roomTypes[3][7] = { {1, 2, 3, 7, 8, 13, 9}, {10, 11, 6, 5, 4, 15, 14}, {16, 17, 18, 19, 20, 21, 0} };
		for (int row = 0; row < 3; ++row)
			for (int column = 0; column < 7; ++column)
				if (roomTypes[row][column])
					AddButton(rooms, roomActions.value(roomTypes[row][column]), row, column + 1);
		QAction* roomUp = MakeAction("Increase room number", "dp_RoomIdUp", "Increase Room Number; wraps after 256.");
		QAction* roomDown = MakeAction("Decrease room number", "dp_RoomIdDown", "Decrease Room Number; wraps before 1.");
		QObject::connect(roomUp, &QAction::triggered, roomNumber, &QSpinBox::stepUp);
		QObject::connect(roomDown, &QAction::triggered, roomNumber, &QSpinBox::stepDown);
		AddButton(rooms, roomUp, 0, 9);
		rooms->addWidget(roomNumber, 1, 9);
		AddButton(rooms, roomDown, 2, 9);
		outer->addWidget(roomPalette, 0, 6, 4, 8);

		PaletteGroups* equipmentPalette = new PaletteGroups(parent);
		equipmentPalette->groups << QRect(0, 0, 1, 2) << QRect(1, 0, 12, 2)
			<< QRect(1, 2, 12, 1) << QRect(13, 0, 2, 3) << QRect(8, 3, 5, 1)
			<< QRect(0, 4, 3, 1) << QRect(6, 4, 7, 1);
		QGridLayout* equipment = new QGridLayout(equipmentPalette);
		equipment->setContentsMargins(2, 6, 2, 3);
		equipment->setSpacing(3);
		deleteEquipmentAction = MakeAction("Erase equipment", "dp_DelDetail", "Remove equipment at the cursor. Left click or drag.");
		deleteEquipmentAreaAction = MakeAction("Erase equipment area", "dp_DelDetails", "Remove equipment at the cursor, then equipment of that same type within the brush.");
		for (QAction* action : { deleteEquipmentAction, deleteEquipmentAreaAction })
		{
			action->setCheckable(true);
			tools->addAction(action);
		}
		QObject::connect(deleteEquipmentAction, &QAction::triggered, window, [this]() { SetToolMode(DeleteEquipmentMode); });
		QObject::connect(deleteEquipmentAreaAction, &QAction::triggered, window, [this]() { SetToolMode(DeleteEquipmentAreaMode); });
		AddButton(equipment, deleteEquipmentAction, 0, 0);
		AddButton(equipment, deleteEquipmentAreaAction, 1, 0);
		for (const EquipmentChoice& choice : EquipmentChoices)
		{
			QAction* action = MakeAction(choice.name, choice.icon, choice.name);
			const bool supported = choice.commodity ? AuLegacyShip::Rendering::VehicleSupported(choice.commodity)
				: AuLegacyShip::Rendering::EquipmentSupported(choice.type)
					|| AuLegacyShip::Rendering::ResourceEquipmentSupported(choice.type);
			action->setEnabled(supported);
			action->setCheckable(true);
			tools->addAction(action);
			QString tip = QString::fromLatin1(choice.name) + QLatin1Char('\n')
				+ AuLegacyShip::DetailPlacementDescription(choice.type);
			if (EquipmentCanRotate(choice.type))
				tip += QStringLiteral("\nLeft/right arrow keys rotate the placement tool.");
			if (EquipmentUsesBrush(choice.type))
				tip += QStringLiteral("\nUses the equipment brush size.");
			if (choice.type == 76 || choice.type == 98 || choice.type == 99 || choice.type == 141)
				tip += QStringLiteral("\nPosition marker; has no physical model in the stationary preview.");
			if (!supported)
				tip += QStringLiteral("\nThis object's 2016 model is not available yet.");
			action->setToolTip(tip);
			const EquipmentChoice* selected = &choice;
			QObject::connect(action, &QAction::triggered, window, [this, selected, action]() {
				selectedEquipment = selected;
				selectedEquipmentType = selected->type;
				selectedEquipmentAction = action;
				SetToolMode(EquipmentMode);
			});
			AddButton(equipment, action, choice.row, choice.column);
		}
		brushUpAction = MakeAction("Increase equipment brush size", "dp_BrushSzUp", "Increase the equipment brush. The original radius range is 0 to 25.");
		brushDownAction = MakeAction("Decrease equipment brush size", "dp_BrushSzDown", "Decrease the equipment brush; the smallest brush places one object.");
		QObject::connect(brushUpAction, &QAction::triggered, window, [this]() { ChangeEquipmentBrush(1); });
		QObject::connect(brushDownAction, &QAction::triggered, window, [this]() { ChangeEquipmentBrush(-1); });
		AddButton(equipment, brushUpAction, 0, 14);
		AddButton(equipment, brushDownAction, 2, 14);
		brushSize = new QLabel(QStringLiteral("1"), equipmentPalette);
		brushSize->setAlignment(Qt::AlignCenter);
		brushSize->setToolTip(QStringLiteral("Equipment brush size, 1 to 26. Applies to machinery and Erase Equipment Area."));
		brushDownAction->setEnabled(false);
		equipment->addWidget(brushSize, 1, 13, 1, 2);
		outer->addWidget(equipmentPalette, 4, 0, 1, 14);

		AddButton(outer, UnavailableAction("System modules", "UpgradeModule", "Module allocation is not available yet."), 5, 0);
		armorUpAction = MakeAction("Increase armor", "dp_ArmorIncrease", "Increase armor, up to 10.");
		armorDownAction = MakeAction("Decrease armor", "dp_ArmorDecrease", "Decrease armor, down to 0.");
		QObject::connect(armorUpAction, &QAction::triggered, window, [this]() { ChangeArmor(1); });
		QObject::connect(armorDownAction, &QAction::triggered, window, [this]() { ChangeArmor(-1); });
		AddButton(outer, armorUpAction, 5, 2);
		AddButton(outer, armorDownAction, 5, 3);
		streamline = new QCheckBox(QStringLiteral("Streamline Hull"), parent);
		streamline->setToolTip(QStringLiteral("Streamline the hull for atmospheric flight: improves speed and reduces hull stress."));
		QObject::connect(streamline, &QCheckBox::toggled, window, [this](bool enabled) {
			if (loaded && bool(design.header.streamline) != enabled)
			{
				if(studioHandler)
				{
					AuLegacyStudioProtocol::Command edit; edit.kind = AuLegacyStudioProtocol::Streamline; edit.value = enabled ? 1 : 0;
					SendStudioEdit(edit); RefreshHeaderControls(); return;
				}
				design.header.streamline = enabled ? 1 : 0;
				HeaderChanged(true);
			}
		});
		outer->addWidget(streamline, 5, 5, 1, 7);
		minimumTech = new QComboBox(parent);
		minimumTech->setAccessibleName(QStringLiteral("Minimum tech level"));
		minimumTech->setToolTip(QStringLiteral("Minimum technology level for manufacturing. The displayed levels are 1 through 32."));
		for (int level = 31; level >= 0; --level)
			minimumTech->addItem(QString::number(level + 1), level);
		QObject::connect(minimumTech, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), window, [this](int index) {
			if (loaded && index >= 0 && design.header.minimumTechLevel != minimumTech->itemData(index).toInt())
			{
				if(studioHandler)
				{
					AuLegacyStudioProtocol::Command edit; edit.kind = AuLegacyStudioProtocol::MinTech;
					edit.value = minimumTech->itemData(index).toInt();
					SendStudioEdit(edit); RefreshHeaderControls(); return;
				}
				design.header.minimumTechLevel = quint8(minimumTech->itemData(index).toInt());
				HeaderChanged(true);
			}
		});
		outer->addWidget(minimumTech, 5, 12, 1, 2);
	}

	void RefreshStudioControls()
	{
		const bool online = !studioHandler || (studioConnected && studioReady);
		const bool ownsFileCommands = !studioHandler || (studioOwner && !studioBusy);
		editPanel->setEnabled(loaded && (!studioHandler || studioReady));
		tools->setEnabled(online);
		deleteDeckAction->setEnabled(loaded && online);
		moveAction->setEnabled(loaded && online);
		streamline->setEnabled(online);
		minimumTech->setEnabled(online);
		roomNumber->setEnabled(online);
		armorUpAction->setEnabled(loaded && online && design.header.armor < 10);
		armorDownAction->setEnabled(loaded && online && design.header.armor > 0);
		newAction->setEnabled(online && ownsFileCommands);
		openAction->setEnabled(online && ownsFileCommands);
		saveAction->setEnabled(loaded && (!studioHandler || studioReady) && ownsFileCommands);
		passwordAction->setVisible(bool(studioHandler));
		passwordButton->setVisible(bool(studioHandler));
		passwordAction->setEnabled(bool(studioHandler) && online && studioOwner && !studioBusy);
		const bool showBurn = bool(studioHandler) && !modified && studioCanBurn;
		burnAction->setVisible(showBurn);
		burnButton->setVisible(showBurn);
		burnAction->setEnabled(bool(studioHandler) && online && !studioBusy && !modified && studioCanBurn);
		saveAction->setToolTip(studioHandler && studioConnected
			? QStringLiteral("Save the spacecraft design in your library or export a .ship file (Ctrl+S).")
			: QStringLiteral("Save spacecraft to a .ship file (Ctrl+S)."));
		editTool.active = loaded && online && !renderAction->isChecked() && toolMode != HandMode;
		renderView->SetEditTool(editTool);
	}

	void SendStudioRequest(AuLegacyStudioProtocol::Request request)
	{
		if(!studioHandler) return;
		if(!studioConnected || !studioReady)
		{
			SetFeedback(QStringLiteral("Waiting for the studio connection before editing."));
			return;
		}
		request.generation = studioGeneration;
		request.revision = studioRevision;
		studioHandler(request);
	}

	void SendStudioEdit(const AuLegacyStudioProtocol::Command& edit)
	{
		AuLegacyStudioProtocol::Request request; request.action = AuLegacyStudioProtocol::Edit; request.edit = edit;
		SendStudioRequest(request);
	}

	bool StudioFileCommandAllowed(bool saveSnapshot = false)
	{
		if(!studioHandler) return true;
		if(!studioOwner)
		{
			SetFeedback(QStringLiteral("Only the studio owner can create, open, or save a design."));
			return false;
		}
		if((!studioConnected && !saveSnapshot) || !studioReady || studioBusy)
		{
			SetFeedback(QStringLiteral("Wait for the studio changes to finish before using file commands."));
			return false;
		}
		return true;
	}

	void ReplaceStudioDesign(const AuLegacyShip::Design& incoming)
	{
		if(!StudioFileCommandAllowed()) return;
		QString error;
		AuLegacyStudioProtocol::Request request; request.action = AuLegacyStudioProtocol::Replace;
		request.document = AuLegacyStudioProtocol::EncodeDocument(incoming, error);
		if(request.document.isEmpty()) { SetFeedback(error); return; }
		SendStudioRequest(request);
	}

	void ChangeStudioPassword()
	{
		if(!studioHandler || !StudioFileCommandAllowed()) return;
		bool accepted = false;
		const QString password = QInputDialog::getText(window, QStringLiteral("Studio Password"),
			QStringLiteral("Enter a design studio password.\n\n"
				"The first person to enter becomes the owner. Other people may join when their password matches yours. "
				"A blank password prevents anyone from joining, even if their password is blank.\n\n"
				"Only the owner can use New, Open, or Save."), QLineEdit::Normal, QString(), &accepted);
		if(!accepted || !StudioFileCommandAllowed()) return;
		if(password.size() > AuLegacyStudioProtocol::MaximumPasswordCharacters)
		{
			SetFeedback(QStringLiteral("Studio passwords may contain at most %1 characters.")
				.arg(AuLegacyStudioProtocol::MaximumPasswordCharacters));
			return;
		}
		AuLegacyStudioProtocol::Request request; request.action = AuLegacyStudioProtocol::Password;
		request.password = password; SendStudioRequest(request);
	}

	void BurnStudioMedia()
	{
		if(!studioHandler || !studioConnected || !studioReady || studioBusy || modified || !studioCanBurn) return;
		const quint32 generation = studioGeneration;
		const quint64 revision = studioRevision;
		// AuView::DesignBurnCD (2016 executable 0x1402e8db0) confirms the
		// first transition to Read Only, then asks the server to burn one disk.
		if(design.header.state == 0 && QMessageBox::warning(window, QStringLiteral("Burn CD"),
			QStringLiteral("Storing the design onto a disk or paper drawing causes the design to become Read Only.\n"
				"A Read Only design can not be replaced later. A Read Only design\n"
				"can be modified but changes must be saved using a different name."),
			QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Ok) return;
		if(!studioConnected || studioBusy || modified || !studioCanBurn
			|| generation != studioGeneration || revision != studioRevision)
		{
			SetFeedback(QStringLiteral("The shared design changed. Review it before burning a disk."));
			return;
		}
		AuLegacyStudioProtocol::Request request; request.action = AuLegacyStudioProtocol::BurnMedia;
		SendStudioRequest(request);
	}

	void RequestStudioLibrary()
	{
		AuLegacyStudioProtocol::Request request; request.action = AuLegacyStudioProtocol::Library;
		SendStudioRequest(request);
	}

	void RefreshStudioLibrary()
	{
		if(studioSaveNames)
		{
			const QString currentName = studioSaveNames->currentText();
			const bool blocked = studioSaveNames->blockSignals(true);
			studioSaveNames->clear();
			for(const auto& entry : studioLibrary) studioSaveNames->addItem(entry.name, entry.id);
			studioSaveNames->setEditText(currentName);
			studioSaveNames->blockSignals(blocked);
		}
		if(studioSaveButton) studioSaveButton->setEnabled(studioLibraryLoaded);
		if(studioLoadTable)
		{
			quint32 selectedId = studioLibraryDesignId;
			if(studioLoadTable->currentRow() >= 0)
				selectedId = studioLoadTable->item(studioLoadTable->currentRow(), 0)->data(Qt::UserRole).toUInt();
			studioLoadTable->setSortingEnabled(false);
			studioLoadTable->setRowCount(0);
			const QStringList states = QStringList() << QStringLiteral("Writable") << QStringLiteral("Read Only")
				<< QStringLiteral("Public") << QStringLiteral("View Only") << QStringLiteral("Published") << QStringLiteral("Deleted");
			for(const auto& entry : studioLibrary)
			{
				const int row = studioLoadTable->rowCount();
				studioLoadTable->insertRow(row);
				QTableWidgetItem* reference = new QTableWidgetItem(QString::number(entry.id));
				reference->setData(Qt::UserRole, entry.id);
				studioLoadTable->setItem(row, 0, reference);
				studioLoadTable->setItem(row, 1, new QTableWidgetItem(entry.name));
				studioLoadTable->setItem(row, 2, new QTableWidgetItem(entry.state < states.size()
					? states.at(entry.state) : QString::number(entry.state)));
				if(entry.id == selectedId) studioLoadTable->selectRow(row);
			}
			studioLoadTable->resizeColumnsToContents();
			studioLoadTable->sortByColumn(1, Qt::AscendingOrder);
			studioLoadTable->setSortingEnabled(true);
			studioLoadTable->horizontalHeader()->setStretchLastSection(true);
		}
		const QString status = !studioLibraryLoaded ? QStringLiteral("Loading spacecraft design library...")
			: studioLibraryTruncated ? QStringLiteral("The first %1 designs are shown.").arg(studioLibrary.size())
			: studioLibrary.isEmpty() ? QStringLiteral("No spacecraft designs are saved in your library.") : QString();
		if(studioSaveLibraryStatus) studioSaveLibraryStatus->setText(status);
		if(studioLoadLibraryStatus) studioLoadLibraryStatus->setText(status);
	}

	bool ChooseDesignFile()
	{
		if(!StudioFileCommandAllowed()) return false;
		QString directory = fileName.isEmpty()
			? settings.value(QStringLiteral("LegacySpacecraft/LastOpenDirectory")).toString()
			: QFileInfo(fileName).absolutePath();
		if(directory.isEmpty() || !QFileInfo(directory).isDir())
			directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
		if(directory.isEmpty() || !QFileInfo(directory).isDir()) directory = QDir::homePath();
		const QString selected = QFileDialog::getOpenFileName(window, QStringLiteral("Open spacecraft design"),
			directory, QStringLiteral("Old spacecraft designs (*.ship);;All files (*)"));
		return !selected.isEmpty() && window->OpenFile(selected);
	}

	void ChooseStudioDesign()
	{
		if(!StudioFileCommandAllowed()) return;
		RequestStudioLibrary();
		// AuDesignOpen (2016 executable 0x140183c60) offers the owner's
		// library separately from importing a file from personal storage.
		AuDialog dialog(window);
		dialog.setAuDlgTitle(QStringLiteral("Load Spacecraft Design"));
		dialog.setAuDlgIcon(AuLegacyShipIcon(QStringLiteral("c_DesignDisk")));
		QGridLayout* layout = new QGridLayout;
		layout->addWidget(new QLabel(QStringLiteral("Select a spacecraft design to load into the design studio.\n"
			"The current contents of the design studio will be lost.\nBe sure to save your work before proceeding."), &dialog), 0, 0, 1, 2);
		layout->addWidget(new QLabel(QStringLiteral("Spacecraft Design Library"), &dialog), 1, 0, 1, 2);
		QTableWidget* library = new QTableWidget(&dialog);
		studioLoadTable = library;
		library->setObjectName(QStringLiteral("twlib"));
		library->setColumnCount(3);
		library->setHorizontalHeaderLabels(QStringList() << QStringLiteral("Reference") << QStringLiteral("Design Name") << QStringLiteral("State"));
		library->setEditTriggers(QAbstractItemView::NoEditTriggers);
		library->setSelectionBehavior(QAbstractItemView::SelectRows);
		library->setSelectionMode(QAbstractItemView::SingleSelection);
		library->setShowGrid(false);
		library->verticalHeader()->hide();
		library->setMinimumHeight(180);
		library->setToolTip(QStringLiteral("Spacecraft designs in your personal library. Spacecraft designs are stored in your personal library when you save them."));
		layout->addWidget(library, 2, 0, 1, 2);
		studioLoadLibraryStatus = new QLabel(&dialog);
		layout->addWidget(studioLoadLibraryStatus, 3, 0, 1, 2);
		QPushButton* load = new QPushButton(QIcon(QStringLiteral(":/data/Library.png")), QStringLiteral("Load Design"), &dialog);
		load->setObjectName(QStringLiteral("pbload"));
		load->setEnabled(false);
		load->setToolTip(QStringLiteral("Loads a spacecraft design from the library."));
		QPushButton* file = new QPushButton(QIcon(QStringLiteral(":/data/c_BlankDisk.png")), QStringLiteral("Load Design From File"), &dialog);
		file->setToolTip(QStringLiteral("Loads a spacecraft design from a file on your local computer."));
		layout->addWidget(load, 4, 0);
		layout->addWidget(file, 4, 1);
		dialog.setAuDlgLayout(layout);
		QObject::connect(library, &QTableWidget::itemSelectionChanged, &dialog, [library, load]() {
			load->setEnabled(library->currentRow() >= 0 && !library->selectedItems().isEmpty());
		});
		QObject::connect(load, &QPushButton::clicked, &dialog, [this, library, &dialog]() {
			if(library->currentRow() < 0 || !StudioFileCommandAllowed()) return;
			const quint32 selectedId = library->item(library->currentRow(), 0)->data(Qt::UserRole).toUInt();
			if(!selectedId || !ConfirmChanges(false) || !StudioFileCommandAllowed()) return;
			AuLegacyStudioProtocol::Request request; request.action = AuLegacyStudioProtocol::LoadDesign;
			request.designId = selectedId;
			SendStudioRequest(request);
			dialog.accept();
		});
		QObject::connect(file, &QPushButton::clicked, &dialog, [this, &dialog]() {
			if(ChooseDesignFile()) dialog.accept();
		});
		RefreshStudioLibrary();
		dialog.exec();
	}

	bool ConfirmChanges(bool leaving)
	{
		if(studioHandler && studioBusy)
		{
			SetFeedback(QStringLiteral("Wait for the studio changes to finish before leaving."));
			return false;
		}
		if(!modified || (studioHandler && !studioOwner)) return true;
		const QMessageBox::StandardButton choice = QMessageBox::question(window, QStringLiteral("Save changes?"),
			QStringLiteral("Save changes to %1 before continuing?").arg(DisplayName()),
			QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
		if(choice == QMessageBox::Discard) return true;
		if(choice != QMessageBox::Save || !SaveDesign()) return false;
		if(studioHandler && studioBusy)
		{
			studioExitAfterSave = leaving;
			return false;
		}
		if(studioHandler && studioConnected && modified)
		{
			SetFeedback(QStringLiteral("The file copy is saved. Save the design to your library or discard the shared changes before continuing."));
			return false;
		}
		return true;
	}

	void ChangeDeck(int delta)
	{
		if (!loaded) return;
		const int next = currentDeck + delta;
		if (next >= MinimumGridDeck && next <= MaximumGridDeck)
			ShowDeck(next);
	}

	void MoveCopyDesign()
	{
		if(!loaded) return;
		AuDialog dialog(window);
		dialog.setAuDlgTitle(QStringLiteral("Move/Copy Spacecraft Design"));
		dialog.setAuDlgIcon(AuLegacyShipIcon(QStringLiteral("dp_Move")));
		QGridLayout* layout = new QGridLayout;
		QRadioButton* entire = new QRadioButton(QStringLiteral("Move Entire Design"), &dialog);
		QRadioButton* deck = new QRadioButton(QStringLiteral("Move Current Deck"), &dialog);
		QRadioButton* copy = new QRadioButton(QStringLiteral("Copy Current Deck"), &dialog);
		entire->setToolTip(QStringLiteral("<html>All rooms and equipment in the design will be moved.<br>"
			"\u00b7 No room or equipment may be moved beyond internal limits.<br>"
			"\u00b7 The rooms and equipment must fit within the design grid extents and deck limits to be valid for burning a CD.</html>"));
		deck->setToolTip(QStringLiteral("<html>Only rooms and equipment on the current deck will be moved.<br>"
			"\u00b7 No room or equipment may be moved beyond internal limits.<br>"
			"\u00b7 The rooms and equipment must fit within the design grid extents and deck limits to be valid for burning a CD.<br>"
			"\u00b7 A deck cannot be moved to a deck that already contains any of the design.</html>"));
		copy->setToolTip(QStringLiteral("<html>Only rooms and equipment on the current deck will be copied.<br>"
			"\u00b7 No room or equipment may be copied beyond internal limits.<br>"
			"\u00b7 The rooms and equipment must fit within the design grid extents and deck limits to be valid for burning a CD.<br>"
			"\u00b7 A deck cannot be copied to a deck that already contains any of the design.</html>"));
		layout->addWidget(entire, 0, 0, 1, 2);
		layout->addWidget(deck, 1, 0, 1, 2);
		layout->addWidget(copy, 2, 0, 1, 2);
		QSpinBox* lateral = new QSpinBox(&dialog);
		QSpinBox* longitudinal = new QSpinBox(&dialog);
		QSpinBox* vertical = new QSpinBox(&dialog);
		QSpinBox* count = new QSpinBox(&dialog);
		lateral->setRange(-80, 80);
		longitudinal->setRange(-80, 80);
		vertical->setRange(-84, 84);
		count->setRange(1, 84);
		lateral->setToolTip(QStringLiteral("<html>Amount to move or copy the design laterally, in grid squares.<br>"
			"\u00b7 Positive moves the design starboard.<br>\u00b7 Negative moves the design port.</html>"));
		longitudinal->setToolTip(QStringLiteral("<html>Amount to move or copy the design longitudinally, in grid squares.<br>"
			"\u00b7 Positive moves the design fore.<br>\u00b7 Negative moves the design aft.</html>"));
		vertical->setToolTip(QStringLiteral("<html>Amount to move or copy the design vertically, in whole decks.<br>"
			"\u00b7 Positive moves the design aloft.<br>\u00b7 Negative moves the design below.</html>"));
		count->setToolTip(QStringLiteral("<html>Number of duplicates when copying. Each successive duplicate is offset "
			"from the last using the values entered. This can be used to make multiple copies of the current deck.</html>"));
		const QString labels[] = { QStringLiteral("Lateral Distance"), QStringLiteral("Longitudinal Distance"),
			QStringLiteral("Vertical Distance"), QStringLiteral("Copy Count") };
		QSpinBox* fields[] = { lateral, longitudinal, vertical, count };
		for(int i = 0; i < 4; ++i)
		{
			QLabel* label = new QLabel(labels[i], &dialog);
			label->setBuddy(fields[i]);
			fields[i]->setAccessibleName(labels[i]);
			layout->addWidget(label, i + 3, 0);
			layout->addWidget(fields[i], i + 3, 1);
		}
		QPushButton* apply = new QPushButton(QStringLiteral("Move/Copy Design"), &dialog);
		layout->addWidget(apply, 7, 1);
		dialog.setAuDlgLayout(layout);
		const int operation = settings.value(QStringLiteral("Widget/DesignMoveCopy/DoWhat"), 0).toInt();
		entire->setChecked(operation != 1 && operation != 2);
		deck->setChecked(operation == 1);
		copy->setChecked(operation == 2);
		lateral->setValue(settings.value(QStringLiteral("Widget/DesignMoveCopy/XDelta"), 0).toInt());
		longitudinal->setValue(settings.value(QStringLiteral("Widget/DesignMoveCopy/YDelta"), 0).toInt());
		vertical->setValue(settings.value(QStringLiteral("Widget/DesignMoveCopy/ZDelta"), 0).toInt());
		count->setValue(settings.value(QStringLiteral("Widget/DesignMoveCopy/RepeatCount"), 1).toInt());
		const auto request = [this, copy, deck, lateral, longitudinal, vertical, count]() {
			AuLegacyShip::MoveCopyRequest value;
			value.mode = copy->isChecked() ? AuLegacyShip::MoveCopyMode::CopyCurrentDeck
				: deck->isChecked() ? AuLegacyShip::MoveCopyMode::MoveCurrentDeck
				: AuLegacyShip::MoveCopyMode::MoveEntireDesign;
			value.deck = currentDeck;
			value.lateral = lateral->value();
			value.longitudinal = longitudinal->value();
			value.vertical = vertical->value();
			value.copies = count->value();
			return value;
		};
		const auto enableControls = [this, copy, deck, count, apply, request]() {
			const QString error = AuLegacyShip::MoveCopyError(design, request());
			count->setEnabled(copy->isChecked());
			apply->setEnabled(error.isEmpty());
			apply->setToolTip(error.isEmpty()
				? (copy->isChecked() ? QStringLiteral("<html>Copy the rooms and equipment on the current deck.</html>")
					: deck->isChecked() ? QStringLiteral("<html>Move the rooms and equipment on the current deck.</html>")
					: QStringLiteral("<html>Move all rooms and equipment in the design.</html>"))
				: QStringLiteral("<html>Move/Copy is disabled.<br><b>Reason</b><br>%1</html>").arg(error.toHtmlEscaped()));
		};
		for(QRadioButton* option : { entire, deck, copy })
			QObject::connect(option, &QRadioButton::toggled, &dialog, enableControls);
		for(QSpinBox* field : fields)
			QObject::connect(field, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), &dialog, enableControls);
		QObject::connect(apply, &QPushButton::clicked, &dialog, &QDialog::accept);
		enableControls();
		const bool accepted = dialog.exec() == QDialog::Accepted;
		const AuLegacyShip::MoveCopyRequest value = request();
		settings.setValue(QStringLiteral("Widget/DesignMoveCopy/DoWhat"), int(value.mode));
		settings.setValue(QStringLiteral("Widget/DesignMoveCopy/XDelta"), value.lateral);
		settings.setValue(QStringLiteral("Widget/DesignMoveCopy/YDelta"), value.longitudinal);
		settings.setValue(QStringLiteral("Widget/DesignMoveCopy/ZDelta"), value.vertical);
		settings.setValue(QStringLiteral("Widget/DesignMoveCopy/RepeatCount"), value.copies);
		if(!accepted) return;
		if(studioHandler)
		{
			AuLegacyStudioProtocol::Command edit; edit.kind = AuLegacyStudioProtocol::MoveCopy; edit.move = value;
			SendStudioEdit(edit); return;
		}
		const AuLegacyShip::MoveCopyResult result = AuLegacyShip::MoveCopy(design, value);
		if(!result.accepted)
		{
			SetFeedback(result.error);
			return;
		}
		if(result.changed)
		{
			modified = true;
			renderDirty = true;
			renderView->ResetEditStroke();
			IndexDecks();
			UpdateTitle();
			ShowDeck(currentDeck);
		}
		QString message = result.changed
			? (value.mode == AuLegacyShip::MoveCopyMode::CopyCurrentDeck
				? QStringLiteral("%1 deck copies made. Press Ctrl+S to save.").arg(result.copiesMade)
				: QStringLiteral("Rooms and equipment moved. Press Ctrl+S to save."))
			: QStringLiteral("No rooms or equipment changed.");
		if(!result.error.isEmpty()) message += QLatin1Char('\n') + result.error;
		SetFeedback(message);
	}

	void ChangeArmor(int delta)
	{
		if (!loaded || (delta > 0 && design.header.armor >= 10) || (delta < 0 && design.header.armor == 0))
			return;
		if(studioHandler)
		{
			AuLegacyStudioProtocol::Command edit; edit.kind = AuLegacyStudioProtocol::Armor; edit.value = delta;
			SendStudioEdit(edit); return;
		}
		design.header.armor = quint8(int(design.header.armor) + delta);
		HeaderChanged(true);
	}

	void RefreshHeaderControls()
	{
		streamline->blockSignals(true);
		streamline->setChecked(design.header.streamline != 0);
		streamline->blockSignals(false);
		minimumTech->blockSignals(true);
		minimumTech->setCurrentIndex(minimumTech->findData(design.header.minimumTechLevel));
		minimumTech->blockSignals(false);
		armorUpAction->setEnabled(loaded && design.header.armor < 10);
		armorDownAction->setEnabled(loaded && design.header.armor > 0);
		const QString armor = QStringLiteral("Current armor: %1 (0-10).").arg(design.header.armor);
		armorUpAction->setToolTip(QStringLiteral("Increase armor\n") + armor);
		armorDownAction->setToolTip(QStringLiteral("Decrease armor\n") + armor);
		RefreshStudioControls();
	}

	void HeaderChanged(bool geometryChanged = false)
	{
		modified = true;
		renderDirty = renderDirty || geometryChanged;
		if (geometryChanged)
			SyncRenderDesign();
		RefreshHeaderControls();
		ScheduleStatsRefresh();
		UpdateTitle();
		SetFeedback(QStringLiteral("Design settings updated. Press Ctrl+S to save."));
	}

	void SetFeedback(const QString& text)
	{
		feedback = text;
		statsBrowser->setToolTip(text);
		if (!statsTimer->isActive())
			statsTimer->start();
	}

	void ScheduleStatsRefresh()
	{
		statsDirty = true;
		if (!statsTimer->isActive())
			statsTimer->start();
	}

	void CheckPaths()
	{
		if (!loaded || pathsChecking)
			return;
		InvalidatePathReport();
		pathsChecking = true;
		checkPathsAction->setEnabled(false);
		QProgressDialog progress(QStringLiteral("Checking paths..."), QStringLiteral("Cancel"), 0, 0, window);
		progress.setWindowTitle(QStringLiteral("Checking paths..."));
		progress.setWindowModality(Qt::WindowModal);
		progress.setMinimumDuration(0);
		progress.setAutoClose(false);
		progress.setAutoReset(false);
		progress.show();
		// Progress dispatches GUI events, including remote studio snapshots.
		// Analyze a stable copy rather than allowing an update to replace the
		// room/detail arrays while the path analyzer is traversing them.
		const AuLegacyShip::Design checkedDesign = design;
		const quint32 checkedGeneration = studioGeneration;
		const quint64 checkedRevision = studioRevision;
		const QString report = AuLegacyShipPathReport(checkedDesign,
			[&progress](const QString& label, int value, int maximum) {
				if (progress.wasCanceled())
					return false;
				if (progress.labelText() != label)
					progress.setLabelText(label);
				if (progress.maximum() != maximum)
					progress.setRange(0, maximum);
				if (progress.value() != value)
					progress.setValue(value);
				QCoreApplication::processEvents();
				return !progress.wasCanceled();
			});
		progress.hide();
		pathsChecking = false;
		checkPathsAction->setEnabled(loaded);
		if (report.isEmpty())
			return;
		if(studioHandler && (checkedGeneration != studioGeneration || checkedRevision != studioRevision))
		{
			SetFeedback(QStringLiteral("The shared design changed while checking paths. Run the check again for the latest design."));
			return;
		}
		QDialog* dialog = new QDialog(window);
		pathReportDialog = dialog;
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->setWindowTitle(QStringLiteral("Check Paths Report"));
		dialog->setWindowIcon(AuLegacyShipIcon(QStringLiteral("dp_Check")));
		dialog->resize(720, 520);
		dialog->setStyleSheet(QStringLiteral(
			"QDialog, QTextBrowser { background: #081a40; color: #b8cef5; }"
			"QLabel { color: #b8cef5; }"));
		QVBoxLayout* layout = new QVBoxLayout(dialog);
		const QString legends[] = {
			QStringLiteral("Warnings do not prevent a design from being burned to a disk and manufactured."),
			QStringLiteral("<span style=\"color: rgb(255,0,0);\">Path Warning</span> Identifies something that may prevent access to parts of the ship by players and AI."),
			QStringLiteral("<span style=\"color: rgb(255,255,0);\">Security Warning</span> Identifies something that may be unsecure.")
		};
		for (const QString& legend : legends)
		{
			QLabel* label = new QLabel(legend, dialog);
			label->setWordWrap(true);
			layout->addWidget(label);
		}
		QTextBrowser* browser = new QTextBrowser(dialog);
		browser->setAccessibleName(QStringLiteral("Spacecraft path check report"));
		browser->setOpenLinks(false);
		browser->setOpenExternalLinks(false);
		browser->setHtml(report);
		layout->addWidget(browser, 1);
		QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
		QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
		layout->addWidget(buttons);
		dialog->show();
	}

	void InvalidatePathReport()
	{
		if (pathReportDialog)
			pathReportDialog->close();
		pathReportDialog.clear();
	}

	void RefreshStats()
	{
		statsTimer->stop();
		if (statsDirty)
		{
			designReport = AuLegacyShipStatsReport(design);
			statsDirty = false;
		}
		QString report = QStringLiteral("<html><body>");
		if (modified)
			report += QStringLiteral("<p style=\"margin: 0; color: #ffff00;\">Modified</p>");
		report += designReport.isEmpty() ? designTips : designReport;
		if(designReport.isEmpty() && studioHandler)
			report += QStringLiteral("<br><br><b>STORING THE FINISHED DESIGN</b><br>"
				"Check Paths, then walk through the spacecraft to inspect it.<br>"
				"Save Design stores it in your spacecraft design library. Save Design To File creates a personal backup.<br>"
				"Burn CD appears when the design is valid and saved in the library. Have a blank data disk in your inventory.");
		if (!feedback.isEmpty())
			report += QStringLiteral("<hr><p style=\"margin: 0;\">%1</p>")
				.arg(feedback.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
		report += QStringLiteral("</body></html>");
		if (report == statsHtml)
			return;
		statsHtml = report;
		const int scroll = statsBrowser->verticalScrollBar()->value();
		statsBrowser->setHtml(report);
		statsBrowser->verticalScrollBar()->setValue(scroll);
	}

	void FitDeck()
	{
		if (loaded && !renderAction->isChecked())
			renderView->FocusDesignPlane();
	}

	void SyncRenderDesign()
	{
		if (renderDirty)
		{
			renderView->SetDesign(design);
			renderDirty = false;
		}
		renderView->SetDeck(currentDeck);
	}

	void ShowRenderHelp()
	{
		if (!renderAction->isChecked())
			return;
		SetFeedback(QStringLiteral("Walkthrough uses your Avatar and Scene control bindings. Use hand input for the sidebar and door clicks.\n"
			"Escape clears the selected tool and returns to the hand cursor; Shift+Escape hides or shows the interface. Configured Scene camera bindings still work. Ctrl+W exits.\n"
			"Settings changes apply here immediately. Legacy Ctrl+P/C opens/closes nearby doors unless rebound; Home returns to the selected deck unless rebound.\n"
			"Studio gravity is %1; ship interiors retain their own gravity. Render Mode returns to the 3D wireframe at the same position.")
			.arg(gravityAction->isChecked() ? QStringLiteral("on") : QStringLiteral("off")));
	}

	void SetRenderMode(bool solid)
	{
		if(solid) PrepareSkyForScene();
		renderView->SetRenderSolid(solid);
		editTool.active = loaded && !solid && toolMode != HandMode
			&& (!studioHandler || (studioConnected && studioReady));
		renderView->SetEditTool(editTool);
		renderView->setFocus();
		if (solid)
			ShowRenderHelp();
		else
			SetToolMode(toolMode);
	}

	void BeginStartup()
	{
		startupStarted = true;
		startingDesigner = true;
		loadingToolsVisible = !toolDock->isHidden();
		loadingMargins = window->centralWidget()->layout()->contentsMargins();
		renderView->SetLoadingScreen(true);
		toolDock->hide();
		window->centralWidget()->layout()->setContentsMargins(0, 0, 0, 0);
		renderView->setFocus();
	}

	void EndStartup()
	{
		window->centralWidget()->layout()->setContentsMargins(loadingMargins);
		toolDock->setVisible(loadingToolsVisible);
		renderView->SetLoadingScreen(false);
		startingDesigner = false;
		if(window->isVisible()) renderView->repaint();
	}

	void PrepareSkyForScene()
	{
		if(startingDesigner || !renderView->isVisible()) return;
		if(renderView->isValid() && !renderView->SkyboxNeedsPreparation()) return;
		renderView->PrepareDesign();
		renderView->PrepareSkybox();
	}

	void SetToolMode(ToolMode mode)
	{
		if (mode != HandMode && renderAction->isChecked())
			renderAction->setChecked(false);
		toolMode = mode;
		if(mode == HandMode)
		{
			tools->setExclusive(false);
			for(QAction* action : tools->actions()) action->setChecked(false);
			tools->setExclusive(true);
			selectedEquipment = nullptr;
			selectedEquipmentAction = nullptr;
			editTool = AuLegacyShipEditTool();
			renderView->SetEditTool(editTool);
			SetFeedback(QStringLiteral("Hand cursor: select a room or equipment button to edit the design."));
			return;
		}
		editTool.active = loaded && (!studioHandler || (studioConnected && studioReady));
		editTool.erasing = mode == EraseMode || mode == DeleteRoomMode || mode == DeleteEquipmentMode || mode == DeleteEquipmentAreaMode;
		editTool.wholeRoom = mode == DeleteRoomMode;
		editTool.equipment = mode == EquipmentMode || mode == DeleteEquipmentMode || mode == DeleteEquipmentAreaMode;
		editTool.detailType = mode == EquipmentMode ? selectedEquipmentType : 0;
		editTool.radius = mode == DeleteEquipmentAreaMode
			|| (mode == EquipmentMode && EquipmentUsesBrush(selectedEquipmentType)) ? equipmentBrushRadius : 0;
		if(mode == PaintMode) editTool.icon = roomActions.value(selectedRoomType)->icon();
		else if(mode == EraseMode) editTool.icon = eraseAction->icon();
		else if(mode == DeleteRoomMode) editTool.icon = deleteRoomAction->icon();
		else if(mode == DeleteEquipmentMode) editTool.icon = deleteEquipmentAction->icon();
		else if(mode == DeleteEquipmentAreaMode) editTool.icon = deleteEquipmentAreaAction->icon();
		else editTool.icon = selectedEquipmentAction ? selectedEquipmentAction->icon() : QIcon();
		editTool.rotation = AuLegacyShip::DetailRotationStep(selectedEquipmentType);
		editTool.footprint = mode == EquipmentMode && selectedEquipment && selectedEquipment->commodity
			? AuLegacyShip::Rendering::VehicleFootprint(selectedEquipment->commodity) : QRectF();
		renderView->ResetEditStroke();
		renderView->SetEditTool(editTool);
		eraseAction->setChecked(mode == EraseMode);
		deleteRoomAction->setChecked(mode == DeleteRoomMode);
		deleteEquipmentAction->setChecked(mode == DeleteEquipmentMode);
		deleteEquipmentAreaAction->setChecked(mode == DeleteEquipmentAreaMode);
		if (mode == PaintMode)
			roomActions.value(selectedRoomType)->setChecked(true);
		if (mode == EquipmentMode && selectedEquipmentAction)
			selectedEquipmentAction->setChecked(true);
		roomNumber->setEnabled(loaded);
		if (editTool.equipment)
		{
			QString text;
			if (mode == EquipmentMode && selectedEquipment)
			{
				text = QString::fromLatin1(selectedEquipment->name) + QStringLiteral(": ")
					+ AuLegacyShip::DetailPlacementDescription(selectedEquipmentType);
				if (EquipmentCanRotate(selectedEquipmentType))
					text += QStringLiteral(" Left/right arrows rotate (%1 degrees).")
						.arg(AuLegacyShip::DetailRotationStep(selectedEquipmentType) * 45);
			}
			else
				text = mode == DeleteEquipmentAreaMode
					? QStringLiteral("Erase Equipment Area: click equipment to remove that type within the brush.")
					: QStringLiteral("Erase Equipment: left click or drag over equipment.");
			SetFeedback(text + QStringLiteral(" Use the hand cursor on the active design plane."));
			renderView->setFocus();
			return;
		}
		SetFeedback(mode == DeleteRoomMode ? QStringLiteral("Erase Room: click inside a room on the active design plane to erase the whole room.")
				: mode == EraseMode ? QStringLiteral("Erase Room Cells: left click or drag with the hand cursor on the active design plane.")
				: QStringLiteral("Paint %1: left click or drag with the hand cursor on the active design plane.").arg(RoomName(selectedRoomType)));
	}

	void ChangeEquipmentBrush(int delta)
	{
		equipmentBrushRadius = std::max(0, std::min(25, equipmentBrushRadius + delta));
		brushSize->setText(QString::number(equipmentBrushRadius + 1));
		brushUpAction->setEnabled(equipmentBrushRadius < 25);
		brushDownAction->setEnabled(equipmentBrushRadius > 0);
		if (editTool.equipment)
			SetToolMode(toolMode);
	}

	void UpdateGrid()
	{
		quint8 mask = 0;
		for (int i = 0; i < 4; ++i)
			if (lineActions[i]->isChecked())
				mask |= quint8(1 << i);
		gridMask = mask;
		renderView->SetGrid(gridAction->isChecked(), gridMask);
		if (mask == 0 || mask == 1 || mask == 2 || mask == 4 || mask == 8)
			SetFeedback(QStringLiteral("These grid lines do not form closed cells. Enable another direction to paint or erase."));
		else
			SetFeedback(QStringLiteral("Grid directions updated. The highlighted cell follows the original 2016 shape rules."));
	}

	void EditAt(const QPointF& cursor)
	{
		if (!loaded || toolMode == HandMode || renderAction->isChecked())
			return;
		const int deck = currentDeck;
		if(studioHandler)
		{
			AuLegacyStudioProtocol::Command edit; edit.deck = deck; edit.cursor = cursor;
			if(editTool.equipment)
			{
				if(toolMode == EquipmentMode && !selectedEquipment) return;
				edit.kind = toolMode == EquipmentMode ? AuLegacyStudioProtocol::PlaceDetail : AuLegacyStudioProtocol::EraseDetails;
				edit.detailType = quint8(selectedEquipmentType);
				edit.commodity = selectedEquipment ? selectedEquipment->commodity : 0;
				edit.radius = editTool.radius;
			}
			else
			{
				edit.kind = toolMode == DeleteRoomMode ? AuLegacyStudioProtocol::EraseRoom : AuLegacyStudioProtocol::PaintCell;
				edit.gridMask = gridMask;
				edit.roomType = toolMode == EraseMode ? -1 : selectedRoomType;
				edit.roomNumber = quint8(roomNumber->value() - 1);
			}
			SendStudioEdit(edit); return;
		}
		if (editTool.equipment)
		{
			if (toolMode == EquipmentMode && !selectedEquipment) return;
			const AuLegacyShip::DetailEditResult result = toolMode == EquipmentMode
				? AuLegacyShip::PlaceDetail(design, deck, cursor, selectedEquipmentType,
					selectedEquipment->commodity, editTool.radius)
				: AuLegacyShip::DeleteDetails(design, deck, cursor, editTool.radius);
			if (!result.accepted)
			{
				SetFeedback(result.error.isEmpty()
					? QStringLiteral("Equipment cannot be placed here.") : result.error);
				return;
			}
			if (result.changed)
			{
				modified = true;
				renderDirty = true;
				IndexDecks();
				UpdateTitle();
				ShowDeck(deck);
			}
			SetFeedback(result.error.isEmpty()
				? (result.changed ? QStringLiteral("Equipment updated (%1 added, %2 removed). Press Ctrl+S to save.")
					.arg(result.added).arg(result.removed) : QStringLiteral("No equipment change at this point.")) : result.error);
			return;
		}
		const int type = toolMode == EraseMode ? -1 : selectedRoomType;
		const AuLegacyShip::CellEditResult result = toolMode == DeleteRoomMode
			? AuLegacyShip::DeleteRoomAt(design, deck, cursor)
			: AuLegacyShip::EditCell(design, deck, cursor, gridMask, type, quint8(roomNumber->value() - 1));
		if (!result.accepted)
		{
			SetFeedback(result.error.isEmpty()
				? QStringLiteral("This cell edit could not be applied.") : result.error);
			return;
		}
		if (result.changed)
		{
			modified = true;
			renderDirty = true;
			IndexDecks();
			UpdateTitle();
			ShowDeck(deck);
		}
		SetFeedback(result.error.isEmpty()
			? (toolMode == DeleteRoomMode
				? (result.changed ? QStringLiteral("Room erased. Press Ctrl+S to save.") : QStringLiteral("No room inside this point."))
				: (result.changed ? QStringLiteral("Cell updated. Press Ctrl+S to save.") : QStringLiteral("No change at this cell."))) : result.error);
	}

	void DeleteCurrentDeck()
	{
		if(!loaded) return;
		if(studioHandler)
		{
			AuLegacyStudioProtocol::Command edit; edit.kind = AuLegacyStudioProtocol::EraseDeck; edit.deck = currentDeck;
			SendStudioEdit(edit); return;
		}
		const AuLegacyShip::CellEditResult result = AuLegacyShip::DeleteDeck(design, currentDeck);
		if(!result.accepted)
		{
			SetFeedback(result.error);
			return;
		}
		if(result.changed)
		{
			modified = true;
			renderDirty = true;
			renderView->ResetEditStroke();
			IndexDecks();
			UpdateTitle();
			ShowDeck(currentDeck);
		}
		SetFeedback(result.changed ? QStringLiteral("Deck erased, including its equipment. Press Ctrl+S to save.")
			: QStringLiteral("This deck is already empty."));
	}

	QString DisplayName() const
	{
		if (!design.header.name.isEmpty())
			return design.header.name;
		return fileName.isEmpty() ? QStringLiteral("Untitled spacecraft") : QFileInfo(fileName).completeBaseName();
	}

	void ShowDeck(int deck)
	{
		deck = std::max(MinimumGridDeck, std::min(MaximumGridDeck, deck));
		if (deck != currentDeck)
			renderView->ResetEditStroke();
		currentDeck = deck;
		SyncRenderDesign();
		deckUpAction->setEnabled(loaded && deck < MaximumGridDeck);
		deckDownAction->setEnabled(loaded && deck > MinimumGridDeck);
		geometryWarnings.clear();
		int visibleRooms = 0;
		int warningCount = 0;
		for (int index : roomDecks.value(deck))
		{
			const AuLegacyShip::Room& room = design.rooms[index];
			QString error;
			const QPolygonF polygon = AuLegacyShip::RoomBoundary(room, &error);
			if (polygon.size() < 3 || !error.isEmpty())
			{
				++warningCount;
				if (geometryWarnings.size() < 10)
					geometryWarnings.append(QStringLiteral("Room %1: %2").arg(room.roomId)
						.arg(error.isEmpty() ? QStringLiteral("fewer than three boundary points") : error));
				continue;
			}
			++visibleRooms;
		}
		if (warningCount > geometryWarnings.size())
			geometryWarnings.append(QStringLiteral("... and %1 more invalid boundaries").arg(warningCount - geometryWarnings.size()));
		QString message = QStringLiteral("Deck %1 (Z %2 ft): %3 room boundaries, %4 details. Other decks remain visible and dimmed.")
			.arg(deck).arg(deck * AuLegacyShip::DeckHeight).arg(visibleRooms).arg(detailDecks.value(deck).size());
		if (!visibleRooms && detailDecks.value(deck).isEmpty())
			message += warningCount ? QStringLiteral("\nNo displayable geometry on this deck.")
				: QStringLiteral("\nThis deck is empty. Use the hand cursor to paint room cells on the active plane.");
		if (warningCount)
			message += QStringLiteral("; %1 geometry warnings").arg(warningCount);
		if (!geometryWarnings.isEmpty())
			message += QLatin1Char('\n') + geometryWarnings.join(QLatin1Char('\n'));
		SetFeedback(message);
		ShowRenderHelp();
	}

	void IndexDecks()
	{
		InvalidatePathReport();
		ScheduleStatsRefresh();
		roomDecks.clear();
		detailDecks.clear();
		QSet<int> occupied;
		for (int i = 0; i < design.rooms.size(); ++i)
		{
			const AuLegacyShip::Room& room = design.rooms[i];
			if (!room.packedVertices.isEmpty())
			{
				roomDecks[int(room.deck)].append(i);
				occupied.insert(int(room.deck));
			}
		}
		for (int i = 0; i < design.details.size(); ++i)
		{
			const int deck = design.details[i].deck;
			detailDecks[deck].append(i);
			occupied.insert(deck);
		}
		occupiedDeckCount = occupied.size();
	}

	void UpdateTitle()
	{
		window->setWindowTitle(QStringLiteral("%1%2 \u2014 Legacy Spacecraft Designer")
			.arg(modified ? QStringLiteral("*") : QString(), DisplayName()));
	}

	bool SaveDesign()
	{
		if(!studioHandler || !studioConnected) return SaveDesignToFile();
		if(!loaded || !StudioFileCommandAllowed()) return false;
		// AuDesignSave (2016 executable 0x140186040) keeps the named
		// server library save separate from an optional local file backup.
		AuDialog dialog(window);
		dialog.setAuDlgTitle(QStringLiteral("Save Spacecraft Design"));
		dialog.setAuDlgIcon(AuLegacyShipIcon(QStringLiteral("dp_Save")));
		QGridLayout* layout = new QGridLayout;
		QLabel* libraryText = new QLabel(QStringLiteral(
			"<html><b>SPACECRAFT DESIGN LIBRARY</b><br><br>To save the design into the spacecraft design library, enter a name<br>"
			"for the spacecraft design.<br><br>The list shows your existing spacecraft designs in the library. If<br>"
			"you use the name of an existing design, the existing design is<br>replaced.<br><br>"
			"When a <i>read-only</i> design is replaced, existing spacecraft, disks and<br>"
			"spacecraft factories referencing the design are not affected.<br>"
			"The replaced design ceases to be published in any empire.<br></html>"), &dialog);
		QComboBox* name = new QComboBox(&dialog);
		studioSaveNames = name;
		name->setEditable(true);
		name->setEditText(design.header.name);
		name->setAccessibleName(QStringLiteral("Spacecraft design name"));
		QPushButton* librarySave = new QPushButton(AuLegacyShipIcon(QStringLiteral("dp_Save")),
			QStringLiteral("Save Design"), &dialog);
		studioSaveButton = librarySave;
		librarySave->setToolTip(QStringLiteral("Saves the current design into the spacecraft design library."));
		QLabel* fileText = new QLabel(QStringLiteral(
			"<html><br><b>SPACECRAFT DESIGN FILE</b><br><br>To save the design to a file on your personal storage medium, press<br>"
			"this button. Then you can choose to create a new file or replace an<br>existing file.<br></html>"), &dialog);
		QPushButton* fileSave = new QPushButton(QIcon(QStringLiteral(":/data/c_BlankDisk.png")),
			QStringLiteral("Save Design To File"), &dialog);
		fileSave->setToolTip(QStringLiteral("Saves the current design into a file on your personal storage medium."));
		layout->addWidget(libraryText, 0, 0, 1, 3);
		layout->addWidget(name, 1, 0, 1, 2);
		layout->addWidget(librarySave, 1, 2);
		layout->addWidget(fileText, 2, 0, 1, 3);
		layout->addWidget(fileSave, 3, 1, 1, 2);
		studioSaveLibraryStatus = new QLabel(&dialog);
		layout->addWidget(studioSaveLibraryStatus, 4, 0, 1, 3);
		dialog.setAuDlgLayout(layout);
		bool saved = false;
		QObject::connect(librarySave, &QPushButton::clicked, &dialog, [this, &dialog, name, &saved]() {
			if(!StudioFileCommandAllowed()) return;
			const QString savedName = name->currentText().trimmed();
			if(!ValidLegacyLibraryName(savedName))
			{
				QMessageBox::warning(&dialog, QStringLiteral("Invalid Design Name"),
					QStringLiteral("Enter a valid design name of 1 to 30 characters without reserved characters or extra whitespace."));
				return;
			}
			bool replacing = false;
			for(const auto& entry : studioLibrary) if(entry.name == savedName) { replacing = true; break; }
			const quint32 generation = studioGeneration;
			const quint64 revision = studioRevision;
			if(replacing && QMessageBox::question(&dialog, QStringLiteral("Replace Spacecraft Design"),
				QStringLiteral("Confirm replacement of existing design '%1' in the spacecraft design library.").arg(savedName),
				QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Ok) return;
			if(!StudioFileCommandAllowed()) return;
			if(generation != studioGeneration || revision != studioRevision)
			{
				SetFeedback(QStringLiteral("The shared design changed. Review it before saving to your library."));
				return;
			}
			AuLegacyStudioProtocol::Request request; request.action = AuLegacyStudioProtocol::Saved;
			request.name = savedName;
			SendStudioRequest(request);
			saved = true;
			dialog.accept();
		});
		QObject::connect(fileSave, &QPushButton::clicked, &dialog, [this, &dialog, &saved]() {
			if(SaveDesignToFile()) { saved = true; dialog.accept(); }
		});
		RefreshStudioLibrary();
		RequestStudioLibrary();
		dialog.exec();
		return saved;
	}

	bool SaveDesignToFile()
	{
		if(!loaded || !StudioFileCommandAllowed(true))
			return false;
		QString suggested = fileName;
		if(suggested.isEmpty())
		{
			QString directory = settings.value(QStringLiteral("LegacySpacecraft/LastOpenDirectory")).toString();
			if(directory.isEmpty() || !QFileInfo(directory).isDir())
				directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
			if(directory.isEmpty() || !QFileInfo(directory).isDir())
				directory = QDir::homePath();
			QString baseName = DisplayName();
			baseName.replace(QLatin1Char('/'), QLatin1Char('_'));
			baseName.replace(QLatin1Char('\\'), QLatin1Char('_'));
			suggested = QDir(directory).filePath(baseName + QStringLiteral(".ship"));
		}
		// The original Save button always opens a save dialog. Let the dialog
		// apply the suffix before its overwrite confirmation, including *.ship.
		QFileDialog dialog(window, QStringLiteral("Spacecraft Design File"), suggested,
			QStringLiteral("Spacecraft Designs (*.ship);;All Files (*.*)"));
		dialog.setAcceptMode(QFileDialog::AcceptSave);
		dialog.setFileMode(QFileDialog::AnyFile);
		dialog.setDefaultSuffix(QStringLiteral("ship"));
		if(dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
			return false;
		// A shared studio can change owner or receive edits while the dialog is open.
		if(!StudioFileCommandAllowed(true)) return false;
		const QString path = QFileInfo(dialog.selectedFiles().first()).absoluteFilePath();
		// Name fresh designs from their first filename; preserve loaded QString
		// bytes, including the distinction between null and empty names.
		const QString fallbackName = fileName.isEmpty() || (studioHandler && design.header.name.isEmpty())
			? QFileInfo(path).completeBaseName() : QString();
		AuLegacyShip::Design exportedDesign = design;
		QString error;
		if(!AuLegacyShip::Save(path, exportedDesign, error, fallbackName))
		{
			QMessageBox::warning(window, QStringLiteral("Could not save spacecraft"), error);
			return false;
		}
		if(!studioHandler)
		{
			if(design.header.name.isEmpty() && !fallbackName.isEmpty())
				design.header.name = fallbackName;
			if(design.header.version == 0 && design.header.scalarSlots.isEmpty())
				design.header.scalarSlots = AuLegacyShip::DefaultModuleSlots();
			design.header.version = 2;
			design.header.versionStateRaw = quint8(0x20 | design.header.state);
		}
		fileName = path;
		// Exporting a shared design is a backup, not a library save. Only
		// the server's acknowledged library save can clear its dirty flag.
		if(!studioHandler) modified = false;
		ScheduleStatsRefresh();
		UpdateTitle();
		SetFeedback(studioHandler && !studioConnected
			? QStringLiteral("Saved the last confirmed studio snapshot to %1.").arg(QFileInfo(path).fileName())
			: QStringLiteral("Saved %1").arg(QFileInfo(path).fileName()));
		settings.setValue(QStringLiteral("LegacySpacecraft/LastOpenDirectory"), QFileInfo(path).absolutePath());
		settings.sync();
		return true;
	}

	void SetDesign(const QString& path, AuLegacyShip::Design&& incoming)
	{
		// The 2016 offline client's design counter starts at 20000 and lasts
		// for the process. New/import allocate once; saving never consumes an ID.
		// Evidence: Shores of Hazeron.exe:0x14013ff50 and DaAtom::DesignSpaceShipLoaded.
		static quint32 nextDesignId = 20000;
		const quint32 workingDesignId = nextDesignId++;
		incoming.header.designId = workingDesignId;
		for(AuLegacyShip::Room& room : incoming.rooms)
			room.designId = workingDesignId;
		for(AuLegacyShip::Detail& detail : incoming.details)
			detail.designId = workingDesignId;
		AuLegacyShip::SortDetails(incoming.details);
		design = std::move(incoming);
		fileName = path.isEmpty() ? QString() : QFileInfo(path).absoluteFilePath();
		loaded = true;
		checkPathsAction->setEnabled(true);
		modified = false;
		renderDirty = true;
		gravityAction->setChecked(false);
		IndexDecks();
		UpdateTitle();
		editPanel->setEnabled(true);
		RefreshHeaderControls();
		SetToolMode(HandMode);
		renderView->UseHandCursor();
		ShowDeck(0);
		renderView->PrepareDesign();
		RefreshStats();
		statsBrowser->verticalScrollBar()->setValue(0);
	}

	AuLegacyShipViewer* window;
	// Use the same preference store for embedded and standalone launches.
	QSettings settings;
	AuLegacyShipRender* renderView;
	QAction* showTools;
	QSpinBox* roomNumber;
	QWidget* editPanel;
	QDockWidget* toolDock;
	QTextBrowser* statsBrowser;
	QTimer* statsTimer;
	QCheckBox* streamline;
	QComboBox* minimumTech;
	QLabel* brushSize;
	QAction* gridAction;
	QAction* renderAction;
	QAction* gravityAction;
	QAction* checkPathsAction;
	QPointer<QDialog> pathReportDialog;
	QAction* closeAction;
	QAction* newAction;
	QAction* openAction;
	QAction* saveAction;
	QAction* passwordAction;
	QAction* burnAction;
	QAction* moveAction;
	QToolButton* passwordButton;
	QToolButton* burnButton;
	QAction* eraseAction;
	QAction* deleteRoomAction;
	QAction* deleteDeckAction;
	QAction* deleteEquipmentAction;
	QAction* deleteEquipmentAreaAction;
	QAction* brushUpAction;
	QAction* brushDownAction;
	QAction* selectedEquipmentAction = nullptr;
	QActionGroup* tools;
	QAction* deckUpAction;
	QAction* deckDownAction;
	QAction* armorUpAction;
	QAction* armorDownAction;
	QMap<int, QAction*> roomActions;
	QAction* lineActions[4];
	std::function<void()> exitHandler;
	std::function<void(AuLegacyStudioProtocol::Request)> studioHandler;
	bool studioConnected = false, studioReady = false, studioOwner = false;
	bool studioBusy = false, studioExitAfterSave = false, studioCanBurn = false;
	bool studioLibraryLoaded = false, studioLibraryTruncated = false;
	quint32 studioLibraryDesignId = 0;
	QVector<AuLegacyStudioProtocol::LibraryEntry> studioLibrary;
	QPointer<QComboBox> studioSaveNames;
	QPointer<QPushButton> studioSaveButton;
	QPointer<QTableWidget> studioLoadTable;
	QPointer<QLabel> studioSaveLibraryStatus;
	QPointer<QLabel> studioLoadLibraryStatus;
	quint32 studioGeneration = 0;
	quint64 studioRevision = 0;
	AuLegacyShip::Design design;
	QString fileName;
	QString statsHtml;
	QString designReport;
	QString designTips;
	QString feedback;
	bool statsDirty = true;
	QStringList geometryWarnings;
	QMap<int, QVector<int> > roomDecks;
	QMap<int, QVector<int> > detailDecks;
	AuLegacyShipEditTool editTool;
	quint8 gridMask = 5;
	ToolMode toolMode = HandMode;
	int selectedRoomType = 2;
	const EquipmentChoice* selectedEquipment = nullptr;
	int selectedEquipmentType = 0;
	int equipmentBrushRadius = 0;
	int currentDeck = 0;
	int occupiedDeckCount = 0;
	bool loaded = false;
	bool modified = false;
	bool renderDirty = true;
	bool pathsChecking = false;
	bool interfaceVisible = true;
	bool toolsWereVisible = true;
	bool startupStarted = false;
	bool startingDesigner = false;
	bool loadingToolsVisible = true;
	QMargins loadingMargins;
};

AuLegacyShipViewer::AuLegacyShipViewer(QWidget* parent)
	: QMainWindow(parent), m_Impl(new Impl(this))
{
	setWindowTitle(QStringLiteral("Legacy Spacecraft Designer"));
	resize(1200, 800);
	NewDesign();
}

AuLegacyShipViewer::~AuLegacyShipViewer()
{
	delete m_Impl;
}

void AuLegacyShipViewer::SetExitHandler(const std::function<void()>& handler)
{
	m_Impl->exitHandler = handler;
	m_Impl->closeAction->setText(handler ? QStringLiteral("E&xit Designer") : QStringLiteral("&Close"));
}

void AuLegacyShipViewer::ControlsChanged()
{
	m_Impl->renderView->ControlsChanged();
	m_Impl->RefreshControlLabels();
	m_Impl->ShowRenderHelp();
}

void AuLegacyShipViewer::ToggleTools()
{
	m_Impl->showTools->trigger();
}

void AuLegacyShipViewer::showEvent(QShowEvent* event)
{
	const bool startLoading = !m_Impl->startupStarted;
	if(startLoading) m_Impl->BeginStartup();
	QMainWindow::showEvent(event);
	if(startLoading)
		QTimer::singleShot(0, this, [this]() {
			// Prepare whichever design was selected before the first frame.
			m_Impl->renderView->repaint();
			m_Impl->renderView->PrepareDesign();
			m_Impl->renderView->PrepareSkybox();
			m_Impl->EndStartup();
		});
}

bool AuLegacyShipViewer::ConfirmDiscardChanges()
{
	return m_Impl->ConfirmChanges(true);
}

void AuLegacyShipViewer::closeEvent(QCloseEvent* event)
{
	if (!ConfirmDiscardChanges())
	{
		event->ignore();
		return;
	}
	QMainWindow::closeEvent(event);
}

void AuLegacyShipViewer::NewDesign()
{
	if (!m_Impl->StudioFileCommandAllowed() || !m_Impl->ConfirmChanges(false))
		return;
	AuLegacyShip::Design incoming;
	incoming.header.version = 2;
	incoming.header.versionStateRaw = 0x20;
	incoming.header.streamline = 1;
	incoming.header.scalarSlots = AuLegacyShip::DefaultModuleSlots();
	if(m_Impl->studioHandler)
	{
		m_Impl->ReplaceStudioDesign(incoming);
		return;
	}
	m_Impl->SetDesign(QString(), std::move(incoming));
}

bool AuLegacyShipViewer::OpenFile(const QString& fileName)
{
	if (!m_Impl->StudioFileCommandAllowed() || !m_Impl->ConfirmChanges(false))
		return false;
	AuLegacyShip::Design incoming;
	QString error;
	if(!AuLegacyShip::Load(fileName, incoming, error))
	{
		QMessageBox::warning(this, QStringLiteral("Could not open legacy spacecraft"),
			QStringLiteral("%1\n\n%2").arg(fileName, error));
		return false;
	}
	if(m_Impl->studioHandler) m_Impl->ReplaceStudioDesign(incoming);
	else m_Impl->SetDesign(fileName, std::move(incoming));
	m_Impl->settings.setValue(QStringLiteral("LegacySpacecraft/LastOpenDirectory"),
		QFileInfo(fileName).absolutePath());
	m_Impl->settings.sync();
	return true;
}

void AuLegacyShipViewer::ChooseFile()
{
	if(m_Impl->studioHandler) m_Impl->ChooseStudioDesign();
	else m_Impl->ChooseDesignFile();
}

void AuLegacyShipViewer::SetStudioCommandHandler(const std::function<void(AuLegacyStudioProtocol::Request)>& handler)
{
	m_Impl->studioHandler = handler;
	m_Impl->studioReady = false;
	m_Impl->studioExitAfterSave = false;
	m_Impl->studioLibraryLoaded = false;
	m_Impl->studioLibraryTruncated = false;
	m_Impl->studioLibraryDesignId = 0;
	m_Impl->studioLibrary.clear();
	m_Impl->RefreshStudioControls();
}

void AuLegacyShipViewer::ApplyStudioState(const AuLegacyShip::Design& design, bool owner, bool modified,
	quint32 generation, quint64 revision)
{
	if(!m_Impl->studioHandler) return;
	if(m_Impl->studioReady && generation == m_Impl->studioGeneration && revision == m_Impl->studioRevision)
	{
		// Ownership/password/save metadata can change without geometry changes.
		m_Impl->studioOwner = owner;
		m_Impl->modified = modified;
		m_Impl->RefreshStudioControls();
		m_Impl->RefreshStats();
		m_Impl->UpdateTitle();
		return;
	}
	// Room boundaries retain their working doubles and the server's identity.
	// Ordinary updates retain the local camera, grid, active deck and paint tool.
	const bool replaced = !m_Impl->studioReady || generation != m_Impl->studioGeneration;
	m_Impl->design = design;
	m_Impl->studioOwner = owner;
	m_Impl->studioGeneration = generation;
	m_Impl->studioRevision = revision;
	m_Impl->studioReady = true;
	m_Impl->loaded = true;
	m_Impl->modified = modified;
	m_Impl->renderDirty = true;
	if(replaced)
	{
		m_Impl->fileName.clear();
		m_Impl->gravityAction->setChecked(false);
		m_Impl->SetToolMode(Impl::HandMode);
		m_Impl->renderView->UseHandCursor();
	}
	m_Impl->IndexDecks();
	m_Impl->RefreshHeaderControls();
	m_Impl->ShowDeck(replaced ? 0 : m_Impl->currentDeck);
	m_Impl->UpdateTitle();
	m_Impl->RefreshStats();
}

void AuLegacyShipViewer::SetStudioConnected(bool connected)
{
	m_Impl->studioConnected = connected;
	m_Impl->RefreshStudioControls();
	if(!connected && m_Impl->studioHandler)
		m_Impl->SetFeedback(QStringLiteral("Studio connection lost. The displayed design is the last confirmed snapshot."));
}

void AuLegacyShipViewer::SetStudioBusy(bool busy)
{
	m_Impl->studioBusy = busy;
	m_Impl->RefreshStudioControls();
	if(busy || !m_Impl->studioExitAfterSave) return;
	m_Impl->studioExitAfterSave = false;
	if(m_Impl->modified)
	{
		m_Impl->SetFeedback(QStringLiteral("The shared design changed while saving. Save the new changes before leaving."));
		return;
	}
	QTimer::singleShot(0, this, [this]() {
		// A path check, startup preparation or another modal operation may
		// dispatch the acknowledgement from a nested event loop. Its parented
		// stack widgets must unwind before the viewer can be deleted.
		if(m_Impl->pathsChecking || m_Impl->startingDesigner || QApplication::activeModalWidget())
		{
			m_Impl->SetFeedback(QStringLiteral("The design is saved. Finish the current operation, then exit the designer."));
			return;
		}
		if(m_Impl->studioBusy || m_Impl->modified)
		{
			m_Impl->SetFeedback(QStringLiteral("The shared design changed while saving. Review the new changes before leaving."));
			return;
		}
		if(m_Impl->exitHandler)
		{
			const std::function<void()> handler = m_Impl->exitHandler;
			handler();
		}
	});
}

void AuLegacyShipViewer::SetStudioBurnAvailable(bool available)
{
	m_Impl->studioCanBurn = available;
	m_Impl->RefreshStudioControls();
}

void AuLegacyShipViewer::SetStudioLibraryDesignId(quint32 designId)
{
	m_Impl->studioLibraryDesignId = designId;
}

void AuLegacyShipViewer::ApplyStudioLibrary(const QVector<AuLegacyStudioProtocol::LibraryEntry>& entries, bool truncated)
{
	m_Impl->studioLibrary = entries;
	m_Impl->studioLibraryLoaded = true;
	m_Impl->studioLibraryTruncated = truncated;
	m_Impl->RefreshStudioLibrary();
}

void AuLegacyShipViewer::ShowStudioMessage(const QString& message)
{
	if(!message.isEmpty()) m_Impl->SetFeedback(message);
}

void AuLegacyShipViewer::ChatReceived(AuCommPacket* message)
{
	m_Impl->renderView->ChatReceived(message);
}
