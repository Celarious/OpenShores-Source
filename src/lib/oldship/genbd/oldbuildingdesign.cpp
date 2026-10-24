#include "oldbuilding.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace OldBuilding
{

namespace
{

class Reader
{
public:
	explicit Reader(const std::vector<uint8_t>& data) : d(data), p(0) {}
	uint8_t U8() { Need(1); return d[p++]; }
	int8_t I8() { return (int8_t)U8(); }
	int16_t I16() { const uint16_t hi = U8(); return (int16_t)(hi << 8 | U8()); }
	uint32_t U32() { uint32_t v = 0; for(int i = 0; i < 4; i++) v = v << 8 | U8(); return v; }
	int32_t I32() { return (int32_t)U32(); }
	std::string String(bool& null)
	{
		const uint32_t n = U32();
		std::string s;
		null = n == 0xffffffffu;
		if(null)
			return s;
		Need(n);
		for(uint32_t i = 0; i + 1 < n; i += 2)
		{
			const unsigned c = d[p + i] << 8 | d[p + i + 1];
			if(c < 0x80)
				s += (char)c;
			else if(c < 0x800)
			{
				s += (char)(0xc0 | c >> 6);
				s += (char)(0x80 | (c & 0x3f));
			}
			else
			{
				s += (char)(0xe0 | c >> 12);
				s += (char)(0x80 | ((c >> 6) & 0x3f));
				s += (char)(0x80 | (c & 0x3f));
			}
		}
		p += n;
		return s;
	}
	size_t Left() const { return d.size() - p; }

private:
	void Need(size_t n) { if(d.size() - p < n) throw std::runtime_error("file ends early"); }
	const std::vector<uint8_t>& d;
	size_t p;
};

void CreateEllipse(std::vector<Vec3>& out, const Vec3& c, const Vec3& a, const Vec3& b, int n)
{
	out.clear();
	const double ax = a.x - c.x, ay = a.y - c.y, az = a.z - c.z;
	const double lenA = sqrt(ax * ax + ay * ay);
	const double bx = b.x - c.x, by = b.y - c.y, bz = b.z - c.z;
	const double lenB = sqrt(by * by + bx * bx);
	const double turn = 6.28318530718;
	for(int i = 0; i < n; i++)
	{
		const double angle = turn - ((double)i * turn) / (double)n;
		const double alongA = cos(angle) * lenA;
		const double alongB = sin(angle) * lenB;
		Vec3 pb, pa;
		if(alongB != 0.0)
			pb = Vec3((bx / lenB) * alongB, (by / lenB) * alongB, (bz / lenB) * alongB);
		if(alongA != 0.0)
			pa = Vec3((ax / lenA) * alongA, (ay / lenA) * alongA, (az / lenA) * alongA);
		out.push_back(Vec3(pa.x + c.x + pb.x, pa.y + c.y + pb.y, pa.z + c.z + pb.z));
	}
}

}

void Room::UpdateOutline()
{
	outline.clear();
	for(size_t i = 0; i < points.size(); i++)
		outline.push_back(Vec3(points[i].x * OldShip::GridUnit, points[i].y * OldShip::GridUnit, deck * OldShip::DeckHeight));
	if(type == 24 && outline.size() == 3)
	{
		const Vec3 c = outline[0], a = outline[1], b = outline[2];
		const double ra = OldShip::Distance2D(c, a);
		const double rb = OldShip::Distance2D(c, b);
		const float cells = ra <= rb ? (float)(rb / 5.656854249) : (float)(ra / 5.656854249);
		const float steps = ceilf(sqrtf(cells));
		CreateEllipse(outline, c, a, b, (int)steps * 4 + 4);
	}
}

bool Design::Load(const std::string& path, std::string& why)
{
	FILE* f = fopen(path.c_str(), "rb");
	if(!f)
	{
		why = "cannot open " + path;
		return false;
	}
	std::vector<uint8_t> data;
	uint8_t chunk[65536];
	size_t n;
	while((n = fread(chunk, 1, sizeof(chunk), f)) > 0)
		data.insert(data.end(), chunk, chunk + n);
	fclose(f);
	return Parse(data, why);
}

bool Design::Parse(const std::vector<uint8_t>& data, std::string& why)
{
	rooms.clear();
	details.clear();
	published.clear();
	try
	{
		Reader r(data);
		if(r.U8() != 0x1a)
			throw std::runtime_error("File is not a building design.");
		if(r.U8() != 1)
			throw std::runtime_error("Building design file version is obsolete.");
		id = r.U32();
		name = r.String(nullName);
		architectId = r.U32();
		architect = r.String(nullArchitect);
		industry = r.U8();
		module = r.I16();
		state = r.U8();
		for(int32_t i = r.I32(); i > 0; i--)
			published.push_back(r.U32());
		if(module == 0xfd) module = 0xfc;
		else if(module == 0xff) module = 0xfe;
		else if(module == 0xf9) module = 0xf8;
		else if(module == 0xfb) module = 0xfa;

		for(int n = r.I16(); n > 0; n--)
		{
			Room room;
			room.design = r.U32();
			room.id = r.I16();
			room.deck = r.I8();
			room.type = r.U8();
			room.number = r.I8();
			for(int16_t k = r.I16(); k != 0; k--)
			{
				GridPoint g;
				g.x = r.I8();
				g.y = r.I8();
				room.points.push_back(g);
			}
			room.UpdateOutline();
			rooms.push_back(room);
		}
		for(int32_t n = r.I32(); n > 0; n--)
		{
			Detail d;
			d.deck = r.I8();
			d.type = r.U8();
			d.flag = r.U8() != 0;
			d.id = r.I32();
			d.design = r.U32();
			d.x = r.I8();
			d.y = r.I8();
			d.room = r.I16();
			d.extra = r.I16();
			if(d.extra == 0xf9) d.extra = 0xf8;
			else if(d.extra == 0xfb) d.extra = 0xfa;
			details.push_back(d);
		}
		if(r.Left() != 0)
			throw std::runtime_error("bytes left over");
	}
	catch(const std::exception& e)
	{
		why = e.what();
		return false;
	}
	return true;
}

namespace
{

class Writer
{
public:
	explicit Writer(std::vector<uint8_t>& data) : d(data) {}
	void U8(unsigned v) { d.push_back((uint8_t)v); }
	void I16(int v) { U8((unsigned)v >> 8); U8((unsigned)v); }
	void U32(uint32_t v) { U8(v >> 24); U8(v >> 16); U8(v >> 8); U8(v); }
	void String(const std::string& s, bool null)
	{
		if(null && s.empty())
		{
			U32(0xffffffffu);
			return;
		}
		std::vector<unsigned> units;
		for(size_t i = 0; i < s.size(); )
		{
			const unsigned c = (unsigned char)s[i];
			if(c < 0x80)
			{
				units.push_back(c);
				i += 1;
			}
			else if(c < 0xe0 && i + 1 < s.size())
			{
				units.push_back((c & 0x1f) << 6 | ((unsigned char)s[i + 1] & 0x3f));
				i += 2;
			}
			else if(i + 2 < s.size())
			{
				units.push_back((c & 0x0f) << 12 | ((unsigned char)s[i + 1] & 0x3f) << 6 | ((unsigned char)s[i + 2] & 0x3f));
				i += 3;
			}
			else
				break;
		}
		U32((uint32_t)units.size() * 2);
		for(size_t i = 0; i < units.size(); i++)
			I16((int)units[i]);
	}

private:
	std::vector<uint8_t>& d;
};

}

void Design::Write(std::vector<uint8_t>& data) const
{
	data.clear();
	Writer w(data);
	w.U8(0x1a);
	w.U8(1);
	w.U32(id);
	w.String(name, nullName);
	w.U32(architectId);
	w.String(architect, nullArchitect);
	w.U8((unsigned)industry);
	w.I16(module);
	w.U8((unsigned)state);
	w.U32((uint32_t)published.size());
	for(size_t i = 0; i < published.size(); i++)
		w.U32(published[i]);
	w.I16((int)rooms.size());
	for(size_t i = 0; i < rooms.size(); i++)
	{
		const Room& room = rooms[i];
		w.U32(room.design);
		w.I16(room.id);
		w.U8((unsigned)room.deck);
		w.U8((unsigned)room.type);
		w.U8((unsigned)room.number);
		w.I16((int)room.points.size());
		for(size_t k = 0; k < room.points.size(); k++)
		{
			w.U8((unsigned)room.points[k].x);
			w.U8((unsigned)room.points[k].y);
		}
	}
	w.U32((uint32_t)details.size());
	for(size_t i = 0; i < details.size(); i++)
	{
		const Detail& d = details[i];
		w.U8((unsigned)d.deck);
		w.U8((unsigned)d.type);
		w.U8(d.flag ? 1 : 0);
		w.U32((uint32_t)d.id);
		w.U32(d.design);
		w.U8((unsigned)d.x);
		w.U8((unsigned)d.y);
		w.I16(d.room);
		w.I16(d.extra);
	}
}

bool Design::Save(const std::string& path, std::string& why) const
{
	std::vector<uint8_t> data;
	Write(data);
	FILE* f = fopen(path.c_str(), "wb");
	if(!f)
	{
		why = "cannot write " + path;
		return false;
	}
	const bool ok = data.empty() || fwrite(&data[0], 1, data.size(), f) == data.size();
	fclose(f);
	if(!ok)
		why = "cannot write all of " + path;
	return ok;
}

void Design::DeckRange(int& lo, int& hi) const
{
	lo = hi = 0;
	for(size_t i = 0; i < rooms.size(); i++)
	{
		if(i == 0 || rooms[i].deck < lo) lo = rooms[i].deck;
		if(i == 0 || rooms[i].deck > hi) hi = rooms[i].deck;
	}
}

}
