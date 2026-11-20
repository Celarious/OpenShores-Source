#include "aulegacystudiolibrary.h"
#include "aulegacyscdesign.h"
#include "aulegacyscprotocol.h"
#include "aulegacybd.h"
#include "aulegacyshipsave.h"
#include "audesign.h"
#include "auglobal.h"
#include "dgdata.h"
#include <QDebug>
#include <QCryptographicHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
#include <limits>
#include <utility>

namespace
{
// Match the existing legacy catalog's connection settings. Each temporary
// connection is created, used, and removed on the requesting chat thread.
class LibraryConnection
{
public:
	LibraryConnection() : name(QStringLiteral("LegacyStudioLibrary-%1").arg(QUuid::createUuid().toString()))
	{
		db = QSqlDatabase::addDatabase("QPSQL7", name);
		db.setDatabaseName("auUniverse");
		db.setHostName(qEnvironmentVariable("AU_UNIVERSE_DB_HOST", "UniverseDb.Hazeron.net"));
		db.setConnectOptions("connect_timeout=5");
	}
	~LibraryConnection()
	{
		db.close(); db = QSqlDatabase(); QSqlDatabase::removeDatabase(name);
	}
	bool Open() { return db.open("audog", qEnvironmentVariable("AU_DB_PASSWORD", "Gold")); }
	QSqlDatabase db;
private:
	QString name;
};

QString LibraryPrefix(const AuId& architect)
{
	return QStringLiteral("studio/%1/").arg(architect.GetID());
}

bool ReadId(const QVariant& value, quint32& id)
{
	bool valid = false; const qulonglong parsed = value.toULongLong(&valid);
	if(!valid || !parsed || parsed > std::numeric_limits<quint32>::max()) return false;
	id = quint32(parsed); return true;
}

bool ReadOptionalId(const QVariant& value, quint32& id)
{
	if(value.isNull()) { id = 0; return true; }
	return ReadId(value, id);
}
}

bool SaveLegacyBuildingStudioDesign(const AuLegacyShip::Design& design, quint32& libraryId, QString& error)
{
	libraryId = 0;
	if(!design.building || !design.header.architectId || !design.buildingIndustry
		|| AuLegacyBd::DesignerIndustry(design.buildingIndustry).name.isEmpty()
		|| !AuLegacyStudioProtocol::IsLibraryNameValid(design.header.name))
	{ error = QStringLiteral("Select a building industry and enter a valid design name."); return false; }
	LibraryConnection connection;
	if(!connection.Open()) { error = QStringLiteral("The legacy design database is unavailable."); return false; }
	QSqlQuery allocate(connection.db);
	quint32 id = 0;
	if(!allocate.exec("SELECT nextval('n_legacybddesign_studio_id_seq')") || !allocate.next()
		|| !ReadId(allocate.value(0), id))
	{ error = QStringLiteral("The building design library could not allocate an identifier."); return false; }
	allocate.finish();
	AuLegacyShip::Design saved = design;
	saved.header.designId = id;
	for(auto& room : saved.rooms) room.designId = id;
	for(auto& detail : saved.details) detail.designId = id;
	QByteArray source;
	if(!AuLegacyShip::Write(saved, source, error)) return false;
	if(source.size() > AuLegacyBd::MaximumCacheBytes)
	{ error = QStringLiteral("The building design exceeds the online library limit."); return false; }
	AuLegacyShip::Design checked;
	if(!AuLegacyBd::ReadDesignFile(source, checked, error)) return false;
	QSqlQuery insert(connection.db);
	insert.prepare("INSERT INTO n_LegacyBdDesign(id,sourceId,contentHash,sourceData,name,industry,architectId) "
		"VALUES(?,?,?,?,?,?,?)");
	insert.addBindValue(qulonglong(id)); insert.addBindValue(qulonglong(id));
	insert.addBindValue(QString::fromLatin1(QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex()));
	insert.addBindValue(source); insert.addBindValue(saved.header.name); insert.addBindValue(saved.buildingIndustry);
	insert.addBindValue(qulonglong(saved.header.architectId));
	if(!insert.exec())
	{ qWarning() << "Legacy building library:" << insert.lastError().text(); error = QStringLiteral("The building design could not be saved."); return false; }
	libraryId = id; error.clear(); return true;
}

