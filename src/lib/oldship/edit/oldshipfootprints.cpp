#include "oldshipedit.h"

#include "model.h"
#include "oldbuildingspots.h"
#include <cmath>

namespace OldShip
{

namespace
{

struct Shape
{
	int	outlineCount;
	int	outline[6][2];
	int	test[7][2];
};

const Shape Shapes[5][2] =
{
	{
		{ 4, { { 0, 0 }, { 0, 4 }, { 8, 4 }, { 8, 0 } },
			{ { 0, 4 }, { 0, 0 }, { 8, 4 }, { 8, 0 }, { 4, 4 }, { 4, 0 }, { 4, 2 } } },
		{ 4, { { 0, 0 }, { -2, 2 }, { 4, 8 }, { 6, 6 } },
			{ { -2, 2 }, { 0, 0 }, { 4, 8 }, { 6, 6 }, { 1, 5 }, { 3, 3 }, { 2, 4 } } }
	},
	{
		{ 6, { { 0, 0 }, { 0, 4 }, { 4, 4 }, { 4, 8 }, { 8, 8 }, { 8, 0 } },
			{ { 0, 4 }, { 0, 0 }, { 4, 8 }, { 8, 8 }, { 4, 4 }, { 8, 0 }, { 6, 2 } } },
		{ 6, { { 0, 0 }, { -2, 2 }, { 2, 6 }, { -2, 10 }, { 0, 12 }, { 6, 6 } },
			{ { -2, 2 }, { 0, 0 }, { -2, 10 }, { 0, 12 }, { 2, 6 }, { 6, 6 }, { 4, 6 } } }
	},
	{
		{ 6, { { 0, 0 }, { 8, 0 }, { 8, -8 }, { 4, -8 }, { 4, -4 }, { 0, -4 } },
			{ { 0, 0 }, { 0, -4 }, { 8, -8 }, { 4, -8 }, { 8, 0 }, { 4, -4 }, { 6, -2 } } },
		{ 6, { { 0, 0 }, { 6, 6 }, { 12, 0 }, { 10, -2 }, { 6, 2 }, { 2, -2 } },
			{ { 0, 0 }, { 2, -2 }, { 12, 0 }, { 10, -2 }, { 6, 6 }, { 6, 2 }, { 6, 4 } } }
	},
	{
		{ 4, { { 0, 0 }, { 0, 8 }, { 8, 8 }, { 8, 0 } },
			{ { 0, 8 }, { 0, 0 }, { 8, 8 }, { 8, 0 }, { 4, 8 }, { 4, 0 }, { 4, 4 } } },
		{ 4, { { 0, 0 }, { -6, 6 }, { 0, 12 }, { 6, 6 } },
			{ { -6, 6 }, { 0, 0 }, { 0, 12 }, { 6, 6 }, { -3, 9 }, { 3, 3 }, { 0, 6 } } }
	},
	{
		{ 4, { { 0, 0 }, { 8, 0 }, { 8, -8 }, { 0, -8 } },
			{ { 0, 0 }, { 0, -8 }, { 8, 0 }, { 8, -8 }, { 4, 0 }, { 4, -8 }, { 4, -4 } } },
		{ 4, { { 0, 0 }, { 6, 6 }, { 12, 0 }, { 6, -6 } },
			{ { 0, 0 }, { 6, -6 }, { 6, 6 }, { 12, 0 }, { 3, 3 }, { 9, -3 }, { 6, 0 } } }
	}
};

FootPoint Turned(const int p[2], int quarters)
{
	int x = p[0], y = p[1];
	for(int i = 0; i < quarters; i++)
	{
		const int was = x;
		x = -y;
		y = was;
	}
	FootPoint out;
	out.x = x / 2.0;
	out.y = y / 2.0;
	return out;
}

}

std::vector<FootPoint> StairFootprint(int detailType)
{
	std::vector<FootPoint> out;
	if(!DetailType::IsStairs(detailType))
		return out;
	const int kind = (detailType - 101) / 8, turn = (detailType - 101) % 8;
	const Shape& shape = Shapes[kind][turn & 1];
	for(int i = 0; i < shape.outlineCount; i++)
		out.push_back(Turned(shape.outline[i], turn / 2));
	return out;
}

std::vector<FootPoint> StairTestPoints(int detailType)
{
	std::vector<FootPoint> out;
	if(!DetailType::IsStairs(detailType))
		return out;
	const int kind = (detailType - 101) / 8, turn = (detailType - 101) % 8;
	const Shape& shape = Shapes[kind][turn & 1];
	for(int i = 0; i < 7; i++)
		out.push_back(Turned(shape.test[i], turn / 2));
	return out;
}

bool StairOnTiles(int detailType, int x, int y)
{
	if(!DetailType::IsStairs(detailType))
		return true;
	if((detailType - 101) & 1)
		return ((x + y) & 1) == 0;
	return (x & 1) == 0 && (y & 1) == 0;
}

void StairSnap(int detailType, double gx, double gy, int& x, int& y)
{
	if(DetailType::IsStairs(detailType) && !((detailType - 101) & 1))
	{
		x = 2 * (int)floor(gx / 2 + 0.5);
		y = 2 * (int)floor(gy / 2 + 0.5);
		return;
	}
	x = (int)floor(gx + 0.5);
	y = (int)floor(gy + 0.5);
	if(DetailType::IsStairs(detailType) && ((x + y) & 1))
	{
		const double dx = gx - x, dy = gy - y;
		if(fabs(dx) >= fabs(dy))
			x += dx >= 0 ? 1 : -1;
		else
			y += dy >= 0 ? 1 : -1;
	}
}

bool VehicleHalfSize(int commodity, double& halfLength, double& halfWidth)
{
	switch(commodity)
	{
	case VEHICLE_SpaceFighter:
	case VEHICLE_SpaceFighter2:
	case VEHICLE_SpaceTransport:
		SpotHalfSize(Spot_Space, halfLength, halfWidth);
		return true;
	case VEHICLE_Suv:
	case VEHICLE_Apc:
	case VEHICLE_MotorBike:
		SpotHalfSize(Spot_Ground, halfLength, halfWidth);
		return true;
	}
	halfLength = halfWidth = 0;
	return false;
}

std::vector<FootPoint> VehicleFootprint(int detailType, int commodity)
{
	double halfLength, halfWidth;
	VehicleHalfSize(commodity, halfLength, halfWidth);
	const double a = DetailType::Rotation(detailType), c = cos(a), s = sin(a);
	const double cx[4] = { -halfLength, -halfLength, halfLength, halfLength }, cy[4] = { -halfWidth, halfWidth, halfWidth, -halfWidth };
	std::vector<FootPoint> out;
	for(int i = 0; i < 4; i++)
	{
		FootPoint p;
		p.x = (cx[i] * c - cy[i] * s) / GridUnit;
		p.y = (cx[i] * s + cy[i] * c) / GridUnit;
		out.push_back(p);
	}
	return out;
}

bool PointInFootprint(const std::vector<FootPoint>& outline, double px, double py, bool orOnEdge)
{
	const size_t n = outline.size();
	if(n < 3)
		return false;
	bool inside = false;
	for(size_t i = 0; i < n; i++)
	{
		const double ax = outline[i].x, ay = outline[i].y;
		const double bx = outline[(i + 1) % n].x, by = outline[(i + 1) % n].y;
		const double cross = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
		if(fabs(cross) < 1e-9 && px >= (ax < bx ? ax : bx) - 1e-9 && px <= (ax > bx ? ax : bx) + 1e-9
				&& py >= (ay < by ? ay : by) - 1e-9 && py <= (ay > by ? ay : by) + 1e-9)
			return orOnEdge;
		if((ay > py) != (by > py) && px < ax + (py - ay) * (bx - ax) / (by - ay))
			inside = !inside;
	}
	return inside;
}

}
