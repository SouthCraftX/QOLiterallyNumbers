#pragma once
#include <cstdint>
#define __QO_INT_CVT_H__

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

#define __QO_HEX_TABLE_UPPERCASE  "0123456789ABCDEF"
#define __QO_HEX_TABLE_LOWERCASE  "0123456789abcdef"
#define __QO_OCT_TABLE            __QO_HEX_TABLE_UPPERCASE // We only use front 8 bits for octal

const char  __qo_bin_to_str_table[16][4] = {
    {'0' , '0' , '0' , '0'} , {'0' , '0' , '0' , '1'} , {'0' , '0' , '1' , '0'} ,
    {'0' , '0' , '1' , '1'} ,
    {'0' , '1' , '0' , '0'} , {'0' , '1' , '0' , '1'} , {'0' , '1' , '1' , '0'} ,
    {'0' , '1' , '1' , '1'} ,
    {'1' , '0' , '0' , '0'} , {'1' , '0' , '0' , '1'} , {'1' , '0' , '1' , '0'} ,
    {'1' , '0' , '1' , '1'} ,
    {'1' , '1' , '0' , '0'} , {'1' , '1' , '0' , '1'} , {'1' , '1' , '1' , '0'} ,
    {'1' , '1' , '1' , '1'}
};
/// @brief Convert a 64 bit unsigned integer that is less than 10000 to a 5 byte string
/// @param x The integer to convert
/// @param buffer The buffer to store the string in. The size of the buffer must be at least 5
// bytes.
inline
void
qo_4udec_to_str(
    uint64_t  x ,
    char *    buffer
) {
    const uint64_t  mask = 0x0FFFFE1FFFF87FFF;
    uint64_t  y;
    // assert(x <= 9999);

    x *= 0x000418A051EC0CCD;
    y  = (x & mask) * 10;

    // extract...
    y &= ~mask;
    x  = (uint32_t) ((y << 9) + (y >> 21) + (y >> 52) + (x >> 60));
    x += 0x30303030;
    memcpy(buffer , &x , 5);
}
/// @brief Convert a 64 bit signed integer that is less than 100 to a 3 byte string
/// @param x The integer to convert
/// @param buffer The buffer to store the string in. The size of the buffer must be at least 3
// bytes.
/// @return The length of the string
inline
uint32_t
qo_2idec_to_str(
    int64_t  x ,
    char *   buffer
) {
    if (x <= 9)
    {
        *buffer = (char) (x | 0x30);
        return 1;
    }
    else if (x <= 99)
    {
        uint64_t  low = x;
        uint64_t  ll  = ((low * 103) >> 9) & 0x1E;
        low += ll * 3;
        ll   = ((low & 0xF0) >> 4) | ((low & 0x0F) << 8);
        *(uint16_t *) buffer = (uint16_t) (ll | 0x3030);
        return 2;
    }
    return 0;
}

