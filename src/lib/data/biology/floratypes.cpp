#include "floratypes.h"
#include "floragenbuild.h"
#include "audice.h"
#include "autime.h"
#include <QPainterPath>
#include <QRadialGradient>
#include <QLinearGradient>
#include <functional>

namespace FloraGen
{

const char* FamilyName(int family)
{
	switch(family)
	{
	case FAM_Earth:		return "Earth-like";
	case FAM_Fungus:	return "Fungi";
	case FAM_Crystal:	return "Crystals";
	case FAM_Spiral:	return "Spirals";
	case FAM_Glow:		return "Lights";
	case FAM_Marine:	return "Reef";
	case FAM_Flesh:		return "Flesh";
	case FAM_Air:		return "Air";
	case FAM_Geometric:	return "Geometric";
	}
	return "?";
}

TreeType::TreeType(const QString& name)
{
	m_Name = name;
	m_Family = FAM_Earth;
	m_Zones = Z_All;
	m_Forms = F_Tree;
	m_Weight = 5;
	m_Builder = B_Arch;
	m_Plan = PLAN_Broadleaf;
	m_Foliage = -1;
	m_LeafKind = 5;
	m_Material = -1;
	m_TrunkFrac = .35f;
	m_Spread = 35;
	m_Ratio = .62f;
	m_Grav[0] = .06f;
	m_Grav[1] = 0;
	m_Grav[2] = -.03f;
	m_Twist = 0;
	m_RadiusMul = 1;
	m_LeafSize = .09f;
	m_LeafSpacing = .55f;
	m_Levels = 3;
	m_Limbs = 0;
	m_Excurrent = false;
	m_Whorl = 0;
	m_Stems = 1;
	m_Extras = 0;
	m_Hue = -1;
	m_Hue2 = -1;
	m_P1 = 0;
	m_P2 = 0;
}

static const float White = 400.f;

static QVector<TreeType> MakeTypes()
{
	typedef TreeType T;
	const int P = Z_Polar, Te = Z_Temperate, R = Z_Tropical;
	QVector<TreeType> v;

	v << T("Oak").z(Te).w(9).forms(F_Both).shape(.32f, 40, .62f).grav(.03f, -.02f, -.04f).x(X_Gnarl).rad(1.3f);
	v << T("Maple").z(Te).w(8).forms(F_Both).fol(FOLIAGE_Palmate).shape(.33f, 35, .6f).grav(.08f, .02f, -.02f).leaf(.1f, .5f);
	v << T("Japanese maple").z(Te).w(3).forms(F_Both).fol(FOLIAGE_Palmate).hue(8).shape(.25f, 55, .6f).grav(0, -.05f, -.03f).lk(6);
	v << T("Birch").z(P|Te).w(7).fol(FOLIAGE_Round).mat(TILE_Birch).shape(.4f, 25, .65f).grav(.1f, 0, -.12f).rad(.6f);
	v << T("Weeping birch").z(P|Te).w(3).fol(FOLIAGE_Round).mat(TILE_Birch).shape(.4f, 30, .85f).grav(.1f, -.3f, -.8f).lk(7).plan(PLAN_Weeping);
	v << T("Aspen").z(P|Te).w(6).fol(FOLIAGE_Round).mat(TILE_Birch).exc().shape(.45f, 30, .55f).grav(.3f, .05f, 0).rad(.5f).hue(75);
	v << T("Beech").z(Te).w(6).mat(TILE_Grey).shape(.3f, 45, .65f).grav(.02f, -.03f, -.05f).rad(1.2f).leaf(.09f, .45f);
	v << T("Elm").z(Te).w(5).shape(.35f, 22, .75f).grav(.2f, .05f, -.08f).limbs(4);
	v << T("Lombardy poplar").z(Te).w(4).exc().fol(FOLIAGE_Round).shape(.1f, 12, .5f).grav(.5f, .4f, .2f).rad(.7f);
	v << T("Weeping willow").z(Te).w(5).plan(PLAN_Weeping).fol(FOLIAGE_Narrow).shape(.3f, 40, .9f).grav(.05f, -.35f, -.9f).lk(7).hue(85);
	v << T("Cherry blossom").z(Te).w(4).forms(F_Both).fol(FOLIAGE_Blossom).hue(-1, 335).shape(.3f, 45, .6f).grav(.05f, -.02f, -.02f);
	v << T("Apple").z(Te).w(5).forms(F_Both).x(X_Fruit).hue(-1, 5).shape(.25f, 50, .55f).grav(0, -.05f, -.05f);
	v << T("Magnolia").z(Te|R).w(3).fol(FOLIAGE_Blossom).hue(-1, 320).shape(.3f, 40, .6f).leaf(.12f, .6f);
	v << T("Horse chestnut").z(Te).w(4).fol(FOLIAGE_Palmate).shape(.3f, 40, .62f).grav(.04f, -.04f, -.06f).rad(1.2f);
	v << T("Linden").z(Te).w(5).exc().fol(FOLIAGE_Round).shape(.25f, 45, .6f).grav(.15f, 0, -.05f).leaf(.1f, .5f);
	v << T("Plane tree").z(Te).w(5).mat(TILE_Peeling).fol(FOLIAGE_Palmate).shape(.35f, 40, .62f).rad(1.1f);
	v << T("Walnut").z(Te).w(4).fol(FOLIAGE_Compound).shape(.35f, 45, .6f).grav(.03f, -.02f, -.04f).rad(1.2f);
	v << T("Hawthorn").z(Te).w(4).forms(F_Both).fol(FOLIAGE_Blossom).hue(-1, White).x(X_Gnarl).shape(.2f, 55, .55f);
	v << T("Dogwood").z(Te).w(3).forms(F_Both).fol(FOLIAGE_Blossom).hue(-1, White).lk(6).shape(.25f, 60, .6f);
	v << T("Holly").z(Te).w(3).forms(F_Both).fol(FOLIAGE_Glossy).exc().x(X_Fruit).hue(110, 0).shape(.15f, 45, .5f);

	v << T("Spruce").z(P|Te).w(9).plan(PLAN_Conifer).fol(FOLIAGE_Needles).lk(8).exc().whorl(5).shape(.1f, 85, .45f).grav(-.04f, .02f, 0).leaf(.17f, .35f);
	v << T("Fir").z(P|Te).w(7).plan(PLAN_Conifer).fol(FOLIAGE_Needles).lk(8).exc().whorl(4).shape(.1f, 75, .45f).grav(.05f, .04f, 0).leaf(.15f, .32f);
	v << T("Scots pine").z(P|Te).w(6).plan(PLAN_Conifer).fol(FOLIAGE_Needles).lk(8).exc().whorl(4).mat(TILE_RedFibre).shape(.6f, 70, .5f).grav(.05f, .05f, .05f).leaf(.14f, .4f);
	v << T("Stone pine").z(Te).w(4).plan(PLAN_Umbrella).fol(FOLIAGE_Needles).lk(6).shape(.55f, 30, .7f).grav(.15f, -.05f, 0).leaf(.14f, .35f);
	v << T("Cedar").z(Te).w(4).plan(PLAN_Conifer).fol(FOLIAGE_Needles).lk(6).exc().whorl(4).shape(.15f, 90, .55f).grav(-.02f, 0, 0).leaf(.16f, .33f);
	v << T("Larch").z(P|Te).w(4).plan(PLAN_Conifer).fol(FOLIAGE_Needles).lk(8).exc().whorl(5).hue(70).shape(.12f, 80, .45f).leaf(.12f, .45f);
	v << T("Cypress").z(Te).w(4).plan(PLAN_Conifer).fol(FOLIAGE_Scale).exc().whorl(6).shape(.05f, 25, .4f).grav(.3f, .2f, .1f).leaf(.12f, .3f);
	v << T("Juniper").z(P|Te).w(4).forms(F_Both).plan(PLAN_Conifer).fol(FOLIAGE_Scale).x(X_Gnarl|X_Fruit).hue(-1, 230).shape(.15f, 50, .6f).stems(2);
	v << T("Redwood").z(Te).w(3).plan(PLAN_Conifer).fol(FOLIAGE_Needles).lk(8).exc().whorl(4).mat(TILE_RedFibre).shape(.45f, 85, .4f).rad(1.6f).leaf(.1f, .4f);
	v << T("Yew").z(Te).w(3).forms(F_Both).plan(PLAN_Conifer).fol(FOLIAGE_Needles).shape(.2f, 55, .6f).rad(1.4f).hue(120);
	v << T("Monkey puzzle").z(Te).w(2).plan(PLAN_Conifer).fol(FOLIAGE_Scale).exc().whorl(5).shape(.3f, 80, .6f).grav(-.02f, .25f, .1f).leaf(.08f, .25f);
	v << T("Bristlecone pine").z(P|Te).w(2).plan(PLAN_Conifer).fol(FOLIAGE_Needles).lk(8).x(X_Gnarl|X_Wind).shape(.15f, 55, .6f).rad(1.4f);
	v << T("Hemlock").z(P|Te).w(4).plan(PLAN_Conifer).fol(FOLIAGE_Needles).lk(8).exc().whorl(4).shape(.08f, 90, .45f).grav(-.1f, -.05f, 0).leaf(.16f, .3f);
	v << T("Krummholz").z(P).w(4).forms(F_Both).plan(PLAN_Conifer).fol(FOLIAGE_Needles).lk(8).x(X_Wind|X_Gnarl).shape(.1f, 60, .6f);

	v << T("Coconut palm").z(R).w(7).plan(PLAN_Palm).b(B_Palm).fol(FOLIAGE_Frond).lk(4).mat(TILE_Ringed).x(X_Leaning).leaf(.4f, 0).p(1.f, 2.f);
	v << T("Date palm").z(R|Te).w(5).plan(PLAN_Palm).b(B_Palm).fol(FOLIAGE_Frond).lk(4).mat(TILE_Ringed).leaf(.33f, 0).p(1.5f, 1.4f);
	v << T("Fan palm").z(R).w(4).plan(PLAN_Palm).b(B_Palm).fol(FOLIAGE_Fan).lk(5).mat(TILE_Ringed).leaf(.3f, 0).p(1.2f, .6f);
	v << T("Royal palm").z(R).w(3).plan(PLAN_Palm).b(B_Palm).fol(FOLIAGE_Frond).lk(4).mat(TILE_Grey).leaf(.35f, 0).p(1.f, 1.8f);
	v << T("Tree fern").z(R|Te).w(4).forms(F_Both).plan(PLAN_Palm).b(B_Palm).fol(FOLIAGE_Frond).lk(4).mat(TILE_RedFibre).leaf(.45f, 0).p(1.4f, 2.2f);
	v << T("Cycad").z(R).w(3).forms(F_Both).plan(PLAN_Palm).b(B_Palm).fol(FOLIAGE_Frond).lk(4).mat(TILE_RedFibre).leaf(.6f, 0).p(1.2f, .8f);
	v << T("Banana").z(R).w(4).forms(F_Both).plan(PLAN_Palm).b(B_Palm).fol(FOLIAGE_Broad).lk(4).mat(TILE_Succulent).leaf(.55f, 0).p(.5f, 1.4f);
	v << T("Bamboo").z(R|Te).w(4).forms(F_Both).b(B_Bamboo).fol(FOLIAGE_Narrow).mat(TILE_Ringed).hue(95).p(12);

	v << T("Mangrove").z(R).w(4).x(X_Stilts).fol(FOLIAGE_Glossy).shape(.4f, 45, .6f).grav(.05f, 0, -.03f);
	v << T("Banyan").z(R).w(4).x(X_Aerial).fol(FOLIAGE_Glossy).shape(.3f, 60, .7f).grav(-.02f, -.04f, -.05f).rad(1.4f).limbs(6);
	v << T("Strangler fig").z(R).w(3).x(X_Buttress|X_Aerial).fol(FOLIAGE_Glossy).shape(.4f, 45, .65f);
	v << T("Kapok").z(R).w(3).x(X_Buttress).plan(PLAN_Umbrella).lk(6).shape(.6f, 55, .65f).grav(.05f, -.05f, 0).rad(1.5f);
	v << T("Rubber tree").z(R).w(4).forms(F_Both).fol(FOLIAGE_Glossy).shape(.3f, 40, .6f).leaf(.12f, .5f);
	v << T("Jacaranda").z(R|Te).w(3).plan(PLAN_Umbrella).fol(FOLIAGE_Blossom).hue(-1, 270).shape(.35f, 55, .65f);
	v << T("Flame tree").z(R).w(3).plan(PLAN_Umbrella).fol(FOLIAGE_Blossom).hue(-1, 10).lk(6).shape(.4f, 60, .65f);
	v << T("Rainbow eucalyptus").z(R).w(2).mat(TILE_Rainbow).fol(FOLIAGE_Narrow).exc().shape(.4f, 35, .6f).grav(.1f, -.05f, -.2f);
	v << T("Eucalyptus").z(R|Te).w(5).mat(TILE_Peeling).fol(FOLIAGE_Narrow).shape(.45f, 35, .65f).grav(.08f, -.08f, -.3f).hue(90);
	v << T("Acacia").z(R).w(6).forms(F_Both).plan(PLAN_Umbrella).fol(FOLIAGE_Compound).lk(6).shape(.4f, 40, .7f).grav(.15f, -.05f, -.02f);
	v << T("Baobab").z(R).w(3).plan(PLAN_Bottle).x(X_Swell).shape(.65f, 50, .5f).lv(2).rad(2.2f).leaf(.06f, .6f);
	v << T("Bottle tree").z(R|Te).w(2).plan(PLAN_Bottle).x(X_Swell).fol(FOLIAGE_Narrow).shape(.5f, 35, .55f).rad(1.6f);
	v << T("Dragon blood tree").z(R).w(2).plan(PLAN_Umbrella).fol(FOLIAGE_Blade).lk(LEAF_Rosette).shape(.4f, 28, .75f).grav(.12f, .05f, .05f).limbs(4).leaf(.12f, 0);
	v << T("Joshua tree").z(Te|R).w(2).fol(FOLIAGE_Blade).lk(LEAF_Rosette).x(X_Gnarl).mat(TILE_RedFibre).shape(.35f, 45, .8f).lv(2).limbs(3).leaf(.14f, 0);
	v << T("Saguaro").z(R|Te).w(3).forms(F_Both).b(B_Cactus).mat(TILE_Succulent).p(3, 0);
	v << T("Candelabra euphorbia").z(R).w(2).b(B_Cactus).mat(TILE_Succulent).p(8, 1);
	v << T("Prickly pear").z(R|Te).w(3).forms(F_Both).b(B_Cactus).mat(TILE_Succulent).p(6, 2);
	v << T("Yucca").z(R|Te).w(3).forms(F_Both).fol(FOLIAGE_Blade).lk(LEAF_Rosette).mat(TILE_RedFibre).lv(1).limbs(2).shape(.6f, 25, .5f).leaf(.25f, 0);
	v << T("Olive").z(Te|R).w(4).forms(F_Both).fol(FOLIAGE_Narrow).x(X_Gnarl).hue(85).shape(.25f, 50, .6f).rad(1.4f);
	v << T("Orange").z(R|Te).w(4).forms(F_Both).fol(FOLIAGE_Glossy).x(X_Fruit).hue(-1, 30).shape(.2f, 50, .55f);
	v << T("Dead snag").z(Z_All).w(2).x(X_Bare|X_Gnarl).mat(TILE_Grey).shape(.4f, 40, .6f).lv(2);
	v << T("Wind-swept cypress").z(Te).w(2).fol(FOLIAGE_Scale).lk(6).x(X_Wind|X_Gnarl).shape(.3f, 50, .7f);

	v << T("Parasol fungus").fam(FAM_Fungus).forms(F_Both).plan(PLAN_Fungal).b(B_Fungus).p(0, 1);
	v << T("Bell cap").fam(FAM_Fungus).forms(F_Both).plan(PLAN_Fungal).b(B_Fungus).p(2, 1);
	v << T("Morel spire").fam(FAM_Fungus).plan(PLAN_Fungal).b(B_Fungus).p(3, 1);
	v << T("Puffball cluster").fam(FAM_Fungus).forms(F_Both).plan(PLAN_Fungal).b(B_Stalks).mat(TILE_Pale).p(5, 1.6f);
	v << T("Bracket tower").fam(FAM_Fungus).plan(PLAN_Fungal).b(B_Shelf).mat(TILE_Grey);
	v << T("Coral fungus").fam(FAM_Fungus).forms(F_Both).plan(PLAN_Fungal).x(X_Bare).mat(3).shape(.15f, 35, .7f).grav(.2f, .15f, .1f).rad(1.5f);

	v << T("Crystal spire").fam(FAM_Crystal).forms(F_Both).plan(PLAN_Crystal).b(B_Crystal).p(0);
	v << T("Crystal bloom").fam(FAM_Crystal).forms(F_Both).plan(PLAN_Crystal).b(B_Crystal).p(1);
	v << T("Prism tree").fam(FAM_Crystal).plan(PLAN_Crystal).x(X_Faceted).fol(FOLIAGE_Shards).mat(3).shape(.35f, 40, .6f);
	v << T("Obsidian obelisk").fam(FAM_Crystal).plan(PLAN_Crystal).b(B_Crystal).p(2).mat(TILE_Obsidian).x(X_Glow);

	v << T("Corkscrew tree").fam(FAM_Spiral).plan(PLAN_Spiral).tw(.6f).fol(FOLIAGE_Tendrils).shape(.35f, 40, .65f);
	v << T("Double helix").fam(FAM_Spiral).plan(PLAN_Spiral).b(B_Helix).fol(FOLIAGE_Tendrils);
	v << T("Fiddlehead tree").fam(FAM_Spiral).plan(PLAN_Spiral).x(X_Curl).fol(FOLIAGE_Tendrils).shape(.3f, 50, .7f);
	v << T("Coil bush").fam(FAM_Spiral).forms(F_Both).plan(PLAN_Spiral).x(X_Curl).tw(.4f).fol(FOLIAGE_Plume);

	v << T("Lantern tree").fam(FAM_Glow).plan(PLAN_Lantern).x(X_Pods).fol(FOLIAGE_Fins).grav(.05f, -.25f, -.5f);
	v << T("Chandelier tree").fam(FAM_Glow).plan(PLAN_Lantern).x(X_Pods).lk(7).fol(FOLIAGE_Narrow).shape(.5f, 60, .85f).grav(.1f, -.3f, -.9f);
	v << T("Torch tree").fam(FAM_Glow).forms(F_Both).plan(PLAN_Lantern).b(B_Stalks).p(1, 1.f).x(X_Glow);
	v << T("Firefly tree").fam(FAM_Glow).plan(PLAN_Lantern).lk(LEAF_Orb).x(X_Glow).shape(.3f, 45, .65f).leaf(.04f, .6f);
	v << T("Vein tree").fam(FAM_Glow).plan(PLAN_Lantern).mat(TILE_Obsidian).fol(FOLIAGE_Fins).x(X_Glow).shape(.35f, 45, .6f);

	v << T("Coral tree").fam(FAM_Marine).forms(F_Both).x(X_Bare).mat(TILE_Coral).shape(.2f, 45, .65f).grav(.15f, .1f, .05f).rad(1.3f);
	v << T("Anemone tree").fam(FAM_Marine).b(B_Anemone).mat(TILE_Flesh).x(X_Glow);
	v << T("Kelp tower").fam(FAM_Marine).forms(F_Both).b(B_Kelp).fol(FOLIAGE_Fins);
	v << T("Sponge tower").fam(FAM_Marine).forms(F_Both).b(B_Tubes).mat(TILE_Coral).p(0);
	v << T("Tube worm cluster").fam(FAM_Marine).forms(F_Both).b(B_Tubes).mat(TILE_Bone).fol(FOLIAGE_Plume).p(1);

	v << T("Tentacle tree").fam(FAM_Flesh).x(X_Bare|X_Curl).mat(TILE_Flesh).shape(.3f, 50, .8f).grav(0, -.2f, -.4f).rad(1.2f);
	v << T("Eye-stalk tree").fam(FAM_Flesh).forms(F_Both).b(B_Stalks).mat(TILE_Flesh).p(7, .7f);
	v << T("Bone tree").fam(FAM_Flesh).x(X_Bare).mat(TILE_Bone).shape(.35f, 45, .6f);
	v << T("Pod tree").fam(FAM_Flesh).x(X_Bigpods).fol(FOLIAGE_Glossy).mat(TILE_Flesh);
	v << T("Writhing tree").fam(FAM_Flesh).forms(F_Both).x(X_Curl|X_Bare).mat(TILE_Flesh).lv(2).stems(3).tw(.3f);

	v << T("Balloon tree").fam(FAM_Air).forms(F_Both).b(B_Stalks).mat(TILE_Pale).p(5, 1.8f);
	v << T("Cloud tree").fam(FAM_Air).lk(LEAF_Puff).fol(FOLIAGE_Bubbles).shape(.4f, 45, .65f).leaf(.18f, .8f);
	v << T("Dandelion tree").fam(FAM_Air).forms(F_Both).lk(LEAF_PlumeBall).fol(FOLIAGE_Plume).lv(2).shape(.5f, 30, .7f).leaf(.2f, 1.f);
	v << T("Feather tree").fam(FAM_Air).fol(FOLIAGE_Plume).exc().shape(.15f, 35, .5f).grav(.3f, .1f, 0);
	v << T("Ribbon tree").fam(FAM_Air).lk(LEAF_Ribbon).fol(FOLIAGE_Fins).shape(.4f, 45, .65f).leaf(.4f, 1.2f);

	v << T("Disc pagoda").fam(FAM_Geometric).b(B_Pagoda).mat(3);
	v << T("Membrane umbrella").fam(FAM_Geometric).b(B_Membrane).fol(FOLIAGE_Fins);
	return v;
}

const QVector<TreeType>& Types()
{
	static const QVector<TreeType> types = MakeTypes();
	return types;
}

int TypeIndex(const QString& name)
{
	const QVector<TreeType>& t = Types();
	for(int i=0; i<t.count(); i++)
		if(t[i].m_Name.compare(name, Qt::CaseInsensitive) == 0)
			return i;
	for(int i=0; i<t.count(); i++)
		if(t[i].m_Name.contains(name, Qt::CaseInsensitive))
			return i;
	return -1;
}

int PickType(Rng& r, const AuFloraDNA& dna, const World& world, int zone, bool tree)
{
	const QVector<TreeType>& types = Types();
	const int zoneBit = 1 << zone;
	const int formBit = tree ? F_Tree : F_Shrub;

	bool alien = false;
	if(world.m_AlienTheme[0] > FAM_Earth)
		alien = r.Chance(powf(world.m_Strangeness, 1.5f) * (tree ? .55f : .65f));
	const int family = alien ? (world.m_AlienTheme[1] > FAM_Earth && r.Chance(.4f) ? world.m_AlienTheme[1]
			: world.m_AlienTheme[0]) : FAM_Earth;

	float total = 0;
	QVector<float> weights(types.count());
	for(int i=0; i<types.count(); i++)
	{
		const TreeType& t = types[i];
		float w = 0;
		if(t.m_Family == family && (t.m_Forms & formBit) && (family != FAM_Earth || (t.m_Zones & zoneBit)))
		{
			w = t.m_Weight;
			if(dna.Inflorescence == AuFloraDNA::INFL_Spike && (t.m_Builder == B_Palm || t.m_LeafKind == LEAF_Rosette ||
					t.m_Builder == B_Cactus))
				w *= 3;
			if((dna.Inflorescence == AuFloraDNA::INFL_UmbelSimple || dna.Inflorescence == AuFloraDNA::INFL_UmbelCompound) &&
					t.m_Plan == PLAN_Umbrella)
				w *= 3;
			if(dna.RootType == AuFloraDNA::Bulb && (t.m_Extras & X_Swell))
				w *= 4;
			if(dna.RootType == AuFloraDNA::Prop && (t.m_Extras & (X_Stilts|X_Aerial|X_Buttress)))
				w *= 4;
			if(dna.Stiffness <= 1 && t.m_LeafKind == 7)
				w *= 2;
			if(dna.FlowerProbability >= 3 && t.m_Foliage == FOLIAGE_Blossom)
				w *= 2;
		}
		weights[i] = w;
		total += w;
	}
	if(total <= 0)
		return 0;
	float pick = r.Unit() * total;
	for(int i=0; i<types.count(); i++)
	{
		if(pick < weights[i])
			return i;
		pick -= weights[i];
	}
	return types.count() - 1;
}

static QPoint TileAt(int index, int tileSize)
{
	return QPoint((index / FT_PaletteHeight) * tileSize, (index % FT_PaletteHeight) * tileSize);
}

static void MaterialPlace(int material, int& tile, int& quarter)
{
	quarter = -1;
	if(material >= TILE_Bark0)
	{
		tile = 58 + (material - TILE_Bark0) / 4;
		quarter = (material - TILE_Bark0) % 4;
		return;
	}
	switch(material)
	{
	case TILE_Ringed:		tile = 54; quarter = 0; break;
	case TILE_Succulent:	tile = 54; quarter = 1; break;
	case TILE_Birch:		tile = 54; quarter = 2; break;
	case TILE_Peeling:		tile = 54; quarter = 3; break;
	case TILE_RedFibre:		tile = 55; quarter = 0; break;
	case TILE_Grey:			tile = 55; quarter = 1; break;
	case TILE_Rainbow:		tile = 55; quarter = 2; break;
	case TILE_Bone:			tile = 55; quarter = 3; break;
	case TILE_Coral:		tile = 56; quarter = 0; break;
	case TILE_Flesh:		tile = 56; quarter = 1; break;
	case TILE_Obsidian:		tile = 56; quarter = 2; break;
	case TILE_Pale:			tile = 56; quarter = 3; break;
	case TILE_Special:		tile = 57; break;
	case TILE_Swatch0:		tile = 60; break;
	case TILE_Swatch1:		tile = 61; break;
	case TILE_Swatch2:		tile = 62; break;
	default:				tile = 63; break;
	}
}

QRect MaterialPixels(int material, int tileSize)
{
	int tile, quarter;
	MaterialPlace(material, tile, quarter);
	const QPoint at = TileAt(tile, tileSize);
	if(quarter < 0)
		return QRect(at, QSize(tileSize, tileSize));
	const int h = tileSize / 2;
	return QRect(at + QPoint((quarter % 2) * h, (quarter / 2) * h), QSize(h, h));
}

Tile MaterialRect(int material, int tileSize)
{
	int tile, quarter;
	MaterialPlace(material, tile, quarter);
	const Tile whole = Tile::Palette(tile, tileSize);
	if(quarter < 0)
		return whole;
	const float w = (whole.u - whole.s) / 2.f, h = (whole.v - whole.t) / 2.f;
	const float inset = 1.f / (FT_PaletteWidth * tileSize);
	const float s = whole.s + w * (quarter % 2), t = whole.t + h * (quarter / 2);
	return Tile(s + inset, t - inset, s + w - inset, t + h + inset);
}

template<typename F> static void Texels(QImage& im, F f)
{
	const int sz = im.width();
	for(int y=0; y<sz; y++)
	{
		QRgb* line = (QRgb*)im.scanLine(y);
		for(int x=0; x<sz; x++)
			line[x] = f((float)x / sz, (float)y / sz);
	}
}

static QRgb Rgb(const QColor& c, float shade)
{
	return qRgb(qBound(0, (int)(c.red() * shade), 255), qBound(0, (int)(c.green() * shade), 255),
			qBound(0, (int)(c.blue() * shade), 255));
}

static QColor Mixc(const QColor& a, const QColor& b, float f)
{
	f = qBound(0.f, f, 1.f);
	return QColor((int)(a.red() + (b.red() - a.red()) * f), (int)(a.green() + (b.green() - a.green()) * f),
			(int)(a.blue() + (b.blue() - a.blue()) * f));
}

void PaintMaterials(QImage& atlas, int tileSize, const World& world, quint32 seed)
{
	QImage im;
	QPainter p(&atlas);
	p.setCompositionMode(QPainter::CompositionMode_Source);
	auto put = [&](int material) { p.drawImage(MaterialPixels(material, tileSize).topLeft(), im); };
	auto size = [&](int material)
	{
		const QRect r = MaterialPixels(material, tileSize);
		im = QImage(r.width(), r.height(), QImage::Format_ARGB32);
	};
	const float s = world.m_Strangeness;
	const QColor tint = world.m_BarkTint;
	auto tinted = [&](const QColor& c) { return s > .4f ? Mixc(c, tint, .5f) : c; };

	{
		const QImage strip(":data/txBark.png");
		for(int bk=0; bk<8; bk++)
		{
			size(TILE_Bark0 + bk);
			PaintBark(im, strip, bk, world, seed);
			put(TILE_Bark0 + bk);
		}
	}
	size(TILE_Ringed);
	PaintRinged(im, world, seed);
	put(TILE_Ringed);

	{
		const QColor base = tinted(QColor(78, 122, 64));
		size(TILE_Succulent);
		Texels(im, [&](float x, float y)
		{
			const float rib = .7f + .3f * fabsf(sinf(x * Pi * 12.f));
			const float n = Fbm(x * 4.f, y * 4.f, 4, seed + 31, 3);
			const bool spine = fabsf(sinf(x * Pi * 12.f)) > .97f && Noise(x * 12.f, y * 24.f, 12, seed + 37) > .7f;
			return spine ? qRgb(230, 220, 190) : Rgb(base, rib * (.85f + .3f * n));
		});
		put(TILE_Succulent);
	}
	size(TILE_Birch);
	Texels(im, [&](float x, float y)
	{
		const float mark = Noise(x * 6.f, y * 40.f, 6, seed + 41);
		const float n = Fbm(x * 4.f, y * 4.f, 4, seed + 43, 2);
		if(mark > .78f)
			return Rgb(QColor(40, 36, 32), .8f + .4f * n);
		return Rgb(tinted(QColor(232, 228, 218)), .9f + .12f * n);
	});
	put(TILE_Birch);
	size(TILE_Peeling);
	Texels(im, [&](float x, float y)
	{
		const float a = Fbm(x * 5.f, y * 3.f, 5, seed + 51, 3), b = Fbm(x * 5.f, y * 3.f, 5, seed + 53, 3);
		QColor c = a > .55f ? QColor(214, 200, 160) : (b > .5f ? QColor(150, 140, 120) : QColor(120, 128, 110));
		return Rgb(tinted(c), .85f + .3f * Fbm(x * 16.f, y * 16.f, 16, seed + 55, 2));
	});
	put(TILE_Peeling);
	size(TILE_RedFibre);
	Texels(im, [&](float x, float y)
	{
		const float fib = Noise(x * 48.f, y * 3.f, 48, seed + 61);
		const float n = Fbm(x * 4.f, y * 4.f, 4, seed + 63, 3);
		return Rgb(tinted(QColor(132, 66, 44)), .6f + .35f * fib + .2f * n);
	});
	put(TILE_RedFibre);
	size(TILE_Grey);
	Texels(im, [&](float x, float y)
	{
		const float n = Fbm(x * 4.f, y * 4.f, 4, seed + 71, 4);
		return Rgb(tinted(QColor(150, 150, 146)), .82f + .3f * n);
	});
	put(TILE_Grey);
	{
		size(TILE_Rainbow);
		const QColor bands[5] = { QColor(96, 150, 70), QColor(210, 130, 50), QColor(140, 70, 120), QColor(70, 110, 160),
				QColor(200, 180, 90) };
		Texels(im, [&](float x, float y)
		{
			const float f = Fbm(x * 6.f, y * 1.5f, 6, seed + 81, 3) * 5.f;
			const int i = qBound(0, (int)f, 4);
			return Rgb(bands[i], .8f + .3f * Noise(x * 32.f, y * 4.f, 32, seed + 83));
		});
		put(TILE_Rainbow);
	}
	size(TILE_Bone);
	Texels(im, [&](float x, float y)
	{
		const float ridge = .8f + .2f * sinf(y * Pi * 16.f);
		return Rgb(QColor(226, 216, 190), ridge * (.88f + .2f * Fbm(x * 4.f, y * 4.f, 4, seed + 91, 3)));
	});
	put(TILE_Bone);
	{
		size(TILE_Coral);
		const QColor base = Mixc(QColor(230, 120, 110), world.m_MaterialColor[1], s * .5f);
		Texels(im, [&](float x, float y)
		{
			const float pore = Noise(x * 40.f, y * 40.f, 40, seed + 101);
			return Rgb(base, (pore > .72f ? .55f : 1.f) * (.85f + .25f * Fbm(x * 4.f, y * 4.f, 4, seed + 103, 2)));
		});
		put(TILE_Coral);
	}
	{
		size(TILE_Flesh);
		const QColor base = Mixc(QColor(196, 120, 120), world.m_MaterialColor[0], s * .4f);
		Texels(im, [&](float x, float y)
		{
			const float vein = fabsf(Fbm(x * 5.f, y * 5.f, 5, seed + 111, 4) - .5f);
			return vein < .025f ? Rgb(QColor(90, 30, 50), 1.f) : Rgb(base, .85f + .25f * Fbm(x * 8.f, y * 8.f, 8, seed + 113, 2));
		});
		put(TILE_Flesh);
	}
	{
		size(TILE_Obsidian);
		const QColor glow = world.m_MaterialColor[0].lighter(160);
		Texels(im, [&](float x, float y)
		{
			const float vein = fabsf(Fbm(x * 4.f, y * 4.f, 4, seed + 121, 4) - .5f);
			if(vein < .02f)
				return Rgb(glow, 1.f);
			return Rgb(QColor(30, 28, 36), .8f + .4f * Noise(x * 16.f, y * 16.f, 16, seed + 123));
		});
		put(TILE_Obsidian);
	}
	size(TILE_Special);
	PaintSpecial(im, world, seed);
	put(TILE_Special);
	size(TILE_Pale);
	Texels(im, [&](float x, float y)
	{
		const float n = Fbm(x * 3.f, y * 3.f, 3, seed + 131, 3);
		return Rgb(Mixc(QColor(240, 236, 226), world.m_MaterialColor[1], .25f + .3f * s), .85f + .2f * n);
	});
	put(TILE_Pale);
	im.fill(QColor(128, 128, 128));
	put(TILE_Swatch0);
	put(TILE_Swatch1);
	put(TILE_Swatch2);
	p.end();
}

static const int SwatchCols = 5, SwatchRows = 4;

void PaintSwatch(QImage& atlas, int tileSize, int zone, int slot, const QColor& color, quint32 seed)
{
	const QPoint at = MaterialPixels(TILE_Swatch0 + zone, tileSize).topLeft();
	const int cw = tileSize / SwatchCols, ch = tileSize / SwatchRows;
	const int cx = at.x() + (slot % SwatchCols) * cw, cy = at.y() + (slot / SwatchCols) * ch;
	for(int y=0; y<ch; y++)
	{
		QRgb* line = (QRgb*)atlas.scanLine(cy + y);
		for(int x=0; x<cw; x++)
		{
			const float f = (float)y / ch;
			const float n = Noise(x * 8.f / cw, y * 8.f / ch, 8, seed);
			line[cx + x] = Rgb(color, (1.15f - .45f * f) * (.9f + .15f * n));
		}
	}
}

static Tile SwatchCell(const Tile& tile, int slot)
{
	const int col = slot % SwatchCols, row = slot / SwatchCols;
	const float w = (tile.u - tile.s) / SwatchCols, h = (tile.v - tile.t) / SwatchRows;
	const float in = .12f;
	return Tile(tile.s + w * (col + in), tile.t + h * (row + in), tile.s + w * (col + 1 - in), tile.t + h * (row + 1 - in));
}

static const DeFlora& SpecieFlora(const DePlanetFlora& flora, int zone, bool tree, int index)
{
	static const COMMODITYID shrubCIDs[ZONE_SHRUBS] =
	{
		CID_Beans, CID_Fiber, CID_Fruit, CID_Grain, CID_Grapes, CID_Herbs, CID_Hops, CID_Nuts, CID_Spices, CID_Vegetable,
		CID_Hay
	};
	if(tree)
		return flora.Tree((AuFloraDNA::ZONE)zone, DeTerrainFlora::TreeCommodity(index));
	return flora.Shrub((AuFloraDNA::ZONE)zone, shrubCIDs[index]);
}

void PaintZone(QImage& atlas, const DePlanetFlora& flora, quint32 genSeed, int resourceZone, int zone, int tile,
		const World* world, int forceType, int forceFoliage, QVector<QPair<int, QColor>>* swatches)
{
	int seed = genSeed + resourceZone * 100000;
	const int height = FT_PaletteHeight * tile;
	const int plantsPerZone = ZONE_TREES + ZONE_SHRUBS + 1;
	int xpos = 0;
	int ypos = zone * plantsPerZone * tile;
	while(ypos >= height)
	{
		xpos += tile;
		ypos -= height;
	}
	QPainter p(&atlas);
	QImage imbranch(tile, tile, QImage::Format_ARGB32);
	auto place = [&](const QImage& im)
	{
		p.drawImage(xpos, ypos, im);
		ypos += tile;
		if(ypos >= height)
		{
			xpos += tile;
			ypos = 0;
		}
	};

	AuDice dice;
	dice.Seed(seed);
	QImage imgrass(tile, tile, QImage::Format_ARGB32);
	{
		DeFlora grass(flora.GroundCover((AuFloraDNA::ZONE)zone));
		AuDice gdice(dice);
		imgrass.fill(Qt::transparent);
		QPainter pg(&imgrass);
		QImage implant(tile/2, tile/2, QImage::Format_ARGB32);
		const int leafBrush = gdice.Roll(1, 51);
		const int petalBrush = gdice.Roll(1, 30);
		for(int q=0; q<4; q++)
		{
			grass.PaintBranch(implant, (AuFloraDNA::ZONE)zone, gdice, leafBrush, petalBrush);
			gdice.Random();
			pg.drawImage((q & 1) * tile/2, (q >> 1) * tile/2, implant);
		}
		pg.end();
		if(world)
		{
			Traits gt = Traits::Derive(grass.DNA(), *world, zone, false, 99);
			RecolourFoliage(imgrass, gt, genSeed + 99);
		}
	}

	for(int pass=0; pass<2; pass++)
	{
		const bool tree = pass == 1;
		const int count = tree ? ZONE_TREES : ZONE_SHRUBS;
		for(int i=0; i<count; i++)
		{
			dice.Seed(seed += 54321);
			DeFlora f(SpecieFlora(flora, zone, tree, i));
			const int petalBrush = dice.Roll(1, 30);
			const int leafBrush = dice.Roll(1, 51);
			f.PaintBranch(imbranch, (AuFloraDNA::ZONE)zone, dice, leafBrush, petalBrush);
			if(world)
			{
				const Traits t = Traits::Derive(f.DNA(), *world, zone, tree, i, forceType, forceFoliage);
				const quint32 tseed = genSeed + (tree ? 100 : 0) + i;
				PaintFoliage(imbranch, t, tseed);
				if(swatches)
					swatches->append(qMakePair(t.m_Slot, t.m_FruitColor));
			}
			place(imbranch);
		}
	}
	place(imgrass);
	p.end();
}

void PaintPalette(QImage& atlas, const DePlanetFlora& flora, quint32 genSeed, int resourceZone, int tile, const World& world,
		float coverage[3][18], int forceType, int forceFoliage)
{
	atlas = QImage(FT_PaletteWidth * tile, FT_PaletteHeight * tile, QImage::Format_ARGB32);
	atlas.fill(Qt::transparent);
	QVector<QPair<int, QColor>> swatches[AuFloraDNA::Zones];
	for(int zone=0; zone<AuFloraDNA::Zones; zone++)
		PaintZone(atlas, flora, genSeed, resourceZone, zone, tile, &world, forceType, forceFoliage, &swatches[zone]);
	PaintMaterials(atlas, tile, world, genSeed);
	for(int zone=0; zone<AuFloraDNA::Zones; zone++)
		for(const QPair<int, QColor>& s : swatches[zone])
			PaintSwatch(atlas, tile, zone, s.first, s.second, genSeed + s.first);

	for(int zone=0; zone<AuFloraDNA::Zones; zone++)
		for(int slot=0; slot<18; slot++)
		{
			const unsigned int index = zone * 18 + slot;
			coverage[zone][slot] = Coverage(atlas.copy((index / FT_PaletteHeight) * tile, (index % FT_PaletteHeight) * tile,
					tile, tile));
		}

	Dilate(atlas, 12);
}

template<typename F> static void Sprig(QPainter& p, int sz, Rng& r, int shoots, int perShoot, QColor stem, F leaf)
{
	const float w = sz / 512.f;
	const QPointF top(sz / 2.f, 1.f), tip(sz / 2.f + r.Signed() * sz * .06f, sz * .9f);
	p.setPen(QPen(stem, 3.5f * w, Qt::SolidLine, Qt::RoundCap));
	p.drawLine(top, tip);
	for(int i=0; i<shoots; i++)
	{
		const float f = .15f + .75f * (i + .5f) / shoots;
		const QPointF at = top + (tip - top) * f;
		const float side = (i & 1) ? 1.f : -1.f;
		const float reach = sz * .38f * (1.f - .5f * f) * r.Range(.8f, 1.1f);
		const float ang = r.Range(.6f, 1.f);
		const QPointF end = at + QPointF(side * sinf(ang) * reach, cosf(ang) * reach);
		p.setPen(QPen(stem, 2.f * w, Qt::SolidLine, Qt::RoundCap));
		p.drawLine(at, end);
		for(int k=0; k<perShoot; k++)
		{
			const float g = (k + .5f) / perShoot;
			const QPointF lp = at + (end - at) * g;
			const float la = atan2f((float)(end - at).y(), (float)(end - at).x()) + (k & 1 ? .7f : -.7f);
			leaf(lp, la, 1.f - .3f * g);
		}
		leaf(end, atan2f((float)(end - at).y(), (float)(end - at).x()), 1.f);
	}
	leaf(tip, Pi / 2.f, 1.1f);
}

static void Leaf(QPainter& p, const QPointF& at, float ang, float len, float wid, const QColor& c, const QColor& vein, float w)
{
	p.save();
	p.translate(at);
	p.rotate(ang * 180.f / Pi);
	QPainterPath path;
	path.moveTo(0, 0);
	path.cubicTo(len * .3f, -wid, len * .8f, -wid * .6f, len, 0);
	path.cubicTo(len * .8f, wid * .6f, len * .3f, wid, 0, 0);
	QLinearGradient g(0, -wid, 0, wid);
	g.setColorAt(0, c.lighter(115));
	g.setColorAt(1, c.darker(115));
	p.setPen(Qt::NoPen);
	p.setBrush(g);
	p.drawPath(path);
	p.setPen(QPen(vein, w));
	p.drawLine(QPointF(0, 0), QPointF(len * .9f, 0));
	p.restore();
}

static QColor FlowerColor(const Traits& tr, float shade)
{
	QColor c;
	if(tr.m_Hue2 >= White)
		c.setHsvF(0, 0, qBound(0.f, .75f + .25f * shade, 1.f));
	else
		c.setHsvF(fmodf(tr.m_Hue2 + 360.f, 360.f) / 360.f, .6f, qBound(0.f, .6f + .4f * shade, 1.f));
	return c;
}

void PaintTypeFoliage(QImage& tile, const Traits& tr, quint32 seed)
{
	const int sz = tile.width();
	tile.fill(Qt::transparent);
	Rng r(Mix(seed, 0xF011 + tr.m_Foliage));
	QPainter p(&tile);
	p.setRenderHint(QPainter::Antialiasing);
	const float w = sz / 512.f;
	const QColor stem = Mixc(QColor(90, 70, 50), tr.m_BarkTint, .3f);
	switch(tr.m_Foliage)
	{
	case FOLIAGE_Round:
		Sprig(p, sz, r, 9, 5, stem, [&](QPointF at, float ang, float k)
		{
			const float s = sz * .05f * k * r.Range(.8f, 1.2f);
			Leaf(p, at, ang + r.Signed() * .5f, s * 1.5f, s * .75f, LeafColor(tr, .4f + .5f * r.Unit(), r.Signed() * 6.f),
					LeafColor(tr, .2f, 0.f), 1.f * w);
		});
		break;
	case FOLIAGE_Palmate:
		Sprig(p, sz, r, 7, 2, stem, [&](QPointF at, float ang, float k)
		{
			const float s = sz * .1f * k * r.Range(.85f, 1.15f);
			const QColor c = LeafColor(tr, .35f + .5f * r.Unit(), r.Signed() * 8.f);
			for(int lobe=-2; lobe<=2; lobe++)
				Leaf(p, at, ang + lobe * .45f, s * (1.f - .18f * abs(lobe)), s * .32f, c, LeafColor(tr, .2f, 0.f), 1.2f * w);
		});
		break;
	case FOLIAGE_Narrow:
		Sprig(p, sz, r, 10, 4, stem, [&](QPointF at, float ang, float k)
		{
			const float a = ang * .4f + (Pi / 2.f) * .6f + r.Signed() * .3f;
			Leaf(p, at, a, sz * .2f * k * r.Range(.8f, 1.2f), sz * .018f, LeafColor(tr, .35f + .5f * r.Unit(), r.Signed() * 6.f),
					LeafColor(tr, .25f, 0.f), .8f * w);
		});
		break;
	case FOLIAGE_Blossom:
		Sprig(p, sz, r, 8, 3, stem, [&](QPointF at, float ang, float k)
		{
			if(r.Chance(.35f))
			{
				Leaf(p, at, ang + r.Signed() * .6f, sz * .08f * k, sz * .03f, LeafColor(tr, .5f, 0.f), LeafColor(tr, .25f, 0.f), w);
				return;
			}
			const float s = sz * .035f * k * r.Range(.8f, 1.2f);
			const QColor c = FlowerColor(tr, r.Unit());
			p.setPen(Qt::NoPen);
			for(int i=0; i<5; i++)
			{
				const float a = ang + i * 2.f * Pi / 5.f;
				p.setBrush(i & 1 ? c : c.lighter(110));
				p.drawEllipse(at + QPointF(cosf(a) * s, sinf(a) * s), s * .9f, s * .9f);
			}
			p.setBrush(QColor(230, 200, 80));
			p.drawEllipse(at, s * .45f, s * .45f);
		});
		break;
	case FOLIAGE_Compound:
		Sprig(p, sz, r, 6, 1, stem, [&](QPointF at, float ang, float k)
		{
			const float len = sz * .18f * k;
			const QPointF dir(cosf(ang), sinf(ang)), n(-dir.y(), dir.x());
			p.setPen(QPen(LeafColor(tr, .25f, 0.f), 1.2f * w));
			p.drawLine(at, at + dir * len);
			for(int i=1; i<=10; i++)
			{
				const QPointF q = at + dir * (len * i / 10.f);
				for(int sgn=-1; sgn<=1; sgn+=2)
					Leaf(p, q, atan2f((float)(n * sgn + dir * .3).y(), (float)(n * sgn + dir * .3).x()), sz * .025f, sz * .009f,
							LeafColor(tr, .4f + .5f * r.Unit(), r.Signed() * 5.f), LeafColor(tr, .3f, 0.f), .5f * w);
			}
		});
		break;
	case FOLIAGE_Scale:
	{
		std::function<void(QPointF, float, float, int)> spray = [&](QPointF at, float ang, float len, int depth)
		{
			const QPointF dir(cosf(ang), sinf(ang));
			const int steps = 10;
			for(int i=0; i<steps; i++)
			{
				const QPointF q = at + dir * (len * i / steps);
				p.setPen(Qt::NoPen);
				p.setBrush(LeafColor(tr, .3f + .5f * r.Unit(), r.Signed() * 5.f));
				p.drawEllipse(q, len * .08f * (1.f - .4f * i / steps) + 1.f, len * .05f + 1.f);
			}
			if(depth < 2)
				for(int i=1; i<4; i++)
				{
					const QPointF q = at + dir * (len * i / 4.f);
					spray(q, ang + (i & 1 ? .7f : -.7f), len * .5f, depth + 1);
				}
		};
		spray(QPointF(sz / 2.f, 1.f), Pi / 2.f, sz * .85f, 0);
		break;
	}
	case FOLIAGE_Broad:
	{
		const QColor c = LeafColor(tr, .55f, 0.f);
		QPainterPath leaf;
		const float W = sz * .3f;
		leaf.moveTo(sz / 2.f, 2);
		leaf.cubicTo(sz / 2.f - W, sz * .2f, sz / 2.f - W, sz * .8f, sz / 2.f, sz * .98f);
		leaf.cubicTo(sz / 2.f + W, sz * .8f, sz / 2.f + W, sz * .2f, sz / 2.f, 2);
		QLinearGradient g(sz / 2.f - W, 0, sz / 2.f + W, 0);
		g.setColorAt(0, c.darker(120));
		g.setColorAt(.5f, c.lighter(115));
		g.setColorAt(1, c.darker(110));
		p.setPen(Qt::NoPen);
		p.setBrush(g);
		p.drawPath(leaf);
		p.setPen(QPen(LeafColor(tr, .8f, 0.f), 4.f * w));
		p.drawLine(QPointF(sz / 2.f, 2), QPointF(sz / 2.f, sz * .97f));
		p.setCompositionMode(QPainter::CompositionMode_Clear);
		for(int i=0; i<6; i++)
		{
			const float y = sz * r.Range(.25f, .85f);
			const float side = r.Chance(.5f) ? 1.f : -1.f;
			p.setPen(QPen(Qt::transparent, 3.f * w));
			p.drawLine(QPointF(sz / 2.f + side * W * .9f, y), QPointF(sz / 2.f + side * sz * .03f, y - sz * .02f));
		}
		p.setCompositionMode(QPainter::CompositionMode_SourceOver);
		break;
	}
	case FOLIAGE_Fan:
	{
		const QPointF hub(sz / 2.f, sz * .3f);
		p.setPen(QPen(stem, 4.f * w));
		p.drawLine(QPointF(sz / 2.f, 1.f), hub);
		const int pleats = 26;
		for(int i=0; i<pleats; i++)
		{
			const float a0 = Pi * (.1f + .8f * i / pleats), a1 = Pi * (.1f + .8f * (i + 1) / pleats);
			const float rr = sz * .62f * r.Range(.9f, 1.f);
			QPolygonF wedge;
			wedge << hub << hub + QPointF(cosf(a0) * rr, sinf(a0) * rr) << hub + QPointF(cosf(a1) * rr, sinf(a1) * rr);
			p.setPen(Qt::NoPen);
			p.setBrush(LeafColor(tr, i & 1 ? .4f : .65f, 0.f));
			p.drawPolygon(wedge);
		}
		break;
	}
	case FOLIAGE_Blade:
	{
		const float W = sz * .07f;
		QPolygonF blade;
		blade << QPointF(sz / 2.f - W, 2) << QPointF(sz / 2.f + W, 2) << QPointF(sz / 2.f + W * .6f, sz * .7f)
				<< QPointF(sz / 2.f, sz * .98f) << QPointF(sz / 2.f - W * .6f, sz * .7f);
		QLinearGradient g(sz / 2.f - W, 0, sz / 2.f + W, 0);
		g.setColorAt(0, LeafColor(tr, .35f, 0.f));
		g.setColorAt(.5f, LeafColor(tr, .75f, 0.f));
		g.setColorAt(1, LeafColor(tr, .4f, 0.f));
		p.setPen(Qt::NoPen);
		p.setBrush(g);
		p.drawPolygon(blade);
		break;
	}
	case FOLIAGE_Glossy:
		Sprig(p, sz, r, 6, 2, stem, [&](QPointF at, float ang, float k)
		{
			const float s = sz * .11f * k * r.Range(.85f, 1.15f);
			const float a = ang + r.Signed() * .4f;
			Leaf(p, at, a, s, s * .42f, LeafColor(tr, .25f + .35f * r.Unit(), r.Signed() * 5.f), LeafColor(tr, .6f, 0.f), 1.5f * w);
			p.save();
			p.translate(at);
			p.rotate(a * 180.f / Pi);
			p.setPen(Qt::NoPen);
			p.setBrush(QColor(255, 255, 255, 70));
			p.drawEllipse(QPointF(s * .45f, -s * .12f), s * .2f, s * .06f);
			p.restore();
		});
		break;
	case FOLIAGE_Bubbles:
	{
		for(int i=0; i<28; i++)
		{
			const QPointF c(sz * (.5f + r.Signed() * .35f), sz * (.1f + .8f * r.Unit()));
			const float rr = sz * r.Range(.05f, .12f);
			QRadialGradient g(c - QPointF(rr * .3f, rr * .3f), rr * 1.2f);
			QColor in = LeafColor(tr, .9f, r.Signed() * 15.f), out = LeafColor(tr, .4f, 0.f);
			in.setAlpha(230);
			out.setAlpha(200);
			g.setColorAt(0, in);
			g.setColorAt(1, out);
			p.setPen(Qt::NoPen);
			p.setBrush(g);
			p.drawEllipse(c, rr, rr);
		}
		break;
	}
	default:
		break;
	}
	p.end();
}

namespace
{

struct TypeBuild
{
	Builder&							b;
	const TreeType&						t;
	bool								shrub;
	Tile								swatch;
	Tile								pale;
	QVector3D							wind;
	QVector<QPair<QVector3D, float>>	aerial;
};

}

static Limb NewLimb(int depth, int material)
{
	Limb l;
	l.depth = depth;
	l.faceted = false;
	l.tip = false;
	l.material = material;
	l.sidesScale = 1;
	return l;
}

static void TypeBranch(TypeBuild& tb, const Arch& a, int level, const QVector3D& start, const QVector3D& dir0, float length,
		float r0)
{
	Builder& b = tb.b;
	const TreeType& t = tb.t;
	if(b.m_Segments >= b.m_Budget.m_MaxSegments || length <= 0.f)
		return;
	const int levels = b.m_Lod < 2 ? qMax(1, a.levels - 1) : a.levels;
	int segs = level == 0 ? 6 : (level == 1 ? 4 : (level >= levels ? 2 : 3));
	if(t.m_Extras & X_Curl)
		segs += 2;
	if(b.m_Lod < 2)
		segs = qMax(2, segs - 2);

	Limb limb = NewLimb(level, t.m_Material >= 0 ? t.m_Material : 0);
	limb.faceted = (t.m_Extras & X_Faceted) != 0;
	limb.tip = level >= levels;
	QVector3D p = start, d = dir0.normalized();
	limb.nodes.append(Node{p, r0});
	limb.children.append(0);
	const float step = length / segs;
	const float tipR = qMax(r0 * .3f, a.radius * .012f);
	QVector3D curlAxis = Perp(d);
	const float gnarl = (t.m_Extras & X_Gnarl) ? 2.2f : 1.f;
	for(int i=1; i<=segs; i++)
	{
		p += d * step;
		const float f = (float)i / segs;
		limb.nodes.append(Node{p, r0 + (tipR - r0) * f});
		limb.children.append(0);
		b.m_Segments++;
		d = Deflect(d, b.m_Rng.Unit() * a.bend * gnarl, b.m_Rng.Range(0.f, 2.f*Pi));
		d = (d + Up * a.grav[qMin(level, 3)] + tb.wind * (.12f + .05f * level)).normalized();
		if(a.twist != 0.f)
			d = Rotate(d, Up, a.twist);
		if((t.m_Extras & X_Curl) && level >= 1)
		{
			d = Rotate(d, curlAxis, .35f + .5f * f);
			curlAxis = Rotate(curlAxis, d, .2f);
		}
	}
	b.m_Limbs.append(limb);
	if(level == 0 && b.m_TrunkLen <= 0.f)
		b.m_TrunkLen = a.excurrent ? length * a.trunkFrac : length;
	if((t.m_Extras & X_Aerial) && level == 1)
		tb.aerial.append(qMakePair(limb.nodes[segs/2].p, limb.nodes[segs/2].r));

	auto at = [&](float u, QVector3D& pos, QVector3D& dir, float& r)
	{
		const float xx = qBound(0.f, u, 1.f) * segs;
		const int i = qMin(segs - 1, (int)xx);
		const float f = xx - i;
		pos = Lerp(limb.nodes[i].p, limb.nodes[i+1].p, f);
		dir = (limb.nodes[i+1].p - limb.nodes[i].p).normalized();
		r = limb.nodes[i].r + (limb.nodes[i+1].r - limb.nodes[i].r) * f;
	};

	if(level < levels)
	{
		const int n = a.children[level];
		const float tFrom = level == 0 ? (a.excurrent ? a.trunkFrac : .72f) : .3f;
		const float tTo = level == 0 ? (a.excurrent ? .96f : 1.f) : .95f;
		float az = b.m_Rng.Range(0.f, 2.f*Pi);
		const int groups = (level == 0 && a.whorl > 1) ? qMax(1, n / a.whorl) : n;
		const int perGroup = (level == 0 && a.whorl > 1) ? a.whorl : 1;
		for(int g=0; g<groups; g++)
		{
			float u = groups == 1 ? (tFrom + tTo) / 2.f : tFrom + (tTo - tFrom) * g / (groups - 1);
			u = qBound(0.f, u + b.m_Rng.Signed() * .3f / qMax(groups, 1), 1.f);
			QVector3D pos, dir;
			float r;
			at(u, pos, dir, r);
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
						const float h = (u - a.trunkFrac) / qMax(.01f, 1.f - a.trunkFrac);
						clen = a.height * (.42f * powf(qMax(0.f, 1.f - h), .9f) + .06f);
					}
					else
						clen = a.height * (1.f - a.trunkFrac) * b.m_Rng.Range(.75f, 1.f);
				}
				else
					clen = length * a.ratio * (1.15f - .45f * u) * b.m_Rng.Range(.85f, 1.1f);
				const float cr = qMax(r * b.m_Rng.Range(.5f, .68f), a.radius * .01f);
				TypeBranch(tb, a, level + 1, pos - cdir * (r * .5f), cdir, clen, cr);
			}
		}
	}

	const bool twig = level >= levels;
	if(!(t.m_Extras & X_Bare) && (twig || level == levels - 1))
	{
		const float size = a.leafSize * (b.m_Lod < 2 ? 1.5f : 1.f);
		const float spacing = qMax(size * a.leafSpacing * (b.m_Lod < 2 ? 1.6f : 1.f), 1e-4f);
		const float from = twig ? .2f : a.leafFrom;
		const bool endOnly = a.leafKind >= LEAF_Rosette || a.leafSpacing <= 0.f;
		const int count = endOnly ? 0 : qMax(1, (int)((1.f - from) * length / spacing));
		for(int i=0; i<count; i++)
		{
			const float u = from + (1.f - from) * (i + b.m_Rng.Unit() * .6f) / count;
			QVector3D pos, dir;
			float r;
			at(u, pos, dir, r);
			Cluster c;
			c.p = pos;
			c.dir = Deflect(dir, b.m_Rng.Range(.5f, 1.2f), b.m_Rng.Range(0.f, 2.f*Pi));
			c.size = size * b.m_Rng.Range(.8f, 1.2f);
			c.kind = a.leafKind;
			b.m_Clusters.append(c);
		}
		if(twig || endOnly)
		{
			Cluster c;
			c.p = limb.nodes.last().p;
			c.dir = (limb.nodes.last().p - limb.nodes[segs-1].p).normalized();
			c.size = size;
			c.kind = a.leafKind;
			b.m_Clusters.append(c);
		}
	}
	if(twig)
	{
		if((t.m_Extras & X_Fruit) && b.m_Rng.Chance(.45f))
		{
			Cluster c;
			c.p = limb.nodes.last().p - Up * (a.leafSize * .1f);
			c.dir = -Up;
			c.size = a.height * .018f;
			c.kind = 14;
			b.m_Clusters.append(c);
		}
		if((t.m_Extras & X_Pods) && b.m_Rng.Chance(.5f))
		{
			Cluster c;
			c.p = limb.nodes.last().p;
			c.dir = -Up;
			c.size = a.leafSize * .22f;
			c.kind = 1;
			b.m_Clusters.append(c);
		}
	}
	if((t.m_Extras & X_Bigpods) && level == levels - 1 && b.m_Rng.Chance(.6f))
	{
		Cluster c;
		c.p = limb.nodes.last().p;
		c.dir = -Up;
		c.size = a.height * .05f;
		c.kind = 1;
		b.m_Clusters.append(c);
	}
}

