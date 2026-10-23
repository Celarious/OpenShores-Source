#ifndef FloraGenBuild_H
#define FloraGenBuild_H

#include "floragen.h"
#include "floratypes.h"
#include <QImage>
#include <QPainter>
#include <QQuaternion>
#include <QVector>
#include <cmath>

namespace FloraGen
{

static const float Pi = 3.14159265358979f;
static const float GoldenAngle = 2.39996323f;
static const QVector3D Up(0, 0, 1);

quint64 HashDNA(const AuFloraDNA& d, quint64 seed);

inline QVector3D Perp(const QVector3D& d)
{
	QVector3D a = fabsf(d.z()) < .9f ? Up : QVector3D(1, 0, 0);
	return QVector3D::crossProduct(d, a).normalized();
}

inline QVector3D Rotate(const QVector3D& v, const QVector3D& axis, float radians)
{
	return QQuaternion::fromAxisAndAngle(axis, radians * 180.f / Pi).rotatedVector(v);
}

inline QVector3D Deflect(const QVector3D& d, float angle, float azimuth)
{
	QVector3D side = Rotate(Perp(d), d, azimuth);
	return Rotate(d, side, angle).normalized();
}

inline QVector3D Lerp(const QVector3D& a, const QVector3D& b, float f)
{
	return a + (b - a) * f;
}

struct Node
{
	QVector3D							p;
	float								r;
};

struct Limb
{
	QVector<Node>						nodes;
	QVector<int>						children;
	int									depth;
	bool								faceted;
	bool								tip;
	int									material;
	float								sidesScale;
	bool								open = false;
};

struct Cluster
{
	QVector3D							p;
	QVector3D							dir;
	float								size;
	int									kind;
};

struct Style
{
	float								lengthMul;
	float								taperMul;
	float								bendDeg;
	float								grav;
	float								twist;
	float								curl;
	float								flatAbove;

	Style() : lengthMul(1), taperMul(1), bendDeg(10), grav(0), twist(0), curl(0), flatAbove(0) {}
};

class Builder
{
public:
	QList<AuFloraVAOTri>&				m_Tris;
	const AuFloraDNA&					m_DNA;
	const Traits&						m_Tr;
	Rng									m_Rng;
	Budget								m_Budget;
	int									m_Lod;
	float								m_CardSize;
	Tile								m_Leaf;
	Tile								m_Bark;
	Tile								m_Ringed;
	Tile								m_Special;
	QVector<Limb>						m_Limbs;
	QVector<Cluster>					m_Clusters;
	int									m_Segments;
	QVector3D							m_CrownCentre;
	float								m_CrownRadius;
	float								m_WindWood;
	float								m_WindLeaf;
	float								m_TrunkDia;
	float								m_MinDia;

	struct CapRequest
	{
		QVector3D						p;
		QVector3D						axis;
		float							radius;
		float							stalkR;
	};
	QVector<CapRequest>					m_CapRequests;
	float								m_CapShape;
	float								m_TrunkLen = 0;
	float								m_Height = 0;
	int									m_TileSize = 0;
	float								m_FrondDroop = 0;

public:
	Builder(QList<AuFloraVAOTri>& tris, const AuFloraDNA& dna, const Traits& tr, float textureSize, const Tile& leaf,
			const Tile& bark, const Tile& ringed, const Tile& special, quint32 seed, int lod) :
		m_Tris(tris), m_DNA(dna), m_Tr(tr), m_Rng(Mix(HashDNA(dna, 0x51ED), seed)), m_Budget(Budget::ForLod(lod)), m_Lod(lod),
		m_CardSize(textureSize), m_Leaf(leaf), m_Bark(bark), m_Ringed(ringed), m_Special(special), m_Segments(0),
		m_CrownRadius(1)
	{
		m_WindLeaf = .25f / (((float)dna.Stiffness + 2.f) / 2.f);
		m_WindWood = .125f / (((float)dna.Stiffness + 2.f) / 2.f);
		m_TrunkDia = dna.TrunkBaseDiameter();
		m_MinDia = dna.MinBaseDiameter();
		if(m_CardSize <= 0.f)
			m_CardSize = m_TrunkDia * 4.f;
		m_CapShape = m_Rng.Range(.25f, .8f);
		const float mid = (m_Special.s + m_Special.u) / 2.f, gap = (m_Special.u - m_Special.s) * .01f;
		if(tr.m_SpecialSide)
			m_Special.s = mid + gap;
		else
			m_Special.u = mid - gap;
	}

	float BranchAngle(int place) const
	{
		return (7.f + 6.f * m_DNA.BranchAngle) * (place + 1) * Pi / 180.f;
	}

