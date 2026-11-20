#include "aulegacyshipstatsreport.h"
#include "aulegacybd.h"
#include "aulegacycitycatalogue.h"
#include "aulegacycityconstruction.h"
#include "aulegacycityconstructionlayout.h"
#include "aulegacyshipcommoditydata.h"
#include "aulegacyshipstats.h"

#include <QStringList>
#include <algorithm>
#include <cmath>

namespace
{
using AuLegacyShip::Stats;
using AuLegacyBd::DesignerIndustryInfo;

QString Warning(const QString& text)
{
	return QStringLiteral("<span style=\"color: #ff2020;\">%1</span>").arg(text);
}

QString Section(const QString& heading, const QStringList& lines)
{
	return (heading.isEmpty() ? QString() : QStringLiteral("<hr><b>%1</b>").arg(heading))
		+ QStringLiteral("<div style=\"margin-left: 10px;\">%1</div>").arg(lines.join(QStringLiteral("<br>")));
}

QString Image(const char* bitmap, bool small)
{
	if(!bitmap || !*bitmap) return QString();
	const QString path = AuLegacyCityConstruction::IconPath(QString::fromLatin1(bitmap));
	return QStringLiteral("<img %1src=\"qrc%2\">").arg(small ? QStringLiteral("height=\"16\" ") : QString(), path);
}

QString ModuleName(quint16 id)
{
	const auto* commodity = AuLegacyShip::LegacyCommodity::Get(id);
	return commodity ? QString::fromUtf8(commodity->name).toHtmlEscaped() : QString();
}

int StoryNumber(int deck)
{
	return deck < 0 ? deck : deck + 1;
}

int ConstructionLevels(const DesignerIndustryInfo& info, int minimumDeck, int stories)
{
	if(!info.stackLimit && info.maximumDeck > minimumDeck) return 1;
	if(info.stackLimit == 32) return std::max(1, (stories - 3) / 2 + 1);
	return stories;
}

QStringList Construction(int industry, const DesignerIndustryInfo& info, const Stats& stats, int levels)
{
	QStringList lines;
	if(!stats.hullVolume)
		lines << Warning(QStringLiteral("Rooms Needed"));
	else
	{
		const int above = stats.maximumDeck + 1;
		if(above > 0)
			lines << (above == 1 ? QStringLiteral("1 Story Above Grade") : QStringLiteral("%1 Stories Above Grade").arg(above));
		const int below = -stats.minimumDeck;
		if(below > 0)
			lines << (below == 1 ? QStringLiteral("1 Story Below Grade") : QStringLiteral("%1 Stories Below Grade").arg(below));
		lines << (levels == 1 ? QStringLiteral("1 Construction Level") : QStringLiteral("%1 Construction Levels").arg(levels));
		if(stats.maximumDeck < 0) lines << Warning(QStringLiteral("Building Under Ground"));
		else if(stats.minimumDeck > 0) lines << Warning(QStringLiteral("Building Above Ground"));
	}

	const int lowest = AuLegacyBd::MinimumDesignDeck(industry);
	if(!info.stackLimit)
	{
		if(info.maximumDeck == lowest)
			lines << QStringLiteral("Ground floor story is constructed at level one.") << QStringLiteral("Building may have 1 story above grade.");
		else
			lines << QStringLiteral("All stories are constructed at level one.") << QStringLiteral("Building may have %1 stories above grade.").arg(info.maximumDeck + 1);
	}
	else if(info.stackLimit == 32)
		lines << QStringLiteral("Ground floor and first two underground stories are constructed at level one.")
			<< QStringLiteral("Two more underground stories are dug with each additional level added.") << QStringLiteral("Building may have 1 story above grade.");
	else if(industry == 51)
		lines << QStringLiteral("Ground floor story is constructed at level one.") << QStringLiteral("Underground story is dug at level two.")
			<< QStringLiteral("Building may have 1 story above grade.");
	else
	{
		lines << QStringLiteral("One story is constructed for each level added.");
		if(info.stackLimit != 34)
			for(quint16 material : AuLegacyBd::BuildingMaterials)
			{
				const auto* commodity = AuLegacyShip::LegacyCommodity::Get(material);
				const int maximum = AuLegacyCityCatalogue::BuildingDesignMaximumDeck(AuLegacyCity::IndustryId(industry),
					AuLegacyCity::CommodityId(material), info.stackLimit);
				lines << QStringLiteral("%1 %2 construction stops at %3 stories above grade.")
					.arg(Image(commodity->bitmap, true), ModuleName(material)).arg(maximum + 1);
			}
	}
	if(lowest < 0) lines << QStringLiteral("Building may have %1 stories below grade.").arg(-lowest);
	if(stats.minimumDeck < lowest)
		lines << Warning(QStringLiteral("Lowest story %1 is lower than minimum story limit %2.").arg(StoryNumber(stats.minimumDeck)).arg(StoryNumber(lowest)));
	if(stats.maximumDeck > info.maximumDeck)
		lines << Warning(QStringLiteral("Highest story %1 is higher than maximum story limit %2.").arg(StoryNumber(stats.maximumDeck)).arg(StoryNumber(info.maximumDeck)));
	if(industry == 31 || industry == 50)
	{
		const quint16 material = industry == 31 ? 86 : 76;
		const auto* commodity = AuLegacyShip::LegacyCommodity::Get(material);
		lines << QStringLiteral("%1 Building is always constructed of %2.").arg(Image(commodity->bitmap, industry == 31), ModuleName(material));
	}
	return lines;
}

// ReportBuilding measures floor space in square meters from an eight-foot story.
// Liquid capacity alone uses cubic meters and truncates its conversion.
qint64 FloorArea(const Stats& stats, int roomType)
{
	return qint64(std::ceil(double(stats.roomVolumes[roomType]) * 0.125 * 0.092903));
}

void Quantity(QStringList& lines, qint64 actual, qint64 required, const QString& present, const QString& missing)
{
	if(actual) lines << present.arg(actual);
	if(actual < required) lines << Warning(missing.arg(required - actual));
}

void OneStation(QStringList& lines, int count, const QString& name, const QString& system, const QString& fitted)
{
	if(count == 0) lines << Warning(name + QStringLiteral(" Station Needed"));
	else if(count == 1) lines << fitted;
	else lines << Warning(QStringLiteral("%1 %2 Stations for One %3").arg(count).arg(name, system));
}

QString Facilities(int industry, const Stats& stats, const QString& module, int troops)
{
	QStringList lines;
	QString heading;
	if(industry == 11 || industry == 12 || industry == 111)
	{
		const bool weapon = industry == 11;
		const bool shield = industry == 12;
		const QString system = weapon ? QStringLiteral("Weapon") : shield ? QStringLiteral("Shield") : QStringLiteral("Radar");
		const QString station = weapon ? QStringLiteral("Fire Control") : system;
		heading = shield ? QStringLiteral("SHIELDS") : system.toUpper();
		Quantity(lines, FloorArea(stats, 2), 50, QStringLiteral("%L1m\u00b2 Command Center"), QStringLiteral("%L1m\u00b2 Command Center Needed"));
		OneStation(lines, stats.consoleCounts[weapon ? 7 : shield ? 17 : 15], station, system, station + QStringLiteral(" Station"));
		const QString equipment = industry == 111 ? QStringLiteral("Radar Equipment") : system + QStringLiteral(" System");
		Quantity(lines, FloorArea(stats, weapon ? 21 : shield ? 17 : 16), 100,
			QStringLiteral("%L1m\u00b2 ") + equipment + QStringLiteral(" Present, ") + module, QStringLiteral("%L1m\u00b2 ") + system + QStringLiteral(" Room Needed"));
		if(industry != 111)
		{
			if(stats.troopStations) lines << QStringLiteral("%L1 Troop Stations").arg(stats.troopStations);
			if(stats.troopStations < troops - 1)
				lines << QStringLiteral("<span style=\"color: #ffff20;\">%L1 Troop Stations Recommended</span>").arg(troops - 1 - stats.troopStations);
		}
	}
	else if(industry == 102 || (industry == 105 && stats.gameTables))
	{
		heading = QStringLiteral("GAMES");
		if(!stats.gameTables) lines << Warning(QStringLiteral("Game Table Needed"));
		else lines << (stats.gameTables == 1 ? QStringLiteral("Game Table") : QStringLiteral("%1 Game Tables").arg(stats.gameTables));
	}
	else if(industry == 113 || industry == 124)
	{
		const bool exhibit = industry == 113;
		heading = exhibit ? QStringLiteral("EXHIBIT") : QStringLiteral("PRISON");
		Quantity(lines, FloorArea(stats, 3), 100, exhibit ? QStringLiteral("%L1m\u00b2 Cages") : QStringLiteral("%L1m\u00b2 Jail Cells"),
			exhibit ? QStringLiteral("%L1m\u00b2 Cage Cells Needed") : QStringLiteral("%L1m\u00b2 Jail Cells Needed"));
	}
	else if(industry == 121)
	{
		heading = QStringLiteral("MEDICAL FACILITIES");
		const int doctors = stats.consoleCounts[4];
		if(!doctors) lines << Warning(QStringLiteral("Doctor's Station Needed"));
		else lines << (doctors == 1 ? QStringLiteral("Doctor's Station") : QStringLiteral("%1 Doctor's Stations").arg(doctors));
		if(!stats.surgeries) lines << Warning(QStringLiteral("Surgery Unit Needed"));
		else lines << (stats.surgeries == 1 ? QStringLiteral("Surgery Unit, %1").arg(module) : QStringLiteral("%1 Surgery Units, %2").arg(stats.surgeries).arg(module));
		if(doctors != stats.surgeries) lines << Warning(QStringLiteral("One Doctor's Station Needed For Each Surgery Unit"));
	}
	else if(industry == 127)
	{
		heading = QStringLiteral("TELESCOPE");
		OneStation(lines, stats.consoleCounts[1], QStringLiteral("Telescope"), QStringLiteral("Telescope"), QStringLiteral("Telescope Station and Telescope"));
	}
	else if(industry == 128)
	{
		heading = QStringLiteral("ARMAMENT");
		if(!stats.turrets) lines << Warning(QStringLiteral("Turret Needed"));
		else lines << QStringLiteral("%1, Range %L2m").arg(module).arg(999);
		if(stats.turrets > 1) lines << Warning(QStringLiteral("%1 Too Many Turrets").arg(stats.turrets - 1));
	}
	return heading.isEmpty() ? QString() : Section(heading, lines);
}

void StoragePerLevel(int industry, const DesignerIndustryInfo& info, int& goods, int& liquid)
{
	const int total = info.storage * 5 / 3;
	goods = total;
	liquid = 0;
	switch(industry)
	{
	case 4: goods = total * 3 / 16; liquid = total / 16; break;
	case 30: case 31: case 32: goods = total / 5; liquid = goods / 4; break;
	case 34: case 85: goods = total / 2; break;
	case 50: goods = total / 4; break;
	case 35: case 83: case 89: goods = total / 4; liquid = total * 3 / 4; break;
	case 41: case 42: case 48: goods = liquid = total / 2; break;
	case 51: goods = total / 8; liquid = total * 3 / 8; break;
	case 60: goods = info.capacitor * 3 / 4; liquid = info.capacitor / 4; break;
	case 61: case 62: case 63: case 64: case 65:
		goods = info.capacitor / 4; liquid = info.capacitor * 3 / 4; break;
	case 66: case 67:
		goods = (info.capacitor / 2) * 3 / 4; liquid = (info.capacitor / 2) / 4; break;
	case 104: liquid = goods; break;
	}
}
}

