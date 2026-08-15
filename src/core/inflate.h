#pragma once
#include <string>
#include <vector>

namespace sl {
// Распаковка raw-deflate (RFC 1951). src = сжатый поток, expected_size подсказка.
// Возвращает пустой вектор при ошибке.
std::vector<unsigned char> inflate_raw(const unsigned char* src, size_t src_len,
                                       size_t expected_size);
} // namespace sl