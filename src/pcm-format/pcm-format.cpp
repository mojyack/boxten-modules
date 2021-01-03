#include "pcm-format.hpp"
#include "configuration.hpp"
#include "plugin.hpp"
#include "type.hpp"
#include <unistd.h>

namespace {
#define EACH_FRAME(decl, proc, src_type)                                        \
    size_t           n_samples  = packet.get_frames() * packet.format.channels; \
    constexpr size_t byte_width = sizeof(src_type);                             \
    decl;                                                                       \
    for(size_t i = 0; i < n_samples; i++) {                                     \
        proc;                                                                   \
    }

#define COPY_PACKET(func, src_type, dst_type)                                           \
    EACH_FRAME(                                                                         \
        std::vector<u8> pcm(n_samples * sizeof(dst_type)),                              \
        src_type*       src = reinterpret_cast<src_type*>(&packet.pcm[i * byte_width]); \
        dst_type* dst       = reinterpret_cast<dst_type*>(&pcm[i * sizeof(dst_type)]);  \
        func(src, dst),                                                                 \
        src_type)                                                                       \
    packet.pcm                = pcm;                                                    \
    packet.format.sample_type = boxten::SampleType::dst_type;

#define CONVERT_PACKET(func, src_type, dst_type)                                  \
    EACH_FRAME(                                                                   \
        ,                                                                         \
        dst_type* dst = reinterpret_cast<dst_type*>(&packet.pcm[i * byte_width]); \
        func(dst),                                                                \
        src_type)

struct i24 {
  private:
    u8 bytes[3];

