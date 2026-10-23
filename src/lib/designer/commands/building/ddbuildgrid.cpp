#include "ddbuildgrid.h"
//
#include "auclientglobal.h"
#include "auelapsedtimer.h"
#include "augleffects.h"
#include "auglobell.h"
#include "auglsl.h"
#include "daworldll.h"
#include <cmath>

namespace BuildGrid
{

namespace
{

const double CosStep = 0.92;
const double PoleRad = 80. * PI / 180.;
const double Lift = 1.0;
const double DrawReach = 150. * 3.2808399;
const int MinCells = 4, MaxCells = 30;
const double Bend = 4. * 3.2808399;

void Metric(const DaWorldLL* world, double lon, double lat, double& north, double& east)
{
	const double e = 1e-6;
	north = world->LocOfLL(AuGlobeLL(lon, lat + e)).Distance(world->LocOfLL(AuGlobeLL(lon, lat - e))) / (2 * e);
	east = world->LocOfLL(AuGlobeLL(lon + e, lat)).Distance(world->LocOfLL(AuGlobeLL(lon - e, lat))) / (2 * e);
}

struct Grid
{
	const DaWorldLL*	world;
	double				spacing;
	double				rowRad;
	bool				globe;

	bool Init(const DaWorldLL* w)
	{
		world = w;
		spacing = g_AuClient.BuildGridSizeInFeet();
		globe = w->LLShape() == LL_Globe;
		double north, east;
		Metric(w, 0, 0, north, east);
		if(!(north > 0) || !(spacing > 0))
			return false;
		rowRad = spacing / north;
		return true;
	}

	bool Covers(double lat) const
	{
		return !globe || fabs(lat) <= PoleRad;
	}

	int Row(double lat) const
	{
		return (int)lround(lat / rowRad);
	}

	int BandStart(int k) const
	{
		return (int)lround(acos(pow(CosStep, k)) / rowRad);
	}

	int Band(int row) const
	{
		if(!globe)
			return 0;
		const int a = row >= 0 ? row : -row;
		const double lat = qMin(a * rowRad, PoleRad);
		int k = qMax(0, (int)floor(log(cos(lat)) / log(CosStep)));
		while(k > 0 && a < BandStart(k))
			k--;
		while(a >= BandStart(k + 1))
			k++;
		return row >= 0 ? k : -k;
	}

