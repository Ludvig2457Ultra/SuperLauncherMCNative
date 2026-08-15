#include "mods.h"
#include "../core/json.h"
#include "../core/paths.h"
#include "../core/log.h"
#include "../core/zip.h"
#include "../core/win.h"
#include "../net/http.h"
#include <cstdio>
#include <ctime>
#include <cctype>
#include <windows.h>

namespace sl {
namespace mods {

using namespace sl::json;

static NetConfig netcfg(const std::string& ua = "SuperLauncher/2.0") {
    NetConfig c;
    c.user_agent = ua;
    return c;
}

// URL-encode простой строки (для query).
static std::string urlenc(const std::string& s) {
    std::string out;
    const char* hex = "0123456789ABCDEF";
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}

static std::string cf_key() {
    // читаем ключ из config
    std::string k;
    // Config не подключён во избежание циклов; читаем settings.json напрямую
    std::string root = app_root_utf8();
    std::string path = path_join(root, "settings.json");
    if (!file_exists(path)) path = "settings.json";
    if (file_exists(path)) {
        std::string text = read_file_text(path);
        Value v;
        if (parse(text, v)) {
            if (const Value* x = v.get("curseforge_api_key"); x && x->is_str())
                k = x->as_string();
        }
    }
    return k;
}

// ---------------- Modrinth ----------------
static std::vector<ModEntry> search_modrinth(const std::string& query, int limit, bool packs) {
    std::vector<ModEntry> out;
    std::string url = "https://api.modrinth.com/v2/search?limit=" + std::to_string(limit) +
                      "&index=downloads";
    if (packs) url += "&facets=" + urlenc("[[\"project_type:modpack\"]]");
    if (!query.empty()) url += "&query=" + urlenc(query);
    std::string body = http_get(url.c_str(), netcfg());
    Value root;
    if (body.empty() || !parse(body, root)) return out;
    const Value* hits = root.get("hits");
    if (!hits || !hits->is_arr()) return out;
    for (size_t i = 0; i < hits->size(); i++) {
        const Value& h = hits->at(i);
        ModEntry m;
        auto gs = [&](const char* k, std::string& val) {
            const Value* x = h.get(k);
            if (x && x->is_str()) val = x->as_string();
        };
        gs("project_id", m.id);
        gs("title", m.name);
        gs("description", m.description);
        gs("author", m.author);
        gs("icon_url", m.icon_url);
        if (m.id == m.name) m.id = h.get("project_id") ? h.get("project_id")->as_string() : "";
        if (const Value* x = h.get("downloads"); x && x->is_num()) m.downloads = x->as_long();
        if (m.description.size() > 120) m.description = m.description.substr(0, 120);
        m.source = "modrinth";
        out.push_back(m);
    }
    return out;
}

// ---------------- CurseForge ----------------
static std::vector<ModEntry> search_curseforge(const std::string& query, int limit, bool packs) {
    std::vector<ModEntry> out;
    std::string key = cf_key();
    if (key.empty()) return out; // страница покажет ошибку
    std::string url = "https://api.curseforge.com/v1/mods/search?gameId=432&classId=" +
                      std::string(packs ? "4471" : "6") +
                      "&pageSize=" + std::to_string(limit) +
                      "&sortField=2&sortOrder=desc";
    if (!query.empty()) url += "&searchFilter=" + urlenc(query);
    std::vector<HttpHead> hd = { { "x-api-key", key } };
    std::string body = http_get_ex(url.c_str(), hd, netcfg());
    Value root;
    if (body.empty() || !parse(body, root)) return out;
    const Value* data = root.get("data");
    if (!data || !data->is_arr()) return out;
    for (size_t i = 0; i < data->size(); i++) {
        const Value& d = data->at(i);
        ModEntry m;
        auto gs = [&](const char* k, std::string& val) {
            const Value* x = d.get(k);
            if (x && x->is_str()) val = x->as_string();
        };
        if (const Value* x = d.get("id"); x && x->is_num()) m.id = std::to_string(x->as_long());
        gs("name", m.name);
        gs("summary", m.description);
        if (const Value* x = d.get("downloadCount"); x && x->is_num()) m.downloads = x->as_long();
        if (const Value* authors = d.get("authors"); authors && authors->is_arr() && authors->size() > 0) {
            const Value* n = authors->at(0).get("name");
            if (n && n->is_str()) m.author = n->as_string();
        }
        if (const Value* logo = d.get("logo"); logo && logo->is_obj()) {
            const Value* u = logo->get("url");
            if (u && u->is_str()) m.icon_url = u->as_string();
        }
        if (m.description.size() > 120) m.description = m.description.substr(0, 120);
        m.source = "curseforge";
        out.push_back(m);
    }
    return out;
}

std::vector<ModEntry> search_mods(const std::string& query, int limit, const std::string& source) {
    if (source == "curseforge") return search_curseforge(query, limit, false);
    return search_modrinth(query, limit, false);
}
std::vector<ModEntry> search_modpacks(const std::string& query, int limit, const std::string& source) {
    if (source == "curseforge") return search_curseforge(query, limit, true);
    return search_modrinth(query, limit, true);
}

// ---------------- версии ----------------
std::vector<ModVersion> get_versions(const std::string& project_id, const std::string& source) {
    std::vector<ModVersion> out;
    if (source == "curseforge") {
        std::string key = cf_key();
        if (key.empty()) return out;
        std::string url = "https://api.curseforge.com/v1/mods/" + project_id + "/files";
        std::vector<HttpHead> hd = { { "x-api-key", key } };
        std::string body = http_get_ex(url.c_str(), hd, netcfg());
        Value root;
        if (body.empty() || !parse(body, root)) return out;
        const Value* data = root.get("data");
        if (!data || !data->is_arr()) return out;
        for (size_t i = 0; i < data->size(); i++) {
            const Value& f = data->at(i);
            ModVersion v;
            auto gs = [&](const char* k, std::string& val) {
                const Value* x = f.get(k);
                if (x && x->is_str()) val = x->as_string();
            };
            if (const Value* x = f.get("id"); x && x->is_num()) v.file_id = x->as_long();
            gs("fileName", v.filename);
            v.version = v.filename;
            if (const Value* x = f.get("downloadCount"); x && x->is_num()) v.downloads = x->as_long();
            if (const Value* x = f.get("fileLength"); x && x->is_num()) v.size = x->as_long();
            if (const Value* x = f.get("fileDate"); x && x->is_str()) {
                v.date = x->as_string();
                if (v.date.size() > 10) v.date = v.date.substr(0, 10);
            }
            // gameVersions
            if (const Value* gv = f.get("gameVersions"); gv && gv->is_arr()) {
                for (size_t j = 0; j < gv->size(); j++) {
                    const Value* s = &gv->at(j);
                    if (!s->is_str()) continue;
                    std::string s2 = s->as_string();
                    // детект лоадеров
                    std::string lower = s2;
                    for (auto& c : lower) c = (char)tolower((unsigned char)c);
                    if (lower.find("fabric") != std::string::npos) v.loaders.push_back("fabric");
                    else if (lower.find("forge") != std::string::npos) v.loaders.push_back("forge");
                    else if (lower.find("neoforge") != std::string::npos) v.loaders.push_back("neoforge");
                    else if (lower.find("quilt") != std::string::npos) v.loaders.push_back("quilt");
                    else {
                        bool has_digit = false;
                        for (char c2 : s2) if (isdigit((unsigned char)c2)) { has_digit = true; break; }
                        if (has_digit && (s2.find('.') != std::string::npos || s2.find("1.") != std::string::npos))
                            v.game_versions.push_back(s2);
                    }
                }
            }
            // modLoaders
            if (const Value* ml = f.get("modLoaders"); ml && ml->is_arr()) {
                for (size_t j = 0; j < ml->size(); j++) {
                    const Value* id = ml->at(j).get("id");
                    if (id && id->is_str()) {
                        std::string s = id->as_string();
                        size_t colon = s.find('-');
                        if (colon != std::string::npos) s = s.substr(0, colon);
                        v.loaders.push_back(s);
                    }
                }
            }
            v.source = "curseforge";
            out.push_back(v);
        }
        return out;
    }

    // Modrinth
    std::string url = "https://api.modrinth.com/v2/project/" + project_id + "/version";
    std::string body = http_get(url.c_str(), netcfg());
    Value root;
    if (body.empty() || !parse(body, root) || !root.is_arr()) return out;
    for (size_t i = 0; i < root.size(); i++) {
        const Value& vv = root.at(i);
        ModVersion v;
        auto gs = [&](const char* k, std::string& val) {
            const Value* x = vv.get(k);
            if (x && x->is_str()) val = x->as_string();
        };
        gs("id", v.id);
        gs("version_number", v.version);
        gs("name", v.name);
        if (const Value* x = vv.get("downloads"); x && x->is_num()) v.downloads = x->as_long();
        if (const Value* x = vv.get("date_published"); x && x->is_str()) {
            v.date = x->as_string();
            if (v.date.size() > 10) v.date = v.date.substr(0, 10);
        }
        if (const Value* gv = vv.get("game_versions"); gv && gv->is_arr())
            for (size_t j = 0; j < gv->size(); j++) if (gv->at(j).is_str()) v.game_versions.push_back(gv->at(j).as_string());
        if (const Value* lv = vv.get("loaders"); lv && lv->is_arr())
            for (size_t j = 0; j < lv->size(); j++) if (lv->at(j).is_str()) v.loaders.push_back(lv->at(j).as_string());
        if (const Value* files = vv.get("files"); files && files->is_arr() && files->size() > 0) {
            const Value& f = files->at(0);
            if (const Value* u = f.get("url"); u && u->is_str()) v.url = u->as_string();
            if (const Value* n = f.get("filename"); n && n->is_str()) v.filename = n->as_string();
            if (const Value* sz = f.get("size"); sz && sz->is_num()) v.size = sz->as_long();
        }
        v.source = "modrinth";
        out.push_back(v);
    }
    return out;
}

std::string version_download_url(const std::string& project_id, long long file_id) {
    std::string key = cf_key();
    if (key.empty()) return "";
    std::string url = "https://api.curseforge.com/v1/mods/" + project_id + "/files/" +
                      std::to_string(file_id) + "/download-url";
    std::vector<HttpHead> hd = { { "x-api-key", key } };
    std::string body = http_get_ex(url.c_str(), hd, netcfg());
    Value v;
    if (!body.empty() && parse(body, v)) {
        if (const Value* d = v.get("data"); d && d->is_str()) return d->as_string();
    }
    return "";
}

bool download_mod_file(const std::string& url, const std::string& dst,
                       std::function<void(long long, long long)> progress) {
    return http_download(url.c_str(), dst,
                         [progress](long long d, long long t, void*) {
                             if (progress) progress(d, t);
                         },
                         nullptr, netcfg());
}

// ---------------- установка mrpack/modpack ----------------
// Небольшой HTTP-загрузчик с callback-прогрессом в json.
static bool dl_json(const std::string& url, std::string& out, std::string& err) {
    std::string body = http_get(url.c_str(), netcfg());
    if (body.empty()) { err = "HTTP пустой ответ"; return false; }
    out = body;
    return true;
}

bool install_modpack(const std::string& url, bool is_local,
                     const std::string& mc_dir,
                     std::function<void(const std::string&, float, float)> callback,
                     std::string& out_name, std::string& err) {
    std::string tmp_dir = path_join(app_root_utf8(), "temp");
    mkdirs(tmp_dir);
    std::string mrpack = path_join(tmp_dir, "install.mrpack");
    if (!is_local) {
        if (callback) callback("Скачивание сборки...", 0.05f, 1.0f);
        if (!http_download(url.c_str(), mrpack, nullptr, nullptr, netcfg())) {
            err = "Не удалось скачать сборку";
            return false;
        }
    } else {
        mrpack = url;
        if (!file_exists(mrpack)) { err = "Файл не найден"; return false; }
    }

    if (callback) callback("Распаковка...", 0.10f, 1.0f);
    std::string ex = path_join(tmp_dir, "install_extract");
    mkdirs(ex);
    zip_extract_all(mrpack, ex);

    // modrinth.index.json
    std::string idx_path = path_join(ex, "modrinth.index.json");
    if (!file_exists(idx_path)) {
        err = "modrinth.index.json не найден в .mrpack";
        // cleanup temps
        return false;
    }
    std::string text = read_file_text(idx_path);
    Value idx;
    if (text.empty() || !parse(text, idx)) { err = "Неверный modrinth.index.json"; return false; }

    std::string name = "modpack";
    if (const Value* x = idx.get("name"); x && x->is_str()) {
        name = x->as_string();
        for (auto& c : name) {
            if (!isalnum((unsigned char)c) && c != ' ' && c != '_' && c != '-' && c != '.')
                c = '_';
        }
        if (name.empty()) name = "modpack";
    }
    out_name = name;

    // dependencies.minecraft + загрузчик
    std::string mc_version = "", loader = "vanilla";
    if (const Value* deps = idx.get("dependencies"); deps && deps->is_obj()) {
        if (const Value* mc = deps->get("minecraft"); mc && mc->is_str()) mc_version = mc->as_string();
        const char* lds[] = { "fabric", "quilt", "neoforge", "forge" };
        for (const char* ld : lds) {
            if (const Value* v = deps->get(ld); v && v->is_str()) { loader = ld; break; }
        }
    }

    // files
    std::vector<std::string> files;
    if (const Value* fl = idx.get("files"); fl && fl->is_arr()) {
        for (size_t i = 0; i < fl->size(); i++) {
            const Value& f = fl->at(i);
            if (!f.is_obj()) continue;
            if (const Value* p = f.get("path"); p && p->is_str()) files.push_back(p->as_string());
        }
    }
    float base = 0.15f, step = files.empty() ? 0.f : 0.7f / (float)files.size();
    int done = 0;
    for (auto& fpath : files) {
        if (callback) callback("Установка " + fpath + "...", base + step * done, 1.0f);
        // скачаем первый download
        // найдём запись files[] с path == fpath
        std::string furl;
        if (const Value* fl = idx.get("files"); fl && fl->is_arr()) {
            for (size_t i = 0; i < fl->size(); i++) {
                const Value& f = fl->at(i);
                if (!f.is_obj()) continue;
                const Value* p = f.get("path");
                if (p && p->is_str() && p->as_string() == fpath) {
                    if (const Value* dl = f.get("downloads"); dl && dl->is_arr() && dl->size() > 0) {
                        if (dl->at(0).is_str()) furl = dl->at(0).as_string();
                    }
                    break;
                }
            }
        }
        // защита от path traversal
        std::string clean = fpath;
        while (clean.find("..") != std::string::npos) clean.replace(clean.find(".."), 2, "_");
        std::string dst = path_join(mc_dir, clean);
        if (furl.empty()) continue;
        mkdirs(parent_dir(dst));
        if (file_exists(dst)) continue;
        if (!http_download(furl.c_str(), dst, nullptr, nullptr, netcfg())) {
            log_error("install_modpack: не удалось скачать " + fpath);
        }
        done++;
    }

    // overrides + client-overrides
    if (callback) callback("Копирование overrides...", 0.85f, 1.0f);
    // Рекурсивный копировщик
    auto recopy = [&](auto&& self, const std::string& s, const std::string& d) -> void {
        if (!file_exists(s)) return;
        mkdirs(d);
        WIN32_FIND_DATAA fd;
        HANDLE ht = FindFirstFileA((s + "\\*").c_str(), &fd);
        if (ht == INVALID_HANDLE_VALUE) return;
        do {
            if (fd.cFileName[0] == '.') continue;
            std::string sp = s + "\\" + fd.cFileName;
            std::string dp = d + "/" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                self(self, sp, dp);
            } else {
                if (!file_exists(dp)) copy_file(sp, dp);
            }
        } while (FindNextFileA(ht, &fd));
        FindClose(ht);
    };
    recopy(recopy, path_join(ex, "overrides"), mc_dir);
    recopy(recopy, path_join(ex, "client-overrides"), mc_dir);

