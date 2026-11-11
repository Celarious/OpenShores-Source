//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|
// $Id: aulaborhandcraft.cpp,v 1.20 2022/05/04 22:01:24 daveh Exp $
// Copyright (c) 2022 Hazeron Freehold LLC
// All rights reserved.
//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

#include "aulaborhandcraft.h"
//
#include "auapp.h"
#include "aucreateempire.h"
#include "auframe.h"
#include "auview.h"
#include "dabd.h"
#include "daperson.h"
#include "daworldll.h"
#include "demanufacturingprocess.h"
#include "dgdata.h"
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <algorithm>

AuHandCraftDock::AuHandCraftDock(QWidget* parent) :
	AuDockWidget(":data/it_HandCraft.png", tr("<b>Handcraft</b>"), tr("Handcraft"), parent)
{
	SetWikiTopic("Handcraft");
	m_HandCraft = new AuLaborHandCraft(this);
	setWidget(m_HandCraft);

	int dockSides = g_Au.Settings().readNumEntry("Dock/HandcraftAreas", (int)allowedAreas());
	setAllowedAreas((Qt::DockWidgetAreas)dockSides);
}

void AuHandCraftDock::SetAllowedAreas(Qt::DockWidgetAreas areas)
{
	setAllowedAreas(areas);
	g_Au.Settings().writeEntry("Dock/HandcraftAreas", (int)allowedAreas());
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

namespace
{
	struct AuCraftNeed
	{
		int			cid;
		int			qty;
		bool		kept;
	};

	struct AuCraftRecipe
	{
		QVector<AuCraftNeed>	needs;
		bool					needsPower;
	};

	struct AuCraftEntry
	{
		int			mpid;
		int			cid;
		int			craft;
		QString		name;
		QString		category;
	};

	struct AuCraftRow
	{
		int			cid;
		int			qty;
		bool		kept;
		int			have;
		bool		madeFirst;
		QString		how;
	};

	const struct
	{
		COMMODITYID	cid;
		const char*	category;
	} s_CraftCategories[] =
	{
		{CID_Flag, QT_TR_NOOP("Survival")},
		{CID_StoneFirePit, QT_TR_NOOP("Survival")},
		{CID_BurningBarrel, QT_TR_NOOP("Survival")},
		{CID_Tepee, QT_TR_NOOP("Survival")},
		{CID_Torch, QT_TR_NOOP("Survival")},
		{CID_Candle, QT_TR_NOOP("Survival")},
		{CID_Water, QT_TR_NOOP("Survival")},
		{CID_Hammer, QT_TR_NOOP("Tools")},
		{CID_Shovel, QT_TR_NOOP("Tools")},
		{CID_SewingNeedle, QT_TR_NOOP("Tools")},
		{CID_Bow, QT_TR_NOOP("Weapons")},
		{CID_Arrows, QT_TR_NOOP("Weapons")},
		{CID_Spear, QT_TR_NOOP("Weapons")},
		{CID_Javelin, QT_TR_NOOP("Weapons")},
		{CID_JeansAndShirt, QT_TR_NOOP("Clothing")},
		{CID_LeatherClothes, QT_TR_NOOP("Clothing")},
		{CID_ParcXUniform, QT_TR_NOOP("Clothing")},
		{CID_CapriHalterOutfit, QT_TR_NOOP("Clothing")},
		{CID_GreyBDU, QT_TR_NOOP("Clothing")},
		{CID_KhakiBDU, QT_TR_NOOP("Clothing")},
		{CID_OliveDrabBDU, QT_TR_NOOP("Clothing")},
		{CID_BrownUniform, QT_TR_NOOP("Clothing")},
		{CID_GreyUniform, QT_TR_NOOP("Clothing")},
		{CID_RedUniform, QT_TR_NOOP("Clothing")},
		{CID_GreenUniform, QT_TR_NOOP("Clothing")},
		{CID_BlueUniform, QT_TR_NOOP("Clothing")},
		{CID_TealUniform, QT_TR_NOOP("Clothing")},
		{CID_GoldUniform, QT_TR_NOOP("Clothing")},
		{CID_PinkUniform, QT_TR_NOOP("Clothing")},
		{CID_WhiteUniform, QT_TR_NOOP("Clothing")},
		{CID_BlackUniform, QT_TR_NOOP("Clothing")},
		{CID_OrangeUniform, QT_TR_NOOP("Clothing")},
		{CID_UtilityPouch, QT_TR_NOOP("Containers")},
		{CID_Knapsack, QT_TR_NOOP("Containers")},
		{CID_Backpack, QT_TR_NOOP("Containers")},
		{CID_Crate, QT_TR_NOOP("Containers")},
		{CID_RedBinder, QT_TR_NOOP("Containers")},
		{CID_GreenBinder, QT_TR_NOOP("Containers")},
		{CID_BlueBinder, QT_TR_NOOP("Containers")},
		{CID_Textiles, QT_TR_NOOP("Materials")},
		{CID_Leather, QT_TR_NOOP("Materials")},
		{CID_Bone, QT_TR_NOOP("Materials")},
		{CID_Lumber, QT_TR_NOOP("Materials")},
		{CID_Paper, QT_TR_NOOP("Materials")},
		{CID_FishMeat, QT_TR_NOOP("Food")},
		{CID_AnimalMeat, QT_TR_NOOP("Food")},
		{CID_Brain, QT_TR_NOOP("Food")},
		{CID_Gills, QT_TR_NOOP("Food")},
		{CID_Heart, QT_TR_NOOP("Food")},
		{CID_Lungs, QT_TR_NOOP("Food")},
		{CID_Stomach, QT_TR_NOOP("Food")},
		{CID_MedicalKit, QT_TR_NOOP("Medical")},
		{CID_Pharmaceuticals, QT_TR_NOOP("Medical")},
		{CID_Dynamite, QT_TR_NOOP("Explosives")},
		{CID_PlasticExplosive, QT_TR_NOOP("Explosives")},
		{CID_GunPowder, QT_TR_NOOP("Explosives")},
		{CID_SailBoat, QT_TR_NOOP("Vehicles")},
	};

	const char* s_CraftCategoryOrder[] =
	{
		QT_TR_NOOP("Survival"),
		QT_TR_NOOP("Tools"),
		QT_TR_NOOP("Weapons"),
		QT_TR_NOOP("Clothing"),
		QT_TR_NOOP("Containers"),
		QT_TR_NOOP("Materials"),
		QT_TR_NOOP("Food"),
		QT_TR_NOOP("Medical"),
		QT_TR_NOOP("Explosives"),
		QT_TR_NOOP("Vehicles"),
		QT_TR_NOOP("This Building"),
		QT_TR_NOOP("Other"),
	};

	QString CraftCategory(int cid, int craft)
	{
		for(unsigned int ic=0; ic<sizeof(s_CraftCategories)/sizeof(s_CraftCategories[0]); ic++)
			if(s_CraftCategories[ic].cid == cid)
				return QObject::tr(s_CraftCategories[ic].category);
		if(craft == 2)
			return QObject::tr("This Building");
		return QObject::tr("Other");
	}

	QHash<int, AuCraftRecipe> s_CraftRecipes;

	const AuCraftRecipe& CraftRecipe(int mpid)
	{
		QHash<int, AuCraftRecipe>::const_iterator ri = s_CraftRecipes.constFind(mpid);
		if(ri != s_CraftRecipes.constEnd())
			return *ri;
		AuCraftRecipe recipe;
		recipe.needsPower = false;
		DeManufacturingProcess mp;
		mp.SetHandCraftingProcessId(mpid);
		for(DeProcessComponentArray::const_iterator pci = mp.Components().cbegin(); pci != mp.Components().cend(); pci++)
		{
			if(!(*pci).IsRequired())
				continue;
			if((*pci).IsPower())
			{
				recipe.needsPower = true;
				continue;
			}
			if(DbCommodity::IsPatent((*pci).CommodityId()))
				continue;
			AuCraftNeed need;
			need.cid = (*pci).CommodityId();
			need.qty = (*pci).QtyConsumed() ? (int)(*pci).QtyConsumed() : 1;
			need.kept = (*pci).QtyConsumed() == 0;
			recipe.needs.append(need);
		}
		return *s_CraftRecipes.insert(mpid, recipe);
	}

	QString NeedsText(int mpid)
	{
		QStringList parts;
		const AuCraftRecipe& recipe = CraftRecipe(mpid);
		for(int in=0; in<recipe.needs.count(); in++)
		{
			const AuCraftNeed& need = recipe.needs[in];
			if(need.kept)
				parts << QObject::tr("%1 (kept)").arg(g_Au.CommodityName((COMMODITYID)need.cid));
			else
				parts << QObject::tr("%1 %2").arg(need.qty).arg(g_Au.CommodityName((COMMODITYID)need.cid));
		}
		return parts.join(", ");
	}

	QHash<int,int> CountGear(const AuGear* inven)
	{
		QHash<int,int> counts;
		for(AuGear::const_iterator ii = inven->begin(); ii != inven->end(); ii++)
		{
			counts[(*ii).Item().CommodityId()] += (*ii).Item().Count();
			const AuItemContent* contents = (*ii).Item().Content();
			if(contents)
				for(AuItemContent::const_iterator ci = contents->begin(); ci != contents->end(); ci++)
					counts[(*ci).second->CommodityId()] += (*ci).second->Count();
		}
		return counts;
	}

	bool PlanCraft(int mpid, const QHash<int,int>& gear, const QVector<AuCraftEntry>& recipes, bool hasPower, QList<int>& plan,
			QStringList& missing, QVector<AuCraftRow>* rows)
	{
		QHash<int,int> stock = gear;
		const AuCraftRecipe& recipe = CraftRecipe(mpid);
		bool ok = true;
		if(recipe.needsPower && !hasPower)
		{
			ok = false;
			missing << QObject::tr("Electricity");
		}
		for(int in=0; in<recipe.needs.count(); in++)
		{
			const AuCraftNeed& need = recipe.needs[in];
			AuCraftRow row;
			row.cid = need.cid;
			row.qty = need.qty;
			row.kept = need.kept;
			row.have = gear.value(need.cid);
			row.madeFirst = false;
			QStringList options;
			int shortBy = need.qty - stock.value(need.cid);
			if(shortBy > 0)
			{
				for(int ir=0; ir<recipes.count(); ir++)
				{
					const AuCraftEntry& sub = recipes[ir];
					if(sub.cid != need.cid || sub.mpid == mpid)
						continue;
					const AuCraftRecipe& subRecipe = CraftRecipe(sub.mpid);
					options << NeedsText(sub.mpid);
					if(subRecipe.needsPower && !hasPower)
						continue;
					QHash<int,int> trial = stock;
					QList<int> subPlan;
					for(int made=0; made<shortBy; made++)
					{
						bool can = true;
						for(int is=0; is<subRecipe.needs.count() && can; is++)
							if(trial.value(subRecipe.needs[is].cid) < subRecipe.needs[is].qty)
								can = false;
						if(!can)
							break;
						for(int is=0; is<subRecipe.needs.count(); is++)
							if(!subRecipe.needs[is].kept)
								trial[subRecipe.needs[is].cid] -= subRecipe.needs[is].qty;
						trial[need.cid] += 1;
						subPlan << sub.mpid;
					}
					if(subPlan.count() == shortBy)
					{
						stock = trial;
						plan << subPlan;
						row.madeFirst = true;
						row.how = QObject::tr("made first from %1").arg(NeedsText(sub.mpid));
						break;
					}
				}
				if(!row.madeFirst && !options.isEmpty())
					row.how = QObject::tr("make from %1").arg(options.join(QObject::tr(" or ")));
				if(!row.madeFirst)
				{
					ok = false;
					missing << QObject::tr("%1 %2").arg(shortBy).arg(g_Au.CommodityName((COMMODITYID)need.cid));
				}
			}
			if(!need.kept)
				stock[need.cid] -= need.qty;
			if(rows)
				rows->append(row);
		}
		if(ok)
			plan << mpid;
		else
			plan.clear();
		return ok;
	}
}

AuHandCraftPanel::AuHandCraftPanel(QWidget* parent) : QWidget(parent)
{
	m_MPID = 0;

	m_Search = new QLineEdit(this);
	m_Search->setPlaceholderText(tr("Search recipes"));
	m_Search->setClearButtonEnabled(true);
	m_Search->setToolTip(tr("Shows recipes from every category whose name contains this text."));
	connect(m_Search, SIGNAL(textChanged(const QString&)), SLOT(SearchChanged()));

	m_Categories = new QListWidget(this);
	m_Categories->setToolTip(tr("Kinds of things you can handcraft."));
	connect(m_Categories, SIGNAL(currentRowChanged(int)), SLOT(CategoryChanged()));

	m_Recipes = new QListWidget(this);
	m_Recipes->setToolTip(tr("<html>"
				"<p><i>Handcrafting</i> creates a new item from items in your gear. "
				"If the new item does not fit into your gear, it is dropped at your location.</p>"
				"<p>Recipes marked * need the workshop of the building you are in. "
				"When a patent is needed, your gear or your current building must have a copy of the patent.</p>"
				"</html>"));
	connect(m_Recipes, SIGNAL(currentRowChanged(int)), SLOT(RecipeChanged()));

	m_Needs = new QListWidget(this);
	m_Needs->setToolTip(tr("What the recipe uses from your gear. Items marked <i>kept</i> are needed but not used up."));
	m_Needs->setSelectionMode(QAbstractItemView::NoSelection);
	m_Needs->setFocusPolicy(Qt::NoFocus);

	m_Note = new QLabel(this);
	m_Note->setWordWrap(true);

	m_Make = new QPushButton(tr("Craft"), this);
	m_Make->setToolTip(tr("<html><b>Craft</b><br>"
				"Handcrafts the selected item from items in your gear. "
				"Parts you can make from what you carry are made first."));
	connect(m_Make, SIGNAL(clicked(bool)), SLOT(GoPushed()));

	QGroupBox* gbCat = new QGroupBox(tr("Category"), this);
	QVBoxLayout* clay = new QVBoxLayout;
	clay->addWidget(m_Categories);
	clay->addWidget(m_Note);
	gbCat->setLayout(clay);

	QGroupBox* gbRec = new QGroupBox(tr("Recipes"), this);
	QVBoxLayout* rlay = new QVBoxLayout;
	rlay->addWidget(m_Recipes);
	gbRec->setLayout(rlay);

	m_NeedsBox = new QGroupBox(tr("Requirements"), this);
	QHBoxLayout* mlay = new QHBoxLayout;
	mlay->addStretch();
	mlay->addWidget(m_Make);
	mlay->addStretch();
	QVBoxLayout* nlay = new QVBoxLayout;
	nlay->addWidget(m_Needs, 10);
	nlay->addLayout(mlay);
	m_NeedsBox->setLayout(nlay);

	QHBoxLayout* hlay = new QHBoxLayout;
	hlay->addWidget(gbCat, 3);
	hlay->addWidget(gbRec, 5);
	hlay->addWidget(m_NeedsBox, 6);

	QVBoxLayout* lay = new QVBoxLayout;
	lay->addWidget(m_Search);
	lay->addLayout(hlay, 10);
	lay->setMargin(0);
	setLayout(lay);
}

AuHandCraftPanel::~AuHandCraftPanel()
{
}

void AuHandCraftPanel::UpdateContents(DaPerson* person)
{
	if(m_FlagAfterEmpire.isValid() && person)
	{
		if(person->EmpireId())
		{
			m_FlagAfterEmpire = AuElapsedTimer();
			emit Go(MPID_Flag);
		}
		else if(m_FlagAfterEmpire.elapsed() > 60000)
			m_FlagAfterEmpire = AuElapsedTimer();
	}

	// Must not allow handcraft at a building under construction when the building is required.
	bool inBd = false;			// true if in a bd that is not under construction and not destroyed.
	bool hasPower = false;
	AuKnownPatents patents;
	DaWorldLL* llworld = person->FindParentWorldInEnv();
	if(llworld)
	{
		if(!person->BdId().IsNull())
		{
			DaBd* bd = llworld->FindBd(person->BdId());
			if(bd && !bd->IsUnderConstruction() && !bd->IsDestroyed())
			{
				inBd = true;
				hasPower = bd->HasPower();
				bd->ListPatents(patents, bd->IndustryId());
			}
		}
	}

	// Add patents in the person's possession.
	person->ListPatents(patents);

	INDUSTRYID iid = person->InIndustry();
	QVector<AuCraftEntry> recipes;
	{
		const DgData& gd = g_Au.LockDataForRead();
		for(DbManufacturingProcessArray::const_iterator mpi = gd.m_bManufacturingProcessArray.cbegin();
				mpi < gd.m_bManufacturingProcessArray.cend(); mpi++)
		{
			// Do not list patented processes unless the patent is available.
			if(!gd.m_bManufacturingComponentArray.HasNecessaryPatents((*mpi).m_id, patents))
				continue;
			// Skip processes that can never be handcrafted.
			if((*mpi).m_craft == 0)
				continue;
			// Skip processes that depend on the scene atom's current industry.
			if((*mpi).m_craft == 2 && !(inBd && (*mpi).m_idi == iid))
				continue;
			AuCraftEntry entry;
			entry.mpid = (*mpi).m_id;
			entry.cid = (*mpi).m_idc;
			entry.craft = (*mpi).m_craft;
			entry.name = (*mpi).m_name;
			if((*mpi).m_craft == 2)
				entry.name += " *";
			entry.category = CraftCategory(entry.cid, entry.craft);
			recipes.append(entry);
		}
		if(recipes.count() && iid != IID_None)
		{
			bool iidreq = false;
			for(int ir=0; ir<recipes.count(); ir++)
				if(recipes[ir].craft == 2)
					iidreq = true;
			m_Note->setText(iidreq ? tr("<small>* %1</small>").arg(gd.IndustryName(iid)) : QString());
		}
		else
			m_Note->setText(QString());
		g_Au.UnlockData();
	}
	std::sort(recipes.begin(), recipes.end(), [](const AuCraftEntry& a, const AuCraftEntry& b)
			{ return a.name.localeAwareCompare(b.name) < 0; });

	const AuGear* inven = person->LockGearForRead();
	QHash<int,int> gear = CountGear(inven);
	person->UnlockGear();

	QStringList categories;
	for(unsigned int ic=0; ic<sizeof(s_CraftCategoryOrder)/sizeof(s_CraftCategoryOrder[0]); ic++)
	{
		QString cat = QObject::tr(s_CraftCategoryOrder[ic]);
		for(int ir=0; ir<recipes.count(); ir++)
			if(recipes[ir].category == cat)
			{
				categories << cat;
				break;
			}
	}
	if(m_Category.isEmpty() || !categories.contains(m_Category))
		m_Category = categories.isEmpty() ? QString() : categories.first();
	QString search = m_Search->text().trimmed();

	m_Categories->blockSignals(true);
	QStringList shownCats;
	for(int ic=0; ic<m_Categories->count(); ic++)
		shownCats << m_Categories->item(ic)->text();
	if(shownCats != categories)
	{
		m_Categories->clear();
		m_Categories->addItems(categories);
	}
	m_Categories->setCurrentRow(search.isEmpty() ? categories.indexOf(m_Category) : -1);
	m_Categories->blockSignals(false);

	QVector<AuCraftEntry> shown;
	for(int ir=0; ir<recipes.count(); ir++)
	{
		if(search.isEmpty() ? recipes[ir].category == m_Category : recipes[ir].name.contains(search, Qt::CaseInsensitive))
			shown.append(recipes[ir]);
	}
	bool found = false;
	for(int is=0; is<shown.count(); is++)
		if(shown[is].mpid == m_MPID)
			found = true;
	if(!found)
		m_MPID = shown.isEmpty() ? 0 : shown.first().mpid;

	m_Recipes->blockSignals(true);
	bool rebuild = m_Recipes->count() != shown.count();
	for(int is=0; is<shown.count() && !rebuild; is++)
		if(m_Recipes->item(is)->data(Qt::UserRole).toInt() != shown[is].mpid)
			rebuild = true;
	if(rebuild)
	{
		m_Recipes->clear();
		for(int is=0; is<shown.count(); is++)
		{
			QListWidgetItem* lwi = new QListWidgetItem(QIcon(g_Au.CommodityBitmapName((COMMODITYID)shown[is].cid)),
					shown[is].name, m_Recipes);
			lwi->setData(Qt::UserRole, shown[is].mpid);
		}
	}
	for(int is=0; is<shown.count(); is++)
		if(shown[is].mpid == m_MPID)
			m_Recipes->setCurrentRow(is);
	m_Recipes->blockSignals(false);

	m_Plan.clear();
	m_Needs->clear();
	if(!m_MPID)
	{
		m_NeedsBox->setTitle(tr("Requirements"));
		m_Make->setEnabled(false);
		return;
	}
	QString makeName;
	for(int ir=0; ir<recipes.count(); ir++)
		if(recipes[ir].mpid == m_MPID)
			makeName = g_Au.CommodityName((COMMODITYID)recipes[ir].cid);
	m_NeedsBox->setTitle(makeName);

	QStringList missing;
	QVector<AuCraftRow> rows;
	bool ok = PlanCraft(m_MPID, gear, recipes, hasPower, m_Plan, missing, &rows);
	for(int ir=0; ir<rows.count(); ir++)
	{
		const AuCraftRow& row = rows[ir];
		QString name = g_Au.CommodityName((COMMODITYID)row.cid);
		QString text = tr("%1 %2/%3").arg(name).arg(qMin(row.have, row.qty)).arg(row.qty);
		if(row.kept)
			text += " " + tr("(kept)");
		new QListWidgetItem(QIcon(g_Au.CommodityBitmapName((COMMODITYID)row.cid)), text, m_Needs);
	}
	if(rows.isEmpty())
		new QListWidgetItem(tr("Nothing needed."), m_Needs);

	m_Make->setEnabled(ok);
}

void AuHandCraftPanel::GoPushed()
{
	if(!m_MPID || m_Plan.isEmpty())
		return;

	QList<int> plan = m_Plan;
	if(plan.last() == MPID_Flag)
	{
		AuFrame* pFrame = auApp->Frame();
		if(!pFrame)
			return;
		AuView* pView = pFrame->View();
		if(!pView)
			return;
		if(!pView->SceneAtomEmpire())
		{
			AuCreateEmpire dlg(pFrame);
			if(pFrame->ExecDialog(dlg) != QDialog::Accepted)
				return;
			plan.removeLast();
			m_FlagAfterEmpire.restart();
		}
	}
	for(int ip=0; ip<plan.count(); ip++)
		emit Go(plan[ip]);
	emit Refresh();
}

void AuHandCraftPanel::CategoryChanged()
{
	QListWidgetItem* lwi = m_Categories->currentItem();
	if(!lwi)
		return;
	m_Category = lwi->text();
	m_Search->blockSignals(true);
	m_Search->clear();
	m_Search->blockSignals(false);
	m_MPID = 0;
	emit Refresh();
}

void AuHandCraftPanel::RecipeChanged()
{
	QListWidgetItem* lwi = m_Recipes->currentItem();
	if(!lwi)
		return;
	m_MPID = lwi->data(Qt::UserRole).toInt();
	emit Refresh();
}

void AuHandCraftPanel::SearchChanged()
{
	emit Refresh();
}

//-+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+

AuLaborHandCraft::AuLaborHandCraft(QWidget* parent) : QWidget(parent)
{
	m_timer = 0;

	AuFrame* pFrame = auApp->Frame();
	if(!pFrame)
		return;
	AuView* pView = pFrame->View();
	if(!pView)
		return;

	// Make the handcrafting controls.
	m_Panel = new AuHandCraftPanel(this);
	connect(m_Panel, SIGNAL(Go(int)), auApp, SLOT(HandCraftGo(int)));
	connect(m_Panel, SIGNAL(Refresh()), SLOT(UpdateContents()));

	// Build the layout.
	QGridLayout* lay = new QGridLayout;
	int row=0;
	lay->addWidget(m_Panel, row++,0);
	lay->setSpacing(2);
	lay->setMargin(2);
	setLayout(lay);
}

AuLaborHandCraft::~AuLaborHandCraft()
{
}

void AuLaborHandCraft::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);

	// Create a timer to keep this page updated.
	if(!m_timer)
	{
		m_timer = new QTimer(this);
		connect(m_timer, SIGNAL(timeout()), SLOT(UpdateContents()));
	}
	if(!m_timer->isActive())
		m_timer->start(1000);
}

