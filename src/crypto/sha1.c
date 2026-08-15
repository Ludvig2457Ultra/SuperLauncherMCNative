// sha1.c — SHA-1 (FIPS 180-1), портируемая C-реализация.
// Функция sl_sha1(data, len, out[20]) — ABI-совместима с прежней asm-версией.
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static uint32_t rol32(uint32_t v, int n) {
    return (v << n) | (v >> (32 - n));
}

void sl_sha1(const unsigned char* data, unsigned long long len, unsigned char out[20]) {
    uint32_t h0 = 0x67452301u, h1 = 0xEFCDAB89u, h2 = 0x98BADCFEu,
             h3 = 0x10325476u, h4 = 0xC3D2E1F0u;

    // Общая длина с дополнением: 0x80, нули до длины, затем 8 байт длины.
    // Если данные не влезают в последний блок с хвостом длины (len%64 в 56..63),
    // нужен дополнительный блок — иначе длина затирает 0x80.
    uint64_t bitlen = (uint64_t)len * 8;
    unsigned long long rem = len % 64;
    unsigned long long zeros = (rem < 56) ? (55 - rem) : (119 - rem);
    unsigned long long nblocks = (len + 1 + zeros + 8) / 64;

    unsigned char* block = (unsigned char*)malloc(64);
    if (!block) return;

    unsigned long long off = 0;
    for (unsigned long long b = 0; b < nblocks; b++) {
        memset(block, 0, 64);
        unsigned long long remain = (off < len) ? (len - off) : 0;
        if (remain >= 64) {
            memcpy(block, data + off, 64);
        } else {
            if (remain > 0) memcpy(block, data + off, (size_t)remain);
            // 0x80 кладём только в блок, где заканчиваются данные (len % 64)
            if (b == len / 64 && remain <= 63) block[remain] = 0x80;
            if (b == nblocks - 1) {
                for (int i = 0; i < 8; i++)
                    block[63 - i] = (unsigned char)(bitlen >> (8 * i));
            }
        }
        off += 64;

        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
                   ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
        for (int i = 16; i < 80; i++)
            w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32_t a = h0, b2 = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (b2 & c) | ((~b2) & d); k = 0x5A827999u; }
            else if (i < 40) { f = b2 ^ c ^ d; k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b2 & c) | (b2 & d) | (c & d); k = 0x8F1BBCDCu; }
            else { f = b2 ^ c ^ d; k = 0xCA62C1D6u; }
            uint32_t t = rol32(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol32(b2, 30); b2 = a; a = t;
        }
        h0 += a; h1 += b2; h2 += c; h3 += d; h4 += e;
    }
    free(block);

    unsigned char* o = out;
    for (int i = 0; i < 4; i++) {
        o[i] = (unsigned char)(h0 >> (24 - 8 * i));
        o[4 + i] = (unsigned char)(h1 >> (24 - 8 * i));
        o[8 + i] = (unsigned char)(h2 >> (24 - 8 * i));
        o[12 + i] = (unsigned char)(h3 >> (24 - 8 * i));
        o[16 + i] = (unsigned char)(h4 >> (24 - 8 * i));
    }
}
