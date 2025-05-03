#pragma once
#define __QOBD_H__

#include <stdint.h>
#include <memory.h>
#include "../include/fp_cvt.h"

// Type Aliases based on policy
#if QOBD_DRBX_INT_TYPES_POLICY == QOBD_DRBX_INT_TYPES_MATCH
typedef qo_uint32_t    remainder_f32_t;
typedef qo_int32_t  dec_exp_f32_t;
typedef qo_int32_t  shift_f32_t;
typedef qo_uint64_t    remainder_f64_t;
typedef qo_int32_t  dec_exp_f64_t;    // Often fits in 32-bit
typedef qo_int32_t  shift_f64_t;
#elif QOBD_DRBX_INT_TYPES_POLICY == QOBD_DRBX_INT_TYPES_PREFER_32
typedef qo_uint32_t    remainder_f32_t;
typedef qo_int32_t  dec_exp_f32_t;
typedef qo_int32_t  shift_f32_t;
// Check if 64-bit remainder bound fits in qo_uint32_t (it doesn't)
typedef qo_uint64_t    remainder_f64_t;
typedef qo_int32_t  dec_exp_f64_t;
typedef qo_int32_t  shift_f64_t;
#elif QOBD_DRBX_INT_TYPES_POLICY == QOBD_DRBX_INT_TYPES_MINIMAL
// kappa = 1 for f32, max rem = 100. kappa = 2 for f64, max rem = 1000.
typedef qo_uint8_t   remainder_f32_t;
typedef qo_int8_t    dec_exp_f32_t;  // min_k=-31, max_k=46 => [-46, 31+1] => fits
typedef qo_int8_t    shift_f32_t;    // beta <= 32 usually
typedef qo_uint16_t  remainder_f64_t;
typedef qo_int16_t   dec_exp_f64_t;  // min_k=-292, max_k=326 => [-326, 292+2] => fits
typedef qo_int8_t    shift_f64_t;    // beta <= 64 usually, but qo_int8_t is requested
#else
 # error "Invalid QOBD_DRBX_INT_TYPES_POLICY"
#endif

typedef struct
{
    qo_uint16_t  total_bits;
    qo_uint16_t  significand_bits;
    qo_uint16_t  exponent_bits;
    qo_uint16_t  min_exponent;
    qo_uint16_t  max_exponent;
    qo_uint16_t  exponent_bias;
    qo_uint16_t  decimal_significand_digits; // Max expected digits for shortest round trip
    qo_uint16_t  decimal_exponent_digits;   // Max expected digits for exponent
} IEEE754Format;

static const IEEE754Format  ieee754_binary32 = {
    32 , 23 , 8 , -126 , 127 , -127 , 9 , 2
};

static const IEEE754Format  ieee754_binary64 = {
    64 , 52 , 11 , -1022 , 1023 , -1023 , 17 , 3
};

// Wrapper struct holding the significand bits plus the sign bit
typedef struct
{
    // Store the raw bit pattern containing sign and significand bits,
    // exponent bits zeroed out.
    qo_uint64_t           u;                   // Use qo_uint64_t to hold both f32 and f64 versions
    const IEEE754Format * format; // Pointer to format info
} SignedSignificandBits;

// Wrapper struct holding the full bit pattern of the float
typedef struct
{
    qo_uint64_t           u;                   // Use qo_uint64_t to hold both f32 and f64 versions
    const IEEE754Format * format; // Pointer to format info
} FloatBits;
// --- Functions for SignedSignificandBits ---
QO_FORCE_INLINE qo_bool_t
signed_significand_is_positive(
    SignedSignificandBits  s
) {
    return s.u < (UINT64_C(1) <<
                (s.format->significand_bits + s.format->exponent_bits));
}
QO_FORCE_INLINE qo_bool_t
signed_significand_is_negative(
    SignedSignificandBits  s
) {
    return !signed_significand_is_positive(s);
}
// Shift the obtained signed significand bits to the left by 1 to remove the sign bit.
QO_FORCE_INLINE qo_uint64_t
signed_significand_remove_sign_bit_and_shift(
    SignedSignificandBits  s
) {
    qo_uint64_t  total_bits_mask = (s.format->total_bits == 64) ?
                                   UINT64_C(0xFFFFFFFFFFFFFFFF) :
                                   (UINT64_C(1) << s.format->total_bits) - 1;
    qo_uint64_t  significand_plus_exponent_mask = (UINT64_C(1) <<
                (s.format->significand_bits + s.format->exponent_bits)) - 1;
    qo_uint64_t  shifted_pattern = (s.u << 1) & total_bits_mask; // Apply total bits mask after
    // shift
    return shifted_pattern & significand_plus_exponent_mask; // Isolate significand/exponent part
                                                             // (sign is gone)

    // Original C++ logic relied on implicit mask from carrier_uint size.
    // Let's re-evaluate the C++:
    // return carrier_uint((carrier_uint(u) << 1) &
    //                               ((((carrier_uint(1) << (Format::total_bits - 1)) - 1u) << 1) |
    // 1u));
    // Example binary32 (total_bits = 32):
    // Mask = ((((1 << 31) - 1) << 1) | 1) = ((0x7FFFFFFF << 1) | 1) = (0xFFFFFFFE | 1) = 0xFFFFFFFF
    // Example binary64 (total_bits = 64):
    // Mask = ((((1ULL << 63) - 1ULL) << 1) | 1ULL) = ((0x7FFFFFFFFFFFFFFF << 1) | 1) =
    // (0xFFFFFFFFFFFFFFFE | 1) = 0xFFFFFFFFFFFFFFFF
    // So the mask seems redundant if carrier_uint is the exact size.
    // Let's try a simpler version based on removing the exponent first.
    // qo_uint64_t exponent_mask = ((UINT64_C(1) << s.format->exponent_bits) - 1) <<
    // s.format->significand_bits;
    // qo_uint64_t sign_significand = s.u & ~exponent_mask;
    // return (sign_significand << 1) & ((UINT64_C(1) << s.format->total_bits) - 1); // Mask needed
    // if qo_uint64_t > total_bits
}
QO_FORCE_INLINE qo_bool_t
signed_significand_has_even_significand_bits(
    SignedSignificandBits  s
) {
    // Check the LSB of the original pattern
    return (s.u & 1) == 0;
}
// --- Functions for FloatBits ---
QO_FORCE_INLINE qo_int32_t
floatbits_extract_exponent_bits(
    FloatBits  b
) {
    return (qo_int32_t) ((b.u >>
        b.format->significand_bits) & ((UINT64_C(1) <<
                b.format->exponent_bits) - 1));
}
QO_FORCE_INLINE qo_uint64_t
floatbits_extract_significand_bits(
    FloatBits  b
) {
    return b.u & ((UINT64_C(1) << b.format->significand_bits) - 1);
}
QO_FORCE_INLINE SignedSignificandBits
floatbits_remove_exponent_bits(
    FloatBits  b
) {
    qo_uint64_t  exponent_mask = ((UINT64_C(1) <<
            b.format->exponent_bits) - 1) <<
                                b.format->significand_bits;
    SignedSignificandBits  s = { b.u & ~exponent_mask , b.format };
    return s;
}
QO_FORCE_INLINE qo_int32_t
floatbits_binary_exponent(
    FloatBits  b
) {
    qo_int32_t  exp_bits = floatbits_extract_exponent_bits(b);
    return exp_bits ==
           0 ? b.format->min_exponent : (exp_bits + b.format->exponent_bias);
}
QO_FORCE_INLINE qo_uint64_t
floatbits_binary_significand(
    FloatBits  b
) {
    qo_int32_t  exp_bits  = floatbits_extract_exponent_bits(b);
    qo_uint64_t  sig_bits = floatbits_extract_significand_bits(b);
    return exp_bits == 0 ? sig_bits : (sig_bits | (UINT64_C(1) <<
            b.format->significand_bits));
}
QO_FORCE_INLINE qo_bool_t
floatbits_is_finite(
    FloatBits  b
) {
    qo_int32_t  exp_bits = floatbits_extract_exponent_bits(b);
    return exp_bits != ((1 << b.format->exponent_bits) - 1);
}
QO_FORCE_INLINE qo_bool_t
floatbits_is_nonzero(
    FloatBits  b
) {
    qo_uint64_t  non_sign_mask = (UINT64_C(1) <<
                (b.format->significand_bits + b.format->exponent_bits)) - 1;
    return (b.u & non_sign_mask) != 0;
}
// --- Factory Functions ---
QO_FORCE_INLINE FloatBits
make_floatbits_float(
    float  x
) {
    qo_uint32_t  u32;
    memcpy(&u32 , &x , sizeof(u32));
    FloatBits    b = { (qo_uint64_t) u32 , &ieee754_binary32 };
    return b;
}
QO_FORCE_INLINE FloatBits
make_floatbits_double(
    double  x
) {
    qo_uint64_t  u64;
    memcpy(&u64 , &x , sizeof(u64));
    FloatBits    b = { u64 , &ieee754_binary64 };
    return b;
}
//------------------------------------------------------------------------------
// Wide Unsigned Integer Arithmetic (u128_t)
//------------------------------------------------------------------------------

typedef struct
{
    qo_uint64_t  high;
    qo_uint64_t  low;
} u128_t;
// Constant 128-bit value
QO_FORCE_INLINE u128_t
make_uint128(
    qo_uint64_t  high ,
    qo_uint64_t  low
) {
    u128_t  v = { high , low };
    return v;
}
// Add a 64-bit integer to a 128-bit integer
QO_FORCE_INLINE u128_t
add128_64(
    u128_t       a ,
    qo_uint64_t  b
) {
#if (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__) && \
    __has_builtin(__builtin_addcll) && !defined(__ibmxl__)
    qo_uint64_t  carry = 0;
    qo_uint64_t  low_res;
    qo_uint64_t  high_res;
    low_res  = (qo_uint64_t) a.low;
    high_res = (qo_uint64_t) a.high;
    low_res  = __builtin_addcll(low_res , b , 0 , &carry);
    high_res = __builtin_addcll(high_res , 0 , carry , &carry);
    a.low  = (qo_uint64_t) low_res;
    a.high = (qo_uint64_t) high_res;
#elif defined(_MSC_VER) && defined(_M_X64)
    unsigned char  carry = _addcarry_u64(0 , a.low , b , &a.low);
    _addcarry_u64(carry , a.high , 0 , &a.high);
#else // Generic fallback
    qo_uint64_t  low_sum = a.low + b;
    qo_uint64_t  carry = (low_sum < a.low) ? 1 : 0; // Check for carry out of low part
    a.low   = low_sum;
    a.high += carry;
#endif
    return a;
}
// Multiply two 64-bit unsigned integers to get a 128-bit result
QO_FORCE_INLINE u128_t
umul128(
    qo_uint64_t  x ,
    qo_uint64_t  y
) {
    u128_t  result;
#if defined(__SIZEOF_INT128__)
    unsigned __int128  p = (unsigned __int128) x * (unsigned __int128) y;
    result.high = (qo_uint64_t) (p >> 64);
    result.low  = (qo_uint64_t) p;
#elif defined(_MSC_VER) && defined(_M_X64)
    result.low = _umul128(x , y , &result.high);
#else // Generic fallback
    qo_uint32_t const  a = (qo_uint32_t) (x >> 32);
    qo_uint32_t const  b = (qo_uint32_t) x;
    qo_uint32_t const  c = (qo_uint32_t) (y >> 32);
    qo_uint32_t const  d = (qo_uint32_t) y;

    qo_uint64_t const  ac = (qo_uint64_t) a * c;
    qo_uint64_t const  bc = (qo_uint64_t) b * c;
    qo_uint64_t const  ad = (qo_uint64_t) a * d;
    qo_uint64_t const  bd = (qo_uint64_t) b * d;

    qo_uint64_t const  intermediate = (bd >>
        32) + (qo_uint32_t) ad + (qo_uint32_t) bc;

    result.high = ac + (intermediate >> 32) + (ad >> 32) + (bc >> 32);
    result.low  = (intermediate << 32) + (qo_uint32_t) bd;
#endif
    return result;
}
// Get high 64 bits of 64x64->128 multiplication
QO_FORCE_INLINE qo_uint64_t
umul128_upper64(
    qo_uint64_t  x ,
    qo_uint64_t  y
) {
#if defined(__SIZEOF_INT128__)
    unsigned __int128  p = (unsigned __int128) x * (unsigned __int128) y;
    return (qo_uint64_t) (p >> 64);
#elif defined(_MSC_VER) && defined(_M_X64)
    return __umulh(x , y);
#else // Generic fallback (same as in umul128)
    qo_uint32_t const  a = (qo_uint32_t) (x >> 32);
    qo_uint32_t const  b = (qo_uint32_t) x;
    qo_uint32_t const  c = (qo_uint32_t) (y >> 32);
    qo_uint32_t const  d = (qo_uint32_t) y;

    qo_uint64_t const  ac = (qo_uint64_t) a * c;
    qo_uint64_t const  bc = (qo_uint64_t) b * c;
    qo_uint64_t const  ad = (qo_uint64_t) a * d;
    qo_uint64_t const  bd = (qo_uint64_t) b * d;

    qo_uint64_t const  intermediate = (bd >>
        32) + (qo_uint32_t) ad + (qo_uint32_t) bc;

    return ac + (intermediate >> 32) + (ad >> 32) + (bc >> 32);
#endif
}
// Get upper 128 bits of 64x128->192 multiplication
QO_FORCE_INLINE u128_t
umul192_upper128(
    qo_uint64_t  x ,
    u128_t       y
) {
    u128_t  r = umul128(x , y.high);
    qo_uint64_t  upper_low = umul128_upper64(x , y.low);
    r = add128_64(r , upper_low);
    return r;
}
// Get lower 128 bits of 64x128->192 multiplication
QO_FORCE_INLINE u128_t
umul192_lower128(
    qo_uint64_t  x ,
    u128_t       y
) {
    qo_uint64_t  high_prod = x * y.high; // Lower 64 bits of high part multiplication
    u128_t  high_low_prod  = umul128(x , y.low);
    u128_t  result;
    result.low  = high_low_prod.low;
    result.high = high_prod + high_low_prod.high; // May overflow, but that's correct for lower 128
                                                  // bits
    return result;
}
// Get upper 64 bits of 32x64->96 multiplication
QO_FORCE_INLINE qo_uint64_t
umul96_upper64(
    qo_uint32_t  x ,
    qo_uint64_t  y
) {
#if defined(__SIZEOF_INT128__) || (defined(_MSC_VER) && defined(_M_X64))
    // Leverage existing 128-bit multiplication
    return umul128_upper64((qo_uint64_t) x << 32 , y);
#else
    qo_uint32_t const  yh = (qo_uint32_t) (y >> 32);
    qo_uint32_t const  yl = (qo_uint32_t) y;

    qo_uint64_t const  xyh = (qo_uint64_t) x * yh;
    qo_uint64_t const  xyl = (qo_uint64_t) x * yl;

    return xyh + (xyl >> 32);
#endif
}
// Get lower 64 bits of 32x64->96 multiplication
QO_FORCE_INLINE qo_uint64_t
umul96_lower64(
    qo_uint32_t  x ,
    qo_uint64_t  y
) {
    return (qo_uint64_t) x * y; // Lower 64 bits are correct
}
//------------------------------------------------------------------------------
// Logarithm Approximations
//------------------------------------------------------------------------------
// These directly translate the C++ constexpr functions into static extern inline C functions.
// The min/max exponent checks are now done via asserts at runtime if NDEBUG is not defined.
// floor(log10(2^e))
QO_FORCE_INLINE qo_int16_t
floor_log10_pow2(
    qo_int32_t  e
) {
    // Using formula from C++ version: floor(e * log10(2)) approx floor(e * 315653 / 2^20)
    // Max |e| for f64 is ~1075. Max product ~340e6, fits in qo_int32_t.
    // Output range for f64: [-323, 323], fits in qo_int16_t.
    assert(e >= -2620 && e <= 2620); // Range from C++ version
    return (qo_int16_t) (((qo_int64_t) e * INT32_C(315653)) >> 20);
}
// floor(log2(10^e))
QO_FORCE_INLINE qo_int16_t
floor_log2_pow10(
    qo_int32_t  e
) {
    // Using formula from C++ version: floor(e * log2(10)) approx floor(e * 1741647 / 2^19)
    // Max |e| for f64 is ~326. Max product ~568e6, fits in qo_int32_t.
    // Output range for f64: [-970, 1082], fits in qo_int16_t.
    assert(e >= -1233 && e <= 1233); // Range from C++ version
    return (qo_int16_t) (((qo_int64_t) e * INT32_C(1741647)) >> 19);
}
// floor(log10(2^e / (4/3))) = floor(log10(3 * 2^(e-2))))
QO_FORCE_INLINE qo_int16_t
floor_log10_pow2_minus_log10_4_over_3(
    qo_int32_t  e
) {
    // Using formula from C++ version: floor(e*log10(2) - log10(4/3)) approx floor((e*631305 -
    // 261663) / 2^21)
    assert(e >= -2985 && e <= 2936); // Range from C++ version
    return (qo_int16_t) (((qo_int64_t) e * INT32_C(631305) - INT32_C(261663)) >>
        21);
}
// floor(log5(2^e))
QO_FORCE_INLINE qo_int32_t
floor_log5_pow2(
    qo_int32_t  e
) {
    // Using formula from C++ version: floor(e*log5(2)) approx floor(e * 225799 / 2^19)
    assert(e >= -1831 && e <= 1831);
    return (qo_int32_t) (((qo_int64_t) e * INT32_C(225799)) >> 19);
}
// floor(log5(2^e / 3))
QO_FORCE_INLINE qo_int32_t
floor_log5_pow2_minus_log5_3(
    qo_int32_t  e
) {
    // Using formula from C++ version: floor(e*log5(2) - log5(3)) approx floor((e*451597 - 715764) /
    // 2^20)
    assert(e >= -3543 && e <= 2427);
    return (qo_int32_t) (((qo_int64_t) e * INT32_C(451597) - INT32_C(715764)) >>
        20);
}
//------------------------------------------------------------------------------
// Division/Divisibility Helpers
//------------------------------------------------------------------------------
// Computes pow(base, exp) for non-negative exp
QO_FORCE_INLINE qo_uint64_t
pow_u64(
    qo_uint64_t  base ,
    int          exp
) {
    assert(exp >= 0);
    qo_uint64_t  res = 1;
    qo_uint64_t  p = base;
    while (exp > 0)
    {
        if (exp & 1)
        {
            res *= p;
        }
        p *= p;
        exp >>= 1;
    }
    return res;
}
QO_FORCE_INLINE qo_uint32_t
pow_u32(
    qo_uint32_t  base ,
    int          exp
) {
    assert(exp >= 0);
    qo_uint32_t  res = 1;
    qo_uint32_t  p = base;
    while (exp > 0)
    {
        if (exp & 1)
        {
            res *= p;
        }
        p *= p;
        exp >>= 1;
    }
    return res;
}
// Divide by 10^N for small N and small inputs (using magic numbers)
// Returns floor(n / 10^N)
QO_FORCE_INLINE qo_uint8_t
small_division_by_pow10_u8(
    qo_uint8_t  n ,
    int         N
) {
    if (N == 1)
    {
        _Static_assert(UCHAR_DIGITS >= 8 , "Need 8-bit char");
        assert(n <= 100); // 10^(1+1)
        return (qo_uint8_t) (((qo_uint16_t) n * 103) >> 10); // Magic for u8/10
    }
    // Add more cases if needed, e.g., N=2
    assert(qo_false && "Unsupported N for small_division_by_pow10_u8");
    return 0;
}
QO_FORCE_INLINE qo_uint16_t
small_division_by_pow10_u16(
    qo_uint16_t  n ,
    int          N
) {
    if (N == 1)
    {
        assert(n <= 100); // 10^(1+1)
        return (qo_uint16_t) (((qo_uint32_t) n * 103) >> 10); // Magic for u16/10
    }
    else if (N == 2)
    {
        assert(n <= 1000); // 10^(2+1)
        return (qo_uint16_t) (((qo_uint32_t) n * 41) >> 12); // Magic for u16/100
    }
    // Add more cases if needed
    assert(qo_false && "Unsupported N for small_division_by_pow10_u16");
    return 0;
}
QO_FORCE_INLINE qo_uint32_t
small_division_by_pow10_u32(
    qo_uint32_t  n ,
    int          N
) {
    if (N == 1)
    {
        assert(n <= 100); // 10^(1+1)
        return (qo_uint32_t) (((qo_uint64_t) n * 6554) >> 16); // Magic for u32/10
    }
    else if (N == 2)
    {
        assert(n <= 1000); // 10^(2+1)
        return (qo_uint32_t) (((qo_uint64_t) n * 656) >> 16); // Magic for u32/100
    }
    // Add more cases if needed
    assert(qo_false && "Unsupported N for small_division_by_pow10_u32");
    return 0;
}
QO_FORCE_INLINE qo_uint64_t
small_division_by_pow10_u64(
    qo_uint64_t  n ,
    int          N
) {
    if (N == 1)
    {
        assert(n <= 100); // 10^(1+1)
        return (qo_uint64_t) (umul128_upper64(
            n , UINT64_C(
                14757395258967641293
            )                     /* 2^64/10
                                   */
        ));
        // Magic for u64/10
    }
    else if (N == 2)
    {
        assert(n <= 1000); // 10^(2+1)
        return (qo_uint64_t) (umul128_upper64(
            n , UINT64_C(
                11805916207174113035
            )                     /* 2^64/100
                                   */
        ));
        // Magic for u64/100
    }
    else if (N == 3)
    {
        assert(n <= 10000); // 10^(3+1)
        // Smallest magic number requires > 64 bits. Use standard division.
        return n / 1000;
    }
    // Fallback to standard division for larger N or unhandled types
    return n / pow_u64(10 , N);
}
// Check divisibility by 10^N and divide *in place* for small N
QO_FORCE_INLINE qo_bool_t
check_divisibility_and_divide_by_pow10_u8_inplace(
    qo_uint8_t * n_ptr ,
    int          N
) {
    qo_uint8_t  n = *n_ptr;
    if (N == 1)
    {
        assert(n <= 100);
        qo_uint16_t  prod = (qo_uint16_t) n * 103;
        qo_bool_t    result = ((prod & ((1 << 10) - 1)) < 103);
        *n_ptr = (qo_uint8_t) (prod >> 10);
        return result;
    }
    assert(
        qo_false &&
        "Unsupported N for check_divisibility_and_divide_by_pow10_u8_inplace"
    );
    return qo_false;
}
QO_FORCE_INLINE qo_bool_t
check_divisibility_and_divide_by_pow10_u16_inplace(
    qo_uint16_t * n_ptr ,
    int           N
) {
    qo_uint16_t  n = *n_ptr;
    if (N == 1)
    {
        assert(n <= 100);
        qo_uint32_t  prod = (qo_uint32_t) n * 103;
        qo_bool_t    result = ((prod & ((1 << 10) - 1)) < 103);
        *n_ptr = (qo_uint16_t) (prod >> 10);
        return result;
    }
    else if (N == 2)
    {
        assert(n <= 1000);
        qo_uint32_t  prod = (qo_uint32_t) n * 41;
        qo_bool_t    result = ((prod & ((1 << 12) - 1)) < 41);
        *n_ptr = (qo_uint16_t) (prod >> 12);
        return result;
    }
    assert(
        qo_false &&
        "Unsupported N for check_divisibility_and_divide_by_pow10_u16_inplace"
    );
    return qo_false;
}
QO_FORCE_INLINE qo_bool_t
check_divisibility_and_divide_by_pow10_u32_inplace(
    qo_uint32_t * n_ptr ,
    int           N
) {
    qo_uint32_t  n = *n_ptr;
    if (N == 1)
    {
        assert(n <= 100);
        qo_uint64_t  prod = (qo_uint64_t) n * 6554;
        qo_bool_t    result = ((prod & ((1ULL << 16) - 1)) < 6554);
        *n_ptr = (qo_uint32_t) (prod >> 16);
        return result;
    }
    else if (N == 2)
    {
        assert(n <= 1000);
        qo_uint64_t  prod = (qo_uint64_t) n * 656;
        qo_bool_t    result = ((prod & ((1ULL << 16) - 1)) < 656);
        *n_ptr = (qo_uint32_t) (prod >> 16);
        return result;
    }
    assert(
        qo_false &&
        "Unsupported N for check_divisibility_and_divide_by_pow10_u32_inplace"
    );
    return qo_false;
}
QO_FORCE_INLINE qo_bool_t
check_divisibility_and_divide_by_pow10_u64_inplace(
    qo_uint64_t * n_ptr ,
    int           N
) {
    qo_uint64_t  n = *n_ptr;
    qo_uint64_t  divisor = pow_u64(10 , N);
    qo_uint64_t  quot = n / divisor;
    qo_uint64_t  rem  = n % divisor;
    *n_ptr = quot;
    return rem == 0;
}
// Divide by 10^N for general case (might use intrinsics or standard division)
QO_FORCE_INLINE qo_uint32_t
divide_by_pow10_u32(
    qo_uint32_t  n ,
    int          N
) {
    if (N == 1)
    {
        // 使用 C++ 版本的方法: n * (magic / 2^32), 取高32位
        // 魔数 429496730 = ceil(2^32 * 0.1)
        qo_uint64_t  product = (qo_uint64_t) n * UINT32_C(429496730); // 标准 32x32 -> 64 乘法
        return (qo_uint32_t) (product >> 32); // 取高 32位
    }
    else if (N == 2)
    {
        // 你的实现: (qo_uint32_t)(umul96_upper64(n, UINT64_C(1374389535)) >> 5); Magic:
        // ceil(2^37 / 100)
        // C++ 实现: (qo_uint32_t)(wuint::umul64(n, UINT32_C(1374389535)) >> 37); Magic: ceil(2^37 /
        // 100)

        // C++ 的方法看起来更可靠
        // 魔数 1374389535 = ceil(2^37 / 100)
        qo_uint64_t  product = (qo_uint64_t) n * UINT32_C(1374389535); // 32x32 -> 64
        return (qo_uint32_t) (product >> 37); // 右移 37 位
    }
    //if (N == 1) {
    // Use magic number optimized for 32-bit division by 10
    // Precondition: n <= 1073741828 (2^30 + 4)
    //    return (qo_uint32_t)(umul96_upper64(n, UINT64_C(3435973837)) >> 0); // 2^35 /
    // 10 = 3435973836.8
    // C++ used: (qo_uint32_t)(wuint::umul64(n, UINT32_C(429496730)) >> 32); // Magic: ceil(2^62 /
    // 10)
    // } else if (N == 2) {
    //      return (qo_uint32_t)(umul96_upper64(n, UINT64_C(1374389535)) >> 5); // Magic:
    // ceil(2^37 / 100)
    // C++ used: (qo_uint32_t)(wuint::umul64(n, UINT32_C(1374389535)) >> 37);
    // }
    // Fallback
    return n / pow_u32(10 , N);
}
QO_FORCE_INLINE qo_uint64_t
divide_by_pow10_u64(
    qo_uint64_t  n ,
    int          N
) {
    if (N == 1)
    {
        // Use magic number optimized for 64-bit division by 10
        // Precondition: n <= 4611686018427387908 (approx 0.4 * 2^64)
        return umul128_upper64(n , UINT64_C(1844674407370955162)); // Magic: ceil(2^64
        // / 10)
    }
    else if (N == 2)
    {
        // Magic number requires > 64 bits, use standard division
        return n / 100;
    }
    else if (N == 3)
    {
        // Use magic number optimized for 64-bit division by 1000
        // Precondition: n <= 15534100272597517998 (approx 0.84 * 2^64)
        return umul128_upper64(n , UINT64_C(4722366482869645214)) >> 8; // Magic:
        // ceil(2^72 /
        // 1000)
    }
    // Fallback
    return n / pow_u64(10 , N);
}
//------------------------------------------------------------------------------
// Trailing Zero Removal Implementation
//------------------------------------------------------------------------------
// remove_t policy implementation for f32
/*
 * QO_FORCE_INLINE void remove_trailing_zeros_f32_remove(qo_uint32_t*
 * significand_ptr, dec_exp_f32_t* exponent_ptr) {
 *  qo_uint32_t significand = *significand_ptr;
 *  dec_exp_f32_t exponent = *exponent_ptr;
 *
 *  // Branchless version from C++ code
 *  qo_uint32_t r = (qo_uint32_t)(((qo_uint64_t)significand * UINT32_C(184254097)) >> 4); //
 * rotr<32>(..., 4)
 * equivalent
 *  qo_bool_t b = r < UINT32_C(429497);
 *  int s = (int)b;
 *  significand = b ? r : significand;
 *
 *  r = (qo_uint32_t)(((qo_uint64_t)significand * UINT32_C(42949673)) >> 2); // rotr<32>(..., 2)
 * equivalent
 *  b = r < UINT32_C(42949673);
 *  s = s * 2 + b;
 *  significand = b ? r : significand;
 *
 *  r = (qo_uint32_t)(((qo_uint64_t)significand * UINT32_C(1288490189)) >> 1); // rotr<32>(..., 1)
 * equivalent
 *  b = r < UINT32_C(429496730);
 *  s = s * 2 + b;
 *  significand = b ? r : significand;
 *
 * significand_ptr = significand;
 * exponent_ptr = exponent + s;
 * }
 */
