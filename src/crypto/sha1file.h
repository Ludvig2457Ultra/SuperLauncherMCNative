#pragma once
#include <string>

// Обёртки вокруг чистого asm sl_sha1 (см. src/asm/sha1.asm).
namespace sl {

// SHA-1 произвольного буфера в HEX (строчными буквами).
std::string sha1_hex(const unsigned char* data, unsigned long long len);

// SHA-1 файла потоково. Возвращает пустую строку при ошибке чтения.
std::string sha1_file(const std::string& path);

} // namespace sl