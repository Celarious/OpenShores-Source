#ifndef FloraGen_H
#define FloraGen_H

#include "deflora.h"
#include <QColor>
#include <QImage>
#include <QList>
#include <QVector3D>

namespace FloraGen
{

enum PLAN
{
	PLAN_Broadleaf,
	PLAN_Conifer,
	PLAN_Palm,
	PLAN_Weeping,
	PLAN_Umbrella,
	PLAN_Bottle,
	PLAN_Fungal,
	PLAN_Spiral,
	PLAN_Crystal,
	PLAN_Lantern,
	PLAN_Count
};

const char* PlanName(int plan);
bool PlanIsAlien(int plan);

class Rng
{
private:
	quint64								m_State;

public:
	explicit Rng(quint64 seed);
	quint32 Next();
	float Unit();
	float Range(float lo, float hi);
	int Below(int n);
	bool Chance(float p);
	float Signed();
};

quint64 Hash(const void* data, int bytes, quint64 seed);
quint64 Mix(quint64 a, quint64 b);

class World
{
public:
	float								m_Strangeness;
	float								m_LeafHue;
	float								m_LeafHueSpread;
	float								m_LeafSat;
	float								m_LeafVal;
	QColor								m_BarkTint;
	int									m_AlienTheme[2];
	QColor								m_MaterialColor[2];

public:
	World();
	static World Derive(quint32 genSeed, float strangeness=-1.f);
};

class Traits
{
public:
	int									m_Plan;
	float								m_LeafHue;
	float								m_LeafSat;
	float								m_LeafVal;
	float								m_Autumn;
	QColor								m_BarkTint;
	QColor								m_Glow;
	float								m_Gravitropism;
	float								m_Twist;
	float								m_Lean;
	float								m_Flare;
	int									m_Buttresses;
	float								m_Swell;
	float								m_CardScale;
	int									m_CardsPerCluster;
	int									m_Whorl;
	float								m_CapSize;
	int									m_Foliage;
	int									m_SpecialSide;
	int									m_Type;
	int									m_Zone;
	int									m_Slot;
	float								m_Hue2;
	QColor								m_FruitColor;
	float								m_Coverage;

public:
	Traits();
	static Traits Derive(const AuFloraDNA& dna, const World& world, int zone, bool tree, int index, int forceType=-1,
			int forceFoliage=-1);
};

enum FOLIAGE
{
	FOLIAGE_Game,
	FOLIAGE_Needles,
	FOLIAGE_Frond,
	FOLIAGE_Plume,
	FOLIAGE_Tendrils,
	FOLIAGE_Fins,
	FOLIAGE_Shards,
	FOLIAGE_Count
};

class Tile
{
public:
	float s, t, u, v;
	Tile() : s(0), t(0), u(0), v(0) {}
	Tile(float s0, float t0, float u0, float v0) : s(s0), t(t0), u(u0), v(v0) {}
	static Tile Palette(unsigned int index, int tileSize);
	Tile Band(float top, float bottom) const;
};

enum
{
	FG_RingedTile = 54,
	FG_SpecialTile = 55
};

class Budget
{
public:
	int									m_MaxSegments;
	int									m_MaxCards;
	int									m_SidesMax;
	int									m_RingsPerSegment;
	float								m_Detail = 1.f;
	static Budget ForLod(int lod);

	enum { TreeTriangleCap = 10000, ShrubTriangleCap = 5000 };
	static float MinDetail() { return .4f; }
};

void BuildTree(QList<AuFloraVAOTri>& tris, const AuFloraDNA& dna, const Traits& traits, float textureSize, const Tile& leaf,
		const Tile& bark, const Tile& ringed, const Tile& special, quint32 seed, int lod, float height=0.f,
		float maxRadius=0.f);
void BuildShrub(QList<AuFloraVAOTri>& tris, const AuFloraDNA& dna, const Traits& traits, float textureSize, const Tile& leaf,
		const Tile& bark, const Tile& ringed, const Tile& special, quint32 seed, int lod, float height=0.f,
		float maxRadius=0.f);

void RecolourFoliage(QImage& tile, const Traits& traits, quint32 seed);
void PaintNeedles(QImage& tile, const Traits& traits, quint32 seed);
void PaintFrond(QImage& tile, const Traits& traits, quint32 seed);
void PaintAlienFoliage(QImage& tile, const Traits& traits, quint32 seed);
void PaintFoliage(QImage& tile, const Traits& traits, quint32 seed);
void PaintBark(QImage& tile, const QImage& barkStrip, int index, const World& world, quint32 genSeed);
void PaintRinged(QImage& tile, const World& world, quint32 genSeed);
void PaintSpecial(QImage& tile, const World& world, quint32 genSeed);
void Dilate(QImage& image, int passes);
float Coverage(const QImage& tile);

}

#endif
