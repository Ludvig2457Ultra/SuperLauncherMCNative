#pragma once
#include "json.h"

namespace sl {

// Настройки запуска (settings.json) — зеркало Python load_config().
struct Config {
    std::string java_path = "";
    long long max_ram = 4096;
    std::string jvm_args = "";
    std::string language = "ru";
    std::string theme = "dark";
    std::string launch_mode = "launcher_lib";
    std::string proxy_host = "";
    int proxy_port = 0;
    std::string proxy_user = "";
    std::string proxy_pass = "";
    std::string curseforge_api_key = "";
    std::string last_username = "";
    std::string last_version_id = "";
    std::string last_loader_type = "";
    bool auto_resolve_alias = true;

    void load();
    void save();
};

// Экземпляр (instance) — запись user_data/instances.json.
struct Instance {
    std::string id;
    std::string name;
    std::string icon;
    std::string mc_version = "latest_release";
    std::string loader = "Vanilla";
    std::string loader_version = "";
    std::string java_path = "";
    long long max_ram = 0;
    std::string jvm_args = "";
    std::string last_played = "";

    // Путь к папке игры (user_data/<id>/game)
    std::string game_dir() const;
};

// Загрузка/сохранение списка инстансов
std::vector<Instance> load_instances();
void save_instances(const std::vector<Instance>& list);

// JSON <-> Instance
Instance instance_from_json(const json::Value& v);
json::Value instance_to_json(const Instance& i);

} // namespace sl