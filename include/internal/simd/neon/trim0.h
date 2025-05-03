#pragma once
#include <cstddef>
#define __QODB_NEON_TRIM0_H__

#include <stdint.h>
#include <arm_neon.h>
#include <string.h>

#if defined (__cplusplus)
extern "C" {
#endif // __cplusplus

extern inline
qo_ccstring_t
qo_trim_str_front_zeros_neon(
    qo_ccstring_t str
) {
    if (!str)
    {
        return NULL;
    }

    qo_ccstring_t ptr  = str;
    const qo_size_t  len = strlen(str);

    qo_ccstring_t  end = str + len;

    qo_ccstring_t  simd_limit = end - 15;
    const uint8x16_t  zero_v = vdupq_n_u8('0');
    while ((uintptr_t) ptr < (uintptr_t) simd_limit)
    {
        uint8x16_t  chunk = vld1q_u8((qo_uint8_t const *) ptr);
        uint8x16_t  comparison_mask_vec = vceqq_u8(chunk , zeros_v);
        qo_uint8_t  min_val_16 = vminvq_u8(comparison_mask_vec);

        if (min_val_16 == 0xFF)
        {
            // All 16 bytes were '0'. Advance and continue.
            ptr += 16;
        }
        else {
            // Found non-'0' within this 16-byte chunk. Check first 8 bytes.
            uint8x8_t  low_mask_8 = vget_low_u8(comparison_mask_vec);
            qo_uint8_t    min_val_low_8 = vminv_u8(low_mask_8);

            if (min_val_low_8 == 0xFF)
            {
                // First 8 bytes were '0', non-'0' must be in the high 8 bytes.
                ptr += 8; // Advance pointer past the first 8 zero bytes.
                // Scalar search within the remaining (up to) 8 bytes.
                int  i = 0;
                // Careful: check boundary as the block might end before 8 bytes from new ptr
                while (i < 8 && (ptr + i) < end && ptr[i] == '0')
                {
                    i++;
                }
                return ptr + i; // Return pointer to first non-'0' or end
            }
            else {
                // Non-'0' is within the first 8 bytes.
                // Scalar search within these first 8 bytes.
                int  i = 0;
                // No need to check end_ptr here as we know these 8 bytes are valid
                while (i < 8 && ptr[i] == '0')
                {
                    i++;
                }
                // Since min_val_low_8 != 0xFF, we are guaranteed to find a non-'0' within these 8
                // bytes
                // unless the non-'0' was somehow past end_ptr, which shouldn't happen based on
                // initial load.
                return ptr + i;
            }
        }
    }

    // --- Scalar Part ---
    // Handle the remaining bytes (less than 16) or if the string was too short.
    while (ptr < end && *ptr == '0')
    {
        ptr++;
    }

    return ptr;
}
#if defined (__cplusplus)
}
#endif // __cplusplus