	int Grow(const QVector3D& start, const QVector3D& dir, float dia, int maxsegs, float mindia, int depth, const Style& st)
	{
		Limb limb;
		limb.depth = depth;
		limb.faceted = false;
		limb.tip = false;
		limb.material = 0;
		limb.sidesScale = 1;
		limb.nodes.append(Node{start, dia/2.f});
		limb.children.append(0);

		QVector3D d = dir.normalized();
		QVector3D p = start;
		const float taper = m_DNA.BranchTaperFactor() * st.taperMul;
		QVector3D curlAxis = Perp(d);
		int segs = 0;
		do
		{
			if(m_Segments >= m_Budget.m_MaxSegments)
				break;
			const float len = m_DNA.BranchLength(dia) * st.lengthMul;
			p += d * len;
			dia -= dia * taper;
			limb.nodes.append(Node{p, dia/2.f});
			limb.children.append(0);
			m_Segments++;
			segs++;

			d = Deflect(d, m_Rng.Unit() * st.bendDeg * Pi / 180.f, m_Rng.Range(0.f, 2.f*Pi));
			d = (d + Up * st.grav).normalized();
			if(st.twist != 0.f)
				d = Rotate(d, Up, st.twist);
			if(st.curl != 0.f)
			{
				d = Rotate(d, curlAxis, st.curl);
				curlAxis = Rotate(curlAxis, d, .3f);
			}
			if(st.flatAbove > 0.f && p.z() > st.flatAbove)
				d = QVector3D(d.x(), d.y(), d.z() * .25f).normalized();
		}
		while(dia >= mindia && segs < maxsegs);

		if(limb.nodes.count() < 2)
			return -1;
		m_Limbs.append(limb);
		return m_Limbs.count() - 1;
	}

	void SideBranches(int limbIndex, int maxsegs, float mindia, const Style& st, int mode, int recurse)
	{
		if(limbIndex < 0)
			return;
		float azimuth = m_Rng.Range(0.f, 2.f*Pi);
		const int count = m_Limbs[limbIndex].nodes.count();
		for(int i=1; i<count; i++)
		{
			const Node node = m_Limbs[limbIndex].nodes[i];
			const QVector3D dir = (node.p - m_Limbs[limbIndex].nodes[i-1].p).normalized();
			int brs = (int)m_DNA.Multiples;
			if(i == count - 1)
				brs++;
			for(int ib=0; ib<brs; ib++)
			{
				if(m_DNA.Inflorescence != AuFloraDNA::INFL_UmbelCompound && m_Rng.Below(4) > (int)m_DNA.BranchProbability)
					continue;
				float szb = node.r * 2.f / ((1 + m_Rng.Below(5) + 1) * .25f + .5f);
				if(szb < mindia)
					continue;
				azimuth += GoldenAngle;
				const float angle = qBound(.25f, BranchAngle(ib) * 1.2f, 1.3f);
				const QVector3D bdir = Deflect(dir, angle, azimuth);
				const int segs = mode == 1 ? 1 : qMax(1, maxsegs - ib);
				Style cst = st;
				int child = Grow(node.p - bdir * node.r * .5f, bdir, szb, segs, mindia, m_Limbs[limbIndex].depth + 1, cst);
				if(child < 0)
					continue;
				m_Limbs[limbIndex].children[i]++;
				if(recurse > 0)
					SideBranches(child, maxsegs - 2, mindia, st, mode, recurse - 1);
			}
		}
	}

	void Twigs(int fromLimb, int count, const Style& st)
	{
		const int limbs = m_Limbs.count();
		for(int li=fromLimb; li<limbs; li++)
		{
			const int depth = m_Limbs[li].depth;
			if(depth == 0 && limbs > 1)
				continue;
			const Node tipNode = m_Limbs[li].nodes.last();
			const QVector3D dir = (tipNode.p - m_Limbs[li].nodes[m_Limbs[li].nodes.count()-2].p).normalized();
			float az = m_Rng.Range(0.f, 2.f*Pi);
			for(int k=0; k<count; k++)
			{
				az += GoldenAngle;
				const QVector3D tdir = Deflect(dir, m_Rng.Range(.35f, .8f), az);
				const float dia = qMax(tipNode.r * 2.f * .8f, m_DNA.MinBranchDiameter() * .5f);
				Style tst = st;
				tst.lengthMul *= .6f;
				Grow(tipNode.p, tdir, dia, 1 + m_Rng.Below(2), 0.f, depth + 1, tst);
			}
		}
	}

