#include "aulegacyspacecraftcatalog.h"

#include "auglobal.h"
#include "audbglobal.h"
#include "aulegacyscdesign.h"
#include "aulegacyscstats.h"
#include "aulegacyscgeometry.h"
#include "aulegacysccollision.h"
#include "aulegacyscprotocol.h"
#include "aulegacystudioprotocol.h"
#include "auscdesign.h"
#include "auscparts.h"
#include "auscreport.h"
#include "auscdesignexchange.h"
#include "aulegacyshipsave.h"
#include "dgdata.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QFileInfo>
#include <QHash>
#include <QSharedPointer>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QUuid>
#include <limits>

namespace
{
bool ReadCatalogId(const QVariant& value, quint32& id)
{
	bool ok = false;
	const qlonglong storedId = value.toLongLong(&ok);
	if(!ok || storedId <= 0 || quint64(storedId) > std::numeric_limits<quint32>::max())
		return false;
	id = quint32(storedId);
	return true;
}
}

bool StoreLegacySpacecraftDesign(QSqlDatabase& db, const QString& sourceName, const AuLegacyScDesign& design, quint32& id)
{
	id = 0;
	QSqlQuery query(db);
	if(!query.prepare("INSERT INTO n_LegacyScDesign (contentHash, sourceName, name, sourceData) "
			"VALUES (?, ?, ?, ?) ON CONFLICT (contentHash) DO UPDATE SET name = EXCLUDED.name RETURNING id"))
	{
		qWarning() << "Legacy spacecraft catalog:" << "cannot prepare design import:" << query.lastError().text();
		return false;
	}
	query.addBindValue(QString::fromLatin1(design.ContentHash()));
	query.addBindValue(sourceName);
	query.addBindValue(design.Name().isNull() ? QStringLiteral("") : design.Name());
	query.addBindValue(design.SourceData());
	if(!query.exec())
	{
		qWarning() << "Legacy spacecraft catalog:" << "cannot store" << sourceName << ':' << query.lastError().text();
		return false;
	}
	if(query.next())
	{
		if(!ReadCatalogId(query.value(0), id))
		{
			qWarning() << "Legacy spacecraft catalog:" << "import returned an invalid server ID:" << query.value(0);
			return false;
		}
		return true;
	}
	if(query.lastError().isValid())
	{
		qWarning() << "Legacy spacecraft catalog:" << "cannot read import result:" << query.lastError().text();
		return false;
	}
	qWarning() << "Legacy spacecraft catalog:" << "storage returned no design ID";
	return false;
}


bool LoadLegacySpacecraftCatalog(QSqlDatabase& db)
{
	QSqlQuery schema(db);
	if(!schema.exec("SELECT id, contentHash, sourceData, sourceName, name FROM n_LegacyScDesign WHERE false"))
	{
		qWarning() << "Legacy spacecraft catalog:" << "storage unavailable; apply"
			<< "deploy/server/postgres/schema/legacyspacecraft.sql to auUniverse as aumaster, then restart scene servers."
			<< schema.lastError().text();
		return false;
	}
	schema.finish();

	QSqlQuery query(db);
	query.setForwardOnly(true);
	if(!query.exec("SELECT id, contentHash, sourceData, sourceName, name FROM n_LegacyScDesign ORDER BY id"))
	{
		qWarning() << "Legacy spacecraft catalog:" << "cannot load stored designs:" << query.lastError().text();
		return false;
	}
	QHash<quint32, QSharedPointer<const AuLegacyScDesign>> catalog;
	int rejected = 0;
	while(query.next())
	{
		quint32 id = 0;
		const QString sourceName = query.value(3).toString();
		if(!ReadCatalogId(query.value(0), id))
		{
			qWarning() << "Legacy spacecraft catalog:" << "skipping invalid server ID" << query.value(0) << "from" << sourceName;
			++rejected;
			continue;
		}
		const QByteArray source = query.value(2).toByteArray();
		if(source.isEmpty() || source.size() > AuLegacyShip::MaximumFileBytes ||
			query.value(1).toString().toLatin1() != QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex())
		{
			qWarning() << "Legacy spacecraft catalog:" << "skipping design" << id << "with invalid bytes or hash from" << sourceName;
			++rejected;
			continue;
		}
		QSharedPointer<AuLegacyScDesign> design(new AuLegacyScDesign);
		QString error;
		if(!AuLegacyScDesign::Read(source, *design, error))
		{
			qWarning() << "Legacy spacecraft catalog:" << "skipping stored design" << id << "from" << sourceName << ':' << error;
			++rejected;
			continue;
		}
		design->SetId(id);
		design->SetName(query.value(4).toString());
		catalog.insert(id, design);
		qInfo() << "Legacy spacecraft catalog:" << "loaded design" << id << design->Name()
			<< "with" << design->SourceDesign().rooms.size() << "rooms and"
			<< design->SourceDesign().details.size() << "details; author center (feet):"
			<< design->AuthorCenter().x << design->AuthorCenter().y << design->AuthorCenter().z;
		for(const QString& diagnostic : design->Stats().diagnostics)
			qWarning() << "Legacy spacecraft catalog:" << "design" << id << "diagnostic:" << diagnostic;
	}
	if(query.lastError().isValid())
	{
		qWarning() << "Legacy spacecraft catalog:" << "cannot finish reading stored designs:" << query.lastError().text();
		return false;
	}
	DgData& data = g_Au.LockScDesignsForWrite();
	data.LegacyScDesignsReplace(catalog);
	g_Au.UnlockScDesigns();
	qInfo() << "Legacy spacecraft catalog:" << "published" << catalog.size() << "designs; skipped" << rejected << "stored records";
	return true;
}

