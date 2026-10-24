#include "oldship.h"

#include <utility>

namespace OldShip
{

namespace
{

inline int Sign(int from, int to)
{
	return to < from ? -1 : (from < to ? 1 : 0);
}

const int StepX[8] = { 4, 2, 0, -2, -4, -2, 0, 2 };
const int StepY[8] = { 0, 2, 4, 2, 0, -2, -4, -2 };

const int CornerAhead[8][2] = { { 2, 0 }, { 1, 1 }, { 0, 2 }, { -1, 1 }, { -2, 0 }, { -1, -1 }, { 0, -2 }, { 1, -1 } };
const int CornerLeft[8][2] = { { 2, 1 }, { 1, 2 }, { -1, 2 }, { -2, 1 }, { -2, -1 }, { -1, -2 }, { 1, -2 }, { 2, -1 } };
const int CornerRight[8][2] = { { 2, -1 }, { 2, 1 }, { 1, 2 }, { -1, 2 }, { -2, 1 }, { -2, -1 }, { -1, -2 }, { 1, -2 } };

const int CenterAhead[4][2] = { { 1, 1 }, { -1, 1 }, { -1, -1 }, { 1, -1 } };
const int CenterLeft[4][2] = { { 0, 1 }, { -1, 0 }, { 0, -1 }, { 1, 0 } };
const int CenterRight[4][2] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };

}

FloorBuffer::FloorBuffer() : m(Size * Size, 0)
{
}

void FloorBuffer::Clear()
{
	m.assign(Size * Size, 0);
}

void FloorBuffer::FillFloor(const std::vector<GridPoint>& outline, int roomType, int style)
{
	if(outline.empty())
		return;

	const uint16_t val = (uint16_t)(((roomType & 0x1f) << 8) | (style & 0xff));
	const bool openBelow = roomType == 11 || roomType == 15;

	int x = Lattice(outline.back().x);
	int y = Lattice(outline.back().y);
	int minX = x, maxX = x, minY = y, maxY = y;

	std::vector<uint16_t> tmp;
	if(openBelow)
		tmp.assign(Size * Size, 0);

	auto paint = [&](int px, int py)
	{
		const uint16_t was = At(px, py);
		if(!openBelow)
			At(px, py) = (was & (Below | Covered)) == 0 ? val : (uint16_t)(val | Covered);
		else if((was & (Below | Covered)) == 0)
			tmp[py * Size + px] = val;
		else if((was & TypeMask) == 0x900)
			tmp[py * Size + px] = (uint16_t)(val | Covered);
		else
			tmp[py * Size + px] = (uint16_t)((was & Identity) | Covered | OpenBelow);
	};

	std::vector<std::pair<int, int> > trace;
	for(size_t i = 0; i < outline.size(); i++)
	{
		const int tx = Lattice(outline[i].x);
		const int ty = Lattice(outline[i].y);
		if(tx < minX) minX = tx;
		if(tx > maxX) maxX = tx;
		if(ty < minY) minY = ty;
		if(ty > maxY) maxY = ty;
		const int sx = Sign(x, tx);
		const int sy = Sign(y, ty);
		int guard = Size * 2;
		while((x != tx || y != ty) && guard-- > 0)
		{
			paint(x, y);
			trace.push_back(std::make_pair(x, y));
			x += sx;
			y += sy;
		}
		x = tx;
		y = ty;
	}
	if(trace.size() < 2)
		return;

	std::pair<int, int> before = trace[trace.size() - 2];
	std::pair<int, int> at = trace[trace.size() - 1];
	for(size_t i = 0; i < trace.size(); i++)
	{
		const std::pair<int, int> next = trace[i];
		const int dxIn = at.first - before.first;
		const int dyIn = at.second - before.second;
		const int dxOut = next.first - at.first;
		const int dyOut = next.second - at.second;

		bool fill = false;
		if(dxOut == 1)
		{
			if(dyOut == 1)
				fill = !(dxIn == -1 && (dyIn == -1 || dyIn == 0));
		}
		else if(dxOut == 0)
		{
			if(dyOut == 1)
			{
				if(dxIn == -1)
					fill = !(dyIn == -1 || dyIn == 0);
				else
					fill = dxIn != 0 || dyIn != -1;
			}
			else if(dxIn == -1)
				fill = dyIn == 1;
		}
		else
		{
			if(dyOut == 1)
				fill = dyIn == 1 || (dxIn == 1 && dyIn == 0);
			else if(dyOut == 0 || dxIn != 1)
				fill = dyIn == 1;
		}

		if(fill)
		{
			for(int xx = at.first + 1; xx <= maxX; xx++)
			{
				if(openBelow)
				{
					if(tmp[at.second * Size + xx] != 0)
						break;
				}
				else if((At(xx, at.second) & (Below | Identity)) == val)
					break;
				paint(xx, at.second);
			}
		}

		before = at;
		at = next;
	}

	if(openBelow)
	{
		for(int yy = minY; yy <= maxY; yy++)
			for(int xx = minX; xx <= maxX; xx++)
				if(tmp[yy * Size + xx] != 0)
					At(xx, yy) = tmp[yy * Size + xx];
	}
}

