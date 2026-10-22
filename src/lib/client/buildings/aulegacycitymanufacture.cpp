#include "aulegacycitymanufacture.h"
#include "auapp.h"
#include "auether.h"
#include "auframe.h"
#include "auglobal.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityinfrastructure.h"
#include "aulegacycityproduction.h"
#include "auview.h"
#include "dalegacycity.h"
#include "daperson.h"
#include "deprocesscomponent.h"
#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QGridLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

namespace
{
bool CanConfigureIndustry(quint32 cityId)
{
	auto* view = auApp->Frame() ? auApp->Frame()->View() : nullptr;
	if(!view) return false;
	DaPerson* person = nullptr; view->LockSceneAtom(&person);
	bool authorized = false;
	if(person && person->EmpireId() && person->Ether())
		if(const auto* city = dynamic_cast<const DaLegacyCity*>(person->Ether()->FindNormalAtom(AuId(cityId))))
			authorized = person->EmpireId() == city->EmpireId();
	view->UnlockSceneAtom();
	return authorized;
}
}

class AuLegacyManufacturePanel : public QWidget
{
	QComboBox *m_Recipe, *m_Technology;
	QCheckBox* m_Employ;
	QPushButton *m_Fetch, *m_Run;
	QListWidget* m_Materials;
	QTableWidget* m_Modifiers;
	QLabel *m_StatusIcon, *m_Status, *m_LastOutput;
	AuLegacyCity::ProductionProcess m_Process;
	quint32 m_CityId = 0, m_DevelopmentId = 0;
	quint8 m_BuildingTechnology = 0;
	QString m_ProductName;
	bool m_RunPending = false;
	std::function<void(AuLegacyCityProtocol::Request)> m_Send;

	void ResizeModifierColumns()
	{
		m_Modifiers->resizeColumnsToContents();
		m_Modifiers->setFixedWidth(m_Modifiers->horizontalHeader()->length() + 2 * m_Modifiers->frameWidth()
			+ (m_Modifiers->verticalScrollBar()->maximum() > m_Modifiers->verticalScrollBar()->minimum()
				? m_Modifiers->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, m_Modifiers) : 0));
	}

	void Send(AuLegacyCityProtocol::Action action, bool selectRecipe = false, bool selectTechnology = false)
	{
		if(action == AuLegacyCityProtocol::RunIndustryProcess)
		{
			if(m_RunPending || m_Process.completeAt) return;
			// Block duplicate starts immediately, without locking another row or
			// inventing a completion time before the server accepts this effort.
			m_RunPending = true; UpdateCountdown();
		}
		AuLegacyCityProtocol::Request request;
		request.action = action; request.cityId = m_CityId; request.developmentId = m_DevelopmentId;
		request.processSlot = m_Process.slot;
		// A filtered menu can omit the stored recipe. Other controls must
		// still address that process, not the menu's first available choice.
		request.manufacturingRecipe = m_Process.processId;
		request.minimumTechnology = selectTechnology ? quint8(m_Technology->currentData().toUInt()) : m_Process.minimumTechnology;
		if(selectTechnology && (m_Technology->currentIndex() < 0
			|| request.minimumTechnology == m_Process.minimumTechnology)) return;
		if(selectRecipe)
		{
			if(m_Recipe->currentIndex() < 0) return;
			request.manufacturingRecipe = quint16(m_Recipe->currentData().toUInt());
			if(request.manufacturingRecipe == m_Process.processId) return;
			if(const auto* catalogue = AuLegacyCityCatalogue::BuiltIn())
				if(const auto* recipe = catalogue->ManufacturingProcess(AuLegacyCity::ManufacturingProcessId(request.manufacturingRecipe)))
					request.minimumTechnology = recipe->minimumTechnology;
		}
		request.citizenEnabled = m_Employ->isChecked();
		m_Send(request);
	}