namespace
{
class CatalogConnection
{
public:
	CatalogConnection() : name(QStringLiteral("LegacySc-%1").arg(QUuid::createUuid().toString()))
	{
		db = QSqlDatabase::addDatabase("QPSQL7", name);
		db.setDatabaseName("auUniverse");
		db.setHostName(qEnvironmentVariable("AU_UNIVERSE_DB_HOST", "UniverseDb.Hazeron.net"));
		db.setConnectOptions("connect_timeout=5");
	}
	~CatalogConnection()
	{
		db.close();
		db = QSqlDatabase();
		QSqlDatabase::removeDatabase(name);
	}
	bool Open() { return db.open("audog", qEnvironmentVariable("AU_DB_PASSWORD", "Gold")); }
	QSqlDatabase db;
private:
	QString name;
};

class CatalogTransaction
{
public:
	explicit CatalogTransaction(QSqlDatabase& database) : db(database) {}
	~CatalogTransaction() { if(active) db.rollback(); }
	bool Begin()
	{
		active = db.transaction();
		if(!active)
			return false;
		QSqlQuery limits(db);
		return limits.exec("SET LOCAL lock_timeout = '5s'")
			&& limits.exec("SET LOCAL statement_timeout = '30s'");
	}
	bool Commit()
	{
		if(!db.commit())
			return false;
		active = false;
		return true;
	}
private:
	QSqlDatabase& db;
	bool active = false;
};

bool HasControlCharacters(const QString& text)
{
	for(QChar character : text)
		if(character.isNull() || character.category() == QChar::Other_Control)
			return true;
	return false;
}

QString CatalogText(const QString& text)
{
	QString result = text.left(AuLegacyScProtocol::MaximumNameCharacters);
	for(int index = 0; index < result.size(); ++index)
	{
		const QChar character = result.at(index);
		if(character.isNull() || character.category() == QChar::Other_Control)
			result[index] = QChar(' ');
	}
	return result;
}
}

