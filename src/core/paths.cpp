#include "paths.h"
#include "common.h"
#ifdef _WIN32
#include "win.h"
#include <shlobj.h>
#include <shlwapi.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include "../platform/plat.h"
#if defined(__linux__)
#include <sys/sysinfo.h>
#endif

namespace sl {

using std::string;
using std::wstring;

// Простой ascii/utf8-перенос строки в wide (для не-Windows, где файловые пути
// из $HOME почти всегда ASCII; полноценный UTF-8 на Unix — за рамками WIP).
static wstring utf8_to_wide(const string& s) {
    wstring o;
    for (unsigned char c : s) o.push_back((wchar_t)c);
    return o;
}

std::wstring appdata_path() {
#ifdef _WIN32
    static wstring cached;
    if (!cached.empty()) return cached;
    wchar_t buf[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf))) {
        cached = buf;
    }
    return cached;
#else
    static wstring cached;
    if (!cached.empty()) return cached;
    const char* h = getenv("HOME");
    cached = utf8_to_wide(h ? h : ".");
    if (!cached.empty() && cached.back() != L'/') cached += L"/";
    cached += L".local/share";
    return cached;
#endif
}

std::wstring app_root() {
    static wstring cached;
    if (!cached.empty()) return cached;
    cached = appdata_path();
#ifdef _WIN32
    if (!cached.empty()) cached += L"\\SuperLauncher";
#else
    if (!cached.empty()) cached += L"/SuperLauncher";
#endif
    return cached;
}

string app_root_utf8() { return w2a(app_root()); }

string minecraft_directory() {
    static string cached;
    if (!cached.empty()) return cached;
#ifdef _WIN32
    wstring base = appdata_path();
    cached = w2a(base + L"\\.minecraft");
#else
    const char* h = getenv("HOME");
    cached = string(h ? h : ".") + "/.minecraft";
#endif
    return cached;
}

string instances_file() { return "user_data/instances.json"; }
string settings_file() { return "settings.json"; }

// ---------------- files ----------------

