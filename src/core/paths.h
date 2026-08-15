#pragma once
#include <string>

// Пути и низкоуровневые файловые операции.
namespace sl {

// %APPDATA% (Roaming)
std::wstring appdata_path();

// %APPDATA%/SuperLauncher — папка данных приложения
std::wstring app_root();
std::string app_root_utf8();

// %APPDATA%/.minecraft — рабочая папка Minecraft по умолчанию
std::string minecraft_directory();

// user_data/instances.json (относительно app_root или CWD — см. config.cpp)
std::string instances_file();
std::string settings_file();

// Поиск java.exe: пользовательский путь, затем общие места установки, затем PATH.
// find_java_path() — самая новая из найденных.
std::string find_java_path();

// Поиск Java с учётом требуемой мажорной версии (из version.json "javaVersion"):
// выбирается наименьшая установленная версия >= required_major, а если такой нет —
// самая новая. required_major <= 0 — самый младший из установленных.
std::string find_java_path_for(int required_major);

// Работа с файлами
bool file_exists(const std::string& path);
bool file_exists_w(const std::wstring& path);
std::string read_file_text(const std::string& path);
bool write_file_text(const std::string& path, const std::string& text);
bool mkdirs(const std::string& path);
bool mkdirs_w(const std::wstring& path);
bool copy_file(const std::string& src, const std::string& dst);
bool remove_file(const std::string& path);
long long file_size(const std::string& path);
std::string path_join(const std::string& a, const std::string& b);
std::string parent_dir(const std::string& path);
std::string file_name(const std::string& path);

} // namespace sl