#include "zip.h"
#include "inflate.h"
#include "paths.h"
#include "log.h"
#ifdef _WIN32
#include "win.h"
#endif
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>

namespace sl {

using std::string;

static uint32_t rd32(const unsigned char* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const unsigned char* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

bool is_zip(const string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    unsigned char sig[4];
    size_t n = fread(sig, 1, 4, f);
    fclose(f);
    return n == 4 && sig[0] == 'P' && sig[1] == 'K' && sig[2] == 3 && sig[3] == 4;
}

// --- raw inflate (свой, без zlib) ---

int zip_extract_all(const string& zip_path, const string& dest_dir) {
    FILE* f = fopen(zip_path.c_str(), "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
#ifdef _WIN32
    long long fsize = _ftelli64(f);
#else
    long long fsize = (long long)ftell(f);
#endif
    if (fsize <= 0 || fsize > 512LL * 1024 * 1024) { fclose(f); return 0; }
    fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> buf((size_t)fsize);
    size_t nr = fread(buf.data(), 1, (size_t)fsize, f);
    fclose(f);
    if (nr != (size_t)fsize) return 0;

    long long eocd = -1;    for (long long i = fsize - 22; i >= 0 && i >= fsize - 65557; i--) {
        if (buf[(size_t)i] == 0x50 && buf[(size_t)i + 1] == 0x4b &&
            buf[(size_t)i + 2] == 0x05 && buf[(size_t)i + 3] == 0x06) {
            eocd = i; break;
        }
    }
    if (eocd < 0) return 0;
    uint16_t nentries = rd16(buf.data() + eocd + 10);
    uint32_t cd_off = rd32(buf.data() + eocd + 16);

    int extracted = 0;
    size_t pos = (size_t)cd_off;
    for (int e = 0; e < nentries && pos + 46 <= buf.size(); e++) {
        if (rd32(buf.data() + pos) != 0x02014b50) break;
        uint16_t method = rd16(buf.data() + pos + 10);
        uint32_t comp = rd32(buf.data() + pos + 20);
        uint32_t uncomp = rd32(buf.data() + pos + 24);
        uint16_t name_len = rd16(buf.data() + pos + 28);
        uint16_t extra_len = rd16(buf.data() + pos + 30);
        uint16_t comment_len = rd16(buf.data() + pos + 32);
        uint32_t local_off = rd32(buf.data() + pos + 42);
        string name((const char*)buf.data() + pos + 46, name_len);

        bool is_dir = !name.empty() && name.back() == '/';
        if (!is_dir && comp <= (size_t)fsize) {
            size_t lp = (size_t)local_off;
            if (lp + 30 <= buf.size()) {
                uint16_t lname_len = rd16(buf.data() + lp + 26);
                uint16_t lextra_len = rd16(buf.data() + lp + 28);
                size_t data_off = lp + 30 + lname_len + lextra_len;
                if (data_off + comp <= buf.size()) {
                    string out_path = path_join(dest_dir, name);
                    if (method == 0) {
                        mkdirs(parent_dir(out_path));
                        FILE* o = fopen(out_path.c_str(), "wb");
                        if (o) {
                            fwrite(buf.data() + data_off, 1, comp, o);
                            fclose(o);
                            extracted++;
                        }
                    } else if (method == 8) {
                        std::vector<unsigned char> outu = inflate_raw(buf.data() + data_off, comp, uncomp);
                        if (!outu.empty() && outu.size() == uncomp) {
                            mkdirs(parent_dir(out_path));
                            FILE* o = fopen(out_path.c_str(), "wb");
                            if (o) {
                                fwrite(outu.data(), 1, outu.size(), o);
                                fclose(o);
                                extracted++;
                            }
                        }
                    }
                }
            }
        }
        pos += 46 + name_len + extra_len + comment_len;
    }
    return extracted;
}

} // namespace sl