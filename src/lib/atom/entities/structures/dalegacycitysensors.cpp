#include "dalegacycity.h"
#include "auglobal.h"
#include "ausensorblip.h"
#include "dasolarsystem.h"
#include "dgdata.h"
#include <algorithm>

float DaLegacyCity::SensorRange() const
{
	if(!g_Au.IsLegacyGeneration() || IsDead()) return 0;
	const auto state = Snapshot();
	quint32 airport = 0, military = 0;
	quint8 technology = 0;
	for(const auto& development : state.developments)
	{
		if(development.demolished || !development.completedLevels) continue;
		if(development.industryId == 111) airport += development.completedLevels;
		else if(development.industryId == 11 || development.industryId == 12) military += development.completedLevels;
		else continue;
		technology = qMax(technology, development.technology);
	}
	// DaCity::SensorRange 0x2cdc80 and SensorTechLevel 0xfbd90.
	return (1.f + technology * .05f) * float(airport + military * 10) * 42500.f * .5f;
}

void DaLegacyCity::SensorScan(AuSensorBlipList& blips, const AuPoint& scannerLoc, const AuId& scannerId,
	quint32 scannerEmpire, int scannerDomain, const AuId& scannerWorld, float scannerSensorRange,
	float scannerWeaponRange, int scanContent, const AuId& lockedBlip,
	unordered_map<quint32, POLITICAL_STANCE, AuEmpireIdHash>& stanceCache) const
{
	Q_UNUSED(scannerLoc);
	Q_UNUSED(scannerId);
	Q_UNUSED(scannerDomain);
	Q_UNUSED(scannerWorld);
	Q_UNUSED(scannerSensorRange);
	Q_UNUSED(scannerWeaponRange);
	if(!g_Au.IsLegacyGeneration() || IsDead() || (!(scanContent & 7) && (!(scanContent & 8) || !Is(lockedBlip)))) return;
	const auto state = Snapshot();
	AuId capital;
	const auto& data = g_Au.LockDataForRead();
	data.EmpireCapital(state.controllingEmpire, capital);
	const bool empireCapital = !capital.IsNull() && capital == Id();
	const bool sectorCapital = data.IsEmpireSectorCapital(state.controllingEmpire, Id());
	g_Au.UnlockData();
	// Original cities without an airport are concealed unless they are the
	// empire capital. A sector-capital designation alone does not reveal one.
	if(!empireCapital && !std::any_of(state.developments.cbegin(), state.developments.cend(),
		[](const AuLegacyCity::Development& development) { return development.industryId == 111
			&& development.completedLevels && !development.demolished; })) return;
	const auto stance = PoliticalStance(scannerEmpire, state.controllingEmpire, stanceCache);
	blips.Add(AuBlipLegacyCity(state.name, quint16(state.population), empireCapital, sectorCapital,
		state.controllingEmpire, stance, Id(), EnvLocation()));
}

void DaLegacyCity::TockSensors()
{
	if(!g_Au.IsLegacyGeneration() || !g_Au.IsSceneWorker() || IsDead() || !StationsPowered()) return;
	const float range = SensorRange();
	auto* system = FindParentSolarSystem();
	const quint32 empire = EmpireId();
	if(!system || !empire || range <= 0 || !system->IsEnemyEmpirePresent(empire)) return;
	AuSensorBlipList blips;
	AuId scannerWorld;
	const int domain = GunnerDomain(scannerWorld);
	unordered_map<quint32, POLITICAL_STANCE, AuEmpireIdHash> stances;
	system->SensorScan(blips, EnvLocation(), Id(), empire, domain, scannerWorld, range, 0, 1, AuId(), stances);
	ReportEnemySensorContact(system, blips, range);
}
