#pragma once
#include <cstdint>
#include <vector>

typedef std::vector<uint8_t> ByteArray;
enum class Endian {
    None,
    Big,
    Little,
};

bool utf8to16(ByteArray& src, ByteArray& dst, Endian ret_endian);
bool utf8to32(ByteArray& src, ByteArray& dst, Endian ret_endian);
bool utf16to8(ByteArray& src, ByteArray& dst, Endian src_endian);
bool utf16to32(ByteArray& src, ByteArray& dst, Endian src_endian, Endian dst_endian);
bool utf32to8(ByteArray& src, ByteArray& dst, Endian src_endian);
bool utf32to16(ByteArray& src, ByteArray& dst, Endian src_endian, Endian dst_endian);