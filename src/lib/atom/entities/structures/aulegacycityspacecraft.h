#ifndef AuLegacyCitySpacecraft_H
#define AuLegacyCitySpacecraft_H

#include "auatom.h"
#include "auzoneresource.h"
#include <QHash>
#include <memory>

class AuItemHash;
class AuScReport;
class DaLegacyCity;
class DaPerson;
class DaVessel;
class DaWorldLL;
namespace AuLegacyCity
{
struct State;
struct ProductionState;
struct ProductionProcess;
struct ProductionOutput;
struct AccountingState;

// The historical replacement ordering is design TL, followed by design id.
AUATOM_DLL bool NewerSpacecraftDesign(const AuScReport& design, const DaVessel& previous);
AUATOM_DLL bool CarriedSpacecraftBlueprint(const DaPerson& person, quint32 designId);
AUATOM_DLL bool AvailableSpacecraftBlueprint(const DaPerson& person, quint32 empire, quint32 designId);
AUATOM_DLL bool CanConfigureSpacecraftFactory(const DaPerson& person, const DaLegacyCity& city);

class AUATOM_DLL SpacecraftProductionBatch
{
public:
	SpacecraftProductionBatch();
	~SpacecraftProductionBatch();
	qint32 Apply(ProductionProcess& process, const ProductionOutput& output,
		const State& state, AuItemHash& inventory, AccountingState& accounting);
	void Publish();
private:
	struct Data;
	std::shared_ptr<Data> m_Data;
	friend bool PrepareSpacecraftProduction(DaLegacyCity&, DaWorldLL&, const State&, const ProductionState&,
		const QHash<quint32, AuZoneResource>&, qint64, SpacecraftProductionBatch&, QString&, DaPerson*);
};

AUATOM_DLL bool PrepareSpacecraftProduction(DaLegacyCity& city, DaWorldLL& world, const State& state,
	const ProductionState& production, const QHash<quint32, AuZoneResource>& resources,
	qint64 now, SpacecraftProductionBatch& output, QString& error, DaPerson* buyer = nullptr);
}
#endif
