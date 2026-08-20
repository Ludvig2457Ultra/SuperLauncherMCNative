#pragma once
#include <string>
#include <vector>

namespace sl {

// Опции запуска (зеркало Python options в LaunchThread.run)
struct LaunchOptions {
    std::string username = "player";
    std::string uuid;
    std::string token;
    long long max_ram = 4096;
    long long min_ram = 1024;
    std::string java_path;   // если пусто — find_java_path()
    std::vector<std::string> extra_jvm; // пользовательские jvm args
    std::string launcher_name = "SuperLauncher";
    std::string launcher_version = "2.0";
    std::string version_type = "release";
    std::string server_host;   // если не пусто — автоматическое подключение к серверу
    int server_port = 0;       // используется вместе с server_host
};

// Собрать командную строку запуска (аналог get_minecraft_command).
// version_id — ID установленной версии (с загрузчиком, если есть).
std::string build_minecraft_command(const std::string& version_id,
                                    const std::string& mc_dir,
                                    const LaunchOptions& opts,
                                    std::string* err);

// Извлечь natives-библиотеки (classifiers natives-windows) в versions/<id>/natives.
void extract_natives(const std::string& mc_dir, const std::string& version_id);

// Запустить процесс и дождаться завершения. Возвращает код возврата.
int launch_and_wait(const std::string& command_line, const std::string& workdir,
                    std::string* err);

// Разбить командную строку на argv и запустить (надёжнее для длинных команд).
int launch_argv_and_wait(const std::vector<std::string>& argv, const std::string& workdir,
                         std::string* err);

} // namespace sl