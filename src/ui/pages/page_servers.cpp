#include "pages_priv.h"
#include "../../backend/servers.h"
#include "../../core/paths.h"
#include <commctrl.h>
#include <windowsx.h>
#include <cstdio>

namespace slui {

enum {
    ID_V_CREATE = 2701, ID_V_NAME = 2702, ID_V_IP = 2703, ID_V_ADD = 2704,
    ID_V_LIST = 2705, ID_V_CONSOLE = 2706, ID_V_DELETE = 2707, ID_V_REFRESH = 2708,
    ID_V_STATUS = 2709,
};

struct ServersData {
    HWND name_edit = 0, ip_edit = 0, list = 0, status = 0;
    std::vector<sl::ServerInfo> servers;
};

static ServersData* V(Page* p) { return (ServersData*)p->data; }

static void fill_list(Page* p) {
    ServersData* d = V(p);
    d->servers = sl::servers::load_list();
    std::vector<std::string> items;
    for (auto& s : d->servers) {
        char buf[256];
        std::string managed = s.managed ? " [управляемый]" : "";
        sprintf_s(buf, "%s  |  %s%s", s.name.c_str(), s.ip.c_str(), managed.c_str());
        items.push_back(buf);
    }
    if (items.empty()) items.push_back("Нет серверов");
    sl_fill_list(d->list, items);
}

static void on_show(Page* p) { fill_list(p); }

static bool on_cmd(Page* p, int id, HWND src) {
    ServersData* d = V(p);
    switch (id) {
        case ID_V_CREATE: {
            sl_show_create_server_dialog(p->hwnd);
            fill_list(p);
            return true;
        }
        case ID_V_ADD: {
            std::string name = GetCtrl(d->name_edit);
            std::string ip = GetCtrl(d->ip_edit);
            if (name.empty() || ip.empty()) {
                sl_msg(p->hwnd, "Ошибка", "Заполните поля", MB_ICONWARNING);
                return true;
            }
            sl::ServerInfo si;
            si.name = name;
            si.ip = ip;
            d->servers.push_back(si);
            sl::servers::save_list(d->servers);
            fill_list(p);
            return true;
        }
        case ID_V_CONSOLE: {
            int sel = (int)SendMessageW(d->list, LB_GETCURSEL, 0, 0);
            if (sel < 0 || (size_t)sel >= d->servers.size()) return true;
            sl::ServerInfo& s = d->servers[sel];
            if (!s.managed) { sl_msg(p->hwnd, "Внимание", "Сервер не управляемый", MB_ICONINFORMATION); return true; }
            // открыть консоль — упрощённо запускаем
            SetCtrl(d->status, "Консоль: запуск " + s.name + "...");
            sl::servers::start_server(s.dir_path.empty() ? sl::path_join(sl::path_join(sl::app_root_utf8(), "servers"), s.name) : s.dir_path,
                                      s.ram_gb > 0 ? s.ram_gb : 4, s.version, s.core, "");
            SetCtrl(d->status, "Сервер запущен (" + s.name + ")");
            return true;
        }
        case ID_V_DELETE: {
            int sel = (int)SendMessageW(d->list, LB_GETCURSEL, 0, 0);
            if (sel < 0 || (size_t)sel >= d->servers.size()) return true;
            std::string name = d->servers[sel].name;
            if (!sl_confirm(p->hwnd, "Удаление", "Удалить сервер '" + name + "'?")) return true;
            auto& servers = d->servers;
            bool managed = servers[sel].managed;
            servers.erase(servers.begin() + sel);
            sl::servers::save_list(servers);
            if (managed) {
                std::string dir = sl::path_join(sl::path_join(sl::app_root_utf8(), "servers"), name);
                // удалить каталог (простой способ через cmd)
                char buf[512];
                sprintf_s(buf, "rmdir /s /q \"%s\"", dir.c_str());
                system(buf);
            }
            fill_list(p);
            return true;
        }
        case ID_V_REFRESH:
            fill_list(p);
            return true;
    }
    return false;
}

HWND create_servers_page(HINSTANCE hi, HWND parent, LauncherApp* app) {
    Page* p = new Page;
    HWND h = create_page_common(hi, parent, app, p);
    ServersData* d = new ServersData;
    p->data = d;
    p->on_cmd = on_cmd;
    p->on_show = on_show;

    int x = 40, y = 30, cw = 680;
    MakeLabel(h, 0, "Серверы", x, y, cw, 30);
    y += 44;

    MakeButton(h, ID_V_CREATE, "Создать сервер", x, y, 200, 36);
    y += 52;

    MakeLabel(h, 0, "Имя:", x, y, 80, 24);
    d->name_edit = MakeEdit(h, ID_V_NAME, "", x + 80, y - 3, 150, 26);
    MakeLabel(h, 0, "IP или домен:", x + 246, y, 120, 24);
    d->ip_edit = MakeEdit(h, ID_V_IP, "", x + 356, y - 3, 200, 26);
    MakeButton(h, ID_V_ADD, "Добавить", x + 568, y - 3, 90, 30);
    y += 44;

    d->list = MakeList(h, ID_V_LIST, x, y, cw, 300);
    y += 316;

    MakeButton(h, ID_V_CONSOLE, "Консоль / запустить", x, y, 180, 36);
    MakeButton(h, ID_V_DELETE, "Удалить", x + 196, y, 120, 36);
    MakeButton(h, ID_V_REFRESH, "Обновить", x + 332, y, 120, 36);
    y += 48;
    d->status = MakeLabel(h, ID_V_STATUS, "", x, y, cw, 22);
    return h;
}

} // namespace slui