#pragma once
#include <array>
#include <cstring>
#include <fstream>

#include "unicode.hpp"

namespace{
inline uint32_t swap32(const uint32_t value) {
    uint32_t ret;
    ret = value << 24;
    ret |= (value & 0x0000FF00) << 8;
    ret |= (value & 0x00FF0000) >> 8;
    ret |= value >> 24;
    return ret;
}
inline uint32_t unsynch(const uint32_t value) {
    uint32_t ret = ((value & 0b00000000000000000000000001111111) >> 0) |
                   ((value & 0b00000000000000000111111100000000) >> 1) |
                   ((value & 0b00000000011111110000000000000000) >> 2) |
                   ((value & 0b01111111000000000000000000000000) >> 3);
    return ret;
}
}
using ID3Tag = std::pair<std::array<char, 4>, std::vector<uint8_t>>;

bool read_id3(std::ifstream& handle, std::vector<ID3Tag>& result){
    bool success = false;
    do {
        std::streampos id3_begin = handle.tellg();
        std::streamoff id3_limit;

        char           id3_identifier[3];
        handle.read(id3_identifier, 3);
        if(std::strncmp("ID3", id3_identifier, 3) != 0) break;

        char major_version, revision_number;
        handle.read(&major_version, 1);
        handle.read(&revision_number, 1);
        if(major_version != 4) break;

        char flags;
        handle.read(&flags, 1);
        bool unsynchronised      = flags & 0b10000000;
        bool has_extended_header = flags & 0b01000000;
        bool is_experimental     = flags & 0b00100000;
        bool has_footer          = flags & 0b00010000;
        if(flags & 0b00001111) break; // other flags must be cleared
        
        uint32_t limit;
        handle.read(reinterpret_cast<char*>(&limit), 4);
        id3_limit = unsynch(swap32(limit)) + 10; // +10: header size

        if(has_extended_header) {
            uint32_t extended_header_limit;
            handle.read(reinterpret_cast<char*>(&extended_header_limit), 4);
            extended_header_limit = unsynch(swap32(extended_header_limit));
            // extended header is not supported now.
            handle.seekg(extended_header_limit - 4, std::ios::cur);
        }

        while(handle.tellg() < (id3_begin + id3_limit)){
            std::array<char, 4> frame_id;
            uint32_t frame_limit;
            handle.read(reinterpret_cast<char*>(frame_id.data()), 4);
            handle.read(reinterpret_cast<char*>(&frame_limit), 4);
            frame_limit = unsynch(swap32(frame_limit));

            char status_message, format_description;
            handle.read(reinterpret_cast<char*>(&status_message), 1);
            handle.read(reinterpret_cast<char*>(&format_description), 1);

            bool discard_on_alteration = status_message & 0b01000000;
            bool discatd_on_file_alter = status_message & 0b00100000;
            bool readonly              = status_message & 0b00010000;

            bool has_grouping_identity     = format_description & 0b01000000;
            bool compressed                = format_description & 0b00001000;
            bool encrypted                 = format_description & 0b00000100;
            bool unsyrchronised            = format_description & 0b00000010;
            bool has_data_length_indicator = format_description & 0b00000001;

            switch(frame_id[0]) {
            case 'T': {
                enum class TextEncoding : char {
                    Latin1  = 0,
                    UTF16   = 1,
                    UTF16BE = 2,
                    UTF8    = 3,
                    UTF8LE  = 4,
                };
                std::vector<uint8_t> raw(frame_limit);
                handle.read(reinterpret_cast<char*>(raw.data()), frame_limit);

                std::vector<uint8_t> data;
                switch(static_cast<TextEncoding>(raw[0])) {
                case TextEncoding::Latin1:
                case TextEncoding::UTF8:
                case TextEncoding::UTF8LE:
                    // treat Latin1 as UTF-8
                    data = std::vector<uint8_t>(raw.begin() + 1, raw.end());
                    break;
                case TextEncoding::UTF16: {
                    bool big_endian = (raw[1] == 0xFE) && (raw[2] == 0xFF);
                    bool lit_endian = (raw[1] == 0xFF) && (raw[2] == 0xFE);
                    if(!big_endian && !lit_endian) continue;
                    ByteArray text(raw.begin() + 3, raw.end());
                    utf16to8(text, data, big_endian ? Endian::Big : Endian::Little);
                } break;
                case TextEncoding::UTF16BE: {
                    ByteArray text(raw.begin() + 1, raw.end());
                    utf16to8(text, data, Endian::Big);
                } break;
                default:
                    continue;
                }
                result.emplace_back(std::make_pair(frame_id, data));
            } break;
            default:
                handle.seekg(frame_limit, std::ios::cur);
                continue;
            }
           
        }
        success = true;
    } while(0);
    return success;
}