// WITH THIS IMPLEMENTATION (identical to remove_compact):
QO_FORCE_INLINE void
remove_trailing_zeros_f32_remove(
    qo_uint32_t *   significand_ptr ,
    dec_exp_f32_t * exponent_ptr
) {
    qo_uint32_t  significand = *significand_ptr;
    dec_exp_f32_t  exponent  = *exponent_ptr;
    // Use the simple loop which is easier to verify
    while (significand > 0 && significand % 10 == 0)   // Added significand > 0 check for safety
    {
        significand /= 10;
        exponent += 1;
    }
    *significand_ptr = significand;
    *exponent_ptr = exponent;
}
// remove_t policy implementation for f64
QO_FORCE_INLINE void
remove_trailing_zeros_f64_remove(
    qo_uint64_t *   significand_ptr ,
    dec_exp_f64_t * exponent_ptr
) {
    qo_uint64_t  significand = *significand_ptr;
    dec_exp_f64_t  exponent  = *exponent_ptr;

    // Branchless version from C++ code
    u128_t  p1 = umul128(significand , UINT64_C(28999941890838049));
    qo_uint64_t  r = p1.high >> 8; // Approx rotr<64>(..., 8)
    qo_bool_t    b = r < UINT64_C(184467440738);
    int  s = (int) b;
    significand = b ? r : significand;

    u128_t  p2 = umul128(significand , UINT64_C(182622766329724561));
    r = p2.high >> 4; // Approx rotr<64>(..., 4)
    b = r < UINT64_C(1844674407370956);
    s = s * 2 + b;
    significand = b ? r : significand;

    u128_t  p3 = umul128(significand , UINT64_C(10330176681277348905));
    r = p3.high >> 2; // Approx rotr<64>(..., 2)
    b = r < UINT64_C(184467440737095517);
    s = s * 2 + b;
    significand = b ? r : significand;

    u128_t  p4 = umul128(significand , UINT64_C(14757395258967641293));
    r = p4.high >> 1; // Approx rotr<64>(..., 1)
    b = r < UINT64_C(1844674407370955162);
    s = s * 2 + b;
    significand = b ? r : significand;

    *significand_ptr = significand;
    *exponent_ptr = exponent + s;
}
// remove_compact_t policy implementation for f32
QO_FORCE_INLINE void
remove_trailing_zeros_f32_remove_compact(
    qo_uint32_t *   significand_ptr ,
    dec_exp_f32_t * exponent_ptr
) {
    qo_uint32_t  significand = *significand_ptr;
    dec_exp_f32_t  exponent  = *exponent_ptr;
    while (qo_true)
    {
        // Magic: ceil(2^31 / 10) = 214748365
        // qo_uint32_t r = (qo_uint32_t)(((qo_uint64_t)significand * UINT32_C(1288490189)) >> 32);
        // //
        // Approximate division by 10
        // Let's try the C++ version's magic number directly.
        qo_uint32_t  r = (qo_uint32_t) (umul96_upper64(significand ,
            UINT64_C(14757395258967641293)) >>
            0);                             // 2^64/10

        if (significand % 10 == 0)          // Check divisibility properly
        {
            significand /= 10;
            exponent += 1;
        }
        else {
            break;
        }
    }
    *significand_ptr = significand;
    *exponent_ptr = exponent;
}
// remove_compact_t policy implementation for f64
QO_FORCE_INLINE void
remove_trailing_zeros_f64_remove_compact(
    qo_uint64_t *   significand_ptr ,
    dec_exp_f64_t * exponent_ptr
) {
    qo_uint64_t  significand = *significand_ptr;
    dec_exp_f64_t  exponent  = *exponent_ptr;
    while (qo_true)
    {
        if (significand % 10 == 0)   // Check divisibility properly
        {
            significand /= 10;
            exponent += 1;
        }
        else {
            break;
        }
    }
    *significand_ptr = significand;
    *exponent_ptr = exponent;
}
//------------------------------------------------------------------------------
// Cache Data and Access
//------------------------------------------------------------------------------

// --- binary32 Cache ---
#define F32_MIN_K       (-31)
#define F32_MAX_K       (46)
#define F32_CACHE_SIZE  (F32_MAX_K - F32_MIN_K + 1)

static const qo_uint64_t  f32_cache[F32_CACHE_SIZE] = {
    UINT64_C(0x81ceb32c4b43fcf5) , UINT64_C(0xa2425ff75e14fc32) ,
    UINT64_C(0xcad2f7f5359a3b3f) , UINT64_C(0xfd87b5f28300ca0e) ,
    UINT64_C(0x9e74d1b791e07e49) , UINT64_C(0xc612062576589ddb) ,
    UINT64_C(0xf79687aed3eec552) , UINT64_C(0x9abe14cd44753b53) ,
    UINT64_C(0xc16d9a0095928a28) , UINT64_C(0xf1c90080baf72cb2) ,
    UINT64_C(0x971da05074da7bef) , UINT64_C(0xbce5086492111aeb) ,
    UINT64_C(0xec1e4a7db69561a6) , UINT64_C(0x9392ee8e921d5d08) ,
    UINT64_C(0xb877aa3236a4b44a) , UINT64_C(0xe69594bec44de15c) ,
    UINT64_C(0x901d7cf73ab0acda) , UINT64_C(0xb424dc35095cd810) ,
    UINT64_C(0xe12e13424bb40e14) , UINT64_C(0x8cbccc096f5088cc) ,
    UINT64_C(0xafebff0bcb24aaff) , UINT64_C(0xdbe6fecebdedd5bf) ,
    UINT64_C(0x89705f4136b4a598) , UINT64_C(0xabcc77118461cefd) ,
    UINT64_C(0xd6bf94d5e57a42bd) , UINT64_C(0x8637bd05af6c69b6) ,
    UINT64_C(0xa7c5ac471b478424) , UINT64_C(0xd1b71758e219652c) ,
    UINT64_C(0x83126e978d4fdf3c) , UINT64_C(0xa3d70a3d70a3d70b) ,
    UINT64_C(0xcccccccccccccccd) , UINT64_C(0x8000000000000000) ,
    UINT64_C(0xa000000000000000) , UINT64_C(0xc800000000000000) ,
    UINT64_C(0xfa00000000000000) , UINT64_C(0x9c40000000000000) ,
    UINT64_C(0xc350000000000000) , UINT64_C(0xf424000000000000) ,
    UINT64_C(0x9896800000000000) , UINT64_C(0xbebc200000000000) ,
    UINT64_C(0xee6b280000000000) , UINT64_C(0x9502f90000000000) ,
    UINT64_C(0xba43b74000000000) , UINT64_C(0xe8d4a51000000000) ,
    UINT64_C(0x9184e72a00000000) , UINT64_C(0xb5e620f480000000) ,
    UINT64_C(0xe35fa931a0000000) , UINT64_C(0x8e1bc9bf04000000) ,
    UINT64_C(0xb1a2bc2ec5000000) , UINT64_C(0xde0b6b3a76400000) ,
    UINT64_C(0x8ac7230489e80000) , UINT64_C(0xad78ebc5ac620000) ,
    UINT64_C(0xd8d726b7177a8000) , UINT64_C(0x878678326eac9000) ,
    UINT64_C(0xa968163f0a57b400) , UINT64_C(0xd3c21bcecceda100) ,
    UINT64_C(0x84595161401484a0) , UINT64_C(0xa56fa5b99019a5c8) ,
    UINT64_C(0xcecb8f27f4200f3a) , UINT64_C(0x813f3978f8940985) ,
    UINT64_C(0xa18f07d736b90be6) , UINT64_C(0xc9f2c9cd04674edf) ,
    UINT64_C(0xfc6f7c4045812297) , UINT64_C(0x9dc5ada82b70b59e) ,
    UINT64_C(0xc5371912364ce306) , UINT64_C(0xf684df56c3e01bc7) ,
    UINT64_C(0x9a130b963a6c115d) , UINT64_C(0xc097ce7bc90715b4) ,
    UINT64_C(0xf0bdc21abb48db21) , UINT64_C(0x96769950b50d88f5) ,
    UINT64_C(0xbc143fa4e250eb32) , UINT64_C(0xeb194f8e1ae525fe) ,
    UINT64_C(0x92efd1b8d0cf37bf) , UINT64_C(0xb7abc627050305ae) ,
    UINT64_C(0xe596b7b0c643c71a) , UINT64_C(0x8f7e32ce7bea5c70) ,
    UINT64_C(0xb35dbf821ae4f38c) , UINT64_C(0xe0352f62a19e306f)
};

#define F32_COMPRESSION_RATIO      13
#define F32_COMPRESSED_TABLE_SIZE  ((F32_CACHE_SIZE + F32_COMPRESSION_RATIO - \
    1) / F32_COMPRESSION_RATIO)
#define F32_POW5_TABLE_SIZE        ((F32_COMPRESSION_RATIO + 1) / 2)

static const qo_uint64_t  f32_compressed_cache[F32_COMPRESSED_TABLE_SIZE] =
{
    UINT64_C(0x81ceb32c4b43fcf5) , UINT64_C(0x9392ee8e921d5d08) ,
    UINT64_C(0x8637bd05af6c69b6) , UINT64_C(0xfa00000000000000) ,
    UINT64_C(0x9184e72a00000000) , UINT64_C(0x878678326eac9000) ,
    //UINT64_C(0x813f3978f8940985)
};
static const qo_uint16_t  f32_pow5_table[F32_POW5_TABLE_SIZE] = {
    1 , 5 , 25 , 125 , 625 , 3125 , 15625
};

// --- binary64 Cache ---
#define F64_MIN_K       (-292)
#define F64_MAX_K       (326)
#define F64_CACHE_SIZE  (F64_MAX_K - F64_MIN_K + 1)

