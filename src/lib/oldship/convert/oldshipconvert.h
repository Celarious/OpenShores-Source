#ifndef OldShipConvert_H
#define OldShipConvert_H

#include "oldship.h"
#include "aupoint.h"
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

class AuScDesign;
class AuScReport;

class AuScParts;

namespace OldShip
{

struct Problem
{
	enum KIND
	{
		NoWayIn,
		RoomUnreached,
		RoomShut,
		InUnreachedRoom,
		LeftOut,
		StairsShut
	};
	KIND				kind;
	QString				text;
	QString				line;
	int					deck;
	int					detailId;
	int					detailType;
	QString				room;
	AuPoint				at;
	QVector<AuPoint>	outline;

	Problem() : kind(LeftOut), deck(0), detailId(0), detailType(0) {}
};

typedef QHash<QString, quint32> SettingValues;

struct VolumeSuggestion
{
	bool			made;
	quint32			hullVolume;
	quint32			kept;
	SettingValues	volumes;
	SettingValues	least;
	quint32			framework;
	bool			scaled;
	int				ftlDetail;
	int				ftlModule;
	QStringList		lines;

	VolumeSuggestion() : made(false), hullVolume(0), kept(0), framework(0), scaled(false), ftlDetail(0), ftlModule(0) {}
};

struct ConvertInfo
{
	struct RoomInfo
	{
		int				room;
		QString			name;
		int				deck;
		int				type;
		bool			walkable;
		bool			openBelow;
		AuPoint			centre;
		QList<quint16>	voids;
		QVector<AuPoint> floorProbes;
	};

	struct DoorInfo
	{
		enum KIND { Door, HullDoor, Hatch, HullHatchTop, HullHatchBottom, Opening, HullOpening };
		KIND			kind;
		int				detailType;
		int				detailId;
		quint16			part;
		QString			roomA, roomB;
		AuPoint			mid;
		AuPoint			sideA, sideB;
		quint16			ladder;
		AuPoint			ladderLow, ladderHigh;
	};

	struct StationInfo
	{
		int				detailType;
		int				detailId;
		quint16			part;
		QString			room;
		QString			what;
		AuPoint			at;
	};

	struct EmitterInfo
	{
		int				detailId;
		int				bayNumber;
		quint16			part;
		AuPoint			at;
	};

	struct RailingInfo
	{
		QString			roomA, roomB;
		AuPoint			sideA, sideB;
	};

	struct StairInfo
	{
		int				detailType;
		int				detailId;
		bool			open;
		QString			roomLow, roomHigh;
		QVector<AuPoint> walk;
		QVector<AuPoint> drawn;
		QVector<AuPoint> plan;

		StairInfo() : detailType(0), detailId(0), open(false) {}
	};

	AuPoint				offset;
	QList<RoomInfo>		rooms;
	QList<DoorInfo>		doors;
	QList<StationInfo>	stations;
	QList<StationInfo>	berths;
	QList<StationInfo>	tables;
	QList<StationInfo>	parking;
	QList<StationInfo>	contrails;
	QList<EmitterInfo>	emitters;
	QList<RailingInfo>	railings;
	bool				pivot;
	QList<StairInfo>	stairs;
	QStringList			stairsShut;
	QVector<AuPoint>	wallProbes;
	QStringList			skipped;
	QStringList			notes;
	int					hullTriangles, roomTriangles, fittingTriangles, doorTriangles;
	QList<Problem>		problems;
	VolumeSuggestion	suggestion;

	ConvertInfo() : pivot(false), hullTriangles(0), roomTriangles(0), fittingTriangles(0), doorTriangles(0) {}
};

struct ConvertOptions
{
	bool	keepOrigin;

	ConvertOptions() : keepOrigin(false) {}
};

bool Convert(const Design& ship, AuScDesign& out, QStringList& log, ConvertInfo* info = 0, const ConvertOptions* options = 0);

QList<Problem> Problems(const Design& ship, const ConvertInfo& info);

struct PathReportCounts
{
	int		pathWarnings;
	int		securityWarnings;
	bool	complete;

	PathReportCounts() : pathWarnings(0), securityWarnings(0), complete(false) {}
};

QString PathReportHtml(const Design& design, const std::function<bool(const QString&, int, int)>& progress =
		std::function<bool(const QString&, int, int)>(), PathReportCounts* counts = 0);

bool AnalyzeConverted(AuScDesign& design, AuScReport& report, QStringList& msgs);

QString ReportHtml(const Design& ship, const ConvertInfo& info, const QStringList& log, bool converted, const AuScReport* game,
		const QStringList& gameMsgs, bool stale = false, const QList<Problem>* problems = 0);

}

#endif
