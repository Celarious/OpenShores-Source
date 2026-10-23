#ifndef FloraTypes_H
#define FloraTypes_H

#include "floragen.h"
#include <QPair>
#include <QRect>
#include <QString>
#include <QVector>

namespace FloraGen
{

enum FAMILY
{
	FAM_Earth,
	FAM_Fungus,
	FAM_Crystal,
	FAM_Spiral,
	FAM_Glow,
	FAM_Marine,
	FAM_Flesh,
	FAM_Air,
	FAM_Geometric,
	FAM_Count
};
const char* FamilyName(int family);

enum BUILDER
{
	B_Arch,
	B_Palm,
	B_Cactus,
	B_Bamboo,
	B_Fungus,
	B_Crystal,
	B_Shelf,
	B_Pagoda,
	B_Anemone,
	B_Kelp,
	B_Stalks,
	B_Helix,
	B_Membrane,
	B_Tubes,
	B_Count
};

enum { Z_Polar = 1, Z_Temperate = 2, Z_Tropical = 4, Z_All = 7 };
enum { F_Tree = 1, F_Shrub = 2, F_Both = 3 };

enum
{
	X_Stilts		= 0x0001,
	X_Aerial		= 0x0002,
	X_Fruit			= 0x0004,
	X_Pods			= 0x0008,
	X_Buttress		= 0x0010,
	X_Swell			= 0x0020,
	X_Bare			= 0x0040,
	X_Wind			= 0x0080,
	X_Faceted		= 0x0100,
	X_Glow			= 0x0200,
	X_Gnarl			= 0x0400,
	X_Bigpods		= 0x0800,
	X_Leaning		= 0x1000,
	X_Curl			= 0x2000
};

enum
{
	TILE_Ringed			= 64,
	TILE_Succulent,
	TILE_Birch,
	TILE_Peeling,
	TILE_RedFibre,
	TILE_Grey,
	TILE_Rainbow,
	TILE_Bone,
	TILE_Coral,
	TILE_Flesh,
	TILE_Obsidian,
	TILE_Special,
	TILE_Swatch0,
	TILE_Swatch1,
	TILE_Swatch2,
	TILE_Pale,
	TILE_Bark0
};

Tile MaterialRect(int material, int tileSize);
QRect MaterialPixels(int material, int tileSize);

enum
{
	FOLIAGE_Round = FOLIAGE_Count,
	FOLIAGE_Palmate,
	FOLIAGE_Narrow,
	FOLIAGE_Blossom,
	FOLIAGE_Compound,
	FOLIAGE_Scale,
	FOLIAGE_Broad,
	FOLIAGE_Fan,
	FOLIAGE_Blade,
	FOLIAGE_Glossy,
	FOLIAGE_Bubbles,
	FOLIAGE_AllCount
};

enum
{
	LEAF_Rosette = 9,
	LEAF_Orb = 10,
	LEAF_PlumeBall = 11,
	LEAF_Puff = 12,
	LEAF_Ribbon = 13
};

class TreeType
{
public:
	QString								m_Name;
	quint8								m_Family;
	quint8								m_Zones;
	quint8								m_Forms;
	quint8								m_Weight;
	quint8								m_Builder;
	quint8								m_Plan;
	qint8								m_Foliage;
	quint8								m_LeafKind;
	qint16								m_Material;
	float								m_TrunkFrac;
	float								m_Spread;
	float								m_Ratio;
	float								m_Grav[3];
	float								m_Twist;
	float								m_RadiusMul;
	float								m_LeafSize;
	float								m_LeafSpacing;
	qint8								m_Levels;
	qint8								m_Limbs;
	bool								m_Excurrent;
	qint8								m_Whorl;
	qint8								m_Stems;
	quint32								m_Extras;
	float								m_Hue;
	float								m_Hue2;
	float								m_P1, m_P2;

public:
	explicit TreeType(const QString& name = QString());
	TreeType& fam(int f) { m_Family = f; return *this; }
	TreeType& z(int zones) { m_Zones = zones; return *this; }
	TreeType& w(int weight) { m_Weight = weight; return *this; }
	TreeType& forms(int f) { m_Forms = f; return *this; }
	TreeType& b(int builder) { m_Builder = builder; return *this; }
	TreeType& plan(int p) { m_Plan = p; return *this; }
	TreeType& fol(int f) { m_Foliage = f; return *this; }
	TreeType& lk(int k) { m_LeafKind = k; return *this; }
	TreeType& mat(int m) { m_Material = m; return *this; }
	TreeType& shape(float trunkFrac, float spread, float ratio) { m_TrunkFrac = trunkFrac; m_Spread = spread; m_Ratio = ratio; return *this; }
	TreeType& grav(float g1, float g2, float g3) { m_Grav[0] = g1; m_Grav[1] = g2; m_Grav[2] = g3; return *this; }
	TreeType& tw(float t) { m_Twist = t; return *this; }
	TreeType& rad(float r) { m_RadiusMul = r; return *this; }
	TreeType& leaf(float size, float spacing) { m_LeafSize = size; m_LeafSpacing = spacing; return *this; }
	TreeType& lv(int levels) { m_Levels = levels; return *this; }
	TreeType& limbs(int n) { m_Limbs = n; return *this; }
	TreeType& exc() { m_Excurrent = true; return *this; }
	TreeType& whorl(int n) { m_Whorl = n; return *this; }
	TreeType& stems(int n) { m_Stems = n; return *this; }
	TreeType& x(quint32 extras) { m_Extras |= extras; return *this; }
	TreeType& hue(float h, float h2 = -1.f) { m_Hue = h; m_Hue2 = h2; return *this; }
	TreeType& p(float p1, float p2 = 0.f) { m_P1 = p1; m_P2 = p2; return *this; }
};

const QVector<TreeType>& Types();
int TypeIndex(const QString& name);

int PickType(Rng& r, const AuFloraDNA& dna, const World& world, int zone, bool tree);

void PaintMaterials(QImage& atlas, int tileSize, const World& world, quint32 genSeed);
void PaintZone(QImage& atlas, const DePlanetFlora& flora, quint32 genSeed, int resourceZone, int zone, int tile,
		const World* world, int forceType = -1, int forceFoliage = -1, QVector<QPair<int, QColor>>* swatches = 0);
void PaintPalette(QImage& atlas, const DePlanetFlora& flora, quint32 genSeed, int resourceZone, int tile, const World& world,
		float coverage[3][18], int forceType = -1, int forceFoliage = -1);
void PaintSwatch(QImage& atlas, int tileSize, int zone, int slot, const QColor& color, quint32 seed);
void PaintTypeFoliage(QImage& tile, const Traits& traits, quint32 seed);

void BuildPlant(QList<AuFloraVAOTri>& tris, const AuFloraDNA& dna, const Traits& traits, float textureSize, int tileSize,
		bool tree, quint32 seed, int lod, float height, float maxRadius=0.f);

}

#endif