	void Foliage(int limbIndex, int kind, float sizeMul, bool tipOnly)
	{
		const Limb& limb = m_Limbs[limbIndex];
		const int count = limb.nodes.count();
		for(int i=1; i<count; i++)
		{
			if(tipOnly && i != count - 1)
				continue;
			const Node& node = limb.nodes[i];
			const QVector3D dir = (node.p - limb.nodes[i-1].p).normalized();
			int leaves = ((int)m_DNA.Multiples + 1) - limb.children[i];
			if(i == count - 1 && leaves < 1)
				leaves = 1;
			for(int leaf=0; leaf<leaves; leaf++)
			{
				if(i != count - 1 && m_Rng.Below(4) > (int)m_DNA.BranchProbability)
					continue;
				const float scl = qBound(.25f, node.r * 2.f / qMax(.001f, m_TrunkDia * m_DNA.BranchTaperFactor()), 1.6f);
				Cluster c;
				c.p = node.p;
				c.dir = Deflect(dir, m_Rng.Range(.2f, BranchAngle(leaf) + .3f), m_Rng.Range(0.f, 2.f*Pi));
				c.size = m_CardSize * scl * sizeMul;
				c.kind = kind;
				m_Clusters.append(c);
			}
		}
	}

	void CrownShape()
	{
		if(m_Clusters.isEmpty())
		{
			m_CrownCentre = QVector3D(0, 0, 0);
			m_CrownRadius = 1;
			return;
		}
		QVector3D sum(0, 0, 0);
		for(const Cluster& c : m_Clusters)
			sum += c.p;
		m_CrownCentre = sum / (float)m_Clusters.count();
		float r2 = 0;
		for(const Cluster& c : m_Clusters)
			r2 += (c.p - m_CrownCentre).lengthSquared();
		m_CrownRadius = qMax(.001f, sqrtf(r2 / m_Clusters.count()));
	}

	QVector3D FoliageNormal(const QVector3D& p, const QVector3D& cardNormal) const
	{
		QVector3D out = p - m_CrownCentre;
		out.setZ(out.z() * .8f + m_CrownRadius * .25f);
		const float len = out.length();
		if(len < 1e-5f)
			return cardNormal;
		out /= len;
		return (cardNormal * .35f + out * .65f).normalized();
	}

	void Tri(const AuFloraVAOVert& a, const AuFloraVAOVert& b, const AuFloraVAOVert& c)
	{
		m_Tris.append(AuFloraVAOTri(a, b, c));
	}

	static AuFloraVAOVert Vert(const QVector3D& p, const QVector3D& n, float s, float t, float move)
	{
		return AuFloraVAOVert(p.x(), p.y(), p.z(), n.x(), n.y(), n.z(), s, t, move);
	}

	Tile MaterialTile(int material) const
	{
		if(material >= 64 && m_TileSize > 0)
			return MaterialRect(material, m_TileSize);
		switch(material)
		{
		case 1:		return m_Ringed;
		case 2:		return m_Special.Band(.5f, .75f);
		case 3:		return m_Special.Band(0.f, .5f);
		default:	return m_Bark;
		}
	}

	static float MaterialHeight(int material)
	{
		return material == 2 ? .25f : (material == 3 ? .5f : 1.f);
	}

	void Card(const QVector3D& base, const QVector3D& dir, const QVector3D& side, float height, float width, float bend,
			const Tile& tile, bool moves=true, bool flat=false)
	{
		const QVector3D n0 = QVector3D::crossProduct(side, dir).normalized();
		QVector3D up1 = dir * (height/2.f);
		QVector3D p[6];
		p[0] = base - side * (width/2.f);
		p[1] = base + side * (width/2.f);
		up1 = Rotate(up1, side, bend);
		p[2] = p[0] + up1;
		p[3] = p[1] + up1;
		QVector3D up2 = Rotate(up1, side, bend);
		p[4] = p[2] + up2;
		p[5] = p[3] + up2;
		const QVector3D n1 = Rotate(n0, side, bend);
		const QVector3D n2 = Rotate(n1, side, bend);
		float move[3];
		for(int k=0; k<3; k++)
			move[k] = moves ? qMax(0.f, p[k*2].z()) * m_WindLeaf : 0.f;
		move[0] *= .5f;
		move[1] *= .75f;
		const float tm = (tile.t + tile.v) / 2.f;
		const float tt[3] = { tile.t, tm, tile.v };
		const QVector3D nn[3] = { n0, n1, n2 };

		if(m_Lod < 2 || flat)
		{
			for(int face=0; face<2; face++)
			{
				const float sg = face ? -1.f : 1.f;
				AuFloraVAOVert a = Vert(p[0], FoliageNormal(p[0], nn[0]*sg), tile.s, tt[0], move[0]);
				AuFloraVAOVert b = Vert(p[1], FoliageNormal(p[1], nn[0]*sg), tile.u, tt[0], move[0]);
				AuFloraVAOVert c = Vert(p[4], FoliageNormal(p[4], nn[2]*sg), tile.s, tt[2], move[2]);
				AuFloraVAOVert d = Vert(p[5], FoliageNormal(p[5], nn[2]*sg), tile.u, tt[2], move[2]);
				if(face == 0)
				{
					Tri(a, b, c);
					Tri(c, b, d);
				}
				else
				{
					Tri(b, a, d);
					Tri(d, a, c);
				}
			}
			return;
		}
		for(int face=0; face<2; face++)
		{
			const float sg = face ? -1.f : 1.f;
			AuFloraVAOVert v[6];
			for(int k=0; k<6; k++)
				v[k] = Vert(p[k], FoliageNormal(p[k], nn[k/2]*sg), (k & 1) ? tile.u : tile.s, tt[k/2], move[k/2]);
			for(int row=0; row<2; row++)
			{
				const AuFloraVAOVert& a = v[row*2];
				const AuFloraVAOVert& b = v[row*2+1];
				const AuFloraVAOVert& c = v[row*2+2];
				const AuFloraVAOVert& d = v[row*2+3];
				if(face == 0)
				{
					Tri(a, b, c);
					Tri(c, b, d);
				}
				else
				{
					Tri(b, a, d);
					Tri(d, a, c);
				}
			}
		}
	}

