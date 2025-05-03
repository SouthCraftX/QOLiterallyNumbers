#pragma once
#define __QO_INT_CVT_H__

#include <ctype.h>
#include <string.h>
#include <assert.h>
#include "qozero.h"

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

#define __QO_HEX_TABLE_UPPERCASE  "0123456789ABCDEF"
#define __QO_HEX_TABLE_LOWERCASE  "0123456789abcdef"
#define __QO_OCT_TABLE            __QO_HEX_TABLE_UPPERCASE // We only use front 8 bits for octal

char const  __qo_bin_to_str_table[16][4] = {
    {'0' , '0' , '0' , '0'} , {'0' , '0' , '0' , '1'} ,
    {'0' , '0' , '1' , '0'} ,
    {'0' , '0' , '1' , '1'} ,
    {'0' , '1' , '0' , '0'} , {'0' , '1' , '0' , '1'} ,
    {'0' , '1' , '1' , '0'} ,
    {'0' , '1' , '1' , '1'} ,
    {'1' , '0' , '0' , '0'} , {'1' , '0' , '0' , '1'} ,
    {'1' , '0' , '1' , '0'} ,
    {'1' , '0' , '1' , '1'} ,
    {'1' , '1' , '0' , '0'} , {'1' , '1' , '0' , '1'} ,
    {'1' , '1' , '1' , '0'} ,
    {'1' , '1' , '1' , '1'}
};
/// @brief Convert a 64 bit unsigned integer that is less than 10000 to a 5 byte string
/// @param x The integer to convert
/// @param buffer The buffer to store the string in. The size of the buffer must be at least 5
// bytes.
extern inline
void
qo_4udec_to_str(
    qo_uint64_t   x ,
    qo_cstring_t  buffer
) {
    const qo_uint64_t  mask = 0x0FFFFE1FFFF87FFF;
    qo_uint64_t  y;
    // assert(x <= 9999);

    x *= 0x000418A051EC0CCD;
    y  = (x & mask) * 10;

    // extract...
    y &= ~mask;
    x  = (qo_uint32_t) ((y << 9) + (y >> 21) + (y >> 52) + (x >> 60));
    x += 0x30303030;
    memcpy(buffer , &x , 5);
}

// tested working
extern inline
void
qo_5udec_to_str_fixed(
    qo_uint16_t  x,
    char     *buf  /* 至少 6 字节：5个数字 + '\0' */
) {
    /* 用于 4 位十进制拆分的常量 */
    const qo_uint64_t M4 = 0x0FFFFE1FFFF87FFFULL;

    /* q = ⌊x/10000⌋，范围 0…6 */
    qo_uint32_t q = (qo_uint32_t)((qo_uint64_t)x * 28147497672ULL >> 48);

    /* r = x mod 10000 */
    qo_uint16_t r = x - (qo_uint16_t)(q * 10000);

    /* 对 r 做“magic multiply + mask”得到四个 ASCII 数字 */
    qo_uint64_t t = (qo_uint64_t)r * 0x000418A051EC0CCDULL;
    qo_uint64_t y = (t & M4) * 10;
    y &= ~M4;
    qo_uint32_t d4 = (qo_uint32_t)(
        (y << 9) +
        (y >> 21) +
        (y >> 52) +
        (t >> 60)
    );
    qo_uint64_t ascii4 = (qo_uint64_t)d4 + 0x30303030ULL;

    /* 把高位 + 4 位一起写入 buf[0..4]，然后终止符 */
    buf[0] = (char)('0' + q);
    memcpy(buf + 1, &ascii4, 4);
    buf[5] = '\0';
}