  public:
    operator i32() {
        return static_cast<i32>(bytes[0]) << 8 |
               static_cast<i32>(bytes[1]) << 16 |
               static_cast<i32>(bytes[2] << 24);
    }
    i32 operator=(i32 src) {
        bytes[0] = (src & 0x0000FF00) >> 8;
        bytes[1] = (src & 0x00FF0000) >> 16;
        bytes[2] = (src & 0xFF000000) >> 24;
        return src;
    }
};
using u24 = i24;
using s8  = i8;
using s16 = i16;
using s24 = i24;
using s32 = i32;

using s16_le = s16;
using s16_be = s16;
using u16_le = u16;
using u16_be = u16;
using s24_le = s24;
using s24_be = s24;
using u24_le = u24;
using u24_be = u24;
using s32_le = s32;
using s32_be = s32;
using u32_le = u32;
using u32_be = u32;
using f32_le = f32;
using f32_be = f32;

namespace macro {
inline void swap(i16* src) {
    *src = *src >> 8 | *src << 8;
}
inline void swap(u16* src) {
    swap(reinterpret_cast<i16*>(src));
}
inline void swap(i24* src) {
    u8* d   = reinterpret_cast<u8*>(src);
    u8  buf = d[0];
    d[0]    = d[2];
    d[2]    = buf;
}
inline void swap(i32* src) {
    *src =
        (*src >> 24 & 0x000000FF) |
        (*src >> 8 & 0x0000FF00) |
        (*src << 8 & 0x00FF0000) |
        (*src << 24 & 0xFF000000);
}
inline void swap(u32* src) {
    swap(reinterpret_cast<i32*>(src));
}
inline void swap(f32* src) {
    swap(reinterpret_cast<i32*>(src));
}
inline void sign(i8* src) {
    *src += 0x80;
}
inline void sign(u8* src) {
    sign(reinterpret_cast<i8*>(src));
}
inline void sign(i16* src) {
    reinterpret_cast<u8*>(src)[1] += 0x80;
}
inline void sign(u16* src) {
    sign(reinterpret_cast<i16*>(src));
}
inline void sign(i24* src) {
    reinterpret_cast<u8*>(src)[2] += 0x80;
}
inline void sign(i32* src) {
    reinterpret_cast<u8*>(src)[3] += 0x80;
}
inline void sign(u32* src) {
    sign(reinterpret_cast<i32*>(src));
}
inline void sign(f32* src) {
    sign(reinterpret_cast<i32*>(src));
}

inline void convert_f32le_e(f32_be* dst) {
    swap(dst);
}
inline void copy(f32_le* src, s8* dst) {
    *dst = *src * (0x80 - 1);
}
inline void copy_s(f32_le* src, u8* dst) {
    copy(src, reinterpret_cast<s8*>(dst));
    sign(dst);
}
inline void copy(f32_le* src, s16_le* dst) {
    *dst = *src * (0x8000 - 1);
}
inline void copy_e(f32_le* src, s16_be* dst) {
    copy(src, dst);
    swap(dst);
}
inline void copy_s(f32_le* src, u16_le* dst) {
    copy(src, reinterpret_cast<s16*>(dst));
    sign(dst);
}
inline void copy_se(f32_le* src, u16_le* dst) {
    copy(src, reinterpret_cast<s16*>(dst));
    sign(dst);
    swap(dst);
}
inline void copy(f32_le* src, s24_le* dst) {
    *dst = *src * static_cast<f32>(0x80000000 - 1);
}
inline void copy_e(f32_le* src, s24_be* dst) {
    copy(src, dst);
    swap(dst);
}
inline void copy_s(f32_le* src, u24_le* dst) {
    copy(src, reinterpret_cast<s24*>(dst));
    sign(dst);
}
inline void copy_se(f32_le* src, u24_be* dst) {
    copy(src, reinterpret_cast<s24*>(dst));
    sign(dst);
    swap(dst);
}
inline void convert_f32le(s32_le* dst) {
    *dst = *reinterpret_cast<f32*>(dst) * static_cast<f32>(0x80000000 - 1);
}
inline void convert_f32le_e(s32_be* dst) {
    convert_f32le(dst);
    swap(dst);
}
inline void convert_f32le_s(u32_le* dst) {
    convert_f32le(reinterpret_cast<s32*>(dst));
    sign(dst);
}
inline void convert_f32le_se(u32_be* dst) {
    convert_f32le_s(dst);
    swap(dst);
}

inline void convert_f32be(f32_le* dst) {
    swap(dst);
}
inline void e_copy(f32_be* src, s8* dst) {
    swap(src);
    copy(src, dst);
}
inline void e_copy_s(f32_be* src, u8* dst) {
    swap(src);
    copy_s(src, dst);
}
inline void e_copy(f32_be* src, s16_le* dst) {
    swap(src);
    copy(src, dst);
}
inline void e_copy_e(f32_be* src, s16_be* dst) {
    swap(src);
    copy_e(src, dst);
}
inline void e_copy_s(f32_be* src, u16_le* dst) {
    swap(src);
    copy_s(src, dst);
}
inline void e_copy_se(f32_be* src, u16_be* dst) {
    swap(src);
    copy_se(src, dst);
}
inline void e_copy(f32_be* src, s24_le* dst) {
    swap(src);
    copy(src, dst);
}
inline void e_copy_e(f32_be* src, s24_be* dst) {
    swap(src);
    copy_e(src, dst);
}
inline void e_copy_s(f32_be* src, u24_le* dst) {
    swap(src);
    copy_s(src, dst);
}
inline void e_copy_se(f32_be* src, u24_be* dst) {
    swap(src);
    copy_se(src, dst);
}
inline void convert_f32be(s32_le* dst) {
    swap(dst);
    convert_f32le(dst);
}
inline void convert_f32be_e(s32_be* dst) {
    swap(dst);
    convert_f32le_e(dst);
}
inline void convert_f32be_s(u32_le* dst) {
    swap(dst);
    convert_f32le_s(dst);
}
inline void convert_f32be_se(u32_be* dst) {
    swap(dst);
    convert_f32le_se(dst);
}

inline void copy(s8* src, f32_le* dst) {
    *dst = static_cast<f32>(*src) / 0x80;
}
inline void copy_e(s8* src, f32_be* dst) {
    copy(src, reinterpret_cast<f32_le*>(dst));
    swap(dst);
}
inline void convert_s8_s(u8* dst) {
    sign(dst);
}
inline void copy(s8* src, s16* dst) {
    *dst = static_cast<s16>(*src) * 0x102;
}
inline void copy_e(s8* src, s16* dst) {
    copy(src, dst);
    swap(dst);
}
inline void copy_s(s8* src, u16* dst) {
    copy(src, reinterpret_cast<s16*>(dst));
    sign(dst);
}
inline void copy_se(s8* src, u16* dst) {
    copy_s(src, dst);
    swap(dst);
}
inline void copy(s8* src, s32* dst) {
    *dst = static_cast<s32>(*src) * 0x1020408;
}
inline void copy_e(s8* src, s32* dst) {
    copy(src, dst);
    swap(dst);
}
inline void copy_s(s8* src, u32* dst) {
    copy(src, reinterpret_cast<s32*>(dst));
    sign(dst);
}
inline void copy_se(s8* src, u32* dst) {
    copy_s(src, dst);
    swap(dst);
}
inline void copy(s8* src, s24* dst) {
    *dst = (static_cast<s32>(*src) * 0x10204) << 8;
}
inline void copy_e(s8* src, s24* dst) {
    copy(src, dst);
    swap(dst);
}
inline void copy_s(s8* src, u24* dst) {
    copy(src, reinterpret_cast<s24*>(dst));
    sign(dst);
}
inline void copy_se(s8* src, u24* dst) {
    copy_s(src, dst);
    swap(dst);
}

inline void s_copy(u8* src, f32_le* dst) {
    sign(src);
    copy(reinterpret_cast<s8*>(src), dst);
}
inline void s_copy_e(u8* src, f32_be* dst) {
    s_copy(src, dst);
    swap(dst);
}
inline void convert_u8(s8* dst) {
    sign(dst);
}
inline void s_copy(u8* src, s16* dst) {
    sign(src);
    copy(reinterpret_cast<s8*>(src), dst);
}
inline void s_copy_e(u8* src, s16* dst) {
    s_copy(src, dst);
    swap(dst);
}
inline void s_copy_s(u8* src, u16* dst) {
    s_copy(src, reinterpret_cast<s16*>(dst));
    sign(dst);
}
inline void s_copy_se(u8* src, u16* dst) {
    s_copy_s(src, dst);
    swap(dst);
}
inline void s_copy(u8* src, s32* dst) {
    sign(src);
    copy(reinterpret_cast<s8*>(src), dst);
}
inline void s_copy_e(u8* src, s32* dst) {
    s_copy(src, dst);
    swap(dst);
}
inline void s_copy_s(u8* src, u32* dst) {
    s_copy(src, reinterpret_cast<s32*>(dst));
    sign(dst);
}
inline void s_copy_se(u8* src, u32* dst) {
    s_copy_s(src, dst);
    swap(dst);
}
inline void s_copy(u8* src, s24* dst) {
    sign(src);
    copy(reinterpret_cast<s8*>(src), dst);
}
inline void s_copy_e(u8* src, s24* dst) {
    s_copy(src, dst);
    swap(dst);
}
inline void s_copy_s(u8* src, u24* dst) {
    s_copy(src, reinterpret_cast<s24*>(dst));
    sign(dst);
}
inline void s_copy_se(u8* src, u24* dst) {
    s_copy_s(src, dst);
    swap(dst);
}

inline void copy(s16_le* src, f32_le* dst) {
    *dst = static_cast<f32>(*src) / 0x8000;
}
inline void copy_e(s16_le* src, f32_be* dst) {
    copy(src, dst);
    swap(dst);
}
inline void copy(s16_le* src, s8* dst) {
    *dst = *src >> 8;
}
inline void copy_s(s16_le* src, u8* dst) {
    copy(src, reinterpret_cast<s8*>(dst));
    sign(dst);
}
inline void convert_s16le_e(s16_be* dst) {
    swap(dst);
}
inline void convert_s16le_s(u16_le* dst) {
    sign(dst);
}
inline void convert_s16le_se(u16_be* dst) {
    sign(dst);
    swap(dst);
}
inline void copy(s16_le* src, s24_le* dst) {
    *dst = (*src * 0x100) << 8;
}
inline void copy_e(s16_le* src, s24_be* dst) {
    copy(src, reinterpret_cast<s24_le*>(dst));
    swap(dst);
}
inline void copy_s(s16_le* src, u24_le* dst) {
    copy(src, reinterpret_cast<s24_le*>(dst));
    sign(dst);
}
inline void copy_se(s16_le* src, u24_be* dst) {
    copy_s(src, dst);
    swap(dst);
}
inline void copy(s16_le* src, s32_le* dst) {
    *dst = *src * 0x10002;
}
inline void copy_e(s16_le* src, s32_be* dst) {
    copy(src, dst);
    swap(dst);
}
inline void copy_s(s16_le* src, u32_le* dst) {
    copy(src, reinterpret_cast<s32_le*>(dst));
    sign(dst);
}
inline void copy_se(s16_le* src, u32_be* dst) {
    copy_s(src, dst);
    swap(dst);
}

inline void e_copy(s16_be* src, f32_le* dst) {
    swap(src);
    copy(reinterpret_cast<s16_le*>(src), dst);
}
inline void e_copy_e(s16_be* src, f32_be* dst) {
    e_copy(src, reinterpret_cast<f32_le*>(dst));
    swap(dst);
}
inline void e_copy(s16_be* src, s8* dst) {
    swap(src);
    *dst = *src;
}
inline void e_copy_s(s16_be* src, u8* dst) {
    e_copy(src, reinterpret_cast<s8*>(dst));
    sign(dst);
}
inline void convert_s16be(s16_le* dst) {
    swap(dst);
}
inline void convert_s16be_s(u16_le* dst) {
    swap(dst);
    sign(dst);
}
inline void convert_s16be_se(u16_be* dst) {
    swap(dst);
    sign(dst);
    swap(dst);
}
inline void e_copy(s16_be* src, s24_le* dst) {
    swap(src);
    copy(reinterpret_cast<s16_le*>(src), dst);
}
inline void e_copy_e(s16_be* src, s24_be* dst) {
    e_copy(src, dst);
    swap(dst);
}
inline void e_copy_s(s16_be* src, u24_le* dst) {
    e_copy(src, reinterpret_cast<s24_le*>(dst));
    sign(dst);
}
inline void e_copy_se(s16_be* src, u24_be* dst) {
    e_copy_s(src, dst);
    swap(dst);
}
inline void e_copy(s16_be* src, s32_le* dst) {
    swap(src);
    copy(reinterpret_cast<s16_le*>(src), dst);
}
inline void e_copy_e(s16_be* src, s32_be* dst) {
    e_copy(src, dst);
    swap(dst);
}
inline void e_copy_s(s16_be* src, u32_le* dst) {
    e_copy(src, reinterpret_cast<s32_le*>(dst));
    sign(dst);
}
inline void e_copy_se(s16_be* src, u32_be* dst) {
    e_copy_s(src, dst);
    swap(dst);
}

inline void s_copy(u16_le* src, f32_le* dst) {
    sign(src);
    copy(reinterpret_cast<s16_le*>(src), dst);
}
inline void s_copy_e(u16_le* src, f32_be* dst) {
    s_copy(src, dst);
    swap(dst);
}
inline void s_copy(u16_le* src, s8* dst) {
    sign(src);
    copy(reinterpret_cast<s16_le*>(src), dst);
}
inline void s_copy_s(u16_le* src, u8* dst) {
    s_copy(src, reinterpret_cast<s8*>(dst));
    sign(dst);
}
inline void s_convert_u16le(s16_le* dst) {
    sign(dst);
}
inline void s_convert_u16le_e(s16_be* dst) {
    sign(dst);
    swap(dst);
}
inline void s_convert_u16le_se(u16_be* dst) {
    swap(dst);
}
inline void s_copy(u16_le* src, s24_be* dst) {
    sign(src);
    copy(reinterpret_cast<s16_le*>(src), dst);
}
inline void s_copy_e(u16_le* src, s24_be* dst) {
    s_copy(src, dst);
    swap(dst);
}
inline void s_copy_s(u16_le* src, u24_le* dst) {
    s_copy(src, reinterpret_cast<s24_le*>(dst));
    sign(dst);
}
inline void s_copy_se(u16_le* src, u24_be* dst) {
    s_copy_s(src, dst);
    swap(dst);
}
inline void s_copy(u16_le* src, s32_be* dst) {
    sign(src);
    copy(reinterpret_cast<s16_le*>(src), dst);
}
inline void s_copy_e(u16_le* src, s32_be* dst) {
    s_copy(src, dst);
    swap(dst);
}
inline void s_copy_s(u16_le* src, u32_le* dst) {
    s_copy(src, reinterpret_cast<s32_le*>(dst));
    sign(dst);
}
inline void s_copy_se(u16_le* src, u32_be* dst) {
    s_copy_s(src, dst);
    swap(dst);
}

inline void se_copy(u16_be* src, f32_le* dst) {
    swap(src);
    s_copy(reinterpret_cast<u16_le*>(src), dst);
}
inline void se_copy_e(u16_be* src, f32_be* dst) {
    se_copy(src, dst);
    swap(dst);
}
inline void se_copy(u16_be* src, s8* dst) {
    swap(src);
    s_copy(reinterpret_cast<u16_le*>(src), dst);
}
inline void se_copy_s(u16_be* src, u8* dst) {
    se_copy(src, reinterpret_cast<s8*>(dst));
    sign(dst);
}
inline void se_convert_u16be(s16_le* dst) {
    swap(dst);
    sign(dst);
}
inline void se_convert_u16be_e(s16_be* dst) {
    swap(dst);
    sign(dst);
    swap(dst);
}
inline void se_convert_u16be_s(u16_le* dst) {
    swap(dst);
}
inline void se_copy(u16_be* src, s24_be* dst) {
    sign(src);
    copy(reinterpret_cast<s16_le*>(src), dst);
}
inline void se_copy_e(u16_be* src, s24_be* dst) {
    s_copy(src, dst);
    swap(dst);
}
inline void se_copy_s(u16_be* src, u24_le* dst) {
    s_copy(src, reinterpret_cast<s24_le*>(dst));
    sign(dst);
}
inline void se_copy_se(u16_be* src, u24_be* dst) {
    s_copy_s(src, dst);
    swap(dst);
}
inline void se_copy(u16_be* src, s32_be* dst) {
    sign(src);
    copy(reinterpret_cast<s16_le*>(src), dst);
}
inline void se_copy_e(u16_be* src, s32_be* dst) {
    s_copy(src, dst);
    swap(dst);
}
inline void se_copy_s(u16_be* src, u32_le* dst) {
    s_copy(src, reinterpret_cast<s32_le*>(dst));
    sign(dst);
}
inline void se_copy_se(u16_be* src, u32_be* dst) {
    s_copy_s(src, dst);
    swap(dst);
}

inline void copy(s24_le* src, f32_le* dst) {
    *dst = static_cast<f32_le>(*src) / 0x80000000;
}
inline void copy_e(s24_le* src, f32_be* dst) {
    copy(src, reinterpret_cast<f32_le*>(dst));
    swap(dst);
}
inline void copy(s24_le* src, s8* dst) {
    *dst = static_cast<s32>(*src) >> 24;
}
inline void copy_s(s24_le* src, u8* dst) {
    copy(src, reinterpret_cast<s8*>(dst));
    sign(dst);
}
inline void copy(s24_le* src, s16_le* dst) {
    *dst = static_cast<s32>(*src) >> 16;
}
inline void copy_e(s24_le* src, s16_be* dst) {
    copy(src, reinterpret_cast<s16_le*>(dst));
    swap(dst);
}
inline void copy_s(s24_le* src, u16_le* dst) {
    copy(src, reinterpret_cast<s16_le*>(dst));
    sign(dst);
}
inline void copy_se(s24_le* src, u16_be* dst) {
    copy_s(src, reinterpret_cast<u16_le*>(dst));
    swap(dst);
}
inline void convert_s24le_e(s24_be* dst) {
    swap(dst);
}
inline void convert_s24le_s(u24_le* dst) {
    sign(dst);
}
inline void convert_s24le_se(u24_be* dst) {
    sign(dst);
    swap(dst);
}
inline void copy(s24_le* src, s32_le* dst) {
    *dst = static_cast<i32>(*src) * 0x100;
}
inline void copy_e(s24_le* src, s32_be* dst) {
    copy(src, reinterpret_cast<s32_le*>(dst));
    swap(dst);
}
inline void copy_s(s24_le* src, u32_le* dst) {
    copy(src, reinterpret_cast<s32_le*>(dst));
    sign(dst);
}
inline void copy_se(s24_le* src, u32_be* dst) {
    copy_s(src, reinterpret_cast<u32_le*>(dst));
    swap(dst);
}

inline void e_copy(s24_be* src, f32_le* dst) {
    swap(src);
    copy(reinterpret_cast<s24_le*>(src), dst);
}
inline void e_copy_e(s24_be* src, f32_be* dst) {
    e_copy(src, reinterpret_cast<f32_le*>(dst));
    swap(dst);
}
inline void e_copy(s24_be* src, s8* dst) {
    swap(src);
    copy(reinterpret_cast<s24_le*>(src), dst);
}
inline void e_copy_s(s24_be* src, u8* dst) {
    e_copy(src, reinterpret_cast<s8*>(dst));
    sign(dst);
}
inline void e_copy(s24_be* src, s16_le* dst) {
    swap(src);
    copy(reinterpret_cast<s24_le*>(src), dst);
}
inline void e_copy_e(s24_be* src, s16_be* dst) {
    e_copy(src, reinterpret_cast<s16_le*>(dst));
    swap(dst);
}
inline void e_copy_s(s24_be* src, u16_le* dst) {
    e_copy(src, reinterpret_cast<s16_le*>(dst));
    sign(dst);
}
inline void e_copy_se(s24_be* src, u16_be* dst) {
    e_copy_s(src, reinterpret_cast<u16_le*>(dst));
    swap(dst);
}
inline void e_convert_s24be(s24_le* dst) {
    swap(dst);
}
inline void e_convert_s24be_s(u24_le* dst) {
    swap(dst);
    sign(dst);
}
inline void e_convert_s24be_se(u24_be* dst) {
    swap(dst);
    sign(dst);
    swap(dst);
}
inline void e_copy(s24_be* src, s32_le* dst) {
    swap(src);
    copy(reinterpret_cast<s24_le*>(src), dst);
}
inline void e_copy_e(s24_be* src, s32_be* dst) {
    e_copy(src, reinterpret_cast<s32_le*>(dst));
    swap(src);
}
inline void e_copy_s(s24_be* src, u32_le* dst) {
    e_copy(src, reinterpret_cast<s32_le*>(dst));
    sign(src);
}
inline void e_copy_se(s24_be* src, u32_be* dst) {
    e_copy_s(src, reinterpret_cast<u32_le*>(dst));
    swap(dst);
}

inline void s_copy(u24_le* src, f32_le* dst) {
    sign(src);
    copy(reinterpret_cast<s24_le*>(src), dst);
}
inline void s_copy_e(u24_le* src, f32_be* dst) {
    s_copy(src, reinterpret_cast<f32_le*>(dst));
    swap(dst);
}
inline void s_copy(u24_le* src, s8* dst) {
    sign(src);
    copy(reinterpret_cast<s24_le*>(src), dst);
}
inline void s_copy_s(u24_le* src, u8* dst) {
    s_copy(src, reinterpret_cast<s8*>(dst));
    sign(dst);
}
inline void s_copy(u24_le* src, s16_le* dst) {
    sign(src);
    copy(reinterpret_cast<s24_le*>(src), dst);
}
inline void s_copy_e(u24_le* src, s16_be* dst) {
    s_copy(src, reinterpret_cast<s16_le*>(dst));
    swap(dst);
}
inline void s_copy_s(u24_le* src, u16_le* dst) {
    s_copy(src, reinterpret_cast<s16_le*>(dst));
    sign(dst);
}
inline void s_copy_se(u24_le* src, u16_be* dst) {
    s_copy_s(src, reinterpret_cast<u16_le*>(dst));
    swap(dst);
}
inline void s_convert_u24le(s24_le* dst) {
    sign(dst);
}
inline void s_convert_u24le_e(s24_be* dst) {
    sign(dst);
    swap(dst);
}
inline void s_convert_u24le_se(u24_be* dst) {
    swap(dst);
}
inline void s_copy(u24_le* src, s32_le* dst) {
    sign(src);
    copy(reinterpret_cast<s24_le*>(src), dst);
}
inline void s_copy_e(u24_le* src, s32_be* dst) {
    s_copy(src, reinterpret_cast<s32_le*>(dst));
    swap(dst);
}
inline void s_copy_s(u24_le* src, u32_le* dst) {
    s_copy(src, reinterpret_cast<s32_le*>(dst));
    sign(dst);
}
inline void s_copy_se(u24_le* src, u32_be* dst) {
    s_copy_s(src, reinterpret_cast<u32_le*>(dst));
    swap(dst);
}

inline void se_copy(u24_be* src, f32_le* dst) {
    swap(src);
    s_copy(reinterpret_cast<u24_le*>(src), dst);
}
inline void se_copy_e(u24_be* src, f32_be* dst) {
    se_copy(src, reinterpret_cast<f32_le*>(dst));
    swap(dst);
}
inline void se_copy(u24_be* src, s8* dst) {
    swap(src);
    s_copy(reinterpret_cast<u24_le*>(src), dst);
}
inline void se_copy_s(u24_be* src, u8* dst) {
    se_copy(src, reinterpret_cast<s8*>(dst));
    sign(dst);
}
inline void se_copy(u24_be* src, s16_le* dst) {
    swap(dst);
    s_copy(reinterpret_cast<u24_le*>(src), dst);
}
inline void se_copy_e(u24_be* src, s16_be* dst) {
    se_copy(src, reinterpret_cast<s16_le*>(dst));
    swap(dst);
}
inline void se_copy_s(u24_be* src, u16_le* dst) {
    se_copy(src, reinterpret_cast<s16_le*>(dst));
    sign(dst);
}
inline void se_copy_se(u24_be* src, u16_be* dst) {
    se_copy_s(src, reinterpret_cast<u16_le*>(dst));
    swap(dst);
}
inline void se_convert_u24be(s24_le* dst) {
    swap(dst);
    sign(dst);
}
inline void se_convert_u24be_e(s24_be* dst) {
    swap(dst);
    sign(dst);
    swap(dst);
}
inline void se_convert_u24be_s(u24_le* dst) {
    swap(dst);
}
inline void se_copy(u24_be* src, s32_le* dst) {
    swap(src);
    s_copy(reinterpret_cast<u24_le*>(src), dst);
}
inline void se_copy_e(u24_be* src, s32_be* dst) {
    se_copy(src, reinterpret_cast<s32_le*>(dst));
    swap(dst);
}
inline void se_copy_s(u24_be* src, u32_le* dst) {
    se_copy(src, reinterpret_cast<s32_le*>(dst));
    sign(dst);
}
inline void se_copy_se(u24_be* src, u32_be* dst) {
    se_copy_s(src, reinterpret_cast<u32_le*>(dst));
    swap(dst);
}

inline void convert_s32le(f32_le* dst) {
    *dst = static_cast<f32>(*reinterpret_cast<s32_le*>(dst)) / 0x80000000;
}
inline void convert_s32le_e(f32_be* dst) {
    convert_s32le(reinterpret_cast<f32_le*>(dst));
    swap(dst);
}
inline void copy(s32_le* src, s8* dst) {
    *dst = *src >> 24;
}
inline void copy_s(s32_le* src, u8* dst) {
    copy(src, reinterpret_cast<s8*>(dst));
    sign(dst);
}
inline void copy(s32_le* src, s16_le* dst) {
    *dst = *src >> 16;
}
inline void copy_e(s32_le* src, s16_be* dst) {
    copy(src, reinterpret_cast<s16_le*>(dst));
    swap(dst);
}
inline void copy_s(s32_le* src, u16_le* dst) {
    copy(src, reinterpret_cast<s16_le*>(dst));
    sign(dst);
}
inline void copy_se(s32_le* src, u16_be* dst) {
    copy_s(src, reinterpret_cast<u16_le*>(dst));
    swap(dst);
}
inline void copy(s32_le* src, s24_le* dst) {
    *dst = *src;
}
inline void copy_e(s32_le* src, s24_be* dst) {
    copy(src, reinterpret_cast<s24_le*>(dst));
    swap(dst);
}
inline void copy_s(s32_le* src, u24_le* dst) {
    copy(src, reinterpret_cast<s16_le*>(dst));
    sign(dst);
}
inline void copy_se(s32_le* src, u24_be* dst) {
    copy_s(src, reinterpret_cast<u24_le*>(dst));
    swap(dst);
}
inline void convert_s32le_e(s32_be* dst) {
    swap(dst);
}
inline void convert_s32le_s(u32_le* dst) {
    sign(dst);
}
inline void convert_s32le_se(u32_be* dst) {
    sign(dst);
    swap(dst);
}

inline void e_convert_s32be(f32_le* dst) {
    swap(dst);
    convert_s32le(dst);
}
inline void e_convert_s32be_e(f32_be* dst) {
    e_convert_s32be(reinterpret_cast<f32_le*>(dst));
    swap(dst);
}
inline void e_copy(s32_be* src, s8* dst) {
    swap(src);
    copy(reinterpret_cast<s32_le*>(src), dst);
}
inline void e_copy_s(s32_be* src, u8* dst) {
    e_copy(src, reinterpret_cast<s8*>(dst));
    sign(dst);
}
inline void e_copy(s32_be* src, s16_le* dst) {
    swap(src);
    copy(reinterpret_cast<s32_le*>(src), dst);
}
inline void e_copy_e(s32_be* src, s16_be* dst) {
    e_copy(src, reinterpret_cast<s16_le*>(dst));
    swap(dst);
}
inline void e_copy_s(s32_be* src, u16_le* dst) {
    e_copy(src, reinterpret_cast<s16_le*>(dst));
    sign(dst);
}
inline void e_copy_se(s32_be* src, u16_be* dst) {
    e_copy_s(src, reinterpret_cast<u16_le*>(dst));
    swap(dst);
}
inline void e_copy(s32_be* src, s24_le* dst) {
    swap(src);
    copy(reinterpret_cast<s32_le*>(src), dst);
}
inline void e_copy_e(s32_be* src, s24_be* dst) {
    e_copy(src, reinterpret_cast<s24_le*>(dst));
    swap(dst);
}
inline void e_copy_s(s32_be* src, u24_le* dst) {
    e_copy(src, reinterpret_cast<s24_le*>(dst));
    sign(dst);
}
inline void e_copy_se(s32_be* src, u24_be* dst) {
    e_copy_s(src, reinterpret_cast<u24_le*>(dst));
    swap(dst);
}
inline void e_convert_s32be(s32_le* dst) {
    swap(dst);
}
inline void e_convert_s32be_s(u32_le* dst) {
    swap(dst);
    sign(dst);
}
inline void e_convert_s32be_se(u32_be* dst) {
    swap(dst);
    sign(dst);
    swap(dst);
}

inline void s_convert_u32le(f32_le* dst) {
    sign(reinterpret_cast<u32_le*>(dst));
    convert_s32le(dst);
}
inline void s_convert_u32le_e(f32_be* dst) {
    s_convert_u32le(dst);
    swap(dst);
}
inline void s_copy(u32_le* src, s8* dst) {
    sign(dst);
    copy(reinterpret_cast<s32_le*>(src), dst);
}
inline void s_copy_s(u32_le* src, u8* dst) {
    s_copy(src, reinterpret_cast<s8*>(dst));
    sign(dst);
}
inline void s_copy(u32_le* src, s16_le* dst) {
    sign(src);
    copy(reinterpret_cast<s32_le*>(src), dst);
}
inline void s_copy_e(u32_le* src, s16_be* dst) {
    s_copy(src, reinterpret_cast<s16_le*>(dst));
    swap(dst);
}
inline void s_copy_s(u32_le* src, u16_le* dst) {
    s_copy(src, reinterpret_cast<s16_le*>(dst));
    sign(dst);
}
inline void s_copy_se(u32_le* src, u16_be* dst) {
    s_copy_s(src, reinterpret_cast<u16_le*>(dst));
    swap(dst);
}
inline void s_copy(u32_le* src, s24_le* dst) {
    sign(dst);
    copy(reinterpret_cast<s32_le*>(src), dst);
}
inline void s_copy_e(u32_le* src, s24_be* dst) {
    s_copy(src, reinterpret_cast<s24_le*>(dst));
    swap(dst);
}
inline void s_copy_s(u32_le* src, u24_le* dst) {
    s_copy(src, reinterpret_cast<s24_le*>(dst));
    sign(dst);
}
inline void s_copy_se(u32_le* src, u24_be* dst) {
    s_copy_s(src, reinterpret_cast<u24_le*>(dst));
    swap(dst);
}
inline void s_convert_u32le(s32_le* dst) {
    sign(dst);
}
inline void s_convert_u32le_e(s32_be* dst) {
    sign(dst);
    swap(dst);
}
inline void s_convert_u32le_se(u32_be* dst) {
    swap(dst);
    sign(dst);
    swap(dst);
}

inline void se_convert_u32be(f32_le* dst) {
    swap(dst);
    s_convert_u32le(dst);
}
inline void se_convert_u32be_e(f32_be* dst) {
    se_convert_u32be(dst);
    swap(dst);
}
inline void se_copy(u32_be* src, s8* dst) {
    swap(src);
    s_copy(reinterpret_cast<u32_le*>(src), dst);
}
inline void se_copy_s(u32_be* src, u8* dst) {
    se_copy(src, reinterpret_cast<s8*>(dst));
    sign(dst);
}
inline void se_copy(u32_be* src, s16_le* dst) {
    swap(src);
    s_copy(reinterpret_cast<u32_le*>(src), dst);
}
inline void se_copy_e(u32_be* src, s16_be* dst) {
    se_copy(src, reinterpret_cast<s16_le*>(dst));
    swap(dst);
}
inline void se_copy_s(u32_be* src, u16_le* dst) {
    se_copy(src, reinterpret_cast<s16_le*>(dst));
    sign(dst);
}
inline void se_copy_se(u32_be* src, u16_be* dst) {
    se_copy_s(src, reinterpret_cast<u16_le*>(dst));
    swap(dst);
}
inline void se_copy(u32_be* src, s24_le* dst) {
    swap(src);
    s_copy(reinterpret_cast<u32_le*>(src), dst);
}
inline void se_copy_e(u32_be* src, s24_be* dst) {
    se_copy(src, reinterpret_cast<s24_le*>(dst));
    swap(dst);
}
inline void se_copy_s(u32_be* src, u24_le* dst) {
    se_copy(src, reinterpret_cast<s24_le*>(dst));
    sign(dst);
}
inline void se_copy_se(u32_be* src, u24_be* dst) {
    se_copy_s(src, reinterpret_cast<u24_le*>(dst));
    swap(dst);
}
inline void se_convert_u32be(s32_le* dst) {
    swap(dst);
    sign(dst);
}
inline void se_convert_u32be_e(s32_be* dst) {
    swap(dst);
    sign(dst);
    swap(dst);
}
inline void se_convert_u32be_s(u32_le* dst) {
    swap(dst);
}
} // namespace macro
void f32le_to_f32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_f32le_e, f32_le, f32_be)
}
void f32le_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, f32_le, s8)
}
void f32le_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, f32_le, u8)
}
void f32le_to_s16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, f32_le, s16_le)
}
void f32le_to_s16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, f32_le, s16_be)
}
void f32le_to_u16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, f32_le, u16_le)
}
void f32le_to_u16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_se, f32_le, u16_be)
}
void f32le_to_s24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, f32_le, s24_le)
}
void f32le_to_s24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, f32_le, s24_be)
}
void f32le_to_u24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, f32_le, u24_le)
}
void f32le_to_u24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_se, f32_le, u24_be)
}
void f32le_to_s32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_f32le, f32_le, s32_le)
}
void f32le_to_s32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_f32le_e, f32_le, s32_be)
}
void f32le_to_u32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_f32le_s, f32_le, u32_le)
}
void f32le_to_u32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_f32le_se, f32_le, u32_be)
}