inline
uint32_t
qo_3udec_to_str(
    uint64_t  x ,
    char *    buffer
) {
    uint64_t  low;
    uint64_t  ll;
    uint32_t  digits;

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

    uint8_t * p = (uint8_t *) &ll;
    if (digits == 4)
    {
        *(uint32_t *) buffer = *(uint32_t *) (&p[4]);
    }
    else {
        *(uint16_t *) buffer = *(uint16_t *) (&p[5]);
        *(((uint8_t *) buffer) + 2) = *(uint8_t *) (&p[7]);
    }

    return digits;
}
/// @brief Convert a 64 bit unsigned integer that less than 999999999 to a 8 byte string
/// @param x The integer to convert
/// @param buffer The buffer to store the string in. The size of the buffer must be at least 8
// bytes.
/// @return The length of the string
inline
uint32_t
qo_10udec_to_str(
    uint64_t  x ,
    char *    buffer
) {
    uint64_t  low;
    uint64_t  ll;
    uint32_t  digits;

    // 8 digits or less?
    // fits into single 64-bit CPU register
    if (x <= 9999)
    {
        return qo_2idec_to_str(x , buffer);
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
        uint64_t  high = (((uint64_t) x) * 0x55E63B89) >> 57;
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
        *(uint64_t *) (buffer + digits - 8) = ll;
    }
    else
    {
        uint32_t  d   = digits;
        char *    s1  = buffer;
        char *    pll = (char *) &(((char *) &ll)[8 - digits]);

        if (d >= 4)
        {
            *(uint32_t *) s1 = *(uint32_t *) pll;
            s1  += 4;
            pll += 4;
            d -= 4;
        }
        if (d >= 2)
        {
            *(uint16_t *) s1 = *(uint16_t *) pll;
            s1  += 2;
            pll += 2;
            d -= 2;
        }
        if (d > 0)
        {
            *(uint8_t *) s1 = *(uint8_t *) pll;
        }
    }

    return digits;
}
inline
void
qo_udec17_to_fixed_str(
    uint64_t  x ,
    char *    buffer
) {
    assert(x < 100000000000000000ULL);
    const uint64_t  magic_lo = (0x00FFFFFFFFFFFFFF / 10000) + 1;
    const uint64_t  magic_hi = (0xFFFFFFFFFFFFFFFF / (100000000 >> 4)) + 1;

    // chunk it into four dwords (packed in two qwords)
    uint64_t  hi = x / 100000000;
    uint64_t  lo = x % 100000000;
    lo = ((lo * magic_lo) & 0x00FFFFFF00000000) +
         (((lo >> 4) * magic_hi) >> 40) + 0x0000000100000001;
    hi = ((hi * magic_lo) & 0x00FFFFFF00000000) +
         (((hi >> 4) * magic_hi) >> 40) + 0x0000000100000001;

    // uses SWAR to extact two digits at-a-time from our qwords
    // (multiplication by `10` is expected to be cheaper than a regular multiply)
    const uint64_t  mask = 0x00FFFFFF00FFFFFF;
    uint64_t  out0 = 0x3030303030303030;
    uint64_t  out1 = 0x3030303030303030;
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
inline
void
qo_udec16_to_fixed_str(
    uint64_t  x ,
    char *    buffer
) {
    assert(x <= 10000000000000000);

    static const char  table[200] = {
        0x30 , 0x30 , 0x30 , 0x31 , 0x30 , 0x32 , 0x30 , 0x33 , 0x30 , 0x34 , 0x30 , 0x35 ,
        0x30 , 0x36 , 0x30 , 0x37 , 0x30 , 0x38 , 0x30 , 0x39 , 0x31 , 0x30 , 0x31 , 0x31 ,
        0x31 , 0x32 , 0x31 , 0x33 , 0x31 , 0x34 , 0x31 , 0x35 , 0x31 , 0x36 , 0x31 , 0x37 ,
        0x31 , 0x38 , 0x31 , 0x39 , 0x32 , 0x30 , 0x32 , 0x31 , 0x32 , 0x32 , 0x32 , 0x33 ,
        0x32 , 0x34 , 0x32 , 0x35 , 0x32 , 0x36 , 0x32 , 0x37 , 0x32 , 0x38 , 0x32 , 0x39 ,
        0x33 , 0x30 , 0x33 , 0x31 , 0x33 , 0x32 , 0x33 , 0x33 , 0x33 , 0x34 , 0x33 , 0x35 ,
        0x33 , 0x36 , 0x33 , 0x37 , 0x33 , 0x38 , 0x33 , 0x39 , 0x34 , 0x30 , 0x34 , 0x31 ,
        0x34 , 0x32 , 0x34 , 0x33 , 0x34 , 0x34 , 0x34 , 0x35 , 0x34 , 0x36 , 0x34 , 0x37 ,
        0x34 , 0x38 , 0x34 , 0x39 , 0x35 , 0x30 , 0x35 , 0x31 , 0x35 , 0x32 , 0x35 , 0x33 ,
        0x35 , 0x34 , 0x35 , 0x35 , 0x35 , 0x36 , 0x35 , 0x37 , 0x35 , 0x38 , 0x35 , 0x39 ,
        0x36 , 0x30 , 0x36 , 0x31 , 0x36 , 0x32 , 0x36 , 0x33 , 0x36 , 0x34 , 0x36 , 0x35 ,
        0x36 , 0x36 , 0x36 , 0x37 , 0x36 , 0x38 , 0x36 , 0x39 , 0x37 , 0x30 , 0x37 , 0x31 ,
        0x37 , 0x32 , 0x37 , 0x33 , 0x37 , 0x34 , 0x37 , 0x35 , 0x37 , 0x36 , 0x37 , 0x37 ,
        0x37 , 0x38 , 0x37 , 0x39 , 0x38 , 0x30 , 0x38 , 0x31 , 0x38 , 0x32 , 0x38 , 0x33 ,
        0x38 , 0x34 , 0x38 , 0x35 , 0x38 , 0x36 , 0x38 , 0x37 , 0x38 , 0x38 , 0x38 , 0x39 ,
        0x39 , 0x30 , 0x39 , 0x31 , 0x39 , 0x32 , 0x39 , 0x33 , 0x39 , 0x34 , 0x39 , 0x35 ,
        0x39 , 0x36 , 0x39 , 0x37 , 0x39 , 0x38 , 0x39 , 0x39 ,
    };
    uint64_t  top = x / 100000000;
    uint64_t  bottom = x % 100000000;
    //
    uint64_t  toptop = top / 10000;
    uint64_t  topbottom = top % 10000;
    uint64_t  bottomtop = bottom / 10000;
    uint64_t  bottombottom = bottom % 10000;
    //
    uint64_t  toptoptop = toptop / 100;
    uint64_t  toptopbottom = toptop % 100;

    uint64_t  topbottomtop = topbottom / 100;
    uint64_t  topbottombottom = topbottom % 100;

    uint64_t  bottomtoptop = bottomtop / 100;
    uint64_t  bottomtopbottom = bottomtop % 100;

    uint64_t  bottombottomtop = bottombottom / 100;
    uint64_t  bottombottombottom = bottombottom % 100;
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
static inline
uint64_t
__lulz_mul128(
    uint64_t * x ,
    uint64_t   y
) {
    // 64:64 = 64*64
    __uint128_t  v = (__uint128_t) (*x) * y;
    *x = (uint64_t) v;
    return v >> 64;
}
// lulz method
inline
void
qo_idec20_to_str(
    int64_t  x ,
    char *   buffer
) {
    // split the uint64_t into 4 streams (18446`74407`37095`51615)
    uint64_t  top = x / 10000000000;
    uint64_t  bottom = x % 10000000000;
    uint64_t  x3 = top / 100000;
    uint64_t  x2 = top % 100000;
    uint64_t  x1 = bottom / 100000;
    uint64_t  x0 = bottom % 100000;

    // divide each value by 10000 but keep the remainder
    const uint64_t  M = (0xFFFFFFFFFFFFFFFFULL / 10000) + 1;
    buffer[0]  = 0x30 + __lulz_mul128(&x3 , M);
    buffer[5]  = 0x30 + __lulz_mul128(&x2 , M);
    buffer[10] = 0x30 + __lulz_mul128(&x1 , M);
    buffer[15] = 0x30 + __lulz_mul128(&x0 , M);

    // get the rest of the digits by multiplying the remainder by 10
    for (int i = 1; i < 5; i++)
    {
        buffer[i + 0]  = 0x30 + __lulz_mul128(&x3 , 10);
        buffer[i + 5]  = 0x30 + __lulz_mul128(&x2 , 10);
        buffer[i + 10] = 0x30 + __lulz_mul128(&x1 , 10);
        buffer[i + 15] = 0x30 + __lulz_mul128(&x0 , 10);
    }
    buffer[20] = 0;
}
inline
void
__qo_hex64_to_fixed17_str_common(
    uint64_t     x ,
    char *       buffer ,
    const char * hex_table
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
inline
void
qo_hex64_to_uppercase_fixed16_str(
    uint64_t  x ,
    char *    buffer
) {
    __qo_hex64_to_fixed16_str_common(x , buffer , __QO_HEX_TABLE_UPPERCASE);
}
inline
void
qo_hex64_to_lowercase_fixed16_str(
    uint64_t  x ,
    char *    buffer
) {
    __qo_hex64_to_fixed16_str_common(x , buffer , __QO_HEX_TABLE_LOWERCASE);
}
// ugly, but fast (branchless)
inline
size_t
__qo_hex64_to_compact_str_common(
    uint64_t     x ,
    char *       buffer ,
    const char * hex_table
) {
    if (!x)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }

    int  start_index = __builtin_ctzll(x) >> 2; // TODO: Replace with QO_CTZ64
    const char * begin = buffer;

    switch (start_index)
    {
        case 0:
            *buffer++ = hex_table[(x >> 60) & 0xF];

        case 1:
            *buffer++ = hex_table[(x >> 56) & 0xF];

        case 2:
            *buffer++ = hex_table[(x >> 52) & 0xF];

        case 3:
            *buffer++ = hex_table[(x >> 48) & 0xF];

        case 4:
            *buffer++ = hex_table[(x >> 44) & 0xF];

        case 5:
            *buffer++ = hex_table[(x >> 40) & 0xF];

        case 6:
            *buffer++ = hex_table[(x >> 36) & 0xF];

        case 7:
            *buffer++ = hex_table[(x >> 32) & 0xF];

        case 8:
            *buffer++ = hex_table[(x >> 28) & 0xF];

        case 9:
            *buffer++ = hex_table[(x >> 24) & 0xF];

        case 10:
            *buffer++ = hex_table[(x >> 20) & 0xF];

        case 11:
            *buffer++ = hex_table[(x >> 16) & 0xF];

        case 12:
            *buffer++ = hex_table[(x >> 12) & 0xF];

        case 13:
            *buffer++ = hex_table[(x >> 8) & 0xF];

        case 14:
            *buffer++ = hex_table[(x >> 4) & 0xF];

        case 15:
            *buffer++ = hex_table[(x) & 0xF];
    }

    *buffer = '\0';
    return buffer - begin;
}
inline
size_t
qo_hex64_to_compact_uppercase_str(
    uint64_t  x ,
    char *    buffer
) {
    return __qo_hex64_to_compact_str_common(x , buffer , __QO_HEX_TABLE_UPPERCASE);
}
inline
size_t
qo_hex64_to_compact_lowercase_str(
    uint64_t  x ,
    char *    buffer
) {
    return __qo_hex64_to_compact_str_common(x , buffer , __QO_HEX_TABLE_LOWERCASE);
}
inline
void
qo_oct64_to_fixed22_str(
    uint64_t  x ,
    char *    buffer
) {
    const char * oct_table = __QO_OCT_TABLE;
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
inline
void
qo_oct64_to_compact_str(
    uint64_t  x ,
    char *    buffer
) {
    if (!x)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    const char * oct_table = __QO_OCT_TABLE;
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
inline
uint32_t
qo_8decstr_to_u32(
    uint64_t  str_mem // Example: 0x3132333435363738 -> "87654321"
) {
    // SWAR
    const uint64_t  mask = 0x000000FF000000FF;
    const uint64_t  mul1 = 0x000F424000000064; // 100 + (1000000ULL << 32)
    const uint64_t  mul2 = 0x0000271000000001; // 1 + (10000ULL << 32)
    str_mem -= 0x3030303030303030;
    str_mem  = (str_mem * 10) + (str_mem >> 8); // str_mem = (str_mem * 2561) >> 8;
    str_mem  = (((str_mem & mask) * mul1) + (((str_mem >> 16) & mask) * mul2)) >> 32;
    return str_mem;
}
inline
uint32_t
qo_decstr_to_u32(
    const char * str
) {
    assert(isdigit(str[0]));

    uint32_t  x = 0;
    for (unsigned char c = str[0] ; c - '0' <= 9 ; c = *(str++))
    {
        x = (x << 1) + (x << 3) + c - '0';
    }
    return x;
}
inline
uint64_t
qo_decstr_to_u64(
    const char * str
) {
    assert(isdigit(str[0]));

    uint64_t  x = 0;
    for (char c = str[0] ; c >= '0' && c <= '9' ; c = *(str++))
    {
        x = (x << 1) + (x << 3) + c - '0';
    }
    return x;
}
inline
uint32_t
qo_hexstr_to_u32(
    const char * str
) {
}
inline
int32_t
qo_decstr_to_i32(
    const char * str
) {
    char  first = str[0];
    int32_t  sign_mask = 0;

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
inline
int64_t
qo_decstr_to_i64(
    const char * str
) {
    char  first = str[0];
    int64_t  sign_mask = 0;

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
