#pragma once
#include <cstdint>
#include <tmmintrin.h>
#define __QO_INT_CVT_SSE2_H__

#include <emmintrin.h>
#include <stdint.h>

inline
void 
qo_bin64_to_fixed65_str_sse4(uint64_t x, char buffer[65]) {
    // 将64位整数拆分为高32位和低32位，并转换为大端序
    const uint32_t val_hi = __builtin_bswap32((uint32_t)(x >> 32)); // 高32位大端序
    const uint32_t val_lo = __builtin_bswap32((uint32_t)x);         // 低32位大端序

    // 预计算静态掩码（避免重复生成）
    static const __m128i bit_mask = _mm_setr_epi8(
        0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,  // 高位到低位
        0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
    );
    static const __m128i expand_shuf_hi = _mm_setr_epi8(
        0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1  // 扩展大端序的字节0、1（高位在前）
    );
    static const __m128i expand_shuf_lo = _mm_setr_epi8(
        2,2,2,2,2,2,2,2, 3,3,3,3,3,3,3,3  // 扩展大端序的字节2、3（低位在后）
    );

    // 处理高32位（分两次扩展）
    __m128i vec_hi = _mm_set_epi32(0, 0, 0, val_hi); // 明确大端序布局
    __m128i hi_part1 = _mm_shuffle_epi8(vec_hi, expand_shuf_hi); // 扩展字节0、1
    __m128i hi_part2 = _mm_shuffle_epi8(vec_hi, expand_shuf_lo); // 扩展字节2、3

    // 位测试与字符转换（高位到低位）
    hi_part1 = _mm_and_si128(hi_part1, bit_mask);
    hi_part1 = _mm_cmpeq_epi8(hi_part1, bit_mask);
    hi_part1 = _mm_add_epi8(_mm_set1_epi8('0'), _mm_and_si128(hi_part1, _mm_set1_epi8(1)));

    hi_part2 = _mm_and_si128(hi_part2, bit_mask);
    hi_part2 = _mm_cmpeq_epi8(hi_part2, bit_mask);
    hi_part2 = _mm_add_epi8(_mm_set1_epi8('0'), _mm_and_si128(hi_part2, _mm_set1_epi8(1)));

    // 处理低32位（同上）
    __m128i vec_lo = _mm_set_epi32(0, 0, 0, val_lo);
    __m128i lo_part1 = _mm_shuffle_epi8(vec_lo, expand_shuf_hi);
    __m128i lo_part2 = _mm_shuffle_epi8(vec_lo, expand_shuf_lo);

    lo_part1 = _mm_and_si128(lo_part1, bit_mask);
    lo_part1 = _mm_cmpeq_epi8(lo_part1, bit_mask);
    lo_part1 = _mm_add_epi8(_mm_set1_epi8('0'), _mm_and_si128(lo_part1, _mm_set1_epi8(1)));

    lo_part2 = _mm_and_si128(lo_part2, bit_mask);
    lo_part2 = _mm_cmpeq_epi8(lo_part2, bit_mask);
    lo_part2 = _mm_add_epi8(_mm_set1_epi8('0'), _mm_and_si128(lo_part2, _mm_set1_epi8(1)));

    // 合并结果（按大端序存储）
    _mm_storeu_si128((__m128i*)buffer, hi_part1);        // 存储字节0、1的二进制
    _mm_storeu_si128((__m128i*)(buffer + 16), hi_part2); // 存储字节2、3的二进制
    _mm_storeu_si128((__m128i*)(buffer + 32), lo_part1); // 存储低32位字节0、1
    _mm_storeu_si128((__m128i*)(buffer + 48), lo_part2); // 存储低32位字节2、3
    buffer[64] = '\0';
}

inline
void
qo_bin64_to_fixed65_str_sse2(
    uint64_t x ,
    char * buffer
) {
    buffer[64] = '\0';
    const char * lookup = __qo_bin_to_str_table;
    for (int chunk = 0 ; chunk < 4 ; ++chunk)
    {
        // Extract 16 bits
        uint16_t chunk_val = (value >> (48 - chunk * 16)) & 0xFFFF;
        
        // Process 4 nibbles in this chunk
        uint8_t nibble0 = (chunk_val >> 12) & 0xF;
        uint8_t nibble1 = (chunk_val >> 8) & 0xF;
        uint8_t nibble2 = (chunk_val >> 4) & 0xF;
        uint8_t nibble3 = chunk_val & 0xF;
        
        // Use SIMD to load and store 16 characters at once
        __m128i chars0 = _mm_loadu_si32(lookup[nibble0]);
        __m128i chars1 = _mm_loadu_si32(lookup[nibble1]);
        __m128i chars2 = _mm_loadu_si32(lookup[nibble2]);
        __m128i chars3 = _mm_loadu_si32(lookup[nibble3]);
        
        // Combine the 4 nibbles into a 128-bit vector
        __m128i result_lo = _mm_unpacklo_epi32(chars0, chars1);
        __m128i result_hi = _mm_unpacklo_epi32(chars2, chars3);
        __m128i result = _mm_unpacklo_epi64(result_lo, result_hi);
        
        // Store the result
        _mm_store_si128((__m128i*)(buffer + chunk * 16), result);
    }
}