void f32be_to_f32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_f32be, f32_be, f32_le)
}
void f32be_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, f32_be, s8)
}
void f32be_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_s, f32_be, u8)
}
void f32be_to_s16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, f32_be, s16_le)
}
void f32be_to_s16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_e, f32_be, s16_be)
}
void f32be_to_u16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_s, f32_be, u16_le)
}
void f32be_to_u16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_se, f32_be, u16_be)
}
void f32be_to_s24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, f32_be, s24_le)
}
void f32be_to_s24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_e, f32_be, s24_be)
}
void f32be_to_u24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_s, f32_be, u24_le)
}
void f32be_to_u24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_se, f32_be, u24_be)
}
void f32be_to_s32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_f32be, f32_be, s32_le)
}
void f32be_to_s32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_f32be_e, f32_be, s32_be)
}
void f32be_to_u32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_f32be_s, f32_be, u32_le)
}
void f32be_to_u32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_f32be_se, f32_be, u32_be)
}

void s8_to_f32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s8, f32_le)
}
void s8_to_f32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, s8, f32_be)
}
void s8_to_u8(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s8_s, s8, u8)
}
void s8_to_s16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s8, s16_le)
}
void s8_to_s16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, s8, s16_be)
}
void s8_to_u16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, s8, u16_le)
}
void s8_to_u16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_se, s8, u16_be)
}
void s8_to_s24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s8, s24_le)
}
void s8_to_s24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, s8, s24_be)
}
void s8_to_u24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, s8, u24_le)
}
void s8_to_u24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_se, s8, u24_be)
}
void s8_to_s32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s8, s32_le)
}
void s8_to_s32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, s8, s32_be)
}
void s8_to_u32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, s8, u32_le)
}
void s8_to_u32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_se, s8, u32_be)
}

