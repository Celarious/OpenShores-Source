#include "oldship.h"

#include <cstdio>
#include <stdexcept>

namespace OldShip
{

namespace
{

class Reader
{
public:
	Reader(const std::vector<uint8_t>& d) : m(d), p(0) {}
	size_t Left() const { return m.size() - p; }
	uint8_t U8() { Need(1); return m[p++]; }
	int8_t I8() { return (int8_t)U8(); }
	uint16_t U16() { Need(2); uint16_t v = (uint16_t)((m[p] << 8) | m[p + 1]); p += 2; return v; }
	int16_t I16() { return (int16_t)U16(); }
	uint32_t U32() { Need(4); uint32_t v = ((uint32_t)m[p] << 24) | ((uint32_t)m[p + 1] << 16) | ((uint32_t)m[p + 2] << 8) | m[p + 3]; p += 4; return v; }
	int32_t I32() { return (int32_t)U32(); }
	std::string String(bool* isNull = 0)
	{
		const uint32_t n = U32();
		std::string out;
		if(isNull)
			*isNull = n == 0xffffffffu;
		if(n == 0xffffffffu)
			return out;
		Need(n);
		for(uint32_t i = 0; i + 1 < n; i += 2)
		{
			uint32_t c = (uint32_t)((m[p + i] << 8) | m[p + i + 1]);
			if(c >= 0xd800 && c < 0xdc00 && i + 3 < n)
			{
				const uint32_t lo = (uint32_t)((m[p + i + 2] << 8) | m[p + i + 3]);
				c = 0x10000 + ((c - 0xd800) << 10) + (lo - 0xdc00);
				i += 2;
			}
			if(c < 0x80) out += (char)c;
			else if(c < 0x800) { out += (char)(0xc0 | (c >> 6)); out += (char)(0x80 | (c & 0x3f)); }
			else if(c < 0x10000) { out += (char)(0xe0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 0x3f)); out += (char)(0x80 | (c & 0x3f)); }
			else { out += (char)(0xf0 | (c >> 18)); out += (char)(0x80 | ((c >> 12) & 0x3f)); out += (char)(0x80 | ((c >> 6) & 0x3f)); out += (char)(0x80 | (c & 0x3f)); }
		}
		p += n;
		return out;
	}

private:
	void Need(size_t n) { if(m.size() - p < n) throw std::runtime_error("the file ends early"); }
	const std::vector<uint8_t>& m;
	size_t p;
};

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
	try
	{
		Reader r(data);
		id = r.U32();
		name = r.String(&nullName);
		architectId = r.U32();
		streamline = r.I8();
		armor = r.U8();
		const uint8_t vs = r.U8();
		version = vs >> 4;
		state = vs & 0xf;
		architect = r.String(&nullArchitect);
		description.clear();
		nullDescription = false;
		if(version < 2)
			description = r.String(&nullDescription);
		minQuality = r.U8();
		hullVolume = 0;
		structure = 0;
		quality = 1;
		if(version > 1)
		{
			hullVolume = r.I32();
			structure = r.U8();
			quality = r.U8();
		}
		bayModules.clear();
		transporterModules.clear();
		for(int i = 0; i < 8; i++)
			modules[i] = 0;
		if(version != 0)
		{
			for(int32_t i = r.I32(); i > 0; i--) { const int k = r.I16(); const int v = r.I16(); bayModules.push_back(std::make_pair(k, v)); }
			for(int i = 0; i < 8; i++) modules[i] = r.I16();
			for(int32_t i = r.I32(); i > 0; i--) { const int k = r.I16(); const int v = r.I16(); transporterModules.push_back(std::make_pair(k, v)); }
		}

		for(int n = r.I16(); n > 0; n--)
		{
			Room room;
			room.design = r.U32();
			room.id = r.I16();
			room.deck = r.I8();
			room.type = r.U8();
			room.style = r.U8();
			for(int k = r.I16(); k > 0; k--)
			{
				GridPoint p;
				p.x = r.I8();
				p.y = r.I8();
				room.points.push_back(p);
			}
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
			details.push_back(d);
		}
		if(r.Left() != 0)
		{
			why = "unexpected data after the design";
			return false;
		}
	}
	catch(const std::exception& e)
	{
		why = e.what();
		return false;
	}

	UpdateDeckRange();
	return true;
}

void Design::UpdateDeckRange()
{
	minDeck = maxDeck = 0;
	for(size_t i = 0; i < rooms.size(); i++)
	{
		if(i == 0 || rooms[i].deck < minDeck) minDeck = rooms[i].deck;
		if(i == 0 || rooms[i].deck > maxDeck) maxDeck = rooms[i].deck;
	}
}

}