	void ClusterCards(const Cluster& c)
	{
		const float bendBase = m_DNA.BranchBendFactor() * (Pi/2.f);
		const bool single = c.kind >= 5;
		const int kind = single ? (c.kind == 6 ? 2 : (c.kind == 7 ? 3 : (c.kind == 8 ? 8 : 0))) : c.kind;
		const int cards = (single || m_Lod < 2) ? 1 : m_Tr.m_CardsPerCluster;
		const float scale = m_Lod < 2 ? qMin(1.f, m_Tr.m_CardScale * 1.35f) : m_Tr.m_CardScale;
		for(int k=0; k<cards; k++)
		{
			if(m_Tris.count() > 0 && m_CardsMade >= m_Budget.m_MaxCards)
				return;
			m_CardsMade++;
			const float h = c.size * (single ? 1.f : scale) * m_Rng.Range(.8f, 1.1f);
			QVector3D dir, side;
			float bend = bendBase * m_Rng.Range(.7f, 1.3f);
			switch(kind)
			{
			case 2:
				dir = QVector3D(c.dir.x(), c.dir.y(), 0);
				if(dir.lengthSquared() < 1e-6f)
					dir = QVector3D(1, 0, 0);
				dir = Rotate(dir.normalized(), Up, m_Rng.Signed() * .9f);
				dir = (dir + Up * m_Rng.Range(-.05f, .2f)).normalized();
				side = QVector3D::crossProduct(Up, dir).normalized();
				side = Rotate(side, dir, m_Rng.Signed() * .25f);
				bend *= .5f;
				break;
			case 8:
			{
				QVector3D along = QVector3D(c.dir.x(), c.dir.y(), 0);
				if(along.lengthSquared() < 1e-6f)
					along = Perp(Up);
				along.normalize();
				dir = (Rotate(along, Up, m_Rng.Signed() * .6f) - Up * m_Rng.Range(.1f, .45f)).normalized();
				side = QVector3D::crossProduct(Up, dir).normalized();
				side = Rotate(side, dir, m_Rng.Signed() * .5f);
				bend *= .8f;
				break;
			}
			case 3:
				dir = Deflect(-Up, m_Rng.Range(0.f, .35f), m_Rng.Range(0.f, 2.f*Pi));
				side = Rotate(Perp(dir), dir, m_Rng.Range(0.f, 2.f*Pi));
				bend *= .3f;
				break;
			default:
				dir = k == 0 ? c.dir : Deflect(c.dir, m_Rng.Range(.35f, 1.1f), m_Rng.Range(0.f, 2.f*Pi));
				side = Rotate(Perp(dir), dir, m_Rng.Range(0.f, 2.f*Pi));
				break;
			}
			if(kind == 0 || kind == 2 || kind == 8)
			{
				QVector3D n = QVector3D::crossProduct(side, dir);
				if(n.z() < 0)
					side = -side;
			}
			const QVector3D base = c.p + QVector3D(m_Rng.Signed(), m_Rng.Signed(), m_Rng.Signed()) * (h * .06f);
			Card(base, dir, side, h, h, bend, m_Leaf, true, single);
		}
	}

	struct Ring
	{
		QVector3D						p;
		QVector3D						t;
		QVector3D						n;
		float							r;
		float							acc;
		float							z0;
	};

