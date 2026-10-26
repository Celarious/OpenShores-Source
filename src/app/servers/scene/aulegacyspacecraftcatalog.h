#ifndef AuLegacySpacecraftCatalog_H
#define AuLegacySpacecraftCatalog_H

class QSqlDatabase;
class QString;
class AuLegacyScDesign;
class AuId;
class AuScReport;
class AuScDesignExchangeEntry;
namespace AuLegacyShip { struct Design; }
namespace AuLegacyScProtocol { struct Request; struct Reply; }

#include <QtGlobal>

// Persists an accepted payload independently of its acquisition method.
// The caller must authorize the operation and accept the bytes server-side.
bool StoreLegacySpacecraftDesign(QSqlDatabase& db, const QString& sourceName,
	const AuLegacyScDesign& design, quint32& id);

// Loads stored designs and publishes a validated, immutable server cache.
// Called once by the database thread during scene-server startup.
bool LoadLegacySpacecraftCatalog(QSqlDatabase& db);

// Called on the authenticated chat thread, using its own SQL connection.
void ProcessLegacySpacecraftRequest(const AuLegacyScProtocol::Request& request,
	AuLegacyScProtocol::Reply& reply);

// Creates or loads the persisted modern geometry derived from authoritative
// legacy bytes. The caller must authorize the architect before calling.
bool PrepareLegacySpacecraftSpawn(quint32 legacyId, const AuId& architectId,
	const QString& architectName, AuScReport& report, QString& error);

// Called after authorizing a saved, burnable legacy studio snapshot. Keep the
// original .ship as authority and publish its existing legacy runtime adapter.
bool SaveLegacyStudioDesign(const AuLegacyShip::Design& design, quint32& libraryId, QString& error);
bool PrepareLegacyStudioMedia(quint32 libraryId,
	AuScDesignExchangeEntry& exchange, QString& error);

#endif // AuLegacySpacecraftCatalog_H
