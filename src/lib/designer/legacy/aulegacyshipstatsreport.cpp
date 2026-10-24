#include "aulegacyshipstatsreport.h"
#include "aulegacyshipstats.h"
#include "aulegacyshipperformance.h"
#include "aulegacyshipsave.h"
#include "aulegacyshipdetails.h"
#include "aulegacyshipcommoditydata.h"
#include "audef.h"
#include "auglobal.h"

#include <QLocale>
#include <QResource>
#include <algorithm>
#include <cmath>

namespace
{
QString Number(double value, int precision = 0)
{
	return std::isfinite(value) ? QLocale().toString(value, 'f', precision) : QStringLiteral("Unavailable");
}

QString Volume(quint64 cubicFeet)
{
	return QStringLiteral("%1m\u00b3").arg(Number(std::ceil(double(cubicFeet) / CubicFeetPerCubicMeter())));
}

QString Mass(double pounds)
{
	return QStringLiteral("%1kg").arg(Number(std::floor(pounds / PoundsPerKilogram())));
}

QString Endurance(double fuel, double fuelPerSecond, const QString& condition)
{
	if (fuelPerSecond <= 0.0)
		return QStringLiteral("No fuel demand");
	// Report rounds consumption to hundredths, then uses the displayed capacity
	// and that rounded consumption to calculate endurance in tenths of a day.
	const float perDay = std::round(float(fuelPerSecond * 86400.0) * .028316846f * 100.f) / 100.f;
	const float capacity = float(std::ceil(fuel * .0283168466));
	const float days = perDay > 0 ? std::round(capacity / perDay * 10.f) / 10.f : 0;
	return QStringLiteral("%1 days at %2m\u00b3 H\u00b2 per Real Day while %3")
		.arg(QLocale().toString(days, 'g', 6), QLocale().toString(perDay, 'g', 6), condition);
}

QString ModuleName(quint16 module)
{
	const AuLegacyShip::LegacyCommodity::Data* data = AuLegacyShip::LegacyCommodity::Get(module);
	return data ? QString::fromUtf8(data->name) : QStringLiteral("Module %1").arg(module);
}

void FormattedLine(QString& html, const QString& markup, int depth = 1, const QString& color = QString())
{
	html += QStringLiteral("<p style=\"margin: 0; margin-left: %1px;%2\">%3</p>")
		.arg(depth * 10).arg(color.isEmpty() ? QString() : QStringLiteral(" color: %1;").arg(color), markup);
}

void Line(QString& html, const QString& text, int depth = 1, const QString& color = QString())
{
	FormattedLine(html, text.toHtmlEscaped(), depth, color);
}

void Section(QString& html, const QString& title)
{
	html += QStringLiteral("<hr><p style=\"margin: 0;\"><b>%1</b></p>").arg(title.toUpper().toHtmlEscaped());
}

void Row(QString& html, const QString& label, const QString& value, int depth = 1)
{
	Line(html, QStringLiteral("%1 %2").arg(value, label), depth);
}

void Count(QString& html, const QString& label, int value, int depth = 1)
{
	if(value) Row(html, label, QLocale().toString(value), depth);
}

quint16 ScalarModule(const AuLegacyShip::Design& design, int slot)
{
	return slot < design.header.scalarSlots.size() ? design.header.scalarSlots.at(slot)
		: AuLegacyShip::DefaultModuleSlots().at(slot);
}

QString ModuleWithTech(quint16 module, int techLevel)
{
	return QStringLiteral("%1 TL%2").arg(ModuleName(module)).arg(techLevel + 1);
}

QString Armor(const AuLegacyShip::ArmorFactors& factors)
{
	return QStringLiteral("%1% %2hp")
		.arg(std::min(100, int(std::ceil(factors.absorbedFraction * 100.f))))
		.arg(int(std::ceil(factors.blockedDamage)));
}

QString RoomDescription(const AuLegacyShip::Room& room, int deck, int minimumDeck)
{
	const char* names[] = {"None", "Barracks", "Bridge", "Brig", "Officer Cabin", "Hold", "Corridor",
		"Crew Cabin", "Engine Room", "Fuel Cell", "Galley", "Gravity Well", "Hull", "Vehicle Bay",
		"Lounge", "Open to Deck Below", "Sensor Room", "Shield Room", "Sickbay", "Stateroom",
		"Transporter Room", "Weapon Bay"};
	const QString name = room.roomType < sizeof(names) / sizeof(names[0])
		? QString::fromLatin1(names[room.roomType]) : QStringLiteral("Room");
	return QStringLiteral("%1 %2 Deck %3").arg(name).arg(room.roomSubtype + 1).arg(deck - minimumDeck + 1);
}

void ExampleDamage(QString& html, const AuLegacyShip::WeaponExample& weapon,
	const QString& action, const QString& single, const QString& bundle, int count, int depth)
{
	if(!weapon.ammunition) return;
	const int thresholds[] = {255, 245, 225, 195, 155, 115, 75, 35, 1};
	const char* grades[] = {"Perfect", "Excellent", "Superb", "Fine", "Good", "Fair", "Poor", "Bad", "Flawed"};
	QString grade = QStringLiteral("?Qual");
	for(int i = 0; i < 9; ++i)
		if(weapon.quality >= thresholds[i]) { grade = QString::fromLatin1(grades[i]); break; }
	const QString technology = weapon.techLevel ? QStringLiteral(" TL%1").arg(weapon.techLevel + 1) : QString();
	Line(html, QStringLiteral("Example %1 %2%3 %4 Q%5").arg(action, ModuleName(weapon.ammunition), technology, grade)
		.arg(weapon.quality), depth);
	const QString radius = weapon.radiusFeet > 0 ? QStringLiteral(", %1m Radius")
		.arg(Number(std::round(weapon.radiusFeet / FeetPerMeter()))) : QString();
	Line(html, QStringLiteral("%1 %2-%3hp%4").arg(single)
		.arg(Number(weapon.damageModifier + 1), Number(weapon.damage + weapon.damageModifier), radius), depth + 1);
	if(count > 0)
		Line(html, QStringLiteral("Full %1 of %2 Damage %3-%4hp%5").arg(bundle).arg(count)
			.arg(Number(weapon.damageModifier * quint64(count) + 1),
				Number((weapon.damage + weapon.damageModifier) * quint64(count)), radius), depth + 1);
}

QString CommodityIcon(quint16 commodity, int size)
{
	const AuLegacyShip::LegacyCommodity::Data* data = AuLegacyShip::LegacyCommodity::Get(commodity);
	if(!data) return QString();
	QString path = QString::fromUtf8(data->bitmap);
	if(path.startsWith(QStringLiteral(":data/"))) path.insert(1, QLatin1Char('/'));
	if(g_Au.IsLegacyGeneration() && path == QLatin1String(":/data/c_Computer.png"))
		path = QStringLiteral(":/legacy/cities/ui/c_Computer.png");
	if(!QResource(path).isValid()) return QString();
	return QStringLiteral("<img height=\"%1\" src=\"qrc%2\"> ").arg(size).arg(path.toHtmlEscaped());
}
}