void AuLaborHandCraft::UpdateContents()
{
	if(!isVisible())
	{
		// Kill the timer. It will be restarted during the next showEvent.
		if(m_timer && m_timer->isActive())
			m_timer->stop();
		return;
	}

	// Lock the root and find the scene atom.
	AuView* pView = auApp->Frame()->View();
	DaPerson* person;
	pView->LockSceneAtom(&person);
	if(person)
		m_Panel->UpdateContents(person);
	pView->UnlockSceneAtom();
}

//-+---+---+---+---+---+---+---+---+---|---+---+---+---+---+---+-==+---+---+---|---+---+---+---+---+---+---+---+---+---+---+---+---|

AuHandCraftDialog::AuHandCraftDialog(QWidget* parent) : AuDialog(parent)
{
	setAuDlgTitle(tr("Handcraft"));
	setAuDlgIcon(QIcon(":data/it_HandCraft.png"));
	SetWikiTopic("Handcraft");

	m_HandCraft = new AuLaborHandCraft(this);

	QVBoxLayout* lay = new QVBoxLayout;
	lay->addWidget(m_HandCraft);
	lay->setMargin(2);
	lay->setSpacing(2);
	setAuDlgLayout(lay);
}

AuHandCraftDialog::~AuHandCraftDialog()
{
}