void u8_to_f32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u8, f32_le)
}
void u8_to_f32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_e, u8, f32_be)
}
void u8_to_s8(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_u8, u8, s8)
}
void u8_to_s16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u8, s16_le)
}
void u8_to_s16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_e, u8, s16_be)
}
void u8_to_u16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_s, u8, u16_le)
}
void u8_to_u16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_se, u8, u16_be)
}
void u8_to_s24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u8, s24_le)
}
void u8_to_s24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_e, u8, s24_be)
}
void u8_to_u24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_s, u8, u24_le)
}
void u8_to_u24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_se, u8, u24_be)
}
void u8_to_s32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u8, s32_le)
}
void u8_to_s32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_e, u8, s32_be)
}
void u8_to_u32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_s, u8, u32_le)
}
void u8_to_u32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_se, u8, u32_be)
}

void s16le_to_f32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s16_le, f32_le);
}
void s16le_to_f32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, s16_le, f32_be);
}
void s16le_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s16_le, s8);
}
void s16le_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, s16_le, u8);
}
void s16le_to_s16be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s16le_e, s16_le, s16_be);
}
void s16le_to_u16le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s16le_s, s16_le, u16_le);
}
void s16le_to_u16be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s16le_se, s16_le, u16_le);
}
void s16le_to_s24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s16_le, s24_le);
}
void s16le_to_s24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, s16_le, s24_be);
}
void s16le_to_u24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, s16_le, u24_le);
}
void s16le_to_u24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_se, s16_le, u24_be);
}
void s16le_to_s32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s16_le, s32_le);
}
void s16le_to_s32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, s16_le, s32_be);
}
void s16le_to_u32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, s16_le, u32_le);
}
void s16le_to_u32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_se, s16_le, u32_be);
}