bool file_exists(const string& path) {
    if (path.empty()) return false;
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
#endif
}
#ifdef _WIN32
bool file_exists_w(const wstring& path) {
    if (path.empty()) return false;
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
#endif

string read_file_text(const string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return string();
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    string s((size_t)(n > 0 ? n : 0), 0);
    if (n > 0) {
        size_t rd = fread(&s[0], 1, (size_t)n, f);
        s.resize(rd);
    }
    fclose(f);
    return s;
}

bool write_file_text(const string& path, const string& text) {
    mkdirs(parent_dir(path));
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t wr = text.empty() ? 0 : fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    return wr == text.size() || text.empty();
}

bool mkdirs(const string& path) {
    if (path.empty()) return false;
#ifdef _WIN32
    std::wstring w = a2w(path);
    return SUCCEEDED(SHCreateDirectoryExW(nullptr, w.c_str(), nullptr)) ||
           GetLastError() == ERROR_ALREADY_EXISTS ||
           (GetFileAttributesW(w.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return !ec || std::filesystem::is_directory(path, ec);
#endif
}
#ifdef _WIN32
bool mkdirs_w(const wstring& path) {
    if (path.empty()) return false;
    return SUCCEEDED(SHCreateDirectoryExW(nullptr, path.c_str(), nullptr)) ||
           GetLastError() == ERROR_ALREADY_EXISTS ||
           (GetFileAttributesW(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
#endif

bool copy_file(const string& src, const string& dst) {
#ifdef _WIN32
    return CopyFileA(src.c_str(), dst.c_str(), FALSE) != 0;
#else
    std::error_code ec;
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
#endif
}
bool remove_file(const string& path) {
#ifdef _WIN32
    return DeleteFileA(path.c_str()) != 0;
#else
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return !ec;
#endif
}
long long file_size(const string& path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &d)) return -1;
    return ((long long)d.nFileSizeHigh << 32) | d.nFileSizeLow;
#else
    std::error_code ec;
    auto n = std::filesystem::file_size(path, ec);
    return ec ? -1 : (long long)n;
#endif
}

string path_join(const string& a, const string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (a.back() == '/' || a.back() == '\\') return a + b;
    return a + "/" + b;
}
string parent_dir(const string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == string::npos) return string();
    return path.substr(0, pos);
}
string file_name(const string& path) {
    size_t pos = path.find_last_of("/\\");
    return pos == string::npos ? path : path.substr(pos + 1);
}

// ---------------- java search ----------------
// Извлечь мажорную версию Java из пути к java.exe (по имени каталога).
// jdk26.0.1_8/bin -> 26 ; zulu-17/bin -> 17 ; jre1.8.0_491/bin -> 8 ; jdk-21.0.11/bin -> 21
static int java_major_from_exe(const string& java_exe) {
    string dir = parent_dir(parent_dir(java_exe)); // ...\bin -> ...\<vendor>-<ver>
    size_t sep = dir.find_last_of("/\\");
    string name = sep == string::npos ? dir : dir.substr(sep + 1);
    size_t fd = string::npos;
    for (size_t i = 0; i < name.size(); i++)
        if (name[i] >= '0' && name[i] <= '9') { fd = i; break; }
    if (fd == string::npos) return 0;
    long v = strtol(name.c_str() + fd, nullptr, 10);
    if (v == 1) {
        // старая схема "1.8.x" -> 8, "1.7.x" -> 7
        const char* q = name.c_str() + fd + 1;
        while (*q && !(*q >= '0' && *q <= '9')) q++;
        if (*q) { long m = strtol(q, nullptr, 10); if (m >= 2 && m <= 8) v = m; }
    }
    return (int)v;
}

static void collect_java_candidates(std::vector<std::pair<string, int>>& out) {
#ifdef _WIN32
    char pf[MAX_PATH] = {0}, pfx86[MAX_PATH] = {0};
    DWORD n1 = GetEnvironmentVariableA("ProgramFiles", pf, MAX_PATH);
    DWORD n2 = GetEnvironmentVariableA("ProgramFiles(x86)", pfx86, MAX_PATH);
    string P = n1 ? pf : "C:\\Program Files";
    string X = n2 ? pfx86 : "C:\\Program Files (x86)";

    std::vector<string> bases = {
        P + "\\Java", X + "\\Java",
        P + "\\Eclipse Adoptium", P + "\\Microsoft", P + "\\Zulu",
        P + "\\Amazon Corretto", P + "\\BellSoft", P + "\\Azul", P + "\\Liberica",
    };
    for (auto& base : bases) {
        if (!file_exists(base)) continue;
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA((base + "\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            // интересуют каталоги вида jdk*, jre*, zulu-*, temurin* и т.п.
            // (с цифрой в имени, чтобы не цеплять служебные "latest")
            bool has_digit = false;
            for (const char* p = fd.cFileName; *p; p++)
                if (*p >= '0' && *p <= '9') { has_digit = true; break; }
            if (!has_digit) continue;
            string full = base + "\\" + fd.cFileName + "\\bin\\java.exe";
            if (file_exists(full)) {
                out.push_back({ full, java_major_from_exe(full) });
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }

    // Oracle javapath (обычная установка Oracle JRE)
    const char* op = "C:\\ProgramData\\Oracle\\Java\\javapath\\java.exe";
    if (file_exists(op)) out.push_back({ op, 0 });

    // PATH
    char* pathvar = getenv("PATH");
    if (pathvar) {
        char* tok = strtok(pathvar, ";");
        while (tok) {
            string cand = string(tok) + "\\java.exe";
            if (file_exists(cand)) {
                out.push_back({ cand, java_major_from_exe(cand) });
                break; // один из PATH достаточно
            }
            tok = strtok(nullptr, ";");
        }
    }
#else
    // Unix: типовые места установки JDK + PATH
    std::vector<string> roots;
    const char* h = getenv("HOME");
    if (h) {
        roots.push_back(string(h) + "/.sdkman/candidates/java");
        roots.push_back(string(h) + "/.jdks");
    }
    roots.push_back("/usr/lib/jvm");
    roots.push_back("/opt/java");
    for (auto& base : roots) {
        std::error_code ec;
        if (!std::filesystem::is_directory(base, ec)) continue;
        for (auto& n : list_directory(base)) {
            string full = base + "/" + n + "/bin/java";
            if (file_exists(full)) out.push_back({ full, java_major_from_exe(full) });
        }
    }
    const char* pathvar = getenv("PATH");
    if (pathvar) {
        string p = pathvar;
        size_t start = 0;
        while (start < p.size()) {
            size_t colon = p.find(':', start);
            string dir = colon == string::npos ? p.substr(start) : p.substr(start, colon - start);
            string cand = dir + "/java";
            if (file_exists(cand)) { out.push_back({ cand, java_major_from_exe(cand) }); break; }
            if (colon == string::npos) break;
            start = colon + 1;
        }
    }
#endif
}

static string pick_java(int required_major, bool prefer_oldest) {
    std::vector<std::pair<string, int>> cands;
    collect_java_candidates(cands);
    if (cands.empty()) return string();

    // кандидаты с известной версией
    std::vector<std::pair<string, int>> known;
    for (auto& c : cands) if (c.second > 0) known.push_back(c);
    if (known.empty()) known = cands; // только неизвестные — берём любой

    std::pair<string, int>* best = nullptr;
    for (auto& c : known) {
        if (prefer_oldest) {
            // наименьшая версия >= required; если нет — самая новая
            bool ok = required_major <= 0 || c.second >= required_major;
            if (ok && (!best || c.second < best->second)) best = &c;
        } else {
            if (!best || c.second > best->second) best = &c;
        }
    }
    if (prefer_oldest && !best) {
        // ни одна не подходит — берём самую новую
        for (auto& c : known)
            if (!best || c.second > best->second) best = &c;
    }
    return best ? best->first : string();
}

string find_java_path() {
    return pick_java(0, false);
}

string find_java_path_for(int required_major) {
    return pick_java(required_major, true);
}

long long system_total_ram_mb() {
#ifdef _WIN32
    MEMORYSTATUSEX ms = {0};
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) return 0;
    return (long long)(ms.ullTotalPhys / (1024 * 1024));
#elif defined(__linux__)
    struct sysinfo si;
    if (sysinfo(&si) != 0) return 0;
    long long total = (long long)si.totalram * si.mem_unit;
    return total / (1024 * 1024);
#else
    return 0;
#endif
}

} // namespace sl