public:
	AuLegacyManufacturePanel(QWidget* parent,
		std::function<void(AuLegacyCityProtocol::Request)> send) : QWidget(parent), m_Send(std::move(send))
	{
		// AuManufacturePanel, original executable 0x1401d16d0. Keep its
		// controls and grid rather than presenting modern workshop settings.
		m_Recipe = new QComboBox(this);
		m_Technology = new QComboBox(this);
		m_Employ = new QCheckBox(tr("Employ Citizen"), this);
		m_Fetch = new QPushButton(tr("Fetch"), this);
		m_Run = new QPushButton(tr("Run"), this);
		m_StatusIcon = new QLabel(this); m_Status = new QLabel(this); m_LastOutput = new QLabel(this);
		m_LastOutput->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		m_LastOutput->setToolTip(tr("<html>Shows quality and amount of commodities made in the last run of the process. "
			"Quality ranges from 1 to 255. A reason is displayed when a process fails to complete properly.<br>"
			"Materials are not consumed when the output cannot be stored in the city's inventory.</html>"));
		const QString statusHelp = tr("<html>Shows the current status of the manufacturing process.</html>");
		m_Status->setToolTip(statusHelp); m_StatusIcon->setToolTip(statusHelp);
		m_Recipe->setToolTip(tr("<html>Selects a manufacturing process to run. "
			"You must have governing authority to select a different process.</html>"));
		m_Technology->setToolTip(tr("<html><p>Sets the minimum production tech level.</p>"
			"<p>Establishes the minimum tech level for commodities consumed by the process, those listed in the first column. "
			"Tech level of tools and other modifiers, listed in the second column, do not affect production tech level.</p>"
			"<p>Does not affect commodities fetched from the environment, as done by farming, logging, mining, drilling, "
			"refining, etc. Tech level of those commodities is determined by the tech level of the building, "
			"limited by the quality of the local natural resources.</p><p>Not applicable to RnD processes.</p></html>"));
		m_Employ->setToolTip(tr("<html><b>Employ Citizen</b><br>Enables citizens of the city and players to fetch materials "
			"and to run the process. Citizens do not change the manufacturing process.<br>When disabled, players must be "
			"in the chain-of-command of the city to fetch materials or run the process.<br>"
			"You must have governing authority to change this setting.</html>"));
		m_Fetch->setToolTip(tr("<html>Fetches commodities from the city's inventory that are used by the manufacturing "
			"process. Commodities that can be used from your ready inventory, such as tools, "
			"are not fetched until the process is run.</html>"));
		m_Run->setToolTip(tr("<html>Runs the manufacturing process. The process will not run until all needed components "
			"are available. That means all commodities consumed by the process must be fetched and all commodities "
			"<i>required</i> by the process must be in the city's inventory or your ready inventory.<br>"
			"%1 <b>Run</b> performs a <b>Fetch</b> first; you don't actually have to push the <b>Fetch</b> button.<br>"
			"%1 Tools in your ready inventory are used when <b>Run</b> is pressed.</html>").arg(AuGlobal::MiddleDotSymbol()));
		m_Materials = new QListWidget(this); m_Materials->setMinimumHeight(10);
		m_Materials->setToolTip(tr("<html>Commodities consumed by the manufacturing process. "
			"All consumed commodities must be fetched before the process will run.</html>"));
		m_Modifiers = new QTableWidget(0, 3, this);
		m_Modifiers->setMinimumHeight(10);
		m_Modifiers->setEditTriggers(QAbstractItemView::NoEditTriggers);
		m_Modifiers->setSelectionBehavior(QAbstractItemView::SelectRows);
		m_Modifiers->setSelectionMode(QAbstractItemView::SingleSelection);
		m_Modifiers->setShowGrid(false); m_Modifiers->horizontalHeader()->hide(); m_Modifiers->verticalHeader()->hide();
		m_Modifiers->verticalHeader()->setDefaultSectionSize(18);
		connect(m_Modifiers->verticalScrollBar(), &QScrollBar::rangeChanged, this,
			[this](int, int) { ResizeModifierColumns(); });
		m_Modifiers->setToolTip(tr("<html>Commodities that affect the manufacturing process without being consumed by "
			"the process. Though not directly consumed, these commodities may wear out when used by the process.<br>"
			"<i>Required</i> commodities must be available in your ready inventory or the city's inventory "
			"before the process will run.</html>"));
		auto* grid = new QGridLayout(this);
		grid->addWidget(m_Recipe, 0, 0, 1, 3); grid->addWidget(m_Fetch, 0, 3); grid->addWidget(m_Run, 0, 4);
		grid->addWidget(m_Employ, 1, 0, 1, 3); grid->addWidget(m_Technology, 1, 3, 1, 2);
		grid->addWidget(m_StatusIcon, 2, 0); grid->addWidget(m_Status, 2, 1); grid->addWidget(m_LastOutput, 2, 2, 1, 3);
		grid->setRowStretch(3, 1); grid->addWidget(m_Materials, 0, 5, 4, 1); grid->addWidget(m_Modifiers, 0, 6, 4, 1);
		grid->setColumnStretch(1, 3); grid->setColumnStretch(5, 2); grid->setSpacing(2); grid->setMargin(2);
		connect(m_Recipe, QOverload<int>::of(&QComboBox::activated), this, [this](int) { Send(AuLegacyCityProtocol::SetIndustryProcess, true); });
		connect(m_Technology, QOverload<int>::of(&QComboBox::activated), this, [this](int) { Send(AuLegacyCityProtocol::SetIndustryProcess, false, true); });
		connect(m_Employ, &QCheckBox::toggled, this, [this](bool) { Send(AuLegacyCityProtocol::SetIndustryProcess); });
		connect(m_Fetch, &QPushButton::clicked, this, [this]() { Send(AuLegacyCityProtocol::FetchIndustryMaterials); });
		connect(m_Run, &QPushButton::clicked, this, [this]() { Send(AuLegacyCityProtocol::RunIndustryProcess); });
	}

	void Update(const AuLegacyCityCatalogue& catalogue, quint32 cityId, quint32 developmentId,
		quint8 industryId, quint8 buildingTechnology, const AuLegacyCity::ProductionProcess& process, bool canConfigure,
		bool hasRecipeChoices, const QVector<quint16>& recipeChoices, bool preserveSelections = false)
	{
		if(m_CityId != cityId || m_DevelopmentId != developmentId || m_Process.slot != process.slot) m_RunPending = false;
		m_Process = process; m_CityId = cityId; m_DevelopmentId = developmentId;
		m_BuildingTechnology = buildingTechnology;
		const auto* recipe = catalogue.ManufacturingProcess(AuLegacyCity::ManufacturingProcessId(process.processId));
		m_ProductName.clear();
		if(recipe)
			if(const auto* product = catalogue.Commodity(recipe->commodity)) m_ProductName = product->descriptor.m_name;
		// Keep open/queued selections intact, but never skip authoritative
		// running state, countdown, component readiness or button updates.
		preserveSelections = preserveSelections || m_Recipe->view()->isVisible() || m_Technology->view()->isVisible();
		const QSignalBlocker blockedRecipe(m_Recipe), blockedTechnology(m_Technology), blockedEmploy(m_Employ);
		if(!preserveSelections)
		{
			m_Recipe->clear();
			for(const auto id : catalogue.ProcessesForIndustry(AuLegacyCity::IndustryId(industryId)))
			{
				if(hasRecipeChoices && !recipeChoices.contains(id.value)) continue;
				if(const auto* recipe = catalogue.ManufacturingProcess(id))
				{
					const auto* commodity = catalogue.Commodity(recipe->commodity);
					m_Recipe->addItem(commodity ? QIcon(commodity->descriptor.BitmapName()) : QIcon(), recipe->name, id.value);
				}
			}
			const int selectedRecipe = m_Recipe->findData(process.processId);
			if(selectedRecipe >= 0) m_Recipe->setCurrentIndex(selectedRecipe);
			m_Technology->clear();
			if(recipe)
				for(int level = recipe->minimumTechnology; level <= std::max(recipe->minimumTechnology, buildingTechnology); ++level)
					m_Technology->addItem(tr("Production TL%1").arg(level + 1), level);
			const int selectedTechnology = m_Technology->findData(process.minimumTechnology);
			if(selectedTechnology >= 0) m_Technology->setCurrentIndex(selectedTechnology);
			m_Employ->setChecked(process.enabled);
		}
		m_Materials->clear(); m_Modifiers->setRowCount(0);
		for(const auto& value : process.components)
		{
			// Reuse the surviving component descriptor for names, effect icons
			// and consumed/modifier classification, with explicit GD conversion.
			DeProcessComponent component;
			component.SetCommodity(COMMODITYID(catalogue.RuntimeCommodityId(AuLegacyCity::CommodityId(value.commodityId))), value.quality);
			component.SetEffect(COMPEFFECT(value.effect)); component.SetQtyConsumed(value.required); component.SetQtyApplied(value.applied);
			const auto* descriptor = catalogue.Commodity(AuLegacyCity::CommodityId(value.commodityId));
			const QString name = descriptor ? descriptor->descriptor.m_name : QString::number(value.commodityId);
			const QIcon icon(component.CommodityBitmapName());
			if(value.effect == 5 && value.required)
			{
				QString text = !value.applied ? tr("%1 needed").arg(value.required)
					: value.applied == value.required ? QString::number(value.applied) : tr("%1 of %2").arg(value.applied).arg(value.required);
				if(value.applied && value.quality) text += tr(" TL%1 Q%2").arg(value.technology + 1).arg(value.quality);
				auto* item = new QListWidgetItem(icon, text, m_Materials); item->setToolTip(name);
			}
			else
			{
				// The original panel prepends each modifier, reversing GD order.
				const int row = 0; m_Modifiers->insertRow(row);
				auto* item = new QTableWidgetItem(icon, QString()); item->setToolTip(name); m_Modifiers->setItem(row, 0, item);
				QString effect;
				switch(value.effect)
				{
				case 1: case 4: effect = tr("Does Nothing"); break;
				case 2: effect = tr("Decreases Time"); break;
				case 3: effect = tr("Decreases Materials"); break;
				case 5: effect = tr("Required"); break;
				case 6: effect = tr("Increases Output"); break;
				}
				const char* effectIcon = value.effect == 1 || value.effect == 4 ? nullptr : component.EffectBitmapName();
				item = new QTableWidgetItem(effectIcon ? QIcon(QString::fromLatin1(effectIcon)) : QIcon(), QString());
				item->setToolTip(tr("%1 %2").arg(name, effect)); m_Modifiers->setItem(row, 1, item);
				item = new QTableWidgetItem;
				if(value.applied)
				{
					item->setIcon(QIcon(":data/mpReady.png")); item->setToolTip(tr("%1 Ready").arg(name));
				}
				else if(value.effect == 5)
				{
					item->setIcon(QIcon(":data/mpNotReady.png")); item->setToolTip(tr("%1 Needed").arg(name));
				}
				m_Modifiers->setItem(row, 2, item);
			}
		}
		ResizeModifierColumns();
		UpdateCountdown();
		if(process.lastQuantity >= 0 && (process.lastQuantity || process.lastQuality))
			m_LastOutput->setText(tr("\nTL%3 Q%1x%2").arg(process.lastQuality).arg(process.lastQuantity).arg(process.lastTechnology + 1));
		else if(process.lastQuantity == -1) m_LastOutput->setText(tr("\nNo Empty Spawn Location"));
		else if(process.lastQuantity == -2) m_LastOutput->setText(tr("\nNo Live Animal Or Animal DNA Sample Present"));
		else if(process.lastQuantity == -3) m_LastOutput->setText(tr("\nInventory Full"));
		else if(process.lastQuantity == -4) m_LastOutput->setText(tr("\nObsolete Product Discarded"));
		else if(process.lastQuantity == -99) m_LastOutput->setText(tr("\nUnimplemented Game Object"));
		else if(process.lastQuantity < 0) m_LastOutput->setText(tr("\nNo Output"));
		else m_LastOutput->clear();
		m_Recipe->setEnabled(canConfigure); m_Employ->setEnabled(canConfigure);
		m_Technology->setEnabled(canConfigure && recipe && !AuLegacyCity::IsProductionTechnology(recipe->commodity.value));
	}

	void FinishRequest(const AuLegacyCityProtocol::Request& request)
	{
		if(request.action == AuLegacyCityProtocol::RunIndustryProcess && request.cityId == m_CityId
			&& request.developmentId == m_DevelopmentId && request.processSlot == m_Process.slot) ClearPendingRun();
	}

	void ClearPendingRun() { m_RunPending = false; UpdateCountdown(); }

	void UpdateCountdown()
	{
		const auto& process = m_Process;
		const qint64 now = QDateTime::currentMSecsSinceEpoch() + g_Au.ServerTimeOffset();
		// SecondsRemaining subtracts whole epoch seconds in the 2016 process.
		const qint64 remaining = process.completeAt > now ? process.completeAt / 1000 - now / 1000 : 0;
		const bool running = process.completeAt != 0;
		const bool technologyAllowed = process.minimumTechnology <= m_BuildingTechnology;
		const bool ready = technologyAllowed && AuLegacyCity::CanRunProduction(process);
		const auto timedStatus = [](const QString& minutesText, const QString& hoursText, qint64 seconds) {
			return seconds < 3600
				? minutesText.arg(seconds / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'))
				: hoursText.arg(seconds / 3600).arg(seconds / 60 % 60, 2, 10, QLatin1Char('0'))
					.arg(seconds % 60, 2, 10, QLatin1Char('0'));
		};
		QString status;
		if(running) status = timedStatus(tr("Running %1:%2"), tr("Running %1:%2:%3"), remaining);
		else if(!technologyAllowed)
			status = tr("Process TL%1 Exceeds Building TL%2.").arg(process.minimumTechnology + 1).arg(m_BuildingTechnology + 1);
		else if(ready) status = timedStatus(tr("Ready %1:%2"), tr("Ready %1:%2:%3"), (quint64(process.laborNeeded) + 3) / 4);
		else status = timedStatus(tr("Not Ready %1:%2"), tr("Not Ready %1:%2:%3"), (quint64(process.laborNeeded) + 3) / 4);
		if(!m_ProductName.isEmpty()) status.prepend(m_ProductName + QLatin1Char('\n'));
		m_StatusIcon->setPixmap(QPixmap(running ? ":data/mpRunning.png" : ready ? ":data/mpReady.png" : ":data/mpNotReady.png").scaledToHeight(16, Qt::SmoothTransformation));
		m_Status->setText(status);
		m_Fetch->setEnabled(!running && technologyAllowed && !ready);
		m_Run->setEnabled(!m_RunPending && !running && technologyAllowed);
	}
};

