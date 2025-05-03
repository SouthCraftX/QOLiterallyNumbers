#pragma once
#define __QOBD_AVX2_TRIM0_H__

#include "qozero.h"
#include <string.h>
#include <immintrin.h>

#if defined (__cplusplus)
extern "C" {
#endif // __cplusplus
static extern inline
int
__qodb_find_first_set_bit(
    qo_uint32_t  mask
) {
#if defined (_MSC_VER)
    qo_uint64_t index;
    return _BitScanForward(&index , mask) ? (int) index : -1;
#elif defined(__GNUC__) || defined(__clang__)
    # if defined (__BMI__)
    return mask ? (int) _tzcnt_u32(mask) : -1;
    # else
    return mask ? (int) __builtin_ctz(mask) : -1;
    # endif // __BMI__
#endif // _MSC_VER
}
extern inline
qo_ccstring_t
qo_trim_str_front_zeros_avx2(
    qo_ccstring_t str
) {
    if (!str)
    {
        return NULL;
    }

    qo_ccstring_t ptr  = str;
    const qo_size_t  len = strlen(str);
    qo_ccstring_t  end = str + len;

    qo_ccstring_t  simd_limit = end - 31;

    // Check if AVX2 processing is possible and worthwhile
    if (ptr < simd_limit)
    {
        // Create a 256-bit vector containing thirty-two '0' characters
        const __m256i  zeros_vector = _mm256_set1_epi8('0');

        while ((uintptr_t) ptr < (uintptr_t) simd_limit)
        {
            // Load 32 bytes from the current pointer (unaligned load)
            __m256i  chunk = _mm256_loadu_si256((const __m256i *) ptr);

            // Compare the chunk with the '0' vector.
            // Result bytes are 0xFF if equal ('0'), 0x00 otherwise.
            __m256i  comparison_mask_vec = _mm256_cmpeq_epi8(chunk ,
                zeros_vector);

            // Create a 32-bit integer mask from the most significant bit of each byte.
            // 1 if the byte was '0', 0 otherwise.
            int  mask = _mm256_movemask_epi8(comparison_mask_vec);

            // Check if all 32 bytes were '0'
            if ((qo_uint32_t) mask == 0xFFFFFFFF)
            {
                // All 32 bytes were '0'. Advance pointer by 32 and continue loop.
                ptr += 32;
            }
            else {
                // Found a non-'0' byte within this 32-byte chunk.
                // Find the first bit that is 0 in the 'mask'.
                // This corresponds to the first set bit in the *inverted* mask (~mask).
                int  index_in_chunk =
                    __qodb_find_first_set_bit(~(qo_uint32_t) mask);
                if (index_in_chunk >= 0)
                {
                    // Index must be < 32 for AVX2 block size
                    return ptr + index_in_chunk; // Return pointer to the first non-'0'
                }
                else {
                    // Should not happen if mask != 0xFFFFFFFF. Safeguard: Fall through.
                    break; // Exit SIMD loop and proceed to scalar part
                }
            }
        }
    }

    // 1. Try one more 32-byte AVX2 block if possible (handles 32-63 bytes remaining)
    qo_ccstring_t simd_limit_32 = end - 31;
    if ((uintptr_t) ptr < (uintptr_t) simd_limit_32)
    {
        __m256i  chunk = _mm256_loadu_si256((const __m256i *) ptr);
        __m256i  comparison_mask_vec = _mm256_cmpeq_epi8(chunk ,
            zeros_vector_256);
        int  mask = _mm256_movemask_epi8(comparison_mask_vec);
        if ((qo_uint32_t) mask != 0xFFFFFFFF)
        {
            int  index_in_chunk = find_first_set_bit(~(qo_uint32_t) mask);
            return (index_in_chunk >= 0) ? ptr + index_in_chunk : ptr;  // Found non-'0'
        }
        // If this block was all zeros, advance ptr
        ptr += 32;
    }

    // 2. Handle >= 16 bytes using SSE2
    qo_ccstring_t simd_limit_16 = end - 15;
    if ((uintptr_t) ptr < (uintptr_t) simd_limit_16)
    {
        const __m128i  zeros_vector_128 = _mm_set1_epi8('0');
        __m128i  chunk = _mm_loadu_si128((const __m128i *) ptr);
        __m128i  cmp_mask_vec = _mm_cmpeq_epi8(chunk , zeros_vector_128);
        int  mask = _mm_movemask_epi8(cmp_mask_vec);
        if ((qo_uint32_t) mask != 0xFFFF)
        {
            // Use find_first_set_bit, ensuring it works correctly for 16 bits
            // (The existing one should be fine if mask input is zero-extended or if it internally
            // handles size)
            // Masking the input ensures we only look at the relevant 16 bits.
            int  index = find_first_set_bit(~(qo_uint32_t) mask & 0xFFFF);
            return (index >= 0) ? ptr + index : ptr; // Found non-'0'
        }
        ptr += 16; // All zeros, advance
    }

    // 3. Handle >= 8 bytes (using scalar check for simplicity and alignment safety)
    // Check if the next 8 bytes (if they exist) are all zeros.
    if (end - ptr >= 8)
    {
        // Use memcpy for potentially unaligned read into a qo_uint64_t
        qo_uint64_t  val64;
        memcpy(&val64 , ptr , sizeof(qo_uint64_t));
        // Check against '00000000' (adjust for endianness if needed, but direct check is safer)
        if (val64 == 0x3030303030303030ULL)   // Check if all bytes are '0' (ASCII 0x30)
        {
            ptr += 8; // All zeros, advance
        }
        else {
            // Non-zero found within these 8 bytes. Scan them.
            // (Using a loop is simpler than complex bit tricks here)
            int  i = 0;
            while (i < 8 && ptr[i] == '0')
            {
                i++;
            }
            return ptr + i; // Return pointer to first non-'0'
        }
    }

    // Now ptr points to the start of the final 0-7 bytes

    // 4. Handle >= 4 bytes
    if (end - ptr >= 4)
    {
        qo_uint32_t  val32;
        memcpy(&val32 , ptr , sizeof(qo_uint32_t));
        if (val32 == 0x30303030UL)  // Check if all 4 bytes are '0'
        {
            ptr += 4;
        }
        else {
            int  i = 0;
            while (i < 4 && ptr[i] == '0')
            {
                i++;
            }
            return ptr + i;
        }
    }

    // Now ptr points to the start of the final 0-3 bytes

    // 5. Handle remaining 0-3 bytes with a simple loop
    while (ptr < end && *ptr == '0')
    {
        ptr++;
    }

    return ptr;
}

#if defined (__cplusplus)
}
#endif // __cplusplus