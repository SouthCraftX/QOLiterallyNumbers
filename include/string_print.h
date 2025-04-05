#pragma once
#define __QOBD_STRING_PRINT_H__

#include <stdio.h>
#include <string.h>

#include "../../QuickOK-Zero/include/qozero.h"
#include "int_cvt.h"

typedef enum
{
    QO_INT_BASE_BINARY ,
    QO_INT_BASE_OCTAL ,
    QO_INT_BASE_DECIMAL ,
    QO_INT_BASE_HEXADECIMAL
} QO_IntFormatingBase;

struct _QO_IntFormatingMode
{
    qo_int8_t  minimum_width;          //< Prefix with zeros if necessary
    qo_int8_t  precision;
    struct
    {
        qo_bool_t  always_show_sign : 1;
        qo_bool_t  show_base_prefix : 1;
        qo_bool_t  uppercase        : 1; //< whether prefix and digits are uppercase
    } switches;
    QO_IntFormatingBase  base;
};
typedef struct _QO_IntFormatingMode QO_IntFormatingMode;
#define QO_INT_FORMATTING_MODE_DEFAULT \
        (QO_IntFormatingMode) { \
            .base = QO_INT_BASE_DECIMAL , \
            .precision = 0 , \
            .minimum_width = 0 , \
            .switches = { \
                .always_show_sign = qo_false , \
                .show_base_prefix = qo_false , \
                .uppercase = qo_false \
            } \
        }

typedef enum
{
    QO_FP_SCIENTIFIC_AUTO ,
    QO_FP_SCIENTIFIC_ALWAYS ,
    QO_FP_SCIENTIFIC_NEVER
} QO_FPScientificMode;

// typedef enum
// {

// } QO_FPRormatingMode;

struct _QO_FPFormatingMode
{
    struct
    {
        qo_bool_t  always_show_sign : 1;
        qo_bool_t  show_base_prefix : 1;
        qo_bool_t  uppercase        : 1; //< whether prefix and digits are uppercase
        qo_bool_t  show_point       : 1; //< show '.' in the output
    } switches;
    QO_FPScientificMode  scientific_mode;
    QO_IntFormatingBase  base;
};
typedef struct _QO_FPFormatingMode QO_FPFormatingMode;
#define QO_FP_FORMATTING_MODE_DEFAULT \
        (QO_FPFormatingMode) { \
            .base = QO_INT_BASE_DECIMAL , \
            .scientific_mode = QO_FP_SCIENTIFIC_AUTO , \
            .switches = { \
                .always_show_sign = qo_false , \
                .show_base_prefix = qo_false , \
                .uppercase  = qo_false , \
                .show_point = qo_true \
            } \
        }

struct _QO_PointerFormatingMode
{
    struct
    {
        qo_bool_t  trim_front_zeros : 1;      //<
        qo_bool_t  uppercase        : 1; //< set it false to use lowercase
        qo_bool_t  show_base_prefix : 1;      //<  like 0x, 0b, etc.
        qo_bool_t  better_null      : 1; //< print NULL as "nullptr"
    } switches;
    QO_IntFormatingBase  base;
};
typedef struct _QO_PointerFormatingMode QO_PointerFormatingMode;
#define QO_POINTER_FORMATTING_MODE_DEFAULT \
        (QO_PointerFormatingMode) { \
            .base = QO_INT_BASE_HEXADECIMAL , \
            .switches = { \
                .trim_front_zeros = qo_true , \
                .uppercase = qo_true , \
                .show_base_prefix = qo_true , \
                .better_null = qo_true \
            } \
        }

struct _QOBD_StringPrintResult
{
    qo_size_t    written_size;
    qo_bool_t    truncated;
};
typedef struct _QOBD_StringPrintResult QOBD_StringPrintResult;

struct _QOBD_StringPrintFormat
{
    QO_IntFormatingMode      int_fmt_mode;
    QO_FPFormatingMode       fp_fmt_mode;
    QO_PointerFormatingMode  ptr_fmt_mode;
};
typedef struct _QOBD_StringPrintFormat QOBD_StringPrintFormat;
#define QOBD_STRING_PRINT_FORMAT_DEFAULT \
        (QOBD_StringPrintFormat) { \
            .int_fmt_mode = QO_INT_FORMATTING_MODE_DEFAULT , \
            .fp_fmt_mode = QO_FP_FORMATTING_MODE_DEFAULT , \
            .ptr_fmt_mode = QO_POINTER_FORMATTING_MODE_DEFAULT \
        }