static void BuildTypeArch(TypeBuild& tb)
{
	Builder& b = tb.b;
	const TreeType& t = tb.t;
	const AuFloraDNA& dna = b.m_DNA;
	const Traits& tr = b.m_Tr;
	Arch a;
	a.height = b.m_Height > 0.f ? b.m_Height : (tb.shrub ? b.m_CardSize * .8f : GameHeight(dna, b.m_CardSize, 4) * 1.1f);
	a.trunkFrac = qBound(.02f, t.m_TrunkFrac + .03f * ((int)dna.TrunkLength % 4 - 1.5f), .9f);
	a.excurrent = t.m_Excurrent;
	a.stems = t.m_Stems;
	const int mul = dna.Multiples, prob = dna.BranchProbability;
	a.levels = t.m_Levels;
	a.children[0] = t.m_Limbs > 0 ? t.m_Limbs : 3 + mul + (prob > 1 ? 1 : 0);
	a.children[1] = 3 + mul / 2 + (prob > 2 ? 1 : 0);
	a.children[2] = 3 + prob / 2;
	a.children[3] = 2;
	a.whorl = t.m_Whorl;
	if(a.whorl > 1)
		a.children[0] = a.whorl * (b.m_Lod < 2 ? 3 : 6);
	if(a.whorl > 1)
		a.children[1] = 4;
	const float spread = (t.m_Spread + 5.f * ((float)dna.BranchAngle - 1.5f)) * Pi / 180.f;
	a.angle[0] = 0;
	a.angle[1] = spread;
	a.angle[2] = qMin(spread * 1.2f, 1.5f);
	a.angle[3] = qMin(spread * 1.35f, 1.6f);
	a.ratio = t.m_Ratio;
	a.grav[0] = a.excurrent ? .45f : .2f;
	a.grav[1] = t.m_Grav[0];
	a.grav[2] = t.m_Grav[1];
	a.grav[3] = t.m_Grav[2];
	a.twist = t.m_Twist * (tr.m_Twist < 0 ? -1.f : 1.f);
	a.bend = (5.f + 4.f * dna.BranchAngle) * Pi / 180.f;
	a.radius = a.height * .03f * t.m_RadiusMul;
	a.leafSize = a.height * t.m_LeafSize * (1.f + .08f * ((float)dna.LeafSize - 3.5f) / 3.5f);
	a.leafSpacing = t.m_LeafSpacing;
	a.leafKind = t.m_LeafKind;
	a.leafFrom = a.leafKind == 8 ? .3f : .75f;
	a.pods = false;
	a.twigWood = a.leafKind != 8;
	if(a.leafKind == 8)
		a.children[1] = 4;

	if(tb.shrub)
	{
		a.stems = qBound(2, (int)dna.Multiples + (int)dna.SproutMultiples / 3 + 1, b.m_Lod < 2 ? 2 : 4);
		a.levels = qMin(a.levels, 2);
		a.excurrent = false;
		a.trunkFrac = .25f;
		a.children[0] = 3;
		a.children[1] = 2;
		a.whorl = 0;
		a.radius = a.height * .025f * qMin(t.m_RadiusMul, 1.5f);
		a.leafSize = a.height * (t.m_LeafSize * 1.8f + .03f);
	}
	if(tr.m_Foliage == FOLIAGE_Game)
		a.leafSize *= qBound(1.f, sqrtf(.3f / qMax(.02f, tr.m_Coverage)), 2.f);
	if(t.m_Extras & X_Wind)
		tb.wind = QVector3D(cosf(tr.m_Lean * 50.f), sinf(tr.m_Lean * 50.f), 0) * .9f;

	QVector3D lean = Deflect(Up, tr.m_Lean * ((t.m_Extras & X_Leaning) ? 4.f : 1.f), b.m_Rng.Range(0.f, 2.f*Pi));
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
		TypeBranch(tb, a, 0, base, dir, len, r);
	}

	if((t.m_Extras & X_Stilts) && !b.m_Limbs.isEmpty())
	{
		const int roots = b.m_Lod < 2 ? 5 : 9;
		const float h = a.height * a.trunkFrac * .45f;
		for(int i=0; i<roots; i++)
		{
			const float ang = 2.f*Pi * i / roots + b.m_Rng.Signed() * .3f;
			const QVector3D out(cosf(ang), sinf(ang), 0);
			const float spread = a.height * b.m_Rng.Range(.15f, .25f);
			Limb root = NewLimb(1, t.m_Material >= 0 ? t.m_Material : 0);
			const int segs = 5;
			for(int k=0; k<=segs; k++)
			{
				const float f = (float)k / segs;
				const QVector3D pp = Up * (h * (1.f - f * f) * b.m_Rng.Range(.8f, 1.f)) + out * (spread * sinf(f * Pi * .5f))
						- Up * (f > .9f ? a.radius * .5f : 0.f);
				root.nodes.append(Node{pp, a.radius * .35f * (1.f - .4f * f)});
				root.children.append(0);
			}
			b.m_Limbs.append(root);
		}
	}
	for(const QPair<QVector3D, float>& ar : tb.aerial)
	{
		const int strands = b.m_Lod < 2 ? 1 : 3;
		for(int i=0; i<strands; i++)
		{
			const QVector3D top = ar.first + QVector3D(b.m_Rng.Signed(), b.m_Rng.Signed(), 0) * ar.second * 2.f;
			if(top.z() <= 0.f)
				continue;
			Limb root = NewLimb(2, t.m_Material >= 0 ? t.m_Material : 0);
			root.nodes.append(Node{top, ar.second * .25f});
			root.nodes.append(Node{QVector3D(top.x(), top.y(), -a.radius * .3f), ar.second * .3f});
			root.children << 0 << 0;
			b.m_Limbs.append(root);
		}
	}
}