bool PrepareLegacySpacecraftSpawn(quint32 legacyId, const AuId& architectId,
	const QString& architectName, AuScReport& report, QString& error)
{
	error.clear();
	if(!legacyId || architectId.IsNull())
	{
		error = QStringLiteral("Select a stored legacy design while controlling an architect avatar.");
		return false;
	}
	CatalogConnection connection;
	if(!connection.Open())
	{
		error = QStringLiteral("The legacy design database is unavailable.");
		return false;
	}
	CatalogTransaction transaction(connection.db);
	if(!transaction.Begin())
	{
		error = QStringLiteral("Unable to start the legacy spawn transaction.");
		qWarning() << "Legacy spacecraft catalog:" << error << connection.db.lastError().text();
		return false;
	}
	QSqlQuery sourceQuery(connection.db);
	if(!sourceQuery.prepare("SELECT contentHash, name, runtimeDesignId, octet_length(sourceData), sourceName, "
			"CASE WHEN octet_length(sourceData) <= ? THEN sourceData END "
			"FROM n_LegacyScDesign WHERE id = ? FOR UPDATE"))
	{
		error = QStringLiteral("Legacy spawn storage is unavailable. Apply legacyspacecraft.sql and check the server logs.");
		qWarning() << "Legacy spacecraft catalog:" << error << sourceQuery.lastError().text();
		return false;
	}
	sourceQuery.addBindValue(AuLegacyScProtocol::MaximumUploadBytes);
	sourceQuery.addBindValue(qulonglong(legacyId));
	if(!sourceQuery.exec())
	{
		error = QStringLiteral("Unable to read the stored design for spawning. Check the database schema and server logs.");
		qWarning() << "Legacy spacecraft catalog:" << error << sourceQuery.lastError().text();
		return false;
	}
	if(!sourceQuery.next())
	{
		error = QStringLiteral("That legacy design no longer exists in the catalog.");
		return false;
	}
	QString name = CatalogText(sourceQuery.value(1).toString());
	if(name.isEmpty())
		name = CatalogText(QFileInfo(sourceQuery.value(4).toString()).completeBaseName());
	if(name.isEmpty())
		name = QStringLiteral("Legacy design %1").arg(legacyId);
	const QByteArray source = sourceQuery.value(5).toByteArray();
	if(sourceQuery.value(3).toLongLong() <= 0
		|| sourceQuery.value(3).toLongLong() > AuLegacyScProtocol::MaximumUploadBytes
		|| source.isEmpty()
		|| sourceQuery.value(0).toString().toLatin1() != QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex())
	{
		error = QStringLiteral("The stored legacy design has invalid bytes, an invalid hash, or exceeds the online size limit.");
		return false;
	}
	quint32 runtimeId = 0;
	const bool derived = !sourceQuery.value(2).isNull();
	if(derived && !ReadCatalogId(sourceQuery.value(2), runtimeId))
	{
		error = QStringLiteral("The stored legacy design has an invalid runtime blueprint ID.");
		return false;
	}
	sourceQuery.finish();
	AuScDesign runtimeDesign;
	AuScReport preparedReport;
	bool regenerate = !derived;
	const quint32 previousRuntimeId = runtimeId;
	if(derived)
	{
		runtimeDesign.Load(runtimeId, connection.db);
		if(runtimeDesign.Id() != runtimeId || runtimeDesign.IsDeleted())
		{
			error = QStringLiteral("The stored runtime blueprint is unavailable or deleted. Check the server logs.");
			return false;
		}
		regenerate = true;
		for(const AuScPart* part : runtimeDesign.Parts())
			if(part->IsHullMesh() && part->GroupName() == QStringLiteral("legacy/debug/static/v12"))
			{
				regenerate = false;
				break;
			}
		if(!regenerate)
		{
			// Catalog naming is presentation metadata; the original file stays intact.
			runtimeDesign.SetName(name);
			runtimeDesign.ReportDesign(preparedReport);
		}
	}
	if(regenerate)
	{
		AuLegacyScDesign legacyDesign;
		if(!AuLegacyScDesign::ReadUpload(source, legacyDesign, error))
			return false;
		legacyDesign.SetId(legacyId);
		legacyDesign.SetName(name);
		if(!g_AuDb.HasSpaceShipDesignIds())
		{
			error = QStringLiteral("The server is restocking spacecraft blueprint IDs. Try spawning again shortly.");
			return false;
		}
		runtimeId = g_Au.NextSpaceShipDesignId();
		if(!runtimeId)
		{
			error = QStringLiteral("The server cannot allocate a runtime spacecraft blueprint ID yet.");
			return false;
		}
		if(!AuLegacyScGeometry::Build(legacyDesign, runtimeId, architectId, architectName,
				runtimeDesign, preparedReport, error))
			return false;
		runtimeDesign.Insert(connection.db);
		QSqlQuery verify(connection.db);
		verify.prepare("SELECT id FROM n_ScDesign WHERE id = ?");
		verify.addBindValue(qulonglong(runtimeId));
		if(!verify.exec() || !verify.next())
		{
			error = QStringLiteral("Unable to store the runtime spacecraft blueprint. Check the server logs.");
			qWarning() << "Legacy spacecraft catalog:" << error << verify.lastError().text();
			return false;
		}
		verify.finish();
		QSqlQuery link(connection.db);
		link.prepare(derived
			? "UPDATE n_LegacyScDesign SET runtimeDesignId = ? WHERE id = ? AND runtimeDesignId = ?"
			: "UPDATE n_LegacyScDesign SET runtimeDesignId = ? WHERE id = ? AND runtimeDesignId IS NULL");
		link.addBindValue(qulonglong(runtimeId));
		link.addBindValue(qulonglong(legacyId));
		if(derived) link.addBindValue(qulonglong(previousRuntimeId));
		if(!link.exec() || link.numRowsAffected() != 1)
		{
			error = QStringLiteral("Unable to link the runtime blueprint to its legacy design. Apply legacyspacecraft.sql.");
			qWarning() << "Legacy spacecraft catalog:" << error << link.lastError().text();
			return false;
		}
	}
	preparedReport.SetLegacyDebug(true);
	if(!preparedReport.LegacyStats() || !preparedReport.LegacyStats()->IsValid())
	{
		error = QStringLiteral("The legacy design has incomplete recovered statistics and cannot be spawned accurately.");
		return false;
	}
	if(!transaction.Commit())
	{
		error = QStringLiteral("Unable to commit the runtime spacecraft blueprint. Check the server logs.");
		qWarning() << "Legacy spacecraft catalog:" << error << connection.db.lastError().text();
		return false;
	}
	DgData& data = g_Au.LockScDesignsForWrite();
	const auto cached = data.m_ScDesigns->find(runtimeDesign.Id());
	if(runtimeDesign.State() == AuDesign::FinalAssembly && cached != data.m_ScDesigns->end()
		&& cached->second.State() == AuDesign::Final && cached->second.LegacyStats())
		runtimeDesign.SetState(AuDesign::Final);
	data.ScDesignAdd(runtimeDesign);
	g_Au.UnlockScDesigns();
	report = preparedReport;
	qInfo() << "Legacy spacecraft catalog:" << "prepared legacy design" << legacyId
		<< "as runtime blueprint" << runtimeId << (!regenerate ? "(existing)" : derived ? "(upgraded)" : "(created)");
	return true;
}

