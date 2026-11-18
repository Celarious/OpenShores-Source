#ifndef AuLegacyGenerationBootstrap_H
#define AuLegacyGenerationBootstrap_H

#include "aulegacybd.h"
#include "aulegacycitycatalogue.h"
#include "auglobal.h"
#include <QCryptographicHash>

// Clients advertise support for the catalogue format, not a local SQL snapshot.
// The server sends its actual definitions before generation activation.
inline AuGameGenerationIdentity AuLegacyGenerationCapability()
{
	AuGameGenerationIdentity identity;
	identity.generation = AuGameGeneration::Legacy;
	identity.catalogueVersion = AuLegacyCityCatalogue::DataVersion() + QStringLiteral("-buildings-1");
	identity.manifestHash = QByteArray(32, '\0');
	return identity;
}

inline AuGameGenerationIdentity AuLegacyGenerationLoginCapability()
{
	const AuGameGenerationIdentity capability = AuLegacyGenerationCapability();
	const AuGameGenerationIdentity current = g_Au.GenerationIdentity();
	return current.generation == AuGameGeneration::Legacy
		&& current.catalogueVersion == capability.catalogueVersion && AuLegacyCityCatalogue::BuiltIn()
		? current : capability;
}

inline bool AuLegacyGenerationIdentityForCatalogue(const AuLegacyCityCatalogue& catalogue,
	AuGameGenerationIdentity& identity, QString& error)
{
	if(!catalogue.IsValid())
	{
		error = QStringLiteral("Legacy city catalogue validation did not produce a valid catalogue.");
		return false;
	}
	if(!AuLegacyBd::Catalog::LoadBuiltIn(error)) return false;
	identity = AuLegacyGenerationCapability();
	QCryptographicHash hash(QCryptographicHash::Sha256);
	hash.addData("Legacy game generation manifest 1\n");
	hash.addData(catalogue.SourceSha256());
	hash.addData("\n");
	hash.addData(AuLegacyBd::Catalog::SourceSha256());
	identity.manifestHash = hash.result();
	return true;
}

inline bool AuPrepareLegacyGenerationIdentity(AuGameGenerationIdentity& identity, QString& error)
{
	if(!AuLegacyCityCatalogue::LoadBuiltIn(error)) return false;
	const AuLegacyCityCatalogue* catalogue = AuLegacyCityCatalogue::BuiltIn();
	if(!catalogue || !AuLegacyGenerationIdentityForCatalogue(*catalogue, identity, error)) return false;
	return g_Au.InstallLegacyCommodityDescriptors(catalogue->RuntimeCommodityDescriptors(), error);
}

// Check the wire length before Qt allocates a QByteArray for network input.
inline bool AuReadLegacyCataloguePayload(QDataStream& in, QByteArray& payload)
{
	quint32 bytes = 0;
	if(in.status() != QDataStream::Ok || !in.device() || in.device()->bytesAvailable() < 4) return false;
	in >> bytes;
	if(bytes == 0xffffffffU)
	{
		payload.clear();
		return true;
	}
	if(bytes > quint32(AuLegacyCityCatalogue::MaximumSourceBytes)
		|| in.device()->bytesAvailable() < qint64(bytes)) return false;
	payload.resize(int(bytes));
	if(!bytes) return true;
	return in.readRawData(payload.data(), int(bytes)) == int(bytes) && in.status() == QDataStream::Ok;
}

#endif