void s16be_to_f32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, s16_be, f32_le);
}
void s16be_to_f32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_e, s16_be, f32_be);
}
void s16be_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, s16_be, s8);
}
void s16be_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_s, s16_be, u8);
}
void s16be_to_s16le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s16be, s16_be, s16_le);
}
void s16be_to_u16le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s16be_s, s16_be, u16_le);
}
void s16be_to_u16be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s16be_se, s16_be, u16_be);
}
void s16be_to_s24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, s16_be, s24_le);
}
void s16be_to_s24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_e, s16_be, s24_be);
}
void s16be_to_u24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_s, s16_be, u24_le);
}
void s16be_to_u24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_se, s16_be, u24_be);
}
void s16be_to_s32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, s16_be, s32_le);
}
void s16be_to_s32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_e, s16_be, s32_be);
}
void s16be_to_u32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_s, s16_be, u32_le);
}
void s16be_to_u32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_se, s16_be, u32_be);
}

void u16le_to_f32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u16_le, f32_le);
}
void u16le_to_f32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_e, u16_le, f32_be);
}
void u16le_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u16_le, s8);
}
void u16le_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_s, u16_le, u8);
}
void u16le_to_s16le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::s_convert_u16le, u16_le, s16_le);
}
void u16le_to_s16be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::s_convert_u16le_e, u16_le, s16_be);
}
void u16le_to_u16be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::s_convert_u16le_se, u16_le, u16_be);
}
void u16le_to_s24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u16_le, s24_le);
}
void u16le_to_s24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_e, u16_le, s24_be);
}
void u16le_to_u24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_s, u16_le, u24_le);
}
void u16le_to_u24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_se, u16_le, u24_be);
}
void u16le_to_s32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u16_le, s32_le);
}
void u16le_to_s32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_e, u16_le, s32_be);
}
void u16le_to_u32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_s, u16_le, u32_le);
}
void u16le_to_u32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_se, u16_le, u32_be);
}

