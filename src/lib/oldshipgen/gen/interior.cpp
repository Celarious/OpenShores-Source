#include "model.h"

#include <cmath>

namespace OldShip
{

namespace
{

const double WallHeight = 8.0;
const double DoorHalfWidth = 2.5;
const float ShortWall = 5.0f;
const float WallTextureLength = 10.0f;
const double RailHeight = 3.0;
const double RailDepth = 0.3;
const double PostWidth = 0.1;
const int MaxWallSteps = 2048;

struct TexRect
{
	float s, t, u, v;
	explicit TexRect(ShipTex tex) { ShipTexture(tex, s, t, u, v); }
};

struct Compass
{
	double	nx, ny;
	double	dx, dy;
};

const Compass Headings[8] =
{
	{ 0.0, -1.0, GridUnit, 0.0 },
	{ 0.7071, -0.7071, GridUnit, GridUnit },
	{ 1.0, 0.0, 0.0, GridUnit },
	{ 0.7071, 0.7071, -GridUnit, GridUnit },
	{ 0.0, 1.0, -GridUnit, 0.0 },
	{ -0.7071, 0.7071, -GridUnit, -GridUnit },
	{ -1.0, 0.0, 0.0, -GridUnit },
	{ -0.7071, -0.7071, GridUnit, -GridUnit },
};

int Heading(const Vec3& dir)
{
	if(dir.x > 0.0)
		return dir.y == 0.0 ? 0 : dir.y > 0.0 ? 1 : 7;
	if(dir.x < 0.0)
		return dir.y > 0.0 ? 3 : dir.y == 0.0 ? 4 : 5;
	return dir.y > 0.0 ? 2 : dir.y < 0.0 ? 6 : 7;
}

bool FuzzyEquals2D(const Vec3& a, const Vec3& b)
{
	return fabs(a.x - b.x) <= 1e-5 && fabs(a.y - b.y) <= 1e-5;
}

inline int LatticeCoord(double feet)
{
	return (int)floor(feet / LatticeUnit + 0.5) + FloorBuffer::Origin;
}

enum WallFinish
{
	FinishPlain,
	FinishCargo,
	FinishEquipment,
};

WallFinish FinishOf(uint16_t cell)
{
	switch((cell >> 8) & 0x1f)
	{
	case 3: case 5: case 13:
		return FinishCargo;
	case 8: case 16: case 17: case 21:
		return FinishEquipment;
	}
	return FinishPlain;
}

struct WallTextures
{
	TexRect	rail, post;
	TexRect	door, crewDoor, largeDoor, largeCrewDoor;