/// @brief Convert a 64 bit signed integer that is less than 100 to a 3 byte string
/// @param x The integer to convert
/// @param buffer The buffer to store the string in. The size of the buffer must be at least 3
// bytes.
/// @return The length of the string
extern inline
qo_uint32_t
qo_2idec_to_str(
    qo_int64_t    x ,
    qo_cstring_t  buffer
) {
    if (x <= 9)
    {
        *buffer = (char) (x | 0x30);
        return 1;
    }
    else if (x <= 99)
    {
        qo_uint64_t  low = x;
        qo_uint64_t  ll  = ((low * 103) >> 9) & 0x1E;
        low += ll * 3;
        ll   = ((low & 0xF0) >> 4) | ((low & 0x0F) << 8);
        *(qo_uint16_t *) buffer = (qo_uint16_t) (ll | 0x3030);
        return 2;
    }
    return 0;
}
extern inline
qo_uint32_t
// tested working
qo_4udec_to_str2(
    qo_uint64_t   x ,
    qo_cstring_t  buffer
) {
    qo_uint64_t  low;
    qo_uint64_t  ll;
    qo_uint32_t  digits;

    if (x <= 99)
    {
        return qo_2idec_to_str(x , buffer);
    }

    low = x;
    digits = (low > 999) ? 4 : 3;

    // division and remainder by 100
    // Simply dividing by 100 instead of multiply-and-shift
    // is about 50% more expensive timewise on my box
    ll   = ((low * 5243) >> 19) & 0xFF;
    low -= ll * 100;

    low = (low << 16) | ll;

    // Two divisions by 10 (14 bits needed)
    ll   = ((low * 103) >> 9) & 0x1E001E;
    low += ll * 3;

    // move digits into correct spot
    ll = ((low & 0x00F000F0) << 28) | (low & 0x000F000F) << 40;

    // convert from decimal digits to ASCII number digit range
    ll |= 0x3030303000000000;

    qo_uint8_t * p = (qo_uint8_t *) &ll;
    if (digits == 4)
    {
        *(qo_uint32_t *) buffer = *(qo_uint32_t *) (&p[4]);
    }
    else {
        *(qo_uint16_t *) buffer = *(qo_uint16_t *) (&p[5]);
        *(((qo_uint8_t *) buffer) + 2) = *(qo_uint8_t *) (&p[7]);
    }

    return digits;
}
/// @brief Convert a 64 bit unsigned integer that less than UINT32_MAX to a 10 byte string
/// @param x The integer to convert
/// @param buffer The buffer to store the string in. The size of the buffer must be at least 8
// bytes.
/// @return The length of the string
extern inline
qo_uint32_t
// tested
qo_10udec_to_str_trimmed(
    qo_uint64_t   x ,
    qo_cstring_t  buffer
) {
    qo_uint64_t  low;
    qo_uint64_t  ll;
    qo_uint32_t  digits;

    // 8 digits or less?
    // fits into single 64-bit CPU register
    if (x <= 9999)
    {
        return qo_4udec_to_str2(x , buffer);
    }
    else if (x < 100000000)
    {
        low = x;

        // more than 6 digits?
        if (low > 999999)
        {
            digits = (low > 9999999) ? 8 : 7;
        }
        else{
            digits = (low > 99999) ? 6 : 5;
        }
    }
    else
    {
        qo_uint64_t  high = (((qo_uint64_t) x) * 0x55E63B89) >> 57;
        low = x - (high * 100000000);
        // h will be at most 42
        // calc num digits
        digits  = qo_2idec_to_str(high , buffer);
        digits += 8;
    }

    ll   = (low * 109951163) >> 40;
    low -= ll * 10000;
    low |= ll << 32;

    // Four divisions and remainders by 100
    ll   = ((low * 5243) >> 19) & 0x000000FF000000FF;
    low -= ll * 100;
    low  = (low << 16) | ll;

    // Eight divisions by 10 (14 bits needed)
    ll   = ((low * 103) >> 9) & 0x001E001E001E001E;
    low += ll * 3;

    // move digits into correct spot
    ll = ((low & 0x00F000F000F000F0) >> 4) | (low & 0x000F000F000F000F) << 8;
    ll = (ll >> 32) | (ll << 32);

    // convert from decimal digits to ASCII number digit range
    ll |= 0x3030303030303030;

    if (digits >= 8)
    {
        *(qo_uint64_t *) (buffer + digits - 8) = ll;
    }
    else
    {
        qo_uint32_t  d = digits;
        qo_cstring_t  s1  = buffer;
        qo_cstring_t  pll = (qo_cstring_t) &(((qo_cstring_t) &ll)[8 - digits]);

        if (d >= 4)
        {
            *(qo_uint32_t *) s1 = *(qo_uint32_t *) pll;
            s1  += 4;
            pll += 4;
            d -= 4;
        }
        if (d >= 2)
        {
            *(qo_uint16_t *) s1 = *(qo_uint16_t *) pll;
            s1  += 2;
            pll += 2;
            d -= 2;
        }
        if (d > 0)
        {
            *(qo_uint8_t *) s1 = *(qo_uint8_t *) pll;
        }
    }

    return digits;
}
extern inline
void
qo_udec17_to_fixed_str(
    qo_uint64_t   x ,
    qo_cstring_t  buffer
) {
    assert(x < 100000000000000000 (qo_uint64_t));
    const qo_uint64_t  magic_lo = (0x00FFFFFFFFFFFFFF / 10000) + 1;
    const qo_uint64_t  magic_hi = (0xFFFFFFFFFFFFFFFF / (100000000 >> 4)) + 1;

    // chunk it into four dwords (packed in two qwords)
    qo_uint64_t  hi = x / 100000000;
    qo_uint64_t  lo = x % 100000000;
    lo = ((lo * magic_lo) & 0x00FFFFFF00000000) + (((lo >> 4) * magic_hi) >>
        40) + 0x0000000100000001;
    hi = ((hi * magic_lo) & 0x00FFFFFF00000000) + (((hi >> 4) * magic_hi) >>
        40) + 0x0000000100000001;

    // uses SWAR to extact two digits at-a-time from our qwords
    // (multiplication by `10` is expected to be cheaper than a regular multiply)
    const qo_uint64_t  mask = 0x00FFFFFF00FFFFFF;
    qo_uint64_t  out0 = 0x3030303030303030;
    qo_uint64_t  out1 = 0x3030303030303030;
    lo = lo * 10;
    hi = hi * 10;
    out0 |= (lo & ~mask) >> 24;
    out1 |= (hi & ~mask) >> 24;
    lo = (lo & mask) * 10;
    hi = (hi & mask) * 10;
    out0 |= (lo & ~mask) >> 16;
    out1 |= (hi & ~mask) >> 16;
    lo = (lo & mask) * 10;
    hi = (hi & mask) * 10;
    out0 |= (lo & ~mask) >> 8;
    out1 |= (hi & ~mask) >> 8;
    lo = (lo & mask) * 10;
    hi = (hi & mask) * 10;
    out0 |= (lo & ~mask);
    out1 |= (hi & ~mask);

    memcpy(&buffer[0] , &out1 , 8);
    memcpy(&buffer[8] , &out0 , 8);
    buffer[16] = '\0';
}
extern inline
void
qo_udec16_to_fixed_str(
    qo_uint64_t   x ,
    qo_cstring_t  buffer
) {
    assert(x <= 10000000000000000);

    static const char  table[200] = {
        0x30 , 0x30 , 0x30 , 0x31 , 0x30 , 0x32 , 0x30 , 0x33 , 0x30 , 0x34 ,
        0x30 , 0x35 ,
        0x30 , 0x36 , 0x30 , 0x37 , 0x30 , 0x38 , 0x30 , 0x39 , 0x31 , 0x30 ,
        0x31 , 0x31 ,
        0x31 , 0x32 , 0x31 , 0x33 , 0x31 , 0x34 , 0x31 , 0x35 , 0x31 , 0x36 ,
        0x31 , 0x37 ,
        0x31 , 0x38 , 0x31 , 0x39 , 0x32 , 0x30 , 0x32 , 0x31 , 0x32 , 0x32 ,
        0x32 , 0x33 ,
        0x32 , 0x34 , 0x32 , 0x35 , 0x32 , 0x36 , 0x32 , 0x37 , 0x32 , 0x38 ,
        0x32 , 0x39 ,
        0x33 , 0x30 , 0x33 , 0x31 , 0x33 , 0x32 , 0x33 , 0x33 , 0x33 , 0x34 ,
        0x33 , 0x35 ,
        0x33 , 0x36 , 0x33 , 0x37 , 0x33 , 0x38 , 0x33 , 0x39 , 0x34 , 0x30 ,
        0x34 , 0x31 ,
        0x34 , 0x32 , 0x34 , 0x33 , 0x34 , 0x34 , 0x34 , 0x35 , 0x34 , 0x36 ,
        0x34 , 0x37 ,
        0x34 , 0x38 , 0x34 , 0x39 , 0x35 , 0x30 , 0x35 , 0x31 , 0x35 , 0x32 ,
        0x35 , 0x33 ,
        0x35 , 0x34 , 0x35 , 0x35 , 0x35 , 0x36 , 0x35 , 0x37 , 0x35 , 0x38 ,
        0x35 , 0x39 ,
        0x36 , 0x30 , 0x36 , 0x31 , 0x36 , 0x32 , 0x36 , 0x33 , 0x36 , 0x34 ,
        0x36 , 0x35 ,
        0x36 , 0x36 , 0x36 , 0x37 , 0x36 , 0x38 , 0x36 , 0x39 , 0x37 , 0x30 ,
        0x37 , 0x31 ,
        0x37 , 0x32 , 0x37 , 0x33 , 0x37 , 0x34 , 0x37 , 0x35 , 0x37 , 0x36 ,
        0x37 , 0x37 ,
        0x37 , 0x38 , 0x37 , 0x39 , 0x38 , 0x30 , 0x38 , 0x31 , 0x38 , 0x32 ,
        0x38 , 0x33 ,
        0x38 , 0x34 , 0x38 , 0x35 , 0x38 , 0x36 , 0x38 , 0x37 , 0x38 , 0x38 ,
        0x38 , 0x39 ,
        0x39 , 0x30 , 0x39 , 0x31 , 0x39 , 0x32 , 0x39 , 0x33 , 0x39 , 0x34 ,
        0x39 , 0x35 ,
        0x39 , 0x36 , 0x39 , 0x37 , 0x39 , 0x38 , 0x39 , 0x39 ,
    };
    qo_uint64_t  top = x / 100000000;
    qo_uint64_t  bottom = x % 100000000;
    //
    qo_uint64_t  toptop = top / 10000;
    qo_uint64_t  topbottom = top % 10000;
    qo_uint64_t  bottomtop = bottom / 10000;
    qo_uint64_t  bottombottom = bottom % 10000;
    //
    qo_uint64_t  toptoptop = toptop / 100;
    qo_uint64_t  toptopbottom = toptop % 100;

    qo_uint64_t  topbottomtop = topbottom / 100;
    qo_uint64_t  topbottombottom = topbottom % 100;

    qo_uint64_t  bottomtoptop = bottomtop / 100;
    qo_uint64_t  bottomtopbottom = bottomtop % 100;

    qo_uint64_t  bottombottomtop = bottombottom / 100;
    qo_uint64_t  bottombottombottom = bottombottom % 100;
    //
    memcpy(buffer , &table[2 * toptoptop] , 2);
    memcpy(buffer + 2 , &table[2 * toptopbottom] , 2);
    memcpy(buffer + 4 , &table[2 * topbottomtop] , 2);
    memcpy(buffer + 6 , &table[2 * topbottombottom] , 2);
    memcpy(buffer + 8 , &table[2 * bottomtoptop] , 2);
    memcpy(buffer + 10 , &table[2 * bottomtopbottom] , 2);
    memcpy(buffer + 12 , &table[2 * bottombottomtop] , 2);
    memcpy(buffer + 14 , &table[2 * bottombottombottom] , 2);
}
// mulx
extern inline
qo_uint64_t
__qo_lulz_mul128(
    qo_uint64_t * x ,
    qo_uint64_t   y
) {
    // 64:64 = 64*64
    __uint128_t  v = (__uint128_t) (*x) * y;
    *x = (qo_uint64_t) v;
    return v >> 64;
}
// lulz method
extern inline
void
qo_idec20_to_str(
    qo_int64_t    x ,
    qo_cstring_t  buffer
) {
    // split the qo_uint64_t into 4 streams (18446`74407`37095`51615)
    qo_uint64_t  top = x / 10000000000;
    qo_uint64_t  bottom = x % 10000000000;
    qo_uint64_t  x3 = top / 100000;
    qo_uint64_t  x2 = top % 100000;
    qo_uint64_t  x1 = bottom / 100000;
    qo_uint64_t  x0 = bottom % 100000;

    // divide each value by 10000 but keep the remainder
    const qo_uint64_t  M = ((qo_uint64_t) 0xFFFFFFFFFFFFFFFF / 10000) + 1;
    buffer[0]  = 0x30 + __qo_lulz_mul128(&x3 , M);
    buffer[5]  = 0x30 + __qo_lulz_mul128(&x2 , M);
    buffer[10] = 0x30 + __qo_lulz_mul128(&x1 , M);
    buffer[15] = 0x30 + __qo_lulz_mul128(&x0 , M);

    // get the rest of the digits by multiplying the remainder by 10
    for (int i = 1; i < 5; i++)
    {
        buffer[i + 0]  = 0x30 + __qo_lulz_mul128(&x3 , 10);
        buffer[i + 5]  = 0x30 + __qo_lulz_mul128(&x2 , 10);
        buffer[i + 10] = 0x30 + __qo_lulz_mul128(&x1 , 10);
        buffer[i + 15] = 0x30 + __qo_lulz_mul128(&x0 , 10);
    }
    buffer[20] = 0;
}
extern inline
void
__qo_hex64_to_untrimmed_str_common(
    qo_uint64_t    x ,
    qo_cstring_t   buffer ,
    qo_ccstring_t  hex_table
) {
    buffer[0]  = hex_table[(x >> 60) & 0xF];
    buffer[1]  = hex_table[(x >> 56) & 0xF];
    buffer[2]  = hex_table[(x >> 52) & 0xF];
    buffer[3]  = hex_table[(x >> 48) & 0xF];
    buffer[4]  = hex_table[(x >> 44) & 0xF];
    buffer[5]  = hex_table[(x >> 40) & 0xF];
    buffer[6]  = hex_table[(x >> 36) & 0xF];
    buffer[7]  = hex_table[(x >> 32) & 0xF];
    buffer[8]  = hex_table[(x >> 28) & 0xF];
    buffer[9]  = hex_table[(x >> 24) & 0xF];
    buffer[10] = hex_table[(x >> 20) & 0xF];
    buffer[11] = hex_table[(x >> 16) & 0xF];
    buffer[12] = hex_table[(x >> 12) & 0xF];
    buffer[13] = hex_table[(x >> 8) & 0xF];
    buffer[14] = hex_table[(x >> 4) & 0xF];
    buffer[15] = hex_table[(x) & 0xF];
    buffer[16] = '\0';
}
extern inline
void
qo_hex64_to_uppercase_untrimmed_str(
    qo_uint64_t   x ,
    qo_cstring_t  buffer
) {
    __qo_hex64_to_untrimmed_str_common(x , buffer , __QO_HEX_TABLE_UPPERCASE);
}
extern inline
void
qo_hex64_to_lowercase_untrimmed_str(
    qo_uint64_t   x ,
    qo_cstring_t  buffer
) {
    __qo_hex64_to_untrimmed_str_common(x , buffer , __QO_HEX_TABLE_LOWERCASE);
}
extern inline
qo_size_t
__qo_hex64_to_trimmed_str_common(
    qo_uint64_t    x ,
    qo_cstring_t   buffer ,
    qo_ccstring_t  hex_table
) {
    if (x)
    {
        qo_int32_t  clz = __builtin_clzll(x); // TODO: Replace with QO_CLZLL
        qo_int32_t  start_index = clz / 4;
        __qo_hex64_to_untrimmed_str_common(x , buffer , hex_table);
        qo_size_t   len = 16 - start_index;
        memmove(buffer , buffer + start_index , len);
        buffer[len] = '\0';
        return len;
    }

    buffer[0] = '0';
    buffer[1] = '\0';
    return 1;
}
extern inline
qo_size_t
qo_hex64_to_uppercase_str(
    qo_uint64_t   x ,
    qo_cstring_t  buffer
) {
    return __qo_hex64_to_trimmed_str_common(x , buffer ,
        __QO_HEX_TABLE_UPPERCASE);
}
extern inline
qo_size_t
qo_hex64_to_lowercase_str(
    qo_uint64_t   x ,
    qo_cstring_t  buffer
) {
    return __qo_hex64_to_trimmed_str_common(x , buffer ,
        __QO_HEX_TABLE_LOWERCASE);
}
extern inline
void
qo_oct64_to_untrimmed_str(
    qo_uint64_t   x ,
    qo_cstring_t  buffer
) {
    qo_ccstring_t  oct_table = __QO_OCT_TABLE;
    buffer[0]  = oct_table[(x >> 63) & 7];
    buffer[1]  = oct_table[(x >> 60) & 7];
    buffer[2]  = oct_table[(x >> 57) & 7];
    buffer[3]  = oct_table[(x >> 54) & 7];
    buffer[4]  = oct_table[(x >> 51) & 7];
    buffer[5]  = oct_table[(x >> 48) & 7];
    buffer[6]  = oct_table[(x >> 45) & 7];
    buffer[7]  = oct_table[(x >> 42) & 7];
    buffer[8]  = oct_table[(x >> 39) & 7];
    buffer[9]  = oct_table[(x >> 36) & 7];
    buffer[10] = oct_table[(x >> 33) & 7];
    buffer[11] = oct_table[(x >> 30) & 7];
    buffer[12] = oct_table[(x >> 27) & 7];
    buffer[13] = oct_table[(x >> 24) & 7];
    buffer[14] = oct_table[(x >> 21) & 7];
    buffer[15] = oct_table[(x >> 18) & 7];
    buffer[16] = oct_table[(x >> 15) & 7];
    buffer[17] = oct_table[(x >> 12) & 7];
    buffer[18] = oct_table[(x >> 9) & 7];
    buffer[19] = oct_table[(x >> 6) & 7];
    buffer[20] = oct_table[(x >> 3) & 7];
    buffer[21] = oct_table[(x) & 7];
    buffer[22] = '\0';
}
/*
 * Reference: https://johnnylee-sde.github.io/Fast-unsigned-integer-to-hex-string/
 * @remark We can do faster using 1KB table, but it is too large.
 */
