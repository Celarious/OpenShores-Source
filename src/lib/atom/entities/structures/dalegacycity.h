#ifndef DaLegacyCity_H
#define DaLegacyCity_H

#include "daatom.h"
#include "aulegacycitystate.h"
#include "aulegacybuildingpaths.h"
#include "austation.h"
#include <QHash>
#include <QMap>
#include <QScopedPointer>

class AuCityReport;
class AuCondition;
class AuCityReportHistory;
class AuGlobeRock;
class AuItemHash;
class AuItem;
class AuOrdnance;
class AuPointArray;
class AuResKey;
class DaItem;
class DaVehicle;
class DaVessel;
class DaNpc;
class DaSentient;
class AuCommAcceptRepair;
class AuCommAccept;
class AuSysId;
class AuLegacyCityCatalogue;
namespace AuLegacyCity { struct CitizenConstructionFacts; struct ProductionState; }
namespace AuLegacyCity { struct PopulationRuntimeFacts; struct PopulationRuntimeReport; struct AccountingState; struct EmploymentTotals; }
namespace AuLegacyCity { struct CityConnection; class PowerImportBatch; }
namespace AuLegacyCity { struct WealthLevyOrder; }
namespace AuLegacyCity { struct DevelopmentSupplyFacts; struct AccountingState; }
namespace AuLegacyCityProtocol { struct Request; struct Reply; }
namespace AuLegacyConstructionMaterials { class Batch; }

