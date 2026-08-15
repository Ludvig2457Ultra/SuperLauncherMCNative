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

void extract_natives(const string& mc_dir, const string& version_id) {
    string ver_dir = path_join(mc_dir, "versions/" + version_id);
    string natives_dir = path_join(ver_dir, "natives");
    // удаляем старые dll (чтобы не оставалось лишних)
    mkdirs(natives_dir);

    VersionMeta m;
    string text = read_file_text(path_join(ver_dir, version_id + ".json"));
    if (text.empty()) return;
    if (!parse_version_json(text, &m)) return;

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
    string ver_dir = path_join(mc_dir, "versions/" + version_id);
    string json_path = path_join(ver_dir, version_id + ".json");
    if (!file_exists(json_path)) { if (err) *err = "no version json: " + json_path; return string(); }
    VersionMeta m;
    string text = read_file_text(json_path);
    if (!parse_version_json(text, &m, err)) return string();
    m.id = version_id;

    string client_jar = path_join(ver_dir, version_id + ".jar");
    if (!file_exists(client_jar)) { if (err) *err = "no client jar: " + client_jar; return string(); }

    // ---- natives ----
    extract_natives(mc_dir, version_id);
    string natives_dir = path_join(ver_dir, "natives");

    // ---- classpath: libraries + client jar ----
    string classpath;
    for (auto& lib : m.libraries) {
        if (!lib.allow_download()) continue;
        if (lib.path.empty()) continue;
        if (lib.url.empty() && lib.sha1.empty()) continue;
        string p = path_join(mc_dir, "libraries/" + lib.path);
        if (!file_exists(p)) {
            log_warn("missing library at launch: " + p);
            continue;
        }
        if (!classpath.empty()) classpath += ";";
        classpath += p;
    }
    if (!classpath.empty()) classpath += ";";
    classpath += client_jar;

    // ---- переменные ----
    std::vector<std::pair<string, string>> vars;
    vars.emplace_back("version_name", version_id);
    vars.emplace_back("version_type", m.type.empty() ? "release" : m.type);
    vars.emplace_back("launcher_name", opts.launcher_name);
    vars.emplace_back("launcher_version", opts.launcher_version);
    vars.emplace_back("library_directory", mc_dir + "/libraries");
    vars.emplace_back("natives_directory", natives_dir);
    vars.emplace_back("game_directory", mc_dir);
    vars.emplace_back("game_assets", mc_dir + "/assets");
    vars.emplace_back("assets_root", mc_dir + "/assets");
    vars.emplace_back("assets_index_name", m.asset_index_name.empty() ? m.assets : m.asset_index_name);
    vars.emplace_back("auth_player_name", opts.username);
    vars.emplace_back("auth_uuid", opts.uuid.empty() ? string("00000000-0000-0000-0000-000000000000") : opts.uuid);
    vars.emplace_back("auth_access_token", opts.token);
    vars.emplace_back("auth_session", opts.token);
    vars.emplace_back("auth_xuid", string());
    vars.emplace_back("user_type", "legacy");
    vars.emplace_back("user_properties", "{}");
    vars.emplace_back("clientid", string());
    vars.emplace_back("classpath", classpath);
    vars.emplace_back("path", string()); // для деталей логирования (заменяется позже)
    vars.emplace_back("-Dsun.jnu.encoding=utf-8", "-Dsun.jnu.encoding=utf-8"); // no-op
    vars.emplace_back("launcherLaunchEmulatedGame1", string());

    // ---- java exe ----
    // выбор Java с учётом требуемой версии из version.json (javaVersion)
    string java = opts.java_path.empty() ? find_java_path_for(m.java_major) : opts.java_path;
    if (java.empty()) java = "java";

    std::vector<string> cmd;
    cmd.push_back(java);
    // пользовательская память
    cmd.push_back("-Xmx" + std::to_string(opts.max_ram) + "M");
    cmd.push_back("-Xms" + std::to_string(opts.min_ram) + "M");
    for (auto& a : opts.extra_jvm) if (!a.empty()) cmd.push_back(a);

    // jvm args из версии (новый стиль) либо дефолт для legacy
    bool newstyle = !m.jvm_args.empty();
    if (newstyle) {
        for (auto& a : m.jvm_args) {
            string s = a;
            substitute(s, vars);
            if (s.empty()) continue;
            cmd.push_back(s);
        }
    } else {
        cmd.push_back("-Dminecraft.client.jar=" + client_jar);
        cmd.push_back("-Djava.library.path=" + natives_dir);
        cmd.push_back("-Dlog4j.configurationFile=" + ver_dir + "/client-1.12.xml");
        cmd.push_back("-cp");
        cmd.push_back(classpath);
    }

    if (m.main_class.empty()) { if (err) *err = "no mainClass"; return string(); }
    cmd.push_back(m.main_class);

    if (newstyle) {
        for (auto& a : m.game_args) {
            string s = a;
            substitute(s, vars);
            cmd.push_back(s);
        }
    } else {
        string ga = m.minecraft_arguments;
        substitute(ga, vars);
        // токенизация game args
        std::vector<string> toks;
        string cur;
        bool inq = false;
        for (char c : ga) {
            if (c == '\"') { inq = !inq; continue; }
            if (c == ' ' && !inq) { if (!cur.empty()) { toks.push_back(cur); cur.clear(); } continue; }
            cur += c;
        }
        if (!cur.empty()) toks.push_back(cur);
        for (auto& t : toks) cmd.push_back(t);
    }

    // собираем argv через CommandLineToArgvW-style
    string full;
    for (size_t i = 0; i < cmd.size(); i++) {
        if (i) full += " ";
        string& a = cmd[i];
        bool need_quote = a.find(' ') != string::npos || a.find('\\') != string::npos;
        if (need_quote) {
            full += "\"";
            // экранируем кавычки
            string t;
            for (char c : a) { if (c == '"') t += "\\\""; else t += c; }
            full += t;
            full += "\"";
        } else full += a;
    }
    return full;
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