#ifndef OLDBUILDING_H
#define OLDBUILDING_H

#include "model.h"

namespace OldBuilding
{

using OldShip::Vec3;
using OldShip::GridPoint;
using OldShip::FloorBuffer;

struct BVertex
{
	Vec3	pos;
	Vec3	normal;
	double	u, v;
	BVertex() : u(0), v(0) {}
	BVertex(const Vec3& p, const Vec3& n, double au, double av) : pos(p), normal(n), u(au), v(av) {}
};

struct BTri
{
	BVertex			v[3];
	unsigned char	window;
	int				door;
	BTri() : window(0), door(0) {}
};

typedef std::vector<BTri> BTriList;

struct Room
{
	uint32_t				design;
	int						id;
	int						deck;
	int						type;
	int						number;
	std::vector<GridPoint>	points;
	std::vector<Vec3>		outline;
	bool IsRoof() const { return type >= 22 && type <= 24; }
	void UpdateOutline();
};

typedef OldShip::Detail Detail;

struct Design
{
	uint32_t			id;
	std::string			name, architect;
	uint32_t			architectId;
	int					industry;
	int					module;
	int					state;
	std::vector<uint32_t> published;
	std::vector<Room>	rooms;
	std::vector<Detail>	details;
	bool				nullName, nullArchitect;

	Design() : id(0), architectId(0), industry(0), module(0), state(0), nullName(false), nullArchitect(false) {}
	bool Load(const std::string& path, std::string& why);
	bool Parse(const std::vector<uint8_t>& data, std::string& why);
	void Write(std::vector<uint8_t>& data) const;
	bool Save(const std::string& path, std::string& why) const;
	void DeckRange(int& lo, int& hi) const;
};

enum BuildingTex
{
	BT_Berth, BT_BerthBottom, BT_LargeDoor, BT_LargeCrewDoor, BT_LargeHullDoor, BT_CrewDoor, BT_CrewHatch, BT_Pole,
	BT_HullBottom, BT_HullDoor, BT_HullHatch, BT_HullTop, BT_HullTopBottomWindow, BT_HullWall, BT_HullTrim, BT_HullWallWindow,
	BT_IntCeiling, BT_IntDoor, BT_IntFloor, BT_IntHatch, BT_IntWallEndCap, BT_IntWallFinished, BT_IntWallFill,
	BT_IntWallCargo, BT_IntWallFillCargo, BT_IntWallEquipment, BT_IntWallFillEquipment, BT_Ladder, BT_LadderPole,
	BT_Railing, BT_RailingPost, BT_StairCasing, BT_StairRiser, BT_StairTread, BT_Surgery, BT_TransporterPad, BT_Turret,
	BT_Lamp, BT_38, BT_StainedGlass, BT_40, BT_41, BT_42, BT_43,
	BT_Count
};

void BuildingTexture(int tx, float& s, float& t, float& u, float& v);

struct TblProfile
{
	int				npoints;
	const double*	points;
	int				nwalls;
	const int*		walls;
	int				ncaps;
	const double*	caps;
};
extern const int TblBuildingTexture[44][4];
extern const TblProfile TblProfiles[];
extern const short TblProfileIndex[2][2][32][6];
const TblProfile& WallProfile(bool exterior, int roomType, int materialCommodity, bool belowGround);

int MaterialCommodity(int buildMaterial);
bool IsCandleLit(int industry);

typedef OldShip::Builder::Opening Opening;

class Builder
{
public:
	Builder(const Design& d, int materialCommodity, int industry, int lo, int hi);

	void CreateModels(BTriList& exterior, BTriList& interior) const;
	struct Sections
	{
		size_t	ground, shell, roofs, poles;
		Sections() : ground(0), shell(0), roofs(0), poles(0) {}
	};
	void RenderExterior(BTriList& out, bool inside, Sections* sections = 0) const;
	void RenderInterior(BTriList& out) const;
	void RenderInteriorDecks(BTriList& out, BTriList& ground) const;
	bool RenderFitting(BTriList& out, const Detail& d) const;
	void SetOpenings(const std::vector<Opening>& list, bool tagDoors = false) { openings = list; tag = tagDoors; }

	void RenderRoof(BTriList& out, const Room& room) const;
	void CreateFlagPole(BTriList& out, const Vec3& at) const;
	void CreateStreetLight(BTriList& out, const Vec3& at) const;
	void RenderExteriorWalls(BTriList& out, const std::vector<OldShip::Outline>& outlines, bool inside) const;

	const OldShip::Design& AsShip() const { return ship; }

private:
	void Take(BTriList& out, const OldShip::TriList& tris, size_t from, unsigned char window) const;
	unsigned char Window() const { return candleLit ? 1 : 2; }
	int DoorTag(uint32_t id) const;

	const Design&			m;
	int						material;
	int						industry;
	int						lo, hi;
	bool					candleLit;
	bool					tag;
	std::vector<Opening>	openings;
	mutable OldShip::Design	ship;
};

struct BuildingTextures
{
	BuildingTextures();
	~BuildingTextures();

private:
	OldShip::TextureSource before;
	BuildingTextures(const BuildingTextures&);
	BuildingTextures& operator=(const BuildingTextures&);
};

}

#endif
