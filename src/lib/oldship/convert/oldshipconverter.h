#ifndef OldShipConverter_H
#define OldShipConverter_H

#include "oldshipconvert.h"
#include "model.h"
#include "auscdesign.h"
#include "auscparts.h"
#include "auscface.h"
#include <QHash>
#include <QMap>

namespace OldShip
{

const double VoidInset = 0.8202099738;
const double VoidFloor = 0.05;
const double VoidCeiling = 7.0;
const double VoidOverlap = 0.3;
const double DoorwayHeight = 6.8;
const double DoorwayHalfWidth = 1.5;
const double DoorwayDip = 0.02;
const double PathHeight = 3.28;
const double SwitchHeight = 3.6;
HOLD_ACCESS ShipRoomHoldAccess(int oldType);

const double HullMargin = 2.0;
const double KeelDepth = 2.5;
const double RoofRise = 1.0;
const double WallDepth = 1.0;
const double SolidSkin = 0.06;
const double OffGrid = 0.0137;
const double HatchWallClear = 0.3;
const double HatchHalf = 1.3;
const double ClimberBack = 1.3;
const float VoidOpacity = .25f;
const quint8 AtlasSlot = 0;

struct P2
{
	double x, y;
	P2() : x(0), y(0) {}
	P2(double ax, double ay) : x(ax), y(ay) {}
	P2 operator+(const P2& o) const { return P2(x + o.x, y + o.y); }
	P2 operator-(const P2& o) const { return P2(x - o.x, y - o.y); }
	P2 operator*(double s) const { return P2(x * s, y * s); }
	double Dot(const P2& o) const { return x * o.x + y * o.y; }
	double Cross(const P2& o) const { return x * o.y - y * o.x; }
	double Length() const;
	P2 Unit() const;
	P2 Left() const { return P2(-y, x); }
};

typedef QVector<P2> Polygon;

double SignedArea(const Polygon& poly);
bool Contains(const Polygon& poly, const P2& p);
double DistanceTo(const Polygon& poly, const P2& p);
Polygon Tidied(const Polygon& poly);
bool Inset(const Polygon& poly, double by, Polygon& out);
void Triangulate(const Polygon& poly, QVector<int>& corners);
P2 PointInside(const Polygon& poly);

void AddTri(AuScFaceList& faces, const AuPoint& a, const AuPoint& b, const AuPoint& c, const AuPoint& n);
void AddQuad(AuScFaceList& faces, const AuPoint& a, const AuPoint& b, const AuPoint& c, const AuPoint& d, const AuPoint& n);
void AddPrism(AuScFaceList& faces, const QVector<AuPoint>& bottom, const QVector<AuPoint>& top);

struct RoomData
{
	int			index;
	QString		name;
	int			deck;
	int			type;
	Polygon		poly;
	Polygon		inner;
	bool		openBelow;
	P2			centre;
	double		floorZ;
};

struct DoorData
{
	int				detail;
	QVector<Tri>	outside;
	QVector<Tri>	inside;
};

class Converter
{
public:
	Converter(const Design& ship, AuDesign& design, QStringList& log, ConvertInfo& info, const ConvertOptions& options);
	virtual ~Converter() {}
	bool Run();

protected:
	void ReadRooms();
	virtual void RenderModels();
	virtual void SetProperties();
	bool CheckSize();
	virtual void BuildSurroundings() {}
	virtual QString RoomTypeName(int type) const;
	virtual void VoidProperties(AuScRoomVoid& v, const RoomData& rd, bool doorway) const;
	virtual double HullOpeningReach(int roomType) const;
	virtual quint64 MemoryLimit() const { return 28000000; }
	virtual void GiveBayVolume(quint16 part, const QString& where);
	virtual double WallSolidDepth() const { return WallDepth; }
	virtual double HullVoidDrop(int deck) const { Q_UNUSED(deck); return 0.0; }
	virtual bool DoorsAreOpenings() const { return false; }
	virtual double DoorwayReach(double reach) const { return reach / 2 + 0.2; }
	virtual double HullVoidFrom(double reach) const { return reach / 2 - 0.2; }
	void BuildHull();
	void BuildWallSolids();
	void BuildRoomSurfaces();
	void BuildFittings();
	void BuildRoomVoids();
	void BuildWallProbes();
	void BuildDoors();
	void BuildOpenings();
	void BuildPaths();
	void BuildHatches();
	void BuildStairs();
	virtual void BuildStations();
	void BuildBerths();
	virtual void ReportTurrets();
	void BuildGameTables();
	void BuildParking();
	void BuildEmitters();
	void ClearHullWays();
	void ReportRailings();
	virtual void ReportTheRest();

public:
	AuPoint At(double x, double y, double z) const { return AuPoint(x - m_Offset.x, y - m_Offset.y, z - m_Offset.z); }
	AuPoint At(const Vec3& p) const { return At(p.x, p.y, p.z); }
	AuPoint At(const P2& p, double z) const { return At(p.x, p.y, z); }
	AuScFace* Face(const Tri& t) const;
	AuScFace* Face(const Tri& t, ShipTex from, ShipTex to) const;
	void AddFaces(AuScFaceList& faces, const QVector<Tri>& tris) const;
	virtual void Decorate(AuScFace& face, const Tri& t) const { Q_UNUSED(face); Q_UNUSED(t); }
	virtual void DoorPicture(AuScFaceList& faces, const QVector<Tri>& tris, int picture) const;
	virtual bool RenderFitting(TriList& out, const Detail& d) const;
	virtual double RoofSolidRise() const { return RoofRise; }
	quint16 Add(AuScPart& part, const QString& what);
	void Log(const QString& line) { m_Log.append(line); }
	void Skip(const QString& line) { m_Info.skipped.append(line); m_Log.append(QString("NOT DONE: ") + line); NoteProblem(line, Problem::LeftOut); }
	void SkipRoom(const QString& line, const RoomData& rd);
	void NoteProblem(const QString& line, Problem::KIND kind);
	void Note(const QString& line) { m_Info.notes.append(line); m_Log.append(QString("NOTE: ") + line); }
	bool FloorPlace(const Detail& d, int& room, P2& at) const;
	int RoomAt(int deck, const P2& p, double reach = 0.0) const;
	QString RoomName(int room) const { return room >= 0 ? m_Rooms[room].name : QString(); }
	bool Walkable(int room) const { return room >= 0 && !m_Rooms[room].inner.isEmpty(); }
	QString Where(const Detail& d) const;
	void AddWallBox(AuScFaceList& faces, const P2& m, const P2& u, const P2& n, double from, double to, double halfWidth,
			double z0, double z1) const;
	quint16 AddRoomVoid(AuScFaceList& faces, int room, quint16 door, const QString& what);
	quint16 AddHullVoid(AuScFaceList& faces, quint16 door, const QString& what);
	void AddGroundPit(AuScFaceList& pit, int deck, const P2& m, const P2& u, const P2& n, double from, double to, double half,
			double zLow, double zHigh);
	quint16 AddWalkPath(const AuPoint& from, const AuPoint& to, quint16 door);
	double PathReach(int room, const P2& onWall, const P2& n) const;
	void WallStretch(int deck, const P2& p, const P2& u, const P2& n, double& back, double& ahead) const;

