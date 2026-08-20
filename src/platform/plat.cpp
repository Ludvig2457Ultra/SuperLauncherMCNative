#include "plat.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>
#include <cerrno>
#endif

namespace sl {

using std::string;

string env_get(const char* name) {
    const char* v = std::getenv(name);
    return v ? string(v) : string();
}

string env_user_home() {
#ifdef _WIN32
    string h = env_get("USERPROFILE");
    if (!h.empty()) return h;
    return env_get("HOMEDRIVE") + env_get("HOMEPATH");
#else
    return env_get("HOME");
#endif
}

std::vector<string> list_directory(const string& path) {
    std::vector<string> out;
#ifdef _WIN32
    string pat = path + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.cFileName[0] == '.') continue;
            out.push_back(fd.cFileName);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    std::error_code ec;
    std::filesystem::directory_iterator it(
        path, std::filesystem::directory_options::skip_permission_denied, ec);
    std::filesystem::directory_iterator end;
    for (; it != end && !ec; it.increment(ec)) {
        string n = it->path().filename().string();
        if (n != "." && n != "..") out.push_back(n);
    }
#endif
    std::sort(out.begin(), out.end());
    return out;
}

int run_command_and_wait(const string& cmdline, const string& cwd, string* err) {
#ifdef _WIN32
    string cmd = "cmd.exe /c " + cmdline;
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {0};
    LPSTR wd = cwd.empty() ? nullptr : (LPSTR)cwd.c_str();
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
#else
    pid_t pid = fork();
    if (pid < 0) {
        if (err) *err = "fork failed: " + string(std::strerror(errno));
        return -1;
    }
    if (pid == 0) {
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) _exit(127);
        execl("/bin/sh", "sh", "-c", cmdline.c_str(), (char*)nullptr);
        _exit(127); // execl не удался
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        if (err) *err = "waitpid failed";
        return -1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
#endif
}

} // namespace sl