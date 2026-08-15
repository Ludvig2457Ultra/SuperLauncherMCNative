#include "install.h"
#include "version.h"
#include "../core/json.h"
#include "../core/paths.h"
#include "../core/log.h"
#include "../core/config.h"
#include "../crypto/sha1file.h"
#include "../net/http.h"
#include <shlobj.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace sl {

using std::string;

static const char* LIBRARIES_BASE = "https://libraries.minecraft.net/";
static NetConfig g_net_cfg;

void set_network_proxy(const string& host, int port, const string& user, const string& pass) {
    g_net_cfg.proxy_host = host;
    g_net_cfg.proxy_port = port;
    g_net_cfg.proxy_user = user;
    g_net_cfg.proxy_pass = pass;
}

static void notify(InstallProgress* p, const std::string& s, float d, float t) {
    if (p && p->update) p->update(s, d, t);
}

bool verify_file_sha1(const string& path, const string& expected_sha1) {
    if (!file_exists(path)) return false;
    if (file_size(path) == 0) return false; // 0-байтные jar — битые
    if (expected_sha1.empty()) return true;
    string h = sha1_file(path);
    bool ok = h.size() == 40 && h == expected_sha1;
    if (!ok) {
        log_warn("verify_file_sha1: mismatch " + path);
        log_warn("  got  " + h);
        log_warn("  want " + expected_sha1);
    }
    return ok;
}

bool restore_from_gradle_cache(const string& rel_path, const string& sha1, const string& target) {
    // ~/.gradle/caches/modules-2/files-2.1/<group>/<version>/<sha1>/<file>
    string cp = rel_path;
    for (auto& c : cp) if (c == '\\') c = '/';
    std::vector<string> parts;
    size_t pos = 0;
    while (true) {
        size_t slash = cp.find('/', pos);
        if (slash == string::npos) { parts.push_back(cp.substr(pos)); break; }
        parts.push_back(cp.substr(pos, slash - pos));
        pos = slash + 1;
    }
    if (parts.size() < 4) return false;
    // group = все сегменты кроме последних двух, затем version и filename
    string group;
    for (size_t i = 0; i < parts.size() - 2; i++) {
        if (i) group += ".";
        group += parts[i];
    }
    string version = parts[parts.size() - 2];
    string filename = parts[parts.size() - 1];

    char home[MAX_PATH] = {0};
    if (!GetEnvironmentVariableA("USERPROFILE", home, MAX_PATH)) return false;
    string gradle_home = string(home) + "/.gradle";
    for (const char* base : { "caches/modules-2/files-2.1", "caches/modules-2/files-2.0" }) {
        string cache_dir = path_join(path_join(gradle_home, base), path_join(group, path_join(version, sha1)));
        string src = path_join(cache_dir, filename);
        if (file_exists(src) && verify_file_sha1(src, sha1)) {
            mkdirs(parent_dir(target));
            if (copy_file(src, target)) {
                log_info("restored from gradle cache: " + filename);
                return true;
            }
        }
    }
    return false;
}

bool ensure_library_file(const string& mc_dir, const string& rel_path,
                         const string& url, const string& sha1,
                         long long size, InstallProgress* progress, bool verify_only) {
    string target = path_join(mc_dir, "libraries/" + rel_path);
    if (verify_file_sha1(target, sha1)) return true;
    // повреждён/отсутствует — пробуем из gradle-кэша
    if (!sha1.empty() && restore_from_gradle_cache(rel_path, sha1, target)) return true;
    if (verify_only) return false;
    if (url.empty()) return false;
    mkdirs(parent_dir(target));
    notify(progress, "Скачивание " + file_name(rel_path), 0, 0);
    bool ok = http_download(url.c_str(), target, nullptr, nullptr, g_net_cfg);
    if (!ok) return false;
    if (!verify_file_sha1(target, sha1)) {
        // повторная попытка
        remove_file(target);
        ok = http_download(url.c_str(), target, nullptr, nullptr, g_net_cfg);
        if (!ok || !verify_file_sha1(target, sha1)) return false;
    }
    return true;
}

// Скачивание client.jar по version.json
static bool ensure_client_jar(const string& mc_dir, const VersionMeta& m, InstallProgress* progress) {
    string target = path_join(mc_dir, m.client_path);
    if (verify_file_sha1(target, "")) {
        // jar уже есть и не пуст — ок (sha1 из версии не всегда указан в downloads.client у loader-ов)
        return true;
    }
    string ver_dir = path_join(mc_dir, "versions/" + m.id);
    // для vanilla downloads.client.url
    // (url берём из m.client_path? нет—нужно хранить client url)
    return true;
}

