#include "aulegacycityenclosure.h"
#include "aulegacybiodome.h"
#include "auglobal.h"
#include "auterraininfo.h"
#include "daatomlist.h"
#include "dalegacycity.h"
#include "daworldll.h"

namespace AuLegacyCity
{
QVector<Development> CompletedBiodomes(const DaWorldLL& world)
{
	QVector<Development> result;
	if(!g_Au.IsLegacyGeneration()) return result;
	DaAtomList cities; world.ListNormalSubatoms(cities);
	for(const auto* atom : cities)
		if(const auto* city = dynamic_cast<const DaLegacyCity*>(atom))
			for(const auto& development : city->Snapshot().developments)
				if(development.industryId == 129 && development.constructed && !development.demolished)
					result.append(development);
	return result;
}

bool InsideBiodome(const DaWorldLL& world, const AuGlobeLLF& location, const QVector<Development>& domes)
{
	// DpBldBiodome::Contains 0x589510 measures a world-space chord at the
	// dome's stored altitude, adding the original two-metre containment margin.
	for(const auto& dome : domes)
	{
		const double radius = dome.biodomeRadiusMeters * FeetPerMeter() + AuLegacyBd::Biodome::ContainmentMargin;
		if(world.LocOfLL(location, dome.altitudeMSL).Distance2(world.LocOfLL(dome.location, dome.altitudeMSL))
			<= radius * radius) return true;
	}
	return false;
}

quint8 ConstructionEnvironment(const DaWorldLL& world, const Development& development)
{
	// The 2016 surcharge quantities are in ConstructionRequirements. Exempt
	// sheltered buildings as requested, using the same containment as the cage.
	if(development.industryId != 2 && development.industryId != 129
		&& InsideBiodome(world, development.location, CompletedBiodomes(world))) return 1;
	if(development.industryId == 2)
	{
		if(world.Hydrographics() && development.roadStartAltitudeMSL <= -32
			&& development.roadEndAltitudeMSL <= -32) return 4;
	}
	else
	{
		AuTerrainInfo terrain; world.QueryTerrain(terrain, development.location);
		if(terrain.IsWater() && world.SurfaceAltitudeMSL(development.location) <= -32) return 4;
	}
	return world.HasBreathableAtmosphere() ? 1 : 2;
}

int EnclosureNeeded(const DaWorldLL& world, const AuPoint& cityLocation)
{
	if(world.Hydrographics() && world.AltitudeMSL(cityLocation) <= -32) return 2;
	return world.HasBreathableAtmosphere() ? 0 : 1;
}
}
