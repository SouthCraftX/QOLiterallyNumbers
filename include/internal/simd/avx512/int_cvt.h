#pragma once
#include "types.h"
#define __QO_INT_CVT_AVX512_H__

#include <stdint.h>
#include <immintrin.h>
// tested working
extern inline
void
qo_dec64_to_untrimmed_str_avx512ifma(
    qo_uint64_t  x ,
    char *       buffer
) {
    qo_uint64_t  n_15_08 = x / 100000000;
    qo_uint64_t  n_07_00 = x % 100000000;
    __m512i  bcstq_h = _mm512_set1_epi64(n_15_08);
    __m512i  bcstq_l = _mm512_set1_epi64(n_07_00);
    __m512i  zmmzero = _mm512_castsi128_si512(_mm_cvtsi64_si128(0x1A1A400));
    __m512i  zmmTen  = _mm512_set1_epi64(10);
    __m512i  ascii_zero = _mm512_set1_epi64('0');

    __m512i  ifma_const = _mm512_setr_epi64(0x00000000002af31dc ,
        0x0000000001ad7f29b , 0x0000000010c6f7a0c , 0x00000000a7c5ac472 ,
        0x000000068db8bac72 , 0x0000004189374bc6b , 0x0000028f5c28f5c29 ,
        0x0000199999999999a);
    __m512i  permb_const = _mm512_castsi128_si512(
        _mm_set_epi8(
            0x78 , 0x70 , 0x68 , 0x60 , 0x58 , 0x50 , 0x48 , 0x40 , 0x38 ,
            0x30 , 0x28 , 0x20 , 0x18 , 0x10 , 0x08 , 0x00
        )
    );
    __m512i  lowbits_h  = _mm512_madd52lo_epu64(zmmzero , bcstq_h , ifma_const);
    __m512i  lowbits_l  = _mm512_madd52lo_epu64(zmmzero , bcstq_l , ifma_const);
    __m512i  highbits_h = _mm512_madd52hi_epu64(ascii_zero , zmmTen ,
        lowbits_h);
    __m512i  highbits_l = _mm512_madd52hi_epu64(ascii_zero , zmmTen ,
        lowbits_l);
    __m512i  perm = _mm512_permutex2var_epi8(highbits_h , permb_const ,
        highbits_l);
    __m128i  digits_15_0 = _mm512_castsi512_si128(perm);
    _mm_storeu_si128((__m128i *) buffer , digits_15_0);
}
// tested working
extern inline
void
qo_bin64_to_untrimmed_str_avx512(
    qo_uint64_t   value ,
    qo_cstring_t  buffer
) {
    // Use AVX-512 to process all bits at once
    __m512i  ascii_zero = _mm512_set1_epi8('0');
    __m512i  ascii_one  = _mm512_set1_epi8('1');

    // Byte-swap the value first (reverse byte order)
    qo_uint64_t  swapped_value = __builtin_bswap64(value);

    // Create bit mask directly - this is our key optimization
    __mmask64  bit_mask = swapped_value;

    // Use the mask to blend '0' and '1' characters
    __m512i  result = _mm512_mask_blend_epi8(bit_mask , ascii_zero , ascii_one);

    // Store the result
    _mm512_store_si512((__m512i *) buffer , result);
}
// tested working
extern inline
void
qo_hex64_to_untrimmed_str_avx512(
    qo_uint64_t   value ,
    qo_cstring_t  buffer
) {
    // Create a vector with our 64-bit value replicated
    __m512i  val_vec = _mm512_set1_epi64(value);

    // Create shift amounts for extracting nibbles (60, 56, 52, ..., 4, 0)
    __m512i  shift_amounts = _mm512_set_epi32(
        0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 60 , 56 , 52 , 48 , 44 , 40 , 36 , 32
    );

    // Create a second vector for the lower 32 bits
    __m512i  shift_amounts2 = _mm512_set_epi32(
        0 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , 28 , 24 , 20 , 16 , 12 , 8 , 4 , 0
    );

    // Extract and convert the upper 8 nibbles
    __m512i  shifted1 = _mm512_srlv_epi32(val_vec , shift_amounts);
    __m512i  nibbles1 = _mm512_and_si512(shifted1 , _mm512_set1_epi32(0xF));

    // Extract and convert the lower 8 nibbles
    __m512i  shifted2 = _mm512_srlv_epi32(val_vec , shift_amounts2);
    __m512i  nibbles2 = _mm512_and_si512(shifted2 , _mm512_set1_epi32(0xF));

    // Combine the nibbles into a single vector
    __m512i  nibbles = _mm512_mask_blend_epi32(0xFF00 , nibbles2 , nibbles1);

    // Convert to ASCII: digits 0-9 -> '0'-'9', digits 10-15 -> 'a'-'f'
    __mmask64  is_digit = _mm512_cmplt_epi8_mask(nibbles ,
        _mm512_set1_epi8(10));
    __m512i    digit_0_9 = _mm512_set1_epi8('0');
    __m512i    digit_a_f = _mm512_set1_epi8('a' - 10);
    __m512i    digit_offset = _mm512_mask_blend_epi8(is_digit , digit_a_f ,
        digit_0_9);
    __m512i    hex_chars = _mm512_add_epi8(nibbles , digit_offset);

    // Store the result (16 bytes)
    _mm_storeu_si128((__m128i *) buffer ,
        _mm512_extracti32x4_epi32(hex_chars , 0));

    // Add null terminator
    buffer[16] = '\0';
}
#if defined(__AVX512VL__)
// Tested working
extern inline
void
qo_hex64_to_untrimmed_str_avx512vl(
    qo_uint64_t  x ,
    char *       buffer
) {
    alignas(16) static const qo_uint8_t hex_table[16] = {
        '0' , '1' , '2' , '3' , '4' , '5' , '6' , '7' , '8' , '9' , 'a' , 'b' ,
        'c' , 'd' , 'e' , 'f'
    };

    // 加载64位值到128位寄存器（小端序）
    __m128i  v = _mm_loadl_epi64((const __m128i *) &value);

    // 关键步骤1：正确反转字节顺序（小端转大端）
    const __m128i  shuffle_rev = _mm_set_epi8(
        0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 ,
        // 低8字节：目标位置7-0 对应源索引0-7（反转顺序）
        0 , 1 , 2 , 3 , 4 , 5 , 6 , 7  // 修正点：确保低8字节反转
    );
    v = _mm_shuffle_epi8(v , shuffle_rev);

    // 关键步骤2：按字节拆分高低四位（而非64位整体移位）
    __m128i  high = _mm_srli_epi16(v , 4);  // 每个16位元素右移4位（即每个字节的高四位）
    high = _mm_and_si128(high , _mm_set1_epi8(0x0F));
    __m128i  low = _mm_and_si128(v , _mm_set1_epi8(0x0F));

    // 关键步骤3：合并半字节（高四位在前，低四位在后）
    __m128i  high_low = _mm_unpacklo_epi8(high , low);

    // 查表转换
    __m128i  result = _mm_shuffle_epi8(
        _mm_load_si128((const __m128i *) hex_table) , high_low
    );

    // 存储结果
    _mm_storeu_si128((__m128i *) buffer , result);
    buffer[16] = '\0';
}
#endif // __AVX512VL__
extern inline
qo_bool_t
// Return: Overflow?
qo_decstr_to_u64_avx512(
    qo_ccstring_t  begin ,
    qo_ccstring_t  end ,
    qo_uint64_t *  p_value
) {
    qo_size_t  digit_count = qo_size_t(end - begin);
    if ((digit_count == 0) || (digit_count > 20))
    {
        return false;
    }
    if ((digit_count > 1) && ('0' == *begin))
    {
        return false;
    }
    // Load bytes
    const __m256i  ASCII_ZERO = _mm256_set1_epi8('0');
    const __m256i  NINE = _mm256_set1_epi8(9);
    qo_uint32_t    mask = qo_uint32_t(0xFFFFFFFF) << (begin - end + 32);
    __m256i  in = _mm256_maskz_loadu_epi8(mask , end - 32);
    __m256i  base10_8bit = _mm256_maskz_sub_epi8(mask , in , ASCII_ZERO);
    auto  nondigits = _mm256_mask_cmpgt_epu8_mask(mask , base10_8bit , NINE);
    if (nondigits)
    {
        return false;
    }

    // Convert bytes to a 32-digit base-10 integer by subtracting '0'.
    __m128i  base10e8_32bit = parse_8digit_integers_simd_reverse(base10_8bit);

    // Maximum 64-bit unsigned integer is 1844 67440737 09551615 (20 digits)
    qo_uint64_t  result_1digit =
        (qo_uint64_t) _mm_extract_epi32(base10e8_32bit , 3);
    if ((mask & 0xffffff) == 0)
    {
        value = result_1digit;
        return true;
    }

    qo_uint64_t  middle_part = (qo_uint64_t) _mm_extract_epi32(base10e8_32bit ,
        2);

    qo_uint64_t  result_2digit = result_1digit + 100000000 * middle_part;
    if ((mask & 0xffff) == 0)
    {
        value = result_2digit;
        return true;
    }
    qo_uint64_t  high_part = (qo_uint64_t) _mm_extract_epi32(base10e8_32bit ,
        1);

    qo_uint64_t  result = result_2digit + 10000000000000000 * high_part;
    if (high_part > 1844 || result < result_2digit)
    {
        return false;
    }
    *p_value = result;
    return true;
}