static const u128_t  f64_cache[F64_CACHE_SIZE] = {
    {UINT64_C(0xff77b1fcbebcdc4f) , UINT64_C(0x25e8e89c13bb0f7b)} ,
    {UINT64_C(0x9faacf3df73609b1) , UINT64(0x77b191618c54e9ad)} ,
    {UINT64_C(0xc795830d75038c1d) , UINT64_C(0xd59df5b9ef6a2418)} ,
    {UINT64_C(0xf97ae3d0d2446f25) , UINT64(0x4b0573286b44ad1e)} ,
    {UINT64_C(0x9becce62836ac577) , UINT64_C(0x4ee367f9430aec33)} ,
    {UINT64_C(0xc2e801fb244576d5) , UINT64(0x229c41f793cda740)} ,
    {UINT64_C(0xf3a20279ed56d48a) , UINT64_C(0x6b43527578c11110)} ,
    {UINT64_C(0x9845418c345644d6) , UINT64(0x830a13896b78aaaa)} ,
    {UINT64_C(0xbe5691ef416bd60c) , UINT64_C(0x23cc986bc656d554)} ,
    {UINT64_C(0xedec366b11c6cb8f) , UINT64(0x2cbfbe86b7ec8aa9)} ,
    {UINT64_C(0x94b3a202eb1c3f39) , UINT64_C(0x7bf7d71432f3d6aa)} ,
    {UINT64_C(0xb9e08a83a5e34f07) , UINT64(0xdaf5ccd93fb0cc54)} ,
    {UINT64_C(0xe858ad248f5c22c9) , UINT64_C(0xd1b3400f8f9cff69)} ,
    {UINT64_C(0x91376c36d99995be) , UINT64(0x23100809b9c21fa2)} ,
    {UINT64_C(0xb58547448ffffb2d) , UINT64_C(0xabd40a0c2832a78b)} ,
    {UINT64_C(0xe2e69915b3fff9f9) , UINT64(0x16c90c8f323f516d)} ,
    {UINT64_C(0x8dd01fad907ffc3b) , UINT64_C(0xae3da7d97f6792e4)} ,
    {UINT64_C(0xb1442798f49ffb4a) , UINT64(0x99cd11cfdf41779d)} ,
    {UINT64_C(0xdd95317f31c7fa1d) , UINT64_C(0x40405643d711d584)} ,
    {UINT64_C(0x8a7d3eef7f1cfc52) , UINT64(0x482835ea666b2573)} ,
    {UINT64_C(0xad1c8eab5ee43b66) , UINT64_C(0xda3243650005eed0)} ,
    {UINT64_C(0xd863b256369d4a40) , UINT64(0x90bed43e40076a83)} ,
    {UINT64_C(0x873e4f75e2224e68) , UINT64_C(0x5a7744a6e804a292)} ,
    {UINT64_C(0xa90de3535aaae202) , UINT64(0x711515d0a205cb37)} ,
    {UINT64_C(0xd3515c2831559a83) , UINT64_C(0x0d5a5b44ca873e04)} ,
    {UINT64_C(0x8412d9991ed58091) , UINT64(0xe858790afe9486c3)} ,
    {UINT64_C(0xa5178fff668ae0b6) , UINT64_C(0x626e974dbe39a873)} ,
    {UINT64_C(0xce5d73ff402d98e3) , UINT64(0xfb0a3d212dc81290)} ,
    {UINT64_C(0x80fa687f881c7f8e) , UINT64_C(0x7ce66634bc9d0b9a)} ,
    {UINT64_C(0xa139029f6a239f72) , UINT64(0x1c1fffc1ebc44e81)} ,
    {UINT64_C(0xc987434744ac874e) , UINT64_C(0xa327ffb266b56221)} ,
    {UINT64_C(0xfbe9141915d7a922) , UINT64(0x4bf1ff9f0062baa9)} ,
    {UINT64_C(0x9d71ac8fada6c9b5) , UINT64_C(0x6f773fc3603db4aa)} ,
    {UINT64_C(0xc4ce17b399107c22) , UINT64(0xcb550fb4384d21d4)} ,
    {UINT64_C(0xf6019da07f549b2b) , UINT64_C(0x7e2a53a146606a49)} ,
    {UINT64_C(0x99c102844f94e0fb) , UINT64(0x2eda7444cbfc426e)} ,
    {UINT64_C(0xc0314325637a1939) , UINT64_C(0xfa911155fefb5309)} ,
    {UINT64_C(0xf03d93eebc589f88) , UINT64(0x793555ab7eba27cb)} ,
    {UINT64_C(0x96267c7535b763b5) , UINT64_C(0x4bc1558b2f3458df)} ,
    {UINT64_C(0xbbb01b9283253ca2) , UINT64(0x9eb1aaedfb016f17)} ,
    {UINT64_C(0xea9c227723ee8bcb) , UINT64_C(0x465e15a979c1cadd)} ,
    {UINT64_C(0x92a1958a7675175f) , UINT64(0x0bfacd89ec191eca)} ,
    {UINT64_C(0xb749faed14125d36) , UINT64_C(0xcef980ec671f667c)} ,
    {UINT64_C(0xe51c79a85916f484) , UINT64(0x82b7e12780e7401b)} ,
    {UINT64_C(0x8f31cc0937ae58d2) , UINT64_C(0xd1b2ecb8b0908811)} ,
    {UINT64_C(0xb2fe3f0b8599ef07) , UINT64(0x861fa7e6dcb4aa16)} ,
    {UINT64_C(0xdfbdcece67006ac9) , UINT64_C(0x67a791e093e1d49b)} ,
    {UINT64_C(0x8bd6a141006042bd) , UINT64(0xe0c8bb2c5c6d24e1)} ,
    {UINT64_C(0xaecc49914078536d) , UINT64_C(0x58fae9f773886e19)} ,
    {UINT64_C(0xda7f5bf590966848) , UINT64(0xaf39a475506a899f)} ,
    {UINT64_C(0x888f99797a5e012d) , UINT64_C(0x6d8406c952429604)} ,
    {UINT64_C(0xaab37fd7d8f58178) , UINT64(0xc8e5087ba6d33b84)} ,
    {UINT64_C(0xd5605fcdcf32e1d6) , UINT64_C(0xfb1e4a9a90880a65)} ,
    {UINT64_C(0x855c3be0a17fcd26) , UINT64(0x5cf2eea09a550680)} ,
    {UINT64_C(0xa6b34ad8c9dfc06f) , UINT64_C(0xf42faa48c0ea481f)} ,
    {UINT64_C(0xd0601d8efc57b08b) , UINT64(0xf13b94daf124da27)} ,
    {UINT64_C(0x823c12795db6ce57) , UINT64_C(0x76c53d08d6b70859)} ,
    {UINT64_C(0xa2cb1717b52481ed) , UINT64(0x54768c4b0c64ca6f)} ,
    {UINT64_C(0xcb7ddcdda26da268) , UINT64_C(0xa9942f5dcf7dfd0a)} ,
    {UINT64_C(0xfe5d54150b090b02) , UINT64(0xd3f93b35435d7c4d)} ,
    {UINT64_C(0x9efa548d26e5a6e1) , UINT64_C(0xc47bc5014a1a6db0)} ,
    {UINT64_C(0xc6b8e9b0709f109a) , UINT64(0x359ab6419ca1091c)} ,
    {UINT64_C(0xf867241c8cc6d4c0) , UINT64_C(0xc30163d203c94b63)} ,
    {UINT64_C(0x9b407691d7fc44f8) , UINT64(0x79e0de63425dcf1e)} ,
    {UINT64_C(0xc21094364dfb5636) , UINT64_C(0x985915fc12f542e5)} ,
    {UINT64_C(0xf294b943e17a2bc4) , UINT64(0x3e6f5b7b17b2939e)} ,
    {UINT64_C(0x979cf3ca6cec5b5a) , UINT64_C(0xa705992ceecf9c43)} ,
    {UINT64_C(0xbd8430bd08277231) , UINT64(0x50c6ff782a838354)} ,
    {UINT64_C(0xece53cec4a314ebd) , UINT64_C(0xa4f8bf5635246429)} ,
    {UINT64_C(0x940f4613ae5ed136) , UINT64(0x871b7795e136be9a)} ,
    {UINT64_C(0xb913179899f68584) , UINT64_C(0x28e2557b59846e40)} ,
    {UINT64_C(0xe757dd7ec07426e5) , UINT64(0x331aeada2fe589d0)} ,
    {UINT64_C(0x9096ea6f3848984f) , UINT64_C(0x3ff0d2c85def7622)} ,
    {UINT64_C(0xb4bca50b065abe63) , UINT64(0x0fed077a756b53aa)} ,
    {UINT64_C(0xe1ebce4dc7f16dfb) , UINT64_C(0xd3e8495912c62895)} ,
    {UINT64_C(0x8d3360f09cf6e4bd) , UINT64(0x64712dd7abbbd95d)} ,
    {UINT64_C(0xb080392cc4349dec) , UINT64_C(0xbd8d794d96aacfb4)} ,
    {UINT64_C(0xdca04777f541c567) , UINT64(0xecf0d7a0fc5583a1)} ,
    {UINT64_C(0x89e42caaf9491b60) , UINT64_C(0xf41686c49db57245)} ,
    {UINT64_C(0xac5d37d5b79b6239) , UINT64(0x311c2875c522ced6)} ,
    {UINT64_C(0xd77485cb25823ac7) , UINT64_C(0x7d633293366b828c)} ,
    {UINT64_C(0x86a8d39ef77164bc) , UINT64(0xae5dff9c02033198)} ,
    {UINT64_C(0xa8530886b54dbdeb) , UINT64_C(0xd9f57f830283fdfd)} ,
    {UINT64_C(0xd267caa862a12d66) , UINT64(0xd072df63c324fd7c)} ,
    {UINT64_C(0x8380dea93da4bc60) , UINT64_C(0x4247cb9e59f71e6e)} ,
    {UINT64_C(0xa46116538d0deb78) , UINT64(0x52d9be85f074e609)} ,
    {UINT64_C(0xcd795be870516656) , UINT64_C(0x67902e276c921f8c)} ,
    {UINT64_C(0x806bd9714632dff6) , UINT64(0x00ba1cd8a3db53b7)} ,
    {UINT64_C(0xa086cfcd97bf97f3) , UINT64_C(0x80e8a40eccd228a5)} ,
    {UINT64_C(0xc8a883c0fdaf7df0) , UINT64(0x6122cd128006b2ce)} ,
    {UINT64_C(0xfad2a4b13d1b5d6c) , UINT64_C(0x796b805720085f82)} ,
    {UINT64_C(0x9cc3a6eec6311a63) , UINT64(0xcbe3303674053bb1)} ,
    {UINT64_C(0xc3f490aa77bd60fc) , UINT64_C(0xbedbfc4411068a9d)} ,
    {UINT64_C(0xf4f1b4d515acb93b) , UINT64(0xee92fb5515482d45)} ,
    {UINT64_C(0x991711052d8bf3c5) , UINT64_C(0x751bdd152d4d1c4b)} ,
    {UINT64_C(0xbf5cd54678eef0b6) , UINT64(0xd262d45a78a0635e)} ,
    {UINT64_C(0xef340a98172aace4) , UINT64_C(0x86fb897116c87c35)} ,
    {UINT64_C(0x9580869f0e7aac0e) , UINT64(0xd45d35e6ae3d4da1)} ,
    {UINT64_C(0xbae0a846d2195712) , UINT64_C(0x8974836059cca10a)} ,
    {UINT64_C(0xe998d258869facd7) , UINT64(0x2bd1a438703fc94c)} ,
    {UINT64_C(0x91ff83775423cc06) , UINT64_C(0x7b6306a34627ddd0)} ,
    {UINT64_C(0xb67f6455292cbf08) , UINT64(0x1a3bc84c17b1d543)} ,
    {UINT64_C(0xe41f3d6a7377eeca) , UINT64_C(0x20caba5f1d9e4a94)} ,
    {UINT64_C(0x8e938662882af53e) , UINT64(0x547eb47b7282ee9d)} ,
    {UINT64_C(0xb23867fb2a35b28d) , UINT64_C(0xe99e619a4f23aa44)} ,
    {UINT64_C(0xdec681f9f4c31f31) , UINT64(0x6405fa00e2ec94d5)} ,
    {UINT64_C(0x8b3c113c38f9f37e) , UINT64_C(0xde83bc408dd3dd05)} ,
    {UINT64_C(0xae0b158b4738705e) , UINT64(0x9624ab50b148d446)} ,
    {UINT64_C(0xd98ddaee19068c76) , UINT64_C(0x3badd624dd9b0958)} ,
    {UINT64_C(0x87f8a8d4cfa417c9) , UINT64(0xe54ca5d70a80e5d7)} ,
    {UINT64_C(0xa9f6d30a038d1dbc) , UINT64_C(0x5e9fcf4ccd211f4d)} ,
    {UINT64_C(0xd47487cc8470652b) , UINT64(0x7647c32000696720)} ,
    {UINT64_C(0x84c8d4dfd2c63f3b) , UINT64_C(0x29ecd9f40041e074)} ,
    {UINT64_C(0xa5fb0a17c777cf09) , UINT64(0xf468107100525891)} ,
    {UINT64_C(0xcf79cc9db955c2cc) , UINT64_C(0x7182148d4066eeb5)} ,
    {UINT64_C(0x81ac1fe293d599bf) , UINT64(0xc6f14cd848405531)} ,
    {UINT64_C(0xa21727db38cb002f) , UINT64_C(0xb8ada00e5a506a7d)} ,
    {UINT64_C(0xca9cf1d206fdc03b) , UINT64(0xa6d90811f0e4851d)} ,
    {UINT64_C(0xfd442e4688bd304a) , UINT64_C(0x908f4a166d1da664)} ,
    {UINT64_C(0x9e4a9cec15763e2e) , UINT64(0x9a598e4e043287ff)} ,
    {UINT64_C(0xc5dd44271ad3cdba) , UINT64_C(0x40eff1e1853f29fe)} ,
    {UINT64_C(0xf7549530e188c128) , UINT64(0xd12bee59e68ef47d)} ,
    {UINT64_C(0x9a94dd3e8cf578b9) , UINT64_C(0x82bb74f8301958cf)} ,
    {UINT64_C(0xc13a148e3032d6e7) , UINT64(0xe36a52363c1faf02)} ,
    {UINT64_C(0xf18899b1bc3f8ca1) , UINT64_C(0xdc44e6c3cb279ac2)} ,
    {UINT64_C(0x96f5600f15a7b7e5) , UINT64(0x29ab103a5ef8c0ba)} ,
    {UINT64_C(0xbcb2b812db11a5de) , UINT64_C(0x7415d448f6b6f0e8)} ,
    {UINT64_C(0xebdf661791d60f56) , UINT64(0x111b495b3464ad22)} ,
    {UINT64_C(0x936b9fcebb25c995) , UINT64_C(0xcab10dd900beec35)} ,
    {UINT64_C(0xb84687c269ef3bfb) , UINT64(0x3d5d514f40eea743)} ,
    {UINT64_C(0xe65829b3046b0afa) , UINT64_C(0x0cb4a5a3112a5113)} ,
    {UINT64_C(0x8ff71a0fe2c2e6dc) , UINT64(0x47f0e785eaba72ac)} ,
    {UINT64_C(0xb3f4e093db73a093) , UINT64_C(0x59ed216765690f57)} ,
    {UINT64_C(0xe0f218b8d25088b8) , UINT64(0x306869c13ec3532d)} ,
    {UINT64_C(0x8c974f7383725573) , UINT64_C(0x1e414218c73a13fc)} ,
    {UINT64_C(0xafbd2350644eeacf) , UINT64(0xe5d1929ef90898fb)} ,
    {UINT64_C(0xdbac6c247d62a583) , UINT64_C(0xdf45f746b74abf3a)} ,
    {UINT64_C(0x894bc396ce5da772) , UINT64(0x6b8bba8c328eb784)} ,
    {UINT64_C(0xab9eb47c81f5114f) , UINT64_C(0x066ea92f3f326565)} ,
    {UINT64_C(0xd686619ba27255a2) , UINT64(0xc80a537b0efefebe)} ,
    {UINT64_C(0x8613fd0145877585) , UINT64_C(0xbd06742ce95f5f37)} ,
    {UINT64_C(0xa798fc4196e952e7) , UINT64(0x2c48113823b73705)} ,
    {UINT64_C(0xd17f3b51fca3a7a0) , UINT64_C(0xf75a15862ca504c6)} ,
    {UINT64_C(0x82ef85133de648c4) , UINT64(0x9a984d73dbe722fc)} ,
    {UINT64_C(0xa3ab66580d5fdaf5) , UINT64_C(0xc13e60d0d2e0ebbb)} ,
    {UINT64_C(0xcc963fee10b7d1b3) , UINT64(0x318df905079926a9)} ,
    {UINT64_C(0xffbbcfe994e5c61f) , UINT64_C(0xfdf17746497f7053)} ,
    {UINT64_C(0x9fd561f1fd0f9bd3) , UINT64(0xfeb6ea8bedefa634)} ,
    {UINT64_C(0xc7caba6e7c5382c8) , UINT64_C(0xfe64a52ee96b8fc1)} ,
    {UINT64_C(0xf9bd690a1b68637b) , UINT64(0x3dfdce7aa3c673b1)} ,
    {UINT64_C(0x9c1661a651213e2d) , UINT64_C(0x06bea10ca65c084f)} ,
    {UINT64_C(0xc31bfa0fe5698db8) , UINT64(0x486e494fcff30a63)} ,
    {UINT64_C(0xf3e2f893dec3f126) , UINT64_C(0x5a89dba3c3efccfb)} ,
    {UINT64_C(0x986ddb5c6b3a76b7) , UINT64(0xf89629465a75e01d)} ,
    {UINT64_C(0xbe89523386091465) , UINT64_C(0xf6bbb397f1135824)} ,
    {UINT64_C(0xee2ba6c0678b597f) , UINT64(0x746aa07ded582e2d)} ,
    {UINT64_C(0x94db483840b717ef) , UINT64_C(0xa8c2a44eb4571cdd)} ,
    {UINT64_C(0xba121a4650e4ddeb) , UINT64(0x92f34d62616ce414)} ,
    {UINT64_C(0xe896a0d7e51e1566) , UINT64_C(0x77b020baf9c81d18)} ,
    {UINT64_C(0x915e2486ef32cd60) , UINT64(0x0ace1474dc1d122f)} ,
    {UINT64_C(0xb5b5ada8aaff80b8) , UINT64_C(0x0d819992132456bb)} ,
    {UINT64_C(0xe3231912d5bf60e6) , UINT64(0x10e1fff697ed6c6a)} ,
    {UINT64_C(0x8df5efabc5979c8f) , UINT64_C(0xca8d3ffa1ef463c2)} ,
    {UINT64_C(0xb1736b96b6fd83b3) , UINT64(0xbd308ff8a6b17cb3)} ,
    {UINT64_C(0xddd0467c64bce4a0) , UINT64_C(0xac7cb3f6d05ddbdf)} ,
    {UINT64_C(0x8aa22c0dbef60ee4) , UINT64(0x6bcdf07a423aa96c)} ,
    {UINT64_C(0xad4ab7112eb3929d) , UINT64_C(0x86c16c98d2c953c7)} ,
    {UINT64_C(0xd89d64d57a607744) , UINT64(0xe871c7bf077ba8b8)} ,
    {UINT64_C(0x87625f056c7c4a8b) , UINT64_C(0x11471cd764ad4973)} ,
    {UINT64_C(0xa93af6c6c79b5d2d) , UINT64(0xd598e40d3dd89bd0)} ,
    {UINT64_C(0xd389b47879823479) , UINT64_C(0x4aff1d108d4ec2c4)} ,
    {UINT64_C(0x843610cb4bf160cb) , UINT64(0xcedf722a585139bb)} ,
    {UINT64_C(0xa54394fe1eedb8fe) , UINT64_C(0xc2974eb4ee658829)} ,
    {UINT64_C(0xce947a3da6a9273e) , UINT64(0x733d226229feea33)} ,
    {UINT64_C(0x811ccc668829b887) , UINT64_C(0x0806357d5a3f5260)} ,
    {UINT64_C(0xa163ff802a3426a8) , UINT64(0xca07c2dcb0cf26f8)} ,
    {UINT64_C(0xc9bcff6034c13052) , UINT64_C(0xfc89b393dd02f0b6)} ,
    {UINT64_C(0xfc2c3f3841f17c67) , UINT64(0xbbac2078d443ace3)} ,
    {UINT64_C(0x9d9ba7832936edc0) , UINT64_C(0xd54b944b84aa4c0e)} ,
    {UINT64_C(0xc5029163f384a931) , UINT64(0x0a9e795e65d4df12)} ,
    {UINT64_C(0xf64335bcf065d37d) , UINT64_C(0x4d4617b5ff4a16d6)} ,
    {UINT64_C(0x99ea0196163fa42e) , UINT64(0x504bced1bf8e4e46)} ,
    {UINT64_C(0xc06481fb9bcf8d39) , UINT64_C(0xe45ec2862f71e1d7)} ,
    {UINT64_C(0xf07da27a82c37088) , UINT64(0x5d767327bb4e5a4d)} ,
    {UINT64_C(0x964e858c91ba2655) , UINT64_C(0x3a6a07f8d510f870)} ,
    {UINT64_C(0xbbe226efb628afea) , UINT64(0x890489f70a55368c)} ,
    {UINT64_C(0xeadab0aba3b2dbe5) , UINT64_C(0x2b45ac74ccea842f)} ,
    {UINT64_C(0x92c8ae6b464fc96f) , UINT64(0x3b0b8bc90012929e)} ,
    {UINT64_C(0xb77ada0617e3bbcb) , UINT64_C(0x09ce6ebb40173745)} ,
    {UINT64_C(0xe55990879ddcaabd) , UINT64(0xcc420a6a101d0516)} ,
    {UINT64_C(0x8f57fa54c2a9eab6) , UINT64_C(0x9fa946824a12232e)} ,
    {UINT64_C(0xb32df8e9f3546564) , UINT64(0x47939822dc96abfa)} ,
    {UINT64_C(0xdff9772470297ebd) , UINT64_C(0x59787e2b93bc56f8)} ,
    {UINT64_C(0x8bfbea76c619ef36) , UINT64(0x57eb4edb3c55b65b)} ,
    {UINT64_C(0xaefae51477a06b03) , UINT64_C(0xede622920b6b23f2)} ,
    {UINT64_C(0xdab99e59958885c4) , UINT64(0xe95fab368e45ecee)} ,
    {UINT64_C(0x88b402f7fd75539b) , UINT64_C(0x11dbcb0218ebb415)} ,
    {UINT64_C(0xaae103b5fcd2a881) , UINT64(0xd652bdc29f26a11a)} ,
    {UINT64_C(0xd59944a37c0752a2) , UINT64_C(0x4be76d3346f04960)} ,
    {UINT64_C(0x857fcae62d8493a5) , UINT64(0x6f70a4400c562ddc)} ,
    {UINT64_C(0xa6dfbd9fb8e5b88e) , UINT64_C(0xcb4ccd500f6bb953)} ,
    {UINT64_C(0xd097ad07a71f26b2) , UINT64(0x7e2000a41346a7a8)} ,
    {UINT64_C(0x825ecc24c873782f) , UINT64_C(0x8ed400668c0c28c9)} ,
    {UINT64_C(0xa2f67f2dfa90563b) , UINT64(0x728900802f0f32fb)} ,
    {UINT64_C(0xcbb41ef979346bca) , UINT64_C(0x4f2b40a03ad2ffba)} ,
    {UINT64_C(0xfea126b7d78186bc) , UINT64(0xe2f610c84987bfa9)} ,
    {UINT64_C(0x9f24b832e6b0f436) , UINT64_C(0x0dd9ca7d2df4d7ca)} ,
    {UINT64_C(0xc6ede63fa05d3143) , UINT64(0x91503d1c79720dbc)} ,
    {UINT64_C(0xf8a95fcf88747d94) , UINT64_C(0x75a44c6397ce912b)} ,
    {UINT64_C(0x9b69dbe1b548ce7c) , UINT64(0xc986afbe3ee11abb)} ,
    {UINT64_C(0xc24452da229b021b) , UINT64_C(0xfbe85badce996169)} ,
    {UINT64_C(0xf2d56790ab41c2a2) , UINT64(0xfae27299423fb9c4)} ,
    {UINT64_C(0x97c560ba6b0919a5) , UINT64_C(0xdccd879fc967d41b)} ,
    {UINT64_C(0xbdb6b8e905cb600f) , UINT64(0x5400e987bbc1c921)} ,
    {UINT64_C(0xed246723473e3813) , UINT64_C(0x290123e9aab23b69)} ,
    {UINT64_C(0x9436c0760c86e30b) , UINT64(0xf9a0b6720aaf6522)} ,
    {UINT64_C(0xb94470938fa89bce) , UINT64_C(0xf808e40e8d5b3e6a)} ,
    {UINT64_C(0xe7958cb87392c2c2) , UINT64(0xb60b1d1230b20e05)} ,
    {UINT64_C(0x90bd77f3483bb9b9) , UINT64_C(0xb1c6f22b5e6f48c3)} ,
    {UINT64_C(0xb4ecd5f01a4aa828) , UINT64(0x1e38aeb6360b1af4)} ,
    {UINT64_C(0xe2280b6c20dd5232) , UINT64_C(0x25c6da63c38de1b1)} ,
    {UINT64_C(0x8d590723948a535f) , UINT64(0x579c487e5a38ad0f)} ,
    {UINT64_C(0xb0af48ec79ace837) , UINT64_C(0x2d835a9df0c6d852)} ,
    {UINT64_C(0xdcdb1b2798182244) , UINT64(0xf8e431456cf88e66)} ,
    {UINT64_C(0x8a08f0f8bf0f156b) , UINT64_C(0x1b8e9ecb641b5900)} ,
    {UINT64_C(0xac8b2d36eed2dac5) , UINT64(0xe272467e3d222f40)} ,
    {UINT64_C(0xd7adf884aa879177) , UINT64_C(0x5b0ed81dcc6abb10)} ,
    {UINT64_C(0x86ccbb52ea94baea) , UINT64(0x98e947129fc2b4ea)} ,
    {UINT64_C(0xa87fea27a539e9a5) , UINT64_C(0x3f2398d747b36225)} ,
    {UINT64_C(0xd29fe4b18e88640e) , UINT64(0x8eec7f0d19a03aae)} ,
    {UINT64_C(0x83a3eeeef9153e89) , UINT64_C(0x1953cf68300424ad)} ,
    {UINT64_C(0xa48ceaaab75a8e2b) , UINT64(0x5fa8c3423c052dd8)} ,
    {UINT64_C(0xcdb02555653131b6) , UINT64_C(0x3792f412cb06794e)} ,
    {UINT64_C(0x808e17555f3ebf11) , UINT64(0xe2bbd88bbee40bd1)} ,
    {UINT64_C(0xa0b19d2ab70e6ed6) , UINT64_C(0x5b6aceaeae9d0ec5)} ,
    {UINT64_C(0xc8de047564d20a8b) , UINT64(0xf245825a5a445276)} ,
    {UINT64_C(0xfb158592be068d2e) , UINT64_C(0xeed6e2f0f0d56713)} ,
    {UINT64_C(0x9ced737bb6c4183d) , UINT64(0x55464dd69685606c)} ,
    {UINT64_C(0xc428d05aa4751e4c) , UINT64_C(0xaa97e14c3c26b887)} ,
    {UINT64_C(0xf53304714d9265df) , UINT64(0xd53dd99f4b3066a9)} ,
    {UINT64_C(0x993fe2c6d07b7fab) , UINT64_C(0xe546a8038efe402a)} ,
    {UINT64_C(0xbf8fdb78849a5f96) , UINT64(0xde98520472bdd034)} ,
    {UINT64_C(0xef73d256a5c0f77c) , UINT64_C(0x963e66858f6d4441)} ,
    {UINT64_C(0x95a8637627989aad) , UINT64(0xdde7001379a44aa9)} ,
    {UINT64_C(0xbb127c53b17ec159) , UINT64_C(0x5560c018580d5d53)} ,
    {UINT64_C(0xe9d71b689dde71af) , UINT64(0xaab8f01e6e10b4a7)} ,
    {UINT64_C(0x9226712162ab070d) , UINT64_C(0xcab3961304ca70e9)} ,
    {UINT64_C(0xb6b00d69bb55c8d1) , UINT64(0x3d607b97c5fd0d23)} ,
    {UINT64_C(0xe45c10c42a2b3b05) , UINT64_C(0x8cb89a7db77c506b)} ,
    {UINT64_C(0x8eb98a7a9a5b04e3) , UINT64(0x77f3608e92adb243)} ,
    {UINT64_C(0xb267ed1940f1c61c) , UINT64_C(0x55f038b237591ed4)} ,
    {UINT64_C(0xdf01e85f912e37a3) , UINT64(0x6b6c46dec52f6689)} ,
    {UINT64_C(0x8b61313bbabce2c6) , UINT64_C(0x2323ac4b3b3da016)} ,
    {UINT64_C(0xae397d8aa96c1b77) , UINT64(0xabec975e0a0d081b)} ,
    {UINT64_C(0xd9c7dced53c72255) , UINT64_C(0x96e7bd358c904a22)} ,
    {UINT64_C(0x881cea14545c7575) , UINT64(0x7e50d64177da2e55)} ,
    {UINT64_C(0xaa242499697392d2) , UINT64_C(0xdde50bd1d5d0b9ea)} ,
    {UINT64_C(0xd4ad2dbfc3d07787) , UINT64(0x955e4ec64b44e865)} ,
    {UINT64_C(0x84ec3c97da624ab4) , UINT64_C(0xbd5af13bef0b113f)} ,
    {UINT64_C(0xa6274bbdd0fadd61) , UINT64(0xecb1ad8aeacdd58f)} ,
    {UINT64_C(0xcfb11ead453994ba) , UINT64_C(0x67de18eda5814af3)} ,
    {UINT64_C(0x81ceb32c4b43fcf4) , UINT64(0x80eacf948770ced8)} ,
    {UINT64_C(0xa2425ff75e14fc31) , UINT64_C(0xa1258379a94d028e)} ,
    {UINT64_C(0xcad2f7f5359a3b3e) , UINT64(0x096ee45813a04331)} ,
    {UINT64_C(0xfd87b5f28300ca0d) , UINT64_C(0x8bca9d6e188853fd)} ,
    {UINT64_C(0x9e74d1b791e07e48) , UINT64(0x775ea264cf55347e)} ,
    {UINT64_C(0xc612062576589dda) , UINT64_C(0x95364afe032a819e)} ,
    {UINT64_C(0xf79687aed3eec551) , UINT64(0x3a83ddbd83f52205)} ,
    {UINT64_C(0x9abe14cd44753b52) , UINT64_C(0xc4926a9672793543)} ,
    {UINT64_C(0xc16d9a0095928a27) , UINT64(0x75b7053c0f178294)} ,
    {UINT64_C(0xf1c90080baf72cb1) , UINT64_C(0x5324c68b12dd6339)} ,
    {UINT64_C(0x971da05074da7bee) , UINT64(0xd3f6fc16ebca5e04)} ,
    {UINT64_C(0xbce5086492111aea) , UINT64_C(0x88f4bb1ca6bcf585)} ,
    {UINT64_C(0xec1e4a7db69561a5) , UINT64(0x2b31e9e3d06c32e6)} ,
    {UINT64_C(0x9392ee8e921d5d07) , UINT64_C(0x3aff322e62439fd0)} ,
    {UINT64_C(0xb877aa3236a4b449) , UINT64(0x09befeb9fad487c3)} ,
    {UINT64_C(0xe69594bec44de15b) , UINT64_C(0x4c2ebe687989a9b4)} ,
    {UINT64_C(0x901d7cf73ab0acd9) , UINT64(0x0f9d37014bf60a11)} ,
    {UINT64_C(0xb424dc35095cd80f) , UINT64_C(0x538484c19ef38c95)} ,
    {UINT64_C(0xe12e13424bb40e13) , UINT64(0x2865a5f206b06fba)} ,
    {UINT64_C(0x8cbccc096f5088cb) , UINT64_C(0xf93f87b7442e45d4)} ,
    {UINT64_C(0xafebff0bcb24aafe) , UINT64(0xf78f69a51539d749)} ,
    {UINT64_C(0xdbe6fecebdedd5be) , UINT64_C(0xb573440e5a884d1c)} ,
    {UINT64_C(0x89705f4136b4a597) , UINT64(0x31680a88f8953031)} ,
    {UINT64_C(0xabcc77118461cefc) , UINT64_C(0xfdc20d2b36ba7c3e)} ,
    {UINT64_C(0xd6bf94d5e57a42bc) , UINT64(0x3d32907604691b4d)} ,
    {UINT64_C(0x8637bd05af6c69b5) , UINT64_C(0xa63f9a49c2c1b110)} ,
    {UINT64_C(0xa7c5ac471b478423) , UINT64(0x0fcf80dc33721d54)} ,
    {UINT64_C(0xd1b71758e219652b) , UINT64_C(0xd3c36113404ea4a9)} ,
    {UINT64_C(0x83126e978d4fdf3b) , UINT64(0x645a1cac083126ea)} ,
    {UINT64_C(0xa3d70a3d70a3d70a) , UINT64_C(0x3d70a3d70a3d70a4)} ,
    {UINT64_C(0xcccccccccccccccc) , UINT64(0xcccccccccccccccd)} ,
    {UINT64_C(0x8000000000000000) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0xa000000000000000) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0xc800000000000000) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0xfa00000000000000) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0x9c40000000000000) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0xc350000000000000) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0xf424000000000000) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0x9896800000000000) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0xbebc200000000000) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0xee6b280000000000) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0x9502f90000000000) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0xba43b74000000000) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0xe8d4a51000000000) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0x9184e72a00000000) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0xb5e620f480000000) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0xe35fa931a0000000) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0x8e1bc9bf04000000) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0xb1a2bc2ec5000000) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0xde0b6b3a76400000) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0x8ac7230489e80000) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0xad78ebc5ac620000) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0xd8d726b7177a8000) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0x878678326eac9000) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0xa968163f0a57b400) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0xd3c21bcecceda100) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0x84595161401484a0) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0xa56fa5b99019a5c8) , UINT64_C(0x0000000000000000)} ,
    {UINT64_C(0xcecb8f27f4200f3a) , UINT64(0x0000000000000000)} ,
    {UINT64_C(0x813f3978f8940984) , UINT64_C(0x4000000000000000)} ,
    {UINT64_C(0xa18f07d736b90be5) , UINT64(0x5000000000000000)} ,
    {UINT64_C(0xc9f2c9cd04674ede) , UINT64_C(0xa400000000000000)} ,
    {UINT64_C(0xfc6f7c4045812296) , UINT64(0x4d00000000000000)} ,
    {UINT64_C(0x9dc5ada82b70b59d) , UINT64_C(0xf020000000000000)} ,
    {UINT64_C(0xc5371912364ce305) , UINT64(0x6c28000000000000)} ,
    {UINT64_C(0xf684df56c3e01bc6) , UINT64_C(0xc732000000000000)} ,
    {UINT64_C(0x9a130b963a6c115c) , UINT64(0x3c7f400000000000)} ,
    {UINT64_C(0xc097ce7bc90715b3) , UINT64_C(0x4b9f100000000000)} ,
    {UINT64_C(0xf0bdc21abb48db20) , UINT64(0x1e86d40000000000)} ,
    {UINT64_C(0x96769950b50d88f4) , UINT64_C(0x1314448000000000)} ,
    {UINT64_C(0xbc143fa4e250eb31) , UINT64(0x17d955a000000000)} ,
    {UINT64_C(0xeb194f8e1ae525fd) , UINT64_C(0x5dcfab0800000000)} ,
    {UINT64_C(0x92efd1b8d0cf37be) , UINT64(0x5aa1cae500000000)} ,
    {UINT64_C(0xb7abc627050305ad) , UINT64_C(0xf14a3d9e40000000)} ,
    {UINT64_C(0xe596b7b0c643c719) , UINT64(0x6d9ccd05d0000000)} ,
    {UINT64_C(0x8f7e32ce7bea5c6f) , UINT64_C(0xe4820023a2000000)} ,
    {UINT64_C(0xb35dbf821ae4f38b) , UINT64(0xdda2802c8a800000)} ,
    {UINT64_C(0xe0352f62a19e306e) , UINT64_C(0xd50b2037ad200000)} ,
    {UINT64_C(0x8c213d9da502de45) , UINT64(0x4526f422cc340000)} ,
    {UINT64_C(0xaf298d050e4395d6) , UINT64_C(0x9670b12b7f410000)} ,
    {UINT64_C(0xdaf3f04651d47b4c) , UINT64(0x3c0cdd765f114000)} ,
    {UINT64_C(0x88d8762bf324cd0f) , UINT64_C(0xa5880a69fb6ac800)} ,
    {UINT64_C(0xab0e93b6efee0053) , UINT64(0x8eea0d047a457a00)} ,
    {UINT64_C(0xd5d238a4abe98068) , UINT64_C(0x72a4904598d6d880)} ,
    {UINT64_C(0x85a36366eb71f041) , UINT64(0x47a6da2b7f864750)} ,
    {UINT64_C(0xa70c3c40a64e6c51) , UINT64_C(0x999090b65f67d924)} ,
    {UINT64_C(0xd0cf4b50cfe20765) , UINT64(0xfff4b4e3f741cf6d)} ,
    {UINT64_C(0x82818f1281ed449f) , UINT64_C(0xbff8f10e7a8921a5)} ,
    {UINT64_C(0xa321f2d7226895c7) , UINT64(0xaff72d52192b6a0e)} ,
    {UINT64_C(0xcbea6f8ceb02bb39) , UINT64_C(0x9bf4f8a69f764491)} ,
    {UINT64_C(0xfee50b7025c36a08) , UINT64(0x02f236d04753d5b5)} ,
    {UINT64_C(0x9f4f2726179a2245) , UINT64_C(0x01d762422c946591)} ,
    {UINT64_C(0xc722f0ef9d80aad6) , UINT64(0x424d3ad2b7b97ef6)} ,
    {UINT64_C(0xf8ebad2b84e0d58b) , UINT64_C(0xd2e0898765a7deb3)} ,
    {UINT64_C(0x9b934c3b330c8577) , UINT64(0x63cc55f49f88eb30)} ,
    {UINT64_C(0xc2781f49ffcfa6d5) , UINT64_C(0x3cbf6b71c76b25fc)} ,
    {UINT64_C(0xf316271c7fc3908a) , UINT64(0x8bef464e3945ef7b)} ,
    {UINT64_C(0x97edd871cfda3a56) , UINT64_C(0x97758bf0e3cbb5ad)} ,
    {UINT64_C(0xbde94e8e43d0c8ec) , UINT64(0x3d52eeed1cbea318)} ,
    {UINT64_C(0xed63a231d4c4fb27) , UINT64_C(0x4ca7aaa863ee4bde)} ,
    {UINT64_C(0x945e455f24fb1cf8) , UINT64(0x8fe8caa93e74ef6b)} ,
    {UINT64_C(0xb975d6b6ee39e436) , UINT64_C(0xb3e2fd538e122b45)} ,
    {UINT64_C(0xe7d34c64a9c85d44) , UINT64(0x60dbbca87196b617)} ,
    {UINT64_C(0x90e40fbeea1d3a4a) , UINT64_C(0xbc8955e946fe31ce)} ,
    {UINT64_C(0xb51d13aea4a488dd) , UINT64(0x6babab6398bdbe42)} ,
    {UINT64_C(0xe264589a4dcdab14) , UINT64_C(0xc696963c7eed2dd2)} ,
    {UINT64_C(0x8d7eb76070a08aec) , UINT64(0xfc1e1de5cf543ca3)} ,
    {UINT64_C(0xb0de65388cc8ada8) , UINT64_C(0x3b25a55f43294bcc)} ,
    {UINT64_C(0xdd15fe86affad912) , UINT64(0x49ef0eb713f39ebf)} ,
    {UINT64_C(0x8a2dbf142dfcc7ab) , UINT64_C(0x6e3569326c784338)} ,
    {UINT64_C(0xacb92ed9397bf996) , UINT64(0x49c2c37f07965405)} ,
    {UINT64_C(0xd7e77a8f87daf7fb) , UINT64_C(0xdc33745ec97be907)} ,
    {UINT64_C(0x86f0ac99b4e8dafd) , UINT64(0x69a028bb3ded71a4)} ,
    {UINT64_C(0xa8acd7c0222311bc) , UINT64_C(0xc40832ea0d68ce0d)} ,
    {UINT64_C(0xd2d80db02aabd62b) , UINT64(0xf50a3fa490c30191)} ,
    {UINT64_C(0x83c7088e1aab65db) , UINT64_C(0x792667c6da79e0fb)} ,
    {UINT64_C(0xa4b8cab1a1563f52) , UINT64(0x577001b891185939)} ,
    {UINT64_C(0xcde6fd5e09abcf26) , UINT64_C(0xed4c0226b55e6f87)} ,
    {UINT64_C(0x80b05e5ac60b6178) , UINT64(0x544f8158315b05b5)} ,
    {UINT64_C(0xa0dc75f1778e39d6) , UINT64_C(0x696361ae3db1c722)} ,
    {UINT64_C(0xc913936dd571c84c) , UINT64(0x03bc3a19cd1e38ea)} ,
    {UINT64_C(0xfb5878494ace3a5f) , UINT64_C(0x04ab48a04065c724)} ,
    {UINT64_C(0x9d174b2dcec0e47b) , UINT64(0x62eb0d64283f9c77)} ,
    {UINT64_C(0xc45d1df942711d9a) , UINT64_C(0x3ba5d0bd324f8395)} ,
    {UINT64_C(0xf5746577930d6500) , UINT64(0xca8f44ec7ee3647a)} ,
    {UINT64_C(0x9968bf6abbe85f20) , UINT64_C(0x7e998b13cf4e1ecc)} ,
    {UINT64_C(0xbfc2ef456ae276e8) , UINT64(0x9e3fedd8c321a67f)} ,
    {UINT64_C(0xefb3ab16c59b14a2) , UINT64_C(0xc5cfe94ef3ea101f)} ,
    {UINT64_C(0x95d04aee3b80ece5) , UINT64(0xbba1f1d158724a13)} ,
    {UINT64_C(0xbb445da9ca61281f) , UINT64_C(0x2a8a6e45ae8edc98)} ,
    {UINT64_C(0xea1575143cf97226) , UINT64(0xf52d09d71a3293be)} ,
    {UINT64_C(0x924d692ca61be758) , UINT64_C(0x593c2626705f9c57)} ,
    {UINT64_C(0xb6e0c377cfa2e12e) , UINT64(0x6f8b2fb00c77836d)} ,
    {UINT64_C(0xe498f455c38b997a) , UINT64_C(0x0b6dfb9c0f956448)} ,
    {UINT64_C(0x8edf98b59a373fec) , UINT64(0x4724bd4189bd5ead)} ,
    {UINT64_C(0xb2977ee300c50fe7) , UINT64_C(0x58edec91ec2cb658)} ,
    {UINT64_C(0xdf3d5e9bc0f653e1) , UINT64(0x2f2967b66737e3ee)} ,
    {UINT64_C(0x8b865b215899f46c) , UINT64_C(0xbd79e0d20082ee75)} ,
    {UINT64_C(0xae67f1e9aec07187) , UINT64(0xecd8590680a3aa12)} ,
    {UINT64_C(0xda01ee641a708de9) , UINT64_C(0xe80e6f4820cc9496)} ,
    {UINT64_C(0x884134fe908658b2) , UINT64(0x3109058d147fdcde)} ,
    {UINT64_C(0xaa51823e34a7eede) , UINT64_C(0xbd4b46f0599fd416)} ,
    {UINT64_C(0xd4e5e2cdc1d1ea96) , UINT64(0x6c9e18ac7007c91b)} ,
    {UINT64_C(0x850fadc09923329e) , UINT64_C(0x03e2cf6bc604ddb1)} ,
    {UINT64_C(0xa6539930bf6bff45) , UINT64(0x84db8346b786151d)} ,
    {UINT64_C(0xcfe87f7cef46ff16) , UINT64_C(0xe612641865679a64)} ,
    {UINT64_C(0x81f14fae158c5f6e) , UINT64(0x4fcb7e8f3f60c07f)} ,
    {UINT64_C(0xa26da3999aef7749) , UINT64_C(0xe3be5e330f38f09e)} ,
    {UINT64_C(0xcb090c8001ab551c) , UINT64(0x5cadf5bfd3072cc6)} ,
    {UINT64_C(0xfdcb4fa002162a63) , UINT64_C(0x73d9732fc7c8f7f7)} ,
    {UINT64_C(0x9e9f11c4014dda7e) , UINT64(0x2867e7fddcdd9afb)} ,
    {UINT64_C(0xc646d63501a1511d) , UINT64_C(0xb281e1fd541501b9)} ,
    {UINT64_C(0xf7d88bc24209a565) , UINT64(0x1f225a7ca91a4227)} ,
    {UINT64_C(0x9ae757596946075f) , UINT64_C(0x3375788de9b06959)} ,
    {UINT64_C(0xc1a12d2fc3978937) , UINT64(0x0052d6b1641c83af)} ,
    {UINT64_C(0xf209787bb47d6b84) , UINT64_C(0xc0678c5dbd23a49b)} ,
    {UINT64_C(0x9745eb4d50ce6332) , UINT64(0xf840b7ba963646e1)} ,
    {UINT64_C(0xbd176620a501fbff) , UINT64_C(0xb650e5a93bc3d899)} ,
    {UINT64_C(0xec5d3fa8ce427aff) , UINT64(0xa3e51f138ab4cebf)} ,
    {UINT64_C(0x93ba47c980e98cdf) , UINT64_C(0xc66f336c36b10138)} ,
    {UINT64_C(0xb8a8d9bbe123f017) , UINT64(0xb80b0047445d4185)} ,
    {UINT64_C(0xe6d3102ad96cec1d) , UINT64_C(0xa60dc059157491e6)} ,
    {UINT64_C(0x9043ea1ac7e41392) , UINT64(0x87c89837ad68db30)} ,
    {UINT64_C(0xb454e4a179dd1877) , UINT64_C(0x29babe4598c311fc)} ,
    {UINT64_C(0xe16a1dc9d8545e94) , UINT64(0xf4296dd6fef3d67b)} ,
    {UINT64_C(0x8ce2529e2734bb1d) , UINT64_C(0x1899e4a65f58660d)} ,
    {UINT64_C(0xb01ae745b101e9e4) , UINT64(0x5ec05dcff72e7f90)} ,
    {UINT64_C(0xdc21a1171d42645d) , UINT64_C(0x76707543f4fa1f74)} ,
    {UINT64_C(0x899504ae72497eba) , UINT64(0x6a06494a791c53a9)} ,
    {UINT64_C(0xabfa45da0edbde69) , UINT64_C(0x0487db9d17636893)} ,
    {UINT64_C(0xd6f8d7509292d603) , UINT64(0x45a9d2845d3c42b7)} ,
    {UINT64_C(0x865b86925b9bc5c2) , UINT64_C(0x0b8a2392ba45a9b3)} ,
    {UINT64_C(0xa7f26836f282b732) , UINT64(0x8e6cac7768d7141f)} ,
    {UINT64_C(0xd1ef0244af2364ff) , UINT64_C(0x3207d795430cd927)} ,
    {UINT64_C(0x8335616aed761f1f) , UINT64(0x7f44e6bd49e807b9)} ,
    {UINT64_C(0xa402b9c5a8d3a6e7) , UINT64_C(0x5f16206c9c6209a7)} ,
    {UINT64_C(0xcd036837130890a1) , UINT64(0x36dba887c37a8c10)} ,
    {UINT64_C(0x802221226be55a64) , UINT64_C(0xc2494954da2c978a)} ,
    {UINT64_C(0xa02aa96b06deb0fd) , UINT64(0xf2db9baa10b7bd6d)} ,
    {UINT64_C(0xc83553c5c8965d3d) , UINT64_C(0x6f92829494e5acc8)} ,
    {UINT64_C(0xfa42a8b73abbf48c) , UINT64(0xcb772339ba1f17fa)} ,
    {UINT64_C(0x9c69a97284b578d7) , UINT64_C(0xff2a760414536efc)} ,
    {UINT64_C(0xc38413cf25e2d70d) , UINT64(0xfef5138519684abb)} ,
    {UINT64_C(0xf46518c2ef5b8cd1) , UINT64_C(0x7eb258665fc25d6a)} ,
    {UINT64_C(0x98bf2f79d5993802) , UINT64(0xef2f773ffbd97a62)} ,
    {UINT64_C(0xbeeefb584aff8603) , UINT64_C(0xaafb550ffacfd8fb)} ,
    {UINT64_C(0xeeaaba2e5dbf6784) , UINT64(0x95ba2a53f983cf39)} ,
    {UINT64_C(0x952ab45cfa97a0b2) , UINT64_C(0xdd945a747bf26184)} ,
    {UINT64_C(0xba756174393d88df) , UINT64(0x94f971119aeef9e5)} ,
    {UINT64_C(0xe912b9d1478ceb17) , UINT64_C(0x7a37cd5601aab85e)} ,
    {UINT64_C(0x91abb422ccb812ee) , UINT64(0xac62e055c10ab33b)} ,
    {UINT64_C(0xb616a12b7fe617aa) , UINT64_C(0x577b986b314d600a)} ,
    {UINT64_C(0xe39c49765fdf9d94) , UINT64(0xed5a7e85fda0b80c)} ,
    {UINT64_C(0x8e41ade9fbebc27d) , UINT64_C(0x14588f13be847308)} ,
    {UINT64_C(0xb1d219647ae6b31c) , UINT64(0x596eb2d8ae258fc9)} ,
    {UINT64_C(0xde469fbd99a05fe3) , UINT64_C(0x6fca5f8ed9aef3bc)} ,
    {UINT64_C(0x8aec23d680043bee) , UINT64(0x25de7bb9480d5855)} ,
    {UINT64_C(0xada72ccc20054ae9) , UINT64_C(0xaf561aa79a10ae6b)} ,
    {UINT64_C(0xd910f7ff28069da4) , UINT64(0x1b2ba1518094da05)} ,
    {UINT64_C(0x87aa9aff79042286) , UINT64_C(0x90fb44d2f05d0843)} ,
    {UINT64_C(0xa99541bf57452b28) , UINT64(0x353a1607ac744a54)} ,
    {UINT64_C(0xd3fa922f2d1675f2) , UINT64_C(0x42889b8997915ce9)} ,
    {UINT64_C(0x847c9b5d7c2e09b7) , UINT64(0x69956135febada12)} ,
    {UINT64_C(0xa59bc234db398c25) , UINT64_C(0x43fab9837e699096)} ,
    {UINT64_C(0xcf02b2c21207ef2e) , UINT64(0x94f967e45e03f4bc)} ,
    {UINT64_C(0x8161afb94b44f57d) , UINT64_C(0x1d1be0eebac278f6)} ,
    {UINT64_C(0xa1ba1ba79e1632dc) , UINT64(0x6462d92a69731733)} ,
    {UINT64_C(0xca28a291859bbf93) , UINT64_C(0x7d7b8f7503cfdcff)} ,
    {UINT64_C(0xfcb2cb35e702af78) , UINT64(0x5cda735244c3d43f)} ,
    {UINT64_C(0x9defbf01b061adab) , UINT64_C(0x3a0888136afa64a8)} ,
    {UINT64_C(0xc56baec21c7a1916) , UINT64(0x088aaa1845b8fdd1)} ,
    {UINT64_C(0xf6c69a72a3989f5b) , UINT64_C(0x8aad549e57273d46)} ,
    {UINT64_C(0x9a3c2087a63f6399) , UINT64(0x36ac54e2f678864c)} ,
    {UINT64_C(0xc0cb28a98fcf3c7f) , UINT64_C(0x84576a1bb416a7de)} ,
    {UINT64_C(0xf0fdf2d3f3c30b9f) , UINT64(0x656d44a2a11c51d6)} ,
    {UINT64_C(0x969eb7c47859e743) , UINT64_C(0x9f644ae5a4b1b326)} ,
    {UINT64_C(0xbc4665b596706114) , UINT64(0x873d5d9f0dde1fef)} ,
    {UINT64_C(0xeb57ff22fc0c7959) , UINT64_C(0xa90cb506d155a7eb)} ,
    {UINT64_C(0x9316ff75dd87cbd8) , UINT64(0x09a7f12442d588f3)} ,
    {UINT64_C(0xb7dcbf5354e9bece) , UINT64_C(0x0c11ed6d538aeb30)} ,
    {UINT64_C(0xe5d3ef282a242e81) , UINT64(0x8f1668c8a86da5fb)} ,
    {UINT64_C(0x8fa475791a569d10) , UINT64_C(0xf96e017d694487bd)} ,
    {UINT64_C(0xb38d92d760ec4455) , UINT64(0x37c981dcc395a9ad)} ,
    {UINT64_C(0xe070f78d3927556a) , UINT64_C(0x85bbe253f47b1418)} ,
    {UINT64_C(0x8c469ab843b89562) , UINT64(0x93956d7478ccec8f)} ,
    {UINT64_C(0xaf58416654a6babb) , UINT64_C(0x387ac8d1970027b3)} ,
    {UINT64_C(0xdb2e51bfe9d0696a) , UINT64(0x06997b05fcc0319f)} ,
    {UINT64_C(0x88fcf317f22241e2) , UINT64_C(0x441fece3bdf81f04)} ,
    {UINT64_C(0xab3c2fddeeaad25a) , UINT64(0xd527e81cad7626c4)} ,
    {UINT64_C(0xd60b3bd56a5586f1) , UINT64_C(0x8a71e223d8d3b075)} ,
    {UINT64_C(0x85c7056562757456) , UINT64(0xf6872d5667844e4a)} ,
    {UINT64_C(0xa738c6bebb12d16c) , UINT64_C(0xb428f8ac016561dc)} ,
    {UINT64_C(0xd106f86e69d785c7) , UINT64(0xe13336d701beba53)} ,
    {UINT64_C(0x82a45b450226b39c) , UINT64_C(0xecc0024661173474)} ,
    {UINT64_C(0xa34d721642b06084) , UINT64(0x27f002d7f95d0191)} ,
    {UINT64_C(0xcc20ce9bd35c78a5) , UINT64_C(0x31ec038df7b441f5)} ,
    {UINT64_C(0xff290242c83396ce) , UINT64(0x7e67047175a15272)} ,
    {UINT64_C(0x9f79a169bd203e41) , UINT64_C(0x0f0062c6e984d387)} ,
    {UINT64_C(0xc75809c42c684dd1) , UINT64(0x52c07b78a3e60869)} ,
    {UINT64_C(0xf92e0c3537826145) , UINT64_C(0xa7709a56ccdf8a83)} ,
    {UINT64_C(0x9bbcc7a142b17ccb) , UINT64(0x88a66076400bb692)} ,
    {UINT64_C(0xc2abf989935ddbfe) , UINT64_C(0x6acff893d00ea436)} ,
    {UINT64_C(0xf356f7ebf83552fe) , UINT64(0x0583f6b8c4124d44)} ,
    {UINT64_C(0x98165af37b2153de) , UINT64_C(0xc3727a337a8b704b)} ,
    {UINT64_C(0xbe1bf1b059e9a8d6) , UINT64(0x744f18c0592e4c5d)} ,
    {UINT64_C(0xeda2ee1c7064130c) , UINT64_C(0x1162def06f79df74)} ,
    {UINT64_C(0x9485d4d1c63e8be7) , UINT64(0x8addcb5645ac2ba9)} ,
    {UINT64_C(0xb9a74a0637ce2ee1) , UINT64_C(0x6d953e2bd7173693)} ,
    {UINT64_C(0xe8111c87c5c1ba99) , UINT64(0xc8fa8db6ccdd0438)} ,
    {UINT64_C(0x910ab1d4db9914a0) , UINT64_C(0x1d9c9892400a22a3)} ,
    {UINT64_C(0xb54d5e4a127f59c8) , UINT64(0x2503beb6d00cab4c)} ,
    {UINT64_C(0xe2a0b5dc971f303a) , UINT64_C(0x2e44ae64840fd61e)} ,
    {UINT64_C(0x8da471a9de737e24) , UINT64(0x5ceaecfed289e5d3)} ,
    {UINT64_C(0xb10d8e1456105dad) , UINT64_C(0x7425a83e872c5f48)} ,
    {UINT64_C(0xdd50f1996b947518) , UINT64(0xd12f124e28f7771a)} ,
    {UINT64_C(0x8a5296ffe33cc92f) , UINT64_C(0x82bd6b70d99aaa70)} ,
    {UINT64_C(0xace73cbfdc0bfb7b) , UINT64(0x636cc64d1001550c)} ,
    {UINT64_C(0xd8210befd30efa5a) , UINT64_C(0x3c47f7e05401aa4f)} ,
    {UINT64_C(0x8714a775e3e95c78) , UINT64(0x65acfaec34810a72)} ,
    {UINT64_C(0xa8d9d1535ce3b396) , UINT64_C(0x7f1839a741a14d0e)} ,
    {UINT64_C(0xd31045a8341ca07c) , UINT64(0x1ede48111209a051)} ,
    {UINT64_C(0x83ea2b892091e44d) , UINT64_C(0x934aed0aab460433)} ,
    {UINT64_C(0xa4e4b66b68b65d60) , UINT64(0xf81da84d56178540)} ,
    {UINT64_C(0xce1de40642e3f4b9) , UINT64_C(0x36251260ab9d668f)} ,
    {UINT64_C(0x80d2ae83e9ce78f3) , UINT64(0xc1d72b7c6b42601a)} ,
    {UINT64_C(0xa1075a24e4421730) , UINT64_C(0xb24cf65b8612f820)} ,
    {UINT64_C(0xc94930ae1d529cfc) , UINT64(0xdee033f26797b628)} ,
    {UINT64_C(0xfb9b7cd9a4a7443c) , UINT64_C(0x169840ef017da3b2)} ,
    {UINT64_C(0x9d412e0806e88aa5) , UINT64(0x8e1f289560ee864f)} ,
    {UINT64_C(0xc491798a08a2ad4e) , UINT64_C(0xf1a6f2bab92a27e3)} ,
    {UINT64_C(0xf5b5d7ec8acb58a2) , UINT64(0xae10af696774b1dc)} ,
    {UINT64_C(0x9991a6f3d6bf1765) , UINT64_C(0xacca6da1e0a8ef2a)} ,
    {UINT64_C(0xbff610b0cc6edd3f) , UINT64(0x17fd090a58d32af4)} ,
    {UINT64_C(0xeff394dcff8a948e) , UINT64_C(0xddfc4b4cef07f5b1)} ,
    {UINT64_C(0x95f83d0a1fb69cd9) , UINT64(0x4abdaf101564f98f)} ,
    {UINT64_C(0xbb764c4ca7a4440f) , UINT64_C(0x9d6d1ad41abe37f2)} ,
    {UINT64_C(0xea53df5fd18d5513) , UINT64(0x84c86189216dc5ee)} ,
    {UINT64_C(0x92746b9be2f8552c) , UINT64_C(0x32fd3cf5b4e49bb5)} ,
    {UINT64_C(0xb7118682dbb66a77) , UINT64(0x3fbc8c33221dc2a2)} ,
    {UINT64_C(0xe4d5e82392a40515) , UINT64_C(0x0fabaf3feaa5334b)} ,
    {UINT64_C(0x8f05b1163ba6832d) , UINT64(0x29cb4d87f2a7400f)} ,
    {UINT64_C(0xb2c71d5bca9023f8) , UINT64_C(0x743e20e9ef511013)} ,
    {UINT64_C(0xdf78e4b2bd342cf6) , UINT64(0x914da9246b255417)} ,
    {UINT64_C(0x8bab8eefb6409c1a) , UINT64_C(0x1ad089b6c2f7548f)} ,
    {UINT64_C(0xae9672aba3d0c320) , UINT64(0xa184ac2473b529b2)} ,
    {UINT64_C(0xda3c0f568cc4f3e8) , UINT64_C(0xc9e5d72d90a2741f)} ,
    {UINT64_C(0x8865899617fb1871) , UINT64(0x7e2fa67c7a658893)} ,
    {UINT64_C(0xaa7eebfb9df9de8d) , UINT64_C(0xddbb901b98feeab8)} ,
    {UINT64_C(0xd51ea6fa85785631) , UINT64(0x552a74227f3ea566)} ,
    {UINT64_C(0x8533285c936b35de) , UINT64_C(0xd53a88958f872760)} ,
    {UINT64_C(0xa67ff273b8460356) , UINT64(0x8a892abaf368f138)} ,
    {UINT64_C(0xd01fef10a657842c) , UINT64_C(0x2d2b7569b0432d86)} ,
    {UINT64_C(0x8213f56a67f6b29b) , UINT64(0x9c3b29620e29fc74)} ,
    {UINT64_C(0xa298f2c501f45f42) , UINT64_C(0x8349f3ba91b47b90)} ,
    {UINT64_C(0xcb3f2f7642717713) , UINT64(0x241c70a936219a74)} ,
    {UINT64_C(0xfe0efb53d30dd4d7) , UINT64_C(0xed238cd383aa0111)} ,
    {UINT64_C(0x9ec95d1463e8a506) , UINT64(0xf4363804324a40ab)} ,
    {UINT64_C(0xc67bb4597ce2ce48) , UINT64_C(0xb143c6053edcd0d6)} ,
    {UINT64_C(0xf81aa16fdc1b81da) , UINT64(0xdd94b7868e94050b)} ,
    {UINT64_C(0x9b10a4e5e9913128) , UINT64_C(0xca7cf2b4191c8327)} ,
    {UINT64_C(0xc1d4ce1f63f57d72) , UINT64(0xfd1c2f611f63a3f1)} ,
    {UINT64_C(0xf24a01a73cf2dccf) , UINT64_C(0xbc633b39673c8ced)} ,
    {UINT64_C(0x976e41088617ca01) , UINT64(0xd5be0503e085d814)} ,
    {UINT64_C(0xbd49d14aa79dbc82) , UINT64_C(0x4b2d8644d8a74e19)} ,
    {UINT64_C(0xec9c459d51852ba2) , UINT64(0xddf8e7d60ed1219f)} ,
    {UINT64_C(0x93e1ab8252f33b45) , UINT64_C(0xcabb90e5c942b504)} ,
    {UINT64_C(0xb8da1662e7b00a17) , UINT64(0x3d6a751f3b936244)} ,
    {UINT64_C(0xe7109bfba19c0c9d) , UINT64_C(0x0cc512670a783ad5)} ,
    {UINT64_C(0x906a617d450187e2) , UINT64(0x27fb2b80668b24c6)} ,
    {UINT64_C(0xb484f9dc9641e9da) , UINT64_C(0xb1f9f660802dedf7)} ,
    {UINT64_C(0xe1a63853bbd26451) , UINT64(0x5e7873f8a0396974)} ,
    {UINT64_C(0x8d07e33455637eb2) , UINT64_C(0xdb0b487b6423e1e9)} ,
    {UINT64_C(0xb049dc016abc5e5f) , UINT64(0x91ce1a9a3d2cda63)} ,
    {UINT64_C(0xdc5c5301c56b75f7) , UINT64_C(0x7641a140cc7810fc)} ,
    {UINT64_C(0x89b9b3e11b6329ba) , UINT64(0xa9e904c87fcb0a9e)} ,
    {UINT64_C(0xac2820d9623bf429) , UINT64_C(0x546345fa9fbdcd45)} ,
    {UINT64_C(0xd732290fbacaf133) , UINT64(0xa97c177947ad4096)} ,
    {UINT64_C(0x867f59a9d4bed6c0) , UINT64_C(0x49ed8eabcccc485e)} ,
    {UINT64_C(0xa81f301449ee8c70) , UINT64(0x5c68f256bfff5a75)} ,
    {UINT64_C(0xd226fc195c6a2f8c) , UINT64_C(0x73832eec6fff3112)} ,
    {UINT64_C(0x83585d8fd9c25db7) , UINT64(0xc831fd53c5ff7eac)} ,
    {UINT64_C(0xa42e74f3d032f525) , UINT64_C(0xba3e7ca8b77f5e56)} ,
    {UINT64_C(0xcd3a1230c43fb26f) , UINT64(0x28ce1bd2e55f35ec)} ,
    {UINT64_C(0x80444b5e7aa7cf85) , UINT64_C(0x7980d163cf5b81b4)} ,
    {UINT64_C(0xa0555e361951c366) , UINT64(0xd7e105bcc3326220)} ,
    {UINT64_C(0xc86ab5c39fa63440) , UINT64_C(0x8dd9472bf3fefaa8)} ,
    {UINT64_C(0xfa856334878fc150) , UINT64(0xb14f98f6f0feb952)} ,
    {UINT64_C(0x9c935e00d4b9d8d2) , UINT64_C(0x6ed1bf9a569f33d4)} ,
    {UINT64_C(0xc3b8358109e84f07) , UINT64(0x0a862f80ec4700c9)} ,
    {UINT64_C(0xf4a642e14c6262c8) , UINT64_C(0xcd27bb612758c0fb)} ,
    {UINT64_C(0x98e7e9cccfbd7dbd) , UINT64(0x8038d51cb897789d)} ,
    {UINT64_C(0xbf21e44003acdd2c) , UINT64_C(0xe0470a63e6bd56c4)} ,
    {UINT64_C(0xeeea5d5004981478) , UINT64(0x1858ccfce06cac75)} ,
    {UINT64_C(0x95527a5202df0ccb) , UINT64_C(0x0f37801e0c43ebc9)} ,
    {UINT64_C(0xbaa718e68396cffd) , UINT64(0xd30560258f54e6bb)} ,
    {UINT64_C(0xe950df20247c83fd) , UINT64_C(0x47c6b82ef32a206a)} ,
    {UINT64_C(0x91d28b7416cdd27e) , UINT64(0x4cdc331d57fa5442)} ,
    {UINT64_C(0xb6472e511c81471d) , UINT64_C(0xe0133fe4adf8e953)} ,
    {UINT64_C(0xe3d8f9e563a198e5) , UINT64(0x58180fddd97723a7)} ,
    {UINT64_C(0x8e679c2f5e44ff8f) , UINT64_C(0x570f09eaa7ea7649)} ,
    {UINT64_C(0xb201833b35d63f73) , UINT64(0x2cd2cc6551e513db)} ,
    {UINT64_C(0xde81e40a034bcf4f) , UINT64_C(0xf8077f7ea65e58d2)} ,
    {UINT64_C(0x8b112e86420f6191) , UINT64(0xfb04afaf27faf783)} ,
    {UINT64_C(0xadd57a27d29339f6) , UINT64_C(0x79c5db9af1f9b564)} ,
    {UINT64_C(0xd94ad8b1c7380874) , UINT64(0x18375281ae7822bd)} ,
    {UINT64_C(0x87cec76f1c830548) , UINT64_C(0x8f2293910d0b15b6)} ,
    {UINT64_C(0xa9c2794ae3a3c69a) , UINT64(0xb2eb3875504ddb23)} ,
    {UINT64_C(0xd433179d9c8cb841) , UINT64_C(0x5fa60692a46151ec)} ,
    {UINT64_C(0x849feec281d7f328) , UINT64(0xdbc7c41ba6bcd334)} ,
    {UINT64_C(0xa5c7ea73224deff3) , UINT64_C(0x12b9b522906c0801)} ,
    {UINT64_C(0xcf39e50feae16bef) , UINT64(0xd768226b34870a01)} ,
    {UINT64_C(0x81842f29f2cce375) , UINT64_C(0xe6a1158300d46641)} ,
    {UINT64_C(0xa1e53af46f801c53) , UINT64(0x60495ae3c1097fd1)} ,
    {UINT64_C(0xca5e89b18b602368) , UINT64_C(0x385bb19cb14bdfc5)} ,
    {UINT64_C(0xfcf62c1dee382c42) , UINT64(0x46729e03dd9ed7b6)} ,
    {UINT64_C(0x9e19db92b4e31ba9) , UINT64_C(0x6c07a2c26a8346d2)} ,
    {UINT64_C(0xc5a05277621be293) , UINT64(0xc7098b7305241886)} ,
    {0xf70867153aa2db38 , 0xb8cbee4fc66d1ea8}
};

