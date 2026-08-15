#pragma once
#include <string>
#include <vector>

namespace sl {

// Minecraft-сервер (models/types.py + backend/servers.py).
struct ServerInfo {
    std::string name;
    std::string ip;
    bool managed = false;
    int ram_gb = 4;
    std::string version;
    std::string core;
    std::string dir_path;
};

namespace servers {

// servers_list.json
std::vector<ServerInfo> load_list();
void save_list(const std::vector<ServerInfo>& servers);

// Создать каталог servers/<name> и вернуть путь.
std::string create_server_dir(const std::string& name);

// Запуск управляемого сервера: создаёт start.bat и запускает в фоне.
// process_index — иденификатор (0..n) для kill.
bool start_server(const std::string& server_path, int ram_gb, const std::string& version,
                  const std::string& core, const std::string& java_args);
// Остановка через stdin ("stop") с таймаутом, затем kill.
void stop_server(int process_index);
// Отправить команду в stdin.
void send_command(int process_index, const std::string& cmd);
// Живой ли процесс.
bool server_running(int process_index);
// Короткое имя процесса для консоли.
int running_count();

} // namespace servers
} // namespace sl