	WallTextures()
		: rail(ST_Railing), post(ST_RailingPost),
		  door(ST_IntDoorHalfOpen), crewDoor(ST_CrewDoorHalfOpen),
		  largeDoor(ST_LargeDoorHalfOpen), largeCrewDoor(ST_LargeCrewDoorHalfOpen)
	{
	}
};

void WallQuad(TriList& out, const Vec3& normal, const int* ints, const Vec3& from, const Vec3& to,
		float uFrom, float uTo, float vLow, float vHigh)
{
	const Vec3 fromTop(from.x, from.y, from.z + WallHeight);
	const Vec3 toTop(to.x, to.y, to.z + WallHeight);
	ModelAdd(out, normal, ints, 0.0f, false,
			uFrom, vLow, -1.0f, -1.0f, from,
			uFrom, vHigh, -1.0f, -1.0f, fromTop,
			uTo, vHigh, -1.0f, -1.0f, toTop,
			uTo, vLow, -1.0f, -1.0f, to);
}

void PlainWall(TriList& out, const Vec3& normal, const int* ints, const Vec3& from, const Vec3& to, uint16_t roomCell)
{
	static const ShipTex WallTex[3] = { ST_IntWallFinished, ST_IntWallCargo, ST_IntWallEquipment };
	static const ShipTex FillTex[3] = { ST_IntWallFill, ST_IntWallFillCargo, ST_IntWallFillEquipment };
	const WallFinish finish = FinishOf(roomCell);
	const float length = (float)Distance2D(from, to);
	if(length <= ShortWall)
	{
		const TexRect fill(FillTex[finish]);
		WallQuad(out, normal, ints, from, to, fill.s, fill.u, fill.t, fill.v);
	}
	else
	{
		const TexRect wall(WallTex[finish]);
		WallQuad(out, normal, ints, from, to, 0.0f, ceilf(length / WallTextureLength), wall.t, wall.v);
	}
}

void Triangle(TriList& out, const Vec3& normal, const int* ints,
		const Vec3& p0, float u0, float v0, const Vec3& p1, float u1, float v1, const Vec3& p2, float u2, float v2)
{
	ModelAdd(out, normal, ints, 0.0f, false, u0, v0, -1.0f, -1.0f, p0, u1, v1, -1.0f, -1.0f, p1, u2, v2, -1.0f, -1.0f, p2);
}

void Railing(TriList& out, const Vec3& normal, const int* ints, const Vec3& from, const Vec3& to, const Vec3& dir,
		const TexRect& rail, const TexRect& post)
{
	const Vec3 inset = dir * PostWidth;
	const double toTop = to.z + RailHeight;
	const double toLow = toTop - RailDepth;
	const double fromTop = from.z + RailHeight;
	const double fromLow = fromTop - RailDepth;

	Triangle(out, normal, ints, Vec3(from.x, from.y, fromLow), rail.s, rail.t,
			Vec3(from.x, from.y, fromTop), rail.s, rail.v, Vec3(to.x, to.y, toTop), rail.u, rail.v);
	Triangle(out, normal, ints, Vec3(to.x, to.y, toTop), rail.u, rail.v,
			Vec3(to.x, to.y, toLow), rail.u, rail.t, Vec3(from.x, from.y, fromLow), rail.s, rail.t);

	const double px = inset.x + from.x;
	const double py = inset.y + from.y;
	const double pTop = (from.z + RailHeight) - RailDepth;
	Triangle(out, normal, ints, from, post.s, post.t,
			Vec3(from.x, from.y, pTop), post.s, post.v, Vec3(px, py, pTop), post.u, post.v);
	Triangle(out, normal, ints, Vec3(px, py, pTop), post.u, post.v,
			Vec3(px, py, from.z), post.u, post.t, from, post.s, post.t);

	const double qTop = (to.z + RailHeight) - RailDepth;
	const double qx = to.x - inset.x;
	const double qy = to.y - inset.y;
	Triangle(out, normal, ints, to, post.s, post.t,
			Vec3(qx, qy, to.z), post.u, post.t, Vec3(qx, qy, qTop), post.u, post.v);
	Triangle(out, normal, ints, Vec3(qx, qy, qTop), post.u, post.v,
			Vec3(to.x, to.y, qTop), post.s, post.v, to, post.s, post.t);
}

}

bool Builder::IsExteriorWall(const Vec3& at, int heading, const FloorBuffer& fb) const
{
	static const int FromCorner[8][2] = { { 2, 1 }, { 1, 2 }, { -1, 2 }, { -2, 1 }, { -2, -1 }, { -1, -2 }, { 1, -2 }, { 2, -1 } };
	const int x = LatticeCoord(at.x);
	const int y = LatticeCoord(at.y);
	if(heading < 0 || heading > 7)
		return false;

	uint16_t left;
	if((x & 3) == 0 && (y & 3) == 0)
		left = fb.At(x + FromCorner[heading][0], y + FromCorner[heading][1]);
	else if((x & 3) == 0 && (y & 1) == 0)
	{
		if(heading == 2)
			left = fb.At(x - 1, y);
		else if(heading == 6)
			left = fb.At(x + 1, y);
		else
			return false;
	}
	else if((x & 1) == 0 && (y & 3) == 0)
	{
		if(heading == 0)
			left = fb.At(x, y + 1);
		else if(heading == 4)
			left = fb.At(x, y - 1);
		else
			return false;
	}
	else if((x & 1) != 0 || (y & 1) != 0)
		return false;
	else
	{
		if(heading == 1)
			left = fb.At(x, y + 1);
		else if(heading == 3)
			left = fb.At(x - 1, y);
		else if(heading == 5)
			left = fb.At(x, y - 1);
		else if(heading == 7)
			left = fb.At(x + 1, y);
		else
			return false;
	}
	return left == 0 || (left & FloorBuffer::Below) != 0;
}

void Builder::AdjacentRoomsData(const Vec3& at, int heading, const FloorBuffer& fb, uint16_t& left, uint16_t& right) const
{
	static const int LeftOfCorner[8][2] = { { 2, 1 }, { 1, 2 }, { -1, 2 }, { -2, 1 }, { -2, -1 }, { -1, -2 }, { 1, -2 }, { 2, -1 } };
	static const int RightOfCorner[8][2] = { { 2, -1 }, { 2, 1 }, { 1, 2 }, { -1, 2 }, { -2, 1 }, { -2, -1 }, { -1, -2 }, { 1, -2 } };
	const int x = LatticeCoord(at.x);
	const int y = LatticeCoord(at.y);
	if(heading < 0 || heading > 7)
		return;

	if((x & 3) == 0 && (y & 3) == 0)
	{
		left = fb.At(x + LeftOfCorner[heading][0], y + LeftOfCorner[heading][1]);
		right = fb.At(x + RightOfCorner[heading][0], y + RightOfCorner[heading][1]);
	}
	else if((x & 3) == 0 && (y & 1) == 0)
	{
		if(heading == 2)
		{
			left = fb.At(x - 1, y);
			right = fb.At(x + 1, y);
		}
		else if(heading == 6)
		{
			left = fb.At(x + 1, y);
			right = fb.At(x - 1, y);
		}
	}
	else if((x & 1) == 0 && (y & 3) == 0)
	{
		if(heading == 0)
		{
			left = fb.At(x, y + 1);
			right = fb.At(x, y - 1);
		}
		else if(heading == 4)
		{
			left = fb.At(x, y - 1);
			right = fb.At(x, y + 1);
		}
	}
	else if((x & 1) == 0 && (y & 1) == 0)
	{
		if(heading == 1)
		{
			left = fb.At(x, y + 1);
			right = fb.At(x + 1, y);
		}
		else if(heading == 3)
		{
			left = fb.At(x - 1, y);
			right = fb.At(x, y + 1);
		}
		else if(heading == 5)
		{
			left = fb.At(x, y - 1);
			right = fb.At(x - 1, y);
		}
		else if(heading == 7)
		{
			left = fb.At(x + 1, y);
			right = fb.At(x, y - 1);
		}
	}
}

void Builder::RenderInteriorWalls(TriList& out, const Room& room, const FloorBuffer& fb) const
{
	static const WallTextures tx;
	int ints[3] = { 0, 0, 0 };
	const int animated = AnimatedDoors();
	const double floorZ = room.deck * DeckHeight;

	Vec3 from(room.points.back().x * GridUnit, room.points.back().y * GridUnit, floorZ);
	for(size_t i = 0; i < room.points.size(); i++)
	{
		const Vec3 corner(room.points[i].x * GridUnit, room.points[i].y * GridUnit, floorZ);
		const Vec3 dir = Unit(corner - from);
		const int heading = Heading(dir);
		const Compass& way = Headings[heading];
		const Vec3 normal(way.nx, way.ny, 0.0);

		Vec3 at = from;
		bool exterior = IsExteriorWall(at, heading, fb);
		uint16_t left = 0, right = 0;
		AdjacentRoomsData(at, heading, fb, left, right);
		left &= FloorBuffer::Identity;
		right &= FloorBuffer::Identity;
		int openingType = 0;
		int openingId = 0;

		bool atCorner = false;
		for(int guard = 0; !atCorner && guard < MaxWallSteps; guard++)
		{
			at = Vec3(at.x + way.dx, at.y + way.dy, at.z + 0.0);
			atCorner = FuzzyEquals(at, corner);
			const bool exteriorNext = IsExteriorWall(at, heading, fb);
			uint16_t leftNext = 0, rightNext = 0;
			AdjacentRoomsData(at, heading, fb, leftNext, rightNext);
			leftNext &= FloorBuffer::Identity;
			rightNext &= FloorBuffer::Identity;
			const bool wallEnds = atCorner || exteriorNext != exterior || leftNext != left || rightNext != right;

			int doorType = 0;
			int doorId = 0;
			if(!wallEnds)
			{
				for(size_t k = 0; k < m.details.size(); k++)
				{
					const Detail& d = m.details[k];
					if(d.deck != room.deck)
						continue;
					if(d.type == 5 || d.type == 49 || d.type == 50)
					{
						if(FuzzyEquals2D(d.Loc(), at))
						{
							doorId = d.id;
							doorType = d.type;
							break;
						}
					}
					else if(d.type >= 53 && d.type <= 56)
					{
						if(FuzzyEquals2D(d.Loc(), at))
						{
							openingId = d.id;
							openingType = d.type;
							break;
						}
					}
				}
				if(exterior || doorId == 0)
					continue;
			}

			if(exterior)
				from = at;
			else if(wallEnds)
			{
				if(openingId == 0)
					PlainWall(out, normal, ints, from, at, right);
				else
				{
					if(openingType == 53 || openingType == 54)
					{
						const int door = DoorIndexDesignLocked(openingId);
						if(door >= 0)
							ints[0] = door < animated ? door + 10 : animated + 9;
						const TexRect& t = openingType == 53 ? tx.largeDoor : tx.largeCrewDoor;
						WallQuad(out, normal, ints, from, at, t.s, t.u, t.t, t.v);
						ints[0] = 0;
					}
					else if(openingType == 56)
						Railing(out, normal, ints, from, at, dir, tx.rail, tx.post);
					openingType = 0;
					openingId = 0;
				}
				from = at;
			}
			else
			{
				const Vec3 back = dir * DoorHalfWidth;
				const Vec3 doorFrom(at.x - back.x, at.y - back.y, at.z - back.z);
				PlainWall(out, normal, ints, from, doorFrom, right);

				const int door = DoorIndexDesignLocked(doorId);
				if(door >= 0)
					ints[0] = door < animated ? door + 10 : animated + 9;
				const Vec3 doorTo(at.x + back.x, at.y + back.y, at.z + back.z);
				const TexRect& t = doorType == 5 ? tx.door : tx.crewDoor;
				WallQuad(out, normal, ints, doorFrom, doorTo, t.s, t.u, t.t, t.v);
				ints[0] = 0;
				from = doorTo;
			}
			exterior = exteriorNext;
			left = leftNext;
			right = rightNext;
		}
		from = corner;
	}
}

void Builder::RenderInteriorDecks(TriList& out) const
{
	FloorBuffer fb;

	for(int deck = m.minDeck; deck <= m.maxDeck + 1; deck++)
	{
		const double z = deck * DeckHeight;
		for(size_t i = 0; i < m.rooms.size(); i++)
			if(m.rooms[i].deck == deck)
				fb.FillFloor(m.rooms[i]);

		int minX = FloorBuffer::Size, maxX = 0, minY = FloorBuffer::Size, maxY = 0;
		RenderInteriorCeilings(out, z, minX, maxX, minY, maxY, fb);
		RenderInteriorFloors(out, ST_IntFloor, z, false, minX, maxX, minY, maxY, fb);
		RenderInteriorFloors(out, ST_HullHatchHalfOpen, z, true, minX, maxX, minY, maxY, fb);

		if(deck > m.maxDeck || maxX >= FloorBuffer::Size || maxY >= FloorBuffer::Size)
			continue;
		maxX += 4;
		maxY += 4;

		for(int y = minY; y <= maxY; y++)
			for(int x = minX; x <= maxX; x++)
				if(fb.At(x, y) & FloorBuffer::Below)
					fb.At(x, y) = 0;

		std::vector<GridPoint> lattice;
		std::vector<uint16_t> segs;
		while(fb.ChaseBoundary(lattice, segs))
			;

		for(size_t i = 0; i < m.rooms.size(); i++)
		{
			const Room& room = m.rooms[i];
			if(room.deck != deck || room.type == 9 || room.type == 22 || room.type == 23 || room.type == 24 || room.points.empty())
				continue;
			RenderInteriorWalls(out, room, fb);
		}

		for(int y = minY; y <= maxY; y++)
			for(int x = minX; x <= maxX; x++)
				if(fb.At(x, y) != 0)
					fb.At(x, y) = (uint16_t)((fb.At(x, y) & ~FloorBuffer::OpenBelow) | FloorBuffer::Below);
	}
}

bool Builder::RenderFitting(TriList& out, const Detail& d) const
{
	if(d.flag)
		return false;
	const int t = d.type;
	{
		if(t == 2 || (t >= 25 && t <= 31))
			RenderBerthDouble(out, d.Loc(), (double)DetailType::Rotation(t));
		else if(t == 8 || t == 51 || t == 52 || (t >= 77 && t <= 97))
			RenderLadder(out, d.Loc(), DetailType::Rotation(t));
		else if(t == 10)
			RenderDriveUnit(out, ST_GravityDrive, d.Loc());
		else if(t == 11)
			RenderSurgeryUnit(out, d.Loc());
		else if(t == 14)
			RenderDriveUnit(out, ST_PowerPlant, d.Loc());
		else if(t == 16)
			RenderSensor(out, d.Loc());
		else if(t == 18)
			RenderShield(out, d.Loc());
		else if(t == 20)
			RenderTransporterPad(out, d.Loc(), (uint16_t)d.room, m.rooms);
		else if(t == 23)
			RenderWeapon(out, d.Loc(), (uint16_t)d.room, m.rooms);
		else if(t == 24)
			RenderDriveUnit(out, ST_FTLDrive, d.Loc());
		else if(t == 32)
			RenderLifeSupport(out, d.Loc());
		else if(t >= 65 && t <= 72)
			RenderBerthSingle(out, d.Loc(), (double)DetailType::Rotation(t));
		else if(t < 101 || t > 140)
			return false;
	}

	if(t >= 101 && t <= 140)
	{
		const int shape = (d.type - 101) / 8;
		const bool diagonal = ((d.type - 101) & 1) != 0;
		const double rot = (double)DetailType::Rotation(d.type);
		switch(shape)
		{
		case 0: RenderStairsStraight(out, d.Loc(), rot, diagonal); break;
		case 1: RenderStairsTurnLeft(out, d.Loc(), rot, diagonal); break;
		case 2: RenderStairsTurnRight(out, d.Loc(), rot, diagonal); break;
		case 3: RenderStairsUTurnLeft(out, d.Loc(), rot, diagonal); break;
		case 4: RenderStairsUTurnRight(out, d.Loc(), rot, diagonal); break;
		}
	}
	return true;
}

void Builder::RenderInterior(TriList& out) const
{
	RenderInteriorDecks(out);

	std::vector<const Detail*> stairs;
	for(size_t i = 0; i < m.details.size(); i++)
	{
		const Detail& d = m.details[i];
		if(d.type >= 101 && d.type <= 140)
		{
			if(!d.flag)
				stairs.push_back(&d);
		}
		else
			RenderFitting(out, d);
	}
	for(size_t i = 0; i < stairs.size(); i++)
		RenderFitting(out, *stairs[i]);
}

}