#define F64_COMPRESSION_RATIO      27
#define F64_COMPRESSED_TABLE_SIZE  ((F64_CACHE_SIZE + F64_COMPRESSION_RATIO - \
    1) / F64_COMPRESSION_RATIO)
#define F64_POW5_TABLE_SIZE        F64_COMPRESSION_RATIO

static const u128_t  f64_compressed_cache[
    F64_COMPRESSED_TABLE_SIZE] = {
    {UINT64_C(0xff77b1fcbebcdc4f) , UINT64_C(0x25e8e89c13bb0f7b)} ,
    {UINT64_C(0xce5d73ff402d98e3) , UINT64_C(0xfb0a3d212dc81290)} ,
    {UINT64_C(0xdfbdcece67006ac9) , UINT64_C(0x67a791e093e1d49b)} ,
    {UINT64_C(0x855c3be0a17fcd26) , UINT64_C(0x5cf2eea09a550680)} ,
    {UINT64_C(0x9d71ac8fada6c9b5) , UINT64_C(0x6f773fc3603db4aa)} ,
    {UINT64_C(0xae0b158b4738705e) , UINT64_C(0x9624ab50b148d446)} ,
    {UINT64_C(0x80fa687f881c7f8e) , UINT64_C(0x7ce66634bc9d0b9a)} ,
    {UINT64_C(0xa5178fff668ae0b6) , UINT64_C(0x626e974dbe39a873)} ,
    {UINT64_C(0x9b407691d7fc44f8) , UINT64_C(0x79e0de63425dcf1e)} ,
    {UINT64_C(0x979cf3ca6cec5b5a) , UINT64_C(0xa705992ceecf9c43)} ,
    {UINT64_C(0xafbd2350644eeacf) , UINT64_C(0xe5d1929ef90898fb)} ,
    {UINT64_C(0x8d3360f09cf6e4bd) , UINT64_C(0x64712dd7abbbd95d)} ,
    {UINT64_C(0x8a7d3eef7f1cfc52) , UINT64_C(0x482835ea666b2573)} ,
    {UINT64_C(0x9096ea6f3848984f) , UINT64_C(0x3ff0d2c85def7622)} ,
    {UINT64_C(0xb080392cc4349dec) , UINT64_C(0xbd8d794d96aacfb4)} ,
    {UINT64_C(0x86ccbb52ea94baea) , UINT64_C(0x98e947129fc2b4ea)} ,
    {UINT64_C(0xaefae51477a06b03) , UINT64_C(0xede622920b6b23f2)} ,
    {UINT64_C(0x8d7eb76070a08aec) , UINT64_C(0xfc1e1de5cf543ca3)} ,
    {UINT64_C(0x8b61313bbabce2c6) , UINT64_C(0x2323ac4b3b3da016)} ,
    {UINT64_C(0xaa242499697392d2) , UINT64_C(0xdde50bd1d5d0b9ea)} ,
    {UINT64_C(0x81ceb32c4b43fcf4) , UINT64_C(0x80eacf948770ced8)} ,
    {UINT64_C(0xabcc77118461cefd) , UINT64_C(0xfdc20d2b36ba7c3e)} ,
    {UINT64_C(0x8000000000000000) , UINT64_C(0x0000000000000000)}
};
static const qo_uint64_t  f64_pow5_table[F64_POW5_TABLE_SIZE] = {
    UINT64_C(1) , UINT64_C(5) , UINT64_C(25) , UINT64_C(125) , UINT64_C(625) ,
    UINT64_C(3125) , UINT64_C(15625) , UINT64_C(78125) , UINT64_C(390625) ,
    UINT64_C(1953125) , UINT64_C(9765625) , UINT64_C(48828125) ,
    UINT64_C(244140625) , UINT64_C(1220703125) , UINT64_C(6103515625) ,
    UINT64_C(30517578125) , UINT64_C(152587890625) , UINT64_C(762939453125) ,
    UINT64_C(3814697265625) , UINT64_C(19073486328125) ,
    UINT64_C(95367431640625) , UINT64_C(476837158203125) ,
    UINT64_C(2384185791015625) , UINT64_C(11920928955078125) ,
    UINT64_C(59604644775390625) , UINT64_C(298023223876953125) ,
    UINT64_C(1490116119384765625)
};
// Cache Access Function (Full Cache)
QO_FORCE_INLINE qo_uint64_t
get_cache_f32_full(
    dec_exp_f32_t  k
) {
    assert(k >= F32_MIN_K && k <= F32_MAX_K);
    return f32_cache[k - F32_MIN_K];
}
QO_FORCE_INLINE u128_t
get_cache_f64_full(
    dec_exp_f64_t  k
) {
    assert(k >= F64_MIN_K && k <= F64_MAX_K);
    return f64_cache[k - F64_MIN_K];
}
// Cache Access Function (Compact Cache)
QO_FORCE_INLINE qo_uint64_t
get_cache_f32_compact(
    dec_exp_f32_t  k ,
    shift_f32_t    beta
) {
    assert(k >= F32_MIN_K && k <= F32_MAX_K);
    // Compute the base index: floor((k - min_k) / ratio)
    int  k_offset = k - F32_MIN_K;
    // Approx: (k_offset * 79) >> 10 according to C++ code
    int  cache_index = (int) (((qo_uint32_t) k_offset * 79U) >> 10);
    int  kb = cache_index * F32_COMPRESSION_RATIO + F32_MIN_K;
    int  offset = k - kb;

    qo_uint64_t  base_cache = f32_compressed_cache[cache_index];

    // --- ALWAYS PERFORM RECOVERY ---
    //if (offset == 0) {
    //    // DEBUG: Print the base cache value when offset is 0
    //    DEBUG_PRINTF("  [DEBUG get_cache_compact offset=0] k=%d, index=%d, base_cache=0x%llx\n",
    // (int)k, cache_index, (qo_uint64_t)base_cache);
    //    return base_cache; // <- This was the potentially problematic assumption
    //} else {
    qo_int32_t  k_log2  = floor_log2_pow10(k);
    qo_int32_t  kb_log2 = floor_log2_pow10(kb);
    int  alpha = k_log2 - kb_log2 - offset;    // Required shift amount

    // Ensure alpha is within bounds. If offset=0, alpha should be 0.
    assert(alpha >= 0 && alpha < 64);

    // Get power of 5 for the offset
    qo_uint32_t  pow5;
    if (offset == 0)
    {
        pow5 = 1;    // Explicitly handle offset 0 case for pow5
    }
    else if (offset >= F32_POW5_TABLE_SIZE)
    {
        // Combine powers if offset is large (should only happen if ratio is large)
        assert(offset - (F32_POW5_TABLE_SIZE - 1) < F32_POW5_TABLE_SIZE);
        pow5 = (qo_uint32_t) f32_pow5_table[F32_POW5_TABLE_SIZE - 1] *
               f32_pow5_table[offset - (F32_POW5_TABLE_SIZE - 1)];
    }
    else {
        pow5 = f32_pow5_table[offset];
    }

    // Perform 64x32 -> 128 (or wider potentially) multiplication
    u128_t  mul_result = umul128(base_cache , pow5);

    // Right shift the 128-bit result by alpha and round up (+1)
    // Note: If alpha = 0, this is just mul_result + 1
    qo_uint64_t  recovered_cache;
    if (alpha == 0)
    {
        // Calculate the product
        u128_t  mul_result = umul128(base_cache , pow5);   // pow5 is 1
        // here

        // Round up: add 1 to high part ONLY if low part is non-zero
        if (mul_result.low != 0)
        {
            recovered_cache = mul_result.high + 1;
        }
        else {
            recovered_cache = mul_result.high;
        }
        // Safety check for the specific case k=8:
        if (k == 8 && recovered_cache == 0)
        {
            DEBUG_PRINTF(
                "  [CRITICAL DEBUG] k=8, alpha=0 yielded recovered_cache=0. mul_result={0x%llx, 0x%llx}\n" ,
                (qo_uint64_t) mul_result.high , (qo_uint64_t) mul_result.low
            );
            // Force to expected problematic value for further debugging if needed
            // recovered_cache = 0xfa00000000000000; // Or the expected 0x9896... ?
        }
    }
    else {
        // Perform the shift as before
        qo_uint64_t  high_shifted = mul_result.high << (64 - alpha);
        qo_uint64_t  low_shifted  = mul_result.low >> alpha;
        // Check for carry from low part shift that needs to be added to high_shifted
        // qo_bool_t requires_carry = (mul_result.low & ((1ULL << alpha) - 1)) != 0; // Simplistic
        // check,
        // might be insufficient for +1 rounding
        // Let's stick to the original shift and add 1 logic for now, assuming it handles rounding
        // correctly.
        recovered_cache = (low_shifted | high_shifted);     // Combine shifted parts

        // Add 1 for rounding up AFTER shifting
        // Check if low part shift lost the bit just below the cut-off and need rounding
        // Proper rounding needs examining the bit shifted out and potentially subsequent bits.
        // The simple +1 might be the intended Dragonbox logic.
        // Let's refine the rounding: add 1 before the shift? Add (1 << (alpha - 1)) before shift?
        // The C++ code's "compute_product_and_shift" seems complex.
        // Let's try the simple +1 *after* the shift for now.
        // Consider adding the rounding offset *before* the shift for better precision?
        // u128_t round_offset = {0, (1ULL << (alpha - 1))}; // Add 0.5 before shift
        // mul_result = add128_128(mul_result, round_offset);
        // recovered_cache = (mul_result.low >> alpha) | (mul_result.high << (64 - alpha));

        // Sticking to the original C port's logic: shift then add 1
        recovered_cache += 1;
    }

    // DEBUG: Print recovered cache value
    DEBUG_PRINTF(
        "  [DEBUG get_cache_compact offset=%d] k=%d, base=0x%llx, pow5=%u, alpha=%d, recovered=0x%llx\n" ,
        offset , (int) k , (qo_uint64_t) base_cache , pow5 , alpha ,
        (qo_uint64_t) recovered_cache
    );

    assert(recovered_cache != 0);     // Ensure we don't get zero

    return recovered_cache;
    //} // End of original else block
}
/*
 * QO_FORCE_INLINE qo_uint64_t
 * get_cache_f32_compact(dec_exp_f32_t k,
 * shift_f32_t beta) {
 *   assert(k >= F32_MIN_K && k <= F32_MAX_K);
 *   // Compute the base index: floor((k - min_k) / ratio)
 *   int k_offset = k - F32_MIN_K;
 *   // Approx: (k_offset * 79) >> 10 according to C++ code
 *   int cache_index = (int)(((qo_uint32_t)k_offset * 79U) >> 10);
 *   int kb = cache_index * F32_COMPRESSION_RATIO + F32_MIN_K;
 *   int offset = k - kb;
 *
 *   qo_uint64_t base_cache = f32_compressed_cache[cache_index];
 *
 *   if (offset == 0) {
 *       return base_cache;
 *   } else {
 *       qo_int32_t k_log2 = floor_log2_pow10(k);
 *       qo_int32_t kb_log2 = floor_log2_pow10(kb);
 *       int alpha = k_log2 - kb_log2 - offset; // Required shift amount
 *       assert(alpha > 0 && alpha < 64);
 *
 *       qo_uint32_t pow5;
 *       if (offset >= F32_POW5_TABLE_SIZE) { // Should only happen if offset >= 7
 *          assert(offset - (F32_POW5_TABLE_SIZE-1) <
 * F32_POW5_TABLE_SIZE);
 *           pow5 = (qo_uint32_t)f32_pow5_table[F32_POW5_TABLE_SIZE-1] *
 *                  f32_pow5_table[offset - (F32_POW5_TABLE_SIZE-1)];
 *       } else {
 *           pow5 = f32_pow5_table[offset];
 *       }
 *
 *       u128_t mul_result = umul128(base_cache, pow5);
 *       // Right shift the 128-bit result by alpha
 *       qo_uint64_t high_shifted = mul_result.high << (64 - alpha);
 *       qo_uint64_t low_shifted = mul_result.low >> alpha;
 *       qo_uint64_t recovered_cache = (low_shifted | high_shifted) + 1; // Add 1 for rounding up
 *       assert(recovered_cache != 0);
 *
 *       return recovered_cache;
 *   }
 * } */