AuLegacyCityManufacture::AuLegacyCityManufacture(QWidget* parent) : QWidget(parent)
{
	// AuLaborManufacture, executable 0x1401d1300, owns the scrolling panels.
	auto* content = new QWidget(this); m_PanelLayout = new QVBoxLayout(content);
	m_PanelLayout->setContentsMargins(2, 8, 2, 2); m_PanelLayout->setSpacing(2);
	auto* scroll = new QScrollArea(this); scroll->setWidget(content); scroll->setWidgetResizable(true);
	m_Message = new QLabel(this); m_Message->setWordWrap(true); m_Message->hide();
	auto* layout = new QVBoxLayout(this); layout->setMargin(0); layout->setSpacing(0);
	layout->addWidget(scroll); layout->addWidget(m_Message);
	m_Timer = new QTimer(this); m_Timer->setInterval(1000);
	m_Timeout = new QTimer(this); m_Timeout->setSingleShot(true); m_Timeout->setInterval(60000);
	m_DeferredTimer = new QTimer(this); m_DeferredTimer->setSingleShot(true); m_DeferredTimer->setInterval(250);
	connect(m_Timer, &QTimer::timeout, this, [this]() {
		for(auto* panel : m_Panels) if(panel->isVisible()) panel->UpdateCountdown();
		Refresh();
	});
	connect(m_Timeout, &QTimer::timeout, this, [this]() {
		m_PendingToken = 0; m_DeferredTimer->stop(); m_DeferredRequests.clear();
		ClearPendingRuns();
		m_Message->setText(tr("No server response from the industry.")); m_Message->show();
	});
	connect(m_DeferredTimer, &QTimer::timeout, this, [this]() {
		if(m_PendingToken || m_DeferredRequests.isEmpty()) return;
		const auto request = m_DeferredRequests.takeFirst();
		if(request.cityId == m_CityId && request.developmentId == m_DevelopmentId) SendRequest(request);
		else FinishRequest(request);
	});
	connect(auApp, &AuApp::LegacyCityReplyReceived, this, &AuLegacyCityManufacture::ReceiveReply);
	connect(auApp, &AuApp::LegacyScTransportReset, this, &AuLegacyCityManufacture::ResetTransport);
}

