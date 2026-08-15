#pragma once
#include <string>
#include <vector>
#include <functional>

// Скачивание из сети. Потокобезопасные функции (каждая открывает свою сессию).
namespace sl {

struct NetConfig {
    std::string proxy_host;
    int proxy_port = 0;
    std::string proxy_user;
    std::string proxy_pass;
    std::string user_agent = "SuperLauncher/2.0";
    int timeout_seconds = 30;
};

struct HttpHead {
    std::string name;
    std::string value;
};

// Запрос с телом (GET/POST/PUT) и дополнительными заголовками.
// Возвращает тело ответа, "" при ошибке.
std::string http_request(const char* method, const char* url, const std::string& body,
                         const std::vector<HttpHead>& headers = {},
                         const NetConfig& cfg = NetConfig());

// Получить тело ответа GET (малый размер: JSON). Возвращает "" при ошибке.
std::string http_get(const char* url, const NetConfig& cfg = NetConfig());

// То же, но с дополнительными заголовками (например, x-api-key).
std::string http_get_ex(const char* url, const std::vector<HttpHead>& headers = {},
                        const NetConfig& cfg = NetConfig());

// POST JSON (тело как body) — для Ollama /chat/completions.
std::string http_post_json(const char* url, const std::string& body,
                           const std::vector<HttpHead>& headers = {},
                           const NetConfig& cfg = NetConfig());

// Скачать url в файл dst.
// progress(bytes, total, *data) — коллбэк; data — произвольный указатель.
// Возвращает true при успехе. При частичной записи файл удаляется.
bool http_download(const char* url, const std::string& dst,
                   std::function<void(long long, long long, void*)> progress = nullptr,
                   void* data = nullptr, const NetConfig& cfg = NetConfig());

} // namespace sl