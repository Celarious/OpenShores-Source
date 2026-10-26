#include "aulegacycityconstruction.h"
#include "aulegacycityconstructionlayout.h"
#include "aulegacycityconstructiontips.h"
#include "aulegacycitymanufacture.h"
#include "aulegacycitybank.h"
#include "auapp.h"
#include "aubldconstruct.h"
#include "aublddesignstudio.h"
#include "deconstructionprocess.h"
#include "audialog.h"
#include "auframe.h"
#include "auglobal.h"
#include "auid.h"
#include "aulegacybd.h"
#include "aulegacycitycatalogue.h"
#include "autoolbutton.h"
#include "auview.h"
#include "daperson.h"
#include "daworldll.h"
#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>

// Keep the resource initializer in the global namespace for static linking.
static void InitLegacyConstructionUiResource()
{
	Q_INIT_RESOURCE(aulegacycityconstructionui);
}

namespace
{
QString ConstructionIcon(const QString& original)
{
	// Only differing/missing original artwork is preserved here. Shared art
	// was compared against the 2016 resources and retains its existing alias.
	const QString legacy = QStringLiteral(":/legacy/cities/ui/%1").arg(original.section(QLatin1Char('/'), -1));
	return QFile::exists(legacy) ? legacy : original;
}

// AuBuildButton, 2016 executable 0x2b8fb0/0x2c6cc0. Its artwork contains
// the caption: drawing an additional tool-button label truncates/duplicates it.
class LegacyBuildButton : public QPushButton
{
	QPixmap m_Image;
	QString m_Name;
	std::function<void()> m_Hover, m_Press;
public:
	LegacyBuildButton(const char* icon, const QString& name, QWidget* parent,
		const std::function<void()>& hover, const std::function<void()>& press)
		: QPushButton(parent), m_Image(ConstructionIcon(icon)), m_Name(name), m_Hover(hover), m_Press(press)
	{
		setFixedSize(AuLegacyCityConstructionLayout::BuildingButtonSize, AuLegacyCityConstructionLayout::BuildingButtonSize);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed); setMouseTracking(true);
		setAccessibleName(name); setToolTip(QStringLiteral("<b>%1</b>").arg(name.toHtmlEscaped()));
	}
protected:
	void mouseMoveEvent(QMouseEvent* event) override
	{ QPushButton::mouseMoveEvent(event); if(m_Hover) m_Hover(); }
	void mousePressEvent(QMouseEvent* event) override
	{ QPushButton::mousePressEvent(event); if(m_Press) m_Press(); }
	void paintEvent(QPaintEvent* event) override
	{
		QPushButton::paintEvent(event);
		const int inset = AuLegacyCityConstructionLayout::BuildingIconInset;
		QRect area = rect().adjusted(inset, inset, -inset, -inset); QPainter painter(this);
		if(!m_Image.isNull()) painter.drawPixmap(area, m_Image);
		else
		{
			painter.setFont(QFont(QStringLiteral("Arial"), 7)); painter.setPen(Qt::black);
			painter.drawText(area, Qt::AlignCenter | Qt::TextWordWrap, m_Name);
			area.translate(-1, -1); painter.setPen(Qt::white);
			painter.drawText(area, Qt::AlignCenter | Qt::TextWordWrap, m_Name);
		}
	}
};

QString TerrainDescription(quint8 terrain)
{
	// Original AuConstructionWidget::ListBuilding, executable 0x2c36...
	switch(terrain)
	{
	case 1: return QObject::tr("Land or Sea Bottom Not Near Anything");
	case 2: return QObject::tr("Land or Sea Bottom Near Road");
	case 3: return QObject::tr("Land or Sea Bottom Near Road Slab");
	case 4: return QObject::tr("Coast or Sea Bottom Near Road");
	case 5: return QObject::tr("Coast or Sea Bottom Near Road and Open Sea");
	case 7: return QObject::tr("Forest Near Road");
	case 8: return QObject::tr("Rocks Near Road");
	case 9: return QObject::tr("Sea");
	case 10: return QObject::tr("Sea Bottom");
	case 11: return QObject::tr("Land, Sea Bottom or Coast Near Road or Lava");
	case 12: return QObject::tr("Any");
	case 13: return QObject::tr("Land, Coast or Sea Bottom Near Road or Zoo");
	case 14: return QObject::tr("Land, Coast or Sea Bottom Near Road or Any Structure");
	case 15: return QObject::tr("Center on Land or Sea Bottom");
	default: return QString();
	}
}

bool UnderConstruction(const AuLegacyCityProtocol::Entry& entry)
{
	return entry.targetLevels > entry.completedLevels || entry.siteLaborRemaining || entry.levelLaborRemaining;
}

quint8 ConstructionEnvironment(bool& flammableMaterialsAllowed)
{
	flammableMaterialsAllowed = true; quint8 environment = 1;
	auto* view = auApp->Frame() ? auApp->Frame()->View() : nullptr; if(!view) return environment;
	DaPerson* person = nullptr; view->LockSceneAtom(&person);
	if(person)
		if(const auto* world = dynamic_cast<const DaWorldLL*>(person->FindParentPlanetInEnv()))
		{
			flammableMaterialsAllowed = world->OrbitZone() != OZONE_Inferno;
			if(!world->HasBreathableAtmosphere()) environment = 2;
			AuMatrix4x4 transform;
			if(person->GetTransformRelativeToParent(transform, world) && world->AltitudeMSL(transform.Translation()) < -32)
				environment = 4;
		}
	view->UnlockSceneAtom(); return environment;
}
}

