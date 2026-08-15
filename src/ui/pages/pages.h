#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace sl {
struct ManifestVersion;
}
namespace sl::mods { struct ModEntry; struct ModVersion; struct InstalledPack; }
namespace sl::updates { struct ReleaseInfo; }
namespace sl { struct ServerInfo; }

namespace slui {

struct LauncherApp;

// ---------------- событийная шина ----------------
// Событие от рабочего потока к конкретной странице.
enum PageEventKind {
    PE_STATUS = 1,      // a = текст статуса
    PE_PROGRESS = 2,    // n = процент 0..100
    PE_LIST = 3,        // data = std::vector<std::string>* (владеет вызывающий)
    PE_DONE = 4,        // n = код завершения
    PE_VERSIONS = 5,    // data = std::vector<sl::ManifestVersion>*
    PE_RELEASES = 6,    // data = std::vector<sl::updates::ReleaseInfo>*
    PE_MODS = 7,        // data = std::vector<sl::mods::ModEntry>*
    PE_MODVERSIONS = 8, // data = std::vector<sl::mods::ModVersion>*
    PE_INSTALLED = 9,   // data = std::vector<sl::mods::InstalledPack>*
    PE_AI = 10,         // a = текст ответа AI
    PE_CHAT = 11,       // a = строка чата
    PE_SERVERS = 12,    // data = std::vector<sl::ServerInfo>*
};

struct PageEvent {
    int kind = 0;
    HWND page = nullptr;   // целевая страница (главное окно, если nullptr)
    std::string a;
    std::string b;
    long long n = 0;
    void* data = nullptr;  // НЕ владеет указателем
};

// ---------------- страница ----------------
struct Page {
    LauncherApp* app = nullptr;
    HWND hwnd = nullptr;
    void* data = nullptr;  // данные страницы
    bool (*on_cmd)(Page* p, int id, HWND src) = nullptr;
    void (*on_event)(Page* p, PageEvent* ev) = nullptr;
    void (*on_show)(Page* p) = nullptr;
};

// Поиск страницы по HWND (из g_pages).
Page* page_of(HWND h);

// Функции создания 11 страниц (порядок совпадает с сайдбаром).
HWND create_home_page(HINSTANCE h, HWND parent, LauncherApp* app);
HWND create_account_page(HINSTANCE h, HWND parent, LauncherApp* app);
HWND create_mods_page(HINSTANCE h, HWND parent, LauncherApp* app);
HWND create_builds_page(HINSTANCE h, HWND parent, LauncherApp* app);
HWND create_skins_page(HINSTANCE h, HWND parent, LauncherApp* app);
HWND create_news_page(HINSTANCE h, HWND parent, LauncherApp* app);
HWND create_updates_page(HINSTANCE h, HWND parent, LauncherApp* app);
HWND create_servers_page(HINSTANCE h, HWND parent, LauncherApp* app);
HWND create_settings_page(HINSTANCE h, HWND parent, LauncherApp* app);
HWND create_launch_page(HINSTANCE h, HWND parent, LauncherApp* app);
HWND create_ai_page(HINSTANCE h, HWND parent, LauncherApp* app);

// Диспетчер WM_COMMAND (вызывается главным окном).
void pages_on_command(LauncherApp* app, int id, HWND src);
// Диспетчер событий шины (WM_SL_EVENT).
void pages_on_event(LauncherApp* app, PageEvent* ev);
// Уведомление о показе страницы (обновление данных).
void pages_on_show(LauncherApp* app, int idx);

// Создать базовое окно страницы (SLPage) и зарегистрировать Page*.
HWND create_page_common(HINSTANCE hi, HWND parent, LauncherApp* app, Page* p);

// Постинг события в главное окно (из потоков).
void post_event(HWND main, PageEvent* ev);

// Регистрация Minecraft-страницы для приёма статусов запуска.
void sl_register_launch_page(Page* p);

// Переключение страниц (из ui.cpp).
void sl_navigate(LauncherApp* app, int idx);

// Форматирование числа (1.0M / 999K / 123).
std::string fmt_number(long long n);

// Запуск Minecraft (из ui.cpp).
void ui_start_launch(LauncherApp* app, const std::string& version_id,
                     const std::string& mc_dir, const std::string& loader = "Vanilla",
                     const std::string& loader_version = "");

// Статусы (совместимость: идут на Minecraft-страницу).
void pages_on_status(LauncherApp* app, const std::string& s);
void pages_on_progress(LauncherApp* app, long long done, long long total);
void pages_on_done(LauncherApp* app, int code);
void pages_on_versions(LauncherApp* app, std::vector<sl::ManifestVersion>* list);

} // namespace slui