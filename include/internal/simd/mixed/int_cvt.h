#pragma once
#define __QO_INT_CVT_MIXED_SIMD_H__

#include <immintrin.h>

// for debug
#include "../../../int_cvt.h"

#if !defined(__QO_INT_CVT_H__)
    # error Never include this header directly. Use int_cvt.h instead.
#endif
extern inline qo_uint32_t
parse_eight_digits_unrolled_sse(
    const qo_uint8_t * chars
) {
    // this actually computes *16* values so we are being wasteful.
    const __m128i  ascii0 = _mm_set1_epi8('0');
    const __m128i  mul_1_10 =
        _mm_setr_epi8(10 , 1 , 10 , 1 , 10 , 1 , 10 , 1 , 10 , 1 , 10 , 1 , 10 ,
            1 , 10 , 1);
    const __m128i  mul_1_100 = _mm_setr_epi16(100 , 1 , 100 , 1 , 100 , 1 ,
        100 , 1);
    const __m128i  mul_1_10000 =
        _mm_setr_epi16(10000 , 1 , 10000 , 1 , 10000 , 1 , 10000 , 1);
    const __m128i  input = _mm_sub_epi8(
        _mm_loadu_si128((const __m128i *) chars) , ascii0
    );                                                    // 使用C语言类型转换
    const __m128i  t1 = _mm_maddubs_epi16(input , mul_1_10);
    const __m128i  t2 = _mm_madd_epi16(t1 , mul_1_100);
    const __m128i  t3 = _mm_packus_epi32(t2 , t2);
    const __m128i  t4 = _mm_madd_epi16(t3 , mul_1_10000);
    return _mm_cvtsi128_si32(
        t4
    );       // only captures the sum of the first 8 digits, drop the rest
}
extern inline
qo_size_t
__qo_hex64_to_compact_str_common_mixed_simd(
    qo_uint64_t     x ,
    qo_cstring_t       buffer ,
    qo_ccstring_t hex_table
) {
    if (!x)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }

    int  start_index = __builtin_clzll(x) >> 2; // 计算最高非零 4-bit 组索引
    qo_cstring_t ptr = buffer + start_index;

    if (0)
    {
    }        // to make following else-ifs work
#if 1 //defined(__AVX512F__)
    else if (start_index >= 15)
    {
        __m512i  v_x = _mm512_set1_epi64(x);
        __m512i  shift = _mm512_set_epi64(60 , 56 , 52 , 48 , 44 , 40 , 36 ,
            32);
        __m512i  indices = _mm512_and_si512(_mm512_srlv_epi64(v_x , shift) ,
            _mm512_set1_epi64(0xF));

        __m512i  table = _mm512_loadu_si512((const __m512i *) hex_table);
        __m512i  chars = _mm512_permutexvar_epi8(indices , table);

        _mm512_mask_storeu_epi8(ptr , 0xFFFF , chars); // 仅写入 16 字节
        ptr += 16;
        // No need to scalarly handle remainings because we have 16 bytes
    }
#endif // #if defined(__AVX512F__)
#if 1 //defined(__AVX2__)
    else if (start_index >= 7)
    {
        __m256i  v_x = _mm256_set1_epi64x(x);
        __m256i  shift = _mm256_set_epi64x(28 , 24 , 20 , 16 , 12 , 8 , 4 , 0);
        __m256i  indices = _mm256_and_si256(_mm256_srlv_epi64(v_x , shift) ,
            _mm256_set1_epi64x(0xF));

        __m256i  table = _mm256_loadu_si256((const __m256i *) hex_table);
        __m256i  chars = _mm256_permutevar8x32_epi32(table , indices);

        _mm256_storeu_si256((__m256i *) ptr , chars);
        ptr += 8;
        // TODO: Scalarly handle remainings
    }
#endif // #if defined(__AVX2__)
#if defined(__SSE2__)
    else if (start_index >= 3)    // SSE2: 4 HEX
    {
        __m128i  v_x = _mm_set1_epi64x(x);
        __m128i  shift = _mm_set_epi64x(12 , 8 , 4 , 0);
        __m128i  indices = _mm_and_si128(_mm_srl_epi64(v_x , shift) ,
            _mm_set1_epi64x(0xF));

        __m128i  table = _mm_loadu_si128((const __m128i *) hex_table);
        __m128i  chars = _mm_shuffle_epi8(table , indices);

        _mm_storeu_si128((__m128i *) ptr , chars);
        ptr += 4;
    }
#endif // #if defined(__SSE2__)

    switch (start_index)
    {
        case 0:
            *ptr++ = hex_table[(x >> 60) & 0xF];

        case 1:
            *ptr++ = hex_table[(x >> 56) & 0xF];

        case 2:
            *ptr++ = hex_table[(x >> 52) & 0xF];

        case 3:
            *ptr++ = hex_table[(x >> 48) & 0xF];

        case 4:
            *ptr++ = hex_table[(x >> 44) & 0xF];

        case 5:
            *ptr++ = hex_table[(x >> 40) & 0xF];

        case 6:
            *ptr++ = hex_table[(x >> 36) & 0xF];

        case 7:
            *ptr++ = hex_table[(x >> 32) & 0xF];

        case 8:
            *ptr++ = hex_table[(x >> 28) & 0xF];

        case 9:
            *ptr++ = hex_table[(x >> 24) & 0xF];

        case 10:
            *ptr++ = hex_table[(x >> 20) & 0xF];

        case 11:
            *ptr++ = hex_table[(x >> 16) & 0xF];

        case 12:
            *ptr++ = hex_table[(x >> 12) & 0xF];

        case 13:
            *ptr++ = hex_table[(x >> 8) & 0xF];

        case 14:
            *ptr++ = hex_table[(x >> 4) & 0xF];

        case 15:
            *ptr++ = hex_table[(x) & 0xF];
    }

    *ptr = '\0';
    return ptr - buffer;
}
