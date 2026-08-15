#include "neoforge.h"
#include "../core/json.h"
#include "../core/paths.h"
#include "../core/log.h"
#include "../core/config.h"
#include "../net/http.h"
#include "../core/common.h"
#include <windows.h>
#include <vector>

namespace sl {

using std::string;

static const char* NF_API   = "https://maven.neoforged.net/api/maven/versions/releases/net/neoforged/neoforge";
static const char* NF_MAVEN = "https://maven.neoforged.net/releases/net/neoforged/neoforge";

static void notify(InstallProgress* p, const string& s, float d, float t) {
    if (p && p->update) p->update(s, d, t);
}

bool fetch_neoforge_versions(std::vector<string>& out, string* err) {
    string text = http_get(NF_API);
    if (text.empty()) { if (err) *err = "neoforge api: пустой ответ"; return false; }
    json::Value root;
    if (!json::parse(text, root) || !root.is_obj()) {
        if (err) *err = "neoforge api: не JSON";
        return false;
    }
    const json::Value* vs = root.get("versions");
    if (!vs || !vs->is_arr()) {
        if (err) *err = "neoforge api: нет списка версий";
        return false;
    }
    out.clear();
    for (size_t i = 0; i < vs->size(); i++) {
        const json::Value& v = vs->at(i);
        if (v.is_str() && !v.as_string().empty()) out.push_back(v.as_string());
    }
    return !out.empty();
}

// "26.2" -> major=26, minor=2. Для 1.20.4 -> major=1? Нет: у Forge/NeoForge 1.x
// мажор — вторая часть. Но NeoForge использует новую схему, где "26.2" — сам мажор.
static void split_mc_version(const string& mc, string& major, string& minor) {
    size_t d = mc.find('.');
    if (d == string::npos) { major = mc; return; }
    major = mc.substr(0, d);
    size_t d2 = mc.find('.', d + 1);
    if (d2 == string::npos) minor = mc.substr(d + 1);
    else minor = mc.substr(d + 1, d2 - d - 1);
}

string resolve_neoforge_version(const string& mc_version, const std::vector<string>& versions) {
    string major, minor;
    split_mc_version(mc_version, major, minor);
    if (major.empty()) return string();

    string best;
    for (auto& v : versions) {
        std::vector<string> p;
        size_t pos = 0;
        while (true) {
            size_t s = v.find('.', pos);
            if (s == string::npos) { p.push_back(v.substr(pos)); break; }
            p.push_back(v.substr(pos, s - pos));
            pos = s + 1;
        }
        if (p.size() < 2) continue;
        if (p[0] != major) continue;
        if (!minor.empty() && p[1] != minor) continue;
        best = v; // массив по возрастанию — последнее подходящее и есть новейшее
    }
    return best;
}

// Запустить процесс (argv), ожидая до timeout_ms. Возвращает true при коде 0.
static bool run_installer(const std::vector<string>& argv, string* err, DWORD timeout_ms) {
    std::wstring cmdline;
    for (size_t i = 0; i < argv.size(); i++) {
        std::wstring a = a2w(argv[i]);
        bool q = !a.empty() && a.find(L' ') != std::wstring::npos;
        if (q) {
            cmdline += L"\"";
            for (wchar_t c : a) { if (c == L'"') cmdline += L"\\\""; else cmdline += c; }
            cmdline += L"\"";
        } else cmdline += a;
        if (i + 1 < argv.size()) cmdline += L" ";
    }
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;
    PROCESS_INFORMATION pi = {0};
    std::wstring mut = cmdline;
    if (!CreateProcessW(nullptr, &mut[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        if (err) *err = "CreateProcessW, error " + std::to_string(GetLastError());
        return false;
    }
    DWORD wait = WaitForSingleObject(pi.hProcess, timeout_ms);
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (err) *err = "инсталлер завис (превышен таймаут)";
        return false;
    }
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (code != 0) {
        if (err) *err = "инсталлер завершился с кодом " + std::to_string(code);
        return false;
    }
    return true;
}

string install_neoforge_client(const string& mc_version, const string& mc_dir,
                               const string& java_path, InstallProgress* progress, string* err) {
    notify(progress, "Поиск NeoForge для " + mc_version, 0, 0);
    std::vector<string> versions;
    if (!fetch_neoforge_versions(versions, err)) {
        notify(progress, "API NeoForge недоступен", 0, 0);
        return string();
    }
    string ver = resolve_neoforge_version(mc_version, versions);
    if (ver.empty()) {
        if (err) *err = "NeoForge не найден для " + mc_version;
        return string();
    }

    // 1. гарантируем vanilla (её клиент и библиотеки наследуются)
    if (!install_minecraft_version(mc_version, mc_dir, "", progress, err, false)) {
        notify(progress, "Ошибка установки vanilla " + mc_version, 0, 0);
        return string();
    }

    // 2. скачиваем инсталлер
    notify(progress, "Скачивание NeoForge " + ver + "...", 0, 0);
    string tmpdir = path_join(app_root_utf8(), "temp");
    mkdirs(tmpdir);
    string inst = path_join(tmpdir, "neoforge-" + ver + "-installer.jar");
    string inst_url = string(NF_MAVEN) + "/" + ver + "/neoforge-" + ver + "-installer.jar";
    if (!http_download(inst_url.c_str(), inst)) {
        if (err) *err = "не удалось скачать инсталлер NeoForge " + ver;
        return string();
    }

    // 3. запускаем официальный инсталлер
    notify(progress, "Запуск инсталлера NeoForge " + ver + "...", 0, 0);
    string java = java_path;
    if (java.empty()) {
        Config cfg;
        cfg.load();
        java = cfg.java_path;
    }
    if (java.empty()) java = find_java_path_for(25);
    if (java.empty()) java = "java";

    std::vector<string> argv;
    argv.push_back(java);
    argv.push_back("-jar");
    argv.push_back(inst);
    argv.push_back("--install-client");
    argv.push_back(mc_dir);
    if (!run_installer(argv, err, 300000)) {
        notify(progress, "Инсталлер NeoForge не завершился", 0, 0);
        return string();
    }

    // 4. проверка результата
    string nf_id = "neoforge-" + ver;
    string vj = path_join(path_join(path_join(mc_dir, "versions"), nf_id), nf_id + ".json");
    if (!file_exists(vj)) {
        if (err) *err = "клиент не установлен: " + nf_id + " (не найден " + vj + ")";
        return string();
    }
    notify(progress, "NeoForge готов: " + nf_id, 1, 1);
    return nf_id;
}

} // namespace sl