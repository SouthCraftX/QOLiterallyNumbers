#pragma once
#include <cctype>
#include <cstdint>
#define __qo_INT_CVT_H__

#include <stdint.h>
#include <string.h>
#include <assert.h>

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

/// @brief Convert a 64 bit unsigned integer that is less than 10000 to a 5 byte string
/// @param x The integer to convert
/// @param buffer The buffer to store the string in. The size of the buffer must be at least 5 bytes.
inline
void
qo_4udec_to_str(
    uint64_t    x ,
    char*       buffer
) {
    const uint64_t mask = 0x0FFFFE1FFFF87FFF;
    uint64_t y;
    // assert(x <= 9999);

    x *= 0x000418A051EC0CCD;
    y = (x & mask) * 10;

    // extract...
    y &= ~mask;
    x = (uint32_t)((y << 9) + (y >> 21) + (y >> 52) + (x >> 60));
    x += 0x30303030;
    memcpy(buffer, &x, 5);
}

/// @brief Convert a 64 bit signed integer that is less than 100 to a 3 byte string
/// @param x The integer to convert
/// @param buffer The buffer to store the string in. The size of the buffer must be at least 3 bytes.
/// @return The length of the string
inline
uint32_t
qo_2idec_to_str(
    int64_t     x ,
    char*       buffer
) {
    if (x <= 9)
    {
        *buffer = (char)(x | 0x30);
        return 1;
    }
    else if (x <= 99)
    {
        uint64_t low = x;
        uint64_t ll = ((low * 103) >> 9) & 0x1E; low += ll * 3;
        ll = ((low & 0xF0) >> 4) | ((low & 0x0F) << 8);
        *(uint16_t *)buffer = (uint16_t)(ll | 0x3030);
        return 2;
    }
    return 0;
}

inline
uint32_t
qo_3udec_to_str(
    uint64_t    x ,
    char*       buffer
) {
    uint64_t low;
    uint64_t ll;
    uint32_t digits;

    if (x <= 99)
        return qo_2idec_to_str(x, buffer);

    low = x;
    digits = (low > 999) ? 4 : 3;

    // division and remainder by 100
    // Simply dividing by 100 instead of multiply-and-shift
    // is about 50% more expensive timewise on my box
    ll = ((low * 5243) >> 19) & 0xFF; low -= ll * 100;

    low = (low << 16) | ll;

    // Two divisions by 10 (14 bits needed)
    ll = ((low * 103) >> 9) & 0x1E001E; low += ll * 3;

    // move digits into correct spot
    ll = ((low & 0x00F000F0) << 28) | (low & 0x000F000F) << 40;

    // convert from decimal digits to ASCII number digit range
    ll |= 0x3030303000000000;

    uint8_t *p = (uint8_t *)&ll;
    if (digits == 4) {
        *(uint32_t *)buffer = *(uint32_t *)(&p[4]);
    } else {
        *(uint16_t *)buffer = *(uint16_t *)(&p[5]);
        *(((uint8_t *)buffer) + 2) = *(uint8_t *)(&p[7]);
    }

    return digits;
}

/// @brief Convert a 64 bit unsigned integer that less than 999999999 to a 8 byte string
/// @param x The integer to convert
/// @param buffer The buffer to store the string in. The size of the buffer must be at least 8 bytes.
/// @return The length of the string
inline
uint32_t
qo_10udec_to_str(
    uint64_t    x ,
    char*       buffer
) {
    uint64_t low;
    uint64_t ll;
    uint32_t digits;

    // 8 digits or less?
    // fits into single 64-bit CPU register
    if (x <= 9999)
    {
        return qo_2idec_to_str(x, buffer);
    }
    else if (x < 100000000)
    {
        low = x;

        // more than 6 digits?
        if (low > 999999) 
            digits = (low > 9999999) ? 8 : 7;
        else 
            digits = (low > 99999) ? 6 : 5;
    }
    else
    {
        uint64_t high = (((uint64_t)x) * 0x55E63B89) >> 57;
        low = x - (high * 100000000);
        // h will be at most 42
        // calc num digits
        digits = qo_2idec_to_str(high, buffer);
        digits += 8;
    }

    ll = (low * 109951163) >> 40; low -= ll * 10000; low |= ll << 32;

    // Four divisions and remainders by 100
    ll = ((low * 5243) >> 19) & 0x000000FF000000FF; low -= ll * 100; low = (low << 16) | ll;

    // Eight divisions by 10 (14 bits needed)
    ll = ((low * 103) >> 9) & 0x001E001E001E001E; low += ll * 3;

    // move digits into correct spot
    ll = ((low & 0x00F000F000F000F0) >> 4) | (low & 0x000F000F000F000F) << 8;
    ll = (ll >> 32) | (ll << 32);

    // convert from decimal digits to ASCII number digit range
    ll |= 0x3030303030303030;

    if (digits >= 8)
    {
        *(uint64_t *)(buffer + digits - 8) = ll;
    }
    else
    {
        auto d = digits;
        auto s1 = buffer;
        auto pll = (char *)&(((char *)&ll)[8 - digits]);

        if (d >= 4) {
            *(uint32_t *)s1 = *(uint32_t *)pll;
            s1 += 4; pll += 4; d -= 4;
        }
        if (d >= 2) {
            *(uint16_t *)s1 = *(uint16_t *)pll;
            s1 += 2; pll += 2; d -= 2;
        }
        if (d > 0) {
            *(uint8_t *)s1 = *(uint8_t *)pll;
        }
    }

    return digits;
}

