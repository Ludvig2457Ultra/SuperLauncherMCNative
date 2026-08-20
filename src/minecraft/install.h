#pragma once
#include <string>
#include <functional>

namespace sl {

struct InstallProgress {
    // status — текст, done/total 0..1
    void* user = nullptr;
    std::function<void(const std::string& status, float done, float total)> update;
};

// Установка/проверка Vanilla-версии в mc_dir (аналог install_minecraft_version).
// manifest_url — ссылка на version.json (уже полученная из manifest).
bool install_minecraft_version(const std::string& version_id,
                               const std::string& mc_dir,
                               const std::string& manifest_url,
                               InstallProgress* progress = nullptr,
                               std::string* err = nullptr,
                               bool verify_only = false);

// Задать прокси для всех загрузок (из Config).
void set_network_proxy(const std::string& host, int port,
                       const std::string& user, const std::string& pass);

// Скачать один файл, если он отсутствует или sha1 не совпадает.
// url_base — базовый URL библиотек ("https://libraries.minecraft.net/").
bool ensure_library_file(const std::string& mc_dir, const std::string& rel_path,
                         const std::string& url, const std::string& sha1,
                         long long size, InstallProgress* progress, bool verify_only);

// Восстановить повреждённую библиотеку из локального кэша Gradle (~/.gradle).
// возвращает true если восстановлено
bool restore_from_gradle_cache(const std::string& rel_path, const std::string& sha1,
                               const std::string& target);

// Проверить SHA-1 файла (с фиксом "wrong Checksum get da39a3ee..."). Возвращает
// true, если файл существует и (если ожидаемый sha1 не пуст) совпадает.
// При protect_sha256=true дополнительно сверяется локальный SHA-256 sidecar
// (<path>.sha256): если он есть и отличается — файл считается повреждённым
// (возможна коллизия SHA-1), чего нет — sidecar создаётся.
bool verify_file_sha1(const std::string& path, const std::string& expected_sha1,
                      bool protect_sha256 = false);

// Список уже установленных версий (каталоги versions/*).
std::vector<std::string> list_installed_versions(const std::string& mc_dir);

} // namespace sl