static void SwatchBall(TypeBuild& tb, const QVector3D& c, float r, float move)
{
	tb.b.Ball(c, r, tb.swatch, move);
}

static void BuildTypePalm(TypeBuild& tb)
{
	Builder& b = tb.b;
	const TreeType& t = tb.t;
	const AuFloraDNA& dna = b.m_DNA;
	const float H = b.m_Height > 0.f ? b.m_Height : GameHeight(dna, b.m_CardSize, 4) * 1.1f;
	const float frond = H * t.m_LeafSize * (tb.shrub ? 1.6f : 1.f);
	const float trunkH = qMax(H * .08f, H - frond * .55f);
	Style st;
	st.taperMul = .25f;
	st.bendDeg = 3.f;
	st.grav = .1f;
	const QVector3D lean = Deflect(Up, b.m_Tr.m_Lean * ((t.m_Extras & X_Leaning) ? 4.f : 1.5f), b.m_Rng.Range(0.f, 2.f*Pi));
	const float r = H * .025f * (t.m_Name == "Cycad" || t.m_Name == "Banana" ? 2.f : 1.f);
	const int segs = 5;
	st.lengthMul = trunkH / (segs * qMax(.001f, dna.BranchLength(r * 2.f)));
	QVector3D top(0, 0, 0);
	if(!tb.shrub || t.m_Name != "Cycad")
	{
		const int trunk = b.Grow(QVector3D(0, 0, 0), lean, r * 2.f, segs, 0.f, 0, st);
		if(trunk >= 0)
		{
			b.m_Limbs[trunk].material = t.m_Material >= 0 ? t.m_Material : TILE_Ringed;
			top = b.m_Limbs[trunk].nodes.last().p;
		}
	}
	const int fronds = qMin(20, (int)((7 + (int)dna.Multiples * 2 + (int)dna.SproutMultiples / 2) * t.m_P1));
	float az = b.m_Rng.Range(0.f, 2.f*Pi);
	for(int i=0; i<fronds; i++)
	{
		az += GoldenAngle;
		const float age = (float)i / qMax(1, fronds);
		const QVector3D d = Deflect(Up, .3f + age * .9f + b.m_Rng.Signed() * .12f, az);
		Cluster c;
		c.p = top;
		c.dir = d;
		c.size = frond * (1.f - age * .2f) * b.m_Rng.Range(.9f, 1.1f);
		if(t.m_LeafKind == 4)
			c.kind = 4;
		else
		{
			Limb stalk = NewLimb(2, 0);
			const QVector3D end = top + d * (c.size * .45f);
			stalk.nodes << Node{top, r * .2f} << Node{end, r * .12f};
			stalk.children << 0 << 0;
			b.m_Limbs.append(stalk);
			c.p = end;
			c.kind = 5;
			c.size *= .7f;
		}
		b.m_Clusters.append(c);
	}
	b.m_FrondDroop = t.m_P2;
}