inline
void
qo_udec17_to_str(
    uint64_t    x ,
    char*       buffer
) {
    assert(x < 100000000000000000ULL);
    const uint64_t magic_lo = (0x00FFFFFFFFFFFFFF / 10000) + 1;
    const uint64_t magic_hi = (0xFFFFFFFFFFFFFFFF / (100000000 >> 4)) + 1;

    // chunk it into four dwords (packed in two qwords)
    uint64_t hi = x / 100000000;
    uint64_t lo = x % 100000000;
    lo = ((lo * magic_lo) & 0x00FFFFFF00000000) +
                (((lo >> 4) * magic_hi) >> 40) + 0x0000000100000001;
    hi = ((hi * magic_lo) & 0x00FFFFFF00000000) +
                (((hi >> 4) * magic_hi) >> 40) + 0x0000000100000001;

    // uses SWAR to extact two digits at-a-time from our qwords
    // (multiplication by `10` is expected to be cheaper than a regular multiply)
    const uint64_t mask = 0x00FFFFFF00FFFFFF;
    uint64_t out0 = 0x3030303030303030;
    uint64_t out1 = 0x3030303030303030;
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

    memcpy(&buffer[0], &out1, 8);
    memcpy(&buffer[8], &out0, 8);
    buffer[16] = '\0';
}

// mulx
static inline
uint64_t __lulz_mul128(uint64_t* x, uint64_t y) {
    // 64:64 = 64*64
    __uint128_t v = (__uint128_t)(*x) * y;
    *x = (uint64_t)v;
    return v >> 64;
}

// lulz method
inline
void
qo_idec20_to_str(
    int64_t     x ,
    char*       buffer
) {
    // split the uint64_t into 4 streams (18446`74407`37095`51615)
    uint64_t top = x / 10000000000;
    uint64_t bottom = x % 10000000000;
    uint64_t x3 = top / 100000;
    uint64_t x2 = top % 100000;
    uint64_t x1 = bottom / 100000;
    uint64_t x0 = bottom % 100000;

    // divide each value by 10000 but keep the remainder
    const uint64_t M = (0xFFFFFFFFFFFFFFFFULL / 10000) + 1;
    buffer[0]  = 0x30 + __lulz_mul128(&x3, M);
    buffer[5]  = 0x30 + __lulz_mul128(&x2, M);
    buffer[10] = 0x30 + __lulz_mul128(&x1, M);
    buffer[15] = 0x30 + __lulz_mul128(&x0, M);

    // get the rest of the digits by multiplying the remainder by 10
    for (int i = 1; i < 5; i++) {
        buffer[i + 0]  = 0x30 + __lulz_mul128(&x3, 10);
        buffer[i + 5]  = 0x30 + __lulz_mul128(&x2, 10);
        buffer[i + 10] = 0x30 + __lulz_mul128(&x1, 10);
        buffer[i + 15] = 0x30 + __lulz_mul128(&x0, 10);
    }
    buffer[20] = 0;
}


inline
uint32_t
qo_str_to_u32(
    const char*  str
) {
    assert(isdigit(str[0]));

    uint32_t x = 0;
    for (char c = str[0] ; c >= '0' && c <= '9' ; c = *(str++)) 
        x = (x << 1) + (x << 3) + c - '0';
    return x;
}

inline
uint64_t
qo_str_to_u64(
    const char*  str
) {
    assert(isdigit(str[0]));

    uint64_t x = 0;
    for (char c = str[0] ; c >= '0' && c <= '9' ; c = *(str++))
        x = (x << 1) + (x << 3) + c - '0';
    return x;
}

inline 
int32_t
qo_str_to_i32(
    const char*  str
) {
    char first = str[0];
    int32_t sign_mask = 0;

    if (first == '-') 
    {
        sign_mask = 0x80000000;
        str++;
    }
    else if (first == '+') 
        str++;
    
    return sign_mask | qo_str_to_u32(str);
}

inline
int64_t
qo_str_to_i64(
    const char*  str
) {
    char first = str[0];
    int64_t sign_mask = 0;

    if (first == '-') 
    {
        sign_mask = 0x8000000000000000LL;
        str++;
    }
    else if (first == '+') 
        str++;
    
    return sign_mask | qo_str_to_u64(str);
}

#if defined(__cplusplus)
}
#endif // __cplusplus