void u16be_to_f32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy, u16_be, f32_le);
}
void u16be_to_f32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_e, u16_be, f32_be);
}
void u16be_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy, u16_be, s8);
}
void u16be_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_s, u16_be, u8);
}
void u16be_to_s16le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::se_convert_u16be, u16_be, s16_le);
}
void u16be_to_s16be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::se_convert_u16be_e, u16_be, s16_be);
}
void u16be_to_u16le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::se_convert_u16be_s, u16_be, u16_le);
}
void u16be_to_s24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy, u16_be, s24_le);
}
void u16be_to_s24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_e, u16_be, s24_be);
}
void u16be_to_u24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_s, u16_be, u24_le);
}
void u16be_to_u24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_se, u16_be, u24_be);
}
void u16be_to_s32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy, u16_be, s32_le);
}
void u16be_to_s32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_e, u16_be, s32_be);
}
void u16be_to_u32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_s, u16_be, u32_le);
}
void u16be_to_u32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_se, u16_be, u32_be);
}

void s24le_to_f32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s24_le, f32_le);
}
void s24le_to_f32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, s24_le, f32_be);
}
void s24le_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s24_le, s8);
}
void s24le_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, s24_le, u8);
}
void s24le_to_s16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s24_le, s16_le);
}
void s24le_to_s16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, s24_le, s16_be);
}
void s24le_to_u16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, s24_le, u16_le);
}
void s24le_to_u16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_se, s24_le, u16_be);
}
void s24le_to_s24be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s24le_e, s24_le, s24_be);
}
void s24le_to_u24le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s24le_s, s24_le, u24_le);
}
void s24le_to_u24be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s24le_se, s24_le, u24_be);
}
void s24le_to_s32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s24_le, s32_le);
}
void s24le_to_s32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, s24_le, s32_be);
}
void s24le_to_u32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, s24_le, u32_le);
}
void s24le_to_u32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_se, s24_le, u32_be);
}

