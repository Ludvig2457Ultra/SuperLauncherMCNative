#include "command.h"
#include "version.h"
#include "../core/json.h"
#include "../core/paths.h"
#include "../core/log.h"
#include "../core/zip.h"
#include "../core/win.h"

namespace sl {

using std::string;

// ---- переменные ${...} ----
static void substitute(string& s, const std::vector<std::pair<string, string>>& vars) {
    for (auto& kv : vars) {
        string key = "${" + kv.first + "}";
        size_t at;
        while ((at = s.find(key)) != string::npos) {
            s.replace(at, key.size(), kv.second);
        }
    }
}

static string prim_string(const json::Value* v) {
    return v ? v->as_string() : string();
}

// ---- структура данных: контекст сборки команды ----
// Держит всё, что понадобится на разных шагах сборки (client jar, natives,
// переменные ${...}, classpath), чтобы каждый шаг был маленьким и тестируемым.
struct CommandCtx {
    std::string     version_id;
    std::string     mc_dir;
    std::string     ver_dir;
    std::string     natives_dir;
    std::string     client_jar;   // путь к клиент-джару (свой или унаследованный)
    std::string     classpath;    // libraries + client jar
    std::vector<std::pair<string, string>> vars; // переменные для substitute()
};

// ---- шаг 1: определить jar клиента (свой / владельца / по цепочке) ----
static bool resolve_client_jar(CommandCtx& c, const VersionMeta& m, string* err) {
    c.client_jar = path_join(c.ver_dir, c.version_id + ".jar");
    if (file_exists(c.client_jar)) return true;

    c.client_jar.clear();
    if (!m.client_owner.empty()) {
        string owner_jar = path_join(path_join(path_join(c.mc_dir, "versions"), m.client_owner),
                                     m.client_owner + ".jar");
        if (file_exists(owner_jar)) c.client_jar = owner_jar;
    }
    if (c.client_jar.empty())
        c.client_jar = find_client_jar_in_chain(c.version_id, c.mc_dir);
    if (c.client_jar.empty() && !m.client_url.empty()) {
        if (err) *err = "no client jar: " + path_join(c.ver_dir, c.version_id + ".json");
        return false;
    }
    return true;
}

// ---- шаг 2: собрать classpath из библиотек + client jar ----
static void build_classpath(CommandCtx& c, const VersionMeta& m) {
    for (auto& lib : m.libraries) {
        if (!lib.allow_download()) continue;
        if (lib.path.empty()) continue;
        if (lib.url.empty() && lib.sha1.empty()) continue;
        string p = path_join(c.mc_dir, "libraries/" + lib.path);
        if (!file_exists(p)) { log_warn("missing library at launch: " + p); continue; }
        if (!c.classpath.empty()) c.classpath += ";";
        c.classpath += p;
    }
    // loader-версия без собственного джар (напр. NeoForge) — класспас уже в библиотеках
    if (c.client_jar.empty()) {
        if (!c.classpath.empty() && c.classpath.back() == ';') c.classpath.pop_back();
    } else {
        if (!c.classpath.empty()) c.classpath += ";";
        c.classpath += c.client_jar;
    }
}

// ---- шаг 3: заполнить переменные ${...} для подстановки в аргументы ----
static void fill_vars(CommandCtx& c, const VersionMeta& m, const LaunchOptions& opts,
                      const string& natives_dir, const string& classpath) {
    auto& v = c.vars;
    v.emplace_back("version_name", c.version_id);
    v.emplace_back("version_type", m.type.empty() ? "release" : m.type);
    v.emplace_back("launcher_name", opts.launcher_name);
    v.emplace_back("launcher_version", opts.launcher_version);
    v.emplace_back("library_directory", c.mc_dir + "/libraries");
    v.emplace_back("natives_directory", natives_dir);
    v.emplace_back("game_directory", c.mc_dir);
    v.emplace_back("game_assets", c.mc_dir + "/assets");
    v.emplace_back("assets_root", c.mc_dir + "/assets");
    v.emplace_back("assets_index_name", m.asset_index_name.empty() ? m.assets : m.asset_index_name);
    v.emplace_back("auth_player_name", opts.username);
    v.emplace_back("auth_uuid", opts.uuid.empty() ? string("00000000-0000-0000-0000-000000000000") : opts.uuid);
    v.emplace_back("auth_access_token", opts.token);
    v.emplace_back("auth_session", opts.token);
    v.emplace_back("auth_xuid", string());
    v.emplace_back("user_type", "legacy");
    v.emplace_back("user_properties", "{}");
    v.emplace_back("clientid", string());
    v.emplace_back("classpath", classpath);
    v.emplace_back("path", string()); // для деталей логирования (заменяется позже)
    v.emplace_back("-Dsun.jnu.encoding=utf-8", "-Dsun.jnu.encoding=utf-8"); // no-op
    v.emplace_back("launcherLaunchEmulatedGame1", string());
}

// ---- шаг 4: выбрать java exe с учётом требуемой версии (javaVersion) ----
static string pick_java(const VersionMeta& m, const LaunchOptions& opts) {
    string java = opts.java_path.empty() ? find_java_path_for(m.java_major) : opts.java_path;
    if (java.empty()) java = "java";
    return java;
}

// ---- шаг 5: собрать jvm-аргументы (новый стиль либо legacy-дефолт) ----
static void add_jvm_args(std::vector<string>& cmd, const VersionMeta& m,
                         const CommandCtx& c, const string& natives_dir) {
    bool newstyle = !m.jvm_args.empty();
    if (newstyle) {
        for (auto& a : m.jvm_args) {
            string s = a;
            substitute(s, c.vars);
            if (s.empty()) continue;
            cmd.push_back(s);
        }
    } else {
        if (!c.client_jar.empty()) cmd.push_back("-Dminecraft.client.jar=" + c.client_jar);
        cmd.push_back("-Djava.library.path=" + natives_dir);
        cmd.push_back("-Dlog4j.configurationFile=" + c.ver_dir + "/client-1.12.xml");
        cmd.push_back("-cp");
        cmd.push_back(c.classpath);
    }
}

// ---- шаг 6: собрать игровые аргументы (новый стиль либо legacy-токенизация) ----
static void add_game_args(std::vector<string>& cmd, const VersionMeta& m, const CommandCtx& c) {
    if (!m.jvm_args.empty()) {
        for (auto& a : m.game_args) {
            string s = a;
            substitute(s, c.vars);
            cmd.push_back(s);
        }
        return;
    }
    string ga = m.minecraft_arguments;
    substitute(ga, c.vars);
    // токенизация legacy game args по пробелам вне кавычек
    std::vector<string> toks;
    string cur;
    bool inq = false;
    for (char ch : ga) {
        if (ch == '\"') { inq = !inq; continue; }
        if (ch == ' ' && !inq) { if (!cur.empty()) { toks.push_back(cur); cur.clear(); } continue; }
        cur += ch;
    }
    if (!cur.empty()) toks.push_back(cur);
    for (auto& t : toks) cmd.push_back(t);
}

// ---- шаг 7: склеить argv в командную строку (CommandLineToArgvW-style) ----
static string join_argv(const std::vector<string>& cmd) {
    string full;
    for (size_t i = 0; i < cmd.size(); i++) {
        if (i) full += " ";
        string a = cmd[i];
        bool need_quote = a.find(' ') != string::npos || a.find('\\') != string::npos;
        if (need_quote) {
            full += "\"";
            for (char ch : a) { if (ch == '"') full += "\\\""; else full += ch; }
            full += "\"";
        } else full += a;
    }
    return full;
}

void extract_natives(const string& mc_dir, const string& version_id) {
    string ver_dir = path_join(mc_dir, "versions/" + version_id);
    string natives_dir = path_join(ver_dir, "natives");
    // удаляем старые dll (чтобы не оставалось лишних)
    mkdirs(natives_dir);

    VersionMeta m;
    if (!load_version_meta_merged(version_id, mc_dir, &m)) return;

    for (auto& lib : m.libraries) {
        if (!lib.allow_download()) continue;
        if (!lib.natives_extract) continue;
        if (lib.path.empty()) continue;
        string jar = path_join(mc_dir, "libraries/" + lib.path);
        if (!file_exists(jar)) continue;
        string rel = lib.path.substr(0, lib.path.find_last_of('/'));
        string target_natives = path_join(natives_dir, rel);
        mkdirs(target_natives);
        int n = zip_extract_all(jar, target_natives);
        if (n > 0) log_info("extracted " + std::to_string(n) + " natives from " + file_name(lib.path));
    }
}

string build_minecraft_command(const string& version_id, const string& mc_dir,
                               const LaunchOptions& opts, string* err) {
    // --- контекст сборки ---
    CommandCtx c;
    c.version_id = version_id;
    c.mc_dir = mc_dir;
    c.ver_dir = path_join(mc_dir, "versions/" + version_id);
    string json_path = path_join(c.ver_dir, version_id + ".json");

    // --- метаданные версии (слитые по цепочке наследования) ---
    if (!file_exists(json_path)) { if (err) *err = "no version json: " + json_path; return string(); }
    VersionMeta m;
    if (!load_version_meta_merged(version_id, mc_dir, &m, err)) return string();
    m.id = version_id;

    // --- шаги сборки (каждый — маленький отдельный метод) ---
    if (!resolve_client_jar(c, m, err)) return string();          // 1. клиент-джар
    extract_natives(mc_dir, version_id);                            // natives
    c.natives_dir = path_join(c.ver_dir, "natives");
    build_classpath(c, m);                                          // 2. classpath
    fill_vars(c, m, opts, c.natives_dir, c.classpath);              // 3. ${...}
    string java = pick_java(m, opts);                               // 4. java exe

    // --- сборка argv ---
    std::vector<string> cmd;
    cmd.push_back(java);
    cmd.push_back("-Xmx" + std::to_string(opts.max_ram) + "M");
    cmd.push_back("-Xms" + std::to_string(opts.min_ram) + "M");
    for (auto& a : opts.extra_jvm) if (!a.empty()) cmd.push_back(a);

    add_jvm_args(cmd, m, c, c.natives_dir);                         // 5. jvm args
    if (m.main_class.empty()) { if (err) *err = "no mainClass"; return string(); }
    cmd.push_back(m.main_class);
    add_game_args(cmd, m, c);                                       // 6. game args

    return join_argv(cmd);                                          // 7. командная строка
}

int launch_and_wait(const string& command_line, const string& workdir, string* err) {
    // строки c cmd.exe т.к. CreateProcessA требует exe
    string cmd = command_line;
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;
    PROCESS_INFORMATION pi = {0};
    LPSTR wd = workdir.empty() ? nullptr : (LPSTR)workdir.c_str();
    if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, wd, &si, &pi)) {
        if (err) *err = "CreateProcessA failed, error " + std::to_string(GetLastError());
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

int launch_argv_and_wait(const std::vector<string>& argv, const string& workdir, string* err) {
    string full;
    for (size_t i = 0; i < argv.size(); i++) {
        string a = argv[i];
        bool need_quote = a.find(' ') != string::npos;
        if (need_quote) {
            // простая экранировка
            full += "\"";
            for (char c : a) { if (c == '"') full += "\\\""; else full += c; }
            full += "\"";
        } else full += a;
        if (i + 1 < argv.size()) full += " ";
    }
    return launch_and_wait(full, workdir, err);
}

} // namespace sl