bool ListLegacyStudioLibrary(const AuId& architect, bool building,
	QVector<AuLegacyStudioProtocol::LibraryEntry>& entries, bool& truncated, QString& error)
{
	error.clear();
	if(architect.IsNull()) { error = QStringLiteral("An architect is required to open the design library."); return false; }
	QVector<AuLegacyStudioProtocol::LibraryEntry> found;
	QVector<quint32> runtimeIds;
	bool hasMore = false;
	{
		LibraryConnection connection;
		if(!connection.Open()) { error = QStringLiteral("The legacy design database is unavailable."); return false; }
		QSqlQuery query(connection.db);
		// Every save creates an immutable catalog row. A newer row with the same
		// library name replaces only the visible reference, retaining burned IDs.
		if(!query.prepare(building
				? "SELECT DISTINCT ON (name) id, left(name,128), 0, NULL FROM n_LegacyBdDesign "
				  "WHERE architectId = ? AND NOT publicDefault ORDER BY name, id DESC LIMIT 257"
				: "SELECT DISTINCT ON (source.name) source.id, left(source.name,128), "
				"CASE WHEN runtime.state = ? THEN 1 ELSE 0 END, runtime.id "
				"FROM n_LegacyScDesign source LEFT JOIN n_ScDesign runtime ON runtime.id = source.runtimeDesignId "
				"WHERE source.sourceName LIKE ? ORDER BY source.name, source.id DESC LIMIT 257"))
		{ qWarning() << "Legacy studio library:" << query.lastError().text(); error = QStringLiteral("The legacy design library cannot be queried."); return false; }
		if(building) query.addBindValue(qulonglong(architect.GetID()));
		else
		{
			query.addBindValue(int(AuDesign::Final));
			query.addBindValue(LibraryPrefix(architect) + QLatin1Char('%'));
		}
		if(!query.exec())
		{ qWarning() << "Legacy studio library:" << query.lastError().text(); error = QStringLiteral("Unable to read the legacy design library. Check the server logs."); return false; }
		while(query.next())
		{
			if(found.size() == AuLegacyStudioProtocol::MaximumLibraryEntries) { hasMore = true; break; }
			AuLegacyStudioProtocol::LibraryEntry entry; quint32 runtimeId = 0;
			if(!ReadId(query.value(0), entry.id) || !ReadOptionalId(query.value(3), runtimeId))
			{ error = QStringLiteral("The design library contains an invalid identifier."); return false; }
			entry.name = query.value(1).toString();
			entry.state = query.value(2).toUInt() ? 1 : 0;
			found.append(entry); runtimeIds.append(runtimeId);
		}
		if(query.lastError().isValid())
		{ qWarning() << "Legacy studio library:" << query.lastError().text(); error = QStringLiteral("The legacy design library could not finish reading."); return false; }
	}
	// A successful burn publishes Final in memory before the ordinary database
	// queue persists it. Do not hold a SQL connection while taking this lock.
	if(!building)
	{
		const DgData& data = g_Au.LockScDesignsForRead();
		for(int index = 0; index < found.size(); ++index)
			if(runtimeIds[index] && data.ScDesignState(runtimeIds[index]) == AuDesign::Final)
				found[index].state = 1;
		g_Au.UnlockScDesigns();
	}
	entries = std::move(found); truncated = hasMore; return true;
}

