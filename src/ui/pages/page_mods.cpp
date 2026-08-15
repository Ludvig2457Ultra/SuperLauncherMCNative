#include "pages_priv.h"
#include "../../backend/mods.h"
#include "../../core/paths.h"
#include "../../core/config.h"
#include <commctrl.h>
#include <windowsx.h>
#include <cstdio>

namespace slui {

enum {
    ID_M_SOURCE = 2201, ID_M_SEARCH = 2202, ID_M_INPUT = 2203,
    ID_M_LIST = 2204, ID_M_OPEN = 2205, ID_M_DELALL = 2206, ID_M_INSTALL = 2207,
    ID_M_STATUS = 2208,
};

struct ModsData {
    HWND source_combo = 0, input = 0, list = 0, status = 0;
    std::vector<sl::mods::ModEntry> entries;
    std::string last_query;
    bool working = false;
};

static ModsData* M(Page* p) { return (ModsData*)p->data; }
static std::string mods_dir() {
    return sl::path_join(sl::minecraft_directory(), "mods");
}

static void fill_list(ModsData* d) {
    std::vector<std::string> items;
    for (auto& m : d->entries) {
        char buf[256];
        sprintf_s(buf, "%s  ⬇%s  — %s", m.name.c_str(), fmt_number(m.downloads).c_str(),
                  m.description.c_str());
        items.push_back(buf);
    }
    if (items.empty()) items.push_back("Ничего не найдено");
    sl_fill_list(d->list, items);
}

static void on_event(Page* p, PageEvent* ev) {
    ModsData* d = M(p);
    switch (ev->kind) {
        case PE_MODS: {
            d->working = false;
            std::vector<sl::mods::ModEntry>* list = (std::vector<sl::mods::ModEntry>*)ev->data;
            if (list) { d->entries = *list; delete list; }
            else d->entries.clear();
            fill_list(d);
            SetCtrl(d->status, d->entries.empty() ? "Ничего не найдено" :
                     "Найдено: " + std::to_string(d->entries.size()));
            break;
        }
        case PE_STATUS:
            SetCtrl(d->status, ev->a);
            d->working = false;
            break;
        case PE_DONE:
            d->working = false;
            SetCtrl(d->status, ev->a);
            break;
    }
}

static bool on_cmd(Page* p, int id, HWND src) {
    ModsData* d = M(p);
    switch (id) {
        case ID_M_SEARCH: {
            if (d->working) return true;
            d->working = true;
            d->last_query = GetCtrl(d->input);
            std::string source = sl_combo_sel(d->source_combo);
            SetCtrl(d->status, "Поиск...");
            sl_thread([p, source]() {
                std::vector<sl::mods::ModEntry>* list =
                    new std::vector<sl::mods::ModEntry>;
                *list = sl::mods::search_mods(GetCtrl(M(p)->input), 30, source);
                PageEvent* e = new PageEvent;
                e->kind = PE_MODS; e->page = p->hwnd; e->data = list;
                post_event(p->app->hwnd, e);
            });
            return true;
        }
        case ID_M_OPEN: {
            sl::mkdirs(mods_dir());
            ShellExecuteW(nullptr, L"open", sl::s2ws(mods_dir()).c_str(), nullptr, nullptr, SW_SHOW);
            return true;
        }
        case ID_M_DELALL: {
            if (!sl_confirm(p->hwnd, "Подтверждение", "Удалить все моды?")) return true;
            int count = 0;
            WIN32_FIND_DATAA fd;
            HANDLE hf = FindFirstFileA((mods_dir() + "\\*.jar").c_str(), &fd);
            if (hf != INVALID_HANDLE_VALUE) {
                do {
                    sl::remove_file(mods_dir() + "\\" + fd.cFileName);
                    count++;
                } while (FindNextFileA(hf, &fd));
                FindClose(hf);
            }
            SetCtrl(d->status, "Удалено: " + std::to_string(count));
            return true;
        }
        case ID_M_INSTALL: {
            if (d->working) return true;
            int sel = (int)SendMessageW(d->list, LB_GETCURSEL, 0, 0);
            if (sel < 0 || (size_t)sel >= d->entries.size()) return true;
            sl::mods::ModEntry m = d->entries[sel];
            // получить версии и взять первую с подходящим файлом
            d->working = true;
            SetCtrl(d->status, "Получение версий...");
            sl_thread([p, d, m]() {
                std::string src = m.source;
                auto vers = sl::mods::get_versions(m.id, src);
                std::string url, fname, err;
                if (!vers.empty()) {
                    url = vers[0].url;
                    fname = vers[0].filename;
                    if (url.empty() && src == "curseforge" && vers[0].file_id) {
                        url = sl::mods::version_download_url(m.id, vers[0].file_id);
                    }
                }
                if (url.empty()) {
                    PageEvent* e = new PageEvent;
                    e->kind = PE_DONE; e->page = p->hwnd; e->a = "Не найдена ссылка для скачивания";
                    post_event(p->app->hwnd, e);
                    return;
                }
                if (fname.empty()) fname = sl::file_name(url);
                std::string dst = sl::path_join(mods_dir(), fname);
                sl::mkdirs(mods_dir());
                bool ok = sl::mods::download_mod_file(url, dst);
                PageEvent* e = new PageEvent;
                e->kind = PE_DONE; e->page = p->hwnd;
                e->a = ok ? "Мод установлен: " + fname : "Ошибка загрузки";
                post_event(p->app->hwnd, e);
            });
            return true;
        }
        case ID_M_LIST: {
            // двойной клик = установка
            if (HIWORD(src) == LBN_DBLCLK) {
                // сэмулируем установку
                return on_cmd(p, ID_M_INSTALL, src);
            }
            return true;
        }
    }
    return false;
}

static void on_show(Page* p) {
    ModsData* d = M(p);
    if (d->entries.empty()) {
        // загрузить featured
        d->working = true;
        SetCtrl(d->status, "Загрузка...");
        sl_thread([p]() {
            std::vector<sl::mods::ModEntry>* list =
                new std::vector<sl::mods::ModEntry>;
            *list = sl::mods::search_mods("", 20, "modrinth");
            PageEvent* e = new PageEvent;
            e->kind = PE_MODS; e->page = p->hwnd; e->data = list;
            post_event(p->app->hwnd, e);
        });
    }
}

HWND create_mods_page(HINSTANCE hi, HWND parent, LauncherApp* app) {
    Page* p = new Page;
    HWND h = create_page_common(hi, parent, app, p);
    ModsData* d = new ModsData;
    p->data = d;
    p->on_cmd = on_cmd;
    p->on_event = on_event;
    p->on_show = on_show;

    int x = 40, y = 30, cw = 680;
    MakeLabel(h, 0, "▣   Моды", x, y, cw, 30);
    y += 44;

    MakeLabel(h, 0, "Источник:", x, y, 90, 24);
    d->source_combo = MakeCombo(h, ID_M_SOURCE, x + 90, y - 3, 140, 200);
    sl_fill_combo(d->source_combo, { "Modrinth", "CurseForge" });
    d->input = MakeEdit(h, ID_M_INPUT, "", x + 250, y - 3, 240, 26);
    MakeButton(h, ID_M_SEARCH, "Поиск", x + 500, y - 3, 110, 30);
    y += 40;

    d->list = MakeList(h, ID_M_LIST, x, y, cw, 320);
    y += 336;

    d->status = MakeLabel(h, ID_M_STATUS, "", x, y, cw, 22);
    y += 30;
    MakeButton(h, ID_M_OPEN, "Открыть папку", x, y, 140, 36);
    MakeButton(h, ID_M_INSTALL, "Установить выбранный", x + 156, y, 200, 36);
    MakeButton(h, ID_M_DELALL, "Удалить всё", x + 372, y, 140, 36);
    return h;
}

} // namespace slui