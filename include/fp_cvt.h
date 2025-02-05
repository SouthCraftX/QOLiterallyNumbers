#pragma once
#include <math.h>
#define __QO_FP_CVT_H__

#define QO_ENABLE_EXPERIMENTAL_CXX
#include "../../QOZero/include/qozero.h"

#define QO_FP64_SIGNIFICANT_BITS (52)
#define QO_FP64_EXPONENT_BITS (11)
#define QO_FP64_MIN_EXPONENT (-1022)
#define QO_FP64_MAX_EXPONENT (1023)
#define QO_FP64_BIAS (-1023)
#define QO_FP64_DEC_SIGNIFICANT_DIGITS (17)
#define QO_FP64_DEC_EXPONENT_DIGITS (3)
#define QO_FP64_MIN_K (-292)
#define QO_FP64_MAX_K (326)

#define QO_FP32_SIGNIFICANT_BITS (23)
#define QO_FP32_EXPONENT_BITS (8)
#define QO_FP32_MIN_EXPONENT (-126)
#define QO_FP32_MAX_EXPONENT (127)
#define QO_FP32_BIAS (-127)
#define QO_FP32_DEC_SIGNIFICANT_DIGITS (9)
#define QO_FP32_DEC_EXPONENT_DIGITS (2)
#define QO_FP32_MIN_K (-31)
#define QO_FP32_MAX_K (46)

#if defined(__cplusplus)
extern "C" {
#endif // __cplusplus

QO_GLOBAL_UNIQUE QO_NO_SIDE_EFFECTS
qo_unt32_t
qo_fp64_get_significand(
    qo_fp64_t value
) {
    return (qo_int32_t)(value * (1.0 / (1LL << QO_FP64_SIGNIFICANT_BITS)));
}

QO_GLOBAL_UNIQUE QO_NO_SIDE_EFFECTS
qo_int32_t
qo_fp64_get_exponent(
    qo_fp64_t value
) {
    return (qo_int32_t)(log2(value) + QO_FP64_BIAS);
}

QO_GLOBAL_UNIQUE QO_NO_SIDE_EFFECTS
qo_bool_t
qo_fp64_get_sign(
    qo_fp64_t value
) {
    qo_uint64_t temp;
    __builtin_memcpy(&temp, &value, sizeof(qo_fp64_t));
    return temp >> 63;
}

QO_GLOBAL_UNIQUE
qo_size_t //< Length of string
qo_fp64_to_str(
    qo_fp64_t       value ,
    qo_cstring_t    buffer 
) {
    qo_bool_t sign = qo_fp64_get_sign(value);
    qo_uint32_t significand = qo_fp64_get_significand(value);
    qo_int32_t exponent = qo_fp64_get_exponent(value);
    
    if(!isfinite(value))
    {
        if(!significand)
        {
            if (sign)
                *buffer++ = '-';
            __builtin_memcpy(buffer, "Inf", 3);
            return 3 + !!sign;
        }
        else {
            buffer[0] = 'N';
            buffer[1] = 'a';
            buffer[2] = 'N';
            buffer[3] = '\0';
            return 3;
        }
    }
    
    if (sign)
        *buffer++ = '-';


}

#if defined(__cplusplus)
}
#endif // __cplusplus