	double ColRad(int band) const
	{
		double mid = 0;
		if(globe)
			mid = qMin(acos(pow(CosStep, (band >= 0 ? band : -band) + .5)), PoleRad);
		double north, east;
		Metric(world, 0, mid, north, east);
		return east > 0 ? spacing / east : 0;
	}
};

struct Drawn
{
	AuId				worldId;
	int					row = 0, col = 0;
	double				spacing = 0;
	AuElapsedTimer		made;
	AuPoint				origin;
	QList<AuPointAlpha>	verts[8];
};
Drawn s_Drawn;

}

bool Enabled()
{
	return g_AuClient.BuildGrid();
}

bool Snap(const DaWorldLL* world, AuPoint& loc)
{
	if(!world)
		return false;
	Grid g;
	if(!g.Init(world))
		return false;
	AuGlobeLL ll;
	if(!world->LLOfLoc(ll, loc) || !g.Covers(ll.Latitude()))
		return false;
	const int row = g.Row(ll.Latitude());
	const double colRad = g.ColRad(g.Band(row));
	if(!(colRad > 0))
		return false;
	const double lon = lround(ll.Longitude() / colRad) * colRad;
	loc = world->GetTerrainLocation(AuGlobeLL(lon, row * g.rowRad));
	return true;
}

double NorthRotation(const DaWorldLL* world, const AuPoint& loc)
{
	AuGlobeLL ll;
	if(!world || !world->LLOfLoc(ll, loc))
		return 0;
	const double e = 1e-6;
	const AuPoint north = world->LocOfLL(AuGlobeLL(ll.Longitude(), ll.Latitude() + e))
			- world->LocOfLL(AuGlobeLL(ll.Longitude(), ll.Latitude() - e));
	const AuPoint vUp = -world->GravityDir(loc);
	const AuMatrix4x4 mAlign = AuPoint(0,0,1).GetAlignmentTransform(vUp);
	const AuPoint n = mAlign.UnProjectVector(north);
	return atan2(n.y, n.x) - PI / 2;
}

void Render(AuGLEffects& re, const AuMatrix4x4& projectionModel, const DaWorldLL* world, const AuPoint& loc)
{
	if(!world)
		return;
	Grid g;
	if(!g.Init(world))
		return;
	AuGlobeLL ll;
	if(!world->LLOfLoc(ll, loc) || !g.Covers(ll.Latitude()))
		return;
	const int row0 = g.Row(ll.Latitude());
	const double colRad0 = g.ColRad(g.Band(row0));
	if(!(colRad0 > 0))
		return;
	const int col0 = (int)lround(ll.Longitude() / colRad0);

	if(s_Drawn.worldId != world->Id() || s_Drawn.row != row0 || s_Drawn.col != col0 || s_Drawn.spacing != g.spacing
			|| !s_Drawn.made.isValid() || s_Drawn.made.elapsed() > 2000)
	{
		s_Drawn.worldId = world->Id();
		s_Drawn.row = row0;
		s_Drawn.col = col0;
		s_Drawn.spacing = g.spacing;
		s_Drawn.made.restart();
		for(int b = 0; b < 8; b++)
			s_Drawn.verts[b].clear();
		const int cells = qBound(MinCells, (int)ceil(DrawReach / g.spacing), MaxCells);
		const int pieces = qMax(1, (int)ceil(g.spacing / Bend));
		const double lon0 = col0 * colRad0;
		s_Drawn.origin = world->GetTerrainLocation(AuGlobeLL(lon0, row0 * g.rowRad));
		auto point = [&](double lon, double lat) -> AuPointAlpha
		{
			const AuPoint p = world->GetTerrainLocation(AuGlobeLL(lon, lat));
			return AuPointAlpha(p - world->GravityDir(p) * Lift - s_Drawn.origin);
		};
		auto fade = [&](double dRow, double dCol) -> float
		{
			const double d = sqrt(dRow * dRow + dCol * dCol) / cells;
			return (float)qMax(0., 1. - d);
		};
		auto line = [&](double lonA, double latA, double lonB, double latB, double dRowA, double dColA, double dRowB,
				double dColB)
		{
			for(int k = 0; k < pieces; k++)
			{
				const double f0 = (double)k / pieces, f1 = (double)(k + 1) / pieces;
				const float a0 = fade(dRowA + (dRowB - dRowA) * f0, dColA + (dColB - dColA) * f0);
				const float a1 = fade(dRowA + (dRowB - dRowA) * f1, dColA + (dColB - dColA) * f1);
				const float a = qMax(a0, a1);
				if(a <= 0)
					continue;
				s_Drawn.verts[qBound(0, (int)((1.f - a) * 8), 7)] << point(lonA + (lonB - lonA) * f0, latA + (latB - latA) * f0)
						<< point(lonA + (lonB - lonA) * f1, latA + (latB - latA) * f1);
			}
		};
		for(int dr = -cells; dr <= cells; dr++)
		{
			const int row = row0 + dr;
			const double lat = row * g.rowRad;
			if(!g.Covers(lat))
				continue;
			const int band = g.Band(row);
			const double colRad = band == g.Band(row0) ? colRad0 : g.ColRad(band);
			if(!(colRad > 0))
				continue;
			const int colMid = (int)lround(lon0 / colRad);
			const bool nextSameBand = dr < cells && g.Band(row + 1) == band && g.Covers((row + 1) * g.rowRad);
			for(int dc = -cells; dc <= cells; dc++)
			{
				const double lon = (colMid + dc) * colRad;
				const double dCol = (lon - lon0) / colRad;
				if(dc < cells)
					line(lon, lat, lon + colRad, lat, dr, dCol, dr, dCol + 1);

				if(nextSameBand)
					line(lon, lat, lon, lat + g.rowRad, dr, dCol, dr + 1, dCol);
			}
		}
	}
	re.GLSL().LineShader().Select();
	AuMatrix4x4 toOrigin;
	toOrigin.Translate(s_Drawn.origin);
	re.GLSL().LineShader().SetProjectionModelView(projectionModel * toOrigin);
	re.glDisable(GL_DEPTH_TEST);
	re.glEnable(GL_BLEND);
	re.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	for(int b = 0; b < 8; b++)
	{
		if(s_Drawn.verts[b].isEmpty())
			continue;
		AuGLVAO vao;
		vao.CreateForSprites(re, s_Drawn.verts[b], GL_STREAM_DRAW);
		if(!vao.VAO())
			continue;
		re.GLSL().LineShader().SetColor(QColor(140, 210, 255, 230 - b * 27));
		vao.SelectGL();
		vao.DrawGL(GL_LINES);
		vao.DeselectGL();
		vao.DeleteGL();
	}
}

}
