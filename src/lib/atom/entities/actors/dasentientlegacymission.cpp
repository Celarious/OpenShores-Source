#include "dasentient.h"
#include "auglobal.h"
#include "dalegacycity.h"
#include "daworld.h"

void DaSentient::PilotToLegacyCity(DaVessel* ship, const DaLegacyCity* city, const DaWorld* planet)
{
	// DaSentient::PilotToCity, AuAtom13:18039af10.
	if(!g_Au.IsLegacyGeneration() || !ship || !city || !planet) return;
	if(ship->MissionOrderType() == AuMissionOrder::PatrolCities)
	{
		AuPoint hover = city->Transform().Translation();
		if(hover.IsZero()) { PilotHoldPosition(ship); DoShipArrivedAtDestination(ship, city->Id()); return; }
		planet->ElevateLoc(hover, planet->CGAltitude(hover) + 100 * FeetPerMeter() + ship->GroundHeightAboveCG());
		PilotToHoverSite(ship, planet, hover);
		return;
	}
	if(ship->IsOnGround())
	{
		bool arrived = false;
		if(ship->MissionOrderType() == AuMissionOrder::GoToCityAnywhere)
		{
			AuPoint location;
			arrived = ship->GetLocationRelativeToParent(location, planet)
				&& location.Distance(city->Transform().Translation()) < 4900;
		}
		else
			arrived = city->IsSpacecraftOnSlab(*ship);
		if(arrived)
		{
			PilotHoldPosition(ship);
			if(!ship->IsMoving()) DoShipArrivedAtDestination(ship, city->Id());
			return;
		}
	}
	const AuPoint landing = city->FindSpacecraftLandingSite(*ship);
	if(landing.IsZero()) PilotHoldPosition(ship);
	else PilotToLandingSite(ship, planet, landing);
}