	float FlareRadius(float r, float z0, float theta, bool trunk) const
	{
		if(!trunk)
			return r;
		float k = r;
		const float rr = qMax(1e-4f, r);
		if(m_Tr.m_Flare > 0.f)
			k *= 1.f + m_Tr.m_Flare * expf(-qMax(0.f, z0) / (rr * 1.6f));
		if(m_Tr.m_Buttresses > 0)
		{
			float lobe = powf(qMax(0.f, cosf(theta * m_Tr.m_Buttresses + 0.7f)), 3.f);
			k *= 1.f + m_Tr.m_Flare * 1.6f * lobe * expf(-qMax(0.f, z0) / (rr * 3.f));
		}
		if(m_Tr.m_Swell > 0.f && m_TrunkLen > 0.f)
		{
			const float f = qBound(0.f, z0 / m_TrunkLen, 1.f);
			k *= 1.f + m_Tr.m_Swell * powf(sinf(Pi * (.15f + .85f * f) * .9f), 1.5f);
		}
		return k;
	}

	void Tube(const Limb& limb, bool trunk)
	{
		const int nn = limb.nodes.count();
		if(nn < 2)
			return;

		const float girth = limb.nodes[0].r / qMax(.001f, m_TrunkDia / 2.f);
		int sides = (int)roundf(m_Budget.m_SidesMax * m_Budget.m_Detail * qBound(.3f, sqrtf(girth), 1.f) * limb.sidesScale);
		if(limb.depth >= 2)
			sides = qMin(sides, m_Lod < 2 ? 3 : Res(5, 3));
		sides = qMax(3, sides);
		if(limb.faceted)
			sides = qBound(4, sides, 6);

		QVector<QVector3D> pts;
		QVector<float> rad;
		const int sub = (limb.depth <= 1 && !limb.faceted) ? Res(m_Budget.m_RingsPerSegment, 1) : 1;
		for(int i=0; i<nn-1; i++)
		{
			const QVector3D p0 = limb.nodes[qMax(0, i-1)].p;
			const QVector3D p1 = limb.nodes[i].p;
			const QVector3D p2 = limb.nodes[i+1].p;
			const QVector3D p3 = limb.nodes[qMin(nn-1, i+2)].p;
			int steps = sub;
			if(trunk && i == 0)
				steps = qMax(steps, m_Lod < 2 ? 2 : Res(5, 2));
			for(int s=0; s<steps; s++)
			{
				float f = (float)s / steps;
				if(trunk && i == 0)
					f = f * f;
				const float f2 = f*f, f3 = f2*f;
				QVector3D q = .5f * ((2.f*p1) + (-p0 + p2)*f + (2.f*p0 - 5.f*p1 + 4.f*p2 - p3)*f2 + (-p0 + 3.f*p1 - 3.f*p2 + p3)*f3);
				pts.append(q);
				rad.append(limb.nodes[i].r + (limb.nodes[i+1].r - limb.nodes[i].r) * f);
			}
		}
		pts.append(limb.nodes[nn-1].p);
		rad.append(limb.nodes[nn-1].r);

		if(trunk)
			pts[0] -= Up * (rad[0] * .6f);

		const int np = pts.count();
		QVector<Ring> rings(np);
		const Tile tile = MaterialTile(limb.material);
		const float tileAspect = MaterialHeight(limb.material);
		float acc = 0, z0 = 0;
		for(int i=0; i<np; i++)
		{
			Ring& rg = rings[i];
			rg.p = pts[i];
			rg.r = rad[i];
			QVector3D t = (i == 0) ? pts[1] - pts[0] : (i == np-1 ? pts[i] - pts[i-1] : pts[i+1] - pts[i-1]);
			rg.t = t.normalized();
			if(i == 0)
				rg.n = Perp(rg.t);
			else
			{
				QVector3D n = rings[i-1].n - rg.t * QVector3D::dotProduct(rings[i-1].n, rg.t);
				rg.n = n.lengthSquared() > 1e-8f ? n.normalized() : Perp(rg.t);
				const float seg = (pts[i] - pts[i-1]).length();
				const float circ = 2.f * Pi * qMax(qMax(rad[i], rad[i-1]), m_TrunkDia * .05f);
				acc += seg / (circ * tileAspect);
				z0 += seg;
			}
			rg.acc = acc;
			rg.z0 = z0;
		}

		for(int i=0; i<np-1; i++)
			Strip(rings[i], rings[i+1], sides, tile, trunk && limb.depth == 0 && !limb.faceted, limb.faceted, limb.depth);

		if(limb.open)
			return;
		const Ring& last = rings[np-1];
		const float tipLen = limb.tip ? last.r * (limb.faceted ? 7.f : 3.f) : last.r * 1.2f;
		const QVector3D apex = last.p + last.t * tipLen;
		const QVector3D bn = QVector3D::crossProduct(last.t, last.n);
		const float frac = last.acc - floorf(last.acc);
		const float vt = tile.t + (tile.v - tile.t) * qMin(frac, .99f);
		const float va = tile.t + (tile.v - tile.t) * qMin(frac + .1f, 1.f);
		const float moveA = qMax(0.f, apex.z()) * m_WindWood * Depthwise(limb.depth);
		for(int k=0; k<sides; k++)
		{
			const float a0 = 2.f*Pi*k/sides, a1 = 2.f*Pi*(k+1)/sides;
			const QVector3D d0 = last.n*cosf(a0) + bn*sinf(a0);
			const QVector3D d1 = last.n*cosf(a1) + bn*sinf(a1);
			const QVector3D q0 = last.p + d0*last.r, q1 = last.p + d1*last.r;
			QVector3D fn0 = (d0 + last.t * .5f).normalized(), fn1 = (d1 + last.t * .5f).normalized();
			if(limb.faceted)
				fn0 = fn1 = QVector3D::crossProduct(q1 - q0, apex - q0).normalized();
			const float m0 = qMax(0.f, q0.z()) * m_WindWood * Depthwise(limb.depth);
			const float m1 = qMax(0.f, q1.z()) * m_WindWood * Depthwise(limb.depth);
			const float u0 = tile.s + (tile.u - tile.s) * k / sides, u1 = tile.s + (tile.u - tile.s) * (k+1) / sides;
			Tri(Vert(q0, fn0, u0, vt, m0), Vert(q1, fn1, u1, vt, m1), Vert(apex, last.t, (u0+u1)/2.f, va, moveA));
		}
	}