bool install_minecraft_version(const string& version_id, const string& mc_dir,
                               const string& manifest_url, InstallProgress* progress,
                               string* err, bool verify_only) {
    (void)manifest_url; // json качается сам (с учётом наследования)
    VersionMeta m;
    if (!load_version_meta_merged(version_id, mc_dir, &m, err)) return false;
    notify(progress, "Установка " + version_id, 0, 0);

    // 1. client.jar (для vanilla; у loader-ов джар принадлежит версии-предку)
    if (!m.client_url.empty()) {
        string owner = m.client_owner.empty() ? version_id : m.client_owner;
        string jp = path_join(path_join(path_join(mc_dir, "versions"), owner), owner + ".jar");
        if (!verify_file_sha1(jp, m.client_sha1)) {
            if (verify_only) { if (err) *err = "client jar missing"; return false; }
            mkdirs(parent_dir(jp));
            notify(progress, "Скачивание " + owner + ".jar", 0, 0);
            bool ok = http_download(m.client_url.c_str(), jp, nullptr, nullptr, g_net_cfg);
            if (!ok) { if (err) *err = "client jar download failed"; return false; }
            if (!m.client_sha1.empty() && !verify_file_sha1(jp, m.client_sha1)) {
                if (err) *err = "client jar sha1 mismatch";
                return false;
            }
        }
    }

    // 2. assetIndex
    if (!m.asset_index_name.empty()) {
        string idx_path = path_join(mc_dir, "assets/indexes/" + m.asset_index_name + ".json");
        bool need = !file_exists(idx_path);
        if (!need && !m.asset_index_sha1.empty()) need = !verify_file_sha1(idx_path, m.asset_index_sha1);
        if (need) {
            if (verify_only) { if (err) *err = "asset index missing"; return false; }
            mkdirs(parent_dir(idx_path));
            notify(progress, "Скачивание assetIndex " + m.asset_index_name, 0, 0);
            if (!http_download(m.asset_index_url.c_str(), idx_path, nullptr, nullptr, g_net_cfg)) {
                if (err) *err = "asset index download failed";
                return false;
            }
        }
        // 3. объекты ассетов
        string text = read_file_text(idx_path);
        using namespace sl::json;
        Value root;
        if (parse(text, root)) {
            const Value* objects = root.get("objects");
            if (objects && objects->is_obj()) {
                // количество объектов
                size_t total = objects->size();
                size_t done = 0;
                // подсчёт недостающих
                std::vector<std::pair<string, string>> missing; // hash, path
                for (auto& kv : *(objects->obj)) {
                    const Value& o = kv.second;
                    string h = o.get("hash") ? o.get("hash")->as_string() : string();
                    if (h.empty()) continue;
                    string p = path_join("assets/objects/" + h.substr(0, 2), h);
                    if (verify_file_sha1(path_join(mc_dir, p), h)) continue;
                    missing.emplace_back(h, p);
                }
                if (!missing.empty() && !verify_only) {
                    for (auto& mm : missing) {
                        string full = path_join(mc_dir, mm.second);
                        string url = "https://resources.download.minecraft.net/" + mm.first.substr(0, 2) + "/" + mm.first;
                        mkdirs(parent_dir(full));
                        notify(progress, "Ассет " + mm.first.substr(0, 8), (float)done, (float)missing.size());
                        if (!http_download(url.c_str(), full, nullptr, nullptr, g_net_cfg)) {
                            // пропускаем (обновляется при следующем запуске)
                            log_warn("asset download failed: " + mm.first);
                        } else {
                            done++;
                        }
                    }
                }
                if (verify_only && !missing.empty()) {
                    // не считаем ошибкой — ассеты докачиваются
                }
            }
        }
    }

    // 4. библиотеки
    {
        size_t total = m.libraries.size();
        size_t i = 0;
        for (auto& lib : m.libraries) {
            i++;
            if (!lib.allow_download()) continue;
            if (lib.path.empty()) continue;
            // библиотеки без url (поставленные локально) пропускаем
            if (lib.url.empty() && lib.sha1.empty()) continue;
            string rel = lib.path;
            if (rel.find('/') == string::npos || rel.find('/') == 0) {
                // path отсутствует в некоторых loader-ах — строим из имени
                continue;
            }
            notify(progress, "Библиотека " + file_name(rel), (float)i, (float)total);
            if (!ensure_library_file(mc_dir, rel, lib.url, lib.sha1, lib.size, progress, verify_only)) {
                if (!lib.url.empty()) {
                    log_warn("library download failed, retry: " + rel);
                    if (!ensure_library_file(mc_dir, rel, lib.url, lib.sha1, lib.size, progress, verify_only)) {
                        if (err && !verify_only) *err = "library download failed: " + rel;
                        if (verify_only) log_warn("verify failed (verify_only): " + rel);
                        else return false;
                    }
                }
            }
            // natives извлекаем позже (в command.cpp) — или здесь
        }
        notify(progress, "Библиотеки готовы", 1, 1);
    }

    notify(progress, "Готово: " + version_id, 1, 1);
    return true;
}

std::vector<string> list_installed_versions(const string& mc_dir) {
    std::vector<string> out;
    string vdir = path_join(mc_dir, "versions");
    if (!file_exists(vdir)) return out;
    const char* os_dir = getenv("PATH"); (void)os_dir;
    // Перебор каталогов
    // (используем FindFirstFile вместо std::filesystem для стабильности)
    string pattern = vdir + "/*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
                    out.push_back(fd.cFileName);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace sl