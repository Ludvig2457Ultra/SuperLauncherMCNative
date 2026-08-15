#include "config.h"
#include "paths.h"
#include "log.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace sl {

using namespace sl::json;

static std::string pick_path(const std::string& rel) {
    // Приоритет: каталог приложения (APPDATA/SuperLauncher), затем CWD
    std::string root = app_root_utf8();
    std::string a = path_join(root, rel);
    if (file_exists(a)) return a;
    if (file_exists(rel)) return rel;
    return a;
}

void Config::load() {
    std::string path = pick_path(settings_file());
    if (!file_exists(path)) return;
    std::string text = read_file_text(path);
    if (text.empty()) return;
    Value root;
    if (!parse(text, root)) return;
    auto gs = [&](const char* k, std::string& out) {
        const Value* v = root.get(k);
        if (v && v->is_str()) out = v->as_string();
    };
    auto gi = [&](const char* k, long long& out) {
        const Value* v = root.get(k);
        if (v && v->is_num()) out = v->as_long();
    };
    gs("java_path", java_path);
    gi("max_ram", max_ram);
    gs("jvm_args", jvm_args);
    gs("language", language);
    gs("theme", theme);
    gs("launch_mode", launch_mode);
    gs("proxy_host", proxy_host);
    { long long t = proxy_port; gi("proxy_port", t); proxy_port = (int)t; }
    gs("proxy_user", proxy_user);
    gs("proxy_pass", proxy_pass);
    gs("curseforge_api_key", curseforge_api_key);
    gs("last_username", last_username);
    gs("last_version_id", last_version_id);
    gs("last_loader_type", last_loader_type);
}

void Config::save() {
    Value root(Type::Object);
    root.type = Type::Object;
    root.obj = new std::vector<std::pair<std::string, Value>>();
    auto put = [&](const char* k, const std::string& v) {
        Value nv(Type::String); nv.str = v; root.obj->push_back({ k, std::move(nv) });
    };
    auto puti = [&](const char* k, long long v) {
        Value nv(Type::Number); nv.num = (double)v; root.obj->push_back({ k, std::move(nv) });
    };
    put("java_path", java_path);
    puti("max_ram", max_ram);
    put("jvm_args", jvm_args);
    put("language", language);
    put("theme", theme);
    put("launch_mode", launch_mode);
    put("proxy_host", proxy_host);
    puti("proxy_port", proxy_port);
    put("proxy_user", proxy_user);
    put("proxy_pass", proxy_pass);
    put("curseforge_api_key", curseforge_api_key);
    put("last_username", last_username);
    put("last_version_id", last_version_id);
    put("last_loader_type", last_loader_type);
    std::string path = pick_path(settings_file());
    write_file_text(path, dump(root, 4));
}

std::string Instance::game_dir() const {
    return path_join(path_join(app_root_utf8(), "user_data"), id + "/game");
}

Instance instance_from_json(const json::Value& v) {
    Instance i;
    auto gs = [&](const char* k, std::string& out) {
        const Value* x = v.get(k);
        if (x && x->is_str()) out = x->as_string();
    };
    auto gi = [&](const char* k, long long& out) {
        const Value* x = v.get(k);
        if (x && x->is_num()) out = x->as_long();
    };
    gs("id", i.id); gs("name", i.name); gs("icon", i.icon);
    gs("mc_version", i.mc_version); gs("loader", i.loader);
    gs("loader_version", i.loader_version); gs("java_path", i.java_path);
    gi("max_ram", i.max_ram); gs("jvm_args", i.jvm_args);
    gs("last_played", i.last_played);
    return i;
}

json::Value instance_to_json(const Instance& i) {
    Value v(Type::Object);
    v.obj = new std::vector<std::pair<std::string, Value>>();
    auto put = [&](const char* k, const std::string& s) {
        Value nv(Type::String); nv.str = s; v.obj->push_back({ k, std::move(nv) });
    };
    auto puti = [&](const char* k, long long x) {
        Value nv(Type::Number); nv.num = (double)x; v.obj->push_back({ k, std::move(nv) });
    };
    put("id", i.id); put("name", i.name); put("icon", i.icon);
    put("mc_version", i.mc_version); put("loader", i.loader);
    put("loader_version", i.loader_version); put("java_path", i.java_path);
    puti("max_ram", i.max_ram); put("jvm_args", i.jvm_args);
    put("last_played", i.last_played);
    return v;
}

std::vector<Instance> load_instances() {
    std::vector<Instance> out;
    std::string path = pick_path(instances_file());
    if (!file_exists(path)) return out;
    std::string text = read_file_text(path);
    if (text.empty()) return out;
    Value root;
    if (!parse(text, root) || !root.is_arr()) return out;
    for (size_t i = 0; i < root.size(); i++) {
        out.push_back(instance_from_json(root.at(i)));
    }
    return out;
}

void save_instances(const std::vector<Instance>& list) {
    Value arr(Type::Array);
    arr.arr = new std::vector<Value>();
    for (auto& i : list) arr.arr->push_back(instance_to_json(i));
    std::string path = pick_path(instances_file());
    write_file_text(path, dump(arr, 2));
}

} // namespace sl