struct __QOBD_StringPrintContext
{
    qo_cstring_t            current_pos;
    qo_ssize_t              remaining_size;
    QOBD_StringPrintFormat  format;
};
typedef struct __QOBD_StringPrintContext _QOBD_StringPrintContext;

static inline
void
__qobd_set_int_fmt_mode(
    _QOBD_StringPrintContext * ctx ,
    QO_IntFormatingMode       mode
) {
    if (ctx)
    {
        ctx->int_fmt_mode = mode;
    }
}

static inline
void
__qobd_set_fp_fmt_mode(
    _QOBD_StringPrintContext * ctx ,
    QO_FPFormatingMode        mode
) {
    if (ctx)
    {
        ctx->fp_fmt_mode = mode;
    }
}

static inline
void
__qobd_str_append_update_pos(
    _QOBD_StringPrintContext * ctx ,
    qo_ssize_t                written_size
) {
    if (!ctx || !ctx->current_pos || !ctx->remaining_size)
    {
        if (ctx)
        {
            ctx->remaining_size = 0;
        }
        return;
    }
    if (written_size < 0)
    {
        ctx->remaining_size = 0;
        return;
    }
    size_t  written_u = (size_t) written_size;
    if (written_u >= ctx->remaining_size)
    {
        ctx->current_pos += (ctx->remaining_size >
            0 ? ctx->remaining_size - 1 : 0);
        ctx->remaining_size = 0;
        *ctx->current_pos = '\0';
    }
    else {
        ctx->current_pos += written_u;
        ctx->remaining_size -= written_u;
    }
}
static inline
void
__qobd_str_append_str(
    _QOBD_StringPrintContext * ctx ,
    qo_ccstring_t             source
) {
    if (!source)
    {
        source = "(null)";
    }

    qo_ssize_t  written_size = snprintf(ctx->current_pos , ctx->remaining_size ,
        "%s" , source);
    __qobd_str_append_update_pos(ctx , written_size);
}
// For other characters that are not single byte length, you should use the above
// function to append the string. Try '字' won't work.
static inline
void
__qobd_str_append_char8(
    _QOBD_StringPrintContext * ctx ,
    char                      character
) {
    if (!ctx || !ctx->current_pos || !ctx->remaining_size)
    {
        *ctx->current_pos = character;
        *++ctx->current_pos = '\0';
        --ctx->remaining_size;
    }
}
static inline
void
__qobd_str_append_hex32_trim(
    _QOBD_StringPrintContext * ctx ,
    uint32_t                  value
) {
}
static inline
void
__qobd_str_append_hex16_trim(
    _QOBD_StringPrintContext * ctx ,
    uint16_t                  value
) {
}
static inline
void
__qobd_str_append_u8_trim(
    _QOBD_StringPrintContext * ctx ,
    uint8_t                   value
) {
}
static inline
void
__qobd_str_append_i32_trim(
    _QOBD_StringPrintContext * ctx ,
    int32_t                   value
) {
}
static inline
void
__qobd_str_append_i16_trim(
    _QOBD_StringPrintContext * ctx ,
    int16_t                   value
) {
}
static inline
void
__qobd_str_append_i8_trim(
    _QOBD_StringPrintContext * ctx ,
    int8_t                    value
) {
}
#define __QOBD_STR_APPEND_CTX_DISPATCH(ctx , value) \
        _Generic((value) , \
    char * : __qobd_str_append_str , \
    char const * : __qobd_str_append_str , \
    char : __qobd_str_append_char8 )

