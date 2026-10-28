#ifndef AuViewFrustum_H
#define AuViewFrustum_H

#include "aubox.h"
#include <cmath>

// Planes are expressed in the space supplied to the clip matrix. Construct once
// per model/pass, then test its cached bounds without projecting every vertex.
class AuViewFrustum
{
	struct Plane
	{
		double x, y, z, d;
	};
	Plane m_Planes[6];
	bool m_Valid;
	int m_PlaneCount;

	static bool Finite(const AuPoint& point)
	{
		return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
	}

	static double Distance(const Plane& plane, const AuPoint& point)
	{
		return plane.x * point.x + plane.y * point.y + plane.z * point.z + plane.d;
	}

public:
	AuViewFrustum() : m_Valid(false), m_PlaneCount(6) {}

	explicit AuViewFrustum(const AuMatrix4x4& clip) { Set(clip); }

	void Set(const AuMatrix4x4& clip, bool depthPlanes = true)
	{
		m_Valid = false;
		m_PlaneCount = depthPlanes ? 6 : 4;
		const double* matrix = clip.GLMatrix();
		for(int index = 0; index < 16; ++index)
			if(!std::isfinite(matrix[index])) return;
		for(int index = 0; index < m_PlaneCount; ++index)
		{
			const int row = index / 2;
			const double sign = (index & 1) ? -1. : 1.;
			// Keep an eight percent screen-edge allowance when the camera turns.
			// Depth planes retain the actual projection, including distant passes.
			const double margin = row < 2 ? 1.08 : 1.;
			Plane& plane = m_Planes[index];
			plane.x = margin * matrix[3] + sign * matrix[row];
			plane.y = margin * matrix[7] + sign * matrix[4 + row];
			plane.z = margin * matrix[11] + sign * matrix[8 + row];
			plane.d = margin * matrix[15] + sign * matrix[12 + row];
			const double length = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
			if(!std::isfinite(length) || length <= 0 || !std::isfinite(plane.d)) return;
			plane.x /= length;
			plane.y /= length;
			plane.z /= length;
			plane.d /= length;
			if(!std::isfinite(plane.d)) return;
		}
		m_Valid = true;
	}

	bool IntersectsSphere(const AuPoint& center, double radius) const
	{
		if(!m_Valid || !Finite(center) || !std::isfinite(radius) || radius < 0) return true;
		for(int index = 0; index < m_PlaneCount; ++index)
			if(Distance(m_Planes[index], center) < -radius - 1.e-6) return false;
		return true;
	}

	bool IntersectsBox(const AuPoint& minimum, const AuPoint& maximum) const
	{
		if(!m_Valid || !Finite(minimum) || !Finite(maximum)
			|| minimum.x > maximum.x || minimum.y > maximum.y || minimum.z > maximum.z) return true;
		for(int index = 0; index < m_PlaneCount; ++index)
		{
			const Plane& plane = m_Planes[index];
			const AuPoint support(plane.x >= 0 ? maximum.x : minimum.x,
				plane.y >= 0 ? maximum.y : minimum.y, plane.z >= 0 ? maximum.z : minimum.z);
			if(Distance(plane, support) < -1.e-6) return false;
		}
		return true;
	}

	// This overload also accepts rotated boxes; do not assume corners 0 and 6
	// still describe an axis-aligned box after an object transform.
	bool IntersectsBox(const AuBox& box) const
	{
		if(!m_Valid) return true;
		for(int corner = 0; corner < 8; ++corner)
			if(!Finite(box.m_fCorners[corner])) return true;
		for(int index = 0; index < m_PlaneCount; ++index)
		{
			bool outside = true;
			for(int corner = 0; corner < 8; ++corner)
				if(Distance(m_Planes[index], box.m_fCorners[corner]) >= -1.e-6)
				{
					outside = false;
					break;
				}
			if(outside) return false;
		}
		return true;
	}
};

#endif
