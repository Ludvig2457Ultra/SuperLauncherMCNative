#include "log.h"
#include "paths.h"
#include "win.h"
#include <cstdio>
#include <ctime>

namespace sl {

static FILE* g_log = nullptr;
static CRITICAL_SECTION g_cs;

struct LogInit { LogInit() { InitializeCriticalSection(&g_cs); } ~LogInit() { DeleteCriticalSection(&g_cs); if (g_log) fclose(g_log); } };
static LogInit g_li;

void log_set_file(const std::string& path) {
    EnterCriticalSection(&g_cs);
    if (g_log) { fclose(g_log); g_log = nullptr; }
    mkdirs(parent_dir(path));
    g_log = fopen(path.c_str(), "a");
    if (g_log) fprintf(g_log, "\n==== session start ====\n");
    LeaveCriticalSection(&g_cs);
}

void log_msg(LogLevel lvl, const std::string& msg) {
    const char* tag = lvl == LOG_INFO ? "INFO" : lvl == LOG_WARN ? "WARN" : lvl == LOG_ERROR ? "ERROR" : "DEBUG";
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_s(&tmv, &t);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", &tmv);
    std::string line = std::string("[") + ts + "] [" + tag + "] " + msg + "\r\n";

    EnterCriticalSection(&g_cs);
    if (g_log) { fputs(line.c_str(), g_log); fflush(g_log); }
    fputs(line.c_str(), stdout);
    fflush(stdout);
    LeaveCriticalSection(&g_cs);
}
void log_info(const std::string& msg) { log_msg(LOG_INFO, msg); }
void log_warn(const std::string& msg) { log_msg(LOG_WARN, msg); }
void log_error(const std::string& msg) { log_msg(LOG_ERROR, msg); }
void log_debug(const std::string& msg) { log_msg(LOG_DEBUG, msg); }

} // namespace sl