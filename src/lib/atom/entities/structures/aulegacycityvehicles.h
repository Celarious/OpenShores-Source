#ifndef AuLegacyCityVehicles_H
#define AuLegacyCityVehicles_H

#include "auatom.h"
#include "auzoneresource.h"
#include <QHash>
#include <memory>

class AuItemHash;
class AuPoint;
class AuGlobeLLF;
class DaLegacyCity;
class DaWorldLL;
namespace AuLegacyCity
{
struct State;
struct ProductionState;
struct ProductionOutput;
struct Development;
AUATOM_DLL bool ProductionRoadEndpoints(const DaWorldLL& world, const Development& road,
	AuGlobeLLF& start, AuGlobeLLF& end);
AUATOM_DLL bool ProductionRoadContains(const DaWorldLL& world, const Development& road, const AuGlobeLLF& point);
AUATOM_DLL AuPoint ProductionSpawnOffset(int index);

// Original physical vehicle outputs; virtual mine/commercial/naval/tank
// registrations remain separate from these slab and water spawns.
AUATOM_DLL bool IsSpawnedProductionVehicle(quint16 commodity);

// Gather terrain, obstructions and unpublished vehicle geometry before taking
// city locks. Apply reserves a prepared position and fuel in transaction-owned
// state. Publish inserts atoms only after that transaction has committed.
class AUATOM_DLL VehicleProductionBatch
{
public:
	VehicleProductionBatch();
	~VehicleProductionBatch();
	bool Apply(quint32 development, quint16 slot, const ProductionOutput& output,
		const State& state, AuItemHash& inventory);
	void Publish(quint32 empire);
private:
	struct Data;
	std::shared_ptr<Data> m_Data;
	friend bool PrepareVehicleProduction(DaLegacyCity&, DaWorldLL&, const State&, const ProductionState&,
		const QHash<quint32, AuZoneResource>&, qint64, VehicleProductionBatch&, QString&);
};

AUATOM_DLL bool PrepareVehicleProduction(DaLegacyCity& city, DaWorldLL& world, const State& state,
	const ProductionState& production, const QHash<quint32, AuZoneResource>& resources,
	qint64 now, VehicleProductionBatch& output, QString& error);
}
#endif
