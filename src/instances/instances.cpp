#include "instances.h"
#include "../core/paths.h"
#include "../core/win.h"
#include <cstdio>
#include <ctime>

namespace sl {

std::vector<Instance> InstancesManager::all() { return load_instances(); }
void InstancesManager::save(const std::vector<Instance>& list) { save_instances(list); }

Instance* InstancesManager::find(std::vector<Instance>& list, const std::string& id) {
    for (auto& i : list) if (i.id == id) return &i;
    return nullptr;
}

Instance InstancesManager::create(const std::string& name, const std::string& mc_version,
                                  const std::string& loader, const std::string& icon) {
    // id — 8 hex символов (аналог uuid4[:8])
    char buf[16];
    UINT_PTR v = 0;
    HKEY hk = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Cryptography", 0, KEY_READ, &hk) == ERROR_SUCCESS) {
        DWORD sz = sizeof(v);
        RegQueryValueExA(hk, "MachineGuid", nullptr, nullptr, (LPBYTE)&v, &sz);
        RegCloseKey(hk);
    }
    v = (unsigned long long)GetTickCount() ^ (UINT_PTR)&buf ^ ((UINT_PTR)name.c_str());
    snprintf(buf, sizeof(buf), "%08llx", (unsigned long long)(v & 0xFFFFFFFF));
    std::string id(buf);

    Instance i;
    i.id = id;
    i.name = name.empty() ? "Новый" : name;
    i.icon = icon;
    i.mc_version = mc_version.empty() ? "latest_release" : mc_version;
    i.loader = loader.empty() ? "Vanilla" : loader;
    i.max_ram = 0;

    std::vector<Instance> list = load_instances();
    list.push_back(i);
    save_instances(list);
    mkdirs(i.game_dir());
    return i;
}

void InstancesManager::remove(const std::string& id) {
    std::vector<Instance> list = load_instances();
    std::vector<Instance> out;
    for (auto& i : list) if (i.id != id) out.push_back(i);
    save_instances(out);
}

} // namespace sl