inline
void
qo_16udec_to_fixed_str_sse2(
    uint64_t x ,
    char * buffer
) {
    
  // v is 16-digit number = abcdefghijklmnop
  const __m128i div_10000 = _mm_set1_epi32(0xd1b71759);
  const __m128i mul_10000 = _mm_set1_epi32(10000);
  const int div_10000_shift = 45;

  const __m128i div_100 = _mm_set1_epi16(0x147b);
  const __m128i mul_100 = _mm_set1_epi16(100);
  const int div_100_shift = 3;

  const __m128i div_10 = _mm_set1_epi16(0x199a);
  const __m128i mul_10 = _mm_set1_epi16(10);

  const __m128i ascii0 = _mm_set1_epi8('0');

  // can't be easliy done in SSE
  const uint32_t a = v / 100000000; // 8-digit number: abcdefgh
  const uint32_t b = v % 100000000; // 8-digit number: ijklmnop

  //                [ 3 | 2 | 1 | 0 | 3 | 2 | 1 | 0 | 3 | 2 | 1 | 0 | 3 | 2 | 1
  //                | 0 ]
  // x            = [       0       |      ijklmnop |       0       | abcdefgh ]
  __m128i x = _mm_set_epi64x(b, a);

  // x div 10^4   = [       0       |          ijkl |       0       | abcd ]
  __m128i x_div_10000;
  x_div_10000 = _mm_mul_epu32(x, div_10000);
  x_div_10000 = _mm_srli_epi64(x_div_10000, div_10000_shift);

  // x mod 10^4   = [       0       |          mnop |       0       | efgh ]
  __m128i x_mod_10000;
  x_mod_10000 = _mm_mul_epu32(x_div_10000, mul_10000);
  x_mod_10000 = _mm_sub_epi32(x, x_mod_10000);

  // y            = [          mnop |          ijkl |          efgh | abcd ]
  __m128i y = _mm_or_si128(x_div_10000, _mm_slli_epi64(x_mod_10000, 32));

  // y_div_100    = [   0   |    mn |   0   |    ij |   0   |    ef |   0   | ab
  // ]
  __m128i y_div_100;
  y_div_100 = _mm_mulhi_epu16(y, div_100);
  y_div_100 = _mm_srli_epi16(y_div_100, div_100_shift);

  // y_mod_100    = [   0   |    op |   0   |    kl |   0   |    gh |   0   | cd
  // ]
  __m128i y_mod_100;
  y_mod_100 = _mm_mullo_epi16(y_div_100, mul_100);
  y_mod_100 = _mm_sub_epi16(y, y_mod_100);

  // z            = [    mn |    ij |    ef |    ab |    op |    kl |    gh | cd
  // ]
  __m128i z = _mm_packus_epi32(y_div_100, y_mod_100);

  // z_div_10     = [ 0 | m | 0 | i | 0 | e | 0 | a | 0 | o | 0 | k | 0 | g | 0
  // | c ]
  __m128i z_div_10 = _mm_mulhi_epu16(z, div_10);

  // z_mod_10     = [ 0 | n | 0 | j | 0 | f | 0 | b | 0 | p | 0 | l | 0 | h | 0
  // | d ]
  __m128i z_mod_10;
  z_mod_10 = _mm_mullo_epi16(z_div_10, mul_10);
  z_mod_10 = _mm_sub_epi16(z, z_mod_10);

  // interleave z_mod_10 and z_div_10 -
  // tmp          = [ m | i | e | a | o | k | g | c | n | j | f | b | p | l | h
  // | d ]
  __m128i tmp = _mm_packus_epi16(z_div_10, z_mod_10);

  const __m128i reorder =
      _mm_set_epi8(15, 7, 11, 3, 14, 6, 10, 2, 13, 5, 9, 1, 12, 4, 8, 0);
  tmp = _mm_shuffle_epi8(tmp, reorder);

  // convert to ascii
  tmp = _mm_add_epi8(tmp, ascii0);

  // and save result
  _mm_storeu_si128((__m128i *)buffer, tmp);
}