static void BuildTypeCactus(TypeBuild& tb)
{
	Builder& b = tb.b;
	const TreeType& t = tb.t;
	const float H = b.m_Height > 0.f ? b.m_Height : b.m_CardSize;
	const int style = (int)t.m_P2;
	const int mat = t.m_Material >= 0 ? t.m_Material : TILE_Succulent;
	auto column = [&](const QVector3D& from, const QVector3D& dir0, float len, float r, float upTurn, int depth)
	{
		Limb l = NewLimb(depth, mat);
		l.sidesScale = 1.6f;
		QVector3D p = from, d = dir0.normalized();
		const int segs = 6;
		l.nodes.append(Node{p, r});
		l.children.append(0);
		for(int i=1; i<=segs; i++)
		{
			p += d * (len / segs);
			l.nodes.append(Node{p, r * (i == segs ? .8f : 1.f)});
			l.children.append(0);
			d = (d + Up * upTurn).normalized();
		}
		b.m_Limbs.append(l);
		return p;
	};
	if(style == 2)
	{
		QVector<QPair<QVector3D, QVector3D>> open;
		open.append(qMakePair(QVector3D(0, 0, 0), Up));
		const int pads = qMax(3, (int)t.m_P1 * (tb.shrub ? 1 : 2));
		const float padH = H / 3.2f;
		for(int i=0; i<pads && !open.isEmpty(); i++)
		{
			const QPair<QVector3D, QVector3D> at = open.takeFirst();
			const QVector3D d = at.second;
			const QVector3D c = at.first + d * (padH * .5f);
			b.Pad(c, d, padH * .5f, padH * .38f, padH * .09f, b.MaterialTile(mat));
			for(int k=0; k<2; k++)
			{
				const QVector3D nd = Deflect(d, b.m_Rng.Range(.3f, .8f), b.m_Rng.Range(0.f, 2.f*Pi));
				open.append(qMakePair(at.first + d * (padH * .9f), QVector3D(nd.x(), nd.y(), qMax(.2f, nd.z())).normalized()));
			}
		}
		return;
	}
	if(style == 3)
	{
		b.Ball(QVector3D(0, 0, H * .35f), H * .35f, b.MaterialTile(mat), 0.f);
		return;
	}
	const float r = H * (style == 0 ? .07f : .045f);
	const QVector3D top = column(QVector3D(0, 0, -r * .3f), Up, style == 0 ? H : H * .35f, r, 0.f, 0);
	Q_UNUSED(top);
	const int arms = (int)t.m_P1;
	float az = b.m_Rng.Range(0.f, 2.f*Pi);
	for(int i=0; i<arms; i++)
	{
		az += style == 0 ? GoldenAngle : 2.f*Pi / arms;
		const float h = style == 0 ? H * b.m_Rng.Range(.35f, .65f) : H * b.m_Rng.Range(.2f, .35f);
		const QVector3D out(cosf(az), sinf(az), 0);
		const QVector3D from = Up * h;
		column(from, (out + Up * .1f).normalized(), H * (style == 0 ? .14f : .12f), r * (style == 0 ? .75f : .9f), .55f, 1);
		const QVector3D bend = from + out * (H * (style == 0 ? .14f : .12f)) + Up * (H * .05f);
		column(bend, Up, H * (style == 0 ? b.m_Rng.Range(.25f, .4f) : b.m_Rng.Range(.45f, .7f)), r * (style == 0 ? .75f : .9f), 0.f, 1);
	}
}

