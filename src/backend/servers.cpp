#include "servers.h"
#include "../core/json.h"
#include "../core/paths.h"
#include "../core/log.h"
#include <windows.h>
#include <cstdio>
#include <ctime>
#include <string>

namespace sl {
namespace servers {

using namespace sl::json;

static std::string pick(const std::string& rel) {
    std::string root = app_root_utf8();
    std::string a = path_join(root, rel);
    if (file_exists(a)) return a;
    if (file_exists(rel)) return rel;
    return a;
}
static std::string list_file() { return pick("servers_list.json"); }

static ServerInfo from_json(const Value& v) {
    ServerInfo s;
    auto gs = [&](const char* k, std::string& out) {
        const Value* x = v.get(k);
        if (x && x->is_str()) out = x->as_string();
    };
    auto gi = [&](const char* k, int& out) {
        const Value* x = v.get(k);
        if (x && x->is_num()) out = (int)x->as_long();
    };
    gs("name", s.name); gs("ip", s.ip);
    if (const Value* x = v.get("managed"); x && x->is_bool()) s.managed = x->as_bool();
    gi("ram_gb", s.ram_gb); gs("version", s.version); gs("core", s.core);
    gs("dir_path", s.dir_path);
    return s;
}

static Value to_json(const ServerInfo& s) {
    Value v(Type::Object);
    v.obj = new std::vector<std::pair<std::string, Value>>();
    auto put = [&](const char* k, const std::string& str) {
        Value nv(Type::String); nv.str = str; v.obj->push_back({ k, std::move(nv) });
    };
    auto puti = [&](const char* k, long long x) {
        Value nv(Type::Number); nv.num = (double)x; v.obj->push_back({ k, std::move(nv) });
    };
    put("name", s.name); put("ip", s.ip);
    Value m(Type::Bool); m.b = s.managed; v.obj->push_back({ "managed", std::move(m) });
    puti("ram_gb", s.ram_gb); put("version", s.version); put("core", s.core);
    put("dir_path", s.dir_path);
    return v;
}

std::vector<ServerInfo> load_list() {
    std::vector<ServerInfo> out;
    if (!file_exists(list_file())) return out;
    std::string text = read_file_text(list_file());
    if (text.empty()) return out;
    Value root;
    if (!parse(text, root) || !root.is_arr()) return out;
    for (size_t i = 0; i < root.size(); i++) out.push_back(from_json(root.at(i)));
    return out;
}

void save_list(const std::vector<ServerInfo>& servers) {
    Value arr(Type::Array);
    arr.arr = new std::vector<Value>();
    for (auto& s : servers) arr.arr->push_back(to_json(s));
    write_file_text(list_file(), dump(arr, 2));
}

std::string create_server_dir(const std::string& name) {
    std::string root = app_root_utf8();
    std::string dir = path_join(path_join(root, "servers"), name);
    mkdirs(dir);
    return dir;
}

// ---- процессы (до 8 управляемых серверов) ----
// Каждый процесс = пара stdin-пайп + дескриптор процесса.
struct ServProc {
    PROCESS_INFORMATION pi = {0};
    HANDLE in = nullptr;   // пишем команды
    HANDLE read_out = nullptr;
    bool running = false;
};
static ServProc g_procs[8];

static DWORD WINAPI DrainOut(LPVOID param) {
    ServProc* sp = (ServProc*)param;
    char buf[512];
    DWORD rd;
    while (ReadFile(sp->read_out, buf, sizeof(buf) - 1, &rd, nullptr) && rd > 0) {
        buf[rd] = 0;
        log_info("[server] " + std::string(buf, rd));
    }
    return 0;
}

bool start_server(const std::string& server_path, int ram_gb, const std::string& version,
                  const std::string& core, const std::string& java_args) {
    // найти свободный слот
    int idx = -1;
    for (int i = 0; i < 8; i++) if (!g_procs[i].running) { idx = i; break; }
    if (idx < 0) return false;

    std::string bat = path_join(server_path, "start.bat");
    if (!file_exists(bat)) {
        std::string java = find_java_path();
        std::string content = "@echo off\n";
        content += "cd /d \"%~dp0\"\n";
        if (!java.empty()) content += "set JAVA=" + java + "\n";
        content += java.empty() ? "java " : "\"%JAVA%\" ";
        content += "-Xmx" + std::to_string(ram_gb) + "G -Xms" + std::to_string(ram_gb) + "G ";
        if (!java_args.empty()) content += java_args + " ";
        content += "-jar server.jar nogui\n";
        write_file_text(bat, content);
    }

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE out_w = nullptr;
    if (!CreatePipe(&g_procs[idx].read_out, &out_w, &sa, 0)) return false;
    if (!CreatePipe(&g_procs[idx].in, nullptr, &sa, 0)) return false;
    SetHandleInformation(out_w, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = g_procs[idx].in;
    si.hStdOutput = out_w;
    si.hStdError = out_w;

    std::string cmd = "cmd.exe /c start.bat";
    BOOL ok = CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, server_path.c_str(),
                             &si, &g_procs[idx].pi);
    CloseHandle(out_w);
    if (!ok) {
        CloseHandle(g_procs[idx].in);
        CloseHandle(g_procs[idx].read_out);
        return false;
    }
    g_procs[idx].running = true;
    g_procs[idx].in = g_procs[idx].in;
    HANDLE hThread = CreateThread(nullptr, 0, DrainOut, &g_procs[idx], 0, nullptr);
    if (hThread) CloseHandle(hThread);
    return true;
}

static int find_proc_index_by_path(const std::string& server_path) {
    // здесь мы не храним пути; используем первый живой процесс
    for (int i = 0; i < 8; i++) if (g_procs[i].running) return i;
    return -1;
}

bool server_running(int process_index) {
    if (process_index < 0 || process_index >= 8) return false;
    if (!g_procs[process_index].running) return false;
    DWORD code = 0;
    if (!GetExitCodeProcess(g_procs[process_index].pi.hProcess, &code)) return false;
    return code == STILL_ACTIVE;
}

void send_command(int process_index, const std::string& cmd) {
    if (process_index < 0 || process_index >= 8) return;
    if (!server_running(process_index)) return;
    std::string c = cmd + "\n";
    DWORD written = 0;
    WriteFile(g_procs[process_index].in, c.data(), (DWORD)c.size(), &written, nullptr);
}

void stop_server(int process_index) {
    if (process_index < 0 || process_index >= 8) return;
    send_command(process_index, "stop");
    // подождать до 10 сек
    for (int i = 0; i < 100; i++) {
        if (!server_running(process_index)) break;
        Sleep(100);
    }
    if (server_running(process_index)) {
        TerminateProcess(g_procs[process_index].pi.hProcess, 0);
    }
    if (g_procs[process_index].pi.hProcess) CloseHandle(g_procs[process_index].pi.hProcess);
    if (g_procs[process_index].pi.hThread) CloseHandle(g_procs[process_index].pi.hThread);
    if (g_procs[process_index].in) CloseHandle(g_procs[process_index].in);
    if (g_procs[process_index].read_out) CloseHandle(g_procs[process_index].read_out);
    g_procs[process_index] = ServProc();
}

int running_count() {
    int n = 0;
    for (int i = 0; i < 8; i++) if (g_procs[i].running) n++;
    return n;
}

} // namespace servers
} // namespace sl