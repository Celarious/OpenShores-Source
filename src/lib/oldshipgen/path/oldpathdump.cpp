#include "oldpath.h"

#include <cstdio>
#include <cstring>

namespace OldPath
{

namespace
{

template<class T> void Put(std::vector<uint8_t>& out, size_t off, T v)
{
	if(out.size() < off + sizeof(T))
		out.resize(off + sizeof(T), 0);
	std::memcpy(&out[off], &v, sizeof(T));
}

std::string Hex(const std::vector<uint8_t>& b)
{
	static const char* d = "0123456789abcdef";
	std::string s;
	for(size_t i=0; i<b.size(); i++)
	{
		s += d[b[i] >> 4];
		s += d[b[i] & 15];
	}
	return s;
}

}

void Opening::Bytes(std::vector<uint8_t>& out) const
{
	out.assign(0x58, 0);
	Put(out, 0x00, id);
	Put(out, 0x04, type);
	Put(out, 0x08, deck);
	Put(out, 0x09, deck2);
	Put(out, 0x0a, room1);
	Put(out, 0x0c, room2);
	for(int i=0; i<3; i++)
	{
		Put(out, 0x10 + 8 * i, a[i]);
		Put(out, 0x28 + 8 * i, b[i]);
		Put(out, 0x40 + 8 * i, loc[i]);
	}
}

void ExtantCell::Bytes(std::vector<uint8_t>& out) const
{
	out.assign(0x18, 0);
	Put(out, 0, type);
	for(int i=0; i<8; i++)
		Put(out, 2 + 2 * i, rooms[i]);
}

void PathableCell::Bytes(std::vector<uint8_t>& out) const
{
	out.assign(0x20, 0);
	Put(out, 0x00, dirs);
	Put(out, 0x04, type);
	Put(out, 0x06, door);
	Put(out, 0x08, hole);
	Put(out, 0x0c, flags);
	Put(out, 0x10, railing);
	Put(out, 0x14, room1);
	Put(out, 0x16, room2);
	Put(out, 0x18, stairDir);
}

void PathCell::Bytes(std::vector<uint8_t>& out) const
{
	out.assign(0x38, 0);
	Put(out, 0x00, type);
	Put(out, 0x10, door);
	Put(out, 0x14, hole);
	Put(out, 0x18, stairFlags);
	Put(out, 0x1c, railing);
	Put(out, 0x20, room1);
	Put(out, 0x22, room2);
}

std::string Path::Dump() const
{
	std::string s = "{\n\"flag\": " + std::to_string((int)flag);
	std::vector<uint8_t> b;
	char buf[64];

	s += ",\n\"h0\": {\"nodeSize\": 40, \"entries\": {";
	bool first = true;
	for(std::map<uint32_t, ExtantCell>::const_iterator it = extant.begin(); it != extant.end(); ++it)
	{
		it->second.Bytes(b);
		std::snprintf(buf, sizeof buf, "%s\n\"%u\": {\"raw\": \"", first ? "" : ",", it->first);
		s += buf + Hex(b) + "\"}";
		first = false;
	}
	s += "}}";

	s += ",\n\"h10\": {\"nodeSize\": 48, \"entries\": {";
	first = true;
	for(std::map<uint32_t, PathableCell>::const_iterator it = pathable.begin(); it != pathable.end(); ++it)
	{
		it->second.Bytes(b);
		std::snprintf(buf, sizeof buf, "%s\n\"%u\": {\"raw\": \"", first ? "" : ",", it->first);
		s += buf + Hex(b) + "\"}";
		first = false;
	}
	s += "}}";

	s += ",\n\"h18\": {\"nodeSize\": 72, \"entries\": {";
	first = true;
	for(std::map<uint32_t, PathCell>::const_iterator it = cells.begin(); it != cells.end(); ++it)
	{
		const PathCell& c = it->second;
		std::snprintf(buf, sizeof buf, "%s\n\"%u\": {\"h8\": {", first ? "" : ",", it->first);
		s += buf;
		bool f2 = true;
		for(std::map<uint32_t, uint64_t>::const_iterator l = c.links.begin(); l != c.links.end(); ++l)
		{
			std::vector<uint8_t> v;
			Put(v, 0, l->second);
			std::snprintf(buf, sizeof buf, "%s\"%u\": \"", f2 ? "" : ", ", l->first);
			s += buf + Hex(v) + "\"";
			f2 = false;
		}
		s += "}, \"h28\": {";
		f2 = true;
		for(std::set<uint32_t>::const_iterator k = c.via1.begin(); k != c.via1.end(); ++k)
		{
			std::snprintf(buf, sizeof buf, "%s\"%u\": \"\"", f2 ? "" : ", ", *k);
			s += buf;
			f2 = false;
		}
		s += "}, \"h30\": {";
		f2 = true;
		for(std::set<uint32_t>::const_iterator k = c.via2.begin(); k != c.via2.end(); ++k)
		{
			std::snprintf(buf, sizeof buf, "%s\"%u\": \"\"", f2 ? "" : ", ", *k);
			s += buf;
			f2 = false;
		}
		c.Bytes(b);
		s += "}, \"raw\": \"" + Hex(b) + "\"}";
		first = false;
	}
	s += "}}\n}\n";
	return s;
}

}

namespace OldPath
{

namespace
{
template<class T> void Get(const uint8_t* b, size_t off, T& v)
{
	std::memcpy(&v, b + off, sizeof(T));
}
}

void ExtantCell::FromBytes(const uint8_t* b)
{
	Get(b, 0, type);
	for(int i=0; i<8; i++)
		Get(b, 2 + 2 * i, rooms[i]);
}

void PathableCell::FromBytes(const uint8_t* b)
{
	Get(b, 0x00, dirs);
	Get(b, 0x04, type);
	Get(b, 0x06, door);
	Get(b, 0x08, hole);
	Get(b, 0x0c, flags);
	Get(b, 0x10, railing);
	Get(b, 0x14, room1);
	Get(b, 0x16, room2);
	Get(b, 0x18, stairDir);
}

}