QO_FORCE_INLINE u128_t
get_cache_f64_compact(
    dec_exp_f64_t  k ,
    shift_f64_t    beta
) {
    assert(k >= F64_MIN_K && k <= F64_MAX_K);
    // Compute the base index: floor((k - min_k) / ratio)
    int  k_offset = k - F64_MIN_K;
    // Approx: (k_offset * 607) >> 14 according to C++ code
    int  cache_index = (int) (((qo_uint32_t) k_offset * 607U) >> 14);
    int  kb = cache_index * F64_COMPRESSION_RATIO + F64_MIN_K;
    int  offset = k - kb;

    u128_t  base_cache = f64_compressed_cache[cache_index];

    if (offset == 0)
    {
        return base_cache;
    }
    else {
        qo_int32_t  k_log2  = floor_log2_pow10(k);
        qo_int32_t  kb_log2 = floor_log2_pow10(kb);
        int  alpha = k_log2 - kb_log2 - offset; // Required shift amount
        assert(alpha > 0 && alpha < 64);

        assert(offset < F64_POW5_TABLE_SIZE);
        qo_uint64_t  pow5 = f64_pow5_table[offset];

        u128_t  recovered_cache_h = umul128(base_cache.high , pow5);
        u128_t  middle_low = umul128(base_cache.low , pow5);

        // recovered_cache = recovered_cache_h + (middle_low.high)
        recovered_cache_h = add128_64(recovered_cache_h , middle_low.high);

        qo_uint64_t  high_to_middle = recovered_cache_h.high << (64 - alpha);
        qo_uint64_t  middle_to_low  = recovered_cache_h.low << (64 - alpha);

        qo_uint64_t  final_high = (recovered_cache_h.low >>
            alpha) | high_to_middle;
        qo_uint64_t  final_low = (middle_low.low >> alpha) | middle_to_low;

        // Add 1 (round up)
        u128_t  final_cache = add128_64(make_uint128(final_high , final_low) ,
            1);

        return final_cache;
    }
}
//------------------------------------------------------------------------------
// Multiplication Traits Implementations (Specialized for f32/f64)
//------------------------------------------------------------------------------