int FloorBuffer::CornerEdgeVertexTest(int x, int y, uint16_t& cell) const
{
	if(At(x, y) == 0)
		return -1;
	int found = -1;
	for(int d = 0; d < 8; d++)
	{
		if(At(x + CornerAhead[d][0], y + CornerAhead[d][1]) == 0)
			continue;
		if(At(x + CornerLeft[d][0], y + CornerLeft[d][1]) != 0)
			continue;
		const uint16_t beside = At(x + CornerRight[d][0], y + CornerRight[d][1]);
		if(beside == 0 || found != -1)
			return -2;
		found = d;
		cell = beside & Identity;
	}
	return found;
}

int FloorBuffer::CornerEdgeVertexTest(int x, int y, uint16_t& cell, int cameFrom) const
{
	if(At(x, y) == 0)
		return -1;
	int d = cameFrom + 3;
	if(d >= 8)
		d -= 8;
	for(int n = 0; n < 7; n++)
	{
		if(At(x + CornerAhead[d][0], y + CornerAhead[d][1]) != 0 && At(x + CornerLeft[d][0], y + CornerLeft[d][1]) == 0)
		{
			const uint16_t beside = At(x + CornerRight[d][0], y + CornerRight[d][1]);
			if(beside == 0)
				return -2;
			cell = beside & Identity;
			return d;
		}
		d = d == 0 ? 7 : d - 1;
	}
	return -1;
}

int FloorBuffer::CenterEdgeVertexTest(int x, int y, uint16_t& cell) const
{
	if(At(x, y) == 0)
		return -1;
	int found = -1;
	for(int i = 0; i < 4; i++)
	{
		if(At(x + CenterAhead[i][0], y + CenterAhead[i][1]) == 0)
			continue;
		if(At(x + CenterLeft[i][0], y + CenterLeft[i][1]) != 0)
			continue;
		const uint16_t beside = At(x + CenterRight[i][0], y + CenterRight[i][1]);
		if(beside == 0 || found != -1)
			return -2;
		found = i * 2 + 1;
		cell = beside & Identity;
	}
	return found;
}

bool FloorBuffer::ChaseBoundary(std::vector<GridPoint>& points, std::vector<uint16_t>& segs)
{
	points.clear();
	segs.clear();

	int startX = 9999, startY = 9999, startDir = -1;
	uint16_t startCell = 0;
	bool onCorner = false;

	for(int y = 0; y < Size - 1 && startDir < 0; y += 4)
	{
		for(int x = 0; x < Size - 1; x += 4)
		{
			const uint16_t c = At(x, y);
			if(c == 0 || (c & Below))
				continue;
			int d = CornerEdgeVertexTest(x, y, startCell);
			if(d == -2)
				return false;
			if(d != -1)
			{
				onCorner = true;
				startX = x; startY = y; startDir = d;
				break;
			}
			d = CenterEdgeVertexTest(x, y, startCell);
			if(d == -2)
				return false;
			if(d != -1)
			{
				onCorner = false;
				startX = x + 2; startY = y + 2; startDir = d;
				break;
			}
		}
	}
	if(startDir < 0)
		return false;

	GridPoint p; p.x = startX; p.y = startY;
	points.push_back(p);
	segs.push_back(startCell);

	int x = startX, y = startY;
	int lastX = startX, lastY = startY;
	int dir = startDir;
	uint16_t cell = startCell;
	int guard = 10000;
	do
	{
		x += StepX[dir];
		y += StepY[dir];
		if(StepX[dir] != 0 && StepY[dir] != 0)
			onCorner = !onCorner;

		const int wasDir = dir;
		const uint16_t wasCell = cell;
		dir = onCorner ? CornerEdgeVertexTest(x, y, cell, wasDir) : CenterEdgeVertexTest(x, y, cell);
		if(dir == -2 || dir == -1)
			return false;

		if(dir != wasDir || cell != wasCell)
		{
			p.x = x; p.y = y;
			points.push_back(p);
			segs.push_back(cell);
			const int sx = Sign(lastX, x);
			const int sy = Sign(lastY, y);
			while(lastX != x || lastY != y)
			{
				At(lastX, lastY) |= Below;
				lastX += sx;
				lastY += sy;
			}
		}
		guard--;
	}
	while((x != startX || y != startY) && guard != 0);

	if(dir == startDir && cell == startCell)
	{
		points.erase(points.begin());
		segs.erase(segs.begin());
	}
	if(points.size() < 3)
	{
		points.clear();
		segs.clear();
	}
	return !points.empty();
}

}