	const Design&		m_Ship;
	AuDesign&			m_Design;
	QStringList&		m_Log;
	ConvertInfo&		m_Info;
	ConvertOptions		m_Options;
	Vec3				m_Offset;
	QVector<RoomData>	m_Rooms;
	Builder				m_Builder;
	QVector<DoorData>	m_Doors;
	QVector<bool>		m_DoorMade;
	QVector<quint16>	m_DoorPart;
	QVector<Tri>		m_Hull;
	QVector<Tri>		m_Inside;
	struct Span { int deck; P2 mid, u; double half; };
	QVector<Span>		m_OpenSpans;
	struct DoorSide { int room; AuPoint at; P2 plan; };
	QVector<DoorSide>	m_DoorSides;
	struct HullDoorSpan { int deck; P2 mid, u, n; double half; quint16 part; };
	QVector<HullDoorSpan>	m_HullDoorSpans;
	struct HullWay { int deck; P2 mid, u, n; double half, from; quint16 part; QString what; };
	QVector<HullWay>	m_HullWays;
	struct ClearPlace { int deck; P2 at; double radius; };
	QVector<ClearPlace>	m_WallClear;
	QHash<QString, QList<quint16> >	m_PartsByBox;
	QMap<QString, int>	m_Duplicates;
	int					m_Failed;
	mutable QHash<QString, int>	m_WhereAsked;
	mutable QVector<Detail>		m_WhereDetails;
};

QVector<AuPoint> OutlineOnFloor(const Room& room);


int FindProblem(const QList<Problem>* problems, Problem::KIND kind, const QString& line);
QString ProblemLink(int index, const QString& text, const char* rgb);

void SuggestVolumes(const Design& ship, AuScDesign& design, ConvertInfo& info, QStringList& log);

quint32 LeastVolume(const QString& setting, const ConvertInfo& info);

bool SetVolume(AuScDesign& design, const ConvertInfo& info, const QString& setting, quint32 volume);

int BuildDoorStates(const Converter& c, AuScDoor& door, const DoorData& dd);

QStringList RoomsNobodyReaches(const Design& ship, const ConvertInfo& info, bool& noWayIn);

}

#endif