bool SaveLegacyStudioDesign(const AuLegacyShip::Design& design, quint32& libraryId, QString& error)
{
	libraryId = 0;
	AuLegacyShip::Design prepared;
	if(!AuLegacyShip::PrepareForSave(design, prepared, error)) return false;
	prepared.header.state = 0; prepared.header.versionStateRaw = 0x20;
	if(!AuLegacyStudioProtocol::IsLibraryNameValid(prepared.header.name))
	{ error = QStringLiteral("Enter a design name of 1 to 30 printable characters without / : ! | < > & or backslash."); return false; }
	CatalogConnection connection;
	if(!connection.Open()) { error = QStringLiteral("The legacy design database is unavailable."); return false; }
	QSqlQuery allocate(connection.db);
	if(!allocate.exec("SELECT nextval('n_legacyscdesign_id_seq')") || !allocate.next()
		|| !ReadCatalogId(allocate.value(0), libraryId))
	{ libraryId = 0; error = QStringLiteral("The design library could not allocate an identifier."); return false; }
	allocate.finish();
	// Put the allocated identity into the saved records. Later saves receive
	// a new immutable source record; previously burned media retain their ID.
	prepared.header.designId = libraryId;
	for(auto& room : prepared.rooms) room.designId = libraryId;
	for(auto& detail : prepared.details) detail.designId = libraryId;
	QByteArray source;
	if(!AuLegacyShip::Write(prepared, source, error)) return false;
	QSharedPointer<AuLegacyScDesign> accepted(new AuLegacyScDesign);
	if(!AuLegacyScDesign::ReadUpload(source, *accepted, error)) return false;
	QSqlQuery write(connection.db);
	write.prepare("INSERT INTO n_LegacyScDesign(id,contentHash,sourceName,name,sourceData) VALUES(?,?,?,?,?)");
	write.addBindValue(qulonglong(libraryId)); write.addBindValue(QString::fromLatin1(accepted->ContentHash()));
	write.addBindValue(QStringLiteral("studio/%1/%2.ship").arg(prepared.header.architectId).arg(prepared.header.name));
	write.addBindValue(prepared.header.name); write.addBindValue(source);
	if(!write.exec())
	{ libraryId = 0; error = QStringLiteral("Unable to save the legacy design library record. Check the server logs."); return false; }
	accepted->SetId(libraryId);
	DgData& data = g_Au.LockScDesignsForWrite(); data.LegacyScDesignAdd(accepted); g_Au.UnlockScDesigns();
	error.clear(); return true;
}