extern inline
void
__qo_hex32_to_untrimmed_str_common(
    qo_uint32_t   x ,
    qo_cstring_t  buffer ,
    qo_bool_t     lowercase
) {
    qo_uint64_t  num = x;

    // 使用位操作将每个4位（nibble）分离到自己的字节中
    num = ((num & 0xFFFF) << 32) | ((num & 0xFFFF0000) >> 16);
    num = ((num & 0x0000FF000000FF00) >> 8) | (num & 0x000000FF000000FF) << 16;
    num = ((num & 0x00F000F000F000F0) >> 4) | (num & 0x000F000F000F000F) << 8;

    // 现在每个字节中都有一个独立的十六进制数字
    // 例如：0x1234FACE => 0x0E0C0A0F04030201

    // 创建一个字节掩码，包含字母的十六进制数字
    qo_uint64_t  mask = ((num + 0x0606060606060606) >> 4) & 0x0101010101010101;

    // 转换为ASCII数字字符
    num |= 0x3030303030303030;

    // 如果有高位的十六进制字符，需要进行调整
    num += ((lowercase) ? 0x27 : 0x07) * mask;

    // 将字符串复制到输出缓冲区
    *(qo_uint64_t *) buffer = num;
}
extern inline
void
qo_oct64_to_compact_str(
    qo_uint64_t   x ,
    qo_cstring_t  buffer
) {
    if (!x)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    qo_ccstring_t  oct_table = __QO_OCT_TABLE;
    int  highest_bit = 63 - __builtin_clzll(x); // TODO: Replace with QO_CLZLL
    int  start_index = highest_bit / 3;

    switch (start_index)
    {
        case 21:
            *buffer++ = oct_table[(x >> 63) & 7];      // 最高位

        case 20:
            *buffer++ = oct_table[(x >> 60) & 7];

        case 19:
            *buffer++ = oct_table[(x >> 57) & 7];

        case 18:
            *buffer++ = oct_table[(x >> 54) & 7];

        case 17:
            *buffer++ = oct_table[(x >> 51) & 7];

        case 16:
            *buffer++ = oct_table[(x >> 48) & 7];

        case 15:
            *buffer++ = oct_table[(x >> 45) & 7];

        case 14:
            *buffer++ = oct_table[(x >> 42) & 7];

        case 13:
            *buffer++ = oct_table[(x >> 39) & 7];

        case 12:
            *buffer++ = oct_table[(x >> 36) & 7];

        case 11:
            *buffer++ = oct_table[(x >> 33) & 7];

        case 10:
            *buffer++ = oct_table[(x >> 30) & 7];

        case  9:
            *buffer++ = oct_table[(x >> 27) & 7];

        case  8:
            *buffer++ = oct_table[(x >> 24) & 7];

        case  7:
            *buffer++ = oct_table[(x >> 21) & 7];

        case  6:
            *buffer++ = oct_table[(x >> 18) & 7];

        case  5:
            *buffer++ = oct_table[(x >> 15) & 7];

        case  4:
            *buffer++ = oct_table[(x >> 12) & 7];

        case  3:
            *buffer++ = oct_table[(x >> 9) & 7];

        case  2:
            *buffer++ = oct_table[(x >> 6) & 7];

        case  1:
            *buffer++ = oct_table[(x >> 3) & 7];

        case  0:
            *buffer++ = oct_table[(x >> 0) & 7];
    }
}
extern inline
qo_uint32_t
qo_8decstr_to_u32(
    qo_uint64_t  str_mem // Example: 0x3132333435363738 -> "87654321"
) {
    // SWAR
    const qo_uint64_t  mask = 0x000000FF000000FF;
    const qo_uint64_t  mul1 = 0x000F424000000064; // 100 + (1000000qo_uint64_t << 32)
    const qo_uint64_t  mul2 = 0x0000271000000001; // 1 + (10000qo_uint64_t << 32)
    str_mem -= 0x3030303030303030;
    str_mem  = (str_mem * 10) + (str_mem >> 8); // str_mem = (str_mem * 2561) >> 8;
    str_mem  = (((str_mem & mask) * mul1) + (((str_mem >>
        16) & mask) * mul2)) >> 32;
    return str_mem;
}
extern inline
qo_uint32_t
qo_decstr_to_u32(
    qo_ccstring_t  str
) {
    assert(isdigit(str[0]));

    qo_uint32_t  x = 0;
    for (unsigned char c = str[0] ; c - '0' <= 9 ; c = *(str++))
    {
        x = (x << 1) + (x << 3) + c - '0';
    }
    return x;
}
/*
 * extern inline
 * qo_uint64_t
 * qo_decstr_to_u64(
 *  qo_ccstring_t str
 * ) {
 *  assert(isdigit(str[0]));
 *
 *  qo_uint64_t  x = 0;
 *  for (char c = str[0] ; c >= '0' && c <= '9' ; c = *(str++))
 *  {
 *      x = (x << 1) + (x << 3) + c - '0';
 *  }
 *  return x;
 * }*/