QString AuLegacyShipStatsReport(const AuLegacyShip::Design& design)
{
	const AuLegacyShip::Stats stats = AuLegacyShip::CalculateStats(design);
	const AuLegacyShip::Performance performance = AuLegacyShip::CalculatePerformance(design, stats, design.header.minimumTechLevel);
	if (!stats.hasHull)
		return QString();
	const int tl = performance.techLevelUsed;
	QString classification = (performance.structure ? AuLegacyShip::LegacyStructureName(performance.structure)
		: QStringLiteral("Invalid Design")).toUpper();
	if (design.header.streamline)
		classification.prepend(QStringLiteral("STREAMLINED "));
	if (!stats.hullContiguous)
		classification.prepend(QStringLiteral("DISJOINT "));
	QString html = QStringLiteral("<p style=\"margin: 0;\"><b>%1</b></p>").arg(classification.toHtmlEscaped());
	Line(html, QStringLiteral("Tech Level %1").arg(tl + 1));
	Line(html, QStringLiteral("Armor Level %1 = TL%2").arg(design.header.armor).arg(performance.armorTechLevel + 1), 2);
	Line(html, QStringLiteral("%1 Mass = TL%2").arg(Mass(performance.massLb)).arg(performance.massTechLevel + 1), 2);
	const QString equipment = stats.equipmentTechSource == 2
		? QStringLiteral("%1 Crew+Passenger+Troop Berths").arg(stats.crewBerths + stats.passengerBerths + stats.troopBerths)
		: AuLegacyShip::DetailName(stats.equipmentTechSource);
	Line(html, QStringLiteral("%1 = TL%2").arg(equipment).arg(stats.equipmentTechLevel + 1), 2);
	Line(html, QStringLiteral("%1 = TL%2").arg(ModuleName(performance.moduleTechSource)).arg(performance.moduleTechLevel + 1), 2);
	Line(html, QStringLiteral("Manufacture = TL%1").arg(performance.manufacturing.techLevel + 1), 2);
	if(performance.manufacturing.valid)
		Line(html, QStringLiteral("Estimated Cost %1\u00a2 at TL%2 Using Q%3 Components")
			.arg(Number(performance.manufacturing.estimatedCost)).arg(tl + 1).arg(performance.manufacturing.componentQuality));

	Section(html, QStringLiteral("HULL"));
	if (!stats.hasHelm) Line(html, QStringLiteral("Helm Needed"), 1, QStringLiteral("#ff2020"));
	if (stats.lifeSupportUnits < performance.lifeSupportRequired)
		Line(html, QStringLiteral("%1 Life Support Units Needed").arg(performance.lifeSupportRequired), 1, QStringLiteral("#ff2020"));
	if (!stats.maneuverUnits) Line(html, QStringLiteral("Gravity Drive Needed"), 1, QStringLiteral("#ff2020"));
	if (!stats.powerPlantUnits) Line(html, QStringLiteral("Power Plant Needed"), 1, QStringLiteral("#ff2020"));
	if (!stats.roomVolumes[9]) Line(html, QStringLiteral("Fuel Cell Needed"), 1, QStringLiteral("#ff2020"));
	if (!stats.hullOpenings && !stats.transporterPads)
		Line(html, QStringLiteral("Hull Opening or Transporter Needed"), 1, QStringLiteral("#ff2020"));
	if (!stats.hullContiguous)
		Line(html, QStringLiteral("Must Connect Separate Hull Pieces Together"), 1, QStringLiteral("#ff2020"));
	Line(html, ModuleWithTech(ScalarModule(design, 2), tl));
	Line(html, (design.header.armor ? QStringLiteral("Armor Level %1 Absorbs %2 Kinetic Damage, %3 Energy Damage")
		.arg(design.header.armor) : QStringLiteral("Unarmored Absorbs %1 Kinetic Damage, %2 Energy Damage"))
		.arg(Armor(performance.hullKinetic), Armor(performance.hullEnergy)));
	Row(html, QStringLiteral("Volume"), Volume(stats.hullVolume));
	const bool station = performance.structure == 3 || performance.structure == 4;
	const auto dimension = [&](const QString& text, bool needsStation) {
		FormattedLine(html, text.toHtmlEscaped() + (needsStation
			? QStringLiteral(", <b>Must Spawn at Station</b>") : QString()), 2);
	};
	dimension(QStringLiteral("%1m Length").arg(Number(stats.length / FeetPerMeter(), 2)), station || stats.length > 119.);
	dimension(QStringLiteral("%1m Width").arg(Number(stats.width / FeetPerMeter(), 2)), station || stats.width > 119.);
	const int decks = stats.maximumDeck - stats.minimumDeck + 1;
	dimension(QStringLiteral("%1m Height, %2 Decks").arg(Number(stats.height / FeetPerMeter(), 2)).arg(decks), station || decks >= 6);
	Row(html, QStringLiteral("Mass"), Mass(performance.massLb));
	Row(html, QStringLiteral("Equipment"), Mass(performance.equipmentMassLb), 2);
	Row(html, QStringLiteral("Fuel"), Mass(performance.fuelMassLb), 2);
	Row(html, QStringLiteral("Hull"), Mass(performance.hullMassLb), 2);

	Section(html, QStringLiteral("Engineering"));
	Count(html, QStringLiteral("Maneuver Drive Units"), stats.maneuverUnits);
	Line(html, ModuleWithTech(performance.maneuverModule, tl), 2);
	Row(html, QStringLiteral("Acceleration"), QStringLiteral("%1 m/sec/sec")
		.arg(Number(std::ceil(performance.maneuverAccelerationFeetPerSecondSquared / FeetPerMeter()))), 2);
	Row(html, QStringLiteral("Vertical Acceleration"), QStringLiteral("%1 m/sec/sec")
		.arg(Number(std::ceil(performance.verticalAccelerationFeetPerSecondSquared / FeetPerMeter()))), 2);
	Count(html, QStringLiteral("Power Plant Units"), stats.powerPlantUnits);
	Line(html, ModuleWithTech(ScalarModule(design, 5), tl), 2);
	Line(html, QStringLiteral("Capacitor %1").arg(QLocale().toString(performance.capacitorPower, 'g', 6)), 2);
	if (stats.ftlUnits)
	{
		Count(html, QStringLiteral("FTL Drive Units"), stats.ftlUnits);
		Line(html, ModuleWithTech(performance.ftlModule, tl), 2);
		Line(html, QStringLiteral("%1 Units %2 to Engage").arg(Number(performance.ftlUnitsRequired),
			stats.ftlUnits >= performance.ftlUnitsRequired ? QStringLiteral("Sufficient") : QStringLiteral("NEEDED")), 2,
			stats.ftlUnits >= performance.ftlUnitsRequired ? QString() : QStringLiteral("#ff2020"));
		if(performance.capacitorPower <= performance.ftlEngagePower)
			Line(html, QStringLiteral("INADEQUATE CAPACITOR to Engage"), 2, QStringLiteral("#ff2020"));
		if (performance.maxWarpFactor)
			Line(html, QStringLiteral("Maximum Warp Factor %1").arg(performance.maxWarpFactor), 2);
	}

	Section(html, QStringLiteral("FUEL"));
	Line(html, QStringLiteral("Fuel Cell TL%1").arg(tl + 1));
	Row(html, QStringLiteral("Hydrogen (H\u00b2)"), Volume(performance.fuelCapacity));
	Line(html, Endurance(performance.fuelCapacity,
		performance.idlePowerPerSecond / 10.0 + performance.maneuverFuelPerSecond, QStringLiteral("Idling")), 2);
	Line(html, QStringLiteral("Life Support, Maneuver Drive and Sensor Powered On"), 3);
	Line(html, Endurance(performance.fuelCapacity,
		performance.idlePowerWithShieldsPerSecond / 10.0 + performance.maneuverFuelPerSecond, QStringLiteral("Idling with Shields Up")), 2);
	Line(html, QStringLiteral("Life Support, Maneuver Drive, Sensor and Shields Powered On"), 3);
	if (performance.maneuverModule != 0xec)
	{
		Line(html, Endurance(performance.fuelCapacity, performance.holdingPowerPerSecond / 10.0, QStringLiteral("Holding")), 2);
		Line(html, QStringLiteral("Life Support and Sensor Powered On; Maneuver Drive on Hold"), 3);
		Line(html, Endurance(performance.fuelCapacity, performance.holdingPowerWithShieldsPerSecond / 10.0, QStringLiteral("Holding with Shields Up")), 2);
		Line(html, QStringLiteral("Life Support, Sensor and Shields Powered On; Maneuver Drive on Hold"), 3);
	}

	Section(html, QStringLiteral("AVIONICS"));
	Line(html, ModuleWithTech(ScalarModule(design, 0), tl));
	QMap<quint16, const AuLegacyShip::Room*> rooms;
	for(const AuLegacyShip::Room& room : design.rooms)
		if((room.hasWorkingBoundary ? !room.workingBoundary.isEmpty() : !room.packedVertices.isEmpty())
			&& !rooms.contains(room.roomId)) rooms.insert(room.roomId, &room);
	QMap<QString, int> consoles;
	for(const AuLegacyShip::Detail& detail : design.details)
	{
		const int type = detail.detailType;
		if(type != 1 && type != 4 && type != 6 && type != 7 && type != 12 && type != 13
			&& type != 15 && type != 17 && type != 19 && type != 73) continue;
		const AuLegacyShip::Room* room = rooms.value(detail.roomId, nullptr);
		if(room) ++consoles[QStringLiteral("%1 - %2").arg(AuLegacyShip::DetailName(type),
			RoomDescription(*room, detail.deck, stats.minimumDeck))];
	}
	if(station) Line(html, QStringLiteral("Station Service Range %1m").arg((tl + 1) * 50), 2);
	for(auto console = consoles.cbegin(); console != consoles.cend(); ++console)
		Line(html, console.value() == 1 ? console.key() : QStringLiteral("%1 %2").arg(console.value()).arg(console.key()), 2);
	Count(html, QStringLiteral("Game Tables"), stats.gameTables);
	if(stats.lifeSupportUnits)
	{
		Count(html, QStringLiteral("Life Support Units"), stats.lifeSupportUnits);
		Line(html, ModuleWithTech(ScalarModule(design, 3), tl), 2);
	}
	if(stats.sensorUnits)
	{
		Count(html, QStringLiteral("Sensor Units"), stats.sensorUnits);
		Line(html, ModuleWithTech(ScalarModule(design, 6), tl), 2);
		const QString range = performance.sensorRangeFeet > 9000000 ? QStringLiteral("System Range")
			: performance.sensorRangeFeet > 300000 ? QStringLiteral("Planetary Range")
			: performance.sensorRangeFeet > 42500 ? QStringLiteral("Orbital Range") : QStringLiteral("Short Range");
		Line(html, QStringLiteral("%1 %2m").arg(range, Number(std::ceil(performance.sensorRangeFeet / FeetPerMeter()))), 2);
	}
	if(stats.shieldUnits)
	{
		Count(html, QStringLiteral("Shield Units"), stats.shieldUnits);
		Line(html, ModuleWithTech(performance.shieldModule, tl), 2);
		Line(html, QStringLiteral("Absorbs %1 Kinetic Damage per Side").arg(Armor(performance.shieldKinetic)), 2);
		Line(html, QStringLiteral("Absorbs %1 Energy Damage per Side").arg(Armor(performance.shieldEnergy)), 2);
	}
	for(const AuLegacyShip::Detail& detail : design.details)
		if(detail.detailType == 11)
		{
			const AuLegacyShip::Room* room = rooms.value(detail.roomId, nullptr);
			if(!room) continue;
			Line(html, RoomDescription(*room, detail.deck, stats.minimumDeck));
			Line(html, ModuleWithTech(detail.commodityIdNormalized ? detail.commodityIdNormalized : 0xf3, tl), 2);
		}
	if (!stats.transporterRooms.isEmpty())
	{
		for (auto room = stats.transporterRooms.cbegin(); room != stats.transporterRooms.cend(); ++room)
		{
			const int deckByte = room.key() >> 8;
			const int deck = deckByte < 128 ? deckByte : deckByte - 256;
			Line(html, QStringLiteral("Transporter Room %1 Deck %2").arg((room.key() & 0xff) + 1)
				.arg(deck - stats.minimumDeck + 1));
			quint16 module = 0xf6;
			for(const AuLegacyShip::ModulePair& pair : design.header.secondPairs)
				if(pair.first == room.key()) { module = pair.secondNormalized; break; }
			Line(html, ModuleWithTech(module, tl), 2);
			Count(html, QStringLiteral("Transporter Pads"), room.value(), 2);
		}
	}
	Count(html, QStringLiteral("Operable Doors/Hatches"), stats.operableDoors);
	Section(html, QStringLiteral("ARMAMENT"));
	if (!stats.weaponBays.isEmpty())
	{
		for (auto bay = stats.weaponBays.cbegin(); bay != stats.weaponBays.cend(); ++bay)
		{
			Line(html, QStringLiteral("Weapon Bay %1, %2 Units").arg(int(bay.key()) + 1).arg(Number(bay.value().units)));
			const auto firing = performance.weaponBays.constFind(bay.key());
			if (firing != performance.weaponBays.constEnd())
			{
				Line(html, ModuleWithTech(firing.value().module, tl), 2);
				switch (firing.value().module)
				{
				case 0x105: Count(html, QStringLiteral("Cannon Barrels"), firing.value().cannons, 2); break;
				case 0xfc: Count(html, QStringLiteral("Beam Projectors"), firing.value().projectors, 2); break;
				case 0xfe: Count(html, QStringLiteral("Launchers"), firing.value().launchers, 2); break;
				case 0x13f: Count(html, QStringLiteral("Ejector Racks"), firing.value().ejectorRacks, 2); break;
				default: break;
				}
				if(firing.value().rangeFeet > 0)
					Line(html, QStringLiteral("%1 %2m").arg(firing.value().example.ammunition
						? QStringLiteral("Targeting Range") : QStringLiteral("Range"),
						Number(std::floor(firing.value().rangeFeet / FeetPerMeter()))), 2);
				if(firing.value().module == 0xfc)
					Line(html, firing.value().capacitorShots > 0
						? (firing.value().capacitorShots == 1 ? QStringLiteral("Capacitor Adequate for One Shot of Energy Weapon at Full Power")
							: QStringLiteral("Capacitor Adequate for %1 Consecutive Shots of Energy Weapons at Full Power").arg(firing.value().capacitorShots))
						: QStringLiteral("INADEQUATE CAPACITOR to Fire Energy Weapons at Full Power"), 3);
				else if(!firing.value().example.ammunition && firing.value().rangeFeet > 0)
				{
					Line(html, QStringLiteral("Harvest %1u at %2m").arg(firing.value().harvestQuantity)
						.arg(Number(std::floor(std::floor(firing.value().rangeFeet / FeetPerMeter()) / 2))), 2);
					Line(html, firing.value().capacitorShots > 0
						? QStringLiteral("Capacitor Adequate for %1 Consecutive Shots at Full Power").arg(firing.value().capacitorShots)
						: QStringLiteral("INADEQUATE CAPACITOR to Fire at Full Power"), 2);
				}
				const quint16 module = firing.value().module;
				ExampleDamage(html, firing.value().example, module == 0xfe ? QStringLiteral("Launching")
					: module == 0x13f ? QStringLiteral("Dropping") : QStringLiteral("Firing"),
					module == 0xfc ? QStringLiteral("One Beam Damage") : module == 0xfe ? QStringLiteral("One Rocket Damage")
					: module == 0x13f ? QStringLiteral("One Bomb Damage") : QStringLiteral("One Round Damage"),
					module == 0xfc ? QStringLiteral("Bundle") : module == 0xfe ? QStringLiteral("Volley")
					: module == 0x13f ? QStringLiteral("Load") : QStringLiteral("Barrage"),
					module == 0xfc ? firing.value().projectors : module == 0xfe ? firing.value().launchers
					: module == 0x13f ? firing.value().ejectorRacks : firing.value().cannons, 3);
				if(module == 0xfe)
					ExampleDamage(html, AuLegacyShip::ExampleWeapon(0x38, tl), QStringLiteral("Launching"),
						QStringLiteral("One Missile Damage"), QStringLiteral("Volley"), firing.value().launchers, 3);
			}
		}
	}
	Count(html, QStringLiteral("Turrets"), stats.turrets);
	for(const AuLegacyShip::Detail& detail : design.details)
	{
		const int type = detail.detailType;
		if(type != 21 && type != 22 && (type < 33 || type > 40)) continue;
		const AuLegacyShip::Room* room = rooms.value(detail.roomId, nullptr);
		if(!room) continue;
		Line(html, RoomDescription(*room, detail.deck, stats.minimumDeck), 2);
		const quint16 module = detail.commodityIdNormalized ? detail.commodityIdNormalized : 0xf8;
		Line(html, ModuleWithTech(module, tl), 3);
		if(module == 0xf8 || module == 0x106)
			ExampleDamage(html, AuLegacyShip::ExampleWeapon(module == 0xf8 ? 0x137 : 0xb, tl),
				QStringLiteral("Firing"), QStringLiteral("Damage"), QString(), 0, 3);
		else if(module == 0xfa)
		{
			ExampleDamage(html, AuLegacyShip::ExampleWeapon(0x44, tl), QStringLiteral("Launching"), QStringLiteral("Damage"), QString(), 0, 3);
			ExampleDamage(html, AuLegacyShip::ExampleWeapon(0x38, tl), QStringLiteral("Launching"), QStringLiteral("Damage"), QString(), 0, 3);
		}
	}
	Section(html, QStringLiteral("CREW"));
	Count(html, QStringLiteral("Officer Stations"), stats.officerStations);
	Count(html, QStringLiteral("Crew Stations"), stats.crewStations);
	Count(html, QStringLiteral("Drivers"), stats.drivers);
	Count(html, QStringLiteral("Pilots"), stats.pilots);
	Count(html, QStringLiteral("Turret Gunners"), stats.turrets);
	Count(html, QStringLiteral("Guards"), stats.troopStations);
	Section(html, QStringLiteral("QUARTERS"));
	Count(html, QStringLiteral("Officer Berths"), stats.officerBerths);
	Count(html, QStringLiteral("Crew Berths"), stats.crewBerths);
	const QString passengerClass = stats.passengerClass == 1 ? QStringLiteral("1st Class")
		: stats.passengerClass == 2 ? QStringLiteral("2nd Class") : QStringLiteral("3rd Class");
	Count(html, QStringLiteral("%1 Passenger Berths").arg(passengerClass), stats.passengerBerths);
	Count(html, QStringLiteral("Troop Berths"), stats.troopBerths);

	Section(html, QStringLiteral("HOLD"));
	Row(html, QStringLiteral("Storage Space"), Volume(stats.roomVolumes[5]));
	Line(html, QStringLiteral("Room for %1 Commodities").arg(QLocale().toString(qulonglong(performance.holdCapacity))));

	Section(html, QStringLiteral("VEHICLE BAYS"));
	Row(html, QStringLiteral("Vehicle Bays"), Volume(stats.roomVolumes[13]));
	if (!stats.vehicleComplement.isEmpty())
	{
		for (auto vehicle = stats.vehicleComplement.cbegin(); vehicle != stats.vehicleComplement.cend(); ++vehicle)
			FormattedLine(html, CommodityIcon(vehicle.key(), 32) + QStringLiteral("%1 %2")
				.arg(vehicle.value()).arg(ModuleName(vehicle.key()).toHtmlEscaped()));
	}
	Section(html, QStringLiteral("Damage control"));
	Row(html, QStringLiteral("Total Hit Points"), QLocale().toString(qulonglong(performance.totalHitPoints)));
	Row(html, QStringLiteral("Structure Hit Points"), QLocale().toString(qulonglong(performance.structureHitPoints)), 2);
	const int structureSystems[] = {1, 3, 4, 5};
	const char* structureNames[] = {"Fuel Cell", "Hold", "Hull", "Vehicle Bay"};
	for(int i = 0; i < 4; ++i)
		if(performance.systemHitPoints[structureSystems[i]])
			Row(html, QString::fromLatin1(structureNames[i]), Number(performance.systemHitPoints[structureSystems[i]]), 3);
	Row(html, QStringLiteral("Equipment Hit Points"), QLocale().toString(qulonglong(performance.equipmentHitPoints)), 2);
	const int equipmentSystems[] = {0, 2, 6, 7, 8, 9, 10, 11, 12, 13, 14};
	const char* equipmentNames[] = {"Command and Control", "Maneuver Drive", "Life Support", "Power Plant",
		"Sensor", "Shield", "Medical", "Transporter", "Turret", "Weapon", "FTL Drive"};
	for(int i = 0; i < 11; ++i)
		if(performance.systemHitPoints[equipmentSystems[i]])
			Row(html, QString::fromLatin1(equipmentNames[i]), Number(performance.systemHitPoints[equipmentSystems[i]]), 3);
	Section(html, QStringLiteral("MANUFACTURE"));
	if(!performance.manufacturing.valid) Line(html, performance.manufacturing.error, 1, QStringLiteral("#ff2020"));
	else for(const AuLegacyShip::MaterialRequirement& material : performance.manufacturing.materials)
	{
		QString text = ModuleName(material.commodity);
		if(material.quantity) text.prepend(Number(material.quantity) + QLatin1Char(' '));
		if(material.minimumTechLevel >= 0) text += QStringLiteral(" TL%1").arg(material.minimumTechLevel + 1);
		FormattedLine(html, CommodityIcon(material.commodity, 16) + text.toHtmlEscaped());
	}
	QStringList diagnostics = performance.diagnostics;
	diagnostics.append(stats.diagnostics);
	diagnostics.removeDuplicates();
	if (!diagnostics.isEmpty())
	{
		html += QStringLiteral("<hr>");
		for (const QString& message : diagnostics)
			Line(html, message, 1, QStringLiteral("#ffff20"));
	}
	return html;
}