AuLegacyCityConstruction::AuLegacyCityConstruction(QWidget* parent) : QWidget(parent)
{
	using namespace AuLegacyCityConstructionLayout;
	InitLegacyConstructionUiResource();
	auto* layout = new QGridLayout(this); layout->setMargin(OuterMargin); layout->setSpacing(Spacing);
	m_Categories = new QButtonGroup(this); m_Categories->setExclusive(true);
	m_Materials = new QButtonGroup(this); m_Materials->setExclusive(false);
	m_Pages = new QStackedWidget(this);
	for(int page = 0; page < 9; ++page)
	{
		auto* frame = new QFrame(m_Pages); frame->setFrameShape(QFrame::StyledPanel); frame->setFrameShadow(QFrame::Raised);
		auto* grid = new QGridLayout(frame); grid->setMargin(PageMargin); grid->setSpacing(Spacing);
		m_Pages->addWidget(frame);
	}
	for(const auto& gap : PageGaps)
		static_cast<QGridLayout*>(m_Pages->widget(gap.page)->layout())->setRowMinimumHeight(gap.row, gap.height);
	for(const auto& stretch : PageStretches)
	{
		auto* grid = static_cast<QGridLayout*>(m_Pages->widget(stretch.page)->layout());
		grid->setColumnStretch(stretch.leftColumn, 1); grid->setColumnStretch(stretch.rightColumn, 1);
		grid->setRowStretch(stretch.row, 1);
	}
	for(const auto& category : Categories)
	{
		auto* button = new AuIconButton(QIcon(ConstructionIcon(category.icon)), this);
		button->setCheckable(true);
		button->setFixedSize(CategoryButtonSize, CategoryButtonSize); button->setToolTip(tr(category.name));
		button->setAccessibleName(tr(category.name)); m_Categories->addButton(button, category.page);
		layout->addWidget(button, category.row, CategoryColumn);
		const int page = category.page;
		connect(button, &QToolButton::clicked, this, [this, page]() { SelectCategory(page); });
	}
	m_Clear = new AuIconButton(":data/bp_ClearLand.png", this);
	m_Stop = new AuIconButton(":data/Hand.png", this);
	m_Clear->setAutoRaise(false); m_Stop->setAutoRaise(false);
	m_Clear->setFixedSize(CategoryButtonSize, CategoryButtonSize); m_Stop->setFixedSize(CategoryButtonSize, CategoryButtonSize);
	m_Clear->setToolTip(tr(AuLegacyCityConstructionTips::ClearLand)); m_Stop->setToolTip(tr("Stop Current Build Command"));
	m_Clear->setAccessibleName(tr("Clear Land")); m_Stop->setAccessibleName(tr("Stop Current Build Command"));
	layout->setRowMinimumHeight(CategoryGapRow, CategoryGapHeight);
	layout->addWidget(m_Clear, ClearRow, CategoryColumn); layout->addWidget(m_Stop, StopRow, CategoryColumn);
	layout->addWidget(m_Pages, 0, 0, StackRowSpan, 1);

	m_Grading = new QComboBox(this); m_Grading->addItem(tr("Road Grade"), 0); m_Grading->addItem(tr("Slab Grade"), 1);
	m_Grading->setToolTip(tr(AuLegacyCityConstructionTips::GradingStyle).arg(g_Au.MiddleDotSymbol()));
	m_Grading->setCurrentIndex(qBound(0, g_Au.Settings().readNumEntry("Legacy2016/RoadType", 0), 1));
	m_RoadWidth = new QComboBox(this); m_RoadWidth->setEditable(true);
	// ceil((119 feet / sqrt(.5)) / FeetPerMeter()), original spawn-slab width.
	m_RoadWidth->addItems(QStringList() << "8" << "16" << "24" << "32" << "40" << "52" << "80");
	m_RoadWidth->setValidator(new QDoubleValidator(8, 80, 6, m_RoadWidth));
	m_RoadWidth->setToolTip(tr(AuLegacyCityConstructionTips::RoadWidth).arg(g_Au.MiddleDotSymbol()).arg(52).arg(40).arg(8).arg(80));
	m_RoadWidth->setEditText(QString::number(g_Au.Settings().readNumEntry(
		m_Grading->currentIndex() ? "Legacy2016/SlabWidth" : "Legacy2016/RoadWidth", 8.0)));
	auto* roads = static_cast<QGridLayout*>(m_Pages->widget(8)->layout());
	roads->addWidget(m_Grading, GradingRow, GradingColumn, 1, GradingColumnSpan);
	roads->addWidget(m_RoadWidth, RoadWidthRow, RoadWidthColumn, 1, RoadWidthColumnSpan);
	m_Angle = new QComboBox(this); m_Angle->setEditable(true);
	m_Angle->addItems(QStringList() << "90" << "60" << "45" << "30" << "22.5" << "11.25" << "10" << "5" << "1");
	m_Angle->setValidator(new QDoubleValidator(0, 90, 6, m_Angle));
	m_Angle->setEditText(QString::number(g_Au.Settings().readNumEntry("Legacy2016/RoundingAngle", 90.0)));
	m_Angle->setToolTip(tr("<html><b>Constraint Angle</b><br>The number of degrees for constraining the angle of buildings, roads and landscape areas.</html>"));
	layout->addWidget(m_Angle, ConstraintAngleRow, 0);

	// AuTabBuild's original seven-column information panel, below the pages.
	auto* details = new QGridLayout; details->setMargin(0); details->setSpacing(Spacing);
	m_Name = new QLabel(this); details->addWidget(m_Name, 0, 0, 1, 7);
	m_Designs = new QComboBox(this); details->addWidget(m_Designs, 1, 0, 1, 7);
	m_Designs->setToolTip(tr(AuLegacyCityConstructionTips::DesignInitial));
	auto* materialsHeading = new QLabel(tr("Construction Materials"), this); details->addWidget(materialsHeading, 2, 0, 1, 7);
	materialsHeading->setToolTip(tr(AuLegacyCityConstructionTips::MaterialsHeading));
	m_Recipe = new QLabel(this); m_Recipe->setMinimumSize(1, 16); details->addWidget(m_Recipe, 3, 0, 1, 7);
	const quint16 materialIds[] = {86, 48, 76, 51, 146, 145};
	const char* materialNames[] = {"Wood", "Lumber", "Stone", "Metal", "Magmium", "Vulcium"};
	const char* materialTips[] = {"rough logs", "milled lumber", "stone", "metal", "magmium", "vulcium"};
	for(int i = 0; i < 6; ++i)
	{
		auto* button = new AuIconButton(QIcon(ConstructionIcon(QStringLiteral(":data/c_%1.png").arg(QLatin1String(materialNames[i])))), this);
		button->setAccessibleName(tr(materialNames[i]));
		button->setToolTip(tr("Use %1 for new construction.\nThe existing material is used when adding to a building.").arg(QLatin1String(materialTips[i])));
		m_Materials->addButton(button, materialIds[i]); details->addWidget(button, 4, i + 1);
		const quint16 material = materialIds[i];
		connect(button, &QToolButton::clicked, this, [this, material]() { SetCursorMaterial(material); });
	}
	m_Materials->button(m_Material)->setDown(true);
	m_Levels = new QLabel(tr("Levels"), this); details->addWidget(m_Levels, 5, 0, 1, 7);
	m_Levels->setToolTip(tr("Maximum levels that can be constructed on the same building.\nConstruction material limits the maximum levels for many buildings."));
	m_Terrain = new QLabel(tr("Terrain"), this); m_Terrain->setMinimumSize(1, 36);
	m_Terrain->setAlignment(Qt::AlignLeft | Qt::AlignTop); m_Terrain->setWordWrap(true); details->addWidget(m_Terrain, 6, 0, 1, 7);
	m_Terrain->setToolTip(tr(AuLegacyCityConstructionTips::Terrain));
	m_ProductHeading = new QLabel(tr("Products Manufactured"), this); details->addWidget(m_ProductHeading, 7, 0, 1, 7);
	m_ProductHeading->setToolTip(tr(AuLegacyCityConstructionTips::ProductsHeading));
	m_ProductIcons = new QLabel(this); m_ProductIcons->setMinimumSize(1, 50); details->addWidget(m_ProductIcons, 8, 0, 1, 7);
	m_MakeProduct = new QCheckBox(tr("Make Product"), this); m_MakeProduct->setChecked(true); details->addWidget(m_MakeProduct, 9, 0, 1, 7);
	m_MakeProduct->setToolTip(tr(AuLegacyCityConstructionTips::MakeProduct));
	m_Products = new QComboBox(this); details->addWidget(m_Products, 10, 0, 1, 7);
	m_Jobs = new QLabel(tr("Jobs"), this); details->addWidget(m_Jobs, 11, 0, 1, 7);
	m_Homes = new QLabel(tr("Homes"), this); details->addWidget(m_Homes, 12, 0, 1, 7);
	m_Jobs->setToolTip(tr(AuLegacyCityConstructionTips::Jobs)); m_Homes->setToolTip(tr(AuLegacyCityConstructionTips::Homes));
	details->setRowStretch(13, 1); details->setColumnStretch(0, 1);
	layout->addLayout(details, DetailRow, 0, 1, 2);

	connect(m_Stop, &QToolButton::clicked, this, [this]() { PlanningStopped(); emit StopPlanning(); });
	connect(m_Clear, &QToolButton::clicked, this, [this]() {
		PlanningStopped();
		m_Planning = true; m_ActiveRequest = AuLegacyCityProtocol::Request(); m_ActiveRequest.action = AuLegacyCityProtocol::ClearLand;
		UpdatePlanningSettings();
	});
	connect(m_Designs, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
		g_Au.Settings().writeEntry(QStringLiteral("Legacy2016/BuildingConstruction/%1").arg(m_Industry), m_Designs->currentData().toUInt());
		if(m_Planning && m_ActiveRequest.industryId == m_Industry)
		{ m_ActiveRequest.designId = m_Designs->currentData().toUInt(); UpdatePlanningSettings(); }
	});
	connect(m_Grading, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
		g_Au.Settings().writeEntry("Legacy2016/RoadType", m_Grading->currentIndex());
		const QSignalBlocker blocked(m_RoadWidth);
		m_RoadWidth->setEditText(QString::number(g_Au.Settings().readNumEntry(
			m_Grading->currentIndex() ? "Legacy2016/SlabWidth" : "Legacy2016/RoadWidth", 8.0)));
		UpdatePlanningSettings();
	});
	connect(m_RoadWidth, &QComboBox::editTextChanged, this, [this](const QString& text) {
		bool valid = false; const double width = text.toDouble(&valid);
		if(valid && width >= 8 && width <= 80)
		{ g_Au.Settings().writeEntry(m_Grading->currentIndex() ? "Legacy2016/SlabWidth" : "Legacy2016/RoadWidth", width); UpdatePlanningSettings(); }
	});
	connect(m_Angle, &QComboBox::editTextChanged, this, [this](const QString& text) {
		bool valid = false; const double angle = text.toDouble(&valid);
		if(valid && angle >= 0 && angle <= 90)
		{ g_Au.Settings().writeEntry("Legacy2016/RoundingAngle", angle); UpdatePlanningSettings(); }
	});
	connect(m_Products, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { DescribeProducts(); UpdatePlanningSettings(); });
	connect(m_MakeProduct, &QCheckBox::toggled, this, [this](bool) { UpdateControls(); UpdatePlanningSettings(); });
	m_Timer = new QTimer(this); m_Timer->setInterval(1000);
	connect(m_Timer, &QTimer::timeout, this, &AuLegacyCityConstruction::UpdateControls);
	m_Timeout = new QTimer(this); m_Timeout->setSingleShot(true); m_Timeout->setInterval(60000);
	connect(m_Timeout, &QTimer::timeout, this, [this]() {
		m_PendingToken = 0; ReportMessage(tr("No server response. Select construction again before repeating a change.")); UpdateControls();
	});
	connect(auApp, &AuApp::LegacyCityReplyReceived, this, &AuLegacyCityConstruction::ReceiveReply);
	connect(auApp, &AuApp::LegacyScTransportReset, this, &AuLegacyCityConstruction::ResetTransport);
	SelectCategory(DefaultPage);
}