	static float Depthwise(int depth)
	{
		return depth == 0 ? .4f : (depth == 1 ? .8f : 1.4f);
	}

	void Strip(const Ring& a, const Ring& b, int sides, const Tile& tile, bool trunk, bool faceted, int depth)
	{
		const float base = floorf(a.acc);
		if(b.acc - base > 1.f && b.acc > a.acc)
		{
			const float f = (base + 1.f - a.acc) / (b.acc - a.acc);
			Ring m;
			m.p = Lerp(a.p, b.p, f);
			m.t = Lerp(a.t, b.t, f).normalized();
			m.n = Lerp(a.n, b.n, f).normalized();
			m.r = a.r + (b.r - a.r) * f;
			m.acc = base + 1.f;
			m.z0 = a.z0 + (b.z0 - a.z0) * f;
			StripPart(a, m, sides, tile, trunk, faceted, depth, a.acc - base, 1.f);
			StripPart(m, b, sides, tile, trunk, faceted, depth, 0.f, qMin(1.f, b.acc - base - 1.f));
			return;
		}
		StripPart(a, b, sides, tile, trunk, faceted, depth, a.acc - base, b.acc - base);
	}

	void StripPart(const Ring& a, const Ring& b, int sides, const Tile& tile, bool trunk, bool faceted, int depth, float va,
			float vb)
	{
		const QVector3D ab = QVector3D::crossProduct(a.t, a.n), bb = QVector3D::crossProduct(b.t, b.n);
		const float ta = tile.t + (tile.v - tile.t) * va, tb = tile.t + (tile.v - tile.t) * vb;
		const float len = qMax(1e-5f, (b.p - a.p).length());
		const float dw = Depthwise(depth);
		for(int k=0; k<sides; k++)
		{
			const float a0 = 2.f*Pi*k/sides, a1 = 2.f*Pi*(k+1)/sides;
			const QVector3D da0 = a.n*cosf(a0) + ab*sinf(a0), da1 = a.n*cosf(a1) + ab*sinf(a1);
			const QVector3D db0 = b.n*cosf(a0) + bb*sinf(a0), db1 = b.n*cosf(a1) + bb*sinf(a1);
			const float ra0 = FlareRadius(a.r, a.z0, a0, trunk), ra1 = FlareRadius(a.r, a.z0, a1, trunk);
			const float rb0 = FlareRadius(b.r, b.z0, a0, trunk), rb1 = FlareRadius(b.r, b.z0, a1, trunk);
			const QVector3D pa0 = a.p + da0*ra0, pa1 = a.p + da1*ra1, pb0 = b.p + db0*rb0, pb1 = b.p + db1*rb1;
			QVector3D na0, na1, nb0, nb1;
			if(faceted)
			{
				na0 = na1 = nb0 = nb1 = QVector3D::crossProduct(pa1 - pa0, pb0 - pa0).normalized();
			}
			else
			{
				na0 = (da0 - a.t * ((rb0 - ra0) / len)).normalized();
				na1 = (da1 - a.t * ((rb1 - ra1) / len)).normalized();
				nb0 = (db0 - b.t * ((rb0 - ra0) / len)).normalized();
				nb1 = (db1 - b.t * ((rb1 - ra1) / len)).normalized();
			}
			const float u0 = tile.s + (tile.u - tile.s) * k / sides, u1 = tile.s + (tile.u - tile.s) * (k+1) / sides;
			const AuFloraVAOVert va0 = Vert(pa0, na0, u0, ta, qMax(0.f, pa0.z()) * m_WindWood * dw * (a.z0 > 0 ? 1.f : 0.f));
			const AuFloraVAOVert va1 = Vert(pa1, na1, u1, ta, qMax(0.f, pa1.z()) * m_WindWood * dw * (a.z0 > 0 ? 1.f : 0.f));
			const AuFloraVAOVert vb0 = Vert(pb0, nb0, u0, tb, qMax(0.f, pb0.z()) * m_WindWood * dw);
			const AuFloraVAOVert vb1 = Vert(pb1, nb1, u1, tb, qMax(0.f, pb1.z()) * m_WindWood * dw);
			Tri(va0, va1, vb1);
			Tri(vb1, vb0, va0);
		}
	}