QString AuLegacyBuildingStatsReport(const AuLegacyShip::Design& design)
{
	// DpBuilding::DesignReport (AuAtom13 0x18059fa20) always requests
	// design requirements from DqDesignAnalysis::ReportBuilding (AuData13 0x1801d0e50).
	const int industry = design.buildingIndustry;
	if(!industry) return QString();
	const DesignerIndustryInfo info = AuLegacyBd::DesignerIndustry(industry);
	const Stats stats = AuLegacyShip::CalculateStats(design);
	const int levels = ConstructionLevels(info, AuLegacyBd::MinimumDesignDeck(industry), stats.maximumDeck - stats.minimumDeck + 1);
	const int troops = industry == 1 || (industry >= 10 && industry <= 12) || industry == 128 ? std::abs(info.employees) : 0;
	QString html = Image(AuLegacyCityConstructionLayout::IndustryIcon(industry), false) + QStringLiteral("<br><b>%1</b>").arg(info.name.toHtmlEscaped());
	html += Section(QString(), Construction(industry, info, stats, levels));
	html += Facilities(industry, stats, ModuleName(design.buildingModule), troops);

	QStringList lines;
	if(info.population || troops)
	{
		if(info.population) Quantity(lines, stats.passengerBerths, levels * info.population, QStringLiteral("%L1 Beds"), QStringLiteral("%L1 Beds Needed"));
		if(troops) Quantity(lines, stats.troopBerths, troops, QStringLiteral("%L1 Troop Bunks"), QStringLiteral("%L1 Troop Bunks Needed"));
		html += Section(QStringLiteral("QUARTERS"), lines);
	}
	if(AuLegacyBd::DoesManufacture(industry))
	{
		int workArea = (std::abs(info.employees) + (info.employees > 0 ? levels - 1 : 0)) * 90;
		if(industry >= 30 && industry <= 32) workArea /= 5;
		if(workArea)
		{
			lines.clear();
			Quantity(lines, FloorArea(stats, 8), workArea, QStringLiteral("%L1m\u00b2 Shop Space"), QStringLiteral("%L1m\u00b2 Shop Space Needed"));
			html += Section(QStringLiteral("WORK SPACE"), lines);
		}
	}
	int goods, liquid;
	StoragePerLevel(industry, info, goods, liquid);
	if(goods || liquid)
	{
		lines.clear();
		if(goods) Quantity(lines, FloorArea(stats, 5), levels * goods, QStringLiteral("%L1m\u00b2 Commodity Storage Space"), QStringLiteral("%L1m\u00b2 Storage Needed"));
		if(liquid) Quantity(lines, qint64(double(stats.roomVolumes[9]) * 0.0283168466), levels * liquid,
			QStringLiteral("%L1m\u00b3 Liquid Storage"), QStringLiteral("%L1m\u00b3 Liquid Storage Needed"));
		html += Section(QStringLiteral("STORAGE"), lines);
	}
	return html;
}