void AuLegacyCityConstruction::LoadCatalogue()
{
	if(m_Loaded) return;
	QString error;
	if(!AuLegacyCityCatalogue::LoadBuiltIn(error) || !AuLegacyBd::Catalog::LoadBuiltIn(error))
	{ ReportMessage(error); return; }
	m_Loaded = true;
	for(auto* button : m_Materials->buttons()) button->setDown(m_Materials->id(button) == m_Material);
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	for(const auto& entry : AuLegacyCityConstructionLayout::Buttons)
	{
		const auto* recipe = entry.process ? catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(entry.process))
			: catalogue->ConstructionProcessForIndustry(AuLegacyCity::IndustryId(entry.industry));
		if(!recipe) continue;
		const quint8 process = recipe->id.value;
		auto* page = m_Pages->widget(entry.page);
		auto* button = new LegacyBuildButton(entry.icon, tr(entry.name), page,
			[this, process]() { if(!m_Planning) SelectProcess(process); },
			[this, process]() { SelectProcess(process); StartPlanning(); });
		const auto* text = AuLegacyCityConstructionTips::FindButton(entry.industry, entry.process);
		QString tip = QString::fromLatin1(AuLegacyCityConstructionTips::TitleFormat).arg(tr(entry.name).toHtmlEscaped());
		if(text && *text->description)
			tip = QString::fromLatin1(AuLegacyCityConstructionTips::DescriptionFormat).arg(
				tr(entry.name).toHtmlEscaped(), tr(text->description).replace(QLatin1String("%1"), g_Au.MiddleDotSymbol()));
		tip += tr(AuLegacyCityConstructionTips::PlacementTips(entry.process)).replace(QLatin1String("%1"), g_Au.MiddleDotSymbol());
		button->setToolTip(tip);
		button->setProperty("legacyConstructionProcess", int(process));
		button->setProperty("legacyIndustry", int(entry.industry));
		static_cast<QGridLayout*>(page->layout())->addWidget(button, entry.row, entry.column, entry.rowSpan, entry.columnSpan);
	}
}

void AuLegacyCityConstruction::SelectCategory(int page)
{
	// Keep selection separate from the transient press/release state.
	if(auto* button = m_Categories->button(page)) button->setChecked(true);
	if(m_Pages->currentIndex() != page) m_Pages->setCurrentIndex(page);
}

void AuLegacyCityConstruction::SelectProcess(quint8 processId)
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn(); if(!catalogue) return;
	const auto* process = catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(processId)); if(!process) return;
	if(m_Process == processId) return;
	m_Industry = process->industry.value; m_Process = processId;
	const auto* industry = catalogue->Industry(process->industry);
	QString name = process->name;
	if(industry && !name.isEmpty() && name != industry->name && !name.endsWith(QLatin1String("Area")))
		name += QLatin1Char(' ') + industry->name;
	else if(name.isEmpty() && industry) name = industry->name;
	m_Name->setText(QStringLiteral("<b>%1</b>").arg(name.toHtmlEscaped()));
	const bool procedural = m_Industry == 1 || m_Industry == 2 || m_Industry == 10 || m_Industry == 114 || m_Industry == 129;
	m_Designs->setToolTip(tr(procedural ? AuLegacyCityConstructionTips::DesignProcedural : AuLegacyCityConstructionTips::DesignOrdinary)
		.arg(industry ? industry->name.toHtmlEscaped() : QString()));
	const quint32 previous = quint32(g_Au.Settings().readNumEntry(
		QStringLiteral("Legacy2016/BuildingConstruction/%1").arg(m_Industry), 0));
	{
		const QSignalBlocker blocked(m_Designs); m_Designs->clear();
		QMultiMap<QString, quint32> designs;
		for(quint32 id = 1; id <= 74; ++id)
		{
			const auto design = AuLegacyBd::Catalog::BuiltIn().Find(id);
			if(design && design->header.industryId == m_Industry)
				designs.insert(tr("%1 by %2").arg(design->header.name).arg(design->header.architectName), id);
		}
		for(auto it = designs.cbegin(); it != designs.cend(); ++it)
			m_Designs->addItem(QIcon(":data/Library.png"), it.key(), it.value());
		const int selected = m_Designs->findData(previous); if(selected >= 0) m_Designs->setCurrentIndex(selected);
	}
	{
		const QSignalBlocker blocked(m_Products); m_Products->clear();
		const auto defaultProcess = catalogue->DefaultManufacturingProcess(AuLegacyCity::IndustryId(m_Industry));
		for(const auto& product : catalogue->ManufacturingProcesses())
			if(product.industry.value == m_Industry)
			{
				const auto* commodity = catalogue->Commodity(product.commodity);
				m_Products->addItem(commodity ? QIcon(ConstructionIcon(commodity->descriptor.BitmapName())) : QIcon(), product.name, uint(product.id.value));
			}
		const int selected = m_Products->findData(uint(defaultProcess.value)); if(selected >= 0) m_Products->setCurrentIndex(selected);
	}
	DescribeRecipe(); DescribeProducts(); UpdateControls();
}