bool PrepareLegacyStudioMedia(quint32 libraryId, AuScDesignExchangeEntry& exchange, QString& error)
{
	const DgData& records = g_Au.LockScDesignsForRead();
	const auto source = records.LegacyScDesign(libraryId);
	g_Au.UnlockScDesigns();
	if(!source) { error = QStringLiteral("The saved legacy design is unavailable in the library."); return false; }
	const auto& design = source->SourceDesign();
	AuScReport report;
	if(!PrepareLegacySpacecraftSpawn(libraryId, AuId(design.header.architectId),
		design.header.architectName, report, error)) return false;
	// Preparation leaves new runtime records as assemblies. Publication is
	// committed only after the authenticated actor actually uses blank media.
	exchange = AuScDesignExchangeEntry(report, AuDesign::Final, 0, true, true, false, 0, POL_Neutral);
	error.clear(); return true;
}

bool AuDbGlobal::ListLegacySpacecraftPublications(quint32 empire, QList<quint32>& runtimeIds, QString& error)
{
	error.clear();
	if(!IsLegacyGeneration()) { error = QStringLiteral("Legacy publication requires a Legacy universe."); return false; }
	struct Publication { quint32 runtime, source; bool published; };
	QVector<Publication> records;
	{
		CatalogConnection connection;
		if(!connection.Open()) { error = QStringLiteral("The legacy publication database is unavailable."); return false; }
		QSqlQuery query(connection.db);
		if(!query.prepare("SELECT DISTINCT runtime.id, source.id, publication.runtimeDesignId IS NOT NULL "
			"FROM n_LegacyScDesign source JOIN n_ScDesign runtime ON runtime.id = source.runtimeDesignId "
			"LEFT JOIN n_LegacyScPublication publication ON publication.runtimeDesignId = runtime.id "
			"AND (publication.empireId = ? OR publication.empireId = 0) "
			"WHERE runtime.state = ? ORDER BY runtime.id"))
		{ error = QStringLiteral("The legacy publication schema is unavailable. Apply legacyspacecraft.sql."); return false; }
		query.addBindValue(qulonglong(empire)); query.addBindValue(int(AuDesign::Final));
		if(!query.exec())
		{ qWarning() << "Legacy publications:" << query.lastError().text(); error = QStringLiteral("The legacy publication library could not be queried."); return false; }
		while(query.next())
		{
			Publication record;
			if(!ReadCatalogId(query.value(0), record.runtime) || !ReadCatalogId(query.value(1), record.source))
			{ error = QStringLiteral("The legacy publication library contains an invalid design identifier."); return false; }
			record.published = query.value(2).toBool(); records.append(record);
		}
		if(query.lastError().isValid())
		{ error = QStringLiteral("The legacy publication library could not finish reading."); return false; }
	}
	// Release SQL before reading the immutable source catalogue. Original
	// DnSpaceShip::IsPublic uses source state 2, independently of empire copies.
	QList<quint32> selected;
	const DgData& data = LockScDesignsForRead();
	for(const auto& record : records)
	{
		const auto source = data.LegacyScDesign(record.source);
		if(record.published || (source && source->SourceDesign().header.state == 2)) selected.append(record.runtime);
	}
	UnlockScDesigns();
	runtimeIds = selected;
	return true;
}

bool AuDbGlobal::SetLegacySpacecraftPublication(quint32 empire, quint32 runtimeId, bool published, QString& error)
{
	error.clear();
	if(!IsLegacyGeneration() || !runtimeId)
	{ error = QStringLiteral("A valid Legacy spacecraft blueprint is required."); return false; }
	CatalogConnection connection;
	if(!connection.Open()) { error = QStringLiteral("The legacy publication database is unavailable."); return false; }
	CatalogTransaction transaction(connection.db);
	if(!transaction.Begin()) { error = QStringLiteral("The legacy publication transaction could not begin."); return false; }
	QSqlQuery query(connection.db);
	if(published)
	{
		if(!query.prepare("INSERT INTO n_LegacyScPublication (runtimeDesignId, empireId) "
			"SELECT runtime.id, ? FROM n_ScDesign runtime JOIN n_LegacyScDesign source ON source.runtimeDesignId = runtime.id "
			"WHERE runtime.id = ? AND runtime.state = ? ON CONFLICT DO NOTHING RETURNING runtimeDesignId"))
		{ error = QStringLiteral("The legacy publication schema is unavailable. Apply legacyspacecraft.sql."); return false; }
		query.addBindValue(qulonglong(empire)); query.addBindValue(qulonglong(runtimeId)); query.addBindValue(int(AuDesign::Final));
	}
	else
	{
		if(!query.prepare("DELETE FROM n_LegacyScPublication WHERE empireId = ? AND runtimeDesignId = ? RETURNING runtimeDesignId"))
		{ error = QStringLiteral("The legacy publication schema is unavailable. Apply legacyspacecraft.sql."); return false; }
		query.addBindValue(qulonglong(empire)); query.addBindValue(qulonglong(runtimeId));
	}
	if(!query.exec())
	{ qWarning() << "Legacy publication:" << query.lastError().text(); error = QStringLiteral("The legacy publication could not be stored."); return false; }
	if(!query.next())
	{ error = published ? QStringLiteral("This blueprint is already published here or its final Legacy design is unavailable.")
		: QStringLiteral("This blueprint is not published to this empire."); return false; }
	query.finish();
	if(!transaction.Commit()) { error = QStringLiteral("The legacy publication transaction could not be committed."); return false; }
	return true;
}