static void BuildTypeBamboo(TypeBuild& tb)
{
	Builder& b = tb.b;
	const TreeType& t = tb.t;
	const float H = b.m_Height > 0.f ? b.m_Height : b.m_CardSize;
	const int canes = qMax(4, (int)(t.m_P1 * (tb.shrub ? .5f : 1.f) * (b.m_Lod < 2 ? .5f : 1.f)));
	for(int i=0; i<canes; i++)
	{
		const float a = b.m_Rng.Range(0.f, 2.f*Pi), off = H * .06f * sqrtf(b.m_Rng.Unit());
		const QVector3D base(cosf(a) * off, sinf(a) * off, 0);
		const QVector3D d = Deflect(Up, b.m_Rng.Range(.02f, .15f), a);
		const float h = H * b.m_Rng.Range(.7f, 1.f);
		Limb cane = NewLimb(1, t.m_Material >= 0 ? t.m_Material : TILE_Ringed);
		const int segs = 6;
		QVector3D p = base, dir = d;
		cane.nodes.append(Node{p, H * .012f});
		cane.children.append(0);
		for(int k=1; k<=segs; k++)
		{
			p += dir * (h / segs);
			dir = (dir + QVector3D(cosf(a), sinf(a), 0) * .04f).normalized();
			cane.nodes.append(Node{p, H * .011f});
			cane.children.append(0);
			if(k >= 2)
			{
				Cluster c;
				c.p = p;
				c.dir = Deflect(dir, b.m_Rng.Range(.6f, 1.2f), b.m_Rng.Range(0.f, 2.f*Pi));
				c.size = H * .14f;
				c.kind = 5;
				b.m_Clusters.append(c);
			}
		}
		b.m_Limbs.append(cane);
	}
}

