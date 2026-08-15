#include "pages_priv.h"
#include "../../backend/updates.h"
#include "../../net/http.h"
#include "../../core/paths.h"
#include <commctrl.h>
#include <windowsx.h>
#include <cstdio>

namespace slui {

enum {
    ID_U_CHECK = 2601, ID_U_DL = 2602, ID_U_VER = 2603,
    ID_U_STATUS = 2604, ID_U_LIST = 2605, ID_U_PROGRESS = 2606,
};

struct UpdatesData {
    HWND ver_lbl = 0, status_lbl = 0, dl_btn = 0, progress = 0, list = 0;
    HWND update_card = 0;
    std::string dl_url;
    bool checking = false;
};

static UpdatesData* U(Page* p) { return (UpdatesData*)p->data; }
static const char* kCur = "v2.0.0_2026";

static void fill_history(UpdatesData* d) {
    std::vector<std::string> items;
    items.push_back("v2.0.0_2026  2026-01-01");
    items.push_back("v1.4.0.7  2025-08-12");
    items.push_back("v1.4.0.5  2025-07-24");
    items.push_back("v1.4.0.4  2025-07-23");
    items.push_back("v1.4.0.3  2025-07-23");
    items.push_back("v1.4.0.2  2025-06-26");
    items.push_back("v1.3  2025-06-26");
    sl_fill_list(d->list, items);
}

static void on_event(Page* p, PageEvent* ev) {
    UpdatesData* d = U(p);
    switch (ev->kind) {
        case PE_STATUS:
            SetCtrl(d->status_lbl, ev->a);
            break;
        case PE_RELEASES: {
            d->checking = false;
            std::vector<sl::updates::ReleaseInfo>* rels =
                (std::vector<sl::updates::ReleaseInfo>*)ev->data;
            if (!rels) break;
            // показать обновление если есть новее
            sl::updates::ReleaseInfo upd;
            bool has = false;
            for (auto& r : *rels) {
                if (sl::updates::compare_versions(r.tag, kCur) > 0) { upd = r; has = true; break; }
            }
            if (has) {
                d->dl_url = upd.dl_url;
                SetCtrl(d->status_lbl, "Доступно обновление: " + upd.tag);
                if (d->update_card) ShowWindow(d->update_card, SW_SHOW);
                if (d->dl_btn) {
                    ShowWindow(d->dl_btn, SW_SHOW);
                    SetCtrl(d->dl_btn, upd.dl_url.empty() ? "Нет файла" : "Скачать");
                }
            } else {
                SetCtrl(d->status_lbl, "Актуальная версия");
            }
            delete rels;
            break;
        }
        case PE_DONE:
            SetCtrl(d->status_lbl, "Скачано");
            break;
    }
}

static bool on_cmd(Page* p, int id, HWND src) {
    UpdatesData* d = U(p);
    switch (id) {
        case ID_U_CHECK: {
            if (d->checking) return true;
            d->checking = true;
            SetCtrl(d->status_lbl, "Проверка...");
            sl_thread([p]() {
                std::vector<sl::updates::ReleaseInfo>* rels =
                    new std::vector<sl::updates::ReleaseInfo>;
                if (!sl::updates::fetch_releases(*rels)) {
                    delete rels;
                    PageEvent* e = new PageEvent;
                    e->kind = PE_STATUS; e->page = p->hwnd; e->a = "Ошибка проверки";
                    post_event(p->app->hwnd, e);
                    return;
                }
                PageEvent* e = new PageEvent;
                e->kind = PE_RELEASES; e->page = p->hwnd; e->data = rels;
                post_event(p->app->hwnd, e);
            });
            return true;
        }
        case ID_U_DL: {
            if (d->dl_url.empty()) { sl_msg(p->hwnd, "Ошибка", "Нет ссылки на скачивание", MB_ICONWARNING); return true; }
            if (d->progress) ShowWindow(d->progress, SW_SHOW);
            sl_thread([p, d]() {
                std::string dst = "SuperLauncher_update.exe";
                auto prog = [p](long long done, long long total, void*) {
                    PageEvent* e = new PageEvent;
                    e->kind = PE_PROGRESS; e->page = p->hwnd;
                    e->n = total > 0 ? (long long)(done * 100 / total) : 0;
                    post_event(p->app->hwnd, e);
                };
                bool ok = sl::http_download(d->dl_url.c_str(), dst, prog, nullptr);
                PageEvent* e = new PageEvent;
                e->kind = PE_DONE; e->page = p->hwnd;
                e->a = ok ? "Готово" : "Ошибка загрузки";
                post_event(p->app->hwnd, e);
                if (ok) sl_msg(p->app->hwnd, "Готово", "Скачано в " + dst, MB_ICONINFORMATION);
                else sl_msg(p->app->hwnd, "Ошибка", "Не удалось скачать", MB_ICONERROR);
            });
            return true;
        }
    }
    return false;
}

HWND create_updates_page(HINSTANCE hi, HWND parent, LauncherApp* app) {
    Page* p = new Page;
    HWND h = create_page_common(hi, parent, app, p);
    UpdatesData* d = new UpdatesData;
    p->data = d;
    p->on_cmd = on_cmd;
    p->on_event = on_event;

    int x = 40, y = 30, cw = 680;
    MakeLabel(h, 0, "Обновления", x, y, cw, 30);
    y += 44;
    MakeLabel(h, 0, "Текущая версия:", x, y, 200, 24);
    d->ver_lbl = MakeLabel(h, ID_U_VER, kCur, x + 200, y, 200, 24);
    y += 32;
    d->status_lbl = MakeLabel(h, ID_U_STATUS, "Проверка...", x, y, 400, 24);
    y += 40;
    MakeButton(h, ID_U_CHECK, "Проверить обновления", x, y, 200, 36);
    d->dl_btn = MakeButton(h, ID_U_DL, "Скачать", x + 220, y, 140, 36);
    ShowWindow(d->dl_btn, SW_HIDE);
    d->update_card = d->dl_btn;
    y += 52;
    d->progress = MakeProgress(h, ID_U_PROGRESS, x, y, cw, 10);
    ShowWindow(d->progress, SW_HIDE);
    y += 30;
    MakeLabel(h, 0, "История версий:", x, y, cw, 22);
    y += 28;
    d->list = MakeList(h, ID_U_LIST, x, y, cw, 280);
    fill_history(d);
    return h;
}

} // namespace slui