#include "pages_priv.h"
#include "../../core/config.h"
#include "../../core/paths.h"
#include "../../core/log.h"
#include "../../minecraft/version.h"
#include "../../minecraft/install.h"
#include "../../minecraft/command.h"
#include "../../backend/mods.h"
#include <commctrl.h>
#include <windowsx.h>
#include <cstdio>

namespace slui {

// Идентификаторы контролов
enum {
    ID_L_PLAY = 2901,
    ID_L_PROFILE = 2902,
    ID_L_USERNAME = 2903,
    ID_L_VERSION = 2904,
    ID_L_LOADER = 2905,
    ID_L_PROGRESS = 2906,
    ID_L_MEMO = 2907,
};

struct LaunchData {
    HWND profile_combo = 0;
    HWND username = 0;
    HWND version_combo = 0;
    HWND loader_combo = 0;
    HWND progress = 0;
    HWND memo = 0;
    HWND play = 0;
    std::vector<sl::ManifestVersion> manifest;
    std::vector<std::string> installed;
    std::vector<std::pair<std::string, std::string>> profiles; // title -> json path
};

static LaunchData* L(Page* p) { return (LaunchData*)p->data; }

static std::string mc_dir() { return sl::minecraft_directory(); }

// Загрузка профилей из profiles/*.json
static void load_profiles(LaunchData* d) {
    d->profiles.clear();
    // профили лежат рядом с exe в profiles/
    std::string base = "profiles";
    if (!sl::file_exists(base)) base = sl::path_join(sl::app_root_utf8(), "profiles");
    if (!sl::file_exists(base)) return;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((base + "\\*.json").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::string path = base + "\\" + fd.cFileName;
        std::string text = sl::read_file_text(path);
        sl::json::Value v;
        if (!text.empty() && sl::json::parse(text, v)) {
            std::string name, title;
            if (const sl::json::Value* x = v.get("name"); x && x->is_str()) name = x->as_string();
            if (const sl::json::Value* x = v.get("title"); x && x->is_str()) title = x->as_string();
            if (title.empty()) title = name.empty() ? fd.cFileName : name;
            d->profiles.push_back({ title, path });
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

// Скачивание модов профиля в mc_dir/mods
static bool install_profile_mods(const std::string& json_path, const std::string& mcdir,
                                 std::function<void(const std::string&)> status) {
    std::string text = sl::read_file_text(json_path);
    sl::json::Value v;
    if (text.empty() || !sl::json::parse(text, v)) return true;
    const sl::json::Value* mods = v.get("mods");
    if (!mods || !mods->is_arr()) return true;
    std::string mods_dir = sl::path_join(mcdir, "mods");
    sl::mkdirs(mods_dir);
    int total = (int)mods->size(), done = 0;
    for (size_t i = 0; i < mods->size(); i++) {
        const sl::json::Value& m = mods->at(i);
        if (!m.is_obj()) continue;
        std::string source, name, url;
        if (const sl::json::Value* x = m.get("source"); x && x->is_str()) source = x->as_string();
        if (const sl::json::Value* x = m.get("name"); x && x->is_str()) name = x->as_string();
        if (const sl::json::Value* x = m.get("url"); x && x->is_str()) url = x->as_string();
        std::string fname = sl::file_name(url);
        if (fname.empty()) fname = name + ".jar";
        std::string dst = sl::path_join(mods_dir, fname);
        if (sl::file_exists(dst)) { done++; continue; }
        if (source == "local") {
            std::string src = sl::path_join(sl::parent_dir(json_path), fname);
            if (sl::file_exists(src)) sl::copy_file(src, dst);
            done++;
        } else if (!url.empty()) {
            if (status) status("(" + std::to_string(done + 1) + "/" + std::to_string(total) +
                               ") Скачивание " + name + "...");
            sl::mods::download_mod_file(url, dst);
            done++;
        }
    }
    return true;
}

static void refresh_installed(LaunchData* d) {
    d->installed = sl::list_installed_versions(mc_dir());
}

static void fill_version_combo(LaunchData* d) {
    std::vector<std::string> items;
    items.push_back("latest_release");
    items.push_back("snapshot");
    for (auto& v : d->manifest)
        if (v.type == "release") {
            bool dup = false;
            for (auto& i : items) if (i == v.id) { dup = true; break; }
            if (!dup) items.push_back(v.id);
        }
    for (auto& v : d->manifest)
        if (v.type != "release") {
            bool dup = false;
            for (auto& i : items) if (i == v.id) { dup = true; break; }
            if (!dup) items.push_back(v.id);
        }
    for (auto& v : d->installed) {
        bool dup = false;
        for (auto& i : items) if (i == v) { dup = true; break; }
        if (!dup) items.push_back(v);
    }
    sl_fill_combo(d->version_combo, items);
}

static void on_profile_change(LaunchData* d) {
    std::string title = sl_combo_sel(d->profile_combo);
    // сбросить версию/лоадер на профиль
    for (auto& pr : d->profiles) {
        if (pr.first == title) {
            std::string text = sl::read_file_text(pr.second);
            sl::json::Value v;
            if (!text.empty() && sl::json::parse(text, v)) {
                if (const sl::json::Value* x = v.get("minecraft_version"); x && x->is_str())
                    sl_fill_combo(d->version_combo, { x->as_string() });
                if (const sl::json::Value* x = v.get("loader"); x && x->is_str()) {
                    std::string ld = x->as_string();
                    for (auto& c : ld) c = (char)tolower((unsigned char)c);
                    sl_fill_combo(d->loader_combo, { "Vanilla", "Fabric", "Forge", "Quilt", "NeoForge" }, 0);
                }
                return;
            }
        }
    }
    // сброс: версия — полный список, лоадер — Vanilla
    fill_version_combo(d);
    sl_fill_combo(d->loader_combo, { "Vanilla", "Fabric", "Forge", "Quilt", "NeoForge" }, 0);
}

static bool on_cmd(Page* p, int id, HWND src) {
    LaunchData* d = L(p);
    switch (id) {
        case ID_L_PLAY: {
            if (p->app->working) return true;
            std::string ver = sl_combo_sel(d->version_combo);
            if (ver.empty()) return true;
            std::string username = GetCtrl(d->username);
            if (username.empty()) username = "Player";
            // сохранить username
            sl::Config cfg;
            cfg.load();
            cfg.last_username = username;
            cfg.save();
            std::string mcdir = mc_dir();
            std::string profile_json;
            std::string title = sl_combo_sel(d->profile_combo);
            for (auto& pr : d->profiles) if (pr.first == title) { profile_json = pr.second; break; }
            std::string loader = sl_combo_sel(d->loader_combo);
            std::string loader_version; // полный выбор версии загрузчика не реализован — берём новейшую
            // запуск в потоке
            sl_thread([p, d, ver, username, mcdir, profile_json, loader, loader_version]() {
                if (!profile_json.empty()) {
                    install_profile_mods(profile_json, mcdir, [p](const std::string& s) {
                        PageEvent* e = new PageEvent; e->kind = PE_STATUS; e->page = p->hwnd; e->a = s;
                        post_event(p->app->hwnd, e);
                    });
                }
                PageEvent* e = new PageEvent;
                e->kind = PE_DONE; e->page = p->hwnd; e->n = 100;
                post_event(p->app->hwnd, e);
            });
            ui_start_launch(p->app, ver, mcdir, loader, loader_version);
            return true;
        }
        case ID_L_PROFILE:
            on_profile_change(d);
            return true;
    }
    return false;
}

static void on_event(Page* p, PageEvent* ev) {
    LaunchData* d = L(p);
    switch (ev->kind) {
        case PE_STATUS:
            sl_memo_append(d->memo, ev->a);
            break;
        case PE_PROGRESS:
            SendMessageW(d->progress, PBM_SETPOS, (WPARAM)ev->n, 0);
            break;
        case PE_DONE:
            if (ev->n != 100) p->app->working = false;
            break;
        case PE_VERSIONS:
            if (ev->data) {
                std::vector<sl::ManifestVersion>* list = (std::vector<sl::ManifestVersion>*)ev->data;
                d->manifest = *list;
                refresh_installed(d);
                fill_version_combo(d);
            }
            break;
    }
}

static void on_show(Page* p) {
    LaunchData* d = L(p);
    sl::Config cfg;
    cfg.load();
    if (cfg.last_username.empty()) cfg.last_username = "Player";
    SetCtrl(d->username, cfg.last_username);
}

HWND create_launch_page(HINSTANCE hi, HWND parent, LauncherApp* app) {
    Page* p = new Page;
    HWND h = create_page_common(hi, parent, app, p);
    LaunchData* d = new LaunchData;
    p->data = d;
    p->on_cmd = on_cmd;
    p->on_event = on_event;
    p->on_show = on_show;

    int x = 40, y = 30, cw = 560;
    MakeLabel(h, 0, "Запуск Minecraft", x, y, cw, 30);
    y += 44;

    sl::Config cfg;
    cfg.load();

    MakeLabel(h, 0, "Профиль:", x, y, 100, 24);
    d->profile_combo = MakeCombo(h, ID_L_PROFILE, x + 110, y - 3, 260, 200);
    y += 36;

    MakeLabel(h, 0, "Имя (username):", x, y, 140, 24);
    d->username = MakeEdit(h, ID_L_USERNAME, cfg.last_username.empty() ? "Player" : cfg.last_username, x + 140, y - 3, 200, 24);
    y += 40;

    MakeLabel(h, 0, "Версия:", x, y, 100, 24);
    d->version_combo = MakeCombo(h, ID_L_VERSION, x + 110, y - 3, 260, 220);
    MakeLabel(h, 0, "Загрузчик:", x + 390, y, 100, 24);
    d->loader_combo = MakeCombo(h, ID_L_LOADER, x + 470, y - 3, 140, 200);
    y += 44;

    d->play = MakeButtonAccent(h, ID_L_PLAY, "Играть", x, y, 180, 44);
    y += 58;
    d->progress = MakeProgress(h, ID_L_PROGRESS, x, y, cw, 10);
    y += 26;
    d->memo = MakeMemo(h, ID_L_MEMO, x, y, cw, 260);

    // профили
    load_profiles(d);
    std::vector<std::string> titles;
    titles.push_back("Стандарт");
    for (auto& pr : d->profiles) titles.push_back(pr.first);
    sl_fill_combo(d->profile_combo, titles);
    sl_fill_combo(d->loader_combo, { "Vanilla", "Fabric", "Forge", "Quilt", "NeoForge" });
    refresh_installed(d);
    fill_version_combo(d);

    sl_register_launch_page(p);
    return h;
}

} // namespace slui