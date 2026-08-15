#include "pages_priv.h"
#include "../../backend/mods.h"
#include "../../core/paths.h"
#include <commctrl.h>
#include <windowsx.h>
#include <cstdio>

namespace slui {

enum {
    ID_B_SOURCE = 2301, ID_B_SEARCH = 2302, ID_B_INPUT = 2303,
    ID_B_RESULTS = 2304, ID_B_INSTALL = 2305, ID_B_MRPACK = 2306,
    ID_B_INSTALLED = 2307, ID_B_REFRESH = 2308, ID_B_UNINSTALL = 2309,
    ID_B_OPENMC = 2310, ID_B_PROGRESS = 2311, ID_B_STATUS = 2312,
};

struct BuildsData {
    HWND source_combo = 0, input = 0, results = 0, installed = 0;
    HWND progress = 0, status = 0;
    std::vector<sl::mods::ModEntry> entries;
    std::vector<sl::mods::InstalledPack> packs;
    std::string last_query;
    bool search_working = false;
    bool install_working = false;
};

static BuildsData* B(Page* p) { return (BuildsData*)p->data; }
static std::string mc_dir() { return sl::minecraft_directory(); }

static void fill_installed(BuildsData* d) {
    d->packs = sl::mods::get_installed_packs(mc_dir());
    std::vector<std::string> items;
    for (auto& pk : d->packs) {
        char buf[256];
        sprintf_s(buf, "● %s   |  MC %s  %s  |  %s  |  файлов: %d  |  %s",
                  pk.name.c_str(), pk.mc_version.c_str(), pk.loader.c_str(),
                  pk.source.c_str(), (int)pk.files.size(), pk.installed_at.c_str());
        items.push_back(buf);
    }
    if (items.empty()) items.push_back("Нет установленных сборок");
    sl_fill_list(d->installed, items);
}

static void on_event(Page* p, PageEvent* ev) {
    BuildsData* d = B(p);
    switch (ev->kind) {
        case PE_MODS: {
            d->search_working = false;
            std::vector<sl::mods::ModEntry>* list = (std::vector<sl::mods::ModEntry>*)ev->data;
            if (list) { d->entries = *list; delete list; } else d->entries.clear();
            std::vector<std::string> items;
            for (auto& m : d->entries) {
                char buf[256];
                sprintf_s(buf, "%s  ⬇%s  — %s", m.name.c_str(),
                          fmt_number(m.downloads).c_str(), m.description.c_str());
                items.push_back(buf);
            }
            if (items.empty()) items.push_back("Ничего не найдено");
            sl_fill_list(d->results, items);
            SetCtrl(d->status, d->entries.empty() ? "Ничего не найдено" :
                     "Найдено: " + std::to_string(d->entries.size()));
            break;
        }
        case PE_STATUS:
            SetCtrl(d->status, ev->a);
            break;
        case PE_PROGRESS:
            SendMessageW(d->progress, PBM_SETPOS, (WPARAM)ev->n, 0);
            break;
        case PE_DONE: {
            d->install_working = false;
            SetCtrl(d->status, ev->a);
            if (ev->n == 0) fill_installed(d);
            break;
        }
        case PE_INSTALLED:
            fill_installed(d);
            break;
    }
}

static void on_show(Page* p) {
    fill_installed(B(p));
}

static bool on_cmd(Page* p, int id, HWND src) {
    BuildsData* d = B(p);
    switch (id) {
        case ID_B_SEARCH: {
            if (d->search_working) return true;
            d->search_working = true;
            d->last_query = GetCtrl(d->input);
            std::string source = sl_combo_sel(d->source_combo);
            SetCtrl(d->status, "Поиск...");
            sl_thread([p, source]() {
                std::vector<sl::mods::ModEntry>* list =
                    new std::vector<sl::mods::ModEntry>;
                *list = sl::mods::search_modpacks(GetCtrl(B(p)->input), 25, source);
                PageEvent* e = new PageEvent;
                e->kind = PE_MODS; e->page = p->hwnd; e->data = list;
                post_event(p->app->hwnd, e);
            });
            return true;
        }
        case ID_B_INSTALL: {
            if (d->install_working) return true;
            int sel = (int)SendMessageW(d->results, LB_GETCURSEL, 0, 0);
            if (sel < 0 || (size_t)sel >= d->entries.size()) return true;
            sl::mods::ModEntry m = d->entries[sel];
            d->install_working = true;
            SendMessageW(d->progress, PBM_SETPOS, 0, 0);
            ShowWindow(d->progress, SW_SHOW);
            SetCtrl(d->status, "Получение версий...");
            sl_thread([p, d, m]() {
                auto vers = sl::mods::get_versions(m.id, m.source);
                // выбор: первая версия
                if (vers.empty()) {
                    PageEvent* e = new PageEvent;
                    e->kind = PE_DONE; e->page = p->hwnd; e->a = "Нет версий"; e->n = -1;
                    post_event(p->app->hwnd, e);
                    return;
                }
                std::string url = vers[0].url;
                // curseforge: получить download-url
                if (url.empty() && m.source == "curseforge" && vers[0].file_id) {
                    url = sl::mods::version_download_url(m.id, vers[0].file_id);
                }
                if (url.empty()) {
                    PageEvent* e = new PageEvent;
                    e->kind = PE_DONE; e->page = p->hwnd; e->a = "Нет ссылки"; e->n = -1;
                    post_event(p->app->hwnd, e);
                    return;
                }
                std::string name, err;
                auto cb = [p, d](const std::string& s, float done, float total) {
                    PageEvent* e = new PageEvent;
                    e->kind = PE_STATUS; e->page = p->hwnd; e->a = s;
                    if (total > 0) e->n = (long long)(done * 100 / total);
                    post_event(p->app->hwnd, e);
                };
                bool ok = sl::mods::install_modpack(url, false, mc_dir(), cb, name, err);
                PageEvent* e = new PageEvent;
                e->kind = PE_DONE; e->page = p->hwnd; e->n = ok ? 0 : -1;
                e->a = ok ? ("Сборка «" + name + "» установлена!") :
                        ("Ошибка: " + (err.empty() ? "?" : err));
                post_event(p->app->hwnd, e);
                if (d->progress) ShowWindow(d->progress, SW_HIDE);
            });
            return true;
        }
        case ID_B_MRPACK: {
            // выбор локального .mrpack
            OPENFILENAMEA ofn = {0};
            char file[MAX_PATH] = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = p->hwnd;
            ofn.lpstrFilter = "Modpacks (*.mrpack *.zip)\0*.mrpack;*.zip\0\0";
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST;
            if (!GetOpenFileNameA(&ofn)) return true;
            if (d->install_working) return true;
            d->install_working = true;
            ShowWindow(d->progress, SW_SHOW);
            SendMessageW(d->progress, PBM_SETPOS, 0, 0);
            SetCtrl(d->status, "Импорт .mrpack...");
            sl_thread([p, d, path = std::string(file)]() {
                std::string name, err;
                auto cb = [p](const std::string& s, float done, float total) {
                    PageEvent* e = new PageEvent;
                    e->kind = PE_STATUS; e->page = p->hwnd; e->a = s;
                    if (total > 0) e->n = (long long)(done * 100 / total);
                    post_event(p->app->hwnd, e);
                };
                bool ok = sl::mods::install_modpack(path, true, mc_dir(), cb, name, err);
                PageEvent* e = new PageEvent;
                e->kind = PE_DONE; e->page = p->hwnd; e->n = ok ? 0 : -1;
                e->a = ok ? ("Сборка «" + name + "» импортирована!") :
                        ("Ошибка: " + (err.empty() ? "?" : err));
                post_event(p->app->hwnd, e);
            });
            return true;
        }
        case ID_B_REFRESH:
            fill_installed(d);
            return true;
        case ID_B_OPENMC: {
            ShellExecuteW(nullptr, L"open", sl::s2ws(mc_dir()).c_str(), nullptr, nullptr, SW_SHOW);
            return true;
        }
        case ID_B_UNINSTALL: {
            int sel = (int)SendMessageW(d->installed, LB_GETCURSEL, 0, 0);
            if (sel < 0 || (size_t)sel >= d->packs.size()) return true;
            std::string name = d->packs[sel].name;
            if (!sl_confirm(p->hwnd, "Удаление", "Удалить сборку «" + name +
                                            "»? Будут удалены файлы, установленные с ней."))
                return true;
            auto res = sl::mods::uninstall_pack(mc_dir(), name);
            SetCtrl(d->status, res.second);
            fill_installed(d);
            return true;
        }
    }
    return false;
}

HWND create_builds_page(HINSTANCE hi, HWND parent, LauncherApp* app) {
    Page* p = new Page;
    HWND h = create_page_common(hi, parent, app, p);
    BuildsData* d = new BuildsData;
    p->data = d;
    p->on_cmd = on_cmd;
    p->on_event = on_event;
    p->on_show = on_show;

    int x = 40, y = 30, cw = 680;
    MakeLabel(h, 0, "▣   Сборки (Modpacks)", x, y, cw, 30);
    y += 44;

    MakeLabel(h, 0, "Источник:", x, y, 90, 24);
    d->source_combo = MakeCombo(h, ID_B_SOURCE, x + 90, y - 3, 140, 200);
    sl_fill_combo(d->source_combo, { "Modrinth", "CurseForge" });
    d->input = MakeEdit(h, ID_B_INPUT, "", x + 250, y - 3, 200, 26);
    MakeButton(h, ID_B_SEARCH, "Поиск", x + 460, y - 3, 100, 30);
    MakeButton(h, ID_B_MRPACK, ".mrpack", x + 570, y - 3, 80, 30);
    y += 40;

    d->results = MakeList(h, ID_B_RESULTS, x, y, cw, 190);
    y += 206;
    d->progress = MakeProgress(h, ID_B_PROGRESS, x, y, cw, 10);
    ShowWindow(d->progress, SW_HIDE);
    y += 26;
    MakeButton(h, ID_B_INSTALL, "Установить выбранную", x, y, 200, 34);
    y += 46;

    d->installed = MakeList(h, ID_B_INSTALLED, x, y, cw, 170);
    y += 186;
    MakeButton(h, ID_B_REFRESH, "Обновить", x, y, 120, 32);
    MakeButton(h, ID_B_OPENMC, "Открыть .minecraft", x + 136, y, 180, 32);
    MakeButton(h, ID_B_UNINSTALL, "Удалить сборку", x + 332, y, 160, 32);
    y += 42;
    d->status = MakeLabel(h, ID_B_STATUS, "Сборки устанавливаются в папку .minecraft. Перед установкой старые моды сохраняются в backup.", x, y, cw, 40);

    fill_installed(d);
    return h;
}

} // namespace slui