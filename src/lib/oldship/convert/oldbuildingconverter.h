#ifndef OldBuildingConverter_H
#define OldBuildingConverter_H

#include "oldbuildingconvert.h"
#include "oldshipconverter.h"
#include "aubddesign.h"
#include "dbindustry.h"

namespace OldShip
{

struct BuildingModel
{
	const OldBuilding::Design&	m_Building;
	int							m_Lo, m_Hi;
	OldBuilding::Design			m_Drawn;
	OldBuilding::Builder		m_BdBuilder;
	Design						m_AsShip;

	BuildingModel(const OldBuilding::Design& building, int material);
};

class BuildingConverter : private BuildingModel, public Converter
{
public:
	BuildingConverter(const OldBuilding::Design& building, AuBdDesign& design, QStringList& log, BuildingInfo& info,
			const BuildingOptions& options);

protected:
	virtual void RenderModels();
	virtual void SetProperties();
	virtual void BuildSurroundings();
	virtual QString RoomTypeName(int type) const;
	virtual void VoidProperties(AuScRoomVoid& v, const RoomData& rd, bool doorway) const;
	virtual double HullOpeningReach(int roomType) const;
	virtual quint64 MemoryLimit() const;
	virtual void GiveBayVolume(quint16 part, const QString& where);
	virtual double WallSolidDepth() const;
	virtual double HullVoidDrop(int deck) const;
	virtual bool DoorsAreOpenings() const { return true; }
	virtual double DoorwayReach(double reach) const;
	virtual double HullVoidFrom(double reach) const;
	virtual void ReportTurrets();
	virtual void ReportTheRest();
	virtual void BuildStations();
	void BuildParkingSpots();
	void BuildBasins();
	void BuildIndustryNeeds();
	void BuildingBounds(double& x0, double& x1, double& y0, double& y1) const;

public:
	virtual double RoofSolidRise() const;
	virtual void Decorate(AuScFace& face, const Tri& t) const;
	virtual void DoorPicture(AuScFaceList& faces, const QVector<Tri>& tris, int picture) const;
	virtual bool RenderFitting(TriList& out, const Detail& d) const;

private:
	AuBdDesign& Bd() { return static_cast<AuBdDesign&>(m_Design); }
	static Tri AsTri(const OldBuilding::BTri& b);
	bool AddNew(AuScPart& part, const QString& what);
	void BuildRoofs();
	void BuildWindows();
	void BuildPoles();
	void BuildSite();
	void BuildLivestockPosts();
	void BuildTurretPlugs();
	void BuildThresholds();
	void BuildOpenGround();
	void AddLevelSlab(AuScFaceList& faces, const P2& mid, const P2& u, const P2& n, double from, double to, double half,
			double zLow, double zHigh, bool up) const;
	void SetIndustry();
	bool Allowed(qint8 rule) const { return !m_HasIndustry || rule >= 0; }

	BuildingInfo&		m_BdInfo;
	BuildingOptions		m_BdOptions;
	QVector<Tri>		m_Roofs;
	QVector<int>		m_RoofSizes;
	QVector<Tri>		m_Poles;
	bool				m_HasIndustry;
	DbIndustry			m_Industry;
	int					m_SensorStations;
	QVector<P2>			m_SiteExtra;
	QVector<QPair<P2, P2> >	m_Basins;
};

}

#endif