extern inline
qo_uint32_t
parse_eight_digits_unrolled(
    const qo_uint8_t * chars
) {
    // Must swap endianess in BE
    qo_uint64_t  val;
    memcpy(&val , chars , sizeof(qo_uint64_t));
    val = (val & 0x0F0F0F0F0F0F0F0F) * 2561 >> 8;
    val = (val & 0x00FF00FF00FF00FF) * 6553601 >> 16;
    return (qo_uint32_t) ((val & 0x0000FFFF0000FFFF) * 42949672960001 >> 32);
}
extern inline
qo_bool_t
__qo_is_made_of_eight_digits_fast(
    const qo_uint8_t * chars
) {
    qo_uint64_t  val;
    memcpy(&val , chars , 8);
    // a branchy method might be faster:
    // return (( val & 0xF0F0F0F0F0F0F0F0 ) == 0x3030303030303030)
    //  && (( (val + 0x0606060606060606) & 0xF0F0F0F0F0F0F0F0 ) ==
    //  0x3030303030303030);
    return ((val & 0xF0F0F0F0F0F0F0F0) |
        (((val + 0x0606060606060606) & 0xF0F0F0F0F0F0F0F0) >> 4)) ==
           0x3333333333333333;
}
qo_uint64_t
parse_digits(
    qo_ccstring_t  str ,
    qo_size_t *    consumed
) {
    const qo_uint8_t * p = (const qo_uint8_t *) str;
    qo_uint64_t  i = 0;

    // 使用SIMD加速处理连续的8位数字
    while (__qo_is_made_of_eight_digits_fast(p))
    {
        i  = i * 100000000 + parse_eight_digits_unrolled((qo_ccstring_t) p);
        p += 8;
    }

    // 处理剩余的数字
    while (*p >= '0' && *p <= '9')
    {
        i = i * 10 + (*p - '0');
        p++;
    }

    if (consumed)
    {
        *consumed = p - (const qo_uint8_t *) str;
    }
    return i;
}
/*
 * Reference: https://johnnylee-sde.github.io/Fast-hex-number-string-to-int/
 */