// *INDENT-OFF*
#define __QOBD_FOR_EACH_0(ctx , action , ...) 
#define __QOBD_FOR_EACH_1(ctx , action , _1) action(ctx , _1);
#define __QOBD_FOR_EACH_2(ctx , action , _1 , _2) action(ctx , _1); __QOBD_FOR_EACH_1(ctx , action , _2)
#define __QOBD_FOR_EACH_3(ctx , action , _1 , _2 , _3) action(ctx , _1); __QOBD_FOR_EACH_2(ctx , action , _2 , _3)
#define __QOBD_FOR_EACH_4(ctx , action , _1 , _2 , _3 , _4) action(ctx , _1); __QOBD_FOR_EACH_3(ctx , action , _2 , _3 , _4)
#define __QOBD_FOR_EACH_5(ctx , action , _1 , _2 , _3 , _4 , _5) action(ctx , _1); __QOBD_FOR_EACH_4(ctx , action , _2 , _3 , _4 , _5)
#define __QOBD_FOR_EACH_6(ctx , action , _1 , _2 , _3 , _4 , _5 , _6) action(ctx , _1); __QOBD_FOR_EACH_5(ctx , action , _2 , _3 , _4 , _5 , _6)
#define __QOBD_FOR_EACH_7(ctx , action , _1 , _2 , _3 , _4 , _5 , _6 , _7) action(ctx , _1); __QOBD_FOR_EACH_6(ctx , action , _2 , _3 , _4 , _5 , _6 , _7)
#define __QOBD_FOR_EACH_8(ctx , action , _1 , _2 , _3 , _4 , _5 , _6 , _7 , _8) action(ctx , _1); __QOBD_FOR_EACH_7(ctx , action , _2 , _3 , _4 , _5 , _6 , _7 , _8)
#define __QOBD_FOR_EACH_9(ctx , action , _1 , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9) action(ctx , _1); __QOBD_FOR_EACH_8(ctx , action , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9)
#define __QOBD_FOR_EACH_10(ctx , action , _1 , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9 , _10) action(ctx , _1); __QOBD_FOR_EACH_9(ctx , action , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9 , _10)
#define __QOBD_FOR_EACH_11(ctx , action , _1 , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9 , _10 , _11) action(ctx , _1); __QOBD_FOR_EACH_10(ctx , action , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9 , _10 , _11)
#define __QOBD_FOR_EACH_12(ctx , action , _1 , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9 , _10 , _11 , _12) action(ctx , _1); __QOBD_FOR_EACH_11(ctx , action , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9 , _10 , _11 , _12)
#define __QOBD_FOR_EACH_13(ctx , action , _1 , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9 , _10 , _11 , _12 , _13) action(ctx , _1); __QOBD_FOR_EACH_12(ctx , action , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9 , _10 , _11 , _12 , _13)
#define __QOBD_FOR_EACH_14(ctx , action , _1 , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9 , _10 , _11 , _12 , _13 , _14) action(ctx , _1); __QOBD_FOR_EACH_13(ctx , action , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9 , _10 , _11 , _12 , _13 , _14)
#define __QOBD_FOR_EACH_15(ctx , action , _1 , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9 , _10 , _11 , _12 , _13 , _14 , _15) action(ctx , _1); __QOBD_FOR_EACH_14(ctx , action , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9 , _10 , _11 , _12 , _13 , _14 , _15)
// *INDENT-ON*

#define __QOBD_COUNT_ARGS__(_0 , _1 , _2 , _3 , _4 , _5 , _6 , _7 , _8 , _9 , _10 , _11 , _12 , _13 , _14 , _15 , N , ...) N
#define __QOBD_COUNT_ARGS(...) __QOBD_COUNT_ARGS__(0 , ##__VA_ARGS__ , 15 , 14 , 13 , 12 , 11 , 10 , 9 , 8 , 7 , 6 , 5 , 4 , 3 , 2 , 1)

#define __QOBD_CONCAT__(x , y) x##y
#define __QOBD_CONCAT(x , y) __QOBD_CONCAT__(x , y)

#define FOR_EACH_DISPATCHER(N , ctx , action , ...) __QOBD_CONCAT(__QOBD_FOR_EACH_ , N)(ctx , action , ##__VA_ARGS__)

#define __QOBD_FOR_EACH(ctx , action , ...) __QOBD_FOR_EACH_DISPATCH

#define qobd_str_print(target , limit , p_result , ...) \
    do { \
        if (!target) \
        { \
            break; \
        } \
        _QOBD_StringPrintContext  ctx; \
        ctx.format = QOBD_STRING_PRINT_FORMAT_DEFAULT; \
        ctx.current_pos = target; \
        ctx.remaining_size = limit; \
        if (ctx.remaining_size > 0) \
        { \
            *ctx.current_pos = '\0'; \