void s24be_to_f32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, s24_be, f32_le);
}
void s24be_to_f32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_e, s24_be, f32_be);
}
void s24be_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, s24_be, s8);
}
void s24be_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_s, s24_be, u8);
}
void s24be_to_s16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, s24_be, s16_le);
}
void s24be_to_s16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_e, s24_be, s16_be);
}
void s24be_to_u16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_s, s24_be, u16_le);
}
void s24be_to_u16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_se, s24_be, u16_be);
}
void s24be_to_s24le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::e_convert_s24be, s24_be, s24_le);
}
void s24be_to_u24le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::e_convert_s24be_s, s24_be, u24_le);
}
void s24be_to_u24be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::e_convert_s24be_se, s24_be, u24_be);
}
void s24be_to_s32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, s24_be, s32_le);
}
void s24be_to_s32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_e, s24_be, s32_be);
}
void s24be_to_u32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_s, s24_be, u32_le);
}
void s24be_to_u32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_se, s24_be, u32_be);
}

void u24le_to_f32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u24_le, f32_le);
}
void u24le_to_f32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_e, u24_le, f32_be);
}
void u24le_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u24_le, s8);
}
void u24le_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_s, u24_le, u8);
}
void u24le_to_s16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u24_le, s16_le);
}
void u24le_to_s16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_e, u24_le, s16_be);
}
void u24le_to_u16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_s, u24_le, u16_le);
}
void u24le_to_u16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_se, u24_le, u16_be);
}
void u24le_to_s24le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::s_convert_u24le, u24_be, s24_le);
}
void u24le_to_s24be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::s_convert_u24le_e, u24_be, s24_be);
}
void u24le_to_u24be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::s_convert_u24le_se, u24_be, u24_be);
}
void u24le_to_s32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u24_le, s32_le);
}
void u24le_to_s32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_e, u24_le, s32_be);
}
void u24le_to_u32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_s, u24_le, u32_le);
}
void u24le_to_u32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_se, u24_le, u32_be);
}

void u24be_to_f32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy, u24_be, f32_le);
}
void u24be_to_f32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_e, u24_be, f32_be);
}
void u24be_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy, u24_be, s8);
}
void u24be_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_s, u24_be, u8);
}
void u24be_to_s16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy, u24_be, s16_le);
}
void u24be_to_s16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_e, u24_be, s16_be);
}
void u24be_to_u16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_s, u24_be, u16_le);
}
void u24be_to_u16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_se, u24_be, u16_be);
}
void u24be_to_s24le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::se_convert_u24be, u24_be, s24_le);
}
void u24be_to_s24be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::se_convert_u24be_e, u24_be, s24_be);
}
void u24be_to_u24le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::se_convert_u24be_s, u24_be, u24_le);
}
void u24be_to_s32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy, u24_le, s32_le);
}
void u24be_to_s32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_e, u24_le, s32_be);
}
void u24be_to_u32le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_s, u24_le, u32_le);
}
void u24be_to_u32be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_se, u24_le, u32_be);
}

void s32le_to_f32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s32le, s32_le, f32_le);
}
void s32le_to_f32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s32le_e, s32_le, f32_be);
}
void s32le_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s32_le, s8);
}
void s32le_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, s32_le, u8);
}
void s32le_to_s16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s32_le, s16_le);
}
void s32le_to_s16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, s32_le, s16_be);
}
void s32le_to_u16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, s32_le, u16_le);
}
void s32le_to_u16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_se, s32_le, u16_be);
}
void s32le_to_s24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy, s32_le, s24_le);
}
void s32le_to_s24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_e, s32_le, s24_be);
}
void s32le_to_u24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_s, s32_le, u24_le);
}
void s32le_to_u24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::copy_se, s32_le, u24_be);
}
void s32le_to_s32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s32le_e, s32_le, s32_be);
}
void s32le_to_u32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s32le_s, s32_le, u32_le);
}
void s32le_to_u32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::convert_s32le_se, s32_le, u32_be);
}

void s32be_to_f32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::e_convert_s32be, s32_be, f32_le);
}
void s32be_to_f32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::e_convert_s32be_e, s32_be, f32_be);
}
void s32be_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, s32_be, s8);
}
void s32be_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_s, s32_be, u8);
}
void s32be_to_s16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, s32_be, s16_le);
}
void s32be_to_s16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_e, s32_be, s16_be);
}
void s32be_to_u16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_s, s32_be, u16_le);
}
void s32be_to_u16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_se, s32_be, u16_be);
}
void s32be_to_s24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy, s32_be, s24_le);
}
void s32be_to_s24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_e, s32_be, s24_be);
}
void s32be_to_u24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_s, s32_be, u24_le);
}
void s32be_to_u24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::e_copy_se, s32_be, u24_be);
}
void s32be_to_s32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::e_convert_s32be, s32_be, s32_le);
}
void s32be_to_u32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::e_convert_s32be_s, s32_be, u32_le);
}
void s32be_to_u32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::e_convert_s32be_se, s32_be, u32_be);
}

void u32le_to_f32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::s_convert_u32le, u32_le, f32_le);
}
void u32le_to_f32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::s_convert_u32le_e, u32_le, f32_be);
}
void u32le_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u32_le, s8);
}
void u32le_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_s, u32_le, u8);
}
void u32le_to_s16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u32_le, s16_le);
}
void u32le_to_s16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_e, u32_le, s16_be);
}
void u32le_to_u16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_s, u32_le, u16_le);
}
void u32le_to_u16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_se, u32_le, u16_be);
}
void u32le_to_s24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy, u32_le, s24_le);
}
void u32le_to_s24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_e, u32_le, s24_be);
}
void u32le_to_u24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_s, u32_le, u24_le);
}
void u32le_to_u24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::s_copy_se, u32_le, u24_be);
}
void u32le_to_s32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::s_convert_u32le, u32_le, s32_le);
}
void u32le_to_s32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::s_convert_u32le_e, u32_le, s32_be);
}
void u32le_to_u32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::s_convert_u32le_se, u32_le, u32_be);
}

void u32be_to_f32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::se_convert_u32be, u32_be, f32_le);
}
void u32be_to_f32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::se_convert_u32be_e, u32_be, f32_be);
}
void u32be_to_s8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy, u32_be, s8);
}
void u32be_to_u8(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_s, u32_be, u8);
}
void u32be_to_s16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy, u32_be, s16_le);
}
void u32be_to_s16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_e, u32_be, s16_be);
}
void u32be_to_u16le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_s, u32_be, u16_le);
}
void u32be_to_u16be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_se, u32_be, u16_be);
}
void u32be_to_s24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy, u32_be, s24_le);
}
void u32be_to_s24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_e, u32_be, s24_be);
}
void u32be_to_u24le(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_s, u32_be, u24_le);
}
void u32be_to_u24be(boxten::PCMPacketUnit& packet) {
    COPY_PACKET(macro::se_copy_se, u32_be, u24_be);
}
void u32be_to_s32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::se_convert_u32be, u32_be, s32_le);
}
void u32be_to_s32be(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::se_convert_u32be_e, u32_be, s32_be);
}
void u32be_to_u32le(boxten::PCMPacketUnit& packet) {
    CONVERT_PACKET(macro::se_convert_u32be_s, u32_be, u32_le);
}
#undef EACH_FRAME
#undef COPY_PACKET
#undef CONVERT_PACKET

