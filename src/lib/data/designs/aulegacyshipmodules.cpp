#include "aulegacyshipedit.h"
#include "aulegacyshipmoduleoptions.h"
#include "aulegacyshipsave.h"
#include <algorithm>

namespace AuLegacyShip
{
namespace
{
QMap<quint16, quint16> CurrentMappedModules(const QSet<quint16>& active,
	const QVector<ModulePair>& stored, quint16 fallback)
{
	QMap<quint16, quint16> result;
	for(quint16 key : active) result.insert(key, fallback);
	// The original stream reader keeps the first occurrence of a duplicate key.
	for(int i = stored.size() - 1; i >= 0; --i)
	{
		const ModulePair& pair = stored.at(i);
		if(active.contains(pair.first)) result[pair.first] = pair.secondNormalized;
	}
	return result;
}

bool UpdateMappedModules(QVector<ModulePair>& stored, const QSet<quint16>& active,
	const QMap<quint16, quint16>& changes, quint16 fallback, bool& changed)
{
	if(changes.isEmpty()) return true;
	for(auto it = changes.cbegin(); it != changes.cend(); ++it)
		if(!active.contains(it.key()) || !IsDesignerModule(DesignerModuleSystem(fallback), it.value(), false))
			return false;

	QSet<quint16> missing = active;
	for(ModulePair& pair : stored)
	{
		if(!missing.remove(pair.first)) continue;
		const auto choice = changes.constFind(pair.first);
		if(choice == changes.cend() || choice.value() == pair.secondNormalized) continue;
		pair.secondRaw = pair.secondNormalized = choice.value();
		changed = true;
	}
	// The original analysis inserts newly drawn systems in ascending key order.
	// Preserve their insertion order for the original hash map's serialized bytes.
	QList<quint16> keys = missing.values();
	std::sort(keys.begin(), keys.end());
	for(quint16 key : keys)
	{
		ModulePair pair;
		pair.first = key;
		pair.secondRaw = pair.secondNormalized = changes.value(key, fallback);
		stored.append(pair);
		changed = true;
	}
	return true;
}

void TrimMissing(QVector<ModulePair>& stored, const QSet<quint16>& active)
{
	const auto missing = [&active](const ModulePair& module) { return !active.contains(module.first); };
	if(std::find_if(stored.cbegin(), stored.cend(), missing) != stored.cend())
		stored.erase(std::remove_if(stored.begin(), stored.end(), missing), stored.end());
}
}

void TrimMissingModuleSystems(Design& design)
{
	if(design.building || (design.header.firstPairs.isEmpty() && design.header.secondPairs.isEmpty())) return;
	QSet<quint16> bays, transporters;
	ModuleSystemKeys(design, bays, transporters);
	TrimMissing(design.header.firstPairs, bays);
	TrimMissing(design.header.secondPairs, transporters);
}

ModuleChanges CurrentModules(const Design& design)
{
	ModuleChanges result;
	const QVector<quint16> defaults = DefaultModuleSlots();
	for(int i = 0; i < defaults.size(); ++i)
		result.scalars.insert(i, ScalarModule(design, i));
	QSet<quint16> bays, transporters;
	ModuleSystemKeys(design, bays, transporters);
	result.bays = CurrentMappedModules(bays, design.header.firstPairs, 0x105);
	result.transporters = CurrentMappedModules(transporters, design.header.secondPairs, 0xf6);
	return result;
}

bool ApplyModuleChanges(Design& design, const ModuleChanges& modules, bool& changed, QString& error)
{
	changed = false;
	error.clear();
	const QVector<quint16> defaults = DefaultModuleSlots();
	Header candidate = design.header;
	if(design.building || (!candidate.scalarSlots.isEmpty() && candidate.scalarSlots.size() != defaults.size()))
	{
		error = QStringLiteral("The design has no valid spacecraft module header.");
		return false;
	}
	bool modified = false;
	for(auto it = modules.scalars.cbegin(); it != modules.scalars.cend(); ++it)
	{
		const int index = it.key();
		if(index >= defaults.size() || !IsDesignerModule(DesignerModuleSystem(defaults.at(index)), it.value(), false))
		{
			error = QStringLiteral("This module is unavailable for the selected spacecraft system.");
			return false;
		}
		if(it.value() == ScalarModule(design, index)) continue;
		if(candidate.scalarSlots.isEmpty()) candidate.scalarSlots = defaults;
		candidate.scalarSlots[index] = it.value();
		modified = true;
	}
	if(!modules.bays.isEmpty() || !modules.transporters.isEmpty())
	{
		QSet<quint16> bays, transporters;
		ModuleSystemKeys(design, bays, transporters);
		if(!UpdateMappedModules(candidate.firstPairs, bays, modules.bays, 0x105, modified)
			|| !UpdateMappedModules(candidate.secondPairs, transporters, modules.transporters, 0xf6, modified))
		{
			error = QStringLiteral("The selected bay or transporter is unavailable for this module.");
			return false;
		}
		if(qint64(design.rooms.size()) + design.details.size() + candidate.firstPairs.size()
			+ candidate.secondPairs.size() > MaximumRecords)
		{
			error = QStringLiteral("The design exceeds the legacy record safety limit.");
			return false;
		}
	}
	if(modified) design.header = candidate;
	changed = modified;
	return true;
}
}
