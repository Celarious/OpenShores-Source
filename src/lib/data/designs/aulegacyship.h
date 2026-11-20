#ifndef AuLegacyShip_H
#define AuLegacyShip_H

#include <QByteArray>
#include <QPointF>
#include <QPolygonF>
#include <QString>
#include <QVector>

// Recovered 2016 .ship authoring records, independent of the modern mesh model.
// Evidence: research/legacy-spacecraft-2016/FORMAT.txt and pseudocode/.
// Coordinates are in feet; one packed coordinate step is half a legacy grid cell.
namespace AuLegacyShip
{
constexpr double CoordinateSpacing = 2.8284271245;
constexpr double GridSpacing = 5.656854249;
constexpr double DeckHeight = 8.0;
// AuAtom13.dll DeckMinLimit/DeckMaxLimit in spacecraft mode (57 decks).
constexpr int MinimumDeck = -28;
constexpr int MaximumDeck = 28;
constexpr int MaximumFileBytes = 64 * 1024 * 1024;
constexpr int MaximumRecords = 250000;

struct ModulePair
{
	quint16 first = 0;
	quint16 secondRaw = 0;
	quint16 secondNormalized = 0;
};

struct Header
{
	quint32 designId = 0;
	QString name;
	quint32 architectId = 0;
	quint8 streamline = 0;
	quint8 armor = 0;
	quint8 versionStateRaw = 0;
	quint8 version = 0;
	quint8 state = 0;
	QString architectName;
	QString legacyDiscardedString;
	quint8 minimumTechLevel = 0;
	quint32 hullVolume = 0;
	quint8 structure = 0;
	quint8 techLevel = 0;
	QVector<ModulePair> firstPairs;
	QVector<quint16> scalarSlots;
	QVector<ModulePair> secondPairs;
};

struct Room
{
	int streamOffset = 0;
	quint32 designId = 0;
	quint16 roomId = 0;
	qint8 deck = 0;
	quint8 roomType = 0;
	quint8 roomSubtype = 0;
	QVector<quint16> packedVertices;
	// Keep edited geometry in doubles, as the 2016 working design did. Packing
	// coordinates is a file-format operation and must not round each paint stroke.
	bool hasWorkingBoundary = false;
	QPolygonF workingBoundary;
	bool IsEllipse() const { return !hasWorkingBoundary && roomType == 0x18 && packedVertices.size() == 3; }
};

struct Detail
{
	int streamOffset = 0;
	qint8 deck = 0;
	quint8 detailType = 0;
	quint8 hiddenRaw = 0;
	quint32 detailId = 0;
	quint32 designId = 0;
	quint16 packedXY = 0;
	quint16 roomId = 0;
	quint16 commodityIdRaw = 0;
	quint16 commodityIdNormalized = 0;
};

struct Design
{
	Header header;
	bool building = false;
	quint8 buildingIndustry = 0;
	quint16 buildingModule = 0;
	quint16 buildingMaterial = 86; // DpBuilding starts with logs. DnBuilding::Tx omits this preview.
	QVector<quint32> publishedEmpires;
	QVector<Room> rooms;
	QVector<Detail> details;
	int bytesConsumed = 0;
};

// Atomic load: on failure, output is unchanged and error includes a byte offset.
bool Read(const QByteArray& bytes, Design& output, QString& error);
bool Load(const QString& fileName, Design& output, QString& error);
QPointF DecodePoint(quint16 packedXY);

// Uses a working boundary after edits; otherwise preserves stored controls and
// expands ellipse rooms for display only. Degenerate
// ellipse controls return an empty boundary and an explanatory error.
QPolygonF RoomBoundary(const Room& room, QString* error = nullptr);
QPolygonF EllipseBoundary(const QPointF& center, const QPointF& axisPoint, const QPointF& sidePoint);
}

#endif // AuLegacyShip_H