static void BuildTypeStalks(TypeBuild& tb)
{
	Builder& b = tb.b;
	const TreeType& t = tb.t;
	const float H = b.m_Height > 0.f ? b.m_Height : b.m_CardSize;
	const int stalks = qMax(1, (int)t.m_P1);
	const bool pale = t.m_Material == TILE_Pale;
	for(int i=0; i<stalks; i++)
	{
		const float a = b.m_Rng.Range(0.f, 2.f*Pi);
		const float off = stalks == 1 ? 0.f : H * .12f * sqrtf(b.m_Rng.Unit());
		const QVector3D base(cosf(a) * off, sinf(a) * off, 0);
		const float h = H * (stalks == 1 ? .85f : b.m_Rng.Range(.4f, .85f));
		const float ballR = H * .07f * t.m_P2 * (stalks == 1 ? 1.4f : b.m_Rng.Range(.7f, 1.1f));
		Style st;
		st.bendDeg = 8;
		st.grav = .12f;
		st.taperMul = .1f;
		const float r = H * .015f * (stalks == 1 ? 2.f : 1.f);
		st.lengthMul = (h - ballR) / (4.f * qMax(.001f, b.m_DNA.BranchLength(r * 2.f)));
		const int l = b.Grow(base, Deflect(Up, stalks == 1 ? 0.f : b.m_Rng.Range(.1f, .4f), a), r * 2.f, 4, 0.f, 1, st);
		if(l < 0)
			continue;
		b.m_Limbs[l].material = t.m_Material >= 0 && !pale ? t.m_Material : TILE_Grey;
		const QVector3D top = b.m_Limbs[l].nodes.last().p;
		const QVector3D centre = top + Up * ballR * .9f;
		const float move = qMax(0.f, centre.z()) * b.m_WindLeaf;
		if(pale)
			b.Ball(centre, ballR, tb.pale, move);
		else
			SwatchBall(tb, centre, ballR, move);
	}
}

