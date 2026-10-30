#include "aulegacycitystate.h"
#include "aulegacycitycatalogue.h"
#include <QBuffer>
#include <QDataStream>
#include <QSet>
#include <cmath>

namespace AuLegacyCity
{
namespace
{
bool Bytes(QDataStream& in, QByteArray& bytes, quint32 maximum)
{
	quint32 size = 0;
	in >> size;
	if(in.status() != QDataStream::Ok) return false;
	if(size == 0xffffffffU) { bytes.clear(); return true; }
	if(in.status() != QDataStream::Ok || size > maximum || size > quint64(in.device()->bytesAvailable())) return false;
	bytes.resize(int(size));
	return !size || in.readRawData(bytes.data(), int(size)) == int(size);
}
bool String(QDataStream& in, QString& value)
{
	QByteArray bytes;
	if(!Bytes(in, bytes, 256) || (bytes.size() & 1)) return false;
	value.resize(bytes.size() / 2);
	for(int i = 0; i < value.size(); ++i)
		value[i] = QChar((quint16(quint8(bytes[2*i])) << 8) | quint8(bytes[2*i+1]));
	return true;
}
bool Valid(const State& state, QString& error)
{
	if(state.name.size() > 128 || state.population < 0 || state.satisfaction < -20 || state.satisfaction > 20
		|| !std::isfinite(state.capacitor) || state.capacitor < 0
		|| state.developments.size() > MaximumDevelopments || state.flags.size() > 4096 || state.citizenSpecies.size() > 4096)
	{ error = QStringLiteral("Invalid legacy city summary or collection size."); return false; }
	QSet<quint32> ids;
	for(const Development& dev : state.developments)
	{
		if(!dev.id || !dev.industryId || ids.contains(dev.id)
			|| dev.minimumDeck < -127 || dev.maximumDeck > 126 || dev.minimumDeck > dev.maximumDeck
			|| int(dev.maximumDeck) - int(dev.minimumDeck) > 127
			|| !std::isfinite(dev.altitudeMSL) || !std::isfinite(dev.facingRadians)
			|| !std::isfinite(dev.location.Latitude()) || !std::isfinite(dev.location.Longitude())
			|| std::abs(dev.location.Latitude()) > PIf / 2 || std::abs(dev.location.Longitude()) > PIf
			|| !dev.targetLevels || dev.targetLevels > 32 || dev.completedLevels > 128
			|| dev.technology > 31
			|| dev.components.size() > 128 || dev.laborRemaining > 65535
			|| !std::isfinite(dev.roadLengthMeters) || dev.roadLengthMeters < 1 || dev.roadLengthMeters > 1000
			|| !std::isfinite(dev.roadWidthMeters) || dev.roadWidthMeters < 1 || dev.roadWidthMeters > 100
			|| !std::isfinite(dev.roadStartAltitudeMSL) || !std::isfinite(dev.roadEndAltitudeMSL)
			|| !std::isfinite(dev.biodomeRadiusMeters) || dev.biodomeRadiusMeters < 1 || dev.biodomeRadiusMeters > 1000)
		{ error = QStringLiteral("Invalid legacy development identity, transform or deck range."); return false; }
		for(const auto& component : dev.components)
			if(!component.commodityId || component.effect < 1 || component.effect > 6
				|| component.quantity > 65535 || component.applied > qMax(quint32(1), component.quantity))
			{ error = QStringLiteral("Invalid legacy construction component."); return false; }
		if(dev.selectedManufacturingRecipe)
		{
			const auto* catalogue = AuLegacyCityCatalogue::BuiltIn();
			const auto* recipe = catalogue ? catalogue->ManufacturingProcess(ManufacturingProcessId(dev.selectedManufacturingRecipe)) : nullptr;
			if(!recipe || recipe->industry.value != dev.industryId)
			{ error = QStringLiteral("Invalid pending legacy manufacturing recipe."); return false; }
		}
		ids.insert(dev.id);
	}
	for(const Flag& flag : state.flags)
		if(flag.level > 30 || !std::isfinite(flag.location.Latitude()) || !std::isfinite(flag.location.Longitude())
			|| std::abs(flag.location.Latitude()) > PIf / 2 || std::abs(flag.location.Longitude()) > PIf)
		{ error = QStringLiteral("Invalid legacy flag level or location."); return false; }
	for(const QByteArray* group : {&state.inventory, &state.manufacturing, &state.accounting, &state.reportHistory})
		if(group->size() > MaximumStateBytes / 2) { error = QStringLiteral("Legacy city group exceeds its size limit."); return false; }
	return true;
}
}

bool EncodeState(const State& state, QByteArray& output, QString& error)
{
	error.clear();
	if(!Valid(state, error)) return false;
	QByteArray bytes;
	QDataStream out(&bytes, QIODevice::WriteOnly);
	out.setVersion(QDataStream::Qt_4_1);
	out << quint16(0x4c43) << StateVersion << state.name << state.controllingEmpire << state.allegianceEmpire
		<< state.loyalPopulation << state.population << state.satisfaction << state.capacitor << state.lastVisit;
	out << quint32(state.developments.size());
	for(const Development& dev : state.developments)
	{
		out << dev.id << dev.designId << dev.industryId << dev.materialId << dev.location
			<< dev.altitudeMSL << dev.facingRadians << dev.minimumDeck << dev.maximumDeck
			// Retain the version-1 list count for existing saves/replication.
			// Original building doorways have no open/closed state.
			<< quint8(dev.constructed) << dev.completedLevels << quint32(0);
		out << dev.constructionProcessId << dev.targetLevels << dev.laborRemaining << dev.halted
			<< dev.citizenWorker << dev.lastPlayerLabor << dev.roadLengthMeters << dev.roadWidthMeters << dev.roadSlab
			<< dev.roadStartAltitudeMSL << dev.roadEndAltitudeMSL << dev.biodomeRadiusMeters << dev.demolished
			<< quint16(dev.components.size());
		for(const auto& component : dev.components)
			out << component.commodityId << component.effect << component.quality << component.quantity << component.applied;
		out << dev.technology << dev.employees << dev.selectedManufacturingRecipe << dev.manufacturingCitizenEnabled;
	}
	out << quint32(state.flags.size());
	for(const Flag& flag : state.flags) out << flag.empire << flag.location << flag.level << flag.capturedAt;
	out << state.inventory << state.manufacturing << state.accounting << state.reportHistory;
	out << state.lastEmployment << state.lastPopulationCycle << state.citizenSpecies << state.lastAttack;
	out << state.garrisonTroops;
	if(out.status() != QDataStream::Ok || bytes.size() > MaximumStateBytes)
	{ error = QStringLiteral("Legacy city state exceeds its envelope limit."); return false; }
	output = bytes;
	return true;
}

bool DecodeState(const QByteArray& bytes, State& output, QString& error)
{
	error.clear();
	if(bytes.size() > MaximumStateBytes)
	{ error = QStringLiteral("Legacy city state exceeds its envelope limit."); return false; }
	QDataStream in(bytes);
	in.setVersion(QDataStream::Qt_4_1);
	quint16 magic = 0; quint8 version = 0;
	in >> magic >> version;
	State state;
	if(magic != 0x4c43 || !SupportedStateVersion(version) || !String(in, state.name))
	{ error = QStringLiteral("Unsupported or malformed legacy city envelope."); return false; }
	in >> state.controllingEmpire >> state.allegianceEmpire >> state.loyalPopulation >> state.population
		>> state.satisfaction >> state.capacitor >> state.lastVisit;
	quint32 count = 0;
	in >> count;
	if(in.status() != QDataStream::Ok || count > MaximumDevelopments || quint64(count) * 43 > quint64(in.device()->bytesAvailable()))
	{ error = QStringLiteral("Invalid legacy development count."); return false; }
	for(quint32 i = 0; i < count; ++i)
	{
		Development dev; quint8 constructed = 0; quint32 doors = 0;
		in >> dev.id >> dev.designId >> dev.industryId >> dev.materialId >> dev.location
			>> dev.altitudeMSL >> dev.facingRadians >> dev.minimumDeck >> dev.maximumDeck
			>> constructed >> dev.completedLevels >> doors;
		if(in.status() != QDataStream::Ok || constructed > 1 || doors > 65535 || quint64(doors) * 4 > quint64(in.device()->bytesAvailable()))
		{ error = QStringLiteral("Invalid legacy door collection."); return false; }
		dev.constructed = constructed != 0;
		// Consume the bounded obsolete test-tool list without restoring state.
		for(quint32 door = 0; door < doors; ++door) { quint32 ignored = 0; in >> ignored; }
		if(version >= 2)
		{
			quint16 components = 0;
			in >> dev.constructionProcessId >> dev.targetLevels >> dev.laborRemaining >> dev.halted
				>> dev.citizenWorker >> dev.lastPlayerLabor >> dev.roadLengthMeters >> dev.roadWidthMeters >> dev.roadSlab
				>> dev.roadStartAltitudeMSL >> dev.roadEndAltitudeMSL >> dev.biodomeRadiusMeters >> dev.demolished >> components;
			if(in.status() != QDataStream::Ok || components > 128 || quint64(components) * 12 > quint64(in.device()->bytesAvailable()))
			{ error = QStringLiteral("Invalid legacy construction bill."); return false; }
			for(int j = 0; j < components; ++j)
			{
				ConstructionComponent component;
				in >> component.commodityId >> component.effect >> component.quality >> component.quantity >> component.applied;
				dev.components.append(component);
			}
		}
		else dev.targetLevels = quint8(qBound(1, int(dev.completedLevels), 32));
		if(version >= 3)
		{
			quint8 citizenEnabled = 0;
			in >> dev.technology >> dev.employees >> dev.selectedManufacturingRecipe >> citizenEnabled;
			if(citizenEnabled > 1) { error = QStringLiteral("Invalid pending legacy manufacturing worker flag."); return false; }
			dev.manufacturingCitizenEnabled = citizenEnabled != 0;
		}
		state.developments.append(dev);
	}
	in >> count;
	if(in.status() != QDataStream::Ok || count > 4096 || quint64(count) * 21 > quint64(in.device()->bytesAvailable()))
	{ error = QStringLiteral("Invalid legacy flag count."); return false; }
	for(quint32 i = 0; i < count; ++i)
	{ Flag flag; in >> flag.empire >> flag.location >> flag.level >> flag.capturedAt; state.flags.append(flag); }
	if(!Bytes(in, state.inventory, MaximumStateBytes / 2) || !Bytes(in, state.manufacturing, MaximumStateBytes / 2)
		|| !Bytes(in, state.accounting, MaximumStateBytes / 2) || !Bytes(in, state.reportHistory, MaximumStateBytes / 2))
	{ error = QStringLiteral("Malformed legacy city state groups."); return false; }
	if(version >= 3)
	{
		in >> state.lastEmployment >> state.lastPopulationCycle;
		if(!Bytes(in, state.citizenSpecies, 4096))
		{ error = QStringLiteral("Invalid legacy city citizen species."); return false; }
		in >> state.lastAttack;
	}
	if(version >= 4) in >> state.garrisonTroops;
	if(in.status() != QDataStream::Ok || !in.atEnd() || !Valid(state, error))
	{ if(error.isEmpty()) error = QStringLiteral("Malformed legacy city state or trailing data."); return false; }
	output = state;
	return true;
}
}
