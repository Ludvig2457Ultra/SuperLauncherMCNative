#pragma once
#include <string>
#include <cstdint>

namespace sl {

// SHA-256 хэш байтового буфера. Возвращает 64-символьную hex-строку.
std::string sha256_hex(const void* data, size_t len);
inline std::string sha256_hex(const std::string& s) {
    return sha256_hex(s.data(), s.size());
}

} // namespace sl