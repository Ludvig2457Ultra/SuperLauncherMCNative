#include "inflate.h"
#include <cstring>
#include <cstdint>

// Низкоуровневый raw-inflate без внешних зависимостей (RFC 1951).
namespace sl {

namespace {

struct BitReader {
    const unsigned char* p;
    size_t len;
    size_t pos = 0;
    uint32_t bitbuf = 0;
    int bitcnt = 0;

    unsigned read(int n) {
        while (bitcnt < n && pos < len) {
            bitbuf |= (uint32_t)p[pos++] << bitcnt;
            bitcnt += 8;
        }
        unsigned v = bitbuf & ((1u << n) - 1);
        bitbuf >>= n;
        bitcnt -= n;
        return v;
    }
    // выровнять на границу байта (для stored block)
    void align_byte() {
        unsigned shift = (unsigned)(bitcnt & 7);
        if (shift) {
            bitbuf >>= shift;
            bitcnt -= shift;
        }
    }
};

struct Huf {
    std::vector<int> left, right, sym;
    int root = 0;

    void init() {
        left.clear(); right.clear(); sym.clear();
        root = add_node();
    }
    int add_node() {
        left.push_back(-1); right.push_back(-1); sym.push_back(-1);
        return (int)left.size() - 1;
    }
    // вставка канонического кода code длиной len (биты MSB->LSB)
    void insert(unsigned code, int len, int symbol) {
        int node = root;
        for (int i = len - 1; i >= 0; i--) {
            bool bit = (code >> i) & 1;
            int& child = bit ? right[node] : left[node];
            if (child < 0) child = add_node();
            node = child;
        }
        sym[node] = symbol;
    }
    // декодирование одного символа
    int decode(BitReader& br) {
        int node = root;
        while (node >= 0 && sym[node] < 0) {
            int bit = (int)br.read(1);
            node = bit ? right[node] : left[node];
        }
        return node >= 0 ? sym[node] : -1;
    }
};

// сборка дерева из длин кодов (канонический алгоритм)
void build_huf(Huf& h, const unsigned* lengths, int count) {
    h.init();
    unsigned counts[16] = {0};
    for (int i = 0; i < count; i++) if (lengths[i] < 16) counts[lengths[i]]++;
    counts[0] = 0;
    unsigned next_code[16] = {0};
    unsigned code = 0;
    for (int bits = 1; bits <= 15; bits++) {
        code = (code + counts[bits - 1]) << 1;
        next_code[bits] = code;
    }
    for (int i = 0; i < count; i++) {
        unsigned len = lengths[i];
        if (len > 0 && len < 16) {
            h.insert(next_code[len], (int)len, i);
            next_code[len]++;
        }
    }
}

// таблицы длина/расстояние
static const unsigned L_BASE[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
static const unsigned L_EXTRA[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
static const unsigned D_BASE[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
static const unsigned D_EXTRA[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };

} // namespace

std::vector<unsigned char> inflate_raw(const unsigned char* src, size_t src_len,
                                       size_t expected_size) {
    std::vector<unsigned char> out;
    if (!src || src_len == 0) return out;
    if (expected_size > 0 && expected_size <= 512u * 1024 * 1024) out.reserve(expected_size);

    BitReader br;
    br.p = src; br.len = src_len;

    bool final = false;
    while (!final && br.pos < br.len) {
        final = br.read(1) != 0;
        int type = (int)br.read(2);
        if (type == 0) {
            br.align_byte();
            unsigned len = br.read(16);
            br.read(16); // nlen (контроль)
            if (br.pos + len > br.len) break;
            out.insert(out.end(), br.p + br.pos, br.p + br.pos + len);
            br.pos += len;
        } else if (type == 1 || type == 2) {
            Huf lit, dist;
            if (type == 2) {
                unsigned hlit = br.read(5) + 257;
                unsigned hdist = br.read(5) + 1;
                unsigned hclen = br.read(4) + 4;
                static const unsigned order[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
                unsigned clens[19] = {0};
                for (unsigned i = 0; i < hclen; i++) clens[order[i]] = br.read(3);
                Huf ccode;
                build_huf(ccode, clens, 19);
                unsigned lengths[286 + 30];
                memset(lengths, 0, sizeof(lengths));
                int n = (int)(hlit + hdist);
                int idx = 0;
                while (idx < n) {
                    int s = ccode.decode(br);
                    if (s < 0) return std::vector<unsigned char>();
                    if (s < 16) lengths[idx++] = (unsigned)s;
                    else if (s == 16) {
                        unsigned rep = 3 + br.read(2);
                        if (idx == 0) return std::vector<unsigned char>();
                        unsigned v = lengths[idx - 1];
                        while (rep-- > 0 && idx < n) lengths[idx++] = v;
                    } else if (s == 17) {
                        unsigned rep = 3 + br.read(3);
                        while (rep-- > 0 && idx < n) lengths[idx++] = 0;
                    } else { // 18
                        unsigned rep = 11 + br.read(7);
                        while (rep-- > 0 && idx < n) lengths[idx++] = 0;
                    }
                }
                build_huf(lit, lengths, (int)hlit);
                build_huf(dist, lengths + hlit, (int)hdist);
            } else {
                // фиксированные коды (RFC 1951 3.2.6)
                unsigned fixed_len[288];
                for (int i = 0; i < 144; i++) fixed_len[i] = 8;
                for (int i = 144; i < 256; i++) fixed_len[i] = 9;
                for (int i = 256; i < 280; i++) fixed_len[i] = 7;
                for (int i = 280; i < 288; i++) fixed_len[i] = 8;
                unsigned fixed_dist[30];
                for (int i = 0; i < 30; i++) fixed_dist[i] = 5;
                build_huf(lit, fixed_len, 288);
                build_huf(dist, fixed_dist, 30);
            }
            // блок данных
            for (;;) {
                int sym = lit.decode(br);
                if (sym < 0) return std::vector<unsigned char>();
                if (sym < 256) {
                    out.push_back((unsigned char)sym);
                } else if (sym == 256) {
                    break;
                } else if (sym <= 285) {
                    int li = sym - 257;
                    if (li > 28) return std::vector<unsigned char>();
                    unsigned len = L_BASE[li] + br.read((int)L_EXTRA[li]);
                    int ds = dist.decode(br);
                    if (ds < 0 || ds > 29) return std::vector<unsigned char>();
                    unsigned distv = D_BASE[ds] + br.read((int)D_EXTRA[ds]);
                    if (distv > out.size()) return std::vector<unsigned char>();
                    size_t start = out.size() - distv;
                    for (unsigned k = 0; k < len; k++) {
                        out.push_back(out[start + k]);
                    }
                } else {
                    return std::vector<unsigned char>();
                }
            }
        } else {
            break; // зарезервированный тип
        }
    }
    return out;
}

} // namespace sl