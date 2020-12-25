#include "unicode.hpp"

namespace{
constexpr Endian host_endian = Endian::Little;
inline void swap(char32_t& val) {
    val = ((val & 0xFF000000) >> 24) |
          ((val & 0x00FF0000) >> 8) |
          ((val & 0x0000FF00) << 8) |
          ((val & 0x000000FF) << 24);
}
inline void swap(char16_t& val) {
    val = ((val & 0xFF00) >> 8) |
          ((val & 0x00FF) << 8);
}
inline bool char32to8(uint8_t* src, ByteArray& dst, Endian src_endian, size_t& read, size_t& appended) {
    read     = 0;
    appended = 0;
    if(src_endian == Endian::None) return false;
    char32_t ch32 = *reinterpret_cast<char32_t*>(src);
    if(src_endian != host_endian) swap(ch32);
    if(ch32 > 0x0010FFFF) return false;

    if(ch32 <= 0x7F) {
        appended = 1;
        dst.emplace_back(ch32);
    } else if(ch32 <= 0x7FF) {
        appended = 2;
        dst.emplace_back(0xC0 | (ch32 >> 6));
        dst.emplace_back(0x80 | (ch32 & 0b00111111));
    } else if(ch32 <= 0xFFFF) {
        appended = 3;
        dst.emplace_back(0xE0 | (ch32 >> 12));
        dst.emplace_back(0x80 | ((ch32 >> 6) & 0b00111111));
        dst.emplace_back(0x80 | (ch32 & 0b00111111));
    } else {
        appended = 4;
        dst.emplace_back(0xF0 | (ch32 >> 18));
        dst.emplace_back(0x80 | ((ch32 >> 12) & 0b00111111));
        dst.emplace_back(0x80 | ((ch32 >> 6) & 0b00111111));
        dst.emplace_back(0x80 | (ch32 & 0b00111111));
    }
    read = 4;
    return true;
}
inline size_t char8bytes(uint8_t ch8) {
    if(ch8 < 0x80) {
        return 1;
    }
    if(0xC2 <= ch8 && ch8 < 0xE0) {
        return 2;
    }
    if(0xE0 <= ch8 && ch8 < 0xF0) {
        return 3;
    }
    if(0xF0 <= ch8 && ch8 < 0xF8) {
        return 4;
    }
    return 0;
}
inline bool is_laterbyte(uint8_t ch8){
    return 0x80 <= ch8 && ch8 < 0xC0;
}
inline bool char8to32(uint8_t* src, ByteArray& dst, Endian dst_endian, size_t& read, size_t& appended) {
    read     = 0;
    appended = 0;
    if(dst_endian == Endian::None) return false;

    read = char8bytes(*src);
    if(read == 0) return false;
    char32_t c32;
    switch(read) {
    case 1:
        c32 = char32_t(src[0]);
        break;
    case 2:
        if((src[0] & 0x1E) == 0) return false;
        if(!is_laterbyte(src[1])) return false;

        c32 = char32_t(src[0] & 0x1F) << 6;
        c32 |= char32_t(src[1] & 0x3F);
        break;
    case 3:
        if((src[0] & 0x0F) == 0 &&
           (src[1] & 0x20) == 0) return false;
        if(!is_laterbyte(src[1]) || !is_laterbyte(src[2])) return false;

        c32 = char32_t(src[0] & 0x0F) << 12;
        c32 |= char32_t(src[1] & 0x3F) << 6;
        c32 |= char32_t(src[2] & 0x3F);
        break;
    case 4:
        if((src[0] & 0x07) == 0 &&
           (src[1] & 0x30) == 0) return false;
        if(!is_laterbyte(src[1]) || !is_laterbyte(src[2]) || !is_laterbyte(src[3])) return false;

        c32 = char32_t(src[0] & 0x07) << 18;
        c32 |= char32_t(src[1] & 0x3F) << 12;
        c32 |= char32_t(src[2] & 0x3F) << 6;
        c32 |= char32_t(src[3] & 0x3F);
        break;
    default:
        return false;
    }
    if(dst_endian != host_endian) swap(c32);
    dst.emplace_back(c32 >> 0 & 0xFF);
    dst.emplace_back(c32 >> 8 & 0xFF);
    dst.emplace_back(c32 >> 16 & 0xFF);
    dst.emplace_back(c32 >> 24 & 0xFF);
    appended = 4;
    return true;
}
inline bool char32to16(uint8_t* src, ByteArray& dst, Endian src_endian, Endian dst_endian, size_t& read, size_t& appended) {
    read     = 0;
    appended = 0;
    if(src_endian == Endian::None) return false;
    if(dst_endian == Endian::None) return false;
    char32_t ch32 = *reinterpret_cast<char32_t*>(src);
    if(src_endian != host_endian) swap(ch32);
    if(ch32 > 0x0010FFFF) return false;

    if(ch32 < 0x10000) {
        appended = 2;
        dst.emplace_back(ch32 & 0x00FF);
        dst.emplace_back((ch32 & 0xFF00) >> 8);
        if(dst_endian != host_endian){
            swap(*reinterpret_cast<char16_t*>(&*(dst.end() - 2)));
        }
    } else {
        appended = 4;
        char16_t first  = (ch32 - 0x10000) / 0x400 + 0xD800;
        char16_t second = (ch32 - 0x10000) % 0x400 + 0xDC00;
        dst.emplace_back(first & 0x00FF);
        dst.emplace_back((first & 0xFF00) >> 8);
        dst.emplace_back(second & 0x00FF);
        dst.emplace_back((second & 0xFF00) >> 8);
        if(dst_endian != host_endian) {
            swap(*reinterpret_cast<char16_t*>(&*(dst.end() - 2)));
            swap(*reinterpret_cast<char16_t*>(&*(dst.end() - 4)));
        }
    }
    read = 4;
    return true;
}
inline bool is_surrogate_highbyte(char16_t ch) {
    return 0xD800 <= ch && ch < 0xDC00;
}
inline bool is_surrogate_lowbyte(char16_t ch) {
    return 0xDC00 <= ch && ch < 0xE000;
}
inline bool char16to32(uint8_t* src, ByteArray& dst, Endian src_endian, Endian dst_endian, size_t& read, size_t& appended) {
    read           = 0;
    appended       = 0;
    char16_t first = *reinterpret_cast<char16_t*>(src);
    if(src_endian != host_endian) swap(first);
    char32_t c32;
    if(is_surrogate_highbyte(first)) {
        char16_t second = *reinterpret_cast<char16_t*>(src + 2);
        if(src_endian != host_endian) swap(second);
        if(is_surrogate_lowbyte(second)) {
            c32 = 0x10000 + (char32_t(first) - 0xD800) * 0x400 + (char32_t(second) - 0xDC00);
            read = 4;
        } else if(second == 0) {
            c32  = first;
            read = 2;
        } else {
            return false;
        }
    } else if(is_surrogate_lowbyte(first)) {
        char16_t second = *reinterpret_cast<char16_t*>(src + 2);
        if(src_endian != host_endian) swap(second);
        if(second == 0) {
            c32  = first;
            read = 2;
        } else {
            return false;
        }
    } else {
        c32  = first;
        read = 2;
    }
    if(dst_endian != host_endian) swap(c32);
    dst.emplace_back(c32 >> 0 & 0xFF);
    dst.emplace_back(c32 >> 8 & 0xFF);
    dst.emplace_back(c32 >> 16 & 0xFF);
    dst.emplace_back(c32 >> 24 & 0xFF);
    appended = 4;
    return true;
}
inline bool char8to16(uint8_t* src, ByteArray& dst, Endian dst_endian, size_t& read, size_t& appended) {
    ByteArray ch32;
    size_t   tmp;
    if(!char8to32(src, ch32, host_endian, read, tmp)) return 0;
    return char32to16(ch32.data(), dst, host_endian, dst_endian, tmp, appended);
}
inline bool char16to8(uint8_t* src, ByteArray& dst, Endian src_endian, size_t& read, size_t& appended) {
    ByteArray ch32;
    size_t   tmp;
    if(!char16to32(src, ch32, src_endian, host_endian, read, tmp)) return 0;
    return char32to8(ch32.data(), dst, host_endian, tmp, appended);
}
} // namespace
bool utf8to16(ByteArray& src, ByteArray& dst, Endian dst_endian){
    for(auto i = src.begin(); i != src.end();){
        size_t read, appended;
        if(!char8to16(&*i, dst, dst_endian, read, appended)) return false;
        i += read;
    }
    return true;
}
bool utf8to32(ByteArray& src, ByteArray& dst, Endian dst_endian){
    for(auto i = src.begin(); i != src.end();) {
        size_t read, appended;
        if(!char8to32(&*i, dst, dst_endian, read, appended)) return false;
        i += read;
    }
    return true;
}
bool utf16to8(ByteArray& src, ByteArray& dst, Endian src_endian){
    for(auto i = src.begin(); i != src.end();) {
        size_t read, appended;
        if(!char16to8(&*i, dst, src_endian, read, appended)) return false;
        i += read;
    }
    return true;
}
bool utf16to32(ByteArray& src, ByteArray& dst, Endian src_endian, Endian dst_endian){
    for(auto i = src.begin(); i != src.end();) {
        size_t read, appended;
        if(!char16to32(&*i, dst, src_endian, dst_endian, read, appended)) return false;
        i += read;
    }
    return true;
}
bool utf32to8(ByteArray& src, ByteArray& dst, Endian src_endian) {
    for(auto i = src.begin(); i != src.end();) {
        size_t read, appended;
        if(!char32to8(&*i, dst, src_endian, read, appended)) return false;
        i += read;
    }
    return true;
}
bool utf32to16(ByteArray& src, ByteArray& dst, Endian src_endian, Endian dst_endian){
    for(auto i = src.begin(); i != src.end();) {
        size_t read, appended;
        if(!char32to16(&*i, dst, src_endian, dst_endian, read, appended)) return false;
        i += read;
    }
    return true;
}