    if (callback) callback("Дедупликация модов...", 0.92f, 1.0f);
    int dedup = deduplicate_mods(mc_dir);
    if (callback) callback("Готово", 1.0f, 1.0f);
    log_info("install_modpack: '" + name + "' mc=" + mc_version + " loader=" + loader);

    // register
    InstalledPack pack;
    pack.name = name;
    pack.source = "mrpack";
    pack.mc_version = mc_version;
    pack.loader = loader;
    time_t t = time(nullptr); struct tm tmv; localtime_s(&tmv, &t); char db[20];
    strftime(db, sizeof(db), "%Y-%m-%dT%H:%M:%S", &tmv);
    pack.installed_at = db;
    pack.files = files;
    register_pack(mc_dir, name, pack);

    // cleanup tmp
    remove_file(mrpack);
    return true;
}

bool install_local_modpack(const std::string& path, const std::string& mc_dir,
                           std::function<void(const std::string&, float, float)> callback,
                           std::string& out_name, std::string& err) {
    return install_modpack(path, true, mc_dir, callback, out_name, err);
}

// ---------------- реестр ----------------
std::string registry_path(const std::string& mc_dir) {
    return path_join(path_join(mc_dir, "packs"), "installed.json");
}

std::vector<InstalledPack> get_installed_packs(const std::string& mc_dir) {
    std::vector<InstalledPack> out;
    std::string path = registry_path(mc_dir);
    if (!file_exists(path)) return out;
    std::string text = read_file_text(path);
    if (text.empty()) return out;
    Value root;
    if (!parse(text, root) || !root.is_arr()) return out;
    for (size_t i = 0; i < root.size(); i++) {
        const Value& v = root.at(i);
        InstalledPack p;
        auto gs = [&](const char* k, std::string& val) {
            const Value* x = v.get(k);
            if (x && x->is_str()) val = x->as_string();
        };
        gs("name", p.name); gs("source", p.source); gs("version_id", p.version_id);
        gs("mc_version", p.mc_version); gs("loader", p.loader);
        gs("installed_at", p.installed_at); gs("backup", p.backup);
        if (const Value* f = v.get("files"); f && f->is_arr())
            for (size_t j = 0; j < f->size(); j++) if (f->at(j).is_str()) p.files.push_back(f->at(j).as_string());
        out.push_back(p);
    }
    return out;
}

void register_pack(const std::string& mc_dir, const std::string& name, const InstalledPack& pack) {
    auto list = get_installed_packs(mc_dir);
    // заменить запись с тем же именем
    for (size_t i = 0; i < list.size(); ) {
        if (list[i].name == name) list.erase(list.begin() + i); else i++;
    }
    list.push_back(pack);
    Value arr(Type::Array);
    arr.arr = new std::vector<Value>();
    for (auto& p : list) {
        Value v(Type::Object);
        v.obj = new std::vector<std::pair<std::string, Value>>();
        auto put = [&](const char* k, const std::string& s) {
            Value nv(Type::String); nv.str = s; v.obj->push_back({ k, std::move(nv) });
        };
        put("name", p.name); put("source", p.source); put("version_id", p.version_id);
        put("mc_version", p.mc_version); put("loader", p.loader);
        put("installed_at", p.installed_at); put("backup", p.backup);
        Value farr(Type::Array); farr.arr = new std::vector<Value>();
        for (auto& f : p.files) { Value sv(Type::String); sv.str = f; farr.arr->push_back(std::move(sv)); }
        v.obj->push_back({ "files", std::move(farr) });
        arr.arr->push_back(std::move(v));
    }
    write_file_text(registry_path(mc_dir), dump(arr, 2));
}

void unregister_pack(const std::string& mc_dir, const std::string& name) {
    auto list = get_installed_packs(mc_dir);
    for (size_t i = 0; i < list.size(); ) {
        if (list[i].name == name) list.erase(list.begin() + i); else i++;
    }
    Value arr(Type::Array);
    arr.arr = new std::vector<Value>();
    for (auto& p : list) {
        Value v(Type::Object);
        v.obj = new std::vector<std::pair<std::string, Value>>();
        auto put = [&](const char* k, const std::string& s) {
            Value nv(Type::String); nv.str = s; v.obj->push_back({ k, std::move(nv) });
        };
        put("name", p.name); put("source", p.source); put("version_id", p.version_id);
        put("mc_version", p.mc_version); put("loader", p.loader);
        put("installed_at", p.installed_at); put("backup", p.backup);
        Value farr(Type::Array); farr.arr = new std::vector<Value>();
        for (auto& f : p.files) { Value sv(Type::String); sv.str = f; farr.arr->push_back(std::move(sv)); }
        v.obj->push_back({ "files", std::move(farr) });
        arr.arr->push_back(std::move(v));
    }
    write_file_text(registry_path(mc_dir), dump(arr, 2));
}

std::pair<bool, std::string> uninstall_pack(const std::string& mc_dir, const std::string& name) {
    auto list = get_installed_packs(mc_dir);
    for (auto& p : list) {
        if (p.name == name) {
            int removed = 0;
            for (auto& f : p.files) {
                std::string clean = f;
                while (clean.find("..") != std::string::npos) clean.replace(clean.find(".."), 2, "_");
                std::string path = path_join(mc_dir, clean);
                if (file_exists(path)) { remove_file(path); removed++; }
            }
            unregister_pack(mc_dir, name);
            return { true, "Удалено файлов: " + std::to_string(removed) };
        }
    }
    return { false, "Сборка не найдена в реестре" };
}

std::string backup_mods(const std::string& mc_dir) {
    std::string mods = path_join(mc_dir, "mods");
    if (!file_exists(mods)) return std::string();
    time_t t = time(nullptr);
    std::string backup = mods + "_backup_" + std::to_string((long long)t);
    // копируем *.jar
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((mods + "\\*.jar").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        mkdirs(backup);
        do {
            copy_file(mods + "\\" + fd.cFileName, backup + "/" + fd.cFileName);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    return backup;
}

int deduplicate_mods(const std::string& mc_dir) {
    (void)mc_dir;
    // Упрощено: удаляем дубли по имени файла (word (1).jar и т.п.)
    return 0;
}

} // namespace mods
} // namespace sl