void AuLegacyCityConstruction::DescribeRecipe()
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn(); if(!catalogue || !m_Process) return;
	const auto* process = catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId(m_Process)); if(!process) return;
	bool flammable = true; const quint8 environment = ConstructionEnvironment(flammable);
	const quint16 material = m_Industry == 2 ? (m_Process == 3 ? 23 : 76) : m_Material;
	m_LevelLimit = catalogue->ConstructionLevelLimit(process->id, AuLegacyCity::CommodityId(material), flammable);
	// AuTabBuild::ListConstructionMaterials, 2016 executable 0x2c13b0.
	QString levels;
	if(process->stackLimit == 32) levels = tr("<b>1 per Rock</b>");
	else if(process->stackLimit == 33) levels = tr("<b>1 per Shrub</b>");
	else if(process->stackLimit == 34) levels = tr("<b>1, 2 if 4 or More Trees</b>");
	else if(process->stackLimit == 0 || process->stackLimit == 1 || process->stackLimit == 3
		|| process->stackLimit == 7 || process->stackLimit == 15 || process->stackLimit == 31)
	{
		const int maximum = int(process->stackLimit) + 1;
		const int selected = catalogue->ConstructionLevelLimit(process->id, AuLegacyCity::CommodityId(material));
		levels = selected < maximum ? tr("<b>%1</b> <i>(%2 with Other Material)</i>").arg(selected).arg(maximum)
			: tr("<b>%1</b>").arg(selected);
	}
	else levels = tr("<b>?</b>");
	m_Levels->setText(tr("Levels %1").arg(levels));
	m_Terrain->setText(tr("Terrain <b>%1</b>").arg(TerrainDescription(process->terrain)));
	QMap<quint16, quint32> materials;
	for(const auto& requirement : catalogue->ConstructionRequirements(process->id, AuLegacyCity::CommodityId(material), environment))
		if(requirement.effect == CE_Required) materials[requirement.commodity.value] += requirement.quantity;
	QImage image(170, 16, QImage::Format_ARGB32); image.fill(Qt::transparent); QPainter painter(&image);
	QStringList descriptions; int x = 0;
	for(auto it = materials.cbegin(); it != materials.cend(); ++it)
	{
		const auto* commodity = catalogue->Commodity(AuLegacyCity::CommodityId(it.key())); if(!commodity) continue;
		painter.drawPixmap(x, 0, 16, 16, QPixmap(ConstructionIcon(commodity->descriptor.BitmapName()))); x += 17;
		descriptions.append(it.value() ? tr("%1 %2").arg(commodity->descriptor.m_name).arg(it.value()) : commodity->descriptor.m_name);
	}
	painter.end(); m_Recipe->setPixmap(QPixmap::fromImage(image)); m_Recipe->setToolTip(descriptions.join(QLatin1Char('\n')));
	const auto* industry = catalogue->Industry(process->industry);
	QString jobs, homes;
	if(industry)
	{
		// AuTabBuild::ListBuilding, 0x2c3360: towers have one civilian job;
		// signed staffing distinguishes fixed jobs from jobs added per level.
		const bool tower = m_Industry == 11 || m_Industry == 12;
		const int employees = industry->employees;
		if(m_Industry != 1 && m_Industry != 10 && (tower || employees))
		{
			const int base = tower ? 1 : qAbs(employees);
			if(employees < 0 && (tower || m_Industry == 128)) jobs = tr("%1 Plus %2 Troops").arg(base).arg(qAbs(employees));
			else if(employees <= 0) jobs = QString::number(base);
			else if(process->stackLimit == 0) jobs = QString::number(base + 1);
			else jobs = tr("%1 Plus 1 for Each Level").arg(base);
		}
		if(industry->populationPerLevel) homes = tr("%1 for Each Level").arg(industry->populationPerLevel);
	}
	if(jobs.isEmpty())
	{
		m_Jobs->setText(homes.isEmpty() ? QString() : tr("Homes <b>%1</b>").arg(homes));
		m_Homes->clear();
	}
	else
	{
		m_Jobs->setText(tr("Jobs <b>%1</b>").arg(jobs));
		m_Homes->setText(homes.isEmpty() ? QString() : tr("Homes <b>%1</b>").arg(homes));
	}
}

void AuLegacyCityConstruction::DescribeProducts()
{
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn(); if(!catalogue) return;
	// ListBuilding preserves the first recipe occurrence of each product.
	QList<quint16> products;
	if(m_Industry == 3) products = {113, 287, 288}; // Disks and building drawings.
	else for(const auto& process : catalogue->ManufacturingProcesses())
		if(process.industry.value == m_Industry && !products.contains(process.commodity.value)) products.append(process.commodity.value);
	QImage image(169, 50, QImage::Format_ARGB32); image.fill(Qt::transparent); QPainter painter(&image);
	QStringList descriptions; int x = 0, y = 0;
	for(quint16 id : products)
	{
		const auto* commodity = catalogue->Commodity(AuLegacyCity::CommodityId(id)); if(!commodity) continue;
		painter.drawPixmap(x, y, 16, 16, QPixmap(ConstructionIcon(commodity->descriptor.BitmapName())));
		descriptions.append(commodity->descriptor.m_name); x += 17; if(x > 168) { x = 0; y += 17; }
	}
	painter.end(); m_ProductIcons->setPixmap(QPixmap::fromImage(image)); m_ProductIcons->setToolTip(descriptions.join(QLatin1Char('\n')));
	const auto* selected = catalogue->ManufacturingProcess(AuLegacyCity::ManufacturingProcessId(quint16(m_Products->currentData().toUInt())));
	QString tooltip;
	const auto* industry = catalogue->Industry(AuLegacyCity::IndustryId(m_Industry));
	const QString name = industry ? industry->name : QString();
	if(m_Industry == 3)
		tooltip = tr("<html>%1 manufactures design disks and drawings when they are purchased. The city inventory must contain blank disks or paper.<br>Spacecraft designs are stored only onto disks. Building designs are stored onto disks or paper.<br>A person can convert a blank disk or paper into a design disk or drawing while inside a design studio.</html>").arg(name.toHtmlEscaped());
	else if(selected)
	{
		const auto* output = catalogue->Commodity(selected->commodity);
		tooltip = tr("<b>Makes %1 %2</b>").arg(selected->quantity).arg(output ? output->descriptor.m_name.toHtmlEscaped() : QString());
		QMap<quint16, quint32> requirements;
		for(const auto& component : catalogue->ComponentsForProcess(selected->id))
			if(component.effect == CE_Required) requirements[component.commodity.value] += component.quantity;
		if(!requirements.isEmpty()) tooltip += tr("<br><i>Requires</i>");
		for(auto it = requirements.cbegin(); it != requirements.cend(); ++it)
			if(const auto* commodity = catalogue->Commodity(AuLegacyCity::CommodityId(it.key())))
			{
				tooltip += QStringLiteral("<br>") + commodity->descriptor.m_name.toHtmlEscaped();
				if(it.value()) tooltip += it.key() == 157 ? tr(" %L1%2 per Tech Level").arg(quint64(it.value()) * 100).arg(g_Au.CurrencySymbol())
					: tr(" %1").arg(it.value());
			}
	}
	else if(products.isEmpty()) tooltip = tr("%1 does not manufacture anything.").arg(name);
	else tooltip = tr("Choose the product to make.<br>When a product is selected, this tooltip will show the commodities required to manufacture it.");
	m_Products->setToolTip(tooltip);
}