void ProcessLegacySpacecraftRequest(const AuLegacyScProtocol::Request& request, AuLegacyScProtocol::Reply& reply)
{
	using namespace AuLegacyScProtocol;
	reply.token = request.token;
	reply.action = request.action == UploadRequest ? UploadReply
		: request.action == SpawnRequest ? SpawnReply : CatalogReply;
	if(request.action != UploadRequest && request.action != CatalogRequest)
	{
		reply.message = QStringLiteral("That operation must use the live spacecraft spawn handler.");
		return;
	}
	if(request.action == UploadRequest)
	{
		const QString name = request.name.trimmed();
		if(name.isEmpty() || name.size() > MaximumNameCharacters || HasControlCharacters(name))
		{
			reply.message = QStringLiteral("Enter a name of 1 to 128 characters, without control characters.");
			return;
		}
		if(request.sourceName.isEmpty() || request.sourceName.contains('/') || request.sourceName.contains('\\')
			|| HasControlCharacters(request.sourceName) || !request.sourceName.endsWith(".ship", Qt::CaseInsensitive))
		{
			reply.message = QStringLiteral("The upload must have a .ship filename without a directory path.");
			return;
		}
		QSharedPointer<AuLegacyScDesign> design(new AuLegacyScDesign);
		if(!AuLegacyScDesign::ReadUpload(request.source, *design, reply.message))
			return;
		design->SetName(name);
		CatalogConnection connection;
		if(!connection.Open() || !StoreLegacySpacecraftDesign(connection.db, request.sourceName, *design, reply.designId))
		{
			reply.designId = 0;
			reply.message = QStringLiteral("Unable to store the design. Check the database schema and server logs.");
			return;
		}
		design->SetId(reply.designId);
		DgData& data = g_Au.LockScDesignsForWrite();
		data.LegacyScDesignAdd(design);
		g_Au.UnlockScDesigns();
		reply.name = name;
		reply.message = QStringLiteral("Stored %1 as design %2.").arg(name).arg(reply.designId);
		qInfo() << "Legacy spacecraft catalog:" << "uploaded design" << reply.designId << name;
		return;
	}

	CatalogConnection connection;
	if(!connection.Open())
	{
		reply.message = QStringLiteral("The legacy design database is unavailable.");
		return;
	}
	QSqlQuery query(connection.db);
	if(!query.prepare("SELECT id, left(name,128), left(sourceName,128) FROM n_LegacyScDesign "
			"WHERE id > ? ORDER BY id LIMIT 65"))
	{
		reply.message = QStringLiteral("Unable to read the legacy design catalog.");
		return;
	}
	query.addBindValue(qulonglong(request.afterId));
	if(!query.exec())
	{
		reply.message = QStringLiteral("Unable to read legacy designs. Check the database schema.");
		return;
	}
	while(query.next())
	{
		if(reply.entries.size() == CatalogPageSize)
		{
			reply.nextAfterId = reply.entries.last().id;
			break;
		}
		Entry entry;
		if(!ReadCatalogId(query.value(0), entry.id))
			continue;
		entry.name = CatalogText(query.value(1).toString());
		entry.sourceName = CatalogText(query.value(2).toString());
		reply.entries.append(entry);
	}
	if(query.lastError().isValid())
	{
		reply.entries.clear();
		reply.nextAfterId = 0;
		reply.message = QStringLiteral("Unable to finish reading the legacy design catalog.");
	}
}