typedef struct
{
    qo_uint64_t  integer_part; // qo_uint32_t for f32, qo_uint64_t for f64
    qo_bool_t    is_integer;
} ComputeMulResult;

typedef struct
{
    qo_bool_t  parity;
    qo_bool_t  is_integer;
} ComputeMulParityResult;
// --- For f32 ---
QO_FORCE_INLINE ComputeMulResult
compute_mul_f32(
    qo_uint32_t  u ,
    qo_uint64_t  cache
) {
    // Compute upper 64 bits of 32x64->96 bit multiplication
    qo_uint64_t  r = umul96_upper64(u , cache);
    ComputeMulResult  res;
    res.integer_part = (qo_uint32_t) (r >> 32); // Corresponds to carrier_uint (qo_uint32_t)
    res.is_integer = ((qo_uint32_t) r == 0);
    return res;
}
QO_FORCE_INLINE qo_uint64_t
// Actually qo_uint32_t would fit
compute_delta_f32(
    qo_uint64_t  cache ,
    shift_f32_t  beta
) {
    // cache_bits = 64, total_bits = 32
    return (qo_uint32_t) (cache >> (64 - 1 - beta));
}
QO_FORCE_INLINE ComputeMulParityResult
compute_mul_parity_f32(
    qo_uint32_t  two_f ,
    qo_uint64_t  cache ,
    shift_f32_t  beta
) {
    assert(beta >= 1 && beta <= 32);
    qo_uint64_t  r = umul96_lower64(two_f , cache);
    ComputeMulParityResult  res;
    res.parity = ((r >> (64 - beta)) & 1) != 0;
    // Integer check: Check if bits from (32-beta) to (64-beta-1) are zero
    res.is_integer = ((r >> (32 - beta)) & UINT32_C(0xffffffff)) == 0;
    return res;
}
QO_FORCE_INLINE qo_uint32_t
compute_left_endpoint_for_shorter_interval_case_f32(
    qo_uint64_t  cache ,
    shift_f32_t  beta
) {
    // significand_bits = 23
    return (qo_uint32_t) ((cache - (cache >> (23 + 2))) >>
        (64 - 23 - 1 - beta));
}
// buggy
QO_FORCE_INLINE qo_uint32_t
compute_right_endpoint_for_shorter_interval_case_f32(
    qo_uint64_t  cache ,
    shift_f32_t  beta
) {
    int  shift_amount = 40 - beta;
    DEBUG_PRINTF(
        "  [DEBUG compute_right_endpoint] cache=0x%llx, beta=%d, shift_amount=%d\n" ,
        (qo_uint64_t) cache , (int) beta , shift_amount
    );

    qo_uint64_t  term2 = cache >> 24ULL; // Use cache directly
    qo_uint64_t  numerator = (qo_uint64_t) cache + (qo_uint64_t) term2;
    DEBUG_PRINTF("  [DEBUG compute_right_endpoint] cache >> 24 = 0x%llx\n" ,
        (qo_uint64_t) term2);
    DEBUG_PRINTF("  [DEBUG compute_right_endpoint] numerator = 0x%llx\n" ,
        (qo_uint64_t) numerator);                                                                                                                            // Expected:
    // 0xbf7a7c2000000000

    qo_uint64_t  shifted_result = numerator >> shift_amount;
    DEBUG_PRINTF("  [DEBUG compute_right_endpoint] shifted_result = 0x%llx\n" ,
        (qo_uint64_t) shifted_result);                                                                       // Expected:
    // 0x17ef4f840

    qo_uint32_t  final_result = (qo_uint32_t) shifted_result;
    DEBUG_PRINTF("  [DEBUG compute_right_endpoint] final_result = 0x%x\n" ,
        final_result);                                                                     // Expected:
                                                                                           // 0x7ef4f840

    return final_result;
}
QO_FORCE_INLINE qo_uint32_t
compute_round_up_for_shorter_interval_case_f32(
    qo_uint64_t  cache ,
    shift_f32_t  beta
) {
    // significand_bits = 23
    return (qo_uint32_t) ((cache >> (64 - 23 - 2 - beta)) + 1) / 2;
}
// --- For f64 ---
QO_FORCE_INLINE ComputeMulResult
compute_mul_f64(
    qo_uint64_t  u ,
    u128_t       cache
) {
    u128_t  r = umul192_upper128(u , cache);
    ComputeMulResult  res;
    res.integer_part = r.high; // Corresponds to carrier_uint (qo_uint64_t)
    res.is_integer = (r.low == 0);
    return res;
}
QO_FORCE_INLINE qo_uint64_t
compute_delta_f64(
    u128_t       cache ,
    shift_f64_t  beta
) {
    // cache_bits = 128, total_bits = 64
    return cache.high >> (64 - 1 - beta);
}
QO_FORCE_INLINE ComputeMulParityResult
compute_mul_parity_f64(
    qo_uint64_t  two_f ,
    u128_t       cache ,
    shift_f64_t  beta
) {
    assert(beta >= 1 && beta < 64);
    u128_t  r = umul192_lower128(two_f , cache);
    ComputeMulParityResult  res;
    res.parity = ((r.high >> (64 - beta)) & 1) != 0;
    // Integer check: Check if bits from (64-beta) of low part up to (128-beta-1) of high part are
    // zero
    res.is_integer = (((r.high << beta)) | (r.low >> (64 - beta))) == 0;
    return res;
}
QO_FORCE_INLINE qo_uint64_t
compute_left_endpoint_for_shorter_interval_case_f64(
    u128_t       cache ,
    shift_f64_t  beta
) {
    // significand_bits = 52, total_bits = 64
    return (cache.high - (cache.high >> (52 + 2))) >> (64 - 52 - 1 - beta);
}
QO_FORCE_INLINE qo_uint64_t
compute_right_endpoint_for_shorter_interval_case_f64(
    u128_t       cache ,
    shift_f64_t  beta
) {
    // significand_bits = 52, total_bits = 64
    return (cache.high + (cache.high >> (52 + 1))) >> (64 - 52 - 1 - beta);
}
QO_FORCE_INLINE qo_uint64_t
compute_round_up_for_shorter_interval_case_f64(
    u128_t       cache ,
    shift_f64_t  beta
) {
    // significand_bits = 52, total_bits = 64
    return ((cache.high >> (64 - 52 - 2 - beta)) + 1) / 2;
}
//------------------------------------------------------------------------------
// Policy Interpretation Structures and Enums
//------------------------------------------------------------------------------

typedef enum
{
    ROUNDING_MODE_TAG_TO_NEAREST ,
    ROUNDING_MODE_TAG_LEFT_CLOSED_DIRECTED ,
    ROUNDING_MODE_TAG_RIGHT_CLOSED_DIRECTED
} RoundingModeTag;

// Interval type representation
typedef struct
{
    qo_bool_t  include_left;
    qo_bool_t  include_right;
} IntervalType;

// Predefined interval types
static const IntervalType  INTERVAL_CLOSED = {qo_true , qo_true};
static const IntervalType  INTERVAL_OPEN = {qo_false , qo_false};
static const IntervalType  INTERVAL_LEFT_CLOSED_RIGHT_OPEN = {qo_true ,
                                                              qo_false};
static const IntervalType  INTERVAL_RIGHT_CLOSED_LEFT_OPEN = {qo_false ,
                                                              qo_true};
// Get interval type based on rounding policy macro and float properties
// TODO:
QO_FORCE_INLINE IntervalType
get_normal_interval_f32(
    SignedSignificandBits  s
) {
#if DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TO_EVEN
    qo_bool_t  is_closed = signed_significand_has_even_significand_bits(s);
    IntervalType  i = { is_closed , is_closed };
    return i;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TO_ODD
    qo_bool_t  is_closed = !signed_significand_has_even_significand_bits(s);
    interval_type  i = { is_closed , is_closed };
    return i;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TOWARD_PLUS_INFINITY
    qo_bool_t  is_left_closed = !signed_significand_is_negative(s);
    interval_type  i = { is_left_closed , !is_left_closed };
    return i;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TOWARD_MINUS_INFINITY
    qo_bool_t  is_left_closed = signed_significand_is_negative(s);
    interval_type  i = { is_left_closed , !is_left_closed };
    return i;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TOWARD_ZERO
    return INTERVAL_RIGHT_CLOSED_LEFT_OPEN;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_AWAY_FROM_ZERO
    return INTERVAL_LEFT_CLOSED_RIGHT_OPEN;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TO_EVEN_STATIC_BOUNDARY
    return signed_significand_has_even_significand_bits(s) ?
           INTERVAL_CLOSED : INTERVAL_OPEN;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TO_ODD_STATIC_BOUNDARY
    return !signed_significand_has_even_significand_bits(s) ?
           INTERVAL_CLOSED : INTERVAL_OPEN;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TOWARD_PLUS_INFINITY_STATIC_BOUNDARY
    return signed_significand_is_negative(s) ?
           INTERVAL_RIGHT_CLOSED_LEFT_OPEN :
           INTERVAL_LEFT_CLOSED_RIGHT_OPEN;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TOWARD_MINUS_INFINITY_STATIC_BOUNDARY
    return signed_significand_is_negative(s) ?
           INTERVAL_LEFT_CLOSED_RIGHT_OPEN :
           INTERVAL_RIGHT_CLOSED_LEFT_OPEN;
#else \
    // Directed rounding modes - handled by choosing a different compute function
    // Return dummy value, should not be used directly for directed modes
    interval_type  dummy = {qo_false , qo_false};
    return dummy;
#endif
}
QO_FORCE_INLINE IntervalType
get_shorter_interval_f32(
    SignedSignificandBits  s
) {
#if DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TO_EVEN
    return INTERVAL_CLOSED;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TO_ODD
    return INTERVAL_OPEN;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TOWARD_PLUS_INFINITY
    qo_bool_t  is_left_closed = !signed_significand_is_negative(s);
    interval_type  i = { is_left_closed , !is_left_closed };
    return i;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TOWARD_MINUS_INFINITY
    qo_bool_t  is_left_closed = signed_significand_is_negative(s);
    interval_type  i = { is_left_closed , !is_left_closed };
    return i;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TOWARD_ZERO
    return INTERVAL_RIGHT_CLOSED_LEFT_OPEN;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_AWAY_FROM_ZERO
    return INTERVAL_LEFT_CLOSED_RIGHT_OPEN;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TO_EVEN_STATIC_BOUNDARY
    return INTERVAL_CLOSED; // Always closed for static boundary
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TO_ODD_STATIC_BOUNDARY
    return INTERVAL_OPEN;  // Always open for static boundary
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TOWARD_PLUS_INFINITY_STATIC_BOUNDARY
    return signed_significand_is_negative(s) ?
           INTERVAL_RIGHT_CLOSED_LEFT_OPEN :
           INTERVAL_LEFT_CLOSED_RIGHT_OPEN;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TOWARD_MINUS_INFINITY_STATIC_BOUNDARY
    return signed_significand_is_negative(s) ?
           INTERVAL_LEFT_CLOSED_RIGHT_OPEN :
           INTERVAL_RIGHT_CLOSED_LEFT_OPEN;
#else // Directed rounding modes
    interval_type  dummy = {qo_false , qo_false};
    return dummy;                                                     // Should not be called
#endif
}
// (Similarly define _f64 versions - identical logic, just different types maybe)
QO_FORCE_INLINE IntervalType
get_normal_interval_f64(
    SignedSignificandBits  s
) {
    // Logic is identical to f32 version
    return get_normal_interval_f32(s);
}
QO_FORCE_INLINE IntervalType
get_shorter_interval_f64(
    SignedSignificandBits  s
) {
    // Logic is identical to f32 version
    return get_shorter_interval_f32(s);
}
// Get rounding mode tag based on policy macro
QO_FORCE_INLINE RoundingModeTag
get_rounding_mode_tag(
    SignedSignificandBits  s
) {
#if DECIMAL_TO_BINARY_ROUNDING_POLICY <= \
    DECIMAL_TO_BINARY_ROUNDING_NEAREST_TOWARD_MINUS_INFINITY_STATIC_BOUNDARY
    return ROUNDING_MODE_TAG_TO_NEAREST;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_TOWARD_PLUS_INFINITY
    return signed_significand_is_negative(s) ?
           ROUNDING_MODE_TAG_LEFT_CLOSED_DIRECTED :
           ROUNDING_MODE_TAG_RIGHT_CLOSED_DIRECTED;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_TOWARD_MINUS_INFINITY
    return signed_significand_is_negative(s) ?
           ROUNDING_MODE_TAG_RIGHT_CLOSED_DIRECTED :
           ROUNDING_MODE_TAG_LEFT_CLOSED_DIRECTED;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_TOWARD_ZERO
    return ROUNDING_MODE_TAG_LEFT_CLOSED_DIRECTED;
#elif DECIMAL_TO_BINARY_ROUNDING_POLICY == \
    DECIMAL_TO_BINARY_ROUNDING_AWAY_FROM_ZERO
    return ROUNDING_MODE_TAG_RIGHT_CLOSED_DIRECTED;
#else
 # error "Invalid DECIMAL_TO_BINARY_ROUNDING_POLICY"
    return ROUNDING_MODE_TAG_TO_NEAREST; // Dummy
#endif
}
// Binary to decimal tie-breaking preference
QO_FORCE_INLINE qo_bool_t
prefer_round_down_b2d(
    qo_uint64_t  significand
) {
#if BINARY_TO_DECIMAL_ROUNDING_POLICY == BINARY_TO_DECIMAL_ROUNDING_TO_EVEN
    return (significand % 2) != 0; // Prefer round down if current significand is odd
#elif BINARY_TO_DECIMAL_ROUNDING_POLICY == BINARY_TO_DECIMAL_ROUNDING_TO_ODD
    return (significand % 2) == 0; // Prefer round down if current significand is even
#elif BINARY_TO_DECIMAL_ROUNDING_POLICY == \
    BINARY_TO_DECIMAL_ROUNDING_AWAY_FROM_ZERO
    return qo_false; // Always prefer round up (away from zero)
#elif BINARY_TO_DECIMAL_ROUNDING_POLICY == \
    BINARY_TO_DECIMAL_ROUNDING_TOWARD_ZERO
    return qo_true; // Always prefer round down (toward zero)
#else // DO_NOT_CARE
    return qo_false; // Default: prefer round up
#endif
}
//------------------------------------------------------------------------------
// Result Type Definition
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Core Algorithm Implementation (Templated via Macros)
//------------------------------------------------------------------------------
// This section implements the main logic from detail::impl in the C++ version.
// It uses macros heavily to adapt to the chosen policies and float types.

// Helper struct to bundle format-specific parameters
typedef struct
{
    const IEEE754Format * format;
    int                   kappa;
    int                   min_k;
    int                   max_k;
    int                   case_shorter_interval_left_endpoint_lower_threshold;
    int                   case_shorter_interval_left_endpoint_upper_threshold;
    int                   case_shorter_interval_right_endpoint_lower_threshold;
    int                   case_shorter_interval_right_endpoint_upper_threshold;
    int                   shorter_interval_tie_lower_threshold;
    int                   shorter_interval_tie_upper_threshold;
} FormatParams;

// Precompute parameters for f32 and f64
static const FormatParams  params_f32 = {
    &ieee754_binary32 , 1 , // kappa = floor(log10(2^(32-23-2))) - 1 = floor(log10(128)) - 1 = 2 - 1
    // = 1
    -31 , 46 , // Calculated min_k, max_k
    2 , 5 ,  // Left endpoint thresholds
    0 , 3 ,  // Right endpoint thresholds
    -31 , -29 // Tie thresholds
};