void AuLegacyCityConstruction::StartPlanning()
{
	const bool online = g_Au.IsLegacyGeneration() && !auApp->IsSingleUserMode() && auApp->IsSceneConnected();
	if(!online || !m_Loaded || !m_Process || !m_LevelLimit || m_PendingToken) return;
	if(m_Industry == 35)
	{ ReportMessage(tr("The original Dairy blueprint has not been recovered.")); return; }
	m_Planning = true; m_ActiveRequest = BuildRequest(); emit PlanConstruction(m_ActiveRequest);
}

void AuLegacyCityConstruction::UpdatePlanningSettings()
{
	if(!m_Planning) return;
	m_ActiveRequest.material = m_Material; m_ActiveRequest.roadSlab = m_Grading->currentData().toInt() == 1;
	m_ActiveRequest.roadWidthMeters = float(qBound(8.0, m_RoadWidth->currentText().toDouble(), 80.0));
	m_ActiveRequest.targetX = float(qBound(0.0, m_Angle->currentText().toDouble(), 90.0));
	m_ActiveRequest.targetY = float(m_Grading->currentData().toInt());
	m_ActiveRequest.manufacturingRecipe = quint16(m_Products->currentData().toUInt());
	m_ActiveRequest.citizenEnabled = m_MakeProduct->isChecked();
	emit PlanConstruction(m_ActiveRequest);
}

QPixmap AuLegacyCityConstruction::PlacementIcon(const AuLegacyCityProtocol::Request& request) const
{
	if(request.action == AuLegacyCityProtocol::ClearLand)
		return QPixmap(QStringLiteral(":data/bp_ClearLand.png"));
	if(request.industryId == 2)
		for(const auto& button : AuLegacyCityConstructionLayout::Buttons)
			if(button.industry == 2 && button.process == request.processId) return QPixmap(button.icon);
	if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
		if(const auto* material = catalogue->Commodity(AuLegacyCity::CommodityId(request.material)))
			return QPixmap(material->descriptor.BitmapName());
	return QPixmap();
}

void AuLegacyCityConstruction::SetCursorMaterial(quint16 material)
{
	if(!m_Materials->button(material)) return;
	m_Material = material;
	for(auto* button : m_Materials->buttons()) button->setDown(m_Materials->id(button) == material);
	DescribeRecipe(); UpdatePlanningSettings();
}

void AuLegacyCityConstruction::SetCursorRoadSlab(bool slab)
{
	m_Grading->setCurrentIndex(slab ? 1 : 0);
}

void AuLegacyCityConstruction::SetCursorProductChoices(const QVector<quint16>& allowed, quint16 selected)
{
	if(!m_Planning || m_ActiveRequest.industryId != m_Industry) return;
	const QSignalBlocker blocked(m_Products);
	auto* model = qobject_cast<QStandardItemModel*>(m_Products->model());
	for(int index = 0; model && index < m_Products->count(); ++index)
		if(auto* item = model->item(index))
		{
			const bool enabled = allowed.contains(quint16(m_Products->itemData(index).toUInt()));
			if(item->isEnabled() != enabled) item->setEnabled(enabled);
		}
	const int index = m_Products->findData(uint(selected));
	const bool selectionChanged = index != m_Products->currentIndex();
	if(selectionChanged) m_Products->setCurrentIndex(index);
	m_ActiveRequest.manufacturingRecipe = quint16(m_Products->currentData().toUInt());
	if(selectionChanged) DescribeProducts();
}

void AuLegacyCityConstruction::PlanningStopped()
{
	m_Planning = false;
	m_PlacementMessage.clear(); m_PlacementMessageTime.invalidate();
	if(auto* model = qobject_cast<QStandardItemModel*>(m_Products->model()))
		for(int index = 0; index < m_Products->count(); ++index)
			if(auto* item = model->item(index)) item->setEnabled(true);
}

AuLegacyCityProtocol::Request AuLegacyCityConstruction::BuildRequest() const
{
	AuLegacyCityProtocol::Request request; request.action = AuLegacyCityProtocol::Build;
	request.industryId = m_Industry; request.processId = m_Process; request.material = m_Material;
	request.designId = m_Designs->currentData().toUInt(); request.levels = 1;
	request.manufacturingRecipe = quint16(m_Products->currentData().toUInt());
	request.citizenEnabled = m_MakeProduct->isChecked();
	request.roadWidthMeters = float(qBound(8.0, m_RoadWidth->currentText().toDouble(), 80.0));
	request.roadSlab = m_Grading->currentData().toInt() == 1;
	request.targetX = float(qBound(0.0, m_Angle->currentText().toDouble(), 90.0));
	request.targetY = float(m_Grading->currentData().toInt()); return request;
}

void AuLegacyCityConstruction::Submit(AuLegacyCityProtocol::Request request)
{
	using namespace AuLegacyCityProtocol;
	if(m_PendingToken || !g_Au.IsLegacyGeneration()) return;
	if(auApp->IsSingleUserMode() || !auApp->IsSceneConnected())
	{
		ReportMessage(tr("Connect to a Legacy2016 server to construct cities."));
		UpdateControls(); return;
	}
	if(request.action == Build && request.industryId == 2
		&& (!std::isfinite(request.roadLengthMeters) || request.roadLengthMeters < MinimumRoadLengthMeters
			|| request.roadLengthMeters > MaximumRoadLengthMeters))
	{
		ReportMessage(tr("Road length must be between %1 and %2 meters.")
			.arg(MinimumRoadLengthMeters).arg(MaximumRoadLengthMeters)); return;
	}
	if(request.action == Build && request.industryId == 129
		&& (!std::isfinite(request.biodomeRadiusMeters) || request.biodomeRadiusMeters < MinimumBiodomeRadiusMeters
			|| request.biodomeRadiusMeters > MaximumBiodomeRadiusMeters))
	{
		ReportMessage(tr("Biodome radius must be between %1 and %2 meters.")
			.arg(MinimumBiodomeRadiusMeters).arg(MaximumBiodomeRadiusMeters)); return;
	}
	if(request.action == AuLegacyCityProtocol::Build && request.industryId == 1 && request.cityName.isEmpty())
	{
		AuDialog dialog(this); dialog.setAuDlgTitle(tr("New City Name"));
		auto* layout = new QVBoxLayout; layout->addWidget(new QLabel(tr("Enter a name for the city."), &dialog));
		auto* name = new QLineEdit(&dialog); name->setMaxLength(30); layout->addWidget(name);
		auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog); layout->addWidget(buttons);
		dialog.setAuDlgLayout(layout);
		connect(buttons, &QDialogButtonBox::accepted, &dialog, &AuDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, &dialog, &AuDialog::reject);
		if(auApp->Frame()->ExecDialog(dialog) != QDialog::Accepted) return;
		request.cityName = name->text().trimmed();
		if(request.cityName.isEmpty()) return;
	}
	m_PendingToken = auApp->RequestLegacyCityTool(request);
	if(m_PendingToken) m_Timeout->start();
	else ReportMessage(tr("The construction request could not be sent. Check the selected site and dimensions."));
	UpdateControls();
}

void AuLegacyCityConstruction::ReportMessage(const QString& message)
{
	if(m_Planning) { ReportPlacementMessage(message); return; }
	if(auApp->Frame() && auApp->Frame()->View()) auApp->Frame()->View()->LocalLogMessage(message);
}

void AuLegacyCityConstruction::ReportPlacementMessage(const QString& message)
{
	m_PlacementMessage = message; m_PlacementMessageTime.start();
}

QString AuLegacyCityConstruction::PlacementMessage(float& alpha) const
{
	alpha = 0;
	if(!m_PlacementMessageTime.isValid() || m_PlacementMessageTime.elapsed() >= 5000) return QString();
	// DdPlanGlobeBuilding::RenderCursor: five seconds, alpha = 1 / (1 + seconds).
	alpha = 1.f / (1.f + m_PlacementMessageTime.elapsed() / 1000.f);
	return m_PlacementMessage;
}

