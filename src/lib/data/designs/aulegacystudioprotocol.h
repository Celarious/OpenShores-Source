#ifndef AuLegacyStudioProtocol_H
#define AuLegacyStudioProtocol_H

#include "audata.h"
#include "aulegacyshipedit.h"
#include <QByteArray>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QVector>

// Working documents retain double-precision edited boundaries. The .ship file
// writer remains a separate export operation and is never used for live edits.
namespace AuLegacyStudioProtocol
{
constexpr int MaximumDocumentBytes = 64 * 1024 * 1024;
constexpr int MaximumPacketBytes = 16 * 1024 * 1024;
// Reserve room for participant names and error text around document payloads.
constexpr int MaximumCompressedDocumentBytes = MaximumPacketBytes - 64 * 1024;
constexpr int MaximumMembers = 64;
constexpr int MaximumPasswordCharacters = 128;
constexpr int MaximumNameCharacters = 128;
constexpr int MaximumLibraryEntries = 256;

enum Action : quint8
{
	Snapshot = 0, Edit = 1, Replace = 2, Saved = 3, Password = 4, BurnMedia = 5,
	Library = 6, LoadDesign = 7
};
enum EditKind : quint8
{
	PaintCell = 0, EraseRoom = 1, PlaceDetail = 2, EraseDetails = 3,
	EraseDeck = 4, MoveCopy = 5, Streamline = 6, MinTech = 7, Armor = 8
};

struct Command
{
	EditKind kind = PaintCell;
	int deck = 0;
	QPointF cursor;
	quint8 gridMask = 5, roomNumber = 0;
	int roomType = 2;
	quint8 detailType = 0;
	quint16 commodity = 0;
	int radius = 0;
	AuLegacyShip::MoveCopyRequest move;
	int value = 0;
};

struct Request
{
	Action action = Snapshot;
	quint32 token = 0, environmentId = 0;
	quint64 operationId = 0;
	quint32 generation = 0;
	quint64 revision = 0;
	Command edit;
	QByteArray document;
	QString password;
	QString name;
	quint32 designId = 0;
};

struct Member { quint32 id = 0; QString name; };
struct LibraryEntry { quint32 id = 0; QString name; quint8 state = 0; };

struct Reply
{
	quint32 token = 0, environmentId = 0, ownerId = 0, generation = 0;
	quint64 revision = 0;
	quint64 lastOperation = 0;
	quint32 libraryDesignId = 0;
	bool accepted = false, modified = false, hasDocument = false, retry = false, canBurn = false;
	QByteArray document;
	QString message;
	QVector<Member> members;
	QVector<LibraryEntry> library;
	bool hasLibrary = false, libraryTruncated = false;
};

struct State
{
	AuLegacyShip::Design design;
	quint32 generation = 1;
	quint64 revision = 1;
	quint32 libraryDesignId = 0;
	bool modified = false;
	QMap<quint32, quint64> appliedOperations;
};

AUDATA_DLL QByteArray EncodeRequest(const Request& request);
AUDATA_DLL bool IsLibraryNameValid(const QString& name);
AUDATA_DLL bool DecodeRequest(const QByteArray& bytes, Request& request, QString& error);
AUDATA_DLL QByteArray EncodeReply(const Reply& reply);
AUDATA_DLL bool DecodeReply(const QByteArray& bytes, Reply& reply);
AUDATA_DLL QByteArray EncodeDocument(const AuLegacyShip::Design& design, QString& error);
AUDATA_DLL bool DecodeDocument(const QByteArray& bytes, AuLegacyShip::Design& design, QString& error);
AUDATA_DLL QByteArray EncodeState(const State& state, QString& error);
AUDATA_DLL bool DecodeState(const QByteArray& bytes, State& state, QString& error);
AUDATA_DLL bool Apply(const Command& command, AuLegacyShip::Design& design, QString& error, bool& changed);
}

#endif