static const FormatParams  params_f64 = {
    &ieee754_binary64 , 2 , // kappa = floor(log10(2^(64-52-2))) - 1 = floor(log10(1024)) - 1 = 3 -
    // 1 = 2
    -292 , 326 , // Calculated min_k, max_k
    2 , 6 ,   // Left endpoint thresholds
    0 , 4 ,   // Right endpoint thresholds
    -60 , -57 // Tie thresholds
};
QO_FORCE_INLINE qo_bool_t
is_left_endpoint_integer_shorter_interval(
    qo_int32_t           binary_exponent ,
    const FormatParams * params
) {
    return binary_exponent >=
           params->case_shorter_interval_left_endpoint_lower_threshold &&
           binary_exponent <=
           params->case_shorter_interval_left_endpoint_upper_threshold;
}
QO_FORCE_INLINE qo_bool_t
is_right_endpoint_integer_shorter_interval(
    qo_int32_t           binary_exponent ,
    const FormatParams * params
) {
    return binary_exponent >=
           params->case_shorter_interval_right_endpoint_lower_threshold &&
           binary_exponent <=
           params->case_shorter_interval_right_endpoint_upper_threshold;
}
// Core computation function for nearest rounding modes
// Templated arguments (C++ policies) are replaced by preprocessor defines
QO_FORCE_INLINE QO_DecimalFPExtract
compute_nearest_f32(
    SignedSignificandBits  s ,
    qo_int32_t             exponent_bits
) {
    const FormatParams * params  = &params_f32;
    const IEEE754Format * format = params->format;
    QO_DecimalFPExtract  result;

    DEBUG_PRINTF("[DEBUG compute_nearest_f32 for 0x%llx, exp_bits=%d]\n" ,
        (qo_uint64_t) s.u , exponent_bits);                                                                        // 打印输入
    DEBUG_PRINTF("  Kappa = %d\n" , params->kappa); // <-- 打印 Kappa

    // Typedefs for convenience based on policy
    typedef qo_uint32_t      carrier_uint_t;
    typedef remainder_f32_t  remainder_t;
    typedef dec_exp_f32_t    decimal_exponent_t;
    typedef shift_f32_t      shift_t;

    qo_uint32_t  two_fc =
        (qo_uint32_t) signed_significand_remove_sign_bit_and_shift(s);
    qo_int32_t   binary_exponent = exponent_bits;

    qo_bool_t    is_normal = (binary_exponent != 0);
    qo_bool_t    shorter_interval_case = qo_false;

    if (is_normal)
    {
        binary_exponent += format->exponent_bias - format->significand_bits;
        if (two_fc == 0)   // Shorter interval check
        {
            shorter_interval_case = qo_true;
        }
        else {
            two_fc |= (UINT32_C(1) << (format->significand_bits + 1));
        }
    }
    else {
        binary_exponent = format->min_exponent - format->significand_bits;
    }
    DEBUG_PRINTF("  binary_exponent = %d\n" , binary_exponent);                             // <--
                                                                                            // 打印
                                                                                            // binary_exponent

    if (shorter_interval_case)
    {
        DEBUG_PRINTF("  Entering Shorter Interval Case\n");
        IntervalType  interval_type = get_shorter_interval_f32(s);

        decimal_exponent_t const  minus_k =
            (decimal_exponent_t) floor_log10_pow2_minus_log10_4_over_3(
                binary_exponent
            );
        DEBUG_PRINTF("  Shorter: minus_k = %d\n" , (int) minus_k); // <-- 打印 minus_k
        shift_t const  beta = (shift_t) (binary_exponent +
            floor_log2_pow10(-minus_k));

        qo_uint64_t  cache;
#if CACHE_POLICY == CACHE_FULL
        cache = get_cache_f32_full(-minus_k);
#else // COMPACT
        cache = get_cache_f32_compact(-minus_k , beta);
#endif
        qo_uint32_t  xi =
            compute_left_endpoint_for_shorter_interval_case_f32(cache , beta);
        qo_uint32_t  zi =
            compute_right_endpoint_for_shorter_interval_case_f32(cache , beta);
        DEBUG_PRINTF("  Shorter: Calculated zi = 0x%x (%u)\n" , zi , zi); // <-- 添加打印 zi
// ... 调整 zi ...
        DEBUG_PRINTF("  Shorter: Adjusted zi = 0x%x (%u)\n" , zi , zi); // <-- 添加打印调整后的 zi

        if (!interval_type.include_right &&
            is_right_endpoint_integer_shorter_interval(binary_exponent ,
                params))
        {
            --zi;
        }
        if (!interval_type.include_left ||
            !is_left_endpoint_integer_shorter_interval(binary_exponent ,
                params))
        {
            ++xi;
        }

        carrier_uint_t  decimal_significand = divide_by_pow10_u32(zi , 1); // Divide
        // by
        // 10^1

        if ((qo_uint64_t) decimal_significand * 10 >= xi)  // Check qo_uint64_t to prevent overflow
                                                           // before
                                                           // comparison
        {
            result.significand = decimal_significand;
            result.exponent = minus_k + 1;
            DEBUG_PRINTF("  Shorter: Returning exponent %d\n" ,
                (int) result.exponent);                                                 // <--
                                                                                        // 打印最终指数
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
            result.may_have_trailing_zeros = qo_true;
#endif
            // Apply remove trailing zero policy if needed (done after assigning sign)
        }
        else {
            decimal_significand =
                compute_round_up_for_shorter_interval_case_f32(cache , beta);

            if (prefer_round_down_b2d(decimal_significand) &&
                binary_exponent >=
                params->shorter_interval_tie_lower_threshold &&
                binary_exponent <= params->shorter_interval_tie_upper_threshold)
            {
                --decimal_significand;
            }
            else if (decimal_significand < xi)
            {
                // This case implies the rounded-up value was still too low, which can happen
                // if xi itself was increased. We need the smallest integer >= xi.
                // Since round_up gives floor(y + 1/2), and we know y < xi,
                // the smallest integer >= xi must be xi itself.
                // However, the C++ code increments significand here. Let's trace:
                // If y < xi and floor(y+1/2) < xi, we must choose xi.
                // If round_up < xi, incrementing it gives the smallest integer *potentially* >= xi.
                // Let's stick to the C++ code's behavior for precise porting.
                ++decimal_significand;
            }
            result.significand = decimal_significand;
            result.exponent = minus_k;
            DEBUG_PRINTF("  Shorter: Returning exponent %d\n" ,
                (int) result.exponent);                                                 // <--
                                                                                        // 打印最终指数
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
            result.may_have_trailing_zeros = qo_false;
#endif
        }
    }
    else {   // Normal interval case
        DEBUG_PRINTF("  Entering Normal Interval Case\n");
        IntervalType  interval_type = get_normal_interval_f32(s);
        int  kappa = params->kappa;

        decimal_exponent_t const  minus_k =
            (decimal_exponent_t) (floor_log10_pow2(binary_exponent) - kappa);
        DEBUG_PRINTF("  Normal: minus_k = %d\n" , (int) minus_k); // <-- 打印 minus_k
        shift_t const  beta = (shift_t) (binary_exponent +
            floor_log2_pow10(-minus_k));

        qo_uint64_t  cache;
#if CACHE_POLICY == CACHE_FULL
        cache = get_cache_f32_full(-minus_k);
#else // COMPACT
        cache = get_cache_f32_compact(-minus_k , beta);
#endif

        remainder_t const  deltai = (remainder_t) compute_delta_f32(cache ,
            beta);
        ComputeMulResult   z_result = compute_mul_f32(
            (qo_uint32_t) ((two_fc | 1) << beta) , cache
        );

        const remainder_t  big_divisor = (remainder_t) pow_u32(10 , kappa + 1);
        const remainder_t  small_divisor = (remainder_t) pow_u32(10 , kappa);

        carrier_uint_t  decimal_significand =
            divide_by_pow10_u32((qo_uint32_t) z_result.integer_part ,
                kappa + 1);
        remainder_t  r = (remainder_t) (z_result.integer_part -
            (qo_uint64_t) big_divisor * decimal_significand);
        qo_bool_t    handled = qo_false;

        // Step 2: Try larger divisor
        if (r < deltai)
        {
            if (r == 0 && !z_result.is_integer && !interval_type.include_right)
            {
                // Need to round down
#if BINARY_TO_DECIMAL_ROUNDING_POLICY == BINARY_TO_DECIMAL_ROUNDING_DO_NOT_CARE
                // C++ implementation optimization: just return lower candidate directly
                --decimal_significand;
                result.significand = decimal_significand * 10 + 9;  // effectively (zi-1)/100 * 10 +
                                                                    // 9 = floor(z/100)*10+9
                result.exponent = minus_k + kappa;
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
                result.may_have_trailing_zeros = qo_false;  // 9 is never zero
 # endif
                handled = qo_true;
#else
                // Fall through to step 3 after rounding down
                --decimal_significand;
                r = big_divisor;  // Will become big_divisor - deltai in step 3
#endif
            }
            else {
                // Upper candidate is valid
                result.significand = decimal_significand;
                result.exponent = minus_k + kappa + 1;
                DEBUG_PRINTF("  Normal: Returning exponent %d\n" ,
                    (int) result.exponent);                                                // <--
                                                                                           // 打印最终指数
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
                result.may_have_trailing_zeros = qo_true; // Could be x.x00...
#endif
                handled = qo_true;
            }
        }
        else if (r == deltai)
        {
            // Tie case: compare fractional parts using parity check
            ComputeMulParityResult  x_result =
                compute_mul_parity_f32((qo_uint32_t) (two_fc - 1) , cache ,
                    beta);

            if (!(x_result.parity ||
                  (x_result.is_integer && interval_type.include_left)))
            {
                // Lower candidate is valid
                result.significand = decimal_significand;
                result.exponent = minus_k + kappa + 1;
                DEBUG_PRINTF("  Normal: Returning exponent %d\n" ,
                    (int) result.exponent);                                                // <--
                                                                                           // 打印最终指数
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
                result.may_have_trailing_zeros = qo_true;
#endif
                handled = qo_true;
            }
            // Otherwise, fall through to step 3 (lower candidate is chosen)
        }
        // Else (r > deltai), fall through to step 3

        // Step 3: Use smaller divisor if not handled in Step 2
        if (!handled)
        {
            decimal_significand *= 10;

#if BINARY_TO_DECIMAL_ROUNDING_POLICY == BINARY_TO_DECIMAL_ROUNDING_DO_NOT_CARE
            // Simplified rounding: add floor(r / small_divisor), but handle right endpoint
            // exclusion
            if (!interval_type.include_right)
            {
                remainder_t  r_copy = r;
                if (check_divisibility_and_divide_by_pow10_u32_inplace(
                    &
                    r_copy , kappa
                    ) && z_result.is_integer)
                {
                    // If zi/10^kappa is an integer and endpoint excluded, round down
                    decimal_significand += r_copy - 1;
                }
                else {
                    // Not exactly the endpoint or endpoint included
                    decimal_significand += small_division_by_pow10_u32(r ,
                        kappa);
                }
            }
            else {
                decimal_significand += small_division_by_pow10_u32(r , kappa);
            }
#else // Tie-breaking modes
            // dist = r - delta/2 + divisor/2 = r - (deltai-small_divisor)/2
            remainder_t  dist = r;
            remainder_t  delta_diff = (deltai >
                small_divisor) ? (deltai - small_divisor) : 0;
            if ((delta_diff / 2) > dist)
            {
                dist = 0;
            }
            else{ dist -= (delta_diff / 2);
            }

            // Add floor(dist / small_divisor)
            remainder_t  quot = small_division_by_pow10_u32(dist , kappa);
            remainder_t  rem_dist = dist - quot * small_divisor;
            decimal_significand += quot;

            // Tie-breaking based on remainder comparison and policy
            if (rem_dist > small_divisor / 2)
            {
                ++decimal_significand;
            }
            else if (rem_dist == small_divisor / 2)
            {
                // Check parity of y = 2f * 10^k / 2^beta
                ComputeMulParityResult  y_result =
                    compute_mul_parity_f32(two_fc , cache , beta);

                // Check approx_y_parity == y_parity
                // approx_y_parity is parity of floor(dist / small_divisor) + parity of
                // small_divisor/2
                qo_bool_t  approx_y_parity = ((quot % 2) != 0); // Parity of floor(dist /
                                                                // small_divisor)
                // If small_divisor / 2 is odd (i.e., small_divisor is 2 mod 4), flip approx parity.
                // 10^kappa / 2 is odd only if kappa=0 and 1/2 (not possible) or kappa=1 and 10/2=5.
                if (kappa == 1)
                {
                    approx_y_parity = !approx_y_parity;
                }

                if (y_result.parity != approx_y_parity) // Implies fraction was slightly
                                                        // underestimated
                {
                    ++decimal_significand;              // Correct round up
                }
                else {
                    // Parities match. Check tie-breaking rule.
                    if (!prefer_round_down_b2d(decimal_significand) &&
                        y_result.is_integer)
                    {
                        ++decimal_significand;  // Round up on tie
                    }
                }
            }
            // else rem_dist < small_divisor / 2: round down is correct
#endif
            result.significand = decimal_significand;
            result.exponent = minus_k + kappa;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
            result.may_have_trailing_zeros = qo_false; // Smaller divisor case usually doesn't end
                                                       // in 0
                                                       // unless input was integer * 10^n
#endif
        }
    } // End normal interval case

    // Apply sign policy
#if SIGN_POLICY == SIGN_RETURN
    result.is_negative = signed_significand_is_negative(s);
#endif

    DEBUG_PRINTF("  Before Trailing Zero Check: Sig=0x%x (%u), Exp=%d" ,
        result.significand , result.significand , (int) result.exponent);

    // Apply trailing zero policy (after sign is potentially set)
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE
    // Check if may_have_trailing_zeros was set qo_true (only possible in large divisor cases)
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
    DEBUG_PRINTF(", MayHaveZeros=%d" , result.may_have_trailing_zeros);
    if (result.may_have_trailing_zeros) // Check the flag if it exists
 # endif
    {                                   // Braces always needed for conditional compilation clarity
        remove_trailing_zeros_f32_remove(&result.significand ,
            &result.exponent);
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_false;  // Zeros removed
 # endif
    }
#elif TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE_COMPACT
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
    if (result.may_have_trailing_zeros)
 # endif
    {
        remove_trailing_zeros_f32_remove_compact(&result.significand ,
            &result.exponent);
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_false;
 # endif
    }
#endif
    DEBUG_PRINTF("  Final Result: Sig=0x%x (%u), Exp=%d\n" ,
        result.significand , result.significand , (int) result.exponent);
    return result;
}
// Core computation function for left-closed directed rounding modes
QO_FORCE_INLINE QO_DecimalFPExtract
compute_left_closed_directed_f32(
    SignedSignificandBits  s ,
    qo_int32_t             exponent_bits
) {
    const FormatParams * params  = &params_f32;
    const IEEE754Format * format = params->format;
    QO_DecimalFPExtract  result;

    // Typedefs for convenience
    typedef qo_uint32_t      carrier_uint_t;
    typedef remainder_f32_t  remainder_t;
    typedef dec_exp_f32_t    decimal_exponent_t;
    typedef shift_f32_t      shift_t;

    qo_uint32_t  two_fc =
        (qo_uint32_t) signed_significand_remove_sign_bit_and_shift(s);
    qo_int32_t   binary_exponent = exponent_bits;

    qo_bool_t    is_normal = (binary_exponent != 0);

    if (is_normal)
    {
        binary_exponent += format->exponent_bias - format->significand_bits;
        two_fc |= (UINT32_C(1) << (format->significand_bits + 1));
    }
    else {
        binary_exponent = format->min_exponent - format->significand_bits;
    }

    // Step 1: Schubfach multiplier calculation
    int  kappa = params->kappa;
    decimal_exponent_t const  minus_k =
        (decimal_exponent_t) (floor_log10_pow2(binary_exponent) - kappa);
    shift_t const  beta = (shift_t) (binary_exponent +
        floor_log2_pow10(-minus_k));

    qo_uint64_t  cache;
#if CACHE_POLICY == CACHE_FULL
    cache = get_cache_f32_full(-minus_k);
#else // COMPACT
    cache = get_cache_f32_compact(-minus_k , beta);
#endif

    remainder_t const  deltai = (remainder_t) compute_delta_f32(cache , beta);
    ComputeMulResult   x_result = compute_mul_f32(
        (qo_uint32_t) (two_fc <<
                beta) , cache
    );

    // Handle unique f32 exceptions (from C++ comments) - check if fractional part is non-zero
    if (!x_result.is_integer)
    {
        // Input is not f * 2^e = M * 10^-k for integer M.
        // We need ceil(x), where x = f * 2^e * 10^-k.
        // Since x is not integer, ceil(x) = floor(x) + 1.
        x_result.integer_part++; // Equivalent to integer_part = xi
    }
    // If x_result.is_integer was qo_true, integer_part = x = xi.

    // Step 2: Try larger divisor
    const remainder_t  big_divisor = (remainder_t) pow_u32(10 , kappa + 1);
    carrier_uint_t  decimal_significand =
        divide_by_pow10_u32((qo_uint32_t) x_result.integer_part , kappa + 1);
    remainder_t  r = (remainder_t) (x_result.integer_part -
        (qo_uint64_t) big_divisor * decimal_significand);
    qo_bool_t    handled = qo_false;

    // If r == 0, xi was divisible by big_divisor. Increment significand, set r=big_divisor for step
    // 3.
    // If r != 0, xi = q*big_divisor + r. We need ceil(xi / big_divisor) = q+1.
    // So increment significand, and remainder for step 3 is big_divisor - r.
    if (r != 0)
    {
        decimal_significand++;
        r = big_divisor - r;
    }
    else {
        // If r==0, xi was divisible. The ceiling is xi/big_divisor = decimal_significand.
        // We still need to check if this ceiling is strictly inside the interval (xi, zi].
        // Compare fractional parts: zi^(f) > xi^(f)? xi^(f) is 0 here.
        // Need zi^(f) > 0.
    }

    // Check condition: r > deltai OR (r == deltai AND zi is not integer)
    // If qo_true, the smaller divisor result is needed. Otherwise, the larger divisor result is
    // correct.
    if (r > deltai)
    {
        // Fall through to step 3
    }
    else if (r == deltai)
    {
        // Check fractional part of Z: compute_mul_parity(two_fc + 2, ...)
        // We need zi^(f) > 0 for the current candidate (larger divisor) to be valid.
        ComputeMulParityResult  z_result =
            compute_mul_parity_f32((qo_uint32_t) (two_fc + 2) , cache , beta);
        if (z_result.parity || z_result.is_integer)    // Check if zi^(f) == 0
        // Fall through to step 3
        {
        }
        else {
            // zi^(f) > 0, so the current candidate (larger divisor) is correct
            result.significand = decimal_significand;
            result.exponent = minus_k + kappa + 1;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
            result.may_have_trailing_zeros = qo_true;
#endif
            handled = qo_true;
        }
    }
    else {   // r < deltai
        // Current candidate (larger divisor) is correct.
        result.significand = decimal_significand;
        result.exponent = minus_k + kappa + 1;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_true;
#endif
        handled = qo_true;
    }

    // Step 3: Find the significand with the smaller divisor
    if (!handled)
    {
        const remainder_t  small_divisor = (remainder_t) pow_u32(10 , kappa);
        // We calculated decimal_significand = ceil(xi / big_divisor)
        // The target is ceil(xi / small_divisor)
        // = ceil( (q*big_divisor + (r==0?0:big_divisor-r)) / small_divisor ) where
        // q=floor(xi/big_divisor)
        // = ceil( q*10 + (r==0?0:big_divisor-r)/small_divisor )
        // = q*10 + ceil( (r==0?0:big_divisor-r)/small_divisor )
        // If r=0: q*10. We had decimal_significand = q. Target is q*10. Remainder for division is
        // 0.
        // If r!=0: q*10 + ceil((big_divisor-r)/small_divisor). We had decimal_significand = q+1.
        // Let rem = big_divisor - r. We need (q+1)*10 - floor(r/small_divisor) ? No.
        // Correct formula: floor(xi / small_divisor) = floor( (xi * 10^kappa) / 10^kappa /
        // small_divisor) ? No.
        // From C++: result = decimal_significand * 10 - floor(r / small_divisor)
        // Let's re-derive. xi = decimal_significand_big * big_divisor - r_adj where r_adj = (r==0 ?
        // 0 : big_divisor-r).
        // We want ceil(xi / small_divisor) = ceil( (ds_big * 10 * small_divisor - r_adj) /
        // small_divisor )
        // = ceil( ds_big * 10 - r_adj / small_divisor ) = ds_big * 10 - floor( r_adj /
        // small_divisor )
        // This matches the C++ code.
        decimal_significand = (decimal_significand * 10) -
                              small_division_by_pow10_u32(r , kappa);                                                           // r
        // here
        // is
        // big_divisor-r
        // from
        // before
        // if
        // r!=0

        result.significand = decimal_significand;
        result.exponent = minus_k + kappa;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_false;
#endif
    }

    DEBUG_PRINTF("  Before Trailing Zero Check: Sig=0x%x (%u), Exp=%d" ,
        result.significand , result.significand , (int) result.exponent);
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
    DEBUG_PRINTF(", MayHaveZeros=%d" , result.may_have_trailing_zeros);
#endif
    DEBUG_PRINTF("\n");

    // Apply sign policy
#if SIGN_POLICY == SIGN_RETURN
    result.is_negative = signed_significand_is_negative(s);
#endif
    // Apply trailing zero policy (done implicitly where needed by returning correct value)
#if (TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE || \
        TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE_COMPACT)
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
    if (result.may_have_trailing_zeros)
 # endif
    {  // Remove only if potentially has zeros (large divisor case)
        DEBUG_PRINTF("  Applying Trailing Zero Removal...\n"); // <-- 确认是否进入
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE
        remove_trailing_zeros_f32_remove(&result.significand ,
            &result.exponent);
 # else // REMOVE_COMPACT
        remove_trailing_zeros_f32_remove_compact(&result.significand ,
            &result.exponent);
 # endif
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_false;
 # endif
        DEBUG_PRINTF("  After Trailing Zero Removal: Sig=0x%x (%u), Exp=%d\n" ,
            result.significand , result.significand , (int) result.exponent);                                                             // <--
                                                                                                                                          // 确认移除后的值
    }
#endif
    DEBUG_PRINTF("  Returning Final: Sig=0x%x (%u), Exp=%d\n" ,
        result.significand , result.significand , (int) result.exponent);                                                     // <--
                                                                                                                              // 确认最终返回的值
    return result;
}
// Core computation function for right-closed directed rounding modes
QO_FORCE_INLINE QO_DecimalFPExtract
compute_right_closed_directed_f32(
    SignedSignificandBits  s ,
    qo_int32_t             exponent_bits
) {
    const FormatParams * params  = &params_f32;
    const IEEE754Format * format = params->format;
    QO_DecimalFPExtract  result;

    // Typedefs for convenience
    typedef qo_uint32_t      carrier_uint_t;
    typedef remainder_f32_t  remainder_t;
    typedef dec_exp_f32_t    decimal_exponent_t;
    typedef shift_f32_t      shift_t;

    qo_uint32_t  two_fc =
        (qo_uint32_t) signed_significand_remove_sign_bit_and_shift(s);
    qo_int32_t   binary_exponent  = exponent_bits;
    qo_bool_t    shorter_interval = qo_false;

    qo_bool_t    is_normal = (binary_exponent != 0);

    if (is_normal)
    {
        // Check for shorter interval case (2f_c = 0 but not smallest normal exponent)
        if (two_fc == 0 && binary_exponent != 1)
        {
            shorter_interval = qo_true;
        }
        binary_exponent += format->exponent_bias - format->significand_bits;
        two_fc |= (UINT32_C(1) << (format->significand_bits + 1));
    }
    else {
        binary_exponent = format->min_exponent - format->significand_bits;
    }

    // Step 1: Schubfach multiplier calculation
    int  kappa = params->kappa;
    // Adjust exponent for shorter interval calculation
    qo_int32_t  effective_binary_exponent = binary_exponent -
                                            (shorter_interval ? 1 : 0);
    decimal_exponent_t const  minus_k =
        (decimal_exponent_t) (floor_log10_pow2(effective_binary_exponent) -
            kappa);
    shift_t const  beta = (shift_t) (binary_exponent +
        floor_log2_pow10(-minus_k));

    qo_uint64_t  cache;
#if CACHE_POLICY == CACHE_FULL
    cache = get_cache_f32_full(-minus_k);
#else // COMPACT
    cache = get_cache_f32_compact(-minus_k , beta);
#endif

    // Adjust beta for delta calculation if shorter interval
    shift_t  delta_beta = beta - (shorter_interval ? 1 : 0);
    remainder_t const  deltai = (remainder_t) compute_delta_f32(cache ,
        delta_beta);

    // Calculate zi = floor( (2f_c+2) * 10^-k / 2^beta ) ? No, zi = floor( z * 10^-k ) = floor(
    // (2f_c+2)/2 * 2^e * 10^-k )
    // zi = floor( (fc+1) * 2^e * 10^-k )
    // C++ code computes floor( 2f_c * 10^-k / 2^beta ) which seems like floor(x).
    // Let z = (2f_c + 2)/2 * 2^e = (f_c+1)*2^e. Target is floor(z * 10^-k).
    // C++ compute_mul calculates floor( u * cache / 2^(input_bits + cache_bits - output_bits) )
    // Here: u = 2f_c << beta. output_bits = 32. input_bits = 32. cache_bits = 64.
    // floor( (2f_c << beta) * cache / 2^(32+64-32) ) = floor( 2f_c * cache / 2^64 )
    // This is floor(x).
    ComputeMulResult  z_result = compute_mul_f32(
        (qo_uint32_t) (two_fc <<
                beta) , cache
    );
    qo_uint64_t  zi = z_result.integer_part; // This is floor(x)

    // Step 2: Try larger divisor
    const remainder_t  big_divisor = (remainder_t) pow_u32(10 , kappa + 1);
    carrier_uint_t  decimal_significand = divide_by_pow10_u32((qo_uint32_t) zi ,
        kappa + 1);
    remainder_t  r = (remainder_t) (zi - (qo_uint64_t) big_divisor *
        decimal_significand);
    qo_bool_t    handled = qo_false;

    // We need floor(z / divisor). Check if candidate floor(x / divisor) is correct.
    // Condition is r > deltai OR (r == deltai AND xi is not integer)
    if (r > deltai)
    {
        // Fall through to step 3
    }
    else if (r == deltai)
    {
        // Check fractional part of X: compute_mul_parity(two_fc - (shorter?1:2), ...)
        // We need xi^(f) > 0 for the smaller divisor candidate to be chosen.
        qo_uint32_t  parity_arg = two_fc - (shorter_interval ? 1 : 2);
        ComputeMulParityResult  x_result = compute_mul_parity_f32(parity_arg ,
            cache , beta);
        if (!x_result.parity)    // Check if xi^(f) == 0
        // Fall through to step 3
        {
        }
        else {
            // xi^(f) > 0, so the current candidate (larger divisor) is correct
            result.significand = decimal_significand;
            result.exponent = minus_k + kappa + 1;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
            result.may_have_trailing_zeros = qo_true;
#endif
            handled = qo_true;
        }
    }
    else {   // r < deltai
        // Current candidate (larger divisor) is correct.
        result.significand = decimal_significand;
        result.exponent = minus_k + kappa + 1;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_true;
#endif
        handled = qo_true;
    }

    // Step 3: Find the significand with the smaller divisor
    if (!handled)
    {
        const remainder_t  small_divisor = (remainder_t) pow_u32(10 , kappa);
        // Target is floor(z / small_divisor).
        // We have floor(x / big_divisor) = decimal_significand, and remainder r = zi -
        // ds*big_divisor.
        // We need floor(zi / small_divisor) = floor( (ds*big_divisor + r) / small_divisor)
        // = floor( ds*10 + r / small_divisor ) = ds*10 + floor(r / small_divisor).
        decimal_significand = (decimal_significand * 10) +
                              small_division_by_pow10_u32(r , kappa);

        result.significand = decimal_significand;
        result.exponent = minus_k + kappa;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_false;
#endif
    }

    // Apply sign policy
#if SIGN_POLICY == SIGN_RETURN
    result.is_negative = signed_significand_is_negative(s);
#endif
    // Apply trailing zero policy
#if (TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE || \
        TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE_COMPACT)
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
    if (result.may_have_trailing_zeros)
 # endif
    {
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE
        remove_trailing_zeros_f32_remove(&result.significand ,
            &result.exponent);
 # else // REMOVE_COMPACT
        remove_trailing_zeros_f32_remove_compact(&result.significand ,
            &result.exponent);
 # endif
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_false;
 # endif
    }
