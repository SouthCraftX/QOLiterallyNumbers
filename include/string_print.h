#pragma once
#define __QOBD_STRING_PRINT_H__

#include <stdio.h>
#include <string.h>

#include "../../QuickOK-Zero/include/qozero.h"
#include "int_cvt.h"

struct QOBD_StringPrintContext
{
    qo_cstring_t  current_pos;
    qo_size_t     remaining_size;
    
};
typedef struct QOBD_StringPrintContext QOBD_StringPrintContext;
static inline
void
__qobd_str_append_update_pos(
    QOBD_StringPrintContext * ctx ,
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
        ctx->current_pos += (ctx->remaining_size > 0 ? ctx->remaining_size - 1 : 0);
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
    QOBD_StringPrintContext * ctx ,
    qo_ccstring_t             source
) {
    if (!source)
    {
        source = "(null)";
    }

    qo_ssize_t  written_size = snprintf(ctx->current_pos , ctx->remaining_size , "%s" , source);
    __qobd_str_append_update_pos(ctx , written_size);
}
// For other characters that are not single byte length, you should use the above
// function to append the string. Try '字' won't work.
static inline
void
__qobd_str_append_char8(
    QOBD_StringPrintContext * ctx ,
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
__qobd_str_append_u32_no_trim(
    QOBD_StringPrintContext * ctx ,
    uint32_t                  value
) {

}