static void Disc(Builder& b, const QVector3D& c, float R, float thick, const Tile& tile, float a0 = 0.f, float a1 = 2.f*Pi)
{
	const int sides = b.m_Lod < 2 ? 8 : b.Res(18, 8);
	const float move = qMax(0.f, c.z()) * b.m_WindWood * .5f;
	const Tile top = tile.Band(0.f, .5f), edge = tile.Band(.5f, 1.f);
	for(int k=0; k<sides; k++)
	{
		const float x0 = a0 + (a1 - a0) * k / sides, x1 = a0 + (a1 - a0) * (k + 1) / sides;
		const QVector3D d0(cosf(x0), sinf(x0), 0), d1(cosf(x1), sinf(x1), 0);
		const QVector3D p0 = c + d0 * R, p1 = c + d1 * R;
		const float u0 = tile.s + (tile.u - tile.s) * k / sides, u1 = tile.s + (tile.u - tile.s) * (k + 1) / sides;
		b.Tri(Builder::Vert(c + Up * thick, Up, (u0 + u1) / 2.f, top.t, move), Builder::Vert(p0 + Up * thick * .3f, Up, u0, top.v, move),
				Builder::Vert(p1 + Up * thick * .3f, Up, u1, top.v, move));
		b.Tri(Builder::Vert(c, -Up, (u0 + u1) / 2.f, top.t, move), Builder::Vert(p1, -Up, u1, top.v, move),
				Builder::Vert(p0, -Up, u0, top.v, move));
		b.Tri(Builder::Vert(p0, d0, u0, edge.t, move), Builder::Vert(p1, d1, u1, edge.t, move),
				Builder::Vert(p1 + Up * thick * .3f, d1, u1, edge.v, move));
		b.Tri(Builder::Vert(p1 + Up * thick * .3f, d1, u1, edge.v, move), Builder::Vert(p0 + Up * thick * .3f, d0, u0, edge.v, move),
				Builder::Vert(p0, d0, u0, edge.t, move));
	}
}

static void BuildTypeShelfOrPagoda(TypeBuild& tb, bool pagoda)
{
	Builder& b = tb.b;
	const TreeType& t = tb.t;
	const float H = b.m_Height > 0.f ? b.m_Height : b.m_CardSize;
	const float r = H * (pagoda ? .025f : .05f);
	Limb trunk = NewLimb(0, pagoda ? TILE_Grey : (t.m_Material >= 0 ? t.m_Material : 0));
	const int segs = 5;
	for(int k=0; k<=segs; k++)
	{
		trunk.nodes.append(Node{QVector3D(b.m_Rng.Signed() * r * .3f * (k > 0), b.m_Rng.Signed() * r * .3f * (k > 0), H * k / segs),
				r * (1.f - .3f * k / segs)});
		trunk.children.append(0);
	}
	if(!pagoda)
		trunk.tip = false;
	b.m_Limbs.append(trunk);
	const Tile mat = b.MaterialTile(3);
	const int tiers = pagoda ? 4 + (int)b.m_DNA.Multiples : 6 + (int)b.m_DNA.Multiples * 2;
	for(int i=0; i<tiers; i++)
	{
		const float f = (i + .5f) / tiers;
		const float z = H * (pagoda ? .3f + .65f * f : .1f + .8f * f);
		if(pagoda)
			Disc(b, QVector3D(0, 0, z), H * (.38f - .25f * f), H * .02f, mat);
		else
		{
			const float a = b.m_Rng.Range(0.f, 2.f*Pi);
			const QVector3D side(cosf(a), sinf(a), 0);
			Disc(b, QVector3D(0, 0, z) + side * r * .6f, H * b.m_Rng.Range(.07f, .14f), H * .02f, mat, a - 1.6f, a + 1.6f);
		}
	}
}

static void BuildTypeAnemone(TypeBuild& tb)
{
	Builder& b = tb.b;
	const TreeType& t = tb.t;
	const float H = b.m_Height > 0.f ? b.m_Height : b.m_CardSize;
	const int mat = t.m_Material >= 0 ? t.m_Material : TILE_Flesh;
	Limb stalk = NewLimb(0, mat);
	stalk.sidesScale = 1.4f;
	const float r = H * .06f;
	const float top = H * .6f;
	for(int k=0; k<=4; k++)
	{
		stalk.nodes.append(Node{QVector3D(0, 0, top * k / 4.f), r * (1.f + .3f * (k == 4))});
		stalk.children.append(0);
	}
	stalk.tip = false;
	b.m_Limbs.append(stalk);
	const int arms = b.m_Lod < 2 ? 8 : 16;
	for(int i=0; i<arms; i++)
	{
		const float a = 2.f*Pi * i / arms + b.m_Rng.Signed() * .2f;
		const QVector3D out(cosf(a), sinf(a), 0);
		Limb arm = NewLimb(2, mat);
		arm.tip = true;
		QVector3D p = QVector3D(0, 0, top) + out * r, d = (out + Up * .8f).normalized();
		const float len = H * b.m_Rng.Range(.35f, .5f);
		const int segs = 6;
		arm.nodes.append(Node{p, r * .22f});
		arm.children.append(0);
		for(int k=1; k<=segs; k++)
		{
			p += d * (len / segs);
			d = (d - Up * .35f + out * .05f).normalized();
			arm.nodes.append(Node{p, r * .22f * (1.f - .8f * k / segs)});
			arm.children.append(0);
		}
		b.m_Limbs.append(arm);
		if(b.m_Rng.Chance(.5f))
			SwatchBall(tb, p, H * .02f, qMax(0.f, p.z()) * b.m_WindLeaf);
	}
}

static void Ribbon(Builder& b, const QVector3D& base, const QVector3D& dir, float len, float width, float waves, const Tile& tile)
{
	const int steps = b.m_Lod < 2 ? 4 : b.Res(10, 4);
	QVector3D side = Perp(dir);
	for(int face=0; face<2; face++)
		for(int i=0; i<steps; i++)
		{
			const float f0 = (float)i / steps, f1 = (float)(i + 1) / steps;
			auto at = [&](float f, float s)
			{
				const QVector3D wave = side * (sinf(f * waves * 2.f * Pi) * width * .8f);
				return base + dir * (len * f) + wave + QVector3D::crossProduct(dir, side) * (s * width * .5f);
			};
			const QVector3D n = QVector3D::crossProduct(side, dir) * (face ? -1.f : 1.f);
			const float t0 = tile.t + (tile.v - tile.t) * f0, t1 = tile.t + (tile.v - tile.t) * f1;
			const float m0 = qMax(0.f, at(f0, 0).z()) * b.m_WindLeaf, m1 = qMax(0.f, at(f1, 0).z()) * b.m_WindLeaf;
			AuFloraVAOVert a = Builder::Vert(at(f0, -1), n, tile.s, t0, m0), c = Builder::Vert(at(f0, 1), n, tile.u, t0, m0);
			AuFloraVAOVert d = Builder::Vert(at(f1, -1), n, tile.s, t1, m1), e = Builder::Vert(at(f1, 1), n, tile.u, t1, m1);
			if(face == 0)
			{
				b.Tri(a, c, d);
				b.Tri(d, c, e);
			}
			else
			{
				b.Tri(c, a, d);
				b.Tri(c, d, e);
			}
		}
}

static void BuildTypeKelp(TypeBuild& tb)
{
	Builder& b = tb.b;
	const float H = b.m_Height > 0.f ? b.m_Height : b.m_CardSize;
	const int strands = tb.shrub ? 5 : (b.m_Lod < 2 ? 5 : 11);
	for(int i=0; i<strands; i++)
	{
		const float a = b.m_Rng.Range(0.f, 2.f*Pi), off = H * .08f * sqrtf(b.m_Rng.Unit());
		const QVector3D base(cosf(a) * off, sinf(a) * off, 0);
		Ribbon(b, base, Deflect(Up, b.m_Rng.Range(0.f, .2f), a), H * b.m_Rng.Range(.6f, 1.f), H * .08f, b.m_Rng.Range(1.5f, 3.f),
				b.m_Leaf);
	}
}

static void BuildTypeTubes(TypeBuild& tb)
{
	Builder& b = tb.b;
	const TreeType& t = tb.t;
	const float H = b.m_Height > 0.f ? b.m_Height : b.m_CardSize;
	const int tubes = tb.shrub ? 5 : 9;
	for(int i=0; i<tubes; i++)
	{
		const float a = b.m_Rng.Range(0.f, 2.f*Pi), off = H * .12f * sqrtf(b.m_Rng.Unit());
		const QVector3D base(cosf(a) * off, sinf(a) * off, 0);
		const float h = H * (i == 0 ? 1.f : b.m_Rng.Range(.3f, .8f));
		const float r = H * (t.m_P1 > 0 ? .02f : .06f) * b.m_Rng.Range(.7f, 1.2f);
		Limb tube = NewLimb(0, t.m_Material >= 0 ? t.m_Material : TILE_Coral);
		tube.open = true;
		tube.sidesScale = 1.3f;
		const QVector3D d = Deflect(Up, b.m_Rng.Range(0.f, .25f), a);
		for(int k=0; k<=4; k++)
		{
			tube.nodes.append(Node{base + d * (h * k / 4.f), r * (t.m_P1 > 0 ? 1.f : 1.f + .25f * k / 4.f)});
			tube.children.append(0);
		}
		b.m_Limbs.append(tube);
		if(t.m_P1 > 0)
		{
			Cluster c;
			c.p = base + d * h;
			c.dir = d;
			c.size = H * .22f;
			c.kind = LEAF_PlumeBall;
			b.m_Clusters.append(c);
		}
	}
}