extern inline
qo_uint32_t
qo_hexstr_to_u32(
    qo_ccstring_t  str
) {
    qo_uint64_t  n = (*(qo_uint64_t *) (str)) & 0x4F4F4F4F4F4F4F4Full;

    qo_uint64_t  alphahex = (qo_uint64_t) (n & 0x4040404040404040ull);
// qo_uint64_t  nine = (alphahex >> 6) * 9;
// qo_uint64_t n0 = alphahex == 0 ? n : nine + (n ^ alphahex);
    qo_uint64_t  n0 = alphahex == 0 ? n :
                      ((alphahex >> 6) * 9) + (n ^ alphahex);
// 0x1001 == 4097 == 256 * 16 + 1
    qo_uint64_t  n1 = n0 * 0x1001 >> 8;
// 0x1000001 == 16777217 == 65536 * 256 + 1
    qo_uint64_t  n2 = (n1 & 0x00FF00FF00FF00FFull) * 0x1000001 >> 16;
// 0x1000000000001 == 281474976710657 == 4294967296 * 65536 + 1
    qo_uint64_t  num = (n2 & 0x0000FFFF0000FFFFull) * 0x1000000000001 >> 32;
    return (qo_uint32_t) num;
}
extern inline
qo_int32_t
qo_decstr_to_i32(
    qo_ccstring_t  str
) {
    char  first = str[0];
    qo_int32_t  sign_mask = 0;

    if (first == '-')
    {
        sign_mask = 0x80000000;
        str++;
    }
    else if (first == '+')
    {
        str++;
    }

    return sign_mask | qo_decstr_to_u32(str);
}
extern inline
qo_int64_t
qo_decstr_to_i64(
    qo_ccstring_t  str
) {
    char  first = str[0];
    qo_int64_t  sign_mask = 0;

    if (first == '-')
    {
        sign_mask = 0x8000000000000000LL;
        str++;
    }
    else if (first == '+')
    {
        str++;
    }

    return sign_mask | qo_decstr_to_u64(str);
}
#if defined(__cplusplus)
}
#endif // __cplusplus

#if !defined(QO_NO_SIMD)
    # include "internal/simd/mixed/int_cvt.h"
#endif // !QO_NO_SIMD