// City-owned legacy developments have no DaBd identity or modern economics.
// Legacy construction remains city-owned and uses the historical GD recipes.
class AUATOM_DLL DaLegacyCity : public DaAtom
{
public:
	DaLegacyCity();
	~DaLegacyCity() override;
	DaAtom* Clone() const override;
	ATOM_TYPE AtomType() const override { return ATOM_LegacyCity; }
	bool IsEthereal() const override { return false; }
	bool CanSetInMotion() const override { return false; }
	// Surface rendering dispatches only atmosphere members. Static cities still
	// need an environment query to establish that membership on globes and rings.
	bool DoesQueryEnvironment() const override { return true; }
	bool IsVisibleNearWorld() const override { return true; }
	bool IsInScene(const DaPerson* sceneAtom, const AuPoint& cameraEnvironmentLocation) const override;
	bool IsTicker() const override { return false; }
	unsigned int IsTocker() const override { return 1; }
	void Tock(const AuTime& last, const AuTime& now, const AuScripts& scripts, DtTockTransactions& transactions) override;
	void RxChatMessage(const AuCommPacket* message, quint32& countSupplied, bool firstInGroup = true) override;
	QString Name() const override;
	void SetName(const QString& name) override;
	quint32 EmpireId() const override;
	quint32 Population() const override;
	// Same eligibility for local respawn and the worker's cross-server query.
	int CanSpawnHere(quint32 empireId) const;
	// A tied development uses its original room/path spawn rules; other city
	// destinations use the historical random ground point near the town square.
	AuPoint SpawnLocation(const AuGlobeLLF* home = nullptr) const;
	AuLegacyCity::State Snapshot() const;
	// Original airport perimeter candidates and road fallback, in the world's frame.
	void AirportTerminalLandingSpots(QList<AuPoint>& spots) const;
	AuPoint FindSpacecraftLandingSite(const DaVessel& ship) const;
	// A finished building establishes the settlement; flags and landscaping do not.
	bool HasCompletedBuilding() const;
	void RegisterEmpireVisit(quint32 empireId, qint64 now);
	// Newly authored shared display envelope; no 2016 database binary import.
	bool ReportHistory(AuCityReportHistory& result) const;
	static bool ReadReportHistory(const QByteArray& bytes, AuCityReportHistory& result);
	static bool WriteReportHistory(const AuCityReportHistory& history, QByteArray& bytes, QString& error);
	// Gather world/global facts before the city transaction. Completion reads
	// only the supplied state and inventories and preserves the precycle snapshot.
	AuCityReport GatherCityReport(const AuLegacyCity::State& state,
		const AuLegacyCity::PopulationRuntimeFacts& facts) const;
	bool CompleteCityReport(AuCityReport& report, const AuLegacyCity::State& state,
		const AuItemHash& initialInventory, const AuItemHash& inventory,
		const AuLegacyCity::ProductionState& production, const AuLegacyCity::AccountingState& accounting,
		const AuLegacyCity::PopulationRuntimeReport& population, const AuLegacyCity::EmploymentTotals& employment,
		qint64 now, QString& error) const;
	void MailLastReport(const AuId& toId) const;
	void SendCityReport(const AuCityReport& report, const AuCityReport* previous = nullptr);
	bool SetState(const AuLegacyCity::State& state, QString& error);
	bool BeginConstruction(DaPerson& person, quint8 industryId, quint8 processId, quint32 designId,
		quint16 materialId, const AuGlobeLLF& location, double headingRadians, quint8 levels, QString& error,
		double roadLengthMeters = 10, double roadWidthMeters = 10, bool roadSlab = false, double biodomeRadiusMeters = 100,
		quint16 manufacturingRecipe = 0, bool manufactureCitizen = true);
	bool ExtendConstruction(DaPerson& person, quint32 developmentId, quint8 additionalLevels, QString& error,
		quint16 manufacturingRecipe = 0, bool manufactureCitizen = true);
	bool SetConstructionHalted(DaPerson& person, quint32 developmentId, bool halted, QString& error);
	bool ContributeConstructionLabor(DaPerson& person, quint32 developmentId, QString& error, bool* accepted = nullptr);
	bool FetchConstructionMaterials(DaPerson& person, quint32 developmentId, QString& error);
	bool ChangeRoadConstruction(DaPerson& person, quint32 developmentId, quint8 processId, QString& error);
	bool EmployConstructionWorker(DaPerson& person, quint32 developmentId, bool enabled, QString& error);
	bool DemolishDevelopment(DaPerson& person, quint32 developmentId, bool clearLand, QString& error);
	bool SetIndustryProcess(DaPerson& person, quint32 developmentId, quint16 processSlot,
		quint16 processId, quint8 minimumTechnology, bool enabled, QString& error);
	bool SetSpacecraftProcess(DaPerson& person, quint32 developmentId, quint16 processSlot,
		quint32 designId, quint8 minimumTechnology, bool enabled, QString& error);
	bool BuySpacecraft(DaPerson& person, quint32 developmentId, quint32 designId, QString& error);
	bool OperateIndustryProcess(DaPerson& person, quint32 developmentId, quint16 processSlot,
		bool fetch, bool run, QString& error);
	// TEMPORARY ARCHITECT MANUFACTURING TEST: remove after testing.
	bool SkipIndustryProcessWaitForTesting(DaPerson& person, quint32 developmentId, quint16 processSlot, QString& error);
	bool OperateBank(DaPerson& person, const AuLegacyCityProtocol::Request& request,
		AuLegacyCityProtocol::Reply& reply);
	void ServiceVehicle(DaVehicle& vehicle, DaPerson& person);
	bool HasOfficerAcademy() const;
	int SpacecraftPersonnelAvailable(const DaVessel& ship, NPC_ROLE role, const DaPerson* loader = nullptr) const;
	int BoardSpacecraftPersonnel(DaVessel& ship, NPC_ROLE role, int requested,
		const DaPerson* loader = nullptr, const DaSentient* commander = nullptr);
	int DebarkSpacecraftPassengers(DaVessel& ship, int requested, quint32* transportFee = nullptr, bool requireHomes = true);
	bool RxPersonnelChat(const AuCommPacket* message, quint32& countSupplied);
	bool RxSpacecraftBlueprintMessage(const AuCommPacket* message);
	void AcceptSpacecraftBlueprint(DaPerson& person, const AuCommAccept& message);
	void EquipSpacecraftPersonnel(DaNpc& npc);
	bool RxSpacecraftServiceMessage(const AuCommPacket* message);
	void AcceptSpacecraftRepair(DaPerson& person, const AuCommAcceptRepair& message);
	void AcceptSpacecraftCargo(DaPerson& person, const AuCommAccept& message);
	bool CanAccessSpacecraft(const DaVessel& ship) const;
	bool IsSpacecraftOnSlab(const DaVessel& ship) const;
	void SupplySpaceStation(DaVessel& ship);
	bool HasSpacecraftService(quint8 industry, quint8 minimumTechnology = 0) const;
	int RepairSpacecraft(DaVessel& ship, const AuSysId* system = nullptr, quint8 minimumQuality = 0,
		quint32 maximumPrice = 0xffffffffu, bool perform = true, quint16* seconds = nullptr);
	int TransferSpacecraftCargo(DaVessel& ship, COMMODITYID commodity, int count, bool buy,
		bool mission = true, const AuItem* exact = nullptr, quint32 maximumPrice = 0xffffffffu);
	DaVessel* FindShipInHangar(const DaPerson& person, quint32* count = nullptr) const;
	bool IsHangarShipValid(const DaVessel& ship) const;
	bool HangarSpacecraft(DaPerson& person, const AuId& shipId, bool launch, bool takeCaptain, QString& message);
	bool CanSupplyVehicleOrdnance(const DaVehicle& vehicle) const;
	void ListVehicleOrdnance(const DaVehicle& vehicle, AuItemHash& ordnance) const;
	// Caller holds the vehicle's ammunition lock; commits the stock exchange
	// before returning one owned ammunition item for the vehicle to load.
	AuItem* ExchangeVehicleOrdnance(const DaVehicle& vehicle, const AuItem& requested, const AuOrdnance& previous);
	void ReceiveTaxLevy(double amount);
	void ApplyWealthLevy(const AuLegacyCity::WealthLevyOrder& order);
	bool Revolt(const QString& reason);
	void NotifyRevolt(quint32 oldEmpire, quint32 newEmpire, bool captured, const QString& reason);
	bool ReceiveConfiscatedWealth(quint32 empire, double amount);
	AuLegacyCity::CitizenConstructionFacts GatherCitizenConstructionFacts(const AuLegacyCity::State& state,
		const AuLegacyCity::Development& development, qint64 now, bool materialsOnly = false) const;
	AuLegacyCity::DevelopmentSupplyFacts GatherDevelopmentSupplyFacts(const AuLegacyCity::State& state,
		const AuLegacyCity::Development& development, qint64 now) const;
	bool SupplyDevelopmentMaterials(AuLegacyCity::State& state, AuLegacyCity::Development& development,
		AuItemHash& inventory, AuLegacyCity::ProductionState& production, AuLegacyCity::AccountingState& accounting,
		const AuLegacyCity::DevelopmentSupplyFacts& facts, AuLegacyConstructionMaterials::Batch& materials) const;
	// Applies to caller-owned transaction state; no city/world/environment locks
	// are acquired here. Publish/finalize physical items after committing it.
	bool ApplyCitizenConstruction(AuLegacyCity::State& state, AuLegacyCity::Development& development,
		AuItemHash& inventory, AuLegacyCity::ProductionState& production,
		const AuLegacyCity::CitizenConstructionFacts& facts, bool& physicalChanged,
		AuLegacyConstructionMaterials::Batch& materials, bool performLabor = true) const;
	static int ConstructionLevelLimit(quint8 industryId, quint16 materialId);
	// Shared by authoritative placement and the client's construction preview.
	static bool RoadCrossesSeaSurface(const DaWorldLL& world, double startAltitudeMSL, double endAltitudeMSL);
	// Resolves the submitted road's endpoints and grades, then checks its
	// destination terrain using the selected width. No city state is changed.
	static int PrepareRoadPlacement(const DaWorldLL& world, AuLegacyCity::Development& road, QString& message);
	static bool ConstructionSitesOverlap(const DaWorldLL& world,
		const AuLegacyCity::Development& site, const AuMatrix4x4& placement,
		const AuLegacyCity::Development& existing, const AuMatrix4x4& otherPlacement);
	void operator>>(QDataStream& out) const override;
	void operator<<(QDataStream& in) override;
	bool Refresh(DaAtom* counterpart, AuAtomRenderInfo* ari) override;
	void TxUpdate(QDataStream& stream, const AuTime& lastUpdate,
		const AuElapsedTimer& lastTx, const DaPerson& sceneAtom) override;
	void RxUpdate(QDataStream& stream, bool firstUpdate) override;
	void Insert(QSqlDatabase& db) const override;
	void Update(QSqlDatabase& db) const override;
	static bool LoadSubatoms(AuEther& ether, QSqlDatabase& db, const AuId& world,
		QString* error = nullptr);
	static bool RegisterPublicDesigns(QSqlDatabase& db, QString& error);
	static bool ValidateStateData(const QByteArray& bytes, QString& error);
	bool RoughExtents(AuBox& extents) const override;
	bool IsSightTarget(const DaAtom* observer) const override;
	DaAtom* SightTarget(const DaAtom* observer, const AuPoint& eyeLocation,
		const AuMatrix4x4& model, DaAtom* best, AuPoint& sightedLocation,
		unsigned int& sightedPart, bool& sightedShield, AuPoint* surfaceNormal) const override;
	bool IsPathBlocked(const AuPoint& fromWorld, const AuPoint& toWorld) const;
	bool IsPathBlocked(const AuPoint& fromWorld, const AuPoint& toWorld,
		AuPoint& hitLocation, AuPoint& hitNormal, const DaAtom* actor) override;
	bool FilterDestination(const DaAtom& actor, const AuPoint& fromWorld,
		AuPoint& toWorld, const AuBox& body) const;
	bool TestRoadConnection(const DaLegacyCity& other) const;
	// Original square site and its ten-meter terrain blend margin. Both height
	// and flora queries use this path, independently of rendered room floors.
	double AdjustSiteTerrain(const DaWorldLL& world, double altitudeMSL, const AuGlobeLL& ll,
		int& levelSites, bool& hideRocks, int& hideGrass, bool& hideShrubs,
		bool& hideTrees, WORLDTEX_TYPE& pavementType) const;
	// Signed distance in feet from 'll' to the nearest edge of pavement that
	// AdjustSiteTerrain paints, negative inside. False if no site paints pavement.
	bool SitePavementEdgeDistance(const DaWorldLL& world, const AuGlobeLL& ll, float& dist) const;
	// Visual rock distributions only: retain the procedural resource nodes used
	// by placement, mining and labor, but clear padded building footprints.
	void RemoveInteriorRocks(const DaWorldLL& world, QMap<AuResKey, AuGlobeRock>& rocks) const;
	void QueryLegacyEnvironment(const DaAtom& actor, const AuPoint& locationWorld,
		AuQueryEnvironment& result) const;
	// Authored equipment stays in the world's frame, as in the original city.
	bool CanEnterConsole(const DaPerson& who, quint16& stationId, AuStation& station, quint16& which) const;
	bool Station(const DaCreature& who, AuStation& station, AuLegacyCity::Development* development = nullptr,
		AuMatrix4x4* placement = nullptr, quint16* roomId = nullptr) const;
	bool StationsPowered() const;
	int ReserveStationPower(int requested, AuItemHash& reserved);
	void RefundStationPower(AuItemHash& reserved, int used);
	bool TransporterPads(const DaCreature& who, AuMatrix4x4& frameWorld,
		AuPointArray& padsLocal, float& maxRange) const;
	int BeamUp(DaPerson& person, int countRequested);
	int EnergizeStationTransporter(const AuMatrix4x4& frameWorld, const AuPointArray& padsLocal, float maxRange,
		bool isFrom, int padLimit, DaAtom* target, const AuPoint& location, const AuPoint& up,
		quint16 targetWhich, AuPointArray& targetPads, bool ignoreShields, bool targetAtBuilding);
	bool WeaponStation(const DaCreature& who, AuLegacyCity::Development& development,
		COMMODITYID& module, AuPoint& emitterWorld, float& range) const;
	AuItem* WeaponAmmunition(COMMODITYID module) const;
	bool FireStationWeapon(DaPerson& gunner, const AuId& target, int powerSetting);
	bool HospitalPatient(const DaCreature& patient, quint32& developmentId,
		AuStation& medic, quint16& medicStationId) const;
	DaCreature* FindPatient(const DaCreature& doctor) const;
	void TreatHospitalPatient(DaCreature& doctor, DaCreature& patient, int procedure = -1,
		const AuCondition* selectedCondition = nullptr);
	// Route positions are returned in the parent world's frame; source cell and
	// opening identities remain in the development's original graph.
	bool Route(quint32 developmentId, const AuPoint& fromWorld, const AuPoint& toWorld,
		QVector<AuLegacyBd::Paths::RoutePoint>& points, bool useGroundLevelDoors = true) const;
	void PaintOnSurfaceMap(const DaWorldLL& world, QPainter& painter, unsigned int width, unsigned int height) const override;
	void DeleteDeviceObjects(AuAtomRenderInfo& ari) override;
	void GetRenderLightsRecursive(const DaPerson& person, const AuMatrix4x4& model,
		QList<AuGLSun>& suns, QList<AuGLLight>& lights) const override;
	void RenderPreTerrain(const AuMatrix4x4& projection, AuAtomRenderInfo& ari,
		AuModelMatrixHash& matrices, const DaPerson* sceneAtom);
	void RenderEnvironmentObject(const AuMatrix4x4& projection, AuAtomRenderInfo& ari,
		AuModelMatrixHash& matrices, const DaPerson* sceneAtom, AuRenderLastMap& last,
		RENDERPASS pass, DaAtomList& sc, DaAtomList& scFog, DaAtomList& scGlobe,
		DaAtomList& scGlobeFog, DaAtomList& scRing, DaAtomList& scRingFog) override;
private:
	void RenderCity(const AuMatrix4x4& projection, AuAtomRenderInfo& ari,
		AuModelMatrixHash& matrices, const DaPerson* sceneAtom, bool beforeTerrain);
	friend class AuLegacyCity::PowerImportBatch;
	DaLegacyCity(const DaLegacyCity& city);
	mutable QReadWriteLock m_CityLock;
	AuLegacyCity::State m_City;
	quint64 m_CityRevision = 1;
	QHash<quint32, qint64> m_ConstructionLaborTimes;
	bool m_TradeMonitored = false;
	QHash<QByteArray, QPair<quint32, qint64>> m_ShopPrices;
	quint32 CachedShopPrice(const AuLegacyCityCatalogue& catalogue, const AuLegacyCity::State& state,
		const AuItemHash& inventory, const AuLegacyCity::ProductionState& production, const AuItem& item, quint8 industry);
	struct Runtime;
	mutable QScopedPointer<Runtime> m_Runtime;
	bool ApplyPayload(const QByteArray& bytes, QString& error);
	void UpdateSiteTerrain(AuAtomRenderInfo& ari, bool removed = false);
	void Store(QSqlDatabase& db, bool insert) const;
	bool OperateConstruction(DaPerson* person, quint32 developmentId, bool fetch, bool labor, QString& error, bool* accepted = nullptr);
	void TockEconomy(qint64 previous, qint64 now);
	void TockHospitalPatients();
	void ImportCityGoods(const QVector<AuLegacyCity::CityConnection>& connections);
	bool ReplaceFinishedDevelopment(const AuLegacyCity::Development& development,
		quint8 expectedProcess, quint8 expectedLevels, QString& error);
};
#endif
