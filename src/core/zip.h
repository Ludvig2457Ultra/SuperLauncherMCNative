#pragma once
#include <string>

// Минимальный ZIP-распаковщик (чтение центрального каталога).
namespace sl {

// Распаковать все члены архива в dest_dir. Возвращает число распакованных файлов.
int zip_extract_all(const std::string& zip_path, const std::string& dest_dir);

// Проверить, является ли файл валидным ZIP (по сигнатуре PK\x03\x04).
bool is_zip(const std::string& path);

} // namespace sl