/*Muhammad Ali Heavyweight Boxing decompression*/

/*
 * Muhammad Ali Heavyweight Boxing (GB) decompressor
 * Disassembled from ROM at $03B5 - $051B
 * Game Boy CPU: LR35902
 *
 * Calling convention (inferred from call sites):
 *   HL = source (compressed data in ROM bank, e.g. bank 5, addr $75AA or $7E7A)
 *   DE = dest (RAM, e.g. $C158, $9020, etc.)
 *   Terminates when flag byte == 0
 *   Whole bank (0x4000 bytes) should be available for back-references
 *   that go before the start of compressed data (offset > current output).
 *   In that case this implementation reads from the bank's prefix
 *   (or returns 0 if not available).
 *
 * Compression types (flag byte):
 *  0xxxxxxx:
 *    00xxxxxx (0x00-0x3F): literal copy, len = flag & 0x3F
 *    01xxxxxx (0x40-0x7F):
 *      0100xxxx (0x40-0x4F): delta single byte,  3 + (flag&0x0F) bytes
 *      0101xxxx (0x50-0x5F): delta 16-bit,       4 + 2*(flag&0x0F) bytes
 *      0110xxxx (0x60-0x6F): RLE single byte,    3 + (flag&0x0F) bytes
 *      0111xxxx (0x70-0x7F): RLE 2-byte pattern, 4 + 2*(flag&0x0F) bytes
 *  1xxxxxxx:
 *    10xxxxxx (0x80-0xBF): copy 3 bytes, offset = (flag&0x3F)+3
 *    110xxxxx (0xC0-0xDF): copy 4+((flag&0x1C)>>2), offset = ((flag&0x03)<<8|next)+3
 *    111xxxxx (0xE0-0xFF): copy 5+extra, offset = ((flag&0x1F)<<8|next)+3
 *
 * Disassembly at $03B5:
 * 03B5: 7E        LD A,(HL)
 * 03B6: B7        OR A
 * 03B7: C8        RET Z
 * 03B8: CB7F      BIT 7,A
 * 03BA: CA4F04    JP Z,$044F ; 0xxxxxxx
 * 03BD: CB77      BIT 6,A
 * 03BF: CA3404    JP Z,$0434 ; 10xxxxxx
 * 03C2: CB6F      BIT 5,A
 * 03C4: C2FF03    JP NZ,$03FF ; 111xxxxx
 * ; 110xxxxx
 * 03C7: E603      AND $03
 * 03C9: 47        LD B,A
 * 03CA: 2A        LD A,(HL+)   ; reload flag
 * 03CB: E61C      AND $1C
 * 03CD: F5        PUSH AF
 * 03CE: 2A        LD A,(HL+)   ; offset low
 * 03CF: C603      ADD $03
 * 03D1: 4F        LD C,A
 * 03D2: 78        LD A,B
 * 03D3: CE00      ADC $00
 * 03D5: 47        LD B,A       ; BC = offset
 * 03D6: E5        PUSH HL
 * 03D7: 7B        LD A,E
 * 03D8: 91        SUB C
 * 03D9: 6F        LD L,A
 * 03DA: 7A        LD A,D
 * 03DB: 98        SBC B
 * 03DC: 67        LD H,A       ; HL = DE - BC
 * 03DD: 2A        LD A,(HL+)  \
 * 03DE: 12        LD (DE),A    \
 * 03DF: 13        INC DE        > copy 4
 * 03E0: 2A        LD A,(HL+)   /
 * 03E1: 12        LD (DE),A  /
 * ... (4 times)
 * 03E9: C1        POP BC
 * 03EA: F1        POP AF
 * 03EB: C5        PUSH BC
 * 03EC: B7        OR A
 * 03ED: 280C      JR Z,$03FB
 * 03EF: CB3F      SRL A
 * 03F1: CB3F      SRL A
 * 03F3: 47        LD B,A
 * 03F4: 2A        LD A,(HL+)
 * 03F5: 12        LD (DE),A
 * 03F6: 13        INC DE
 * 03F7: 05        DEC B
 * 03F8: C2F403    JP NZ,$03F4
 * 03FB: E1        POP HL
 * 03FC: C3B503    JP $03B5
 * ; 111xxxxx at 03FF, 10xxxxxx at 0434, etc. (see full disassembly in comments)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define BANK_SIZE 0x4000

 // Decompress from src to dst. bank is whole bank (0x4000) for history before start.
 // src_offset is offset within bank where compressed data starts (0..0x3FFF)
 // dst is output buffer, returns decompressed length. src is advanced to terminator+1.
size_t decompress(const uint8_t* bank, size_t src_offset, uint8_t* dst, size_t dst_capacity,
    const uint8_t* bank_prefix, size_t prefix_len) {
    size_t src = src_offset;
    size_t dst_pos = 0;
    // For back-references that go before dst start, we need bank history.
    // We treat dst history as: [bank_prefix (if provided) + already decompressed]
    // For simplicity, if offset > dst_pos, we read from bank_prefix if available, else 0.
    while (1) {
        if (src >= BANK_SIZE) break;
        uint8_t flag = bank[src++];
        if (flag == 0) break; // terminator
        if ((flag & 0x80) == 0) { // 0xxxxxxx
            if ((flag & 0x40) == 0) { // 00xxxxxx literal
                size_t n = flag & 0x3F;
                if (src + n > BANK_SIZE) n = BANK_SIZE - src;
                if (dst_pos + n > dst_capacity) break;
                memcpy(dst + dst_pos, bank + src, n);
                src += n;
                dst_pos += n;
            }
            else { // 01
                if ((flag & 0x20) == 0) {
                    if ((flag & 0x10) == 0) { // 0100
                        uint8_t c = flag & 0x0F;
                        // need at least 2 bytes history
                        if (dst_pos < 2) {
                            // Not enough history - pad with 0 (should not happen after preload)
                            // Original code would read from (DE-2) which would be before buffer
                            // We use bank prefix if available
                            for (size_t i = 0; i < 3 + c; i++) {
                                uint8_t v = 0;
                                // delta would be 0
                                if (dst_pos + i < dst_capacity) dst[dst_pos + i] = v;
                            }
                            dst_pos += 3 + c;
                            continue;
                        }
                        int delta = (int)dst[dst_pos - 1] - (int)dst[dst_pos - 2];
                        uint8_t val = dst[dst_pos - 1];
                        for (size_t i = 0; i < 3 + c; i++) {
                            val = (val + delta) & 0xFF;
                            dst[dst_pos++] = val;
                        }
                    }
                    else { // 0101
                        uint8_t c = flag & 0x0F;
                        if (dst_pos < 4) {
                            // pad
                            for (size_t i = 0; i < 2 + c; i++) {
                                if (dst_pos + 1 < dst_capacity) {
                                    dst[dst_pos++] = 0;
                                    dst[dst_pos++] = 0;
                                }
                            }
                            continue;
                        }
                        uint16_t v1 = dst[dst_pos - 4] | (dst[dst_pos - 3] << 8);
                        uint16_t v2 = dst[dst_pos - 2] | (dst[dst_pos - 1] << 8);
                        int16_t delta = (int16_t)(v2 - v1);
                        uint16_t hl = v2;
                        for (size_t i = 0; i < 2 + c; i++) {
                            hl = (hl + delta) & 0xFFFF;
                            dst[dst_pos++] = hl & 0xFF;
                            dst[dst_pos++] = (hl >> 8) & 0xFF;
                        }
                    }
                }
                else {
                    if ((flag & 0x10) == 0) { // 0110
                        uint8_t c = flag & 0x0F;
                        if (dst_pos < 1) {
                            for (size_t i = 0; i < 3 + c; i++) dst[dst_pos++] = 0;
                            continue;
                        }
                        uint8_t v = dst[dst_pos - 1];
                        for (size_t i = 0; i < 3 + c; i++) dst[dst_pos++] = v;
                    }
                    else { // 0111
                        uint8_t c = flag & 0x0F;
                        if (dst_pos < 2) {
                            for (size_t i = 0; i < 2 + c; i++) {
                                dst[dst_pos++] = 0;
                                dst[dst_pos++] = 0;
                            }
                            continue;
                        }
                        uint8_t lo = dst[dst_pos - 2];
                        uint8_t hi = dst[dst_pos - 1];
                        for (size_t i = 0; i < 2 + c; i++) {
                            dst[dst_pos++] = lo;
                            dst[dst_pos++] = hi;
                        }
                    }
                }
            }
        }
        else { // 1xxxxxxx
            if ((flag & 0x40) == 0) { // 10
                size_t offset = (flag & 0x3F) + 3;
                for (int i = 0; i < 3; i++) {
                    uint8_t v;
                    if (offset > dst_pos) {
                        // before start - read from bank prefix if available
                        if (bank_prefix && prefix_len > 0) {
                            // offset beyond dst_pos goes into prefix
                            // prefix is before dst, so index = prefix_len - (offset - dst_pos)
                            size_t idx = prefix_len - (offset - dst_pos) + i;
                            // Need to handle i offset correctly: for each i, source is dst_pos - offset + i
                            // If dst_pos - offset + i < 0, it's in prefix
                            long src_idx = (long)dst_pos - (long)offset + i;
                            if (src_idx < 0) {
                                size_t p_idx = prefix_len + src_idx; // src_idx negative
                                v = (p_idx < prefix_len) ? bank_prefix[p_idx] : 0;
                            }
                            else {
                                v = dst[src_idx];
                            }
                        }
                        else {
                            v = 0;
                        }
                    }
                    else {
                        v = dst[dst_pos - offset];
                    }
                    dst[dst_pos++] = v;
                    // For next iteration, offset stays same, but dst_pos increased, so -offset moves forward (overlapping copy)
                }
            }
            else {
                if ((flag & 0x20) == 0) { // 110
                    if (src >= BANK_SIZE) break;
                    uint8_t nxt = bank[src++];
                    size_t offset = ((flag & 0x03) << 8 | nxt) + 3;
                    size_t len = 4 + ((flag & 0x1C) >> 2);
                    for (size_t i = 0; i < len; i++) {
                        uint8_t v;
                        if (offset > dst_pos) {
                            if (bank_prefix && prefix_len > 0) {
                                long src_idx = (long)dst_pos - (long)offset + (long)i;
                                if (src_idx < 0) {
                                    size_t p_idx = prefix_len + src_idx;
                                    v = (p_idx < prefix_len) ? bank_prefix[p_idx] : 0;
                                }
                                else {
                                    v = dst[src_idx];
                                }
                            }
                            else v = 0;
                        }
                        else {
                            v = dst[dst_pos - offset];
                        }
                        dst[dst_pos++] = v;
                    }
                }
                else { // 111
                    if (src + 1 >= BANK_SIZE) break;
                    uint8_t nxt = bank[src++];
                    uint8_t extra = bank[src++];
                    size_t offset = ((flag & 0x1F) << 8 | nxt) + 3;
                    size_t len = 5 + extra;
                    for (size_t i = 0; i < len; i++) {
                        uint8_t v;
                        if (offset > dst_pos) {
                            if (bank_prefix && prefix_len > 0) {
                                long src_idx = (long)dst_pos - (long)offset + (long)i;
                                if (src_idx < 0) {
                                    size_t p_idx = prefix_len + src_idx;
                                    v = (p_idx < prefix_len) ? bank_prefix[p_idx] : 0;
                                }
                                else {
                                    v = dst[src_idx];
                                }
                            }
                            else v = 0;
                        }
                        else {
                            v = dst[dst_pos - offset];
                        }
                        dst[dst_pos++] = v;
                    }
                }
            }
        }
    }
    return dst_pos;
}