#endif

    return result;
}
// TODO: Implement _f64 versions of compute_nearest, compute_left_closed_directed,
// compute_right_closed_directed
// These will be very similar, using the _f64 helper functions and types.
QO_FORCE_INLINE QO_DecimalFPExtract
compute_nearest_f64(
    SignedSignificandBits  s ,
    qo_int32_t             exponent_bits
) {
    // --- Implementation similar to f32 version, using f64 types and functions ---
    const FormatParams * params  = &params_f64;
    const IEEE754Format * format = params->format;
    QO_DecimalFPExtract  result;

    typedef qo_uint64_t      carrier_uint_t;
    typedef remainder_f64_t  remainder_t;
    typedef dec_exp_f64_t    decimal_exponent_t;
    typedef shift_f64_t      shift_t;

    qo_uint64_t  two_fc = signed_significand_remove_sign_bit_and_shift(s);
    qo_int32_t   binary_exponent = exponent_bits;

    qo_bool_t    is_normal = (binary_exponent != 0);
    qo_bool_t    shorter_interval_case = qo_false;

    if (is_normal)
    {
        binary_exponent += format->exponent_bias - format->significand_bits;
        if (two_fc == 0)
        {
            shorter_interval_case = qo_true;
        }
        else {
            two_fc |= (UINT64_C(1) << (format->significand_bits + 1));
        }
    }
    else {
        binary_exponent = format->min_exponent - format->significand_bits;
    }

    if (shorter_interval_case)
    {
        IntervalType  interval_type = get_shorter_interval_f64(s);

        decimal_exponent_t const  minus_k =
            (decimal_exponent_t) floor_log10_pow2_minus_log10_4_over_3(
                binary_exponent
            );
        shift_t const  beta = (shift_t) (binary_exponent +
            floor_log2_pow10(-minus_k));

        u128_t  cache;
#if CACHE_POLICY == CACHE_FULL
        cache = get_cache_f64_full(-minus_k);
#else // COMPACT
        cache = get_cache_f64_compact(-minus_k , beta);
#endif
        qo_uint64_t  xi =
            compute_left_endpoint_for_shorter_interval_case_f64(cache , beta);
        qo_uint64_t  zi =
            compute_right_endpoint_for_shorter_interval_case_f64(cache , beta);

        if (!interval_type.include_right &&
            is_right_endpoint_integer_shorter_interval(binary_exponent ,
                params))
        {
            --zi;
        }
        if (!interval_type.include_left ||
            !is_left_endpoint_integer_shorter_interval(binary_exponent ,
                params))
        {
            ++xi;
        }

        carrier_uint_t  decimal_significand = divide_by_pow10_u64(zi , 1);

        if (decimal_significand * 10 >= xi)   // No overflow concern here
        {
            result.significand = decimal_significand;
            result.exponent = minus_k + 1;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
            result.may_have_trailing_zeros = qo_true;
#endif
        }
        else {
            decimal_significand =
                compute_round_up_for_shorter_interval_case_f64(cache , beta);
            if (prefer_round_down_b2d(decimal_significand) &&
                binary_exponent >=
                params->shorter_interval_tie_lower_threshold &&
                binary_exponent <= params->shorter_interval_tie_upper_threshold)
            {
                --decimal_significand;
            }
            else if (decimal_significand < xi)
            {
                ++decimal_significand;
            }
            result.significand = decimal_significand;
            result.exponent = minus_k;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
            result.may_have_trailing_zeros = qo_false;
#endif
        }
    }
    else {   // Normal interval case
        IntervalType  interval_type = get_normal_interval_f64(s);
        int  kappa = params->kappa;

        decimal_exponent_t const  minus_k =
            (decimal_exponent_t) (floor_log10_pow2(binary_exponent) - kappa);
        shift_t const  beta = (shift_t) (binary_exponent +
            floor_log2_pow10(-minus_k));

        u128_t  cache;
#if CACHE_POLICY == CACHE_FULL
        cache = get_cache_f64_full(-minus_k);
#else // COMPACT
        cache = get_cache_f64_compact(-minus_k , beta);
#endif

        remainder_t const  deltai = (remainder_t) compute_delta_f64(cache ,
            beta);
        ComputeMulResult   z_result = compute_mul_f64(
            (qo_uint64_t) ((two_fc | 1) << beta) , cache
        );

        const remainder_t  big_divisor = (remainder_t) pow_u64(10 , kappa + 1);
        const remainder_t  small_divisor = (remainder_t) pow_u64(10 , kappa);

        carrier_uint_t  decimal_significand =
            divide_by_pow10_u64(z_result.integer_part , kappa + 1);
        remainder_t  r = (remainder_t) (z_result.integer_part -
            (qo_uint64_t) big_divisor * decimal_significand);
        qo_bool_t    handled = qo_false;

        if (r < deltai)
        {
            if (r == 0 && !z_result.is_integer && !interval_type.include_right)
            {
#if BINARY_TO_DECIMAL_ROUNDING_POLICY == BINARY_TO_DECIMAL_ROUNDING_DO_NOT_CARE
                --decimal_significand;
                result.significand = decimal_significand * 10 + 9;
                result.exponent = minus_k + kappa;
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
                result.may_have_trailing_zeros = qo_false;
 # endif
                handled = qo_true;
#else
                --decimal_significand;
                r = big_divisor;
#endif
            }
            else {
                result.significand = decimal_significand;
                result.exponent = minus_k + kappa + 1;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
                result.may_have_trailing_zeros = qo_true;
#endif
                handled = qo_true;
            }
        }
        else if (r == deltai)
        {
            ComputeMulParityResult  x_result =
                compute_mul_parity_f64((qo_uint64_t) (two_fc - 1) , cache ,
                    beta);
            if (!(x_result.parity ||
                  (x_result.is_integer && interval_type.include_left)))
            {
                result.significand = decimal_significand;
                result.exponent = minus_k + kappa + 1;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
                result.may_have_trailing_zeros = qo_true;
#endif
                handled = qo_true;
            }
        }

        if (!handled)
        {
            decimal_significand *= 10;
#if BINARY_TO_DECIMAL_ROUNDING_POLICY == BINARY_TO_DECIMAL_ROUNDING_DO_NOT_CARE
            if (!interval_type.include_right)
            {
                remainder_t  r_copy = r;
                if (check_divisibility_and_divide_by_pow10_u64_inplace(
                    &
                    r_copy , kappa
                    ) && z_result.is_integer)
                {
                    decimal_significand += r_copy - 1;
                }
                else {
                    decimal_significand += small_division_by_pow10_u64(r ,
                        kappa);
                }
            }
            else {
                decimal_significand += small_division_by_pow10_u64(r , kappa);
            }
#else
            remainder_t  dist = r;
            remainder_t  delta_diff = (deltai >
                small_divisor) ? (deltai - small_divisor) : 0;
            if ((delta_diff / 2) > dist)
            {
                dist = 0;
            }
            else{ dist -= (delta_diff / 2);
            }

            remainder_t  quot = small_division_by_pow10_u64(dist , kappa);
            remainder_t  rem_dist = dist - quot * small_divisor;
            decimal_significand += quot;

            if (rem_dist > small_divisor / 2)
            {
                ++decimal_significand;
            }
            else if (rem_dist == small_divisor / 2)
            {
                ComputeMulParityResult  y_result =
                    compute_mul_parity_f64(two_fc , cache , beta);
                qo_bool_t  approx_y_parity = ((quot % 2) != 0);
                if (kappa == 1)
                {
                    approx_y_parity = !approx_y_parity;              // Only 10^1/2 is odd
                }
                if (y_result.parity != approx_y_parity)
                {
                    ++decimal_significand;
                }
                else {
                    if (!prefer_round_down_b2d(decimal_significand) &&
                        y_result.is_integer)
                    {
                        ++decimal_significand;
                    }
                }
            }
#endif
            result.significand = decimal_significand;
            result.exponent = minus_k + kappa;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
            result.may_have_trailing_zeros = qo_false;
#endif
        }
    } // End normal interval case

#if SIGN_POLICY == SIGN_RETURN
    result.is_negative = signed_significand_is_negative(s);
#endif

#if (TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE || \
        TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE_COMPACT)
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
    if (result.may_have_trailing_zeros)
 # endif
    {
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE
        remove_trailing_zeros_f64_remove(&result.significand ,
            &result.exponent);
 # else // REMOVE_COMPACT
        remove_trailing_zeros_f64_remove_compact(&result.significand ,
            &result.exponent);
 # endif
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_false;
 # endif
    }
#endif

    return result;
}
// (Continuing from the previous response)
// --- Core Algorithm Implementation (f64 directed modes) ---
QO_FORCE_INLINE QO_DecimalFPExtract
compute_left_closed_directed_f64(
    SignedSignificandBits  s ,
    qo_int32_t             exponent_bits
) {
    const FormatParams * params  = &params_f64;
    const IEEE754Format * format = params->format;
    QO_DecimalFPExtract  result;

    typedef qo_uint64_t      carrier_uint_t;
    typedef remainder_f64_t  remainder_t;
    typedef dec_exp_f64_t    decimal_exponent_t;
    typedef shift_f64_t      shift_t;

    qo_uint64_t  two_fc = signed_significand_remove_sign_bit_and_shift(s);
    qo_int32_t   binary_exponent = exponent_bits;

    qo_bool_t    is_normal = (binary_exponent != 0);

    if (is_normal)
    {
        binary_exponent += format->exponent_bias - format->significand_bits;
        two_fc |= (UINT64_C(1) << (format->significand_bits + 1));
    }
    else {
        binary_exponent = format->min_exponent - format->significand_bits;
    }

    // Step 1: Schubfach multiplier calculation
    int  kappa = params->kappa;
    decimal_exponent_t const  minus_k =
        (decimal_exponent_t) (floor_log10_pow2(binary_exponent) - kappa);
    shift_t const  beta = (shift_t) (binary_exponent +
        floor_log2_pow10(-minus_k));

    u128_t  cache;
#if CACHE_POLICY == CACHE_FULL
    cache = get_cache_f64_full(-minus_k);
#else // COMPACT
    cache = get_cache_f64_compact(-minus_k , beta);
#endif

    remainder_t const  deltai = (remainder_t) compute_delta_f64(cache , beta);
    // Compute ceil(x) = ceil (2f_c * cache / 2^(cache_bits))
    ComputeMulResult   x_result = compute_mul_f64(
        (qo_uint64_t) (two_fc <<
                beta) , cache
    );

    if (!x_result.is_integer)
    {
        x_result.integer_part++;
    }
    qo_uint64_t  xi = x_result.integer_part;

    // Step 2: Try larger divisor
    const remainder_t  big_divisor = (remainder_t) pow_u64(10 , kappa + 1);
    carrier_uint_t  decimal_significand = divide_by_pow10_u64(xi , kappa + 1);
    remainder_t  r = (remainder_t) (xi - (qo_uint64_t) big_divisor *
        decimal_significand);
    qo_bool_t    handled = qo_false;

    if (r != 0)
    {
        decimal_significand++;
        r = big_divisor - r; // Remainder for step 3 check
    }
    else {
        // r == 0, xi is divisible by big_divisor. Candidate is decimal_significand.
        // We need zi^(f) > 0 for this candidate to be valid.
    }

    if (r > deltai)
    {
        // Fall through to step 3
    }
    else if (r == deltai)
    {
        // Check fractional part of Z: compute_mul_parity(two_fc + 2, ...)
        // We need zi^(f) > 0. If zi^(f) == 0, fall through.
        ComputeMulParityResult  z_result =
            compute_mul_parity_f64((qo_uint64_t) (two_fc + 2) , cache , beta);
        if (z_result.parity || z_result.is_integer)    // Check if zi^(f) == 0
        // Fall through to step 3
        {
        }
        else {
            // zi^(f) > 0, larger divisor candidate is correct.
            result.significand = decimal_significand;
            result.exponent = minus_k + kappa + 1;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
            result.may_have_trailing_zeros = qo_true;
#endif
            handled = qo_true;
        }
    }
    else {   // r < deltai
        // Larger divisor candidate is correct.
        result.significand = decimal_significand;
        result.exponent = minus_k + kappa + 1;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_true;
#endif
        handled = qo_true;
    }

    // Step 3: Find the significand with the smaller divisor
    if (!handled)
    {
        // Calculate ceil(xi / small_divisor) = ds_big * 10 - floor(r_adj / small_divisor)
        // where r_adj = (r==0 ? 0 : big_divisor-r) and r here is (big_divisor-r) from the
        // adjustment above.
        decimal_significand = (decimal_significand * 10) -
                              small_division_by_pow10_u64(r , kappa);

        result.significand = decimal_significand;
        result.exponent = minus_k + kappa;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_false;
#endif
    }

    // Apply sign policy
#if SIGN_POLICY == SIGN_RETURN
    result.is_negative = signed_significand_is_negative(s);
#endif
    // Apply trailing zero policy
#if (TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE || \
        TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE_COMPACT)
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
    if (result.may_have_trailing_zeros)
 # endif
    {
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE
        remove_trailing_zeros_f64_remove(&result.significand ,
            &result.exponent);
 # else // REMOVE_COMPACT
        remove_trailing_zeros_f64_remove_compact(&result.significand ,
            &result.exponent);
 # endif
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_false;
 # endif
    }
#endif

    return result;
}
QO_FORCE_INLINE QO_DecimalFPExtract
compute_right_closed_directed_f64(
    SignedSignificandBits  s ,
    qo_int32_t             exponent_bits
) {
    const FormatParams * params  = &params_f64;
    const IEEE754Format * format = params->format;
    QO_DecimalFPExtract  result;

    typedef qo_uint64_t      carrier_uint_t;
    typedef remainder_f64_t  remainder_t;
    typedef dec_exp_f64_t    decimal_exponent_t;
    typedef shift_f64_t      shift_t;

    qo_uint64_t  two_fc = signed_significand_remove_sign_bit_and_shift(s);
    qo_int32_t   binary_exponent  = exponent_bits;
    qo_bool_t    shorter_interval = qo_false;

    qo_bool_t    is_normal = (binary_exponent != 0);

    if (is_normal)
    {
        if (two_fc == 0 && binary_exponent != 1)
        {
            shorter_interval = qo_true;
        }
        binary_exponent += format->exponent_bias - format->significand_bits;
        two_fc |= (UINT64_C(1) << (format->significand_bits + 1));
    }
    else {
        binary_exponent = format->min_exponent - format->significand_bits;
    }

    // Step 1: Schubfach multiplier calculation
    int  kappa = params->kappa;
    qo_int32_t  effective_binary_exponent = binary_exponent -
                                            (shorter_interval ? 1 : 0);
    decimal_exponent_t const  minus_k =
        (decimal_exponent_t) (floor_log10_pow2(effective_binary_exponent) -
            kappa);
    shift_t const  beta = (shift_t) (binary_exponent +
        floor_log2_pow10(-minus_k));

    u128_t  cache;
#if CACHE_POLICY == CACHE_FULL
    cache = get_cache_f64_full(-minus_k);
#else // COMPACT
    cache = get_cache_f64_compact(-minus_k , beta);
#endif

    shift_t  delta_beta = beta - (shorter_interval ? 1 : 0);
    remainder_t const  deltai = (remainder_t) compute_delta_f64(cache ,
        delta_beta);

    // Compute zi = floor(x) = floor( 2f_c * cache / 2^cache_bits )
    ComputeMulResult  z_result = compute_mul_f64(
        (qo_uint64_t) (two_fc <<
                beta) , cache
    );
    qo_uint64_t  zi = z_result.integer_part;

    // Step 2: Try larger divisor
    const remainder_t  big_divisor = (remainder_t) pow_u64(10 , kappa + 1);
    carrier_uint_t  decimal_significand = divide_by_pow10_u64(zi , kappa + 1);
    remainder_t  r = (remainder_t) (zi - (qo_uint64_t) big_divisor *
        decimal_significand);
    qo_bool_t    handled = qo_false;

    // We need floor(z / divisor). Candidate is floor(x / divisor).
    // Condition: r > deltai OR (r == deltai AND xi^(f) > 0)
    if (r > deltai)
    {
        // Fall through to step 3
    }
    else if (r == deltai)
    {
        // Check fractional part of X: compute_mul_parity(two_fc - (shorter?1:2), ...)
        // If xi^(f) == 0, fall through. Otherwise, larger divisor candidate is correct.
        qo_uint64_t  parity_arg = two_fc - (shorter_interval ? 1 : 2);
        ComputeMulParityResult  x_result = compute_mul_parity_f64(parity_arg ,
            cache , beta);
        if (!x_result.parity)    // Check if xi^(f) == 0
        // Fall through to step 3
        {
        }
        else {
            // xi^(f) > 0, larger divisor candidate is correct.
            result.significand = decimal_significand;
            result.exponent = minus_k + kappa + 1;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
            result.may_have_trailing_zeros = qo_true;
#endif
            handled = qo_true;
        }
    }
    else {   // r < deltai
        // Larger divisor candidate is correct.
        result.significand = decimal_significand;
        result.exponent = minus_k + kappa + 1;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_true;
#endif
        handled = qo_true;
    }

    // Step 3: Find the significand with the smaller divisor
    if (!handled)
    {
        const remainder_t  small_divisor = (remainder_t) pow_u64(10 , kappa);
        // Target is floor(zi / small_divisor) = ds_big * 10 + floor(r / small_divisor)
        decimal_significand = (decimal_significand * 10) +
                              small_division_by_pow10_u64(r , kappa);

        result.significand = decimal_significand;
        result.exponent = minus_k + kappa;
#if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_false;
#endif
    }

    // Apply sign policy
#if SIGN_POLICY == SIGN_RETURN
    result.is_negative = signed_significand_is_negative(s);
#endif
    // Apply trailing zero policy
#if (TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE || \
        TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE_COMPACT)
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
    if (result.may_have_trailing_zeros)
 # endif
    {
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REMOVE
        remove_trailing_zeros_f64_remove(&result.significand ,
            &result.exponent);
 # else // REMOVE_COMPACT
        remove_trailing_zeros_f64_remove_compact(&result.significand ,
            &result.exponent);
 # endif
 # if TRAILING_ZERO_POLICY == TRAILING_ZERO_REPORT
        result.may_have_trailing_zeros = qo_false;
 # endif
    }
#endif

    return result;
}
//------------------------------------------------------------------------------
// Internal Dispatcher Functions
//------------------------------------------------------------------------------
QO_FORCE_INLINE QO_DecimalFPExtract
to_decimal_f32_internal(
    SignedSignificandBits  s ,
    qo_int32_t             exponent_bits
) {
    RoundingModeTag  tag = get_rounding_mode_tag(s);

    if (tag == ROUNDING_MODE_TAG_TO_NEAREST)
    {
        return compute_nearest_f32(s , exponent_bits);
    }
    else if (tag == ROUNDING_MODE_TAG_LEFT_CLOSED_DIRECTED)
    {
        return compute_left_closed_directed_f32(s , exponent_bits);
    }
    else {   // RIGHT_CLOSED_DIRECTED
        return compute_right_closed_directed_f32(s , exponent_bits);
    }
}
QO_FORCE_INLINE QO_DecimalFPExtract
to_decimal_f64_internal(
    SignedSignificandBits  s ,
    qo_int32_t             exponent_bits
) {
    RoundingModeTag  tag = get_rounding_mode_tag(s);

    if (tag == ROUNDING_MODE_TAG_TO_NEAREST)
    {
        return compute_nearest_f64(s , exponent_bits);
    }
    else if (tag == ROUNDING_MODE_TAG_LEFT_CLOSED_DIRECTED)
    {
        return compute_left_closed_directed_f64(s , exponent_bits);
    }
    else {   // RIGHT_CLOSED_DIRECTED
        return compute_right_closed_directed_f64(s , exponent_bits);
    }
}
//------------------------------------------------------------------------------
// Public API Functions
//------------------------------------------------------------------------------
/**
 * @brief Converts a finite, non-zero float to its shortest, round-trippable
 *        decimal representation based on the compile-time policies.
 *
 * @param x The input float value. Must be finite and non-zero.
 * @return A QO_DecimalFPExtract struct containing the decimal significand,
 *         exponent, and potentially sign/trailing zero info based on policies.
 *
 * @pre x must be finite (not infinity or NaN).
 * @pre x must not be zero.
 * @note Behavior for zero, infinity, or NaN is undefined.
 * @note Policies (rounding, cache, etc.) are set via macros defined BEFORE
 *       including this header. See configuration macros near the top.
 */
QO_FORCE_INLINE QO_DecimalFPExtract
to_decimal(
    float  x
) {
    FloatBits  br = make_floatbits_float(x);
    assert(floatbits_is_finite(br) && "Input must be finite.");
    assert(floatbits_is_nonzero(br) && "Input must not be zero.");

    qo_int32_t  exponent_bits = floatbits_extract_exponent_bits(br);
    SignedSignificandBits  s  = floatbits_remove_exponent_bits(br);

    return to_decimal_f32_internal(s , exponent_bits);
}
// (Continuing from the previous response)
/**
 * @brief Converts a finite, non-zero double to its shortest, round-trippable
 *        decimal representation based on the compile-time policies.
 *
 * @param x The input double value. Must be finite and non-zero.
 * @return A QO_DecimalFPExtract struct containing the decimal significand,
 *         exponent, and potentially sign/trailing zero info based on policies.
 *
 * @pre x must be finite (not infinity or NaN).
 * @pre x must not be zero.
 * @note Behavior for zero, infinity, or NaN is undefined.
 * @note Policies (rounding, cache, etc.) are set via macros defined BEFORE
 *       including this header. See configuration macros near the top.
 */
QO_FORCE_INLINE QO_DecimalFPExtract
to_decimal_double(
    double  x
) {                                         // Use a distinct name to avoid C overloading issues
    FloatBits  br = make_floatbits_double(x);
    assert(floatbits_is_finite(br) && "Input must be finite.");
    assert(floatbits_is_nonzero(br) && "Input must not be zero.");

    qo_int32_t  exponent_bits = floatbits_extract_exponent_bits(br);
    SignedSignificandBits  s  = floatbits_remove_exponent_bits(br);

    return to_decimal_f64_internal(s , exponent_bits);
}
