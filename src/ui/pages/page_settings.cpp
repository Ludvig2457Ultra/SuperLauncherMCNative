#include "pages_priv.h"
#include "../../core/config.h"
#include "../../net/http.h"
#include "../../minecraft/install.h"
#include <commctrl.h>
#include <windowsx.h>
#include <cstdio>
#include <string>

namespace slui {

enum {
    ID_SET_SAVE = 2801, ID_SET_JAVA = 2802, ID_SET_JAVA_BROWSE = 2803,
    ID_SET_RAM = 2804, ID_SET_RAM_LBL = 2805, ID_SET_JVM = 2806,
    ID_SET_CF = 2807, ID_SET_CF_TEST = 2808, ID_SET_THEME = 2809,
    ID_SET_LANG = 2810, ID_SET_MODE = 2811, ID_SET_PROXY = 2812,
    ID_SET_PROXY_PORT = 2813,
};

struct SettingsData {
    HWND theme_combo = 0, lang_combo = 0, mode_combo = 0;
    HWND java_edit = 0, ram_slider = 0, ram_lbl = 0;
    HWND jvm_edit = 0, cf_edit = 0, proxy_edit = 0, proxy_port_edit = 0;
};

static SettingsData* S(Page* p) { return (SettingsData*)p->data; }

static void on_theme_change(SettingsData* d) {
    // смена темы применяется только после сохранения — просто обновим ярлык
    int ram = (int)SendMessageW(d->ram_slider, TBM_GETPOS, 0, 0);
    SetCtrl(d->ram_lbl, std::to_string(ram) + " MB");
}

static bool on_cmd(Page* p, int id, HWND src) {
    SettingsData* d = S(p);
    switch (id) {
        case ID_SET_JAVA_BROWSE: {
            OPENFILENAMEA ofn = {0};
            char file[MAX_PATH] = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = p->hwnd;
            ofn.lpstrFilter = "Java (*.exe)\0*.exe\0\0";
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST;
            if (GetOpenFileNameA(&ofn)) SetCtrl(d->java_edit, file);
            return true;
        }
        case ID_SET_CF_TEST: {
            std::string key = GetCtrl(d->cf_edit);
            if (key.empty()) { sl_msg(p->hwnd, "Ошибка", "Введите ключ", MB_ICONWARNING); return true; }
            sl_thread([p, key]() {
                std::vector<sl::HttpHead> hd = { { "x-api-key", key } };
                std::string body = sl::http_get_ex(
                    "https://api.curseforge.com/v1/mods/search?gameId=432&classId=6&pageSize=1",
                    hd);
                if (!body.empty()) {
                    sl_msg(p->hwnd, "Успех", "Ключ работает!", MB_ICONINFORMATION);
                } else {
                    sl_msg(p->hwnd, "Ошибка", "HTTP ошибка", MB_ICONERROR);
                }
            });
            return true;
        }
        case ID_SET_SAVE: {
            sl::Config cfg;
            cfg.load();
            cfg.theme = sl_combo_sel(d->theme_combo);
            cfg.language = sl_combo_sel(d->lang_combo);
            cfg.launch_mode = sl_combo_sel(d->mode_combo);
            cfg.java_path = GetCtrl(d->java_edit);
            int ram = (int)SendMessageW(d->ram_slider, TBM_GETPOS, 0, 0);
            cfg.max_ram = ram;
            cfg.jvm_args = GetCtrl(d->jvm_edit);
            cfg.curseforge_api_key = GetCtrl(d->cf_edit);
            cfg.proxy_host = GetCtrl(d->proxy_edit);
            try { cfg.proxy_port = std::stoi(GetCtrl(d->proxy_port_edit)); } catch (...) {}
            cfg.save();
            sl::set_network_proxy(cfg.proxy_host, cfg.proxy_port, cfg.proxy_user, cfg.proxy_pass);
            sl_msg(p->hwnd, "Готово", "Настройки сохранены", MB_ICONINFORMATION);
            return true;
        }
        case ID_SET_RAM: {
            on_theme_change(d);
            return true;
        }
    }
    return false;
}

static void on_show(Page* p) {
    SettingsData* d = S(p);
    sl::Config cfg;
    cfg.load();
    // не перезаписываем комбо с тему — только слайдер
    int ram = (int)cfg.max_ram;
    if (ram < 1024) ram = 1024;
    if (ram > 32768) ram = 32768;
    SendMessageW(d->ram_slider, TBM_SETPOS, TRUE, ram);
    SetCtrl(d->ram_lbl, std::to_string(ram) + " MB");
}

HWND create_settings_page(HINSTANCE hi, HWND parent, LauncherApp* app) {
    Page* p = new Page;
    HWND h = create_page_common(hi, parent, app, p);
    SettingsData* d = new SettingsData;
    p->data = d;
    p->on_cmd = on_cmd;
    p->on_show = on_show;

    sl::Config cfg;
    cfg.load();

    int x = 40, y = 24, cw = 680;
    MakeLabel(h, 0, "Настройки", x, y, cw, 28);
    y += 40;

    int lw = 220, fx = x + lw;

    MakeLabel(h, 0, "Тема:", x, y, lw, 22);
    d->theme_combo = MakeCombo(h, ID_SET_THEME, fx, y - 3, 180, 200);
    sl_fill_combo(d->theme_combo, { "dark", "light" },
                  cfg.theme == "light" ? 1 : 0);
    y += 34;

    MakeLabel(h, 0, "Язык:", x, y, lw, 22);
    d->lang_combo = MakeCombo(h, ID_SET_LANG, fx, y - 3, 180, 200);
    sl_fill_combo(d->lang_combo, { "ru", "en" }, cfg.language == "en" ? 1 : 0);
    y += 34;

    MakeLabel(h, 0, "Режим запуска:", x, y, lw, 22);
    d->mode_combo = MakeCombo(h, ID_SET_MODE, fx, y - 3, 220, 200);
    sl_fill_combo(d->mode_combo, { "minecraft-launcher-lib", "Java" },
                  cfg.launch_mode == "java" ? 1 : 0);
    y += 40;

    MakeLabel(h, 0, "Путь к Java:", x, y, lw, 22);
    d->java_edit = MakeEdit(h, ID_SET_JAVA, cfg.java_path, fx, y - 3, 360, 26);
    MakeButton(h, ID_SET_JAVA_BROWSE, "Обзор", fx + 372, y - 3, 80, 30);
    y += 40;

    MakeLabel(h, 0, "ОЗУ:", x, y, lw, 22);
    d->ram_slider = CreateWindowExW(0, TRACKBAR_CLASSW, 0,
                                    WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ,
                                    fx, y, 300, 24, h, (HMENU)(INT_PTR)ID_SET_RAM, hi, 0);
    SendMessageW(d->ram_slider, TBM_SETRANGE, TRUE, MAKELPARAM(1024, 32768));
    SendMessageW(d->ram_slider, TBM_SETPOS, TRUE, (int)cfg.max_ram);
    d->ram_lbl = MakeLabel(h, ID_SET_RAM_LBL, std::to_string(cfg.max_ram) + " MB",
                           fx + 320, y, 120, 24);
    y += 34;

    MakeLabel(h, 0, "JVM аргументы:", x, y, lw, 22);
    d->jvm_edit = MakeEdit(h, ID_SET_JVM, cfg.jvm_args, fx, y - 3, 420, 26);
    y += 40;

    MakeLabel(h, 0, "CurseForge API Key:", x, y, lw, 22);
    d->cf_edit = MakeEdit(h, ID_SET_CF, cfg.curseforge_api_key, fx, y - 3, 360, 26);
    MakeButton(h, ID_SET_CF_TEST, "Проверить", fx + 372, y - 3, 100, 30);
    y += 40;

    MakeLabel(h, 0, "Прокси (хост):", x, y, lw, 22);
    d->proxy_edit = MakeEdit(h, ID_SET_PROXY, cfg.proxy_host, fx, y - 3, 260, 26);
    MakeLabel(h, 0, "Порт:", x + 340, y, 50, 22);
    d->proxy_port_edit = MakeEdit(h, ID_SET_PROXY_PORT, std::to_string(cfg.proxy_port),
                                  x + 400, y - 3, 90, 26);
    y += 44;

    MakeButtonAccent(h, ID_SET_SAVE, "Сохранить", x, y, 160, 40);
    return h;
}

} // namespace slui