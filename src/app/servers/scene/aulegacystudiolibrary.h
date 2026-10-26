#ifndef AuLegacyStudioLibrary_H
#define AuLegacyStudioLibrary_H

#include "auid.h"
#include "aulegacystudioprotocol.h"

// These helpers own a temporary database connection in the calling thread.
// Call them without an avatar, etherium, or game-data lock held.
bool ListLegacyStudioLibrary(const AuId& architect,
	QVector<AuLegacyStudioProtocol::LibraryEntry>& entries, bool& truncated, QString& error);
bool LoadLegacyStudioLibrary(quint32 designId, const AuId& architect,
	AuLegacyShip::Design& design, QString& error);

#endif
