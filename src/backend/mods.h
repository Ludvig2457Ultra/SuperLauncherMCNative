#pragma once
#include <string>
#include <vector>
#include <functional>

namespace sl {

namespace mods {

// Мод (search hit).
struct ModEntry {
    std::string id;
    std::string name;
    std::string description;
    long long downloads = 0;
    std::string icon_url;
    std::string author = "Unknown";
    std::string source = "modrinth";   // "modrinth" | "curseforge"
};

// Версия мода/сборки.
struct ModVersion {
    std::string id;          // modrinth version id
    std::string name;        // version_number / fileName
    std::string version;
    std::vector<std::string> game_versions;
    std::vector<std::string> loaders;
    long long downloads = 0;
    long long size = 0;
    std::string url;         // прямая ссылка на файл
    std::string filename;
    std::string date;
    long long file_id = 0;   // curseforge file id
    std::string source = "modrinth";
};

// Установленная сборка (registry).
struct InstalledPack {
    std::string name;
    std::string source;
    std::string version_id;
    std::string mc_version;
    std::string loader;
    std::string installed_at;
    std::vector<std::string> files;
    std::string backup;
};

// -------- поиск ----
std::vector<ModEntry> search_mods(const std::string& query, int limit, const std::string& source);
std::vector<ModEntry> search_modpacks(const std::string& query, int limit, const std::string& source);
std::vector<ModVersion> get_versions(const std::string& project_id, const std::string& source);
std::string version_download_url(const std::string& project_id, long long file_id);

// -------- установка ----
// Установить файл мода (url или у нас есть версия) в mods_dir.
bool download_mod_file(const std::string& url, const std::string& dst,
                       std::function<void(long long, long long)> progress = nullptr);

// Установка .mrpack/modpack. Успех -> true + имя сборки в out_name.
// callback(status, done, total) — прогресс 0..1.
bool install_modpack(const std::string& url_or_path, bool is_local,
                     const std::string& mc_dir,
                     std::function<void(const std::string&, float, float)> callback,
                     std::string& out_name, std::string& err);
// Установка локального .mrpack / .zip файла (эквивалент образа лок).
bool install_local_modpack(const std::string& path, const std::string& mc_dir,
                           std::function<void(const std::string&, float, float)> callback,
                           std::string& out_name, std::string& err);

// -------- реестр установленных сборок ----
std::string registry_path(const std::string& mc_dir);
std::vector<InstalledPack> get_installed_packs(const std::string& mc_dir);
void register_pack(const std::string& mc_dir, const std::string& name, const InstalledPack& pack);
void unregister_pack(const std::string& mc_dir, const std::string& name);
std::pair<bool, std::string> uninstall_pack(const std::string& mc_dir, const std::string& name);

// Резервная копия mods/ и дедупликация одноимённых модов.
std::string backup_mods(const std::string& mc_dir);
int deduplicate_mods(const std::string& mc_dir);

} // namespace mods
} // namespace sl