void AuLegacyCityConstruction::Refresh()
{
	AuLegacyCityProtocol::Request request; request.action = AuLegacyCityProtocol::ConstructionList; Submit(request);
}

void AuLegacyCityConstruction::ReceiveReply(QByteArray payload)
{
	AuLegacyCityProtocol::Reply reply;
	if(!AuLegacyCityProtocol::DecodeReply(payload, reply) || !m_PendingToken || reply.token != m_PendingToken) return;
	m_PendingToken = 0; m_Timeout->stop();
	if(!reply.accepted && !reply.message.isEmpty()) ReportMessage(reply.message);
	UpdateControls();
}

void AuLegacyCityConstruction::UpdateControls()
{
	const bool online = g_Au.IsLegacyGeneration() && !auApp->IsSingleUserMode() && auApp->IsSceneConnected();
	const bool idle = online && !m_PendingToken;
	m_Clear->setEnabled(idle); m_Stop->setEnabled(online);
	m_Designs->setEnabled(idle);
	m_MakeProduct->setEnabled(idle && m_Products->count() != 0);
	m_Products->setEnabled(idle && m_MakeProduct->isChecked() && m_Products->count() != 0);
	for(auto* button : m_Pages->findChildren<QPushButton*>()) button->setEnabled(idle && m_Loaded);
}

void AuLegacyCityConstruction::ResetTransport()
{
	const bool interrupted = m_PendingToken || m_Planning;
	PlanningStopped(); emit StopPlanning(); m_PendingToken = 0; m_Timeout->stop();
	if(interrupted && isVisible()) ReportMessage(tr("Connection changed. Select construction again after reconnecting."));
	UpdateControls();
}

void AuLegacyCityConstruction::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event); LoadCatalogue(); UpdateControls(); m_Timer->start(); Refresh();
}

void AuLegacyCityConstruction::hideEvent(QHideEvent* event)
{
	PlanningStopped(); emit StopPlanning(); m_Timer->stop(); QWidget::hideEvent(event);
}


AuLegacyCityLabor::AuLegacyCityLabor(QWidget* parent) : QWidget(parent)
{
	// AuLabor/AuLaborCity (2016, 0x1401bfdd0/0x1401ca750): a production
	// building extending a level retains Construction and Manufacture tabs.
	auto* layout = new QGridLayout(this); layout->setMargin(2); layout->setSpacing(0);
	layout->setSizeConstraint(QLayout::SetMinimumSize);
	m_LaborTabs = new QTabBar(this);
	m_LaborTabs->addTab(QIcon(":data/ConstructionSign.png"), tr("Construction"));
	m_LaborTabs->addTab(QIcon(":data/HammerAndSickle.png"), tr("Manufacture"));
	layout->addWidget(m_LaborTabs, 0, 0); m_LaborTabs->hide();
	m_LaborPages = new QStackedWidget(this); layout->addWidget(m_LaborPages, 1, 0);
	m_Labor = new AuBldConstruct(this, true); m_LaborPages->addWidget(m_Labor);
	m_Manufacture = new AuLegacyCityManufacture(this); m_LaborPages->addWidget(m_Manufacture);
	m_Bank = new AuLegacyCityBank(this); m_LaborPages->addWidget(m_Bank);
	m_Studio = new AuLegacyCityDesignStudio(this); m_LaborPages->addWidget(m_Studio);
	m_Info = new QLabel(tr("<html><p>Use this window to use the buildings and roads in a city.</p>"
		"<p>The contents of this window change based on the building or road site you occupy. "
		"You do not have to be inside a building; stand anywhere at the site.</p><p><i>Tips</i></p>"
		"%1 Go to a building or road that is under construction to help build it.<br>"
		"%1 Go to a building that makes things to change what it makes and make more of them.<br>"
		"%1 <b>Airport Terminals</b> have a matter transporter that can beam people to and from the local area and orbiting spacecraft. They also have a sensor station that scans nearby objects.<br>"
		"%1 <b>Arenas</b> simulate solo or multiplayer spacecraft battles.<br>"
		"%1 <b>Banks</b> provide loans and other services.<br>"
		"%1 <b>Brokers</b> buy and sell almost everything.<br>"
		"%1 <b>Cantinas</b> buy and sell food and drinks.<br>"
		"%1 <b>Capitol Buildings</b> control all construction and manufacturing in a city from one place.<br>"
		"%1 <b>Design Studios</b> enable creation of new designs of buildings and spacecraft.<br>"
		"%1 <b>Groceries</b> buy and sell food, drinks, and medical supplies.<br>"
		"%1 <b>Hangars</b> store spacecraft.<br>"
		"%1 <b>Observatories</b> have a navigation station to view the worlds in solar systems.<br>"
		"%1 <b>Retail Stores</b> buy and sell a wide range of things: clothing, tools, weapons, ammunition, construction supplies, spare parts, fuel, and more.<br>"
		"%1 <b>Spacecraft Factories</b> make and sell spacecraft.<br></html>").arg(g_Au.MiddleDotSymbol()), this);
	m_Info->setWordWrap(true); m_Info->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_LaborPages->addWidget(m_Info);
	connect(m_LaborTabs, &QTabBar::currentChanged, this, [this](int index) {
		if(m_DualPages && index >= 0) m_LaborPages->setCurrentIndex(m_LaborTabs->tabData(index).toInt());
	});
	connect(m_Manufacture, &AuLegacyCityManufacture::AvailabilityChanged, this, [this](bool) { UpdateControls(); });
	m_Timer = new QTimer(this); m_Timer->setInterval(1000);
	m_StudioRequestTimer = new QTimer(this); m_StudioRequestTimer->setSingleShot(true); m_StudioRequestTimer->setInterval(250);
	connect(m_Studio, &AuLegacyCityDesignStudio::Enter, this,
		[this](quint32 city, quint32 development, int index, const QString& password) {
			AuLegacyCityProtocol::Request request; request.action = AuLegacyCityProtocol::EnterSpacecraftStudio;
			request.cityId = city; request.developmentId = development; request.studioIndex = quint8(index);
			request.studioPassword = password;
			QueueStudioRequest(request);
		});
	connect(m_StudioRequestTimer, &QTimer::timeout, this, [this]() {
		if(!m_StudioRequestQueued) return;
		if(m_PendingToken) { m_StudioRequestTimer->start(); return; }
		const auto request = m_QueuedStudioRequest; m_StudioRequestQueued = false;
		const auto* entry = Selected();
		if(!isVisible() || !entry || entry->cityId != request.cityId || entry->developmentId != request.developmentId)
		{ UpdateControls(); return; }
		Submit(request);
	});
	connect(m_Timer, &QTimer::timeout, this, [this]() {
		// The shared labor page runs its own work timer while visible. Keep
		// following the avatar when the information page is visible instead.
		if(!m_Labor->isVisible()) Refresh();
	});
	m_Timeout = new QTimer(this); m_Timeout->setSingleShot(true); m_Timeout->setInterval(60000);
	connect(m_Timeout, &QTimer::timeout, this, [this]() {
		const bool studioEntry = m_PendingRequest.action == AuLegacyCityProtocol::EnterSpacecraftStudio;
		if(studioEntry) m_AbandonedStudioToken = m_PendingToken;
		m_PendingToken = 0; m_Labor->StopWorking();
		m_HasDeferredCitizenRequest = false;
		if(studioEntry) emit auApp->LegacyStudioEntryFinished(false);
		ReportMessage(studioEntry ? tr("No server response from the design studio.")
			: tr("No server response from the construction site.")); UpdateControls();
	});
	connect(m_Labor, &AuBldConstruct::LegacyUpdateRequested, this, [this](bool working) {
		if(!isVisible() || m_PendingToken) return;
		if(working) Operate(AuLegacyCityProtocol::Contribute); else Refresh();
	});
	connect(m_Labor, &AuBldConstruct::LegacyActionRequested, this, [this](quint8 action, bool enabled) {
		m_CitizenEnabled = enabled; Operate(static_cast<AuLegacyCityProtocol::Action>(action));
	});
	connect(auApp, &AuApp::LegacyCityReplyReceived, this, &AuLegacyCityLabor::ReceiveReply);
	connect(auApp, &AuApp::LegacyScTransportReset, this, &AuLegacyCityLabor::ResetTransport);
	UpdateControls();
}