	void Ball(const QVector3D& c, float r, const Tile& tile, float move)
	{
		const int rings = m_Lod < 2 ? 3 : Res(6, 3);
		const int sides = m_Lod < 2 ? 5 : Res(9, 5);
		auto at = [&](int ri, int k, QVector3D& p, QVector3D& n)
		{
			const float ph = Pi * ri / rings, th = 2.f*Pi*k / sides;
			n = QVector3D(sinf(ph)*cosf(th), sinf(ph)*sinf(th), cosf(ph));
			p = c + QVector3D(n.x() * r, n.y() * r, n.z() * r * 1.25f) * (n.z() > 0 ? 1.f - .35f*n.z() : 1.f);
		};
		for(int ri=0; ri<rings; ri++)
			for(int k=0; k<sides; k++)
			{
				QVector3D p00, p01, p10, p11, n00, n01, n10, n11;
				at(ri, k, p00, n00);
				at(ri, k+1, p01, n01);
				at(ri+1, k, p10, n10);
				at(ri+1, k+1, p11, n11);
				const float u0 = tile.s + (tile.u - tile.s) * k / sides, u1 = tile.s + (tile.u - tile.s) * (k+1) / sides;
				const float t0 = tile.t + (tile.v - tile.t) * ri / rings, t1 = tile.t + (tile.v - tile.t) * (ri+1) / rings;
				const AuFloraVAOVert a = Vert(p00, n00, u0, t0, move), b = Vert(p01, n01, u1, t0, move);
				const AuFloraVAOVert cc = Vert(p10, n10, u0, t1, move), d = Vert(p11, n11, u1, t1, move);
				if(ri > 0)
					Tri(a, cc, b);
				if(ri < rings - 1)
					Tri(b, cc, d);
			}
	}

	void Pad(const QVector3D& c, const QVector3D& axis, float a, float bw, float thick, const Tile& tile)
	{
		const QVector3D ax = axis.normalized();
		const QVector3D side = Perp(ax);
		const QVector3D nrm = QVector3D::crossProduct(ax, side);
		const int rings = m_Lod < 2 ? 4 : Res(8, 4);
		const int sides = m_Lod < 2 ? 6 : Res(12, 6);
		auto at = [&](int ri, int k, QVector3D& p, QVector3D& n)
		{
			const float ph = Pi * ri / rings, th = 2.f*Pi * k / sides;
			const float x = sinf(ph) * cosf(th), y = sinf(ph) * sinf(th), z = cosf(ph);
			p = c + ax * (z * a) + side * (x * bw) + nrm * (y * thick);
			n = (ax * (z / a) + side * (x / bw) + nrm * (y / thick)).normalized();
		};
		const float move = qMax(0.f, c.z()) * m_WindWood * .3f;
		for(int ri=0; ri<rings; ri++)
			for(int k=0; k<sides; k++)
			{
				QVector3D p00, p01, p10, p11, n00, n01, n10, n11;
				at(ri, k, p00, n00);
				at(ri, k+1, p01, n01);
				at(ri+1, k, p10, n10);
				at(ri+1, k+1, p11, n11);
				const float u0 = tile.s + (tile.u - tile.s) * k / sides, u1 = tile.s + (tile.u - tile.s) * (k+1) / sides;
				const float t0 = tile.t + (tile.v - tile.t) * ri / rings, t1 = tile.t + (tile.v - tile.t) * (ri+1) / rings;
				const AuFloraVAOVert va = Vert(p00, n00, u0, t0, move), vb = Vert(p01, n01, u1, t0, move);
				const AuFloraVAOVert vc = Vert(p10, n10, u0, t1, move), vd = Vert(p11, n11, u1, t1, move);
				if(ri > 0)
					Tri(va, vc, vb);
				if(ri < rings - 1)
					Tri(vb, vc, vd);
			}
	}