void AuLegacyCityManufacture::SetAvailable(bool available)
{
	if(m_HasIndustry == available) return;
	m_HasIndustry = available; emit AvailabilityChanged(available);
}

void AuLegacyCityManufacture::SetTarget(quint32 cityId, quint32 developmentId)
{
	if(m_CityId == cityId && m_DevelopmentId == developmentId) return;
	m_CityId = cityId; m_DevelopmentId = developmentId;
	// Replies for the previous site cannot repopulate this page.
	m_PendingToken = 0; m_Timeout->stop(); m_DeferredTimer->stop(); m_DeferredRequests.clear();
	ClearPendingRuns();
	for(auto* panel : m_Panels) panel->hide();
	SetAvailable(false); m_Message->clear(); m_Message->hide();
	if(cityId && developmentId) Refresh();
}

void AuLegacyCityManufacture::Submit(AuLegacyCityProtocol::Request request)
{
	if(!g_Au.IsLegacyGeneration() || auApp->IsSingleUserMode() || !auApp->IsSceneConnected())
	{ FinishRequest(request); return; }
	if(m_PendingToken || m_DeferredTimer->isActive() || !m_DeferredRequests.isEmpty())
	{
		if(request.action == AuLegacyCityProtocol::EconomyList) return;
		// Serialize changes without disabling every process panel. Routine
		// refreshes and request round trips must not flash the F10 controls.
		if(m_DeferredRequests.size() < 32) m_DeferredRequests.append(request);
		else
		{
			FinishRequest(request);
			m_Message->setText(tr("Wait for the pending industry changes before making another change.")); m_Message->show();
		}
		return;
	}
	SendRequest(request);
}

