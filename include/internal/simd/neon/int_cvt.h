#pragma once
#define __QO_INT_CVT_NEON_H__

#include <stdint.h>
#include <arm_neon.h>

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <arm_neon.h>

// tested working
extern inline void
qo_udec17_to_fixed_str_neon(
    qo_uint64_t    x,
    char*       buffer
) {
    assert(x < 100000000000000000ULL);
    
    // Split the number into high and low parts
    qo_uint64_t hi = x / 100000000;
    qo_uint64_t lo = x % 100000000;
    
    // Use scalar operations for the initial calculations with magic numbers
    const qo_uint64_t magic_lo = (0x00FFFFFFFFFFFFFF / 10000) + 1;
    const qo_uint64_t magic_hi = (0xFFFFFFFFFFFFFFFF / (100000000 >> 4)) + 1;
    
    lo = ((lo * magic_lo) & 0x00FFFFFF00000000) +
                (((lo >> 4) * magic_hi) >> 40) + 0x0000000100000001;
    hi = ((hi * magic_lo) & 0x00FFFFFF00000000) +
                (((hi >> 4) * magic_hi) >> 40) + 0x0000000100000001;
    
    // Split 64-bit values into 32-bit chunks for NEON processing
    qo_uint32_t lo_low = (qo_uint32_t)lo;
    qo_uint32_t lo_high = (qo_uint32_t)(lo >> 32);
    qo_uint32_t hi_low = (qo_uint32_t)hi;
    qo_uint32_t hi_high = (qo_uint32_t)(hi >> 32);
    
    // Load into NEON registers - we'll process all 4 32-bit values in parallel
    uint32x4_t values = {lo_low, lo_high, hi_low, hi_high};
    uint32x4_t mask = {0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF, 0x00FFFFFF};
    uint32x4_t not_mask = vmvnq_u32(mask);
    uint32x4_t out = {0x30303030, 0x30303030, 0x30303030, 0x30303030};
    
    // First round
    values = vmulq_n_u32(values, 10);
    uint32x4_t temp = vandq_u32(values, not_mask);
    temp = vshrq_n_u32(temp, 24);
    out = vorrq_u32(out, temp);
    
    // Second round
    values = vandq_u32(values, mask);
    values = vmulq_n_u32(values, 10);
    temp = vandq_u32(values, not_mask);
    temp = vshrq_n_u32(temp, 16);
    out = vorrq_u32(out, temp);
    
    // Third round
    values = vandq_u32(values, mask);
    values = vmulq_n_u32(values, 10);
    temp = vandq_u32(values, not_mask);
    temp = vshrq_n_u32(temp, 8);
    out = vorrq_u32(out, temp);
    
    // Fourth round
    values = vandq_u32(values, mask);
    values = vmulq_n_u32(values, 10);
    temp = vandq_u32(values, not_mask);
    out = vorrq_u32(out, temp);
    
    // Extract results and reconstruct 64-bit values
    qo_uint32_t out_array[4];
    vst1q_u32(out_array, out);
    
    qo_uint64_t out0 = ((qo_uint64_t)out_array[1] << 32) | out_array[0];
    qo_uint64_t out1 = ((qo_uint64_t)out_array[3] << 32) | out_array[2];
    
    // Write to buffer
    memcpy(&buffer[0], &out1, 8);
    memcpy(&buffer[8], &out0, 8);
    buffer[16] = '\0';
}

// tested working
// NEON-optimized implementation for converting a 64-bit integer to a 17-character hex string
extern inline 
void
__qo_hex64_to_untrimmed_str_common_neon(
    qo_uint64_t    x,
    char*       buffer,
    qo_ccstring_thex_table
) {
    // x = __builtin_bswap64(x);
// Pre-load the hex table into NEON registers if it's static
    // This assumes the hex table is the standard "0123456789ABCDEF"
    uint8x16_t hex_chars_0_to_15 = vld1q_u8(hex_table);

    // Extract bytes from the 64-bit value
    uint8x8_t bytes = vreinterpret_u8_u64(vdup_n_u64(x));

    // Extract nibbles and prepare indices
    uint8x8_t high_nibbles = vshr_n_u8(bytes, 4);
    uint8x8_t low_nibbles = vand_u8(bytes, vdup_n_u8(0x0F));

    // Convert to 16-bit vectors for wider processing
    uint8x16_t all_nibbles = vcombine_u8(high_nibbles, low_nibbles);

    // Rearrange nibbles to match output order
    uint8x16_t indices = {7,15,6,14,5,13,4,12,3,11,2,10,1,9,0,8};
    uint8x16_t ordered_nibbles = vqtbl1q_u8(all_nibbles, indices);

    // Perform table lookup directly with NEON
    uint8x16_t result_chars = vqtbl1q_u8(hex_chars_0_to_15, ordered_nibbles);

    // Store the result
    vst1q_u8((qo_uint8_t*)buffer, result_chars);
    buffer[16] = '\0';
}