	void Frond(const QVector3D& base, const QVector3D& dir0, float length, float width, float droop, const Tile& tile)
	{
		const int steps = m_Lod < 2 ? 3 : 6;
		QVector3D side = QVector3D::crossProduct(Up, dir0);
		if(side.lengthSquared() < 1e-6f)
			side = Perp(dir0);
		side.normalize();
		QVector3D p = base, d = dir0;
		const float seg = length / steps;
		const float keel = .22f;
		QVector3D prevC, prevL, prevR, prevN;
		float prevT = tile.t, prevMove = 0;
		for(int i=0; i<=steps; i++)
		{
			const float f = (float)i / steps;
			const float w = width * .5f * (0.35f + .65f * sinf(Pi * qMin(1.f, f * 1.15f + .08f)));
			QVector3D n = QVector3D::crossProduct(side, d).normalized();
			if(n.z() < 0)
				n = -n;
			const QVector3D c = p;
			const QVector3D l = p - side * w - n * (w * keel);
			const QVector3D r = p + side * w - n * (w * keel);
			const float t = tile.t + (tile.v - tile.t) * f;
			const float move = qMax(0.f, p.z()) * m_WindLeaf * (.4f + .6f * f);
			if(i > 0)
			{
				const float tm = (tile.s + tile.u) / 2.f;
				const QVector3D nl0 = FoliageNormal(prevL, prevN), nl1 = FoliageNormal(l, n);
				const QVector3D nr0 = FoliageNormal(prevR, prevN), nr1 = FoliageNormal(r, n);
				const QVector3D nc0 = FoliageNormal(prevC, prevN), nc1 = FoliageNormal(c, n);
				for(int face=0; face<2; face++)
				{
					AuFloraVAOVert L0 = Vert(prevL, nl0, tile.s, prevT, prevMove), L1 = Vert(l, nl1, tile.s, t, move);
					AuFloraVAOVert C0 = Vert(prevC, nc0, tm, prevT, prevMove), C1 = Vert(c, nc1, tm, t, move);
					AuFloraVAOVert R0 = Vert(prevR, nr0, tile.u, prevT, prevMove), R1 = Vert(r, nr1, tile.u, t, move);
					if(face == 0)
					{
						Tri(L0, C0, L1); Tri(L1, C0, C1);
						Tri(C0, R0, C1); Tri(C1, R0, R1);
					}
					else
					{
						Tri(C0, L0, L1); Tri(C0, L1, C1);
						Tri(R0, C0, C1); Tri(R0, C1, R1);
					}
				}
			}
			prevC = c; prevL = l; prevR = r; prevN = n; prevT = t; prevMove = move;
			p += d * seg;
			d = Rotate(d, side, droop / steps);
		}
	}

	int m_CardsMade = 0;

	int Res(int n, int least) const
	{
		return qMax(least, (int)roundf(n * m_Budget.m_Detail));
	}
};

struct Arch
{
	float								height;
	float								trunkFrac;
	bool								excurrent;
	int									stems;
	int									levels;
	int									children[4];
	int									whorl;
	float								angle[4];
	float								ratio;
	float								grav[4];
	float								twist;
	float								bend;
	float								radius;
	float								leafSize;
	float								leafSpacing;
	int									leafKind;
	float								leafFrom;
	bool								pods;
	bool								twigWood;
};

void ScaleToFit(QList<AuFloraVAOTri>& tris, int from, float maxRadius);
void BuildPalm(Builder& b);
void BuildFungal(Builder& b, bool shrub);
void BuildCrystal(Builder& b, bool shrub);
void Specials(Builder& b);
void ScaleHeight(QList<AuFloraVAOTri>& tris, int from, float height);
void Finish(Builder& b, int from, float maxRadius, bool scaleToHeight=false);
void ArchBranch(Builder& b, const Arch& a, int level, const QVector3D& start, const QVector3D& dir0, float length,
		float r0);
float GameHeight(const AuFloraDNA& dna, float textureSize, int trunkSegs);
void BuildArch(Builder& b, int plan, bool shrub);
float Noise(float x, float y, int period, quint32 seed);
float Fbm(float x, float y, int period, quint32 seed, int octaves);
QColor LeafColor(const Traits& tr, float shade, float hueJitter);
QRgb Tint(QRgb px, const QColor& tint, float shade);

}

#endif
