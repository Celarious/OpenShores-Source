#include "oldship.h"

#include <cstdio>

namespace OldShip
{

namespace
{

class Writer
{
public:
	Writer(std::vector<uint8_t>& d) : m(d) {}
	void U8(uint32_t v) { m.push_back((uint8_t)v); }
	void U16(uint32_t v) { U8(v >> 8); U8(v); }
	void U32(uint32_t v) { U16(v >> 16); U16(v); }
	void String(const std::string& s, bool isNull)
	{
		if(isNull && s.empty())
		{
			U32(0xffffffffu);
			return;
		}
		std::vector<uint16_t> units;
		for(size_t i = 0; i < s.size(); )
		{
			const uint8_t b = (uint8_t)s[i];
			uint32_t c;
			int more;
			if(b < 0x80) { c = b; more = 0; }
			else if(b < 0xe0) { c = b & 0x1f; more = 1; }
			else if(b < 0xf0) { c = b & 0x0f; more = 2; }
			else { c = b & 0x07; more = 3; }
			i++;
			for(; more > 0 && i < s.size(); more--, i++)
				c = (c << 6) | ((uint8_t)s[i] & 0x3f);
			if(c >= 0x10000)
			{
				c -= 0x10000;
				units.push_back((uint16_t)(0xd800 + (c >> 10)));
				units.push_back((uint16_t)(0xdc00 + (c & 0x3ff)));
			}
			else
				units.push_back((uint16_t)c);
		}
		U32((uint32_t)units.size() * 2);
		for(size_t i = 0; i < units.size(); i++)
			U16(units[i]);
	}

private:
	std::vector<uint8_t>& m;
};

}

void Design::Write(std::vector<uint8_t>& data) const
{
	data.clear();
	Writer w(data);
	w.U32(id);
	w.String(name, nullName);
	w.U32(architectId);
	w.U8((uint32_t)streamline);
	w.U8((uint32_t)armor);
	w.U8((uint32_t)((version << 4) | (state & 0xf)));
	w.String(architect, nullArchitect);
	if(version < 2)
		w.String(description, nullDescription);
	w.U8((uint32_t)minQuality);
	if(version > 1)
	{
		w.U32((uint32_t)hullVolume);
		w.U8((uint32_t)structure);
		w.U8((uint32_t)quality);
	}
	if(version != 0)
	{
		w.U32((uint32_t)bayModules.size());
		for(size_t i = 0; i < bayModules.size(); i++) { w.U16((uint32_t)bayModules[i].first); w.U16((uint32_t)bayModules[i].second); }
		for(int i = 0; i < 8; i++) w.U16((uint32_t)modules[i]);
		w.U32((uint32_t)transporterModules.size());
		for(size_t i = 0; i < transporterModules.size(); i++) { w.U16((uint32_t)transporterModules[i].first); w.U16((uint32_t)transporterModules[i].second); }
	}

	w.U16((uint32_t)rooms.size());
	for(size_t i = 0; i < rooms.size(); i++)
	{
		const Room& room = rooms[i];
		w.U32(room.design);
		w.U16((uint32_t)room.id);
		w.U8((uint32_t)room.deck);
		w.U8((uint32_t)room.type);
		w.U8((uint32_t)room.style);
		w.U16((uint32_t)room.points.size());
		for(size_t k = 0; k < room.points.size(); k++)
		{
			w.U8((uint32_t)room.points[k].x);
			w.U8((uint32_t)room.points[k].y);
		}
	}
	w.U32((uint32_t)details.size());
	for(size_t i = 0; i < details.size(); i++)
	{
		const Detail& d = details[i];
		w.U8((uint32_t)d.deck);
		w.U8((uint32_t)d.type);
		w.U8(d.flag ? 1 : 0);
		w.U32((uint32_t)d.id);
		w.U32(d.design);
		w.U8((uint32_t)d.x);
		w.U8((uint32_t)d.y);
		w.U16((uint32_t)d.room);
		w.U16((uint32_t)d.extra);
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
	const size_t written = data.empty() ? 0 : fwrite(&data[0], 1, data.size(), f);
	const bool closed = fclose(f) == 0;
	if(written != data.size() || !closed)
	{
		why = "could not write all of " + path;
		return false;
	}
	return true;
}

}
