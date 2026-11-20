#include "aulegacyshipdetails.h"
#include "aulegacyshipsave.h"
#include "aulegacybd.h"

#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace AuLegacyShip
{
namespace
{
constexpr double Fuzz = 1e-5;
constexpr double Pi = 3.14159265358979323846;
struct Placement { int room; bool bridge; };
// Original GD 0x18/0x3a; see detail-types.json and DnDetailType::Rx.
constexpr Placement Placements[] = {
	{0, false}, // 0: None
	{2, true}, // 1: Navigator Station
	{-1, false}, // 2: Double Berth
	{-2, true}, // 3: Large Hull Door
	{18, false}, // 4: Medic Station
	{-3, true}, // 5: Door
	{8, true}, // 6: Engineer Station
	{21, true}, // 7: Fire Control Station
	{-4, true}, // 8: Hatch
	{-2, false}, // 9: Hull Opening
	{8, false}, // 10: Maneuver Drive
	{18, false}, // 11: Surgery Unit
	{2, true}, // 12: Helm
	{8, false}, // 13: Power Relay Station
	{8, false}, // 14: Power Plant
	{16, true}, // 15: Sensor Station
	{16, false}, // 16: Sensors
	{17, true}, // 17: Shield Station
	{17, false}, // 18: Shields
	{20, false}, // 19: Transporter Station
	{20, false}, // 20: Transporter Pad
	{-2, true}, // 21: Turret (Side)
	{-5, true}, // 22: Turret (Top/Bottom)
	{21, false}, // 23: Weapon System
	{8, false}, // 24: FTL Drive
	{-1, false}, // 25: Double Berth
	{-1, false}, // 26: Double Berth
	{-1, false}, // 27: Double Berth
	{-1, false}, // 28: Double Berth
	{-1, false}, // 29: Double Berth
	{-1, false}, // 30: Double Berth
	{-1, false}, // 31: Double Berth
	{8, false}, // 32: Life Support
	{-5, true}, // 33: Turret (Top/Bottom)
	{-2, true}, // 34: Turret (Side)
	{-2, true}, // 35: Turret (Side)
	{-2, true}, // 36: Turret (Side)
	{-2, true}, // 37: Turret (Side)
	{-2, true}, // 38: Turret (Side)
	{-2, true}, // 39: Turret (Side)
	{-2, true}, // 40: Turret (Side)
	{13, false}, // 41: Space Fighter Location
	{13, false}, // 42: Space Fighter Location
	{13, false}, // 43: Space Fighter Location
	{13, false}, // 44: Space Fighter Location
	{13, false}, // 45: Space Fighter Location
	{13, false}, // 46: Space Fighter Location
	{13, false}, // 47: Space Fighter Location
	{13, false}, // 48: Space Fighter Location
	{-3, true}, // 49: Crew Door
	{-2, true}, // 50: Hull Door
	{-4, true}, // 51: Crew Hatch
	{-5, true}, // 52: Hull Hatch
	{-3, true}, // 53: Large Door
	{-3, true}, // 54: Large Crew Door
	{-3, true}, // 55: Wall Opening
	{-3, true}, // 56: Railing
	{13, false}, // 57: Vehicle Location
	{13, false}, // 58: Vehicle Location
	{13, false}, // 59: Vehicle Location
	{13, false}, // 60: Vehicle Location
	{13, false}, // 61: Vehicle Location
	{13, false}, // 62: Vehicle Location
	{13, false}, // 63: Vehicle Location
	{13, false}, // 64: Vehicle Location
	{-1, false}, // 65: Single Berth
	{-1, false}, // 66: Single Berth
	{-1, false}, // 67: Single Berth
	{-1, false}, // 68: Single Berth
	{-1, false}, // 69: Single Berth
	{-1, false}, // 70: Single Berth
	{-1, false}, // 71: Single Berth
	{-1, false}, // 72: Single Berth
	{2, true}, // 73: Captain's Chair
	{14, false}, // 74: Game Table
	{14, false}, // 75: Game Table
	{-6, true}, // 76: Pivot Point
	{-4, true}, // 77: Hatch
	{-4, true}, // 78: Hatch
	{-4, true}, // 79: Hatch
	{-4, true}, // 80: Hatch
	{-4, true}, // 81: Hatch
	{-4, true}, // 82: Hatch
	{-4, true}, // 83: Hatch
	{-4, true}, // 84: Crew Hatch
	{-4, true}, // 85: Crew Hatch
	{-4, true}, // 86: Crew Hatch
	{-4, true}, // 87: Crew Hatch
	{-4, true}, // 88: Crew Hatch
	{-4, true}, // 89: Crew Hatch
	{-4, true}, // 90: Crew Hatch
	{-5, true}, // 91: Hull Hatch
	{-5, true}, // 92: Hull Hatch
	{-5, true}, // 93: Hull Hatch
	{-5, true}, // 94: Hull Hatch
	{-5, true}, // 95: Hull Hatch
	{-5, true}, // 96: Hull Hatch
	{-5, true}, // 97: Hull Hatch
	{-7, true}, // 98: Contrail Emitter
	{21, false}, // 99: Weapon Emitter
	{-8, false}, // 100: Flag Pole
	{-9, true}, // 101: Stairs, Straight
	{-9, true}, // 102: Stairs, Straight
	{-9, true}, // 103: Stairs, Straight
	{-9, true}, // 104: Stairs, Straight
	{-9, true}, // 105: Stairs, Straight
	{-9, true}, // 106: Stairs, Straight
	{-9, true}, // 107: Stairs, Straight
	{-9, true}, // 108: Stairs, Straight
	{-9, true}, // 109: Stairs, Turn Left
	{-9, true}, // 110: Stairs, Turn Left
	{-9, true}, // 111: Stairs, Turn Left
	{-9, true}, // 112: Stairs, Turn Left
	{-9, true}, // 113: Stairs, Turn Left
	{-9, true}, // 114: Stairs, Turn Left
	{-9, true}, // 115: Stairs, Turn Left
	{-9, true}, // 116: Stairs, Turn Left
	{-9, true}, // 117: Stairs, Turn Right
	{-9, true}, // 118: Stairs, Turn Right
	{-9, true}, // 119: Stairs, Turn Right
	{-9, true}, // 120: Stairs, Turn Right
	{-9, true}, // 121: Stairs, Turn Right
	{-9, true}, // 122: Stairs, Turn Right
	{-9, true}, // 123: Stairs, Turn Right
	{-9, true}, // 124: Stairs, Turn Right
	{-9, true}, // 125: Stairs, U-turn Left
	{-9, true}, // 126: Stairs, U-turn Left
	{-9, true}, // 127: Stairs, U-turn Left
	{-9, true}, // 128: Stairs, U-turn Left
	{-9, true}, // 129: Stairs, U-turn Left
	{-9, true}, // 130: Stairs, U-turn Left
	{-9, true}, // 131: Stairs, U-turn Left
	{-9, true}, // 132: Stairs, U-turn Left
	{-9, true}, // 133: Stairs, U-turn Right
	{-9, true}, // 134: Stairs, U-turn Right
	{-9, true}, // 135: Stairs, U-turn Right
	{-9, true}, // 136: Stairs, U-turn Right
	{-9, true}, // 137: Stairs, U-turn Right
	{-9, true}, // 138: Stairs, U-turn Right
	{-9, true}, // 139: Stairs, U-turn Right
	{-9, true}, // 140: Stairs, U-turn Right
	{-10, true}, // 141: Troop Station
	{-8, false}, // 142: Light Pole
};

double Dot(const QPointF& a, const QPointF& b) { return a.x()*b.x() + a.y()*b.y(); }
bool Near(const QPointF& a, const QPointF& b)
{
	return std::abs(a.x()-b.x()) <= Fuzz && std::abs(a.y()-b.y()) <= Fuzz;
}
QPointF Rotate(const QPointF& p, double angle)
{
	const double c = std::cos(angle), s = std::sin(angle);
	return QPointF(p.x()*c - p.y()*s, p.y()*c + p.x()*s);
}
QPointF Closest(const QPointF& p, const QPointF& a, const QPointF& b)
{
	const QPointF v = b-a;
	const double len = std::sqrt(Dot(v,v));
	if(len == 0) return a;
	const double along = Dot(p-a,v)/len;
	if(along <= 0) return a;
	if(along >= len) return b;
	return QPointF(v.x()*along/len+a.x(), v.y()*along/len+a.y());
}
double Distance(const QPointF& a, const QPointF& b)
{
	const QPointF v=a-b;
	return std::sqrt(Dot(v,v));
}
bool Between(const QPointF& p, const QPointF& a, const QPointF& b)
{
	if(Near(p,a) || Near(p,b)) return true;
	const QPointF v=b-a;
	const double len=std::sqrt(Dot(v,v));
	if(len==0) return false;
	const double along=Dot(p-a,v/len);
	return along>=0 && along<=len;
}
bool On(const QPolygonF& polygon, const QPointF& p)
{
	for(int i=1;i<polygon.size();++i)
		if(Distance(p,Closest(p,polygon[i-1],polygon[i]))<=Fuzz) return true;
	return polygon.size()>2 &&
		Distance(p,Closest(p,polygon.last(),polygon.first()))<=Fuzz;
}
// AuMath13.dll 0x18000ce40: reproduce the original rotated-ray test and
// its fuzzy vertex retries instead of Qt's differently defined edge handling.
bool Inside(const QPolygonF& polygon, const QPointF& p)
{
	if(polygon.size()<3) return false;
	const QRectF bounds=polygon.boundingRect();
	if(p.x()<=bounds.left() || p.x()>=bounds.right() ||
		p.y()<=bounds.top() || p.y()>=bounds.bottom()) return false;
	QPointF ray=Rotate(QPointF(bounds.width(),bounds.height()),Pi/180);
	int tries=0;
	for(;;)
	{
		const QPointF end=p+ray;
		QPointF previous=polygon.last();
		int crossings=0;
		bool retry=false;
		for(const QPointF& current:polygon)
		{
			if(current.x()==previous.x()&&current.y()==previous.y()) continue;
			const QPointF d0=end-p, d1=previous-current, d2=current-p;
			const double d00=Dot(d0,d0), d01=Dot(d0,d1), d11=Dot(d1,d1);
			const double fa=d01*d01, fb=d00*d11, det=fa-fb;
			if(std::abs(det)<=Fuzz*(std::abs(fa)+std::abs(fb)))
				retry=Distance(current,Closest(current,p,end))<=Fuzz;
			else
			{
				const double r0=Dot(d2,d0), r1=Dot(d2,d1);
				const QPointF a=p+d0*((r1*d01-r0*d11)/det);
				const QPointF b=current+d1*((d00*r1-d01*r0)/det);
				if(Near(a,b) && Between(a,p,end) && Between(a,current,previous))
				{
					if(Near(a,p)) return false;
					if(Near(a,current)||Near(a,previous))
					{
						if(++tries==360) return false;
						retry=true;
					}
					++crossings;
				}
			}
			if(retry) break;
			previous=current;
		}
		if(!retry) return (crossings&1)!=0;
		ray=Rotate(ray,Pi/180);
	}
}
int RotationStep(int t)
{
	if(t>=0x19 && t<=0x1f) return t-0x18;
	if(t>=0x22 && t<=0x28) return t-0x21;
	if(t>=0x39 && t<=0x40) return t-0x39;
	if(t>=0x41 && t<=0x48) return t-0x41;
	if(t==0x4b) return 1;
	if(t>=0x4d && t<=0x53) return t-0x4c;
	if(t>=0x54 && t<=0x5a) return t-0x53;
	if(t>=0x5b && t<=0x61) return t-0x5a;
	if(t>=0x65 && t<=0x8c) return (t-0x65)%8;
	return 0;
}
double Rotation(int t) { return double(float(RotationStep(t)*Pi/4)); }
bool IsBerth(int t) { return t==2 || (t>=0x19&&t<=0x1f) || (t>=0x41&&t<=0x48); }
bool IsTurret(int t) { return t==0x15 || t==0x16 || (t>=0x21&&t<=0x28); }
bool IsStairs(int t) { return t>=0x65&&t<=0x8c; }
bool LimitOne(int t)
{
	switch(t)
	{
	case 1: case 4: case 6: case 7: case 0xb: case 0xc: case 0xd:
	case 0xf: case 0x11: case 0x13: return true;
	default: return false;
	}
}
int VehicleCommodity(const Detail& d)
{
	if(d.detailType>=0x39 && d.detailType<=0x40) return d.commodityIdNormalized;
	return 0;
}
// Filled from original model-resource bounds, never modern replacement assets.
bool VehicleHalfSize(int commodity, QPointF& result, QString& error)
{
	QPointF half;
	switch(commodity)
	{
	case 0: case 54: case 70: case 71: case 103:
		// Empty/missing original resources cause CreateGeometry to return null.
		half=QPointF(0,0); break;
	case 6: half=QPointF(8.319742202758789,11.813350200653076); break;
	case 7: half=QPointF(10.208333015441895,19.750000715255737); break;
	case 8: half=QPointF(14.044610023498535,14.789060592651367); break;
	case 9: half=QPointF(18.64858913421631,13.657800674438477); break;
	case 28: half=QPointF(4.406362771987915,7.853582143783569); break;
	case 77: half=QPointF(7.353567838668823,10.512041091918945); break;
	case 82: half=QPointF(8.201797008514404,9.152505874633789); break;
	case 104: half=QPointF(6.217533111572266,8.501940727233887); break;
	case 132: half=QPointF(5,16.090107917785645); break;
	case 133: half=QPointF(1.49844628572464,4.320924758911133); break;
	case 231: half=QPointF(12.763139724731445,13.934858798980713); break;
	default:
		error=QStringLiteral("The original vehicle model bounds for commodity %1 are not available; the edit was not applied.").arg(commodity);
		return false;
	}
	// Both original fit and spacing routines narrow model half-extents to float.
	result=QPointF(double(float(half.x())),double(float(half.y())));
	return true;
}
bool Spacing(const Detail& d, QPointF& result, QString& error)
{
	const int t=d.detailType;
	if(t>=0x39&&t<=0x40)
	{
		QPointF half;
		if(!VehicleHalfSize(d.commodityIdNormalized,half,error)) return false;
		result=QPointF();
		const double angle=(t-0x39)*Pi/4;
		for(int x:{-1,1}) for(int y:{-1,1})
		{
			const QPointF p=Rotate(QPointF(x*half.y(),y*half.x()),angle);
			result.setX(std::max(result.x(),p.x()));
			result.setY(std::max(result.y(),p.y()));
		}
		return true;
	}
	double value=0;
	switch(t)
	{
	case 1: case 2: case 4: case 6: case 7: case 0xb: case 0xc: case 0xd:
	case 0xf: case 0x11: case 0x13: case 0x19: case 0x1a: case 0x1b:
	case 0x1c: case 0x1d: case 0x1e: case 0x1f: case 0x41: case 0x42:
	case 0x43: case 0x44: case 0x45: case 0x46: case 0x47: case 0x48:
	case 0x49: value=2.7450937911666666; break;
	case 10: case 0xe: case 0x10: case 0x12: case 0x14: case 0x15:
	case 0x16: case 0x17: case 0x18: case 0x20: case 0x21: case 0x22:
	case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28:
	case 100: case 0x8d: case 0x8e: value=1; break;
	case 0x4a: value=5; break;
	case 0x4b: value=7.071; break;
	default: if(IsStairs(t)) value=1; break;
	}
	result=QPointF(value,value);
	return true;
}
struct Geometry
{
	const Room* room;
	QPolygonF polygon;
};
// DnDetail::IsValid constructs seven double-precision test points. This is
// deliberately distinct from StairBoundary's float-sized collision polygon.
QVector<QPointF> StairChecks(const Detail& d)
{
	const int family=(d.detailType-0x65)/8;
	const bool diagonal=((d.detailType-0x65)&1)!=0;
	QVector<QPointF> p(7);
	double a,b;
	switch(family)
	{
	case 0:
		a=diagonal?12.0:GridSpacing*2; b=diagonal?4.0:GridSpacing;
		p={QPointF(0,b),QPointF(0,0),QPointF(a,b),QPointF(a,0),
			QPointF(a*.5,b),QPointF(a*.5,0),QPointF(a*.5,b*.5)};
		break;
	case 1:
		a=diagonal?8.0:GridSpacing; b=diagonal?4.0:GridSpacing;
		p={QPointF(0,b),QPointF(0,0),QPointF(a,a+b),QPointF(a+b,a+b),
			QPointF(a,b),QPointF(a+b,0),QPointF((2*a+b)*.5,b*.5)};
		break;
	case 2:
		a=diagonal?8.0:GridSpacing; b=diagonal?4.0:GridSpacing;
		p={QPointF(0,0),QPointF(0,-b),QPointF(a+b,-a-b),QPointF(a,-a-b),
			QPointF(a+b,0),QPointF(a,-b),QPointF((2*a+b)*.5,-b*.5)};
		break;
	case 3:
		a=diagonal?12.0:GridSpacing*2; b=diagonal?6.0:GridSpacing;
		p={QPointF(0,a),QPointF(0,0),QPointF(a,a),QPointF(a,0),
			QPointF(b,a),QPointF(b,0),QPointF(a*.5,a*.5)};
		break;
	default:
		a=diagonal?12.0:GridSpacing*2; b=diagonal?6.0:GridSpacing;
		p={QPointF(0,0),QPointF(0,-a),QPointF(a,0),QPointF(a,-a),
			QPointF(b,0),QPointF(b,-a),QPointF(a*.5,-a*.5)};
		break;
	}
	const QPointF origin=DecodePoint(d.packedXY);
	for(QPointF& v:p) v=origin+Rotate(v,Rotation(d.detailType));
	return p;
}
QPolygonF StairBoundary(const Detail& d)
{
	const int family=(d.detailType-0x65)/8;
	const bool diagonal=((d.detailType-0x65)&1)!=0;
	const float a=diagonal?(family==1||family==2?8.0f:12.0f):
		(family==1||family==2?float(GridSpacing):float(GridSpacing*2));
	const float b=diagonal?4.0f:float(GridSpacing);
	QVector<QPointF> p;
	if(family==0) p={QPointF(0,0),QPointF(0,b),QPointF(a,b),QPointF(a,0)};
	else if(family==1) p={QPointF(0,0),QPointF(0,b),QPointF(a,b),
		QPointF(a,float(a+b)),QPointF(float(a+b),float(a+b)),QPointF(float(a+b),0)};
	else if(family==2) p={QPointF(0,0),QPointF(float(a+b),0),
		QPointF(float(a+b),-float(a+b)),QPointF(a,-float(a+b)),
		QPointF(a,-b),QPointF(0,-b)};
	else if(family==3) p={QPointF(0,0),QPointF(0,a),QPointF(a,a),QPointF(a,0)};
	else p={QPointF(0,0),QPointF(a,0),QPointF(a,-a),QPointF(0,-a)};
	const QPointF origin=DecodePoint(d.packedXY);
	for(QPointF& v:p) v=origin+Rotate(v,Rotation(d.detailType));
	return p;
}
bool ValidStairs(const Detail& d, const QVector<Geometry>& rooms)
{
	const QVector<QPointF> points=StairChecks(d);
	QVector<const Geometry*> possible;
	for(const Geometry& r:rooms)
	{
		if(r.room->deck!=d.deck) continue;
		const bool forbidden=r.room->roomType==9 ||
			(r.room->roomType>=0x16&&r.room->roomType<=0x18);
		if(On(r.polygon,points[0]))
		{
			if(!forbidden) possible.append(&r);
		}
		else if(Inside(r.polygon,points[0]))
		{
			if(forbidden) return false;
			possible.append(&r);
		}
	}
	if(possible.isEmpty()) return false;
	for(int i=1;i<points.size();++i)
	{
		bool found=false;
		for(const Geometry* r:possible)
			if(Inside(r->polygon,points[i])||On(r->polygon,points[i])) {found=true;break;}
		if(!found) return false;
	}
	return true;
}
bool OnVertex(const QPolygonF& polygon, const QPointF& p)
{
	// AuPointArray::DoesContain uses component-wise fuzzy equality.
	for(const QPointF& v:polygon) if(Near(v,p)) return true;
	return false;
}
int SideTurretType(const QPolygonF& polygon, const QPointF& p)
{
	QPointF a,b;
	double closest=std::numeric_limits<double>::max();
	for(int i=0;i<polygon.size();++i)
	{
		const QPointF current=polygon[i], next=polygon[(i+1)%polygon.size()];
		const double distance=Distance(p,Closest(p,current,next));
		if(distance<closest) {closest=distance;a=current;b=next;}
	}
	QPointF tangent=b-a;
	const double length=std::sqrt(Dot(tangent,tangent));
	if(length==0) tangent.setX(1);
	else tangent/=length;
	QPointF normal(-tangent.y(),tangent.x());
	if(Inside(polygon,p+normal)) normal=-normal;
	double angle;
	if(std::abs(normal.y())<=Fuzz) angle=normal.x()<-Fuzz?Pi:0;
	else if(std::abs(normal.x())<=Fuzz) angle=normal.y()>=-Fuzz?Pi/2:-Pi/2;
	else angle=std::atan2(normal.y(),normal.x());
	if(angle<0) angle+=2*Pi;
	if(angle<0.39269908169872414) return 0x15;
	if(angle<1.1780972450961724) return 0x22;
	if(angle<1.9634954084936207) return 0x23;
	if(angle<2.748893571891069) return 0x24;
	if(angle<3.5342917352885173) return 0x25;
	if(angle<4.319689898685966) return 0x26;
	if(angle<5.105088062083414) return 0x27;
	if(angle<5.8904862254808625) return 0x28;
	return 0x15;
}
enum class DetailResult { Valid, Invalid, Unavailable };
DetailResult TestDetail(Detail& d, const QVector<Geometry>& rooms,
	const QVector<Detail>& details, bool building, QString& error)
{
	const int t=d.detailType;
	const QPointF point=DecodePoint(d.packedXY);
	const int gx=qint8(d.packedXY>>8), gy=qint8(d.packedXY&255);
	if(std::abs(int(d.deck))>42 || std::abs(gx)>120 || std::abs(gy)>120 ||
		t>=int(sizeof(Placements)/sizeof(Placements[0]))) return DetailResult::Invalid;
	const Placement placement=Placements[t];
	if(placement.room==-8 && d.deck!=0) return DetailResult::Invalid;
	if(placement.room==-9) return ValidStairs(d,rooms)?DetailResult::Valid:DetailResult::Invalid;
	if(placement.room==-6)
	{
		bool any=false;
		double minX=0,minY=0,maxX=0,maxY=0;
		int low=0,high=0;
		for(const Geometry& r:rooms) if(!r.polygon.isEmpty())
		{
			const QRectF b=r.polygon.boundingRect();
			if(!any) {minX=b.left();minY=b.top();maxX=b.right();maxY=b.bottom();low=high=r.room->deck;any=true;}
			else {minX=std::min(minX,b.left());minY=std::min(minY,b.top());maxX=std::max(maxX,b.right());maxY=std::max(maxY,b.bottom());low=std::min(low,int(r.room->deck));high=std::max(high,int(r.room->deck));}
		}
		return any && point.x()>=minX&&point.x()<=maxX&&point.y()>=minY&&point.y()<=maxY&&
			d.deck>=low&&d.deck<=high+1?DetailResult::Valid:DetailResult::Invalid;
	}
	int count=0;
	for(const Geometry& r:rooms)
	{
		if(r.room->deck!=d.deck) continue;
		const bool inside=Inside(r.polygon,point), on=On(r.polygon,point);
		if(!inside&&!on) continue;
		const int roomType=r.room->roomType;
		if(roomType==2&&!placement.bridge) return DetailResult::Invalid;
		switch(placement.room)
		{
		case -1:
			if(count!=0 || !(roomType==1||roomType==4||roomType==7||roomType==0x13)||on)
				return DetailResult::Invalid;
			if(IsBerth(t)&&!Inside(r.polygon,point+Rotate(QPointF(CoordinateSpacing,0),Rotation(t))))
				return DetailResult::Invalid;
			break;
		case -2:
			if((roomType==9&&t!=0x62)||count!=0||!on||OnVertex(r.polygon,point)) return DetailResult::Invalid;
			if(IsTurret(t)) d.detailType=quint8(SideTurretType(r.polygon,point));
			break;
		case -3:
			if(roomType==9||count>1||!on||OnVertex(r.polygon,point)) return DetailResult::Invalid;
			break;
		case -4: case -10:
			if(roomType==9||count!=0||on) return DetailResult::Invalid;
			break;
		case -5:
			if(roomType==9||count!=0||on) return DetailResult::Invalid;
			if(IsTurret(t)) d.detailType=0x21;
			break;
		case -7:
			for(const Detail& other:details)
				if(other.detailId!=d.detailId && other.detailType==t&&other.deck==d.deck&&
					Near(DecodePoint(other.packedXY),point)) return DetailResult::Invalid;
			break;
		case -8: return DetailResult::Invalid;
		default:
			if(count!=0) return DetailResult::Invalid;
			if(t==0x4a||t==0x4b)
			{
				if(roomType!=placement.room&&roomType!=10) return DetailResult::Invalid;
			}
			else if(roomType!=placement.room && !(roomType==2&&placement.bridge)) return DetailResult::Invalid;
			if(on && !(t==10||t==14||t==16||t==18||t==23||t==0x63)) return DetailResult::Invalid;
			if(LimitOne(t))
			{
				for(const Detail& other:details)
					if(other.detailId!=d.detailId&&other.detailType==t&&other.deck==d.deck&&
						other.roomId==r.room->roomId) return DetailResult::Invalid;
			}
			else if(t==0x63)
			{
				for(const Detail& other:details) if(other.detailId!=d.detailId&&other.detailType==t)
				{
					int subtype=0;
					for(const Geometry& rr:rooms) if(rr.room->roomId==other.roomId)
						{subtype=rr.room->roomSubtype;break;}
					if(subtype==r.room->roomSubtype) return DetailResult::Invalid;
				}
			}
			if(VehicleCommodity(d)!=0)
			{
				QPointF half;
				if(!VehicleHalfSize(VehicleCommodity(d),half,error)) return DetailResult::Unavailable;
				for(int x:{-1,1}) for(int y:{-1,1})
					if(!Inside(r.polygon,point+Rotate(QPointF(x*half.y(),y*half.x()),Rotation(t))))
						return DetailResult::Invalid;
			}
			else if(t==0x4a||t==0x4b)
				for(int x:{-1,1}) for(int y:{-1,1})
					if(!Inside(r.polygon,point+Rotate(QPointF(x*5,y*5),t==0x4b?Pi/4:0)))
						return DetailResult::Invalid;
			break;
		}
		QPointF spacing;
		Detail original=d;
		original.detailType=quint8(t);
		if(!Spacing(original,spacing,error)) return DetailResult::Unavailable;
		if(spacing.x()!=0&&spacing.y()!=0)
			for(const Detail& other:details)
			{
				if(other.detailId==d.detailId||other.deck!=d.deck||other.roomId!=r.room->roomId) continue;
				if((t==100||t==0x8e)&&other.detailType==t) continue;
				if(IsStairs(other.detailType))
				{
					const QPolygonF stairs=StairBoundary(other);
					// Confirmed original bug: tests the stair origin, not point.
					const QPointF origin=DecodePoint(other.packedXY);
					if(Inside(stairs,origin)||On(stairs,origin)) return DetailResult::Invalid;
				}
				else
				{
					QPointF theirs;
					if(!Spacing(other,theirs,error)) return DetailResult::Unavailable;
					if(theirs.x()!=0||theirs.y()!=0)
					{
						const QPointF delta=DecodePoint(other.packedXY)-point;
						if(std::abs(delta.x())<spacing.x()+theirs.x()&&
							std::abs(delta.y())<spacing.y()+theirs.y()) return DetailResult::Invalid;
					}
				}
			}
		d.roomId=r.room->roomId;
		++count;
	}
	if(placement.room==-5||placement.room==-4)
	{
		int below=0;
		for(const Geometry& r:rooms) if(r.room->deck==d.deck-1)
		{
			if(On(r.polygon,point)) return DetailResult::Invalid;
			if(!Inside(r.polygon,point)) continue;
			if(r.room->roomType==9||below!=0) return DetailResult::Invalid;
			if(count==0)
			{
				if(IsTurret(t)) d.detailType=0x16;
				d.roomId=r.room->roomId;
			}
			below=1;
			++count;
		}
		if(count==0 || (building && placement.room == -5 &&
			(d.deck < -1 || (d.deck < 1 && !below)))) return DetailResult::Invalid;
	}
	else
	{
		// DnDetail::IsValid (0x18015feae). Outdoor poles and troop stations
		// can stand clear of rooms on the ground story.
		if(count == 0 && !(building && d.deck == 0 && (placement.room == -8 || placement.room == -10)))
			return DetailResult::Invalid;
		if(building && d.deck < 0 && (t == 3 || t == 9 || t == 50 || t == 52 || (t >= 91 && t <= 97)))
			return DetailResult::Invalid;
	}
	if((placement.room==-4||placement.room==-3)&&count!=2) return DetailResult::Invalid;
	if((placement.room==-5||placement.room==-2)&&count!=1) return DetailResult::Invalid;
	return DetailResult::Valid;
}

bool CollectRooms(const Design& design, QVector<Geometry>& rooms, QString& error)
{
	rooms.reserve(design.rooms.size());
	for(const Room& room : design.rooms)
	{
		QString geometryError;
		const QPolygonF polygon = RoomBoundary(room, &geometryError);
		if(!geometryError.isEmpty()) { error = geometryError; return false; }
		if(!polygon.isEmpty()) rooms.append({&room, polygon});
	}
	return true;
}

bool SnapDetail(const QPointF& cursor, quint16& packed)
{
	const double x = std::round(cursor.x() / CoordinateSpacing);
	const double y = std::round(cursor.y() / CoordinateSpacing);
	if(!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 120 || std::abs(y) > 120)
		return false;
	packed = quint16(((int(x) & 255) << 8) | (int(y) & 255));
	return true;
}

bool CanBeOnStairEdge(int type)
{
	if(IsStairs(type)) return true;
	switch(type)
	{
	case 3: case 5: case 9: case 21: case 34: case 35: case 36: case 37:
	case 38: case 39: case 40: case 49: case 50: case 53: case 54: case 55:
	case 56: case 76: case 98: case 99: return true;
	default: return false;
	}
}

quint32 NextDetailId(const QVector<Detail>& details)
{
	QSet<quint32> used;
	for(const Detail& detail : details) used.insert(detail.detailId);
	for(quint32 id = 1; id != 0; ++id)
		if(!used.contains(id)) return id;
	return 0;
}

void InsertDetail(QVector<Detail>& details, const Detail& detail)
{
	int at = 0;
	for(; at < details.size(); ++at)
	{
		const Detail& other = details.at(at);
		if(detail.deck < other.deck || (detail.deck == other.deck &&
			(detail.roomId < other.roomId || (detail.roomId == other.roomId && detail.detailId < other.detailId))))
			break;
	}
	details.insert(at, detail);
}

enum class PlacementResult { Added, Unchanged, Rejected, Unavailable };

PlacementResult PlaceOne(QVector<Detail>& details, const QVector<Geometry>& rooms,
	const Design& design, int deck, quint16 packed, int type, quint16 commodity,
	Detail& placed, int& removed, QString& error)
{
	Detail detail;
	detail.deck = qint8(deck);
	detail.detailType = quint8(type);
	detail.designId = design.header.designId;
	detail.packedXY = packed;
	detail.commodityIdRaw = commodity;
	detail.commodityIdNormalized = commodity == 0xf9 ? 0xf8 : commodity == 0xfb ? 0xfa : commodity;
	const QPointF point = DecodePoint(packed);
	QVector<Detail> candidate = details;
	removed = 0;
	const bool lightPole = design.building && type == 142;
	if(type != 76 && type != 98 && type != 99 && !lightPole)
	{
		const QPolygonF stair = IsStairs(type) ? StairBoundary(detail) : QPolygonF();
		for(int i = 0; i < candidate.size();)
		{
			const Detail& other = candidate.at(i);
			if(other.deck != deck || other.detailType == 76 || other.detailType == 98 || other.detailType == 99)
			{ ++i; continue; }
			bool replace = false;
			if(IsStairs(type))
			{
				if(IsStairs(other.detailType))
				{
					for(const QPointF& vertex : StairBoundary(other))
						if(Inside(stair, vertex)) { replace = true; break; }
				}
				else
				{
					const QPointF location = DecodePoint(other.packedXY);
					replace = Inside(stair, location) || (!CanBeOnStairEdge(other.detailType) && On(stair, location));
				}
			}
			else if(IsStairs(other.detailType))
			{
				const QPolygonF boundary = StairBoundary(other);
				if(Inside(boundary, point) || (!CanBeOnStairEdge(type) && On(boundary, point)))
				{
					error = QStringLiteral("%1 cannot overlap these stairs.").arg(DetailName(type));
					return PlacementResult::Rejected;
				}
			}
			else if(other.packedXY == packed)
			{
				if(other.detailType == type) { placed = other; return PlacementResult::Unchanged; }
				replace = true;
			}
			if(replace)
			{
				candidate.removeAt(i);
				++removed;
				if(!IsStairs(type)) break;
			}
			else ++i;
		}
	}
	const DetailResult valid = TestDetail(detail, rooms, candidate, design.building, error);
	if(valid == DetailResult::Unavailable) return PlacementResult::Unavailable;
	if(valid == DetailResult::Invalid)
	{
		error = QStringLiteral("Cannot place %1 here. %2 Check its clearance from other equipment.")
			.arg(DetailName(type), DetailPlacementDescription(type));
		return PlacementResult::Rejected;
	}
	// BuildingDesignAddLightPole (AuData13 0x18012c410) validates the
	// new location before replacing the design's light pole and allocating its ID.
	if(type == 76 || lightPole)
	{
		for(int i = candidate.size() - 1; i >= 0; --i)
			if(candidate.at(i).detailType == type) { candidate.removeAt(i); ++removed; }
	}
	else if(type == 98)
	{
		int count = 0;
		for(const Detail& other : candidate) if(other.detailType == 98) ++count;
		if(count >= 10)
		{
			error = QStringLiteral("A spacecraft can have at most ten contrail emitters.");
			return PlacementResult::Rejected;
		}
	}
	if(candidate.size() >= MaximumRecords)
	{
		error = QStringLiteral("The equipment record limit has been reached.");
		return PlacementResult::Unavailable;
	}
	detail.detailId = NextDetailId(candidate);
	if(!detail.detailId)
	{
		error = QStringLiteral("No equipment identifier is available.");
		return PlacementResult::Unavailable;
	}
	InsertDetail(candidate, detail);
	details = std::move(candidate);
	placed = detail;
	return PlacementResult::Added;
}

int DeleteOne(QVector<Detail>& details, int deck, quint16 packed, int type)
{
	for(int i = 0; i < details.size(); ++i)
	{
		const Detail& detail = details.at(i);
		if(detail.deck != deck || detail.packedXY != packed) continue;
		if(type && detail.detailType != type) return -1;
		const int removedType = detail.detailType;
		details.removeAt(i);
		return removedType;
	}
	return -1;
}

bool ValidEditInput(int deck, const QPointF& cursor, int brushRadius, quint16& packed, QString& error)
{
	if(deck < -42 || deck > 42)
	{
		error = QStringLiteral("Equipment must be on a legacy deck from -42 through 42.");
		return false;
	}
	if(brushRadius < 0 || brushRadius > 25)
	{
		error = QStringLiteral("The equipment brush radius is outside the legacy grid.");
		return false;
	}
	if(!SnapDetail(cursor, packed))
	{
		error = QStringLiteral("The equipment position is outside the legacy design grid.");
		return false;
	}
	return true;
}
}

QString DetailName(int type)
{
	static const char* const names[] = {
		"None",
		"Navigator Station",
		"Double Berth",
		"Large Hull Door",
		"Medic Station",
		"Door",
		"Engineer Station",
		"Fire Control Station",
		"Hatch",
		"Hull Opening",
		"Maneuver Drive",
		"Surgery Unit",
		"Helm",
		"Power Relay Station",
		"Power Plant",
		"Sensor Station",
		"Sensors",
		"Shield Station",
		"Shields",
		"Transporter Station",
		"Transporter Pad",
		"Turret (Side)",
		"Turret (Top/Bottom)",
		"Weapon System",
		"FTL Drive",
		"Double Berth",
		"Double Berth",
		"Double Berth",
		"Double Berth",
		"Double Berth",
		"Double Berth",
		"Double Berth",
		"Life Support",
		"Turret (Top/Bottom)",
		"Turret (Side)",
		"Turret (Side)",
		"Turret (Side)",
		"Turret (Side)",
		"Turret (Side)",
		"Turret (Side)",
		"Turret (Side)",
		"Space Fighter Location",
		"Space Fighter Location",
		"Space Fighter Location",
		"Space Fighter Location",
		"Space Fighter Location",
		"Space Fighter Location",
		"Space Fighter Location",
		"Space Fighter Location",
		"Crew Door",
		"Hull Door",
		"Crew Hatch",
		"Hull Hatch",
		"Large Door",
		"Large Crew Door",
		"Wall Opening",
		"Railing",
		"Vehicle Location",
		"Vehicle Location",
		"Vehicle Location",
		"Vehicle Location",
		"Vehicle Location",
		"Vehicle Location",
		"Vehicle Location",
		"Vehicle Location",
		"Single Berth",
		"Single Berth",
		"Single Berth",
		"Single Berth",
		"Single Berth",
		"Single Berth",
		"Single Berth",
		"Single Berth",
		"Captain's Chair",
		"Game Table",
		"Game Table",
		"Pivot Point",
		"Hatch",
		"Hatch",
		"Hatch",
		"Hatch",
		"Hatch",
		"Hatch",
		"Hatch",
		"Crew Hatch",
		"Crew Hatch",
		"Crew Hatch",
		"Crew Hatch",
		"Crew Hatch",
		"Crew Hatch",
		"Crew Hatch",
		"Hull Hatch",
		"Hull Hatch",
		"Hull Hatch",
		"Hull Hatch",
		"Hull Hatch",
		"Hull Hatch",
		"Hull Hatch",
		"Contrail Emitter",
		"Weapon Emitter",
		"Flag Pole",
		"Stairs, Straight",
		"Stairs, Straight",
		"Stairs, Straight",
		"Stairs, Straight",
		"Stairs, Straight",
		"Stairs, Straight",
		"Stairs, Straight",
		"Stairs, Straight",
		"Stairs, Turn Left",
		"Stairs, Turn Left",
		"Stairs, Turn Left",
		"Stairs, Turn Left",
		"Stairs, Turn Left",
		"Stairs, Turn Left",
		"Stairs, Turn Left",
		"Stairs, Turn Left",
		"Stairs, Turn Right",
		"Stairs, Turn Right",
		"Stairs, Turn Right",
		"Stairs, Turn Right",
		"Stairs, Turn Right",
		"Stairs, Turn Right",
		"Stairs, Turn Right",
		"Stairs, Turn Right",
		"Stairs, U-turn Left",
		"Stairs, U-turn Left",
		"Stairs, U-turn Left",
		"Stairs, U-turn Left",
		"Stairs, U-turn Left",
		"Stairs, U-turn Left",
		"Stairs, U-turn Left",
		"Stairs, U-turn Left",
		"Stairs, U-turn Right",
		"Stairs, U-turn Right",
		"Stairs, U-turn Right",
		"Stairs, U-turn Right",
		"Stairs, U-turn Right",
		"Stairs, U-turn Right",
		"Stairs, U-turn Right",
		"Stairs, U-turn Right",
		"Troop Station",
		"Light Pole"
	};
	return type >= 0 && type < int(sizeof(names) / sizeof(names[0]))
		? QString::fromLatin1(names[type]) : QStringLiteral("Unknown equipment (%1)").arg(type);
}

QString DetailPlacementDescription(int type)
{
	if(type <= 0 || type >= int(sizeof(Placements) / sizeof(Placements[0])))
		return QStringLiteral("This equipment type is unavailable.");
	const Placement placement = Placements[type];
	QString text;
	switch(placement.room)
	{
	case -1: text = QStringLiteral("Place inside a barracks, officer cabin, crew cabin or stateroom; the berth must fit."); break;
	case -2: text = QStringLiteral("Place on an exterior wall, away from room corners and fuel cells."); break;
	case -3: text = QStringLiteral("Place on a wall shared by exactly two rooms, away from room corners and fuel cells."); break;
	case -4: text = QStringLiteral("Place inside rooms on this deck and the deck below, away from walls and fuel cells."); break;
	case -5: text = QStringLiteral("Place inside exactly one room on this deck or the deck below, away from walls and fuel cells."); break;
	case -6: text = QStringLiteral("Place within the ship's overall bounds; replaces its existing pivot point."); break;
	case -7: text = QStringLiteral("Place inside or on a room; no duplicate at the same position, at most ten per ship."); break;
	case -8: text = QStringLiteral("This outdoor building equipment cannot be placed in a spacecraft."); break;
	case -9: text = QStringLiteral("The complete stair footprint must fit on this deck, outside fuel cells and roofs."); break;
	case -10: text = QStringLiteral("Place inside a room, away from walls and fuel cells."); break;
	default:
	{
		QString room;
		switch(placement.room)
		{
		case 2: room = QStringLiteral("bridge"); break;
		case 8: room = QStringLiteral("engine room"); break;
		case 13: room = QStringLiteral("vehicle bay"); break;
		case 14: room = QStringLiteral("lounge or galley"); break;
		case 16: room = QStringLiteral("sensor room"); break;
		case 17: room = QStringLiteral("shield room"); break;
		case 18: room = QStringLiteral("sick bay"); break;
		case 20: room = QStringLiteral("transporter room"); break;
		case 21: room = QStringLiteral("weapon room"); break;
		default: room = QStringLiteral("room type %1").arg(placement.room); break;
		}
		if(placement.bridge && placement.room != 2) room += QStringLiteral(" or bridge");
		text = QStringLiteral("Requires a %1.").arg(room);
		if(LimitOne(type)) text += QStringLiteral(" One station of this type per room.");
		if(type >= 57 && type <= 64) text += QStringLiteral(" The complete vehicle footprint must fit inside.");
		if(type == 99) text += QStringLiteral(" One emitter per Room Number, including other decks.");
		break;
	}
	}
	return text;
}

int CanonicalDetailType(int type)
{
	if(type >= 25 && type <= 31) return 2;
	if(type == 33) return 22;
	if(type >= 34 && type <= 40) return 21;
	if(type >= 57 && type <= 64) return 57;
	if(type >= 65 && type <= 72) return 65;
	if(type == 75) return 74;
	if(type >= 77 && type <= 83) return 8;
	if(type >= 84 && type <= 90) return 51;
	if(type >= 91 && type <= 97) return 52;
	if(IsStairs(type)) return 101 + ((type - 101) / 8) * 8;
	return type;
}

int DetailRotationStep(int type) { return RotationStep(type); }

QRectF BerthFootprint(int type)
{
	// Both 2016 RenderDesign passes anchor the berth at the center of its first cell.
	const double half = CoordinateSpacing / 2;
	return IsBerth(type) ? QRectF(-half, -half, 2 * CoordinateSpacing, CoordinateSpacing) : QRectF();
}

int RotateDetailType(int type, int steps)
{
	const int base = CanonicalDetailType(type);
	const int count = base == 74 ? 2 : 8;
	const int step = ((RotationStep(type) + steps % count) % count + count) % count;
	if(base == 2) return step ? 24 + step : base;
	if(base == 8) return step ? 76 + step : base;
	if(base == 21) return step ? 33 + step : base;
	if(base == 51) return step ? 83 + step : base;
	if(base == 52) return step ? 90 + step : base;
	if(base == 57 || base == 65 || base == 74 || IsStairs(base)) return base + step;
	return type;
}

DetailEditResult PlaceDetail(Design& design, int deck, const QPointF& cursor,
	int type, quint16 commodity, int brushRadius)
{
	DetailEditResult result;
	quint16 packed = 0;
	if(!ValidEditInput(deck, cursor, brushRadius, packed, result.error)) return result;
	if(type <= 0 || type >= int(sizeof(Placements) / sizeof(Placements[0])))
	{
		result.error = QStringLiteral("The selected equipment type is unavailable.");
		return result;
	}
	// IsValid(..., spacecraft=true) cannot accept outdoor-building placement.
	// Check before the same-type shortcut, including imported pole records.
	if(design.building && (!AuLegacyBd::BuildingDetailAllowed(design.buildingIndustry, type)
		|| !AuLegacyBd::ValidDesignerPoint(design.buildingIndustry, deck, cursor)))
	{
		result.error = QStringLiteral("This equipment or position is unavailable for the selected industry.");
		return result;
	}
	if(!design.building && Placements[type].room == -8)
	{
		result.error = DetailPlacementDescription(type);
		return result;
	}
	QVector<Geometry> rooms;
	if(!CollectRooms(design, rooms, result.error)) return result;
	QVector<Detail> details = design.details;
	Detail center;
	int removed = 0;
	const PlacementResult placed = PlaceOne(details, rooms, design, deck,
		packed, type, commodity, center, removed, result.error);
	if(placed == PlacementResult::Rejected || placed == PlacementResult::Unavailable) return result;
	result.accepted = true;
	result.detailId = center.detailId;
	if(placed == PlacementResult::Unchanged) return result;
	result.added = 1;
	result.removed = removed;
	// The original special marker tools do not use the equipment brush.
	if(brushRadius && type != 76 && type != 98 && type != 99 && type != 142 && center.roomId)
	{
		const Geometry* room = nullptr;
		for(const Geometry& candidate : rooms)
			if(candidate.room->roomId == center.roomId) { room = &candidate; break; }
		if(room)
		{
			const bool boundaryAllowed = type < 24 && ((0x854400U >> type) & 1) != 0;
			for(int x = -brushRadius; x <= brushRadius; ++x)
				for(int y = -brushRadius; y <= brushRadius; ++y)
				{
					if(x == 0 && y == 0) continue;
					// The brush loop multiplies its offsets in single precision,
					// then rounds each resulting location back to the legacy grid.
					const QPointF location = cursor + QPointF(double(float(x) * float(CoordinateSpacing)),
						double(float(y) * float(CoordinateSpacing)));
					quint16 brushPoint = 0;
					if(!SnapDetail(location, brushPoint)) continue;
					const QPointF snapped = DecodePoint(brushPoint);
					if(design.building && !AuLegacyBd::ValidDesignerPoint(design.buildingIndustry, deck, location))
						continue;
					if(!Inside(room->polygon, snapped) && !(boundaryAllowed && On(room->polygon, snapped))) continue;
					Detail next;
					QString error;
					const PlacementResult applied = PlaceOne(details, rooms, design,
						deck, brushPoint, type, commodity, next, removed, error);
					if(applied == PlacementResult::Unavailable)
					{
						DetailEditResult failed;
						failed.error = error;
						return failed;
					}
					if(applied == PlacementResult::Added) { ++result.added; result.removed += removed; }
				}
		}
	}
	design.details = std::move(details);
	if(result.removed) TrimMissingModuleSystems(design);
	result.changed = true;
	return result;
}

DetailEditResult DeleteDetails(Design& design, int deck, const QPointF& cursor, int brushRadius)
{
	DetailEditResult result;
	quint16 packed = 0;
	if(!ValidEditInput(deck, cursor, brushRadius, packed, result.error)) return result;
	result.accepted = true;
	QVector<Detail> details = design.details;
	const int type = DeleteOne(details, deck, packed, 0);
	if(type < 0) return result;
	result.removed = 1;
	// The original still deletes a type-zero source record, but its zero
	// return value prevents the caller from expanding the erase brush.
	for(int x = -brushRadius; type != 0 && x <= brushRadius; ++x)
		for(int y = -brushRadius; y <= brushRadius; ++y)
		{
			if(x == 0 && y == 0) continue;
			const QPointF location = cursor + QPointF(double(float(x) * float(CoordinateSpacing)),
				double(float(y) * float(CoordinateSpacing)));
			quint16 brushPoint = 0;
			if(SnapDetail(location, brushPoint) && DeleteOne(details, deck, brushPoint, type) >= 0) ++result.removed;
		}
	design.details = std::move(details);
	if(type == 20 || type == 23 || type == 99) TrimMissingModuleSystems(design);
	result.changed = true;
	return result;
}

bool ReconcileDetails(Design& candidate, QString& error)
{
	error.clear();
	QVector<Geometry> rooms;
	rooms.reserve(candidate.rooms.size());
	for(const Room& r:candidate.rooms)
	{
		QString geometryError;
		const QPolygonF polygon=RoomBoundary(r,&geometryError);
		if(!geometryError.isEmpty()) {error=geometryError;return false;}
		// The 2016 room-array reader discards empty room records. Retained
		// source records must not participate even in room-ID/subtype lookups.
		if(polygon.isEmpty()) continue;
		rooms.append({&r,polygon});
	}
	QVector<Detail> details=candidate.details;
	for(int i=0;i<details.size();)
	{
		Detail corrected=details[i];
		switch(TestDetail(corrected,rooms,details,candidate.building,error))
		{
		case DetailResult::Unavailable: return false;
		case DetailResult::Invalid: details.removeAt(i); break;
		case DetailResult::Valid: details[i]=corrected; ++i; break;
		}
	}
	candidate.details=std::move(details);
	TrimMissingModuleSystems(candidate);
	return true;
}
}
