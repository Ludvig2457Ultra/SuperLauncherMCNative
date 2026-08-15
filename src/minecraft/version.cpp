#include "version.h"
#include "../core/json.h"
#include "../core/paths.h"
#include "../core/log.h"
#include "../net/http.h"
#include <set>

namespace sl {

using std::string;
using namespace sl::json;

static const char* MANIFEST_URL = "https://launchermeta.mojang.com/mc/game/version_manifest_v2.json";

bool Library::allow_download() const {
    if (rules.empty()) return true;
    bool allow = false;
    bool have = false;
    for (auto& r : rules) {
        // применяем правила: os.name
        bool os_match = true;
        if (!r.os_name.empty()) {
            os_match = false;
#ifdef _WIN32
            if (r.os_name == "windows") os_match = true;
#endif
        }
        if (!os_match) continue;
        have = true;
        if (r.action == "disallow") { allow = false; break; }
        allow = true;
    }
    return have ? allow : false;
}

// ---------------- manifest ----------------
bool fetch_manifest(std::vector<ManifestVersion>& out, std::string* err) {
    string text = http_get(MANIFEST_URL);
    if (text.empty()) {
        if (err) *err = "manifest download failed";
        return false;
    }
    Value root;
    if (!parse(text, root) || !root.is_obj()) {
        if (err) *err = "manifest parse failed";
        return false;
    }
    const Value* versions = root.get("versions");
    if (!versions || !versions->is_arr()) {
        if (err) *err = "manifest: no versions";
        return false;
    }
    out.clear();
    for (size_t i = 0; i < versions->size(); i++) {
        const Value& v = versions->at(i);
        ManifestVersion mv;
        const Value* x;
        if ((x = v.get("id"))) mv.id = x->as_string();
        if ((x = v.get("type"))) mv.type = x->as_string();
        if ((x = v.get("url"))) mv.url = x->as_string();
        if ((x = v.get("releaseTime"))) mv.release_time = x->as_string();
        if (!mv.id.empty()) out.push_back(mv);
    }
    return true;
}

std::string resolve_version_alias(const std::vector<ManifestVersion>& list, std::string alias, bool* ok) {
    if (ok) *ok = false;
    bool want_snapshot = alias == "snapshot" || alias == "latest_snapshot";
    std::string best_release, best_snapshot, any;
    for (auto& v : list) {
        if (v.type == "release" && best_release.empty()) best_release = v.id;
        if (v.type == "snapshot" && best_snapshot.empty()) best_snapshot = v.id;
        if (any.empty()) any = v.id;
    }
    std::string r;
    if (want_snapshot) r = best_snapshot.empty() ? any : best_snapshot;
    else r = best_release.empty() ? any : best_release;
    if (ok && !r.empty()) *ok = true;
    return r;
}

// ---------------- version json ----------------
static void parse_artifact(const Value& v, Artifact& a) {
    const Value* x;
    if ((x = v.get("path"))) a.path = x->as_string();
    if ((x = v.get("sha1"))) a.sha1 = x->as_string();
    if ((x = v.get("size"))) a.size = (long long)x->as_number();
    if ((x = v.get("url"))) a.url = x->as_string();
}

static void parse_rules(const Value* rules, std::vector<Rule>& out) {
    if (!rules || !rules->is_arr()) return;
    for (size_t i = 0; i < rules->size(); i++) {
        const Value& r = rules->at(i);
        if (!r.is_obj()) continue;
        Rule rl;
        const Value* x;
        if ((x = r.get("action"))) rl.action = x->as_string();
        const Value* os = r.get("os");
        if (os && os->is_obj()) {
            if ((x = os->get("name"))) rl.os_name = x->as_string();
            if ((x = os->get("version"))) rl.os_version = x->as_string();
            if ((x = os->get("arch"))) rl.os_arch = x->as_string();
        }
        out.push_back(rl);
    }
}

static void parse_args(const Value* node, std::vector<std::string>& out) {
    if (!node || !node->is_arr()) return;
    for (size_t i = 0; i < node->size(); i++) {
        const Value& a = node->at(i);
        if (a.is_str()) out.push_back(a.as_string());
        // объекты с rules пропускаем (несущественно для большинства версий)
    }
}

bool parse_version_json(const std::string& text, VersionMeta* out, std::string* err) {
    Value root;
    if (!parse(text, root) || !root.is_obj()) {
        if (err) *err = "version json parse failed";
        return false;
    }
    const Value* x;
    if ((x = root.get("id"))) out->id = x->as_string();
    if ((x = root.get("type"))) out->type = x->as_string();
    if ((x = root.get("mainClass"))) out->main_class = x->as_string();
    if ((x = root.get("assets"))) out->assets = x->as_string();
    if ((x = root.get("minecraftArguments"))) out->minecraft_arguments = x->as_string();
    if ((x = root.get("inheritsFrom"))) out->inherits_from = x->as_string();

    const Value* ai = root.get("assetIndex");
    if (ai && ai->is_obj()) {
        if ((x = ai->get("id"))) out->asset_index_name = x->as_string();
        if ((x = ai->get("sha1"))) out->asset_index_sha1 = x->as_string();
        if ((x = ai->get("url"))) out->asset_index_url = x->as_string();
        if ((x = ai->get("size"))) out->asset_index_size = (long long)x->as_number();
    }

    const Value* dl = root.get("downloads");
    if (dl && dl->is_obj()) {
        const Value* client = dl->get("client");
        if (client && client->is_obj()) {
            Artifact a; parse_artifact(*client, a);
            if (!a.sha1.empty()) {
                out->client_path = a.path.empty() ? std::string("client.jar") : a.path;
                out->client_url = a.url;
                out->client_sha1 = a.sha1;
            }
        }
    }

    const Value* jv = root.get("javaVersion");
    if (jv && jv->is_obj()) {
        if ((x = jv->get("majorVersion"))) out->java_major = (int)x->as_long();
    }

    const Value* args = root.get("arguments");
    if (args && args->is_obj()) {
        parse_args(args->get("jvm"), out->jvm_args);
        parse_args(args->get("game"), out->game_args);
    }

    const Value* libs = root.get("libraries");
    if (libs && libs->is_arr()) {
        for (size_t i = 0; i < libs->size(); i++) {
            const Value& l = libs->at(i);
            if (!l.is_obj()) continue;
            Library lib;
            if ((x = l.get("name"))) lib.name = x->as_string();
            parse_rules(l.get("rules"), lib.rules);
            const Value* nv = l.get("natives");
            if (nv && nv->is_obj()) {
                const Value* w = nv->get("windows");
                if (w && w->is_str()) lib.natives_extract = true;
            }
            const Value* dls = l.get("downloads");
            if (dls && dls->is_obj()) {
                const Value* art = dls->get("artifact");
                if (art && art->is_obj()) {
                    Artifact a; parse_artifact(*art, a);
                    lib.path = a.path; lib.url = a.url; lib.sha1 = a.sha1; lib.size = a.size;
                }
                if (lib.natives_extract) {
                    const Value* cls = dls->get("classifiers");
                    if (cls && cls->is_obj()) {
                        const Value* w = cls->get("natives-windows");
                        if (w && w->is_obj()) {
                            Artifact a; parse_artifact(*w, a);
                            lib.path = a.path; lib.url = a.url; lib.sha1 = a.sha1; lib.size = a.size;
                        }
                    }
                }
            }
            if (lib.path.empty() && !lib.name.empty()) {
                // имя вида group:name:version -> group/name/version/name-version.jar
                // на всякий случай строим из имени
                lib.path = lib.name;
            }
            out->libraries.push_back(std::move(lib));
        }
    }
    return true;
}

// Скачать и распарсить version.json для выбранной версии.
bool install_version_json(const std::string& version_id, const std::string& mc_dir,
                          const std::string& manifest_url, VersionMeta* out,
                          std::string* err) {
    string versions_dir = path_join(mc_dir, "versions");
    string ver_dir = path_join(versions_dir, version_id);
    mkdirs(ver_dir);
    string json_path = path_join(ver_dir, version_id + ".json");
    if (!file_exists(json_path)) {
        if (manifest_url.empty()) {
            if (err) *err = "no manifest url for " + version_id;
            return false;
        }
        if (!http_download(manifest_url.c_str(), json_path)) {
            if (err) *err = "cannot download " + manifest_url;
            return false;
        }
    }
    string text = read_file_text(json_path);
    if (text.empty()) {
        if (err) *err = "empty version json";
        return false;
    }
    VersionMeta m;
    if (!parse_version_json(text, &m, err)) return false;
    m.id = version_id;
    // клиентский путь по умолчанию
    if (m.client_path.empty()) m.client_path = path_join(ver_dir, version_id + ".jar");
    else m.client_path = path_join(ver_dir, file_name(m.client_path));
    if (out) *out = m;
    return true;
}

// Имя библиотеки без версии: "group:name:version[:classifier]" -> "group:name"
// (как _get_lib_name_without_version в minecraft-launcher-lib).
static std::string lib_name_without_version(const std::string& name) {
    if (name.empty()) return name;
    std::string::size_type last = name.rfind(':');
    if (last == std::string::npos) return name;
    return name.substr(0, last);
}

// Слить версию-наследника с версией-предком (семантика VanillaLauncher
// / minecraft-launcher-lib inherit_json).
static VersionMeta merge_inherited(const VersionMeta& child, const VersionMeta& parent) {
    VersionMeta out = child;

    // библиотеки: дочерние + предковые, чьё имя-без-версии не занято дочерними.
    // ВАЖНО: set заполняем ТОЛЬКО именами дочерних — классификаторы родителя
    // (natives-windows, unsafe и т.п.) не должны вычёркивать друг друга.
    std::set<std::string> names;
    for (auto& l : out.libraries) names.insert(lib_name_without_version(l.name));
    for (auto& l : parent.libraries) {
        std::string n = lib_name_without_version(l.name);
        if (names.find(n) == names.end())
            out.libraries.push_back(l);
    }

    // аргументы: дочерние идут первыми, затем предковые (-cp ${classpath} в хвосте)
    std::vector<std::string> jv = out.jvm_args;
    jv.insert(jv.end(), parent.jvm_args.begin(), parent.jvm_args.end());
    out.jvm_args = jv;
    std::vector<std::string> ga = out.game_args;
    ga.insert(ga.end(), parent.game_args.begin(), parent.game_args.end());
    out.game_args = ga;

    // приоритет у наследника
    if (out.main_class.empty()) out.main_class = parent.main_class;
    if (out.type.empty()) out.type = parent.type;
    if (out.assets.empty()) out.assets = parent.assets;
    if (out.minecraft_arguments.empty()) out.minecraft_arguments = parent.minecraft_arguments;
    if (out.java_major <= 0) out.java_major = parent.java_major;
    if (out.asset_index_name.empty()) {
        out.asset_index_name = parent.asset_index_name;
        out.asset_index_sha1 = parent.asset_index_sha1;
        out.asset_index_url = parent.asset_index_url;
        out.asset_index_size = parent.asset_index_size;
    }
    if (out.client_url.empty() && out.client_sha1.empty()) {
        out.client_url = parent.client_url;
        out.client_sha1 = parent.client_sha1;
        out.client_owner = parent.client_owner;
    }
    return out;
}

bool load_version_meta_merged(const std::string& version_id, const std::string& mc_dir,
                              VersionMeta* out, std::string* err) {
    // 1. гарантируем файл versions/<id>/<id>.json
    string versions_dir = path_join(mc_dir, "versions");
    string ver_dir = path_join(versions_dir, version_id);
    mkdirs(ver_dir);
    string json_path = path_join(ver_dir, version_id + ".json");
    if (!file_exists(json_path)) {
        // скачиваем по манифесту
        std::vector<ManifestVersion> list;
        if (!fetch_manifest(list)) {
            if (err) *err = "cannot load manifest for " + version_id;
            return false;
        }
        string url;
        for (auto& v : list) if (v.id == version_id) { url = v.url; break; }
        if (url.empty()) {
            if (err) *err = "version " + version_id + " not found in manifest";
            return false;
        }
        if (!http_download(url.c_str(), json_path)) {
            if (err) *err = "cannot download version json " + version_id;
            return false;
        }
    }
    string text = read_file_text(json_path);
    if (text.empty()) {
        if (err) *err = "empty version json " + version_id;
        return false;
    }
    VersionMeta child;
    if (!parse_version_json(text, &child, err)) return false;
    child.id = version_id;
    if (!child.client_url.empty() && child.client_owner.empty()) child.client_owner = version_id;

    // 2. наследование
    int guard = 0;
    std::string cur = version_id;
    while (!child.inherits_from.empty()) {
        if (++guard > 16 || child.inherits_from == cur) {
            if (err) *err = "inheritsFrom loop for " + version_id;
            return false;
        }
        cur = child.inherits_from;
        string pjson = path_join(path_join(versions_dir, cur), cur + ".json");
        if (!file_exists(pjson)) {
            std::vector<ManifestVersion> list;
            if (!fetch_manifest(list)) {
                if (err) *err = "cannot load manifest for parent " + cur;
                return false;
            }
            string url;
            for (auto& v : list) if (v.id == cur) { url = v.url; break; }
            if (url.empty()) {
                if (err) *err = "parent version " + cur + " not found in manifest";
                return false;
            }
            mkdirs(path_join(versions_dir, cur));
            if (!http_download(url.c_str(), pjson)) {
                if (err) *err = "cannot download parent version json " + cur;
                return false;
            }
        }
        string ptext = read_file_text(pjson);
        if (ptext.empty()) {
            if (err) *err = "empty parent version json " + cur;
            return false;
        }
        VersionMeta par;
        if (!parse_version_json(ptext, &par, err)) return false;
        par.id = cur;
        if (!par.client_url.empty() && par.client_owner.empty()) par.client_owner = cur;
        child = merge_inherited(child, par);
        child.inherits_from.clear(); // предок слит — прерываем цепочку
    }

    if (out) *out = child;
    return true;
}

std::string find_client_jar_in_chain(const std::string& version_id, const std::string& mc_dir) {
    string v = version_id;
    std::set<std::string> seen;
    while (!v.empty() && seen.insert(v).second) {
        string jar = path_join(path_join(path_join(mc_dir, "versions"), v), v + ".jar");
        if (file_exists(jar)) return jar;
        string jpath = path_join(path_join(path_join(mc_dir, "versions"), v), v + ".json");
        if (!file_exists(jpath)) return string();
        string text = read_file_text(jpath);
        if (text.empty()) return string();
        using namespace sl::json;
        Value root;
        if (!parse(text, root)) return string();
        const Value* x = root.get("inheritsFrom");
        if (!x || !x->is_str()) return string();
        v = x->as_string();
    }
    return string();
}

} // namespace sl