static void BuildTypeHelix(TypeBuild& tb)
{
	Builder& b = tb.b;
	const float H = b.m_Height > 0.f ? b.m_Height : b.m_CardSize;
	const float trunkH = H * .55f, R = H * .05f, r = H * .025f;
	const float turns = 1.5f + b.m_DNA.Multiples * .25f;
	for(int strand=0; strand<2; strand++)
	{
		Limb l = NewLimb(0, 0);
		const int segs = b.m_Lod < 2 ? 8 : 18;
		for(int k=0; k<=segs; k++)
		{
			const float f = (float)k / segs;
			const float a = f * turns * 2.f * Pi + strand * Pi;
			l.nodes.append(Node{QVector3D(cosf(a) * R * (1.f - .5f * f), sinf(a) * R * (1.f - .5f * f), trunkH * f), r});
			l.children.append(0);
		}
		b.m_Limbs.append(l);
	}
	TreeType crown = tb.t;
	crown.m_Levels = 2;
	TypeBuild ctb{b, crown, tb.shrub, tb.swatch, tb.pale, QVector3D(), {}};
	Arch a;
	a.height = H;
	a.trunkFrac = .1f;
	a.excurrent = false;
	a.stems = 1;
	a.levels = 2;
	a.children[0] = 4; a.children[1] = 4; a.children[2] = 3; a.children[3] = 2;
	a.whorl = 0;
	a.angle[0] = 0; a.angle[1] = .6f; a.angle[2] = .8f; a.angle[3] = .9f;
	a.ratio = .6f;
	a.grav[0] = .2f; a.grav[1] = .05f; a.grav[2] = 0; a.grav[3] = 0;
	a.twist = .3f;
	a.bend = .15f;
	a.radius = r;
	a.leafSize = H * .1f;
	a.leafSpacing = .55f;
	a.leafKind = 5;
	a.leafFrom = .75f;
	a.pods = false;
	a.twigWood = true;
	TypeBranch(ctb, a, 0, Up * trunkH, Up, H * .45f, r);
}

static void BuildTypeMembrane(TypeBuild& tb)
{
	Builder& b = tb.b;
	const float H = b.m_Height > 0.f ? b.m_Height : b.m_CardSize;
	const float top = H * .75f, r = H * .025f;
	Limb trunk = NewLimb(0, 0);
	for(int k=0; k<=4; k++)
	{
		trunk.nodes.append(Node{QVector3D(0, 0, top * k / 4.f), r * (1.f - .3f * k / 4.f)});
		trunk.children.append(0);
	}
	b.m_Limbs.append(trunk);
	const int ribs = 6 + (int)b.m_DNA.Multiples;
	QVector<QVector<QVector3D>> pts;
	for(int i=0; i<ribs; i++)
	{
		const float a = 2.f*Pi * i / ribs;
		const QVector3D out(cosf(a), sinf(a), 0);
		Limb rib = NewLimb(2, 0);
		rib.tip = true;
		QVector<QVector3D> line;
		const int segs = 5;
		for(int k=0; k<=segs; k++)
		{
			const float f = (float)k / segs;
			const QVector3D p = QVector3D(0, 0, top) + out * (H * .45f * f) + Up * (H * .15f * sinf(f * Pi * .8f) - H * .12f * f * f);
			rib.nodes.append(Node{p, r * .3f * (1.f - .7f * f)});
			rib.children.append(0);
			line.append(p);
		}
		b.m_Limbs.append(rib);
		pts.append(line);
	}
	const Tile& tile = b.m_Leaf;
	for(int i=0; i<ribs; i++)
	{
		const QVector<QVector3D>& L = pts[i];
		const QVector<QVector3D>& M = pts[(i + 1) % ribs];
		for(int k=0; k<L.count()-1; k++)
		{
			const float t0 = tile.t + (tile.v - tile.t) * k / (L.count() - 1), t1 = tile.t + (tile.v - tile.t) * (k + 1) / (L.count() - 1);
			const QVector3D n = QVector3D::crossProduct(L[k+1] - L[k], M[k] - L[k]).normalized();
			const float m0 = qMax(0.f, L[k].z()) * b.m_WindLeaf * .5f, m1 = qMax(0.f, L[k+1].z()) * b.m_WindLeaf * .5f;
			for(int face=0; face<2; face++)
			{
				const QVector3D nn = face ? -n : n;
				AuFloraVAOVert a = Builder::Vert(L[k], nn, tile.s, t0, m0), c = Builder::Vert(M[k], nn, tile.u, t0, m0);
				AuFloraVAOVert d = Builder::Vert(L[k+1], nn, tile.s, t1, m1), e = Builder::Vert(M[k+1], nn, tile.u, t1, m1);
				if(face == 0)
				{
					b.Tri(a, d, c);
					b.Tri(d, e, c);
				}
				else
				{
					b.Tri(c, d, a);
					b.Tri(c, e, d);
				}
			}
		}
	}
}

static void BuildTypeFungus(TypeBuild& tb)
{
	Builder& b = tb.b;
	const TreeType& t = tb.t;
	switch((int)t.m_P1)
	{
	case 0:	b.m_CapShape = .25f; break;
	case 2:	b.m_CapShape = 1.1f; break;
	case 3:	b.m_CapShape = 2.2f; break;
	default: b.m_CapShape = .6f; break;
	}
	BuildFungal(b, tb.shrub);
}

static void BuildTypeCrystal(TypeBuild& tb)
{
	Builder& b = tb.b;
	const TreeType& t = tb.t;
	const float H = b.m_Height > 0.f ? b.m_Height : b.m_CardSize;
	const int style = (int)t.m_P1;
	const int shards = style == 2 ? 3 : (style == 1 ? 9 + (int)b.m_DNA.Multiples : 5 + (int)b.m_DNA.Multiples);
	const int mat = t.m_Material >= 0 ? t.m_Material : 3;
	float az = b.m_Rng.Range(0.f, 2.f*Pi);
	for(int i=0; i<shards; i++)
	{
		az += GoldenAngle;
		const bool main = i == 0;
		const float h = H * (main ? 1.f : (style == 1 ? b.m_Rng.Range(.4f, .8f) : b.m_Rng.Range(.25f, .6f)));
		const float r = H * (style == 2 ? .09f : .05f) * (main ? 1.f : b.m_Rng.Range(.4f, .75f));
		const QVector3D d = main ? Up : Deflect(Up, style == 1 ? b.m_Rng.Range(.5f, 1.1f) : b.m_Rng.Range(.2f, .6f), az);
		Limb l = NewLimb(main ? 0 : 1, mat);
		l.faceted = true;
		l.tip = true;
		l.sidesScale = 2.f;
		l.nodes << Node{-d * r, r} << Node{d * (h * .85f), r * .85f};
		l.children << 0 << 0;
		b.m_Limbs.append(l);
	}
}

static void TypeClusters(TypeBuild& tb)
{
	Builder& b = tb.b;
	QVector<Cluster> keep;
	for(const Cluster& c : b.m_Clusters)
	{
		const float move = qMax(0.f, c.p.z()) * b.m_WindLeaf;
		switch(c.kind)
		{
		case 1:
			SwatchBall(tb, c.p - Up * (c.size * 1.6f), c.size, move);
			break;
		case 14:
			SwatchBall(tb, c.p, c.size, move);
			break;
		case LEAF_Orb:
			SwatchBall(tb, c.p, c.size * .5f, move);
			break;
		case LEAF_Rosette:
		case LEAF_PlumeBall:
		{
			const int blades = b.m_Lod < 2 ? 5 : (c.kind == LEAF_Rosette ? 11 : 16);
			for(int i=0; i<blades; i++)
			{
				const QVector3D d = c.kind == LEAF_Rosette ? Deflect((c.dir + Up).normalized(), b.m_Rng.Range(.1f, 1.3f), i * GoldenAngle)
						: Deflect(Up, acosf(1.f - 2.f * (i + .5f) / blades), i * GoldenAngle);
				const QVector3D side = Rotate(Perp(d), d, b.m_Rng.Range(0.f, 2.f*Pi));
				b.Card(c.p, d, side, c.size * b.m_Rng.Range(.8f, 1.1f), c.size * (c.kind == LEAF_Rosette ? .9f : .6f), .05f,
						b.m_Leaf, true, true);
			}
			break;
		}
		case LEAF_Puff:
		{
			for(int i=0; i<3; i++)
			{
				const QVector3D d = i == 0 ? Up : Rotate(QVector3D(1, 0, 0), Up, i * Pi / 3.f + b.m_Rng.Unit());
				const QVector3D side = i == 0 ? QVector3D(1, 0, 0) : QVector3D::crossProduct(Up, d).normalized();
				b.Card(c.p - d * (c.size * .5f), d, side, c.size, c.size, 0.f, b.m_Leaf, true, true);
			}
			break;
		}
		case LEAF_Ribbon:
			Ribbon(b, c.p, Deflect(-Up, b.m_Rng.Range(0.f, .3f), b.m_Rng.Range(0.f, 2.f*Pi)), c.size, c.size * .12f, 1.5f, b.m_Leaf);
			break;
		default:
			keep.append(c);
			break;
		}
	}
	b.m_Clusters = keep;
}

void BuildPlant(QList<AuFloraVAOTri>& tris, const AuFloraDNA& dna, const Traits& tr, float textureSize, int tileSize, bool tree,
		quint32 seed, int lod, float height, float maxRadius)
{
	const TreeType& t = Types()[qBound(0, tr.m_Type, Types().count() - 1)];
	const int from = tris.count();
	const unsigned int texBase = tr.m_Zone == AuFloraDNA::Polar ? 0 : (tr.m_Zone == AuFloraDNA::Tropical ? 36 : 18);
	const Tile leaf = Tile::Palette(texBase + tr.m_Slot, tileSize);
	const Tile bark = MaterialRect(TILE_Bark0 + dna.BarkTexture, tileSize);
	const Tile ringed = MaterialRect(TILE_Ringed, tileSize);
	const Tile special = MaterialRect(TILE_Special, tileSize);
	const int cap = lod < 2 ? 0 : (tree ? Budget::TreeTriangleCap : Budget::ShrubTriangleCap);
	float detail = 1.f;
	for(int attempt=0; ; attempt++)
	{
		Builder b(tris, dna, tr, textureSize, leaf, bark, ringed, special, seed, lod);
		b.m_Budget.m_Detail = detail;
		b.m_Height = height;
		b.m_TileSize = tileSize;
		TypeBuild tb{b, t, !tree, SwatchCell(MaterialRect(TILE_Swatch0 + tr.m_Zone, tileSize), tr.m_Slot),
				MaterialRect(TILE_Pale, tileSize), QVector3D(), {}};

		bool scale = false;
		switch(t.m_Builder)
		{
		case B_Palm:		BuildTypePalm(tb); break;
		case B_Cactus:		BuildTypeCactus(tb); scale = true; break;
		case B_Bamboo:		BuildTypeBamboo(tb); break;
		case B_Fungus:		BuildTypeFungus(tb); scale = true; break;
		case B_Crystal:		BuildTypeCrystal(tb); scale = true; break;
		case B_Shelf:		BuildTypeShelfOrPagoda(tb, false); break;
		case B_Pagoda:		BuildTypeShelfOrPagoda(tb, true); break;
		case B_Anemone:		BuildTypeAnemone(tb); break;
		case B_Kelp:		BuildTypeKelp(tb); break;
		case B_Stalks:		BuildTypeStalks(tb); break;
		case B_Helix:		BuildTypeHelix(tb); break;
		case B_Membrane:	BuildTypeMembrane(tb); break;
		case B_Tubes:		BuildTypeTubes(tb); break;
		default:			BuildTypeArch(tb); break;
		}
		b.CrownShape();
		TypeClusters(tb);
		Finish(b, from, maxRadius, scale);

		const int made = tris.count() - from;
		if(!cap || made <= cap || attempt >= 3 || detail <= Budget::MinDetail())
			break;
		detail = qMax(Budget::MinDetail(), detail * powf((float)cap / made, .75f) * .97f);
		tris.erase(tris.begin() + from, tris.end());
	}
}

}
