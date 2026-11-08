#include "dasentient.h"
#include "aucommpacket.h"
#include "auglobal.h"
#include "dalegacycity.h"
#include "daworld.h"
#include "dasector.h"

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
		if(city->IsSpacecraftOnSlab(*ship))
		{
			PilotHoldPosition(ship);
			if(!ship->IsMoving()) DoShipArrivedAtDestination(ship, city->Id());
			return;
		}
	}
	const AuPoint landing = city->FindSpacecraftLandingSite(*ship);
	if(landing.IsZero())
	{
		PilotHoldPosition(ship);
		if(const DaSector* sector = ship->FindParentSector())
		{
			AuCommChat message(tr("Captain, no clear spacecraft slab is available at %1. Holding position until a landing spot clears.")
				.arg(city->Name()), AuCommStdChannel::CrewChannelName(), Id(), Name(), sector->Id(), Galaxy(),
				AuCommStdChannel::SolarSystem, AuCommStdChannel::CrewMates);
			SayNoRepeat(message, AuVisage::Perplexed);
		}
	}
	else PilotToLandingSite(ship, planet, landing);
}
