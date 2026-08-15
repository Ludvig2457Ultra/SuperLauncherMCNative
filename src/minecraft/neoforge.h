#pragma once
#include <string>
#include <vector>
#include "install.h"

namespace sl {

// Список версий NeoForge из maven API (формат {"versions": [...]}).
bool fetch_neoforge_versions(std::vector<std::string>& out, std::string* err = nullptr);

// Подобрать версию NeoForge для версии Minecraft ("26.2" -> "26.2.0.59").
// versions — из fetch_neoforge_versions (по возрастанию). Возвращает "" если нет.
std::string resolve_neoforge_version(const std::string& mc_version,
                                     const std::vector<std::string>& versions);

// Установить клиент NeoForge в mc_dir (гарантирует установку vanilla и запускает
// официальный инсталлер). progress — обратная связь UI.
// Возвращает "neoforge-<ver>" при успехе или "" (err заполнен).
std::string install_neoforge_client(const std::string& mc_version, const std::string& mc_dir,
                                    const std::string& java_path,
                                    InstallProgress* progress, std::string* err);

} // namespace sl