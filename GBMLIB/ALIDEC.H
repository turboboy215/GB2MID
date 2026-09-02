/*Muhammad Ali Heavyweight Boxing decompression*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

size_t decompress(const uint8_t* bank, size_t src_offset, uint8_t* dst, size_t dst_capacity,
    const uint8_t* bank_prefix, size_t prefix_len);