bool LoadLegacyStudioLibrary(quint32 designId, const AuId& architect, bool building,
	AuLegacyShip::Design& design, QString& error)
{
	error.clear();
	if(!designId || architect.IsNull()) { error = QStringLiteral("Select one of your saved designs."); return false; }
	QByteArray source; bool readOnly = false; quint32 runtimeId = 0;
	{
		LibraryConnection connection;
		if(!connection.Open()) { error = QStringLiteral("The legacy design database is unavailable."); return false; }
		QSqlQuery query(connection.db);
		// CASE prevents the SQL driver from allocating a large bytea value before
		// its length has been checked. Use the same 8 MiB online intake bound.
		if(!query.prepare(building
				? "SELECT octet_length(sourceData), CASE WHEN octet_length(sourceData) <= ? THEN sourceData END, "
				  "0, NULL FROM n_LegacyBdDesign WHERE id = ? AND architectId = ? AND NOT publicDefault"
				: "SELECT octet_length(source.sourceData), "
				"CASE WHEN octet_length(source.sourceData) <= ? THEN source.sourceData END, "
				"CASE WHEN runtime.state = ? THEN 1 ELSE 0 END, runtime.id "
				"FROM n_LegacyScDesign source LEFT JOIN n_ScDesign runtime ON runtime.id = source.runtimeDesignId "
				"WHERE source.id = ? AND source.sourceName LIKE ?"))
		{ qWarning() << "Legacy studio library:" << query.lastError().text(); error = QStringLiteral("The legacy design library cannot load this design."); return false; }
		const int maximum = building ? AuLegacyBd::MaximumCacheBytes : AuLegacyScProtocol::MaximumUploadBytes;
		query.addBindValue(maximum);
		if(!building) query.addBindValue(int(AuDesign::Final));
		query.addBindValue(qulonglong(designId));
		if(building) query.addBindValue(qulonglong(architect.GetID()));
		else query.addBindValue(LibraryPrefix(architect) + QLatin1Char('%'));
		if(!query.exec())
		{ qWarning() << "Legacy studio library:" << query.lastError().text(); error = QStringLiteral("Unable to load the legacy design. Check the server logs."); return false; }
		if(!query.next()) { error = QStringLiteral("That design is not in your design library."); return false; }
		bool validSize = false; const qlonglong size = query.value(0).toLongLong(&validSize);
		if(!validSize || size <= 0 || size > maximum)
		{ error = QStringLiteral("The stored design exceeds the online library limits."); return false; }
		if(!ReadOptionalId(query.value(3), runtimeId))
		{ error = QStringLiteral("The design library contains an invalid identifier."); return false; }
		source = query.value(1).toByteArray(); readOnly = query.value(2).toUInt() != 0;
		if(source.size() != size) { error = QStringLiteral("The stored design is incomplete."); return false; }
	}
	if(building)
	{
		AuLegacyShip::Design incoming;
		if(!AuLegacyBd::ReadDesignFile(source, incoming, error)) return false;
		if(incoming.header.architectId != architect.GetID() || incoming.header.designId != designId)
		{ error = QStringLiteral("The stored building design does not match its library record."); return false; }
		design = std::move(incoming); return true;
	}
	if(runtimeId)
	{
		const DgData& data = g_Au.LockScDesignsForRead();
		readOnly = readOnly || data.ScDesignState(runtimeId) == AuDesign::Final;
		g_Au.UnlockScDesigns();
	}
	// Analysis can take time and can consult game data internally. No SQL,
	// avatar, etherium, or game-data lock is retained while parsing it.
	AuLegacyScDesign accepted;
	if(!AuLegacyScDesign::ReadUpload(source, accepted, error)) return false;
	if(accepted.SourceDesign().header.architectId != architect.GetID())
	{ error = QStringLiteral("That spacecraft design belongs to another architect."); return false; }
	// A design saved on another scene worker may not be in this worker's
	// startup catalog. Publish the validated source for subsequent media use.
	QSharedPointer<AuLegacyScDesign> cached(new AuLegacyScDesign(accepted));
	cached->SetId(designId);
	DgData& records = g_Au.LockScDesignsForWrite();
	records.LegacyScDesignAdd(cached);
	g_Au.UnlockScDesigns();
	AuLegacyShip::Design incoming = accepted.SourceDesign();
	incoming.header.state = readOnly ? 1 : 0;
	incoming.header.versionStateRaw = quint8((incoming.header.version << 4) | incoming.header.state);
	design = std::move(incoming); error.clear(); return true;
}
