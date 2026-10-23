#include "floragen.h"
#include "floragenbuild.h"
#include "floratypes.h"
#include <QPainter>
#include <QPainterPath>
#include <QQuaternion>
#include <QtMath>
#include <cmath>

namespace FloraGen
{

const char* PlanName(int plan)
{
	switch(plan)
	{
	case PLAN_Broadleaf:	return "Broadleaf";
	case PLAN_Conifer:		return "Conifer";
	case PLAN_Palm:			return "Palm";
	case PLAN_Weeping:		return "Weeping";
	case PLAN_Umbrella:		return "Umbrella";
	case PLAN_Bottle:		return "Bottle";
	case PLAN_Fungal:		return "Giant fungus";
	case PLAN_Spiral:		return "Spiral";
	case PLAN_Crystal:		return "Crystal";
	case PLAN_Lantern:		return "Lantern";
	}
	return "?";
}

bool PlanIsAlien(int plan)
{
	return plan >= PLAN_Fungal;
}

quint64 Mix(quint64 a, quint64 b)
{
	quint64 z = a + 0x9E3779B97F4A7C15ULL + (b << 6) + (b >> 2);
	z ^= b * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	return z ^ (z >> 31);
}

quint64 Hash(const void* data, int bytes, quint64 seed)
{
	quint64 h = 0xCBF29CE484222325ULL ^ seed;
	const unsigned char* p = (const unsigned char*)data;
	for(int i=0; i<bytes; i++)
	{
		h ^= p[i];
		h *= 0x100000001B3ULL;
	}
	return Mix(h, seed);
}

Rng::Rng(quint64 seed)
{
	m_State = Mix(seed, 0x6A09E667F3BCC909ULL);
}

quint32 Rng::Next()
{
	quint64 old = m_State;
	m_State = old * 6364136223846793005ULL + 1442695040888963407ULL;
	quint32 xorshifted = (quint32)(((old >> 18) ^ old) >> 27);
	quint32 rot = (quint32)(old >> 59);
	return (xorshifted >> rot) | (xorshifted << ((32 - rot) & 31));
}

float Rng::Unit()
{
	return (float)(Next() >> 8) * (1.f / 16777216.f);
}

float Rng::Range(float lo, float hi)
{
	return lo + (hi - lo) * Unit();
}

int Rng::Below(int n)
{
	if(n <= 1)
		return 0;
	return (int)(Unit() * n) % n;
}

bool Rng::Chance(float p)
{
	return Unit() < p;
}

float Rng::Signed()
{
	return Unit() * 2.f - 1.f;
}

quint64 HashDNA(const AuFloraDNA& d, quint64 seed)
{
	const unsigned char f[] =
	{
		(unsigned char)d.Inflorescence, (unsigned char)d.TrunkLength, (unsigned char)d.Multiples,
		(unsigned char)d.SproutMultiples, (unsigned char)d.BranchAngle, (unsigned char)d.BranchProbability,
		(unsigned char)d.RootType, (unsigned char)d.RootSize, (unsigned char)d.LeafProbability, (unsigned char)d.LeafType,
		(unsigned char)d.LeafSize, (unsigned char)d.LeafVeinCount, (unsigned char)d.LeafVeinWidth,
		(unsigned char)d.Chemical, (unsigned char)d.ChemicalStrength, (unsigned char)d.InternodeSizeRatio,
		(unsigned char)d.BarkTexture, (unsigned char)d.PetalMultiples, (unsigned char)d.PetalLength,
		(unsigned char)d.PetalTaper, (unsigned char)d.PetalWidth, (unsigned char)d.FlowerProbability,
		(unsigned char)d.FlowerPetalRed, (unsigned char)d.FlowerPetalGreen, (unsigned char)d.FlowerPetalBlue,
		(unsigned char)d.FlowerOvarySize, (unsigned char)d.FlowerOvaryRed, (unsigned char)d.FlowerOvaryGreen,
		(unsigned char)d.FlowerOvaryBlue, (unsigned char)d.LeafStemLength, (unsigned char)d.FlowerOvary,
		(unsigned char)d.ThornSize, (unsigned char)d.ThornFreq, (unsigned char)d.Stiffness
	};
	return Hash(f, sizeof(f), seed);
}

World::World()
{
	m_Strangeness = 0;
	m_LeafHue = 100;
	m_LeafHueSpread = 14;
	m_LeafSat = 1;
	m_LeafVal = 1;
	m_BarkTint = QColor(255, 255, 255);
	m_AlienTheme[0] = -1;
	m_AlienTheme[1] = -1;
	m_MaterialColor[0] = QColor(200, 190, 170);
	m_MaterialColor[1] = QColor(120, 160, 200);
}

World World::Derive(quint32 genSeed, float strangeness)
{
	World w;
	Rng r(Mix(genSeed, 0xF10A));

	float u = r.Unit();
	if(u < .55f)
		w.m_Strangeness = u / .55f * .25f;
	else if(u < .85f)
		w.m_Strangeness = .25f + (u - .55f) / .30f * .35f;
	else
		w.m_Strangeness = .60f + (u - .85f) / .15f * .40f;
	if(strangeness >= 0.f)
		w.m_Strangeness = qBound(0.f, strangeness, 1.f);
	const float s = w.m_Strangeness;

	static const float alienHues[] = { 0, 18, 45, 170, 195, 225, 265, 290, 320, 340 };
	const float greenHue = 92.f + r.Signed() * 18.f;
	if(r.Chance(qBound(0.f, (s - .2f) * 1.4f, 1.f)))
	{
		w.m_LeafHue = alienHues[r.Below(sizeof(alienHues)/sizeof(alienHues[0]))] + r.Signed() * 10.f;
		w.m_LeafHueSpread = 10.f + s * 40.f;
		w.m_LeafSat = r.Range(.75f, 1.25f);
		w.m_LeafVal = r.Range(.8f, 1.15f);
		if(r.Chance(s * .15f))
			w.m_LeafSat = r.Range(.08f, .25f);
		else if(r.Chance(s * .12f))
			w.m_LeafVal = r.Range(.35f, .55f);
	}
	else
	{
		w.m_LeafHue = greenHue;
		w.m_LeafHueSpread = 8.f + s * 30.f;
		w.m_LeafSat = r.Range(.85f, 1.15f);
		w.m_LeafVal = r.Range(.9f, 1.1f);
	}

	const float barkHue = r.Range(0.f, 360.f);
	const float barkSat = qBound(0.f, (s - .25f) * .6f, .45f) * r.Unit();
	QColor bark;
	bark.setHsvF(fmodf(barkHue, 360.f) / 360.f, barkSat, r.Range(.85f, 1.f));
	w.m_BarkTint = bark;

	if(s >= .3f)
	{
		w.m_AlienTheme[0] = FAM_Fungus + r.Below(FAM_Count - FAM_Fungus);
		if(s >= .6f)
		{
			int second = FAM_Fungus + r.Below(FAM_Count - FAM_Fungus - 1);
			if(second >= w.m_AlienTheme[0])
				second++;
			w.m_AlienTheme[1] = second;
		}
	}

	for(int i=0; i<2; i++)
	{
		QColor c;
		c.setHsvF(r.Unit(), r.Range(.25f, .8f), r.Range(.55f, .95f));
		w.m_MaterialColor[i] = c;
	}
	return w;
}

Traits::Traits()
{
	m_Plan = PLAN_Broadleaf;
	m_LeafHue = 100;
	m_LeafSat = 1;
	m_LeafVal = 1;
	m_Autumn = 0;
	m_BarkTint = QColor(255, 255, 255);
	m_Glow = QColor(0, 0, 0);
	m_Gravitropism = .1f;
	m_Twist = 0;
	m_Lean = 0;
	m_Flare = .4f;
	m_Buttresses = 0;
	m_Swell = 0;
	m_CardScale = .62f;
	m_CardsPerCluster = 3;
	m_Whorl = 4;
	m_CapSize = .5f;
	m_Foliage = FOLIAGE_Game;
	m_SpecialSide = 0;
	m_Coverage = .35f;
	m_Type = 0;
	m_Zone = 1;
	m_Slot = 0;
	m_Hue2 = 0;
	m_FruitColor = QColor(200, 40, 30);
}

Traits Traits::Derive(const AuFloraDNA& dna, const World& world, int zone, bool tree, int index, int forceType,
		int forceFoliage)
{
	Traits t;
	Rng r(HashDNA(dna, Mix(Mix((quint64)zone, tree ? 0x7EE : 0x5B), (quint64)index)));

	t.m_Type = forceType >= 0 && forceType < Types().count() ? forceType : PickType(r, dna, world, zone, tree);
	const TreeType& kind = Types()[t.m_Type];
	t.m_Plan = kind.m_Plan;
	t.m_Zone = zone;
	t.m_Slot = index >= ZONE_SHRUBS + ZONE_TREES ? 17 : (tree ? ZONE_SHRUBS + index : index);
	t.m_SpecialSide = world.m_AlienTheme[1] >= 0 && kind.m_Family == world.m_AlienTheme[1] ? 1 : 0;

	t.m_LeafHue = world.m_LeafHue + r.Signed() * world.m_LeafHueSpread;
	t.m_LeafSat = world.m_LeafSat * r.Range(.85f, 1.15f);
	t.m_LeafVal = world.m_LeafVal * r.Range(.85f, 1.1f);
	if(zone == AuFloraDNA::Polar)
	{
		t.m_LeafVal *= .85f;
		t.m_LeafHue += 12.f;
	}
	if(t.m_Plan == PLAN_Conifer)
	{
		t.m_LeafVal *= .8f;
		t.m_LeafHue += 15.f;
	}
	if(world.m_Strangeness < .35f && zone == AuFloraDNA::Temperate && r.Chance(.12f))
	{
		t.m_LeafHue = r.Range(12.f, 48.f);
		t.m_LeafSat *= 1.1f;
	}
	t.m_Autumn = r.Range(.1f, .45f) + world.m_Strangeness * .3f;
	if(kind.m_Hue >= 0.f)
		t.m_LeafHue = world.m_LeafHue + (kind.m_Hue - 100.f) + r.Signed() * 6.f;
	{
		static const float fruitHues[] = { 0, 12, 30, 50, 280, 320, 200 };
		t.m_Hue2 = kind.m_Hue2 >= 0.f ? kind.m_Hue2 : (kind.m_Family != FAM_Earth ? r.Range(0.f, 360.f)
				: fruitHues[r.Below(sizeof(fruitHues)/sizeof(fruitHues[0]))]);
		QColor fc;
		if(t.m_Hue2 >= 400.f)
			fc.setHsvF(0, 0, .95f);
		else
			fc.setHsvF(fmodf(t.m_Hue2, 360.f) / 360.f, kind.m_Family == FAM_Glow ? .6f : .75f, kind.m_Family == FAM_Glow ? 1.f : .85f);
		t.m_FruitColor = fc;
	}

	QColor bark = world.m_BarkTint;
	bark.setHsvF(bark.hsvHueF() < 0 ? 0 : bark.hsvHueF(), qBound(0., bark.hsvSaturationF() * r.Range(.6f, 1.3f), 1.),
			qBound(0., bark.valueF() * r.Range(.8f, 1.05f), 1.));
	t.m_BarkTint = bark;

	const float stiff = (float)dna.Stiffness / 7.f;
	t.m_Gravitropism = r.Range(-.05f, .2f) + stiff * .1f;
	t.m_Lean = r.Range(0.f, 6.f) * Pi / 180.f;
	t.m_Flare = .25f + .25f * dna.RootSize + r.Range(0.f, .2f);
	t.m_Buttresses = 0;
	if(dna.RootType == AuFloraDNA::Prop)
		t.m_Buttresses = 3 + r.Below(4);
	else if(dna.RootType == AuFloraDNA::Tap)
		t.m_Flare *= .5f;
	t.m_Swell = 0;
	t.m_CardScale = r.Range(.55f, .7f);
	t.m_CardsPerCluster = 2 + r.Below(3);
	t.m_Whorl = 3 + r.Below(4);
	t.m_CapSize = r.Range(.3f, .65f);
	t.m_Twist = 0;

	switch(t.m_Plan)
	{
	case PLAN_Conifer:
		t.m_Gravitropism = .5f;
		t.m_Lean = 0;
		t.m_CardsPerCluster = 2;
		break;
	case PLAN_Palm:
		t.m_Lean = r.Range(4.f, 22.f) * Pi / 180.f;
		t.m_Flare = .3f;
		t.m_Buttresses = 0;
		break;
	case PLAN_Weeping:
		t.m_Gravitropism = -r.Range(.35f, .7f);
		break;
	case PLAN_Umbrella:
		t.m_Gravitropism = r.Range(.2f, .45f);
		break;
	case PLAN_Bottle:
		t.m_Swell = r.Range(.8f, 1.8f);
		t.m_Flare = .2f;
		t.m_Buttresses = 0;
		break;
	case PLAN_Spiral:
		t.m_Twist = (r.Chance(.5f) ? 1.f : -1.f) * r.Range(.5f, 1.1f);
		t.m_Gravitropism = r.Range(-.1f, .25f);
		break;
	case PLAN_Lantern:
		t.m_Gravitropism = -r.Range(.2f, .5f);
		t.m_CardsPerCluster = 1 + r.Below(2);
		break;
	default:
		break;
	}

	const TreeType& glowType = Types()[t.m_Type];
	if((glowType.m_Extras & X_Glow) || (glowType.m_Family != FAM_Earth && r.Chance(glowType.m_Family == FAM_Glow ? .9f : .45f)))
	{
		QColor g;
		g.setHsvF(fmodf(t.m_LeafHue + r.Range(120.f, 240.f), 360.f) / 360.f, r.Range(.5f, 1.f), r.Range(.12f, .35f));
		t.m_Glow = g;
	}

	t.m_Foliage = FOLIAGE_Game;
	const float alienLeaves = r.Unit();
	const int alienKind = FOLIAGE_Plume + r.Below(FOLIAGE_Count - FOLIAGE_Plume);
	const TreeType& type = Types()[t.m_Type];
	if(type.m_Foliage >= 0)
		t.m_Foliage = type.m_Foliage;
	else if(type.m_Family == FAM_Earth && alienLeaves < world.m_Strangeness * .5f)
		t.m_Foliage = alienKind;
	if(forceFoliage >= 0 && forceFoliage < FOLIAGE_AllCount)
		t.m_Foliage = forceFoliage;
	if(t.m_Foliage >= FOLIAGE_Plume && !t.m_Glow.value() && r.Chance(.35f))
	{
		QColor g;
		g.setHsvF(fmodf(t.m_LeafHue + 180.f, 360.f) / 360.f, .7f, .15f);
		t.m_Glow = g;
	}
	return t;
}

Tile Tile::Palette(unsigned int index, int tileSize)
{
	const float maxx = FT_PaletteWidth * tileSize;
	const float maxy = FT_PaletteHeight * tileSize;
	const int column = index / FT_PaletteHeight;
	const int row = index - FT_PaletteHeight*column;
	const float inset = 2.f;
	return Tile((tileSize*column + inset)/maxx, 1.f - (row*tileSize + inset)/maxy,
			(tileSize*(column+1) - inset)/maxx, 1.f - ((row+1)*tileSize - inset)/maxy);
}

Tile Tile::Band(float top, float bottom) const
{
	return Tile(s, t + (v - t) * top, u, t + (v - t) * bottom);
}

Budget Budget::ForLod(int lod)
{
	Budget b;
	if(lod >= 2)
	{
		b.m_MaxSegments = 700;
		b.m_MaxCards = 500;
		b.m_SidesMax = 12;
		b.m_RingsPerSegment = 3;
	}
	else
	{
		b.m_MaxSegments = 160;
		b.m_MaxCards = 160;
		b.m_SidesMax = 6;
		b.m_RingsPerSegment = 1;
	}
	return b;
}

void ScaleToFit(QList<AuFloraVAOTri>& tris, int from, float maxRadius)
{
	if(maxRadius <= 0.f)
		return;
	float r = 0;
	for(int i=from; i<tris.count(); i++)
	{
		const AuFloraVAOTri& t = tris[i];
		r = qMax(r, sqrtf(t.m_V0.m_X*t.m_V0.m_X + t.m_V0.m_Y*t.m_V0.m_Y));
		r = qMax(r, sqrtf(t.m_V1.m_X*t.m_V1.m_X + t.m_V1.m_Y*t.m_V1.m_Y));
		r = qMax(r, sqrtf(t.m_V2.m_X*t.m_V2.m_X + t.m_V2.m_Y*t.m_V2.m_Y));
	}
	if(r <= maxRadius)
		return;
	const float k = maxRadius / r;
	for(int i=from; i<tris.count(); i++)
	{
		AuFloraVAOVert* v[3] = { &tris[i].m_V0, &tris[i].m_V1, &tris[i].m_V2 };
		for(int j=0; j<3; j++)
		{
			v[j]->m_X *= k; v[j]->m_Y *= k; v[j]->m_Z *= k; v[j]->m_MoveFactor *= k;
		}
	}
}

void BuildPalm(Builder& b)
{
	const AuFloraDNA& dna = b.m_DNA;
	const Traits& tr = b.m_Tr;
	Style trunkStyle;
	trunkStyle.taperMul = .25f;
	trunkStyle.bendDeg = 3.f;
	trunkStyle.grav = .12f;
	trunkStyle.lengthMul = 1.15f;
	const QVector3D trunkDir = Deflect(Up, tr.m_Lean, b.m_Rng.Range(0.f, 2.f*Pi));
	const int trunk = b.Grow(QVector3D(0, 0, 0), trunkDir, b.m_TrunkDia * .8f, 4 + (int)dna.TrunkLength / 12, 0.f, 0, trunkStyle);
	if(trunk < 0)
		return;
	b.m_Limbs[trunk].material = 1;
	const Node top = b.m_Limbs[trunk].nodes.last();

	const int fronds = qMin(16, 7 + (int)dna.Multiples * 2 + (int)dna.SproutMultiples / 2);
	const float length = b.m_CardSize * b.m_Rng.Range(1.3f, 1.8f);
	float az = b.m_Rng.Range(0.f, 2.f*Pi);
	for(int i=0; i<fronds; i++)
	{
		az += GoldenAngle;
		const float age = (float)i / fronds;
		const QVector3D d = Deflect(Up, .35f + age * .9f + b.m_Rng.Signed() * .12f, az);
		Cluster c;
		c.p = top.p;
		c.dir = d;
		c.size = length * (1.f - age * .2f);
		c.kind = 4;
		b.m_Clusters.append(c);
	}
}

void BuildFungal(Builder& b, bool shrub)
{
	const Traits& tr = b.m_Tr;
	const int stalks = shrub ? 2 + b.m_Rng.Below(4) : (b.m_Rng.Chance(.3f) ? 2 + b.m_Rng.Below(2) : 1);
	float height = 0, dia = b.m_TrunkDia;
	for(int i=0; i<4 && dia >= b.m_MinDia; i++)
	{
		height += b.m_DNA.BranchLength(dia);
		dia -= dia * b.m_DNA.BranchTaperFactor();
	}
	if(shrub)
		height = b.m_CardSize * .7f;
	for(int si=0; si<stalks; si++)
	{
		const float h = height * (si == 0 ? 1.f : b.m_Rng.Range(.35f, .7f));
		const float r = (shrub ? h * .06f : b.m_TrunkDia * .55f) * (si == 0 ? 1.f : .6f);
		const float spread = si == 0 ? 0.f : h * .35f;
		const float az = b.m_Rng.Range(0.f, 2.f*Pi);
		const QVector3D base(cosf(az) * spread, sinf(az) * spread, 0);
		const QVector3D lean = Deflect(Up, si == 0 ? tr.m_Lean : b.m_Rng.Range(.15f, .45f), az);
		Style st;
		st.bendDeg = 4;
		st.grav = .15f;
		st.taperMul = 0.f;
		const int segs = 3;
		st.lengthMul = h / (segs * qMax(.001f, b.m_DNA.BranchLength(r * 2.f)));
		const int stalk = b.Grow(base, lean, r * 2.f, segs, 0.f, 0, st);
		if(stalk < 0)
			continue;
		b.m_Limbs[stalk].material = 2;
		b.m_Limbs[stalk].sidesScale = 1.3f;
		const QVector<Node>& sn = b.m_Limbs[stalk].nodes;
		const Node top = sn.last();
		const QVector3D axis = (top.p - sn[sn.count()-2].p).normalized();
		b.m_CapRequests.append(Builder::CapRequest{top.p, axis, h * tr.m_CapSize * (shrub ? 1.2f : 1.f), r});
	}
}

void BuildCrystal(Builder& b, bool shrub)
{
	float height = 0, dia = b.m_TrunkDia;
	for(int i=0; i<4 && dia >= b.m_MinDia; i++)
	{
		height += b.m_DNA.BranchLength(dia);
		dia -= dia * b.m_DNA.BranchTaperFactor();
	}
	if(shrub)
		height = b.m_CardSize * .8f;
	const int shards = shrub ? 4 + b.m_Rng.Below(5) : 5 + b.m_Rng.Below(6);
	float az = b.m_Rng.Range(0.f, 2.f*Pi);
	for(int i=0; i<shards; i++)
	{
		az += GoldenAngle;
		const bool main = i == 0;
		const float h = height * (main ? 1.f : b.m_Rng.Range(.25f, .65f));
		const float r = (shrub ? height * .07f : b.m_TrunkDia * .7f) * (main ? 1.f : b.m_Rng.Range(.35f, .7f));
		QVector3D base(0, 0, 0);
		QVector3D d = Up;
		if(!main)
		{
			const float off = b.m_Rng.Range(0.f, height * (shrub ? .2f : .12f));
			base = QVector3D(cosf(az) * off, sinf(az) * off, 0);
			d = Deflect(Up, b.m_Rng.Range(.25f, .8f), az);
			if(b.m_Rng.Chance(.35f) && !shrub)
			{
				base = Up * (height * b.m_Rng.Range(.2f, .6f));
			}
		}
		Style st;
		st.bendDeg = 0;
		st.grav = 0;
		st.taperMul = 0;
		st.lengthMul = h / qMax(.001f, b.m_DNA.BranchLength(r * 2.f));
		const int l = b.Grow(base - d * r, d, r * 2.f, 1, 0.f, main ? 0 : 1, st);
		if(l < 0)
			continue;
		b.m_Limbs[l].faceted = true;
		b.m_Limbs[l].tip = true;
		b.m_Limbs[l].material = 3;
		b.m_Limbs[l].sidesScale = 2.f;
	}
}

void Specials(Builder& b)
{
	for(const Builder::CapRequest& cap : b.m_CapRequests)
	{
		const int rings = b.m_Lod < 2 ? 3 : 7;
		const int sides = b.m_Lod < 2 ? 7 : 16;
		const QVector3D axis = cap.axis;
		const QVector3D n0 = Perp(axis), b0 = QVector3D::crossProduct(axis, n0);
		const float R = cap.radius;
		const float H = R * b.m_CapShape;
		const Tile top = b.m_Special.Band(0.f, .5f);
		const Tile under = b.m_Special.Band(.75f, 1.f);
		auto dome = [&](int ri, int k, QVector3D& p, QVector3D& n)
		{
			const float a = (float)ri / rings;
			const float ph = a * Pi * .5f * 1.08f;
			const float th = 2.f*Pi * k / sides;
			const QVector3D radial = n0*cosf(th) + b0*sinf(th);
			const float rr = R * sinf(ph);
			const float zz = H * cosf(ph) - (a > .9f ? R * .05f : 0.f);
			p = cap.p + axis * (zz + cap.stalkR * .5f) + radial * rr;
			n = (radial * (sinf(ph) / R) + axis * (cosf(ph) / qMax(1e-4f, H))).normalized();
		};
		const float move = qMax(0.f, cap.p.z()) * b.m_WindWood * .4f;
		for(int ri=0; ri<rings; ri++)
			for(int k=0; k<sides; k++)
			{
				QVector3D p00, p01, p10, p11, q00, q01, q10, q11;
				dome(ri, k, p00, q00);
				dome(ri, k+1, p01, q01);
				dome(ri+1, k, p10, q10);
				dome(ri+1, k+1, p11, q11);
				const float u0 = top.s + (top.u - top.s) * k / sides, u1 = top.s + (top.u - top.s) * (k+1) / sides;
				const float t0 = top.t + (top.v - top.t) * ri / rings, t1 = top.t + (top.v - top.t) * (ri+1) / rings;
				AuFloraVAOVert a = Builder::Vert(p00, q00, u0, t0, move), bb = Builder::Vert(p01, q01, u1, t0, move);
				AuFloraVAOVert c = Builder::Vert(p10, q10, u0, t1, move), d = Builder::Vert(p11, q11, u1, t1, move);
				if(ri > 0)
					b.Tri(a, c, bb);
				b.Tri(bb, c, d);
			}
		for(int k=0; k<sides; k++)
		{
			QVector3D r0, r1, nn;
			dome(rings, k, r0, nn);
			dome(rings, k+1, r1, nn);
			const float th0 = 2.f*Pi * k / sides, th1 = 2.f*Pi * (k+1) / sides;
			const QVector3D s0 = cap.p + axis * (H * .15f) + (n0*cosf(th0) + b0*sinf(th0)) * cap.stalkR;
			const QVector3D s1 = cap.p + axis * (H * .15f) + (n0*cosf(th1) + b0*sinf(th1)) * cap.stalkR;
			const QVector3D down = -axis;
			const float u0 = under.s + (under.u - under.s) * k / sides, u1 = under.s + (under.u - under.s) * (k+1) / sides;
			AuFloraVAOVert a = Builder::Vert(r0, down, u0, under.t, move), bb = Builder::Vert(r1, down, u1, under.t, move);
			AuFloraVAOVert c = Builder::Vert(s0, down, u0, under.v, move), d = Builder::Vert(s1, down, u1, under.v, move);
			b.Tri(a, c, bb);
			b.Tri(c, d, bb);
		}
	}
}

void ScaleHeight(QList<AuFloraVAOTri>& tris, int from, float height)
{
	float top = 0;
	for(int i=from; i<tris.count(); i++)
		top = qMax(top, qMax(tris[i].m_V0.m_Z, qMax(tris[i].m_V1.m_Z, tris[i].m_V2.m_Z)));
	if(top <= 0.f || height <= 0.f)
		return;
	const float k = height / top;
	for(int i=from; i<tris.count(); i++)
	{
		AuFloraVAOVert* v[3] = { &tris[i].m_V0, &tris[i].m_V1, &tris[i].m_V2 };
		for(int j=0; j<3; j++)
		{
			v[j]->m_X *= k; v[j]->m_Y *= k; v[j]->m_Z *= k; v[j]->m_MoveFactor *= k;
		}
	}
}

void Finish(Builder& b, int from, float maxRadius, bool scaleToHeight)
{
	b.CrownShape();
	for(int li=0; li<b.m_Limbs.count(); li++)
		b.Tube(b.m_Limbs[li], b.m_Limbs[li].depth == 0);
	Specials(b);

	for(const Cluster& c : b.m_Clusters)
	{
		if(c.kind == 1)
		{
			const float move = qMax(0.f, c.p.z()) * b.m_WindLeaf;
			b.Ball(c.p - Up * (c.size * 1.6f), c.size, b.m_Special.Band(0.f, .5f), move);
			continue;
		}
		if(c.kind == 4)
		{
			b.Frond(c.p, c.dir, c.size, c.size * .55f,
					(b.m_FrondDroop > 0.f ? b.m_FrondDroop : 1.6f) + b.m_Rng.Range(0.f, .6f), b.m_Leaf);
			continue;
		}
		b.ClusterCards(c);
	}
	if(scaleToHeight)
		ScaleHeight(b.m_Tris, from, b.m_Height);
	ScaleToFit(b.m_Tris, from, maxRadius);

	for(int i=from; i<b.m_Tris.count(); i++)
		qSwap(b.m_Tris[i].m_V1, b.m_Tris[i].m_V2);
}

void ArchBranch(Builder& b, const Arch& a, int level, const QVector3D& start, const QVector3D& dir0, float length,
		float r0)
{
	if(b.m_Segments >= b.m_Budget.m_MaxSegments || length <= 0.f)
		return;
	int segs = level == 0 ? 6 : (level == 1 ? 4 : (level >= a.levels ? 2 : 3));
	if(b.m_Lod < 2)
		segs = qMax(2, segs - 2);

	Limb limb;
	limb.depth = level;
	limb.faceted = false;
	limb.tip = level >= a.levels;
	limb.material = 0;
	limb.sidesScale = 1;
	QVector3D p = start, d = dir0.normalized();
	limb.nodes.append(Node{p, r0});
	limb.children.append(0);
	QVector<QVector3D> dirs;
	dirs.append(d);
	const float step = length / segs;
	const float tipR = qMax(r0 * .3f, a.radius * .012f);
	for(int i=1; i<=segs; i++)
	{
		p += d * step;
		const float f = (float)i / segs;
		limb.nodes.append(Node{p, r0 + (tipR - r0) * f});
		limb.children.append(0);
		b.m_Segments++;
		d = Deflect(d, b.m_Rng.Unit() * a.bend, b.m_Rng.Range(0.f, 2.f*Pi));
		d = (d + Up * a.grav[qMin(level, 3)]).normalized();
		if(a.twist != 0.f)
			d = Rotate(d, Up, a.twist);
		dirs.append(d);
	}
	if(a.twigWood || level < (b.m_Lod < 2 ? qMax(1, a.levels - 1) : a.levels))
		b.m_Limbs.append(limb);
	if(level == 0 && b.m_TrunkLen <= 0.f)
		b.m_TrunkLen = a.excurrent ? length * a.trunkFrac : length;

	auto at = [&](float t, QVector3D& pos, QVector3D& dir, float& r)
	{
		const float x = qBound(0.f, t, 1.f) * segs;
		const int i = qMin(segs - 1, (int)x);
		const float f = x - i;
		pos = Lerp(limb.nodes[i].p, limb.nodes[i+1].p, f);
		dir = (limb.nodes[i+1].p - limb.nodes[i].p).normalized();
		r = limb.nodes[i].r + (limb.nodes[i+1].r - limb.nodes[i].r) * f;
	};

	const int levels = b.m_Lod < 2 ? qMax(1, a.levels - 1) : a.levels;
	if(level < levels)
	{
		const int n = a.children[level];
		float tFrom, tTo;
		if(level == 0)
		{
			tFrom = a.excurrent ? a.trunkFrac : .72f;
			tTo = a.excurrent ? .96f : 1.f;
		}
		else
		{
			tFrom = .3f;
			tTo = .95f;
		}
		float az = b.m_Rng.Range(0.f, 2.f*Pi);
		const int groups = (level == 0 && a.whorl > 1) ? qMax(1, n / a.whorl) : n;
		const int perGroup = (level == 0 && a.whorl > 1) ? a.whorl : 1;
		for(int g=0; g<groups; g++)
		{
			float t = groups == 1 ? (tFrom + tTo) / 2.f : tFrom + (tTo - tFrom) * g / (groups - 1);
			t = qBound(0.f, t + b.m_Rng.Signed() * .3f / qMax(groups, 1), 1.f);
			QVector3D pos, dir;
			float r;
			at(t, pos, dir, r);
			for(int k=0; k<perGroup; k++)
			{
				az += perGroup > 1 ? 2.f*Pi / perGroup : GoldenAngle;
				const float ang = a.angle[qMin(level + 1, 3)] * b.m_Rng.Range(.8f, 1.2f);
				const QVector3D cdir = Deflect(dir, ang, az + b.m_Rng.Signed() * .2f);
				float clen;
				if(level == 0)
				{
					if(a.excurrent)
					{
						const float h = (t - a.trunkFrac) / qMax(.01f, 1.f - a.trunkFrac);
						clen = a.height * (.42f * powf(qMax(0.f, 1.f - h), .9f) + .06f);
					}
					else
						clen = a.height * (1.f - a.trunkFrac) * b.m_Rng.Range(.75f, 1.f);
				}
				else
					clen = length * a.ratio * (1.15f - .45f * t) * b.m_Rng.Range(.85f, 1.1f);
				const float cr = qMax(r * b.m_Rng.Range(.5f, .68f), a.radius * .01f);
				ArchBranch(b, a, level + 1, pos - cdir * (r * .5f), cdir, clen, cr);
			}
		}
	}

	const bool twig = level >= levels;
	if(twig || level == levels - 1)
	{
		const float size = a.leafSize * (b.m_Lod < 2 ? 1.5f : 1.f);
		const float spacing = size * a.leafSpacing * (b.m_Lod < 2 ? 1.6f : 1.f);
		const float from = twig ? .2f : a.leafFrom;
		const int count = qMax(1, (int)((1.f - from) * length / qMax(spacing, 1e-4f)));
		for(int i=0; i<count; i++)
		{
			const float t = from + (1.f - from) * (i + b.m_Rng.Unit() * .6f) / count;
			QVector3D pos, dir;
			float r;
			at(t, pos, dir, r);
			Cluster c;
			c.p = pos;
			c.dir = Deflect(dir, b.m_Rng.Range(.5f, 1.2f), b.m_Rng.Range(0.f, 2.f*Pi));
			c.size = size * b.m_Rng.Range(.8f, 1.2f);
			c.kind = a.leafKind;
			b.m_Clusters.append(c);
		}
		Cluster c;
		c.p = limb.nodes.last().p;
		c.dir = dirs.last();
		c.size = size;
		c.kind = a.leafKind;
		b.m_Clusters.append(c);
		if(a.pods && twig && b.m_Rng.Chance(.5f))
		{
			Cluster pod;
			pod.p = limb.nodes.last().p;
			pod.dir = -Up;
			pod.size = size * .22f;
			pod.kind = 1;
			b.m_Clusters.append(pod);
		}
	}
}

float GameHeight(const AuFloraDNA& dna, float textureSize, int trunkSegs)
{
	float h = 0, dia = dna.TrunkBaseDiameter();
	const float mindia = dna.MinBaseDiameter();
	for(int i=0; i<trunkSegs && dia >= mindia; i++)
	{
		h += dna.BranchLength(dia);
		dia -= dia * dna.BranchTaperFactor();
	}
	return h + textureSize * .6f;
}

void BuildArch(Builder& b, int plan, bool shrub)
{
	const AuFloraDNA& dna = b.m_DNA;
	const Traits& tr = b.m_Tr;
	Arch a;
	a.height = b.m_Height > 0.f ? b.m_Height : (shrub ? b.m_CardSize * .8f : GameHeight(dna, b.m_CardSize, 4) * 1.1f);
	a.trunkFrac = .3f + .04f * (dna.TrunkLength % 4);
	a.excurrent = dna.Inflorescence == AuFloraDNA::INFL_Raceme && !shrub;
	a.stems = 1;
	const int mul = dna.Multiples, prob = dna.BranchProbability;
	a.levels = dna.Inflorescence == AuFloraDNA::INFL_Spike ? 2 : 3;
	a.children[0] = 3 + mul + (prob > 1 ? 1 : 0);
	a.children[1] = 3 + mul / 2 + (prob > 2 ? 1 : 0);
	a.children[2] = 3 + prob / 2;
	a.children[3] = 2;
	a.whorl = 0;
	const float spread = (25.f + 8.f * dna.BranchAngle) * Pi / 180.f;
	a.angle[0] = 0;
	a.angle[1] = spread;
	a.angle[2] = spread * 1.2f;
	a.angle[3] = spread * 1.35f;
	a.ratio = .62f;
	a.grav[0] = .2f;
	a.grav[1] = .06f + tr.m_Gravitropism * .5f;
	a.grav[2] = tr.m_Gravitropism * .5f;
	a.grav[3] = tr.m_Gravitropism * .5f - .03f;
	a.twist = 0;
	a.bend = (6.f + 4.f * dna.BranchAngle) * Pi / 180.f;
	a.radius = qBound(a.height * .018f, dna.TrunkBaseDiameter() / 2.f, a.height * .05f);
	a.leafSize = a.height * (.085f + .012f * dna.LeafSize);
	a.leafSpacing = .5f + .08f * (3 - dna.LeafProbability);
	a.leafKind = 5;
	a.leafFrom = .75f;
	a.pods = false;
	a.twigWood = true;

	switch(plan)
	{
	case PLAN_Conifer:
		a.excurrent = true;
		a.trunkFrac = .12f + .04f * (dna.TrunkLength % 3);
		a.levels = 2;
		a.whorl = tr.m_Whorl;
		a.children[0] = tr.m_Whorl * (b.m_Lod < 2 ? 3 : 6);
		a.twigWood = false;
		a.children[1] = 4;
		a.angle[1] = (80.f + 3.f * dna.BranchAngle) * Pi / 180.f;
		a.angle[2] = 50.f * Pi / 180.f;
		a.grav[0] = .5f;
		a.grav[1] = -.04f;
		a.grav[2] = .02f;
		a.ratio = .45f;
		a.radius = a.height * .022f;
		a.leafSize = a.height * .17f;
		a.leafSpacing = .35f;
		a.leafKind = 8;
		a.leafFrom = .3f;
		a.bend = 4.f * Pi / 180.f;
		break;
	case PLAN_Weeping:
		a.trunkFrac = .3f;
		a.grav[1] = .05f;
		a.grav[2] = -.3f;
		a.grav[3] = -.8f;
		a.ratio = .85f;
		a.angle[2] = .7f;
		a.leafKind = 7;
		a.leafSpacing = .45f;
		break;
	case PLAN_Umbrella:
		a.trunkFrac = .55f;
		a.angle[1] = .45f;
		a.angle[2] = .9f;
		a.grav[1] = .1f;
		a.grav[2] = -.05f;
		a.grav[3] = 0;
		a.leafKind = 6;
		break;
	case PLAN_Bottle:
		a.trunkFrac = .62f;
		a.levels = 2;
		a.children[0] = 4 + mul;
		a.ratio = .5f;
		a.radius = a.height * .07f;
		a.leafSize *= .7f;
		break;
	case PLAN_Spiral:
		a.twist = tr.m_Twist;
		a.grav[0] = .05f;
		a.grav[1] = .15f;
		a.grav[2] = .1f;
		a.grav[3] = .1f;
		a.bend *= .3f;
		a.ratio = .7f;
		break;
	case PLAN_Lantern:
		a.grav[2] = -.25f;
		a.grav[3] = -.5f;
		a.pods = true;
		a.leafSpacing = .7f;
		break;
	default:
		break;
	}

	if(shrub)
	{
		a.stems = qBound(2, (int)dna.Multiples + (int)dna.SproutMultiples / 3 + 1, b.m_Lod < 2 ? 2 : 4);
		a.levels = 2;
		a.excurrent = false;
		a.trunkFrac = .25f;
		a.children[0] = 3;
		a.children[1] = 2;
		a.radius = a.height * .025f;
		a.leafSize = a.height * (.2f + .015f * dna.LeafSize);
		a.leafSpacing = .6f;
		a.whorl = 0;
		if(plan == PLAN_Conifer)
		{
			a.leafKind = 8;
			a.twigWood = false;
		}
	}

	if(tr.m_Foliage == FOLIAGE_Game)
		a.leafSize *= qBound(1.f, sqrtf(.3f / qMax(.02f, tr.m_Coverage)), 2.f);

	const QVector3D lean = Deflect(Up, tr.m_Lean, b.m_Rng.Range(0.f, 2.f*Pi));
	float az = b.m_Rng.Range(0.f, 2.f*Pi);
	for(int s=0; s<a.stems; s++)
	{
		QVector3D base(0, 0, 0), dir = lean;
		float len = a.excurrent ? a.height : a.height * a.trunkFrac * 1.1f;
		float r = a.radius;
		if(a.stems > 1)
		{
			az += GoldenAngle;
			const float off = a.height * .08f * sqrtf(b.m_Rng.Unit());
			base = QVector3D(cosf(az) * off, sinf(az) * off, 0);
			dir = Deflect(Up, b.m_Rng.Range(.1f, .45f), az);
			len *= b.m_Rng.Range(.8f, 1.1f);
			r *= .7f;
		}
		ArchBranch(b, a, 0, base, dir, len, r);
	}
	if(plan == PLAN_Bottle && !shrub)
		b.m_TrunkLen = a.height * a.trunkFrac * 1.1f;
}

void BuildTree(QList<AuFloraVAOTri>& tris, const AuFloraDNA& dna, const Traits& traits, float textureSize, const Tile& leaf,
		const Tile& bark, const Tile& ringed, const Tile& special, quint32 seed, int lod, float height, float maxRadius)
{
	const int from = tris.count();
	Builder b(tris, dna, traits, textureSize, leaf, bark, ringed, special, seed, lod);
	b.m_Height = height;
	switch(traits.m_Plan)
	{
	case PLAN_Palm:
		BuildPalm(b);
		break;
	case PLAN_Fungal:
		BuildFungal(b, false);
		break;
	case PLAN_Crystal:
		BuildCrystal(b, false);
		break;
	default:
		BuildArch(b, traits.m_Plan, false);
		break;
	}
	const int plan = traits.m_Plan;
	Finish(b, from, maxRadius, plan == PLAN_Palm || plan == PLAN_Fungal || plan == PLAN_Crystal);
}

void BuildShrub(QList<AuFloraVAOTri>& tris, const AuFloraDNA& dna, const Traits& traits, float textureSize, const Tile& leaf,
		const Tile& bark, const Tile& ringed, const Tile& special, quint32 seed, int lod, float height, float maxRadius)
{
	const int from = tris.count();
	Builder b(tris, dna, traits, textureSize, leaf, bark, ringed, special, seed, lod);
	b.m_Height = height;
	const float size = b.m_CardSize;
	switch(traits.m_Plan)
	{
	case PLAN_Fungal:
		BuildFungal(b, true);
		Finish(b, from, maxRadius, true);
		return;
	case PLAN_Crystal:
		BuildCrystal(b, true);
		Finish(b, from, maxRadius, true);
		return;
	case PLAN_Palm:
	{
		const int fronds = 6 + (int)dna.Multiples * 2;
		float az = b.m_Rng.Range(0.f, 2.f*Pi);
		for(int i=0; i<fronds; i++)
		{
			az += GoldenAngle;
			Cluster c;
			c.p = QVector3D(0, 0, size * .05f);
			c.dir = Deflect(Up, b.m_Rng.Range(.3f, 1.f), az);
			c.size = size * b.m_Rng.Range(.7f, 1.f);
			c.kind = 4;
			b.m_Clusters.append(c);
		}
		Finish(b, from, maxRadius);
		return;
	}
	default:
		break;
	}

	BuildArch(b, traits.m_Plan, true);
	Finish(b, from, maxRadius);
}

float Noise(float x, float y, int period, quint32 seed)
{
	auto cell = [&](int i, int j) -> float
	{
		i = ((i % period) + period) % period;
		j = ((j % period) + period) % period;
		quint64 h = Mix(Mix(seed, (quint64)i), (quint64)j);
		return (float)(h & 0xFFFF) / 65535.f;
	};
	const int x0 = (int)floorf(x), y0 = (int)floorf(y);
	float fx = x - x0, fy = y - y0;
	fx = fx * fx * (3.f - 2.f * fx);
	fy = fy * fy * (3.f - 2.f * fy);
	const float a = cell(x0, y0), b = cell(x0+1, y0), c = cell(x0, y0+1), d = cell(x0+1, y0+1);
	return (a + (b - a)*fx) + ((c + (d - c)*fx) - (a + (b - a)*fx)) * fy;
}

float Fbm(float x, float y, int period, quint32 seed, int octaves)
{
	float sum = 0, amp = .5f, norm = 0;
	for(int o=0; o<octaves; o++)
	{
		sum += Noise(x, y, period, seed + o * 101) * amp;
		norm += amp;
		x *= 2; y *= 2; period *= 2; amp *= .5f;
	}
	return sum / norm;
}

void RecolourFoliage(QImage& tile, const Traits& tr, quint32 seed)
{
	if(tile.format() != QImage::Format_ARGB32)
		tile = tile.convertToFormat(QImage::Format_ARGB32);
	const int w = tile.width(), h = tile.height();
	const QColor bark = tr.m_BarkTint;
	for(int y=0; y<h; y++)
	{
		QRgb* line = (QRgb*)tile.scanLine(y);
		for(int x=0; x<w; x++)
		{
			const QRgb px = line[x];
			const int a = qAlpha(px);
			if(a == 0)
				continue;
			QColor c = QColor::fromRgb(px);
			float hh = c.hsvHueF() * 360.f, ss = c.hsvSaturationF(), vv = c.valueF();
			if(hh < 0)
				hh = 0;
			if(hh >= 40.f && hh <= 175.f && ss > .12f)
			{
				const float patch = Fbm(x * 8.f / w, y * 8.f / h, 8, seed, 2) - .5f;
				float nh = tr.m_LeafHue + (hh - 100.f) * .6f + patch * 60.f * tr.m_Autumn;
				nh = fmodf(nh + 720.f, 360.f);
				const float ns = qBound(0.f, ss * tr.m_LeafSat, 1.f);
				const float lift = fabsf(nh - 100.f) > 30.f ? 1.2f : 1.f;
				const float nv = qBound(0.f, vv * tr.m_LeafVal * lift * (1.f + patch * .25f * tr.m_Autumn), 1.f);
				c.setHsvF(nh / 360.f, ns, nv);
				line[x] = qRgba(c.red(), c.green(), c.blue(), a);
			}
			else if(ss < .55f && (hh < 45.f || hh > 340.f || ss < .15f))
			{
				line[x] = qRgba(qRed(px) * bark.red() / 255, qGreen(px) * bark.green() / 255, qBlue(px) * bark.blue() / 255, a);
			}
		}
	}
}

QColor LeafColor(const Traits& tr, float shade, float hueJitter)
{
	QColor c;
	const float h = fmodf(tr.m_LeafHue + hueJitter + 720.f, 360.f);
	c.setHsvF(h / 360.f, qBound(0.f, .55f * tr.m_LeafSat * (1.1f - .25f * shade), 1.f),
			qBound(0.f, (.22f + .5f * shade) * tr.m_LeafVal, 1.f));
	return c;
}

void PaintNeedles(QImage& tile, const Traits& tr, quint32 seed)
{
	const int sz = tile.width();
	tile.fill(Qt::transparent);
	Rng r(Mix(seed, 0x9EED));
	QPainter p(&tile);
	p.setRenderHint(QPainter::Antialiasing);
	const float w = sz / 512.f;
	const QColor bark = QColor(90, 70, 50);
	const float needleLen = sz * r.Range(.035f, .06f);
	auto needles = [&](QPointF a, QPointF b, float lenScale)
	{
		const QPointF d = b - a;
		const float len = sqrtf(d.x()*d.x() + d.y()*d.y());
		if(len < 1)
			return;
		const QPointF u(d.x() / len, d.y() / len), n(-u.y(), u.x());
		const int count = (int)(len / (2.2f * w));
		for(int i=0; i<count; i++)
		{
			const float f = (i + r.Unit()) / count;
			const QPointF at = a + d * f;
			const float nl = needleLen * lenScale * (1.f - .45f * f) * r.Range(.8f, 1.15f);
			for(int side=-1; side<=1; side+=2)
			{
				const float ang = r.Range(.6f, 1.1f);
				const QPointF dir = u * cosf(ang) + n * (side * sinf(ang));
				p.setPen(QPen(LeafColor(tr, r.Unit(), r.Signed() * 6.f), 2.2f * w, Qt::SolidLine, Qt::RoundCap));
				p.drawLine(at, at + dir * nl);
			}
		}
	};
	const QPointF top(sz / 2.f, 1.f);
	const QPointF tip(sz / 2.f + r.Signed() * sz * .08f, sz * .93f);
	p.setPen(QPen(bark, 4.f * w, Qt::SolidLine, Qt::RoundCap));
	p.drawLine(top, tip);
	const int shoots = 7 + r.Below(5);
	for(int i=0; i<shoots; i++)
	{
		const float f = .12f + .8f * (i + .5f) / shoots;
		const QPointF at = top + (tip - top) * f;
		const float side = (i & 1) ? 1.f : -1.f;
		const float reach = sz * .42f * (1.f - .6f * f) * r.Range(.8f, 1.1f);
		const float ang = r.Range(.55f, .9f);
		const QPointF end = at + QPointF(side * sinf(ang) * reach, cosf(ang) * reach);
		p.setPen(QPen(bark, 2.f * w, Qt::SolidLine, Qt::RoundCap));
		p.drawLine(at, end);
		needles(at, end, .9f);
	}
	needles(top, tip, 1.f);
	p.end();
}

void PaintFrond(QImage& tile, const Traits& tr, quint32 seed)
{
	const int sz = tile.width();
	tile.fill(Qt::transparent);
	Rng r(Mix(seed, 0xF20D));
	QPainter p(&tile);
	p.setRenderHint(QPainter::Antialiasing);
	const float w = sz / 512.f;
	const QPointF top(sz / 2.f, 1.f), tip(sz / 2.f, sz * .97f);
	const int pairs = 16 + r.Below(10);
	const float width = r.Range(.04f, .09f) * sz;
	for(int i=0; i<pairs; i++)
	{
		const float f = .06f + .9f * (i + .5f) / pairs;
		const QPointF at = top + (tip - top) * f;
		const float len = sz * .47f * sinf(Pi * qMin(1.f, f * 1.05f)) * r.Range(.9f, 1.05f) + sz * .03f;
		for(int side=-1; side<=1; side+=2)
		{
			const float ang = r.Range(.7f, 1.05f);
			const QPointF dir(side * sinf(ang), cosf(ang));
			const QPointF n(-dir.y(), dir.x());
			const QPointF end = at + dir * len;
			QPainterPath leaf;
			leaf.moveTo(at);
			leaf.quadTo(at + dir * (len * .45f) + n * (width * .5f), end);
			leaf.quadTo(at + dir * (len * .45f) - n * (width * .5f), at);
			QLinearGradient grad(at, end);
			grad.setColorAt(0, LeafColor(tr, .45f + .2f * r.Unit(), r.Signed() * 5.f));
			grad.setColorAt(1, LeafColor(tr, .75f + .2f * r.Unit(), r.Signed() * 8.f));
			p.setPen(Qt::NoPen);
			p.setBrush(grad);
			p.drawPath(leaf);
			p.setPen(QPen(LeafColor(tr, .2f, 0.f), 1.2f * w));
			p.drawLine(at, at + dir * (len * .9f));
		}
	}
	p.setPen(QPen(LeafColor(tr, .3f, 10.f), 5.f * w, Qt::SolidLine, Qt::RoundCap));
	p.drawLine(top, tip);
	p.end();
}

void PaintAlienFoliage(QImage& tile, const Traits& tr, quint32 seed)
{
	const int sz = tile.width();
	tile.fill(Qt::transparent);
	Rng r(Mix(seed, 0xA11E + tr.m_Foliage));
	QPainter p(&tile);
	p.setRenderHint(QPainter::Antialiasing);
	const float w = sz / 512.f;
	const QPointF top(sz / 2.f, 1.f);
	const QColor accent = QColor::fromHsvF(fmodf(tr.m_LeafHue + 150.f + r.Signed() * 30.f, 360.f) / 360.f, .6f, .95f);
	switch(tr.m_Foliage)
	{
	case FOLIAGE_Plume:
	{
		const int quills = 5 + r.Below(5);
		for(int q=0; q<quills; q++)
		{
			const float a = (q - (quills - 1) / 2.f) / qMax(1.f, (quills - 1) / 2.f) * r.Range(.5f, .8f);
			const float len = sz * r.Range(.6f, .92f);
			const float curl = r.Signed() * .6f;
			QPointF prev = top;
			const int steps = 40;
			for(int i=1; i<=steps; i++)
			{
				const float f = (float)i / steps;
				const float ang = a + curl * f * f;
				const QPointF at = top + QPointF(sinf(ang) * len * f, cosf(ang) * len * f);
				const QPointF d = at - prev;
				const float dl = qMax(1e-3f, sqrtf(d.x()*d.x() + d.y()*d.y()));
				const QPointF n(-d.y() / dl, d.x() / dl);
				const float barb = sz * .07f * sinf(Pi * f) * r.Range(.8f, 1.2f);
				for(int s=-1; s<=1; s+=2)
				{
					p.setPen(QPen(LeafColor(tr, .4f + .5f * f, r.Signed() * 8.f), 1.6f * w));
					p.drawLine(at, at + n * (s * barb) + d * (barb / dl * .8f));
				}
				p.setPen(QPen(LeafColor(tr, .3f, 0.f), 2.5f * w));
				p.drawLine(prev, at);
				prev = at;
			}
		}
		break;
	}
	case FOLIAGE_Tendrils:
	{
		const int tendrils = 6 + r.Below(5);
		for(int k=0; k<tendrils; k++)
		{
			float ang = r.Range(-.9f, .9f);
			const float turn = (r.Chance(.5f) ? 1.f : -1.f) * r.Range(.06f, .12f);
			float step = sz * .018f;
			QPointF at = top;
			QPainterPath path(at);
			for(int i=0; i<45; i++)
			{
				at += QPointF(sinf(ang) * step, cosf(ang) * step);
				ang += turn * (1.f + i * .05f);
				step *= .975f;
				path.lineTo(at);
			}
			p.setBrush(Qt::NoBrush);
			p.setPen(QPen(LeafColor(tr, .55f, r.Signed() * 10.f), 5.f * w, Qt::SolidLine, Qt::RoundCap));
			p.drawPath(path);
			p.setPen(QPen(LeafColor(tr, .85f, 0.f), 1.5f * w, Qt::SolidLine, Qt::RoundCap));
			p.drawPath(path);
			QRadialGradient bud(at, sz * .03f);
			bud.setColorAt(0, accent.lighter(140));
			bud.setColorAt(1, accent.darker(150));
			p.setPen(Qt::NoPen);
			p.setBrush(bud);
			p.drawEllipse(at, sz * .025f, sz * .025f);
		}
		break;
	}
	case FOLIAGE_Fins:
	{
		const int fins = 3 + r.Below(3);
		for(int f=0; f<fins; f++)
		{
			const float a0 = r.Range(-.8f, .8f);
			const int ribs = 4 + r.Below(3);
			const float len = sz * r.Range(.55f, .9f);
			QPolygonF fan;
			fan << top;
			QVector<QPointF> ends;
			for(int i=0; i<ribs; i++)
			{
				const float a = a0 + (i - (ribs - 1) / 2.f) * .16f;
				const float l = len * (1.f - .15f * fabsf(i - (ribs - 1) / 2.f));
				const QPointF e = top + QPointF(sinf(a) * l, cosf(a) * l);
				fan << e;
				ends << e;
			}
			QRadialGradient g(top, len);
			QColor inner = LeafColor(tr, .5f, 0.f), outer = LeafColor(tr, .95f, 20.f);
			inner.setAlpha(235);
			outer.setAlpha(150);
			g.setColorAt(0, inner);
			g.setColorAt(1, outer);
			p.setPen(Qt::NoPen);
			p.setBrush(g);
			p.drawPolygon(fan);
			p.setPen(QPen(LeafColor(tr, .2f, 0.f), 2.5f * w));
			for(const QPointF& e : ends)
				p.drawLine(top, e);
		}
		break;
	}
	default:
	{
		const int blades = 6 + r.Below(6);
		for(int i=0; i<blades; i++)
		{
			const float a = r.Range(-.8f, .8f);
			const float len = sz * r.Range(.45f, .9f), wid = sz * r.Range(.03f, .06f);
			const QPointF dir(sinf(a), cosf(a)), n(-dir.y(), dir.x());
			const QPointF base = top + dir * (sz * .03f);
			QPolygonF blade;
			blade << base - n * wid << base + dir * (len * .8f) - n * (wid * .6f) << base + dir * len
					<< base + dir * (len * .8f) + n * (wid * .6f) << base + n * wid;
			QLinearGradient g(base - n * wid, base + n * wid);
			g.setColorAt(0, LeafColor(tr, .35f, 0.f));
			g.setColorAt(.5f, LeafColor(tr, .95f, 15.f));
			g.setColorAt(1, LeafColor(tr, .55f, 0.f));
			p.setPen(QPen(LeafColor(tr, 1.f, 30.f), 1.5f * w));
			p.setBrush(g);
			p.drawPolygon(blade);
		}
		break;
	}
	}
	p.end();
}

void PaintFoliage(QImage& tile, const Traits& tr, quint32 seed)
{
	switch(tr.m_Foliage)
	{
	case FOLIAGE_Game:		RecolourFoliage(tile, tr, seed);	break;
	case FOLIAGE_Needles:	PaintNeedles(tile, tr, seed);		break;
	case FOLIAGE_Frond:		PaintFrond(tile, tr, seed);			break;
	default:
		if(tr.m_Foliage >= FOLIAGE_Count)
			PaintTypeFoliage(tile, tr, seed);
		else
			PaintAlienFoliage(tile, tr, seed);
		break;
	}
}

QRgb Tint(QRgb px, const QColor& tint, float shade)
{
	const int r = qBound(0, (int)(qRed(px) * tint.redF() * shade), 255);
	const int g = qBound(0, (int)(qGreen(px) * tint.greenF() * shade), 255);
	const int b = qBound(0, (int)(qBlue(px) * tint.blueF() * shade), 255);
	return qRgba(r, g, b, 255);
}

void PaintBark(QImage& tile, const QImage& barkStrip, int index, const World& world, quint32 genSeed)
{
	const int sz = tile.width();
	tile.fill(Qt::black);
	const int bw = barkStrip.width();
	QImage src = barkStrip.copy(0, index * bw, bw, bw).scaled(sz, sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
			.convertToFormat(QImage::Format_ARGB32);
	Rng r(Mix(genSeed, 0xBA4C + index));
	QColor tint = world.m_BarkTint;
	{
		float hh = tint.hsvHueF() < 0 ? 0 : tint.hsvHueF();
		tint.setHsvF(hh, qBound(0., tint.hsvSaturationF() * r.Range(.7f, 1.2f), 1.), qBound(0., tint.valueF() * r.Range(.9f, 1.05f), 1.));
	}
	const bool smooth = world.m_Strangeness > .45f && index >= 6;
	for(int y=0; y<sz; y++)
	{
		QRgb* line = (QRgb*)tile.scanLine(y);
		const QRgb* sl = (const QRgb*)src.constScanLine(y);
		for(int x=0; x<sz; x++)
		{
			const float n = Fbm(x * 4.f / sz, y * 4.f / sz, 4, genSeed + index * 7, 3);
			if(smooth)
			{
				const float pores = Noise(x * 32.f / sz, y * 32.f / sz, 32, genSeed + index * 13);
				const float lum = .75f + .3f * n - (pores > .82f ? .35f : 0.f);
				line[x] = Tint(qRgb(200, 200, 200), tint, lum);
			}
			else
				line[x] = Tint(sl[x], tint, .85f + .3f * n);
		}
	}
}

void PaintRinged(QImage& tile, const World& world, quint32 genSeed)
{
	const int sz = tile.width();
	Rng r(Mix(genSeed, 0x419E));
	const QColor base = QColor::fromHsvF(r.Range(.07f, .1f), r.Range(.25f, .45f), r.Range(.45f, .6f));
	const int bands = 6 + r.Below(5);
	for(int y=0; y<sz; y++)
	{
		QRgb* line = (QRgb*)tile.scanLine(y);
		const float fy = (float)y / sz * bands;
		const float within = fy - floorf(fy);
		const float ring = within < .12f ? .55f + within * 2.f : .8f + .2f * sinf(within * Pi);
		for(int x=0; x<sz; x++)
		{
			const float fib = Noise(x * 64.f / sz, y * 6.f / sz, 64, genSeed + 3);
			const float n = Fbm(x * 4.f / sz, y * 4.f / sz, 4, genSeed + 5, 2);
			line[x] = Tint(qRgb(255, 255, 255), base, ring * (.8f + .25f * fib) * (.85f + .3f * n));
			line[x] = Tint(line[x], world.m_BarkTint, 1.f);
		}
	}
}

void PaintSpecial(QImage& tile, const World& world, quint32 genSeed)
{
	const int sz = tile.width();
	Rng r(Mix(genSeed, 0x5BEC));
	const int half = sz / 2, stemEnd = sz * 3 / 4;
	for(int y=0; y<sz; y++)
	{
		QRgb* line = (QRgb*)tile.scanLine(y);
		for(int x=0; x<sz; x++)
		{
			const int side = x < half ? 0 : 1;
			const int first = world.m_AlienTheme[0] >= 0 ? world.m_AlienTheme[0] : FAM_Fungus;
			const int family = world.m_AlienTheme[side] >= 0 ? world.m_AlienTheme[side] : first;
			const int theme = family == FAM_Crystal || family == FAM_Geometric ? PLAN_Crystal
					: (family == FAM_Glow || family == FAM_Flesh || family == FAM_Air ? PLAN_Lantern
					: (family == FAM_Spiral || family == FAM_Marine ? PLAN_Spiral : PLAN_Fungal));
			const QColor c0 = world.m_MaterialColor[side];
			const QColor c1 = world.m_MaterialColor[1 - side];
			const float fx = (float)(x - side * half) / half, fy = (float)y / sz;
			const float n = Fbm(fx * 4.f, fy * 4.f, 4, genSeed + 11, 4);
			QRgb px;
			if(y < half)
			{
				switch(theme)
				{
				case PLAN_Crystal:
				{
					const float band = Noise(fx * 6.f, 0.5f, 6, genSeed + 17);
					const float seam = fabsf(sinf(fx * 6.f * Pi));
					const float lum = .55f + .45f * band + (seam < .06f ? .5f : 0.f) + .15f * n;
					px = Tint(qRgb(255, 255, 255), c0, lum);
					break;
				}
				case PLAN_Lantern:
				{
					const float vein = fabsf(Fbm(fx * 6.f, fy * 3.f, 6, genSeed + 19, 3) - .5f);
					const float lum = .85f + .2f * n - (vein < .03f ? .45f : 0.f);
					px = Tint(qRgb(255, 255, 255), c0, lum);
					break;
				}
				case PLAN_Spiral:
				{
					const float stripe = sinf((fx + fy * .5f) * 2.f * Pi * 4.f);
					px = Tint(qRgb(255, 255, 255), stripe > 0.f ? c0 : c1, .75f + .3f * n);
					break;
				}
				default:
				{
					const float spot = Noise(fx * 10.f, fy * 5.f, 10, genSeed + 23);
					const bool spotted = world.m_Strangeness > .4f || (genSeed & 1);
					if(spotted && spot > .78f)
						px = Tint(qRgb(255, 255, 255), QColor(240, 235, 220), .9f + .1f * n);
					else
						px = Tint(qRgb(255, 255, 255), c0, .7f + .4f * n);
					break;
				}
				}
			}
			else if(y < stemEnd)
			{
				const float fib = Noise(fx * 48.f, fy * 4.f, 48, genSeed + 29);
				QColor stem = c0.lighter(150);
				if(theme == PLAN_Crystal)
					stem = c0;
				px = Tint(qRgb(235, 230, 220), stem, .75f + .2f * fib + .1f * n);
			}
			else
			{
				const float gill = fabsf(sinf(fx * Pi * 48.f));
				px = Tint(qRgb(255, 255, 255), c1, (.35f + .5f * gill) * (.8f + .3f * n));
			}
			line[x] = px;
		}
	}
	Q_UNUSED(r);
}

float Coverage(const QImage& tile)
{
	if(tile.isNull())
		return 0.f;
	const QImage im = tile.format() == QImage::Format_ARGB32 ? tile : tile.convertToFormat(QImage::Format_ARGB32);
	qint64 opaque = 0;
	for(int y=0; y<im.height(); y++)
	{
		const QRgb* line = (const QRgb*)im.constScanLine(y);
		for(int x=0; x<im.width(); x++)
			if(qAlpha(line[x]) >= 38)
				opaque++;
	}
	return (float)opaque / ((qint64)im.width() * im.height());
}

void Dilate(QImage& image, int passes)
{
	if(image.format() != QImage::Format_ARGB32)
		image = image.convertToFormat(QImage::Format_ARGB32);
	const int w = image.width(), h = image.height();
	QVector<quint8> has(w * h);
	for(int y=0; y<h; y++)
	{
		const QRgb* line = (const QRgb*)image.constScanLine(y);
		for(int x=0; x<w; x++)
			has[y*w + x] = qAlpha(line[x]) >= 16 ? 1 : 0;
	}
	for(int pass=0; pass<passes; pass++)
	{
		QVector<quint8> next = has;
		QImage src = image.copy();
		bool any = false;
		for(int y=0; y<h; y++)
		{
			QRgb* line = (QRgb*)image.scanLine(y);
			for(int x=0; x<w; x++)
			{
				if(has[y*w + x])
					continue;
				int r = 0, g = 0, b = 0, n = 0;
				for(int dy=-1; dy<=1; dy++)
				{
					const int yy = y + dy;
					if(yy < 0 || yy >= h)
						continue;
					const QRgb* sl = (const QRgb*)src.constScanLine(yy);
					for(int dx=-1; dx<=1; dx++)
					{
						const int xx = x + dx;
						if(xx < 0 || xx >= w || !has[yy*w + xx])
							continue;
						r += qRed(sl[xx]); g += qGreen(sl[xx]); b += qBlue(sl[xx]); n++;
					}
				}
				if(n)
				{
					line[x] = qRgba(r/n, g/n, b/n, qAlpha(line[x]));
					next[y*w + x] = 1;
					any = true;
				}
			}
		}
		has = next;
		if(!any)
			break;
	}
}

}
