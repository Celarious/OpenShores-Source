#ifndef AuLegacyStudioLibrary_H
#define AuLegacyStudioLibrary_H

#include "auid.h"
#include "aulegacystudioprotocol.h"

// These helpers own a temporary database connection in the calling thread.
// Call them without an avatar, etherium, or game-data lock held.
bool SaveLegacyBuildingStudioDesign(const AuLegacyShip::Design& design, quint32& libraryId, QString& error);
bool FinalizeLegacyBuilding(quint32 id, QString& error);
QByteArray LegacyStudioBuildingSource(quint32 id, bool finalOnly, QString& error);
bool ListLegacyStudioLibrary(const AuId& architect, bool building,
	QVector<AuLegacyStudioProtocol::LibraryEntry>& entries, bool& truncated, QString& error);
bool LoadLegacyStudioLibrary(quint32 designId, const AuId& architect, bool building,
	AuLegacyShip::Design& design, QString& error);

#endif
