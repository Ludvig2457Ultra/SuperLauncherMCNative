#include "paths.h"
#include "common.h"
#include "win.h"
#include <shlobj.h>
#include <shlwapi.h>
#include <cstdio>
#include <cstdlib>

namespace sl {

using std::string;
using std::wstring;

std::wstring appdata_path() {
    static wstring cached;
    if (!cached.empty()) return cached;
    wchar_t buf[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf))) {
        cached = buf;
    }
    return cached;
}

std::wstring app_root() {
    static wstring cached;
    if (!cached.empty()) return cached;
    cached = appdata_path();
    if (!cached.empty()) cached += L"\\SuperLauncher";
    return cached;
}

string app_root_utf8() { return w2a(app_root()); }

string minecraft_directory() {
    static string cached;
    if (!cached.empty()) return cached;
    wstring base = appdata_path();
    cached = w2a(base + L"\\.minecraft");
    return cached;
}

string instances_file() { return "user_data/instances.json"; }
string settings_file() { return "settings.json"; }

// ---------------- files ----------------

bool file_exists(const string& path) {
    if (path.empty()) return false;
    DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
bool file_exists_w(const wstring& path) {
    if (path.empty()) return false;
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

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
    std::wstring w = a2w(path);
    return SUCCEEDED(SHCreateDirectoryExW(nullptr, w.c_str(), nullptr)) ||
           GetLastError() == ERROR_ALREADY_EXISTS ||
           (GetFileAttributesW(w.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
bool mkdirs_w(const wstring& path) {
    if (path.empty()) return false;
    return SUCCEEDED(SHCreateDirectoryExW(nullptr, path.c_str(), nullptr)) ||
           GetLastError() == ERROR_ALREADY_EXISTS ||
           (GetFileAttributesW(path.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool copy_file(const string& src, const string& dst) {
    return CopyFileA(src.c_str(), dst.c_str(), FALSE) != 0;
}
bool remove_file(const string& path) {
    return DeleteFileA(path.c_str()) != 0;
}
long long file_size(const string& path) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &d)) return -1;
    return ((long long)d.nFileSizeHigh << 32) | d.nFileSizeLow;
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

} // namespace sl