using ConvertFunc                             = void (*)(boxten::PCMPacketUnit&);
static ConvertFunc convert_func_table[17][17] = {
    {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {nullptr, nullptr, f32le_to_f32be, f32le_to_s8, f32le_to_u8, f32le_to_s16le, f32le_to_s16be, f32le_to_u16le, f32le_to_u16be, f32le_to_s24le, f32le_to_s24be, f32le_to_u24le, f32le_to_u24be, f32le_to_s32le, f32le_to_s32be, f32le_to_u32le, f32le_to_u32be},
    {nullptr, f32be_to_f32le, nullptr, f32be_to_s8, f32be_to_u8, f32be_to_s16le, f32be_to_s16be, f32be_to_u16le, f32be_to_u16be, f32be_to_s24le, f32be_to_s24be, f32be_to_u24le, f32be_to_u24be, f32be_to_s32le, f32be_to_s32be, f32be_to_u32le, f32be_to_u32be},
    {nullptr, s8_to_f32le, s8_to_f32be, nullptr, s8_to_u8, s8_to_s16le, s8_to_s16be, s8_to_u16le, s8_to_u16be, s8_to_s24le, s8_to_s24be, s8_to_u24le, s8_to_u24be, s8_to_s32le, s8_to_s32be, s8_to_u32le, s8_to_u32be},
    {nullptr, u8_to_f32le, u8_to_f32be, u8_to_s8, nullptr, u8_to_s16le, u8_to_s16be, u8_to_u16le, u8_to_u16be, u8_to_s24le, u8_to_s24be, u8_to_u24le, u8_to_u24be, u8_to_s32le, u8_to_s32be, u8_to_u32le, u8_to_u32be},
    {nullptr, s16le_to_f32le, s16le_to_f32be, s16le_to_s8, s16le_to_u8, nullptr, s16le_to_s16be, s16le_to_u16le, s16le_to_u16be, s16le_to_s24le, s16le_to_s24be, s16le_to_u24le, s16le_to_u24be, s16le_to_s32le, s16le_to_s32be, s16le_to_u32le, s16le_to_u32be},
    {nullptr, s16be_to_f32le, s16be_to_f32be, s16be_to_s8, s16be_to_u8, s16be_to_s16le, nullptr, s16be_to_u16le, s16be_to_u16be, s16be_to_s24le, s16be_to_s24be, s16be_to_u24le, s16be_to_u24be, s16be_to_s32le, s16be_to_s32be, s16be_to_u32le, s16be_to_u32be},
    {nullptr, u16le_to_f32le, u16le_to_f32be, u16le_to_s8, u16le_to_u8, u16le_to_s16le, u16le_to_s16be, nullptr, u16le_to_u16be, u16le_to_s24le, u16le_to_s24be, u16le_to_u24le, u16le_to_u24be, u16le_to_s32le, u16le_to_s32be, u16le_to_u32le, u16le_to_u32be},
    {nullptr, u16be_to_f32le, u16be_to_f32be, u16be_to_s8, u16be_to_u8, u16be_to_s16le, u16be_to_s16be, u16be_to_u16le, nullptr, u16be_to_s24le, u16be_to_s24be, u16be_to_u24le, u16be_to_u24be, u16be_to_s32le, u16be_to_s32be, u16be_to_u32le, u16be_to_u32be},
    {nullptr, s24le_to_f32le, s24le_to_f32be, s24le_to_s8, s24le_to_u8, s24le_to_s16le, s24le_to_s16be, s24le_to_u16le, s24le_to_u16be, nullptr, s24le_to_s24be, s24le_to_u24le, s24le_to_u24be, s24le_to_s32le, s24le_to_s32be, s24le_to_u32le, s24le_to_u32be},
    {nullptr, s24be_to_f32le, s24be_to_f32be, s24be_to_s8, s24be_to_u8, s24be_to_s16le, s24be_to_s16be, s24be_to_u16le, s24be_to_u16be, s24be_to_s24le, nullptr, s24be_to_u24le, s24be_to_u24be, s24be_to_s32le, s24be_to_s32be, s24be_to_u32le, s24be_to_u32be},
    {nullptr, u24le_to_f32le, u24le_to_f32be, u24le_to_s8, u24le_to_u8, u24le_to_s16le, u24le_to_s16be, u24le_to_u16le, u24le_to_u16be, u24le_to_s24le, u24le_to_s24be, nullptr, u24le_to_u24be, u24le_to_s32le, u24le_to_s32be, u24le_to_u32le, u24le_to_u32be},
    {nullptr, u24be_to_f32le, u24be_to_f32be, u24be_to_s8, u24be_to_u8, u24be_to_s16le, u24be_to_s16be, u24be_to_u16le, u24be_to_u16be, u24be_to_s24le, u24be_to_s24be, u24be_to_u24le, nullptr, u24be_to_s32le, u24be_to_s32be, u24be_to_u32le, u24be_to_u32be},
    {nullptr, s32le_to_f32le, s32le_to_f32be, s32le_to_s8, s32le_to_u8, s32le_to_s16le, s32le_to_s16be, s32le_to_u16le, s32le_to_u16be, s32le_to_s24le, s32le_to_s24be, s32le_to_u24le, s32le_to_u24be, nullptr, s32le_to_s32be, s32le_to_u32le, s32le_to_u32be},
    {nullptr, s32be_to_f32le, s32be_to_f32be, s32be_to_s8, s32be_to_u8, s32be_to_s16le, s32be_to_s16be, s32be_to_u16le, s32be_to_u16be, s32be_to_s24le, s32be_to_s24be, s32be_to_u24le, s32be_to_u24be, s32be_to_s32le, nullptr, s32be_to_u32le, s32be_to_u32be},
    {nullptr, u32le_to_f32le, u32le_to_f32be, u32le_to_s8, u32le_to_u8, u32le_to_s16le, u32le_to_s16be, u32le_to_u16le, u32le_to_u16be, u32le_to_s24le, u32le_to_s24be, u32le_to_u24le, u32le_to_u24be, u32le_to_s32le, u32le_to_s32be, nullptr, u32le_to_u32be},
    {nullptr, u32be_to_f32le, u32be_to_f32be, u32be_to_s8, u32be_to_u8, u32be_to_s16le, u32be_to_s16be, u32be_to_u16le, u32be_to_u16be, u32be_to_s24le, u32be_to_s24be, u32be_to_u24le, u32be_to_u24be, u32be_to_s32le, u32be_to_s32be, u32be_to_u32le, nullptr}};
} // namespace

[[maybe_unused]] void dump_packet(boxten::PCMPacketUnit packet, boxten::SampleType to, const char* name) {
    convert_func_table[static_cast<size_t>(packet.format.sample_type)][static_cast<size_t>(to)](packet);
    std::ofstream out(name, std::ios::out | std::ios::binary | std::ios::app);
    out.write((char*)&packet.pcm[0], packet.pcm.size());
}

// convesion from / to (s8 | u16 | u24 | u32) are not tested
bool PCMFormat::modify_packet(boxten::PCMPacketUnit& packet) {
    auto func = convert_func_table[static_cast<size_t>(packet.format.sample_type)][static_cast<size_t>(to)];
    if(func != nullptr) func(packet);
    return true;
}
PCMFormat::PCMFormat(void* param) : boxten::SoundProcessor(param) {
    if(i64 to_s; get_number("to", to_s)) {
        to = static_cast<boxten::SampleType>(to_s);
    }
}
PCMFormat::~PCMFormat() {
    set_number("to", static_cast<i64>(to));
}

BOXTEN_MODULE({"packet format", boxten::COMPONENT_TYPE::SOUND_PROCESSOR, CATALOGUE_CALLBACK(PCMFormat)})
