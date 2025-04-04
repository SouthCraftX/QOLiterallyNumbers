#pragma once
#define __QOBD_FP_CVT_H__

#if defined (__cplusplus)
extern "C" {
#endif // defined (__cplusplus)


#include "../../QuickOK-Zero/include/qozero.h"

struct _QO_DecimalFPExtract
{
    qo_uint64_t significand;
    qo_int8_t   exponent;
    qo_uint8_t  significand_demical_digits;
    qo_uint8_t  exponent_digits;
    qo_bool_t   is_negative;

    /// @brief Whether the significand is a pure integer.
    /// @note  This field is only valid when QO_FP_ZERO_TRAILING_POLICY_REPORT 
    ///        is specified.
    qo_bool_t   may_have_trailing_zeros;
};
typedef struct _QO_DecimalFPExtract QO_DecimalFPExtract;

/// @remark Dragonbox use it to ensure the Round-trip Guarantee.
typedef enum 
{
    QO_FP_DEC2BIN_ROUND_NEAREST_TO_EVEN = 0,
    QO_FP_DEC2BIN_ROUND_NEAREST_TO_ODD ,
    QO_FP_DEC2BIN_ROUND_NEAREST_TO_POSITIVE_INF,
    QO_FP_DEC2BIN_ROUND_NEAREST_TO_NEGATIVE_INF,
    QO_FP_DEC2BIN_ROUND_NEAREST_TO_ZERO        ,
    QO_FP_DEC2BIN_ROUND_NEAREST_AWAY_FROM_ZERO  ,       
    QO_FP_DEC2BIN_ROUND_NEAREST_TO_EVEN_STATIC_BOUNDARY,
    QO_FP_DEC2BIN_ROUND_NEAREST_TO_ODD_STATIC_BOUNDARY,
    QO_FP_DEC2BIN_ROUND_NEAREST_TOWARD_POSITIVE_INF_STATIC_BOUNDARY ,
    QO_FP_DEC2BIN_ROUND_NEAREST_TOWARD_NEGATIVE_INF_STATIC_BOUNDARY ,
    QO_FP_DEC2BIN_ROUND_TOWARD_POSITIVE_INF ,
    QO_FP_DEC2BIN_ROUND_TOWARD_NEGATIVE_INF ,
    QO_FP_DEC2BIN_ROUND_TOWARD_ZERO ,
    QO_FP_DEC2BIN_ROUND_TOWARD_AWAY_FROM_ZERO 
} QO_FPDecimalToBinaryRoundNearestPolicy;

#define QO_DEFAULT_FP_DEC2BIN_ROUND_POLICY QO_FP_DEC2BIN_ROUND_NEAREST_TO_EVEN

/// @brief When tie breaking, use the specified policy.
/// @details When we're finding the shortest representation of a decimal
///         number, we may encounter a tie. For example, 0.1 can be represented
///         as 1e-1 or 1e-2. In this case, we need to break the tie. The
///         specified policy will be used to break the tie.
typedef enum 
{
    /// @brief Randomly choose one of the two representations. 
    /// @details    It depends on the implementation. Usually, the one which 
    ///             results in the best performance is chosen.
    QO_FP_BIN2DEC_ROUND_RANDOM = 0,

    /// @brief Round to the nearest even number.
    /// @details    It is name as IEEE-754 standard rounding mode and widely accepted.
    ///             We strongly recommend this policy for it best suits modern 
    ///             computing standards and practices.
    QO_FP_BIN2DEC_ROUND_TO_EVEN ,

    /// @brief Round to the nearest odd number.
    /// @details It is very rare and can result in statistical bias.
    QO_FP_BIN2DEC_ROUND_TO_ODD ,

    /// @brief Round to the nearest number away from zero.
    /// @details It is equal to "round half-up" and can result in positive bias.
    ///          It is suitable for conventional rounding (sometimes called
    ///          "commercial rounding" despite its not completely equality).
    QO_FP_BIN2DEC_ROUND_AWAY_FROM_ZERO ,

    /// @brief Round to the nearest number toward zero.
    /// @details It is equal to "round half-down" and can result in negative bias.
    ///          It is suitable for truncation. Usually it is very efficient.
    QO_FP_BIN2DEC_ROUND_TOWARD_ZERO 
} QO_FPBinaryToDecimalRoundNearestPolicy;

#define QO_DEFAULT_FP_BIN2DEC_ROUND_POLICY QO_FP_BIN2DEC_ROUND_TO_EVEN

typedef enum 
{
    /// @brief Do no check or remove trailing zeros, which produces the best 
    /// performance.
    QO_FP_ZERO_TRAILING_POLICY_NONE = 0,

    /// @brief Remove trailing zeros, which provides the simplest and most 
    /// intuitive result, useful for string formatting.
    QO_FP_ZERO_TRAILING_POLICY_REMOVE,

    /// @brief Simply report whether there are trailing zeros.
    QO_FP_ZERO_TRAILING_POLICY_REPORT
} QO_FPZeroTrailingPolicy;

QO_DecimalFPExtract
qo_fp64_extract_decimal(
    qo_fp64_t               value ,
    QO_FPDecimalToBinaryRoundNearestPolicy round_policy
);

QO_DecimalFPExtract
qo_fp32_extract_decimal(
    qo_fp32_t value ,
    QO_FPDecimalToBinaryRoundNearestPolicy round_policy
);

qo_size_t
qo_fp_extract_make_scientific_notation(
    QO_DecimalFPExtract *   extract,
    qo_cstring_t            buffer ,
    qo_size_t               buffer_size ,
    qo_uint8_t              significant_figures
) QO_NONNULL(1 , 2);

qo_size_t
qo_fp_extract_make_fixed_point(
    QO_DecimalFPExtract *   extract,
    qo_cstring_t            buffer ,
    qo_size_t               buffer_size ,
    qo_int8_t               precision 
) QO_NONNULL(1 , 2);

qo_bool_t
qo_str_to_fp32(
    qo_ccstring_t            str ,
    qo_fp32_t *              p_value
) QO_NONNULL(1 , 2);

#if defined (__cplusplus)
}
#endif // __cplusplus