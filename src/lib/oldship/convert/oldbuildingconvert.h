#ifndef OldBuildingConvert_H
#define OldBuildingConvert_H

#include "oldshipconvert.h"
#include "oldbuilding.h"

class AuBdDesign;
class AuBdReport;

namespace OldShip
{

enum BuildingMaterial
{
	BM_Wood = 86,
	BM_Lumber = 48,
	BM_Stone = 76,
	BM_Metal = 51,
	BM_Magmium = 146,
	BM_Vulcium = 145,
};

struct BuildingInfo : public ConvertInfo
{
	int			industry;
	QString		industryName;
	QString		industryFault;
	int			material;
	AuPoint		siteLo, siteHi;
	double		lift;
	int			roofs, flagPoles, lightPoles, flags;
	int			groundHoles;
	int			roofTriangles, poleTriangles;
	int			windows, windowTriangles;

	BuildingInfo() : industry(0), material(BM_Stone), lift(0), roofs(0), flagPoles(0), lightPoles(0), flags(0), groundHoles(0),
			roofTriangles(0), poleTriangles(0), windows(0), windowTriangles(0) {}
};

struct BuildingOptions
{
	int		material;
	bool	keepOrigin;
	double	groundLift;
	bool	litWindows;

	BuildingOptions() : material(BM_Stone), keepOrigin(false), groundLift(0.0), litWindows(true) {}
};

bool ConvertBuilding(const OldBuilding::Design& building, AuBdDesign& out, QStringList& log, BuildingInfo* info = 0,
		const BuildingOptions* options = 0);

bool AnalyzeConvertedBuilding(AuBdDesign& design, AuBdReport& report, QStringList& msgs);

int BuildingAtlas(int material);
QString BuildingMaterialName(int material);

void CheckBuildingIndustry(const AuBdDesign& design, BuildingInfo& info, QStringList& log);

QList<Problem> BuildingProblems(const OldBuilding::Design& building, const BuildingInfo& info);

QString BuildingReportHtml(const OldBuilding::Design& building, const BuildingInfo& info, const QStringList& log, bool converted,
		const AuBdReport* game, const QStringList& gameMsgs, bool stale = false, const QList<Problem>* problems = 0);

}

#endif
