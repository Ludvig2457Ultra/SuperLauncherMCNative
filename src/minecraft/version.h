#pragma once
#include <string>
#include <vector>

namespace sl {

// Версия из version_manifest_v2.json
struct ManifestVersion {
    std::string id;
    std::string type; // release / snapshot / old_beta ...
    std::string url;  // ссылка на version.json
    std::string release_time;
};

// Один файл-артефакт (artifact/classifier) в version.json
struct Artifact {
    std::string path;   // относительный путь в libraries/ или client jar name
    std::string sha1;
    long long size = 0;
    std::string url;
    bool valid() const { return !url.empty() || !path.empty(); }
};

// Правило (rules) из version.json
struct Rule {
    std::string action; // allow / disallow
    std::string os_name;  // "windows" и т.п.
    std::string os_version;
    std::string os_arch;
};

// Библиотека
struct Library {
    std::string name;      // "com.example:lib:1.0"
    std::string path;      // путь artifact (или из name)
    std::string url;
    std::string sha1;
    long long size = 0;
    bool natives_extract = false; // из natives.windows
    std::vector<Rule> rules;
    bool allow_download() const; // с учётом rules для текущей ОС
};

// Полная версия (version.json)
struct VersionMeta {
    std::string id;
    std::string type;
    std::string main_class;
    std::string assets;              // для старых: "legacy"/"pre-1.6"
    std::string innerjar;            // например "client-extra" (rare)
    std::string client_path;         // путь к клиент-джар в versions/<id>/
    std::string asset_index_name;
    std::string asset_index_sha1;
    std::string asset_index_url = "https://piston-meta.mojang.com/mc/assets/";
    long long asset_index_size = 0;
    int java_major = 21;             // javaVersion.majorVersion
    // аргументы
    std::vector<std::string> jvm_args;   // из arguments.jvm (строки/подстроки правил)
    std::vector<std::string> game_args;  // из arguments.game
    std::string minecraft_arguments;     // legacy "minecraftArguments" (если нет пре-разобранных)
    std::vector<Library> libraries;
    std::vector<std::string> logging_client_file; // logging.client.file (артефакт)
};

// Скачать и распарсить version_manifest_v2.json -> список версий.
// Возвращает false при сетевой/парсинговой ошибке.
bool fetch_manifest(std::vector<ManifestVersion>& out, std::string* err = nullptr);

// Разрешить псевдоним (latest_release / latest / snapshot ...) в реальный id.
std::string resolve_version_alias(const std::vector<ManifestVersion>& list,
                                  std::string alias, bool* ok = nullptr);

// Скачать <id>.json в versions/<id>/<id>.json и распарсить.
bool install_version_json(const std::string& version_id, const std::string& mc_dir,
                          const std::string& manifest_url, VersionMeta* out,
                          std::string* err = nullptr);

// Распарсить уже скачанный version.json.
bool parse_version_json(const std::string& text, VersionMeta* out, std::string* err = nullptr);

} // namespace sl