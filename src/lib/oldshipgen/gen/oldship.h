#ifndef OLDSHIP_H
#define OLDSHIP_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace OldShip
{

const double LatticeUnit = 1.41421356225;
const double GridUnit = 2.8284271245;
const double DeckHeight = 8.0;

struct Vec3
{
	double x, y, z;
	Vec3() : x(0), y(0), z(0) {}
	Vec3(double ax, double ay, double az) : x(ax), y(ay), z(az) {}
	Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
	Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
	Vec3 operator*(double s) const { return Vec3(x * s, y * s, z * s); }
};

struct Vertex
{
	Vec3	pos;
	float	u, v;
	Vec3	normal;
	float	shine;
	float	eu, ev;
	int		ints[3];
	Vertex() : u(0), v(0), shine(0), eu(0), ev(0) { ints[0] = ints[1] = ints[2] = 0; }
};

struct Tri
{
	Vertex	v[3];
};

typedef std::vector<Tri> TriList;

struct GridPoint
{
	int x, y;
};

struct Room
{
	uint32_t				design;
	int						id;
	int						deck;
	int						type;
	int						style;
	std::vector<GridPoint>	points;
};

struct Detail
{
	int			deck;
	int			type;
	bool		flag;
	uint32_t	design;
	int32_t		id;
	int			x, y;
	int			room;
	int			extra;
	Vec3 Loc() const { return Vec3(x * GridUnit, y * GridUnit, deck * DeckHeight); }
};

struct Design
{
	uint32_t			id;
	std::string			name;
	std::string			architect;
	std::string			description;
	int					version;
	int					state;
	int					armor;
	int					streamline;
	std::vector<Room>	rooms;
	std::vector<Detail>	details;
	int					minDeck, maxDeck;

	uint32_t			architectId;
	int					minQuality;
	int32_t				hullVolume;
	int					structure;
	int					quality;
	std::vector<std::pair<int, int> >	bayModules;
	int					modules[8];
	std::vector<std::pair<int, int> >	transporterModules;
	bool				nullName, nullArchitect, nullDescription;

	Design() : id(0), version(2), state(0), armor(0), streamline(0), minDeck(0), maxDeck(0), architectId(0), minQuality(1),
			hullVolume(0), structure(0), quality(1), nullName(false), nullArchitect(false), nullDescription(false)
	{
		for(int i = 0; i < 8; i++)
			modules[i] = 0;
	}
	bool Load(const std::string& path, std::string& why);
	bool Parse(const std::vector<uint8_t>& data, std::string& why);
	void Write(std::vector<uint8_t>& data) const;
	bool Save(const std::string& path, std::string& why) const;
	void UpdateDeckRange();
};

class FloorBuffer
{
public:
	enum { Size = 0x204, Origin = 0x100 };
	enum
	{
		Below = 0x8000,
		Covered = 0x4000,
		OpenBelow = 0x2000,
		Identity = 0x1fff,
		TypeMask = 0x1f00,
	};

	FloorBuffer();
	void Clear();
	uint16_t At(int x, int y) const { return (unsigned)x < (unsigned)Size && (unsigned)y < (unsigned)Size ? m[x * Size + y] : 0; }
	uint16_t& At(int x, int y) { return m[x * Size + y]; }
	const std::vector<uint16_t>& Data() const { return m; }

	static int Lattice(int grid) { return grid * 2 + Origin; }
	static double Feet(int lattice) { return (lattice - Origin) * LatticeUnit; }

	void FillFloor(const std::vector<GridPoint>& outline, int roomType, int style);
	void FillFloor(const Room& r) { FillFloor(r.points, r.type, r.style); }

	bool ChaseBoundary(std::vector<GridPoint>& points, std::vector<uint16_t>& segs);

	int CornerEdgeVertexTest(int x, int y, uint16_t& cell) const;
	int CornerEdgeVertexTest(int x, int y, uint16_t& cell, int cameFrom) const;
	int CenterEdgeVertexTest(int x, int y, uint16_t& cell) const;

private:
	std::vector<uint16_t> m;
};

}

#endif
