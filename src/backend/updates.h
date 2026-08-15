#pragma once
#include <string>
#include <vector>

namespace sl {

namespace updates {

struct ReleaseInfo {
    std::string tag;
    std::string name;
    std::string body;
    std::string date;
    std::string dl_url;
    bool prerelease = false;
};

// Список релизов с GitHub (per_page=10).
// Возвращает true при успехе.
bool fetch_releases(std::vector<ReleaseInfo>& out);

// Найти первый релиз с тегом новее current_version.
// returns true и заполняет release.
bool check_for_update(const std::string& current_version, ReleaseInfo& out);

// Утилита сравнения версий вида "v1.2.3" / "2.0.0_2026".
// >0: a новее; <0: b новее; 0: равны.
int compare_versions(const std::string& a, const std::string& b);

} // namespace updates
} // namespace sl