void AuHandCraftDialog::setVisible(bool visible)
{
	if(visible)
	{
		AuDialog::setVisible(visible);
		RestoreGeometry();
	}
	else if(isVisible())
	{
		SaveGeometry();
		AuDialog::setVisible(visible);
	}
}

void AuHandCraftDialog::RestoreGeometry()
{
	int posX = g_Au.Settings().readNumEntry("Widget/HandCraft/PosX",-123);
	int posY = g_Au.Settings().readNumEntry("Widget/HandCraft/PosY",-123);
	QSize szdef = sizeHint();
	int sizeX = g_Au.Settings().readNumEntry("Widget/HandCraft/SizeX",szdef.width());
	int sizeY = g_Au.Settings().readNumEntry("Widget/HandCraft/SizeY",szdef.height());
	resize(sizeX, sizeY);
	if(posX != -123 && posY != -123)
		move(posX, posY);
}

void AuHandCraftDialog::SaveGeometry()
{
	QPoint ps = pos();
	QSize sz = size();
	g_Au.Settings().writeEntry("Widget/HandCraft/PosX",ps.x());
	g_Au.Settings().writeEntry("Widget/HandCraft/PosY",ps.y());
	g_Au.Settings().writeEntry("Widget/HandCraft/SizeX",sz.width());
	g_Au.Settings().writeEntry("Widget/HandCraft/SizeY",sz.height());
}

QSize AuHandCraftDialog::sizeHint() const
{
	return QSize(700,400);
}
