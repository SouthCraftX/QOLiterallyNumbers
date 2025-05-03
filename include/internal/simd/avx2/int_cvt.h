#pragma once
#define __QO_INV_CVT_AVX2__

#include <stdint.h>
#include <immintrin.h>

// tested working
extern inline
void 
qo_bin64_to_untrimmed_str_avx2(
    qo_uint64_t x, 
    char buffer[65]
) {
    // 将64位整数拆分为高32位和低32位，并转换为大端序
    const qo_uint32_t val_hi = __builtin_bswap32((qo_uint32_t)(x >> 32));
    const qo_uint32_t val_lo = __builtin_bswap32((qo_uint32_t)x);

    // 预计算静态掩码（避免重复生成）
    static const __m256i bit_mask = _mm256_setr_epi8(
        0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,  // 高位到低位
        0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
        0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
        0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
    );
    static const __m256i expand_shuf = _mm256_setr_epi8(
        0,0,0,0,0,0,0,0,  // 第0字节扩展8次
        1,1,1,1,1,1,1,1,  // 第1字节扩展8次
        2,2,2,2,2,2,2,2,  // 第2字节扩展8次
        3,3,3,3,3,3,3,3   // 第3字节扩展8次
    );

    // 处理高32位（每个字节扩展8次）
    __m256i vec_hi = _mm256_broadcastd_epi32(_mm_set1_epi32(val_hi));  // 广播到256位
    vec_hi = _mm256_shuffle_epi8(vec_hi, expand_shuf);                 // 扩展每个字节
    vec_hi = _mm256_and_si256(vec_hi, bit_mask);                       // 位分离
    vec_hi = _mm256_cmpeq_epi8(vec_hi, bit_mask);                      // 生成布尔掩码
    vec_hi = _mm256_add_epi8(_mm256_set1_epi8('0'), 
                            _mm256_and_si256(vec_hi, _mm256_set1_epi8(1))); // 转为'0'/'1'

    // 处理低32位（同上）
    __m256i vec_lo = _mm256_broadcastd_epi32(_mm_set1_epi32(val_lo));
    vec_lo = _mm256_shuffle_epi8(vec_lo, expand_shuf);
    vec_lo = _mm256_and_si256(vec_lo, bit_mask);
    vec_lo = _mm256_cmpeq_epi8(vec_lo, bit_mask);
    vec_lo = _mm256_add_epi8(_mm256_set1_epi8('0'), 
                            _mm256_and_si256(vec_lo, _mm256_set1_epi8(1)));

    // 合并结果并存储
    _mm256_storeu_si256((__m256i*)buffer, vec_hi);        // 前32字节
    _mm256_storeu_si256((__m256i*)(buffer + 32), vec_lo); // 后32字节
    buffer[64] = '\0'; // 终止符
}

// tested working
extern inline
void 
qo_hex64_to_untrimmed_str_avx2(
    qo_uint64_t value , 
    char* buffer
) {
    // Create byte array from the 64-bit value
    union {
        qo_uint64_t val;
        qo_uint8_t bytes[8];
    } u;
    u.val = __builtin_bswap64(value); // Ensure big-endian byte order
    
    // Expand each byte into two nibbles using AVX2
    __m128i bytes_vec = _mm_loadl_epi64((__m128i*)u.bytes);
    __m256i expanded;
    
    // Unpack each byte into two nibbles
    __m256i shuffle_mask = _mm256_set_epi8(
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0
    );
    expanded = _mm256_shuffle_epi8(_mm256_castsi128_si256(bytes_vec), shuffle_mask);
    
    // Extract high and low nibbles
    __m256i high_mask = _mm256_set1_epi8(0xF0);
    __m256i low_mask = _mm256_set1_epi8(0x0F);
    
    __m256i high_nibbles = _mm256_and_si256(_mm256_srli_epi16(expanded, 4), low_mask);
    __m256i low_nibbles = _mm256_and_si256(expanded, low_mask);
    
    // Interleave high and low nibbles
    __m256i nibbles_vec = _mm256_unpacklo_epi8(high_nibbles, low_nibbles);
    
    // Convert to ASCII: digits 0-9 -> '0'-'9', digits 10-15 -> 'a'-'f'
    __m256i digit_0_9 = _mm256_set1_epi8('0');
    __m256i digit_a_f = _mm256_set1_epi8('a' - 10);
    
    // Compare each nibble with 10 to determine if it's 0-9 or a-f
    __m256i cmp = _mm256_cmpgt_epi8(_mm256_set1_epi8(10), nibbles_vec);
    
    // Select the appropriate offset based on the comparison
    __m256i offset = _mm256_blendv_epi8(digit_a_f, digit_0_9, cmp);
    
    // Add the offset to convert nibbles to ASCII
    __m256i hex_chars = _mm256_add_epi8(nibbles_vec, offset);
    
    // Store the result
    _mm256_storeu_si256((__m256i*)buffer, hex_chars);
    
    // Add null terminator
    buffer[16] = '\0';
}


extern inline __m128i parse_8digit_integers_simd_reverse(__m256i base10_8bit) {
    const __m256i DIGIT_VALUE_BASE10_8BIT =
        _mm256_set_epi8(1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1,
                        10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10);
    const __m128i DIGIT_VALUE_BASE10E2_8BIT = _mm_set_epi8(
        1, 100, 1, 100, 1, 100, 1, 100, 1, 100, 1, 100, 1, 100, 1, 100);
    const __m128i DIGIT_VALUE_BASE10E4_16BIT =
        _mm_set_epi16(1, 10000, 1, 10000, 1, 10000, 1, 10000);
    // Multiply pairs of base-10 digits by [10,1] and add them to create 16
    // base-10^2 digits.
    __m256i base10e2_16bit =
        _mm256_maddubs_epi16(base10_8bit, DIGIT_VALUE_BASE10_8BIT);
    __m128i base10e2_8bit = _mm256_cvtepi16_epi8(base10e2_16bit);
  
    // Multiply pairs of base-10^2 digits by [10^2,1] and add them to create a 16
    // base-10^4 digits.
    __m128i base10e4_16bit =
        _mm_maddubs_epi16(base10e2_8bit, DIGIT_VALUE_BASE10E2_8BIT);
    // Multiply pairs of base-10^4 digits by [10^4,1] and add them to create an 8
    // base-10^8 digits.
    __m128i base10e8_32bit =
        _mm_madd_epi16(base10e4_16bit, DIGIT_VALUE_BASE10E4_16BIT);
    return base10e8_32bit;
  }