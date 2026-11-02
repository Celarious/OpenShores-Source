#ifndef AuLegacyCityEnclosure_H
#define AuLegacyCityEnclosure_H

#include "auatom.h"
#include "aulegacycitystate.h"

class DaWorldLL;
class AuPoint;

namespace AuLegacyCity
{
// Snapshot completed domes before taking a city state write lock. A dome in
// another city on the same world can also shelter a construction site.
AUATOM_DLL QVector<Development> CompletedBiodomes(const DaWorldLL& world);
AUATOM_DLL bool InsideBiodome(const DaWorldLL& world, const AuGlobeLLF& location,
	const QVector<Development>& domes);
AUATOM_DLL quint8 ConstructionEnvironment(const DaWorldLL& world, const Development& development);
// DaCity::EnclosureNeeded: 0 open air, 1 unbreathable atmosphere, 2 sea bottom.
AUATOM_DLL int EnclosureNeeded(const DaWorldLL& world, const AuPoint& cityLocation);
}

#endif