void AuLegacyCityManufacture::SendRequest(AuLegacyCityProtocol::Request request)
{
	m_ActiveRequest = request;
	m_PendingToken = auApp->RequestLegacyCityTool(request);
	if(m_PendingToken)
	{
		m_Timeout->start();
	}
	else
	{
		m_DeferredRequests.clear();
		ClearPendingRuns();
		m_Message->setText(tr("The industry request could not be sent.")); m_Message->show();
	}
}

void AuLegacyCityManufacture::Refresh()
{
	if(!m_CityId || !m_DevelopmentId || m_RefreshQueued || m_PendingToken
		|| m_DeferredTimer->isActive() || !m_DeferredRequests.isEmpty()) return;
	// F10's occupied-site poll uses the same transport's 200ms rate gate.
	// Offset this read so construction and industry refreshes can coexist.
	m_RefreshQueued = true;
	QTimer::singleShot(250, this, [this]() {
		m_RefreshQueued = false;
		if(!m_CityId || !m_DevelopmentId) return;
		AuLegacyCityProtocol::Request request; request.action = AuLegacyCityProtocol::EconomyList;
		request.cityId = m_CityId; request.developmentId = m_DevelopmentId; Submit(request);
	});
}

void AuLegacyCityManufacture::ReceiveReply(QByteArray payload)
{
	AuLegacyCityProtocol::Reply reply;
	if(!AuLegacyCityProtocol::DecodeReply(payload, reply) || !m_PendingToken || reply.token != m_PendingToken) return;
	m_PendingToken = 0; m_Timeout->stop();
	FinishRequest(m_ActiveRequest);
	if(reply.action != AuLegacyCityProtocol::EconomyList || !reply.message.isEmpty())
	{ m_Message->setText(reply.message); m_Message->setVisible(!reply.message.isEmpty()); }
	if(!m_DeferredRequests.isEmpty())
	{
		// Queue timing must never defer a state update. Preserve only selector
		// edits that have not been sent; apply every process snapshot below.
		m_DeferredTimer->start();
	}
	if(!reply.message.isEmpty() && !reply.hasEconomy) return;
	const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
	AuLegacyCity::ProductionState state; QString error;
	if(!catalogue || !reply.hasEconomy || reply.selectedCity != m_CityId || reply.selectedDevelopment != m_DevelopmentId
		|| !AuLegacyCity::DecodeProductionState(*catalogue, reply.production, state, error))
	{ for(auto* panel : m_Panels) panel->hide(); SetAvailable(false); return; }
	std::sort(state.processes.begin(), state.processes.end(), [](const AuLegacyCity::ProductionProcess& a,
		const AuLegacyCity::ProductionProcess& b) { return a.slot < b.slot; });
	// AuLaborManufacture addresses only process indices 0 through 7, even
	// when the reply contains more or omits a private, disabled process.
	int count = 0;
	while(count < state.processes.size() && state.processes[count].slot < 8) ++count;
	const bool canConfigure = CanConfigureIndustry(m_CityId);
	while(m_Panels.size() < count)
	{
		auto* panel = new AuLegacyManufacturePanel(this, [this](AuLegacyCityProtocol::Request request) { Submit(request); });
		m_PanelLayout->addWidget(panel); m_Panels.append(panel);
	}
	for(int index = 0; index < m_Panels.size(); ++index)
	{
		if(index >= count) { m_Panels[index]->hide(); continue; }
		const bool preserveSelections = std::any_of(m_DeferredRequests.cbegin(), m_DeferredRequests.cend(),
			[this, &state, index](const AuLegacyCityProtocol::Request& request) {
				return request.action == AuLegacyCityProtocol::SetIndustryProcess && request.cityId == m_CityId
					&& request.developmentId == m_DevelopmentId && request.processSlot == state.processes[index].slot;
			});
		m_Panels[index]->Update(*catalogue, m_CityId, m_DevelopmentId, reply.economyIndustry,
			reply.economyTechnology, state.processes[index], canConfigure, reply.hasRecipeChoices, reply.recipeChoices, preserveSelections);
		m_Panels[index]->show();
	}
	SetAvailable(count != 0);
}

void AuLegacyCityManufacture::ResetTransport()
{
	m_PendingToken = 0; m_Timeout->stop(); m_DeferredTimer->stop(); m_DeferredRequests.clear();
	ClearPendingRuns();
	for(auto* panel : m_Panels) panel->hide(); SetAvailable(false);
}

void AuLegacyCityManufacture::FinishRequest(const AuLegacyCityProtocol::Request& request)
{
	for(auto* panel : m_Panels) panel->FinishRequest(request);
}

void AuLegacyCityManufacture::ClearPendingRuns()
{
	for(auto* panel : m_Panels) panel->ClearPendingRun();
}

void AuLegacyCityManufacture::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event); m_Timer->start(); Refresh();
}

void AuLegacyCityManufacture::hideEvent(QHideEvent* event)
{
	m_Timer->stop(); QWidget::hideEvent(event);
}