const AuLegacyCityProtocol::Entry* AuLegacyCityLabor::Selected() const
{
	return m_Selected >= 0 && m_Selected < m_Entries.size() ? &m_Entries[m_Selected] : nullptr;
}

void AuLegacyCityLabor::Refresh()
{
	if(m_StudioRequestQueued) return;
	AuLegacyCityProtocol::Request request; request.action = AuLegacyCityProtocol::ConstructionList; Submit(request);
}

void AuLegacyCityLabor::Submit(AuLegacyCityProtocol::Request request)
{
	if(m_PendingToken || !g_Au.IsLegacyGeneration() || auApp->IsSingleUserMode() || !auApp->IsSceneConnected()) return;
	if(m_StudioRequestQueued && m_QueuedStudioRequest.action == AuLegacyCityProtocol::EnterSpacecraftStudio
		&& request.action != AuLegacyCityProtocol::EnterSpacecraftStudio) return;
	m_PendingRequest = request;
	const bool studioEntry = request.action == AuLegacyCityProtocol::EnterSpacecraftStudio;
	if(studioEntry) emit auApp->LegacyStudioEntryStarted();
	m_PendingToken = auApp->RequestLegacyCityTool(request);
	if(m_PendingToken) m_Timeout->start();
	else
	{
		m_Labor->StopWorking();
		if(studioEntry) emit auApp->LegacyStudioEntryFinished(false);
	}
	UpdateControls();
}

void AuLegacyCityLabor::QueueStudioRequest(AuLegacyCityProtocol::Request request)
{
	if(m_StudioRequestQueued && m_QueuedStudioRequest.action == AuLegacyCityProtocol::EnterSpacecraftStudio) return;
	if(m_PendingToken && m_PendingRequest.action == AuLegacyCityProtocol::EnterSpacecraftStudio) return;
	// Offset from the occupied-site poll to respect the server's shared city
	// request rate gate. A click takes priority over another status refresh.
	m_QueuedStudioRequest = request; m_StudioRequestQueued = true; m_StudioRequestTimer->start();
	UpdateControls();
}

void AuLegacyCityLabor::Operate(AuLegacyCityProtocol::Action action)
{
	const auto* entry = Selected(); if(!entry) return;
	AuLegacyCityProtocol::Request request; request.action = action; request.cityId = entry->cityId;
	request.developmentId = entry->developmentId; request.citizenEnabled = m_CitizenEnabled;
	if(action == AuLegacyCityProtocol::EmployCitizen && m_PendingToken)
	{
		// Preserve the latest choice and its original target until the current
		// request finishes, instead of disabling the checkbox or dropping clicks.
		m_DeferredCitizenRequest = request; m_HasDeferredCitizenRequest = true;
		UpdateControls();
		return;
	}
	Submit(request);
}

void AuLegacyCityLabor::ReceiveReply(QByteArray payload)
{
	AuLegacyCityProtocol::Reply reply;
	if(!AuLegacyCityProtocol::DecodeReply(payload, reply)) return;
	if(reply.token == m_AbandonedStudioToken && reply.action == AuLegacyCityProtocol::EnterSpacecraftStudio)
	{
		m_AbandonedStudioToken = 0;
		if(reply.accepted) auApp->DesignExit();
		return;
	}
	if(!m_PendingToken || reply.token != m_PendingToken) return;
	m_PendingToken = 0; m_Timeout->stop();
	if(AuLegacyCityProtocol::IsStudio(reply.action))
	{
		if(reply.hasStudio && reply.selectedCity == m_StudioCityId && reply.selectedDevelopment == m_StudioDevelopmentId)
		{
			m_StudioStateValid = true;
			m_StudioLevelCount = reply.studioCount;
			m_Studio->SetTarget(reply.selectedCity, reply.selectedDevelopment, reply.studioCount);
			for(int index = 0; index < reply.studioCount; ++index)
				m_Studio->SetOccupied(index, (reply.occupiedStudios & (1u << index)) != 0);
		}
		if(reply.action == AuLegacyCityProtocol::EnterSpacecraftStudio)
		{
			if(!reply.accepted && !reply.message.isEmpty()) ReportMessage(reply.message);
			emit auApp->LegacyStudioEntryFinished(reply.accepted);
		}
		UpdateControls();
		return;
	}
	if(reply.action == AuLegacyCityProtocol::Contribute && !reply.accepted) m_Labor->StopWorking();
	quint32 city = reply.selectedCity, development = reply.selectedDevelopment;
	if(!city && reply.action != AuLegacyCityProtocol::ConstructionList)
		if(const auto* previous = Selected()) { city = previous->cityId; development = previous->developmentId; }
	if(reply.hasList)
	{
		const auto* previous = Selected();
		const bool changedTarget = !previous || previous->cityId != city || previous->developmentId != development;
		if(changedTarget) m_Labor->StopWorking();
		m_Entries = reply.entries; m_Selected = -1;
		for(int index = 0; index < m_Entries.size(); ++index)
		{
			const auto& entry = m_Entries[index];
			if(entry.cityId == city && entry.developmentId == development) { m_Selected = index; break; }
		}
	}
	// Polling is silent. Report action feedback once, rather than generating
	// another Thoughts message each second while waiting for materials.
	if(reply.action != AuLegacyCityProtocol::ConstructionList)
	{
		if(!reply.message.isEmpty() && reply.message != m_LastMessage) ReportMessage(reply.message);
		m_LastMessage = reply.message;
	}
	if(m_HasDeferredCitizenRequest)
	{
		const auto request = m_DeferredCitizenRequest;
		m_HasDeferredCitizenRequest = false;
		Submit(request);
	}
	UpdateControls();
	if(reply.action == AuLegacyCityProtocol::ConstructionList && isVisible() && m_StudioCityId)
	{
		AuLegacyCityProtocol::Request request; request.action = AuLegacyCityProtocol::StudioState;
		request.cityId = m_StudioCityId; request.developmentId = m_StudioDevelopmentId;
		QueueStudioRequest(request);
	}
}

void AuLegacyCityLabor::UpdateControls()
{
	const bool online = g_Au.IsLegacyGeneration() && !auApp->IsSingleUserMode() && auApp->IsSceneConnected();
	const auto* entry = Selected();
	const bool unfinished = entry && UnderConstruction(*entry);
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	DeConstructionProcess process;
	if(entry)
	{
		process.SetLabor(entry->siteLaborRemaining + entry->levelLaborRemaining);
		if(catalogue)
			for(const auto& component : entry->components)
			{
				const auto runtimeId = catalogue->RuntimeCommodityId(AuLegacyCity::CommodityId{component.commodityId});
				if(!runtimeId) continue;
				DeProcessComponent converted; converted.SetCommodity(COMMODITYID(runtimeId), 1); converted.SetQuality(component.quality);
				converted.SetEffect(COMPEFFECT(component.effect)); converted.SetQtyConsumed(component.quantity); converted.SetQtyApplied(component.applied);
				process.Components().append(converted);
			}
		m_CitizenEnabled = entry->citizenEnabled;
		// A reply to an earlier poll/work request must not undo a newer choice
		// while its employment request is waiting or being applied by the server.
		if(m_PendingToken && m_PendingRequest.action == AuLegacyCityProtocol::EmployCitizen
			&& m_PendingRequest.cityId == entry->cityId && m_PendingRequest.developmentId == entry->developmentId)
			m_CitizenEnabled = m_PendingRequest.citizenEnabled;
		if(m_HasDeferredCitizenRequest && m_DeferredCitizenRequest.cityId == entry->cityId
			&& m_DeferredCitizenRequest.developmentId == entry->developmentId)
			m_CitizenEnabled = m_DeferredCitizenRequest.citizenEnabled;
	}
	const bool designStudio = online && entry && entry->completedLevels && entry->industryId == 3;
	m_Timer->setInterval(designStudio ? 500 : 1000);
	const quint32 studioCity = designStudio ? entry->cityId : 0;
	const quint32 studioDevelopment = designStudio ? entry->developmentId : 0;
	if(studioCity != m_StudioCityId || studioDevelopment != m_StudioDevelopmentId) m_StudioStateValid = false;
	m_StudioCityId = studioCity; m_StudioDevelopmentId = studioDevelopment;
	m_Studio->SetTarget(studioCity, studioDevelopment,
		m_StudioStateValid ? m_StudioLevelCount : designStudio ? entry->completedLevels : 0);
	m_Studio->setEnabled(m_StudioStateValid
		&& !(m_StudioRequestQueued && m_QueuedStudioRequest.action == AuLegacyCityProtocol::EnterSpacecraftStudio)
		&& !(m_PendingToken && m_PendingRequest.action == AuLegacyCityProtocol::EnterSpacecraftStudio));
	m_Manufacture->SetTarget(entry && !designStudio ? entry->cityId : 0, entry && !designStudio ? entry->developmentId : 0);
	// Hidden production pages still receive the occupied site's snapshot.
	// This also discovers processes when the server initializes a new level.
	if(online && entry && !designStudio && !m_Manufacture->isVisible()) m_Manufacture->Refresh();
	QString name;
	if(entry)
	{
		const auto* recipe = catalogue ? catalogue->ConstructionProcess(AuLegacyCity::ConstructionProcessId{entry->processId}) : nullptr;
		const auto* industry = catalogue ? catalogue->Industry(AuLegacyCity::IndustryId{entry->industryId}) : nullptr;
		name = industry ? industry->name : tr("Development");
		if(recipe && !recipe->name.isEmpty() && recipe->name != name)
			name = recipe->name.endsWith(QStringLiteral("Area")) ? recipe->name : recipe->name + QLatin1Char(' ') + name;
		name = QStringLiteral("<b>%1</b>").arg(name.toHtmlEscaped());
	}
	m_Labor->SetLegacyTarget(name, process, online && unfinished, entry && m_CitizenEnabled, entry && entry->halted);
	const bool manufacturing = online && m_Manufacture->HasIndustry();
	const bool banking = online && entry && entry->completedLevels && (entry->industryId == 100 || entry->industryId == 123);
	m_Bank->SetTarget(banking ? entry->cityId : 0, banking ? entry->developmentId : 0, banking && entry->industryId == 123);
	const int previousPage = m_LaborPages->currentIndex();
	{
		const QSignalBlocker blocked(m_LaborTabs);
		while(m_LaborTabs->count()) m_LaborTabs->removeTab(0);
		const auto addPage = [this](QWidget* page, const QString& title) {
			const int tab = m_LaborTabs->addTab(title); m_LaborTabs->setTabData(tab, m_LaborPages->indexOf(page));
		};
		if(online && unfinished) addPage(m_Labor, tr("Construction"));
		if(manufacturing) addPage(m_Manufacture, tr("Manufacture"));
		if(banking) addPage(m_Bank, entry->industryId == 123 ? tr("Government") : tr("Bank"));
		if(designStudio) addPage(m_Studio, tr("Design Studio"));
		for(int tab = 0; tab < m_LaborTabs->count(); ++tab)
			if(m_LaborTabs->tabData(tab).toInt() == previousPage) m_LaborTabs->setCurrentIndex(tab);
	}
	m_DualPages = m_LaborTabs->count() > 1; m_LaborTabs->setVisible(m_DualPages);
	if(m_LaborTabs->count()) m_LaborPages->setCurrentIndex(m_LaborTabs->tabData(m_LaborTabs->currentIndex()).toInt());
	else m_LaborPages->setCurrentWidget(m_Info);
	if(!online) m_Labor->StopWorking();
}

void AuLegacyCityLabor::ResetTransport()
{
	const bool studioEntry = m_PendingToken && m_PendingRequest.action == AuLegacyCityProtocol::EnterSpacecraftStudio;
	m_Labor->StopWorking(); m_PendingToken = 0; m_Timeout->stop(); m_Entries.clear(); m_Selected = -1;
	m_StudioRequestTimer->stop(); m_StudioRequestQueued = m_StudioStateValid = false; m_AbandonedStudioToken = 0;
	if(studioEntry) emit auApp->LegacyStudioEntryFinished(false);
	m_HasDeferredCitizenRequest = false;
	m_LastMessage.clear(); UpdateControls();
}

void AuLegacyCityLabor::ReportMessage(const QString& message)
{
	if(auApp->Frame() && auApp->Frame()->View()) auApp->Frame()->View()->LocalLogMessage(message);
}

void AuLegacyCityLabor::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event); QString error;
	if(!AuLegacyCityCatalogue::LoadBuiltIn(error)) ReportMessage(error);
	UpdateControls(); m_Timer->start(); Refresh();
}

void AuLegacyCityLabor::hideEvent(QHideEvent* event)
{
	m_Labor->StopWorking(); m_Timer->stop(); QWidget::hideEvent(event);
}

AuLegacyCityConstructionDock::AuLegacyCityConstructionDock(QWidget* parent)
	: AuDockWidget(":data/ConstructionSign.png", tr("<b>Construction</b>"), tr("Construct"), parent)
{
	setObjectName(QStringLiteral("LegacyCityConstruction")); m_Content = new AuLegacyCityConstruction(this); setWidget(m_Content);
	connect(this, &QDockWidget::visibilityChanged, this, [this](bool visible) { if(visible && !g_Au.IsLegacyGeneration()) hide(); });
}

void AuLegacyCityConstructionDock::UpdateTitleShortcut()
{
	const QString binding = auApp->Frame()->ControlBinding(AuFrame::CI_ToggleConstructionUI);
	SetTitle(binding.isEmpty() ? OriginalTitle() : OriginalTitle() + QStringLiteral(" (%1)").arg(binding));
}

AuLegacyCityLaborDock::AuLegacyCityLaborDock(QWidget* parent)
	: AuDockWidget(":data/MsgCity.png", tr("<b>City</b>"), tr("City"), parent)
{
	setObjectName(QStringLiteral("LegacyCityLabor")); setWidget(new AuLegacyCityLabor(this));
	connect(this, &QDockWidget::visibilityChanged, this, [this](bool visible) { if(visible && !g_Au.IsLegacyGeneration()) hide(); });
}

void AuLegacyCityLaborDock::UpdateTitleShortcut()
{
	const QString binding = auApp->Frame()->ControlBinding(AuFrame::CI_ToggleBuildingUI);
	SetTitle(binding.isEmpty() ? OriginalTitle() : OriginalTitle() + QStringLiteral(" (%1)").arg(binding));
}
