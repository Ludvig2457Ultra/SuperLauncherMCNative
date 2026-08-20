#pragma once
#include <string>
#include <cstdint>

namespace sl {

// SHA-256 хэш байтового буфера. Возвращает 64-символьную hex-строку.
std::string sha256_hex(const void* data, size_t len);
inline std::string sha256_hex(const std::string& s) {
    return sha256_hex(s.data(), s.size());
}

// SHA-256 хэш файла (потоково, без загрузки в память). Возвращает
// 64-символьную hex-строку или пустую строку при ошибке чтения.
std::string sha256_file(const std::string& path);

} // namespace sl