#pragma once
#include "../core/config.h"

namespace sl {

// CRUD-обёртка над user_data/instances.json
struct InstancesManager {
    static std::vector<Instance> all();
    static void save(const std::vector<Instance>& list);
    static Instance* find(std::vector<Instance>& list, const std::string& id);
    // Создать инстанс, вернуть заполненный объект (id присвоен).
    static Instance create(const std::string& name, const std::string& mc_version,
                            const std::string& loader, const std::string& icon = "Minecraft");
    static void remove(const std::string& id);
};

} // namespace sl