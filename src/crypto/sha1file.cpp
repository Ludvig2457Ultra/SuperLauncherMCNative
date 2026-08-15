#include "sha1file.h"
#include <cstdio>

extern "C" void sl_sha1(const unsigned char* data, unsigned long long len, unsigned char out[20]);

namespace sl {

static const char* HEX = "0123456789abcdef";

std::string sha1_hex(const unsigned char* data, unsigned long long len) {
    unsigned char out[20];
    sl_sha1(data, len, out);
    std::string s(40, '0');
    for (int i = 0; i < 20; i++) {
        s[(size_t)i * 2] = HEX[out[i] >> 4];
        s[(size_t)i * 2 + 1] = HEX[out[i] & 0xF];
    }
    return s;
}

std::string sha1_file(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return std::string();
    unsigned char* buf = new unsigned char[65536];
    // Однопроходный SHA-1: кормим asm кусками (состояние не сохраняется между
    // вызовами — см. примечание ниже).
    // Так как sl_sha1 не умеет потоковый режим, для файлов > 1 блока используем
    // повторный вызов на каждом 64-байтном блоке через локальную копию? Нет.
    // Практичное решение: читаем весь файл в память (для библиотек Minecraft это
    // файлы до ~40 МБ; допустимо).
    // TODO: потоковый hasher на ассемблере для больших файлов.
    fseek(f, 0, SEEK_END);
    long long n = _ftelli64(f);
    fseek(f, 0, SEEK_SET);
    std::string result;
    if (n > 0) {
        unsigned char* mem = new unsigned char[(size_t)n];
        size_t rd = fread(mem, 1, (size_t)n, f);
        result = sha1_hex(mem, rd);
        delete[] mem;
    } else {
        result = sha1_hex(buf, 0);
    }
    delete[] buf;
    fclose(f);
    return result;
}

} // namespace sl