#include "pages_priv.h"
#include "../../backend/account.h"
#include <windowsx.h>

namespace slui {

enum {
    ID_H_ACT_MC = 2001, ID_H_ACT_SET = 2002, ID_H_ACT_MODS = 2003,
    ID_H_ACT_SKINS = 2004, ID_H_ACT_SERVERS = 2005, ID_H_ACT_ACCOUNT = 2006,
    ID_H_NAME = 2007,
};

struct HomeData {
    HWND name_lbl = 0;
};

static HomeData* H(Page* p) { return (HomeData*)p->data; }

static bool on_cmd(Page* p, int id, HWND src) {
    switch (id) {
        case ID_H_ACT_MC: sl_navigate(p->app, 9); return true;
        case ID_H_ACT_SET: sl_navigate(p->app, 8); return true;
        case ID_H_ACT_MODS: sl_navigate(p->app, 2); return true;
        case ID_H_ACT_SKINS: sl_navigate(p->app, 4); return true;
        case ID_H_ACT_SERVERS: sl_navigate(p->app, 7); return true;
        case ID_H_ACT_ACCOUNT: sl_navigate(p->app, 1); return true;
    }
    return false;
}

static void on_show(Page* p) {
    HomeData* d = H(p);
    if (!d->name_lbl) return;
    sl::User* u = sl::account::current();
    std::string name = u ? u->username : "Гость";
    SetCtrl(d->name_lbl, "👤 " + name);
}

HWND create_home_page(HINSTANCE hi, HWND parent, LauncherApp* app) {
    Page* p = new Page;
    HWND h = create_page_common(hi, parent, app, p);
    HomeData* d = new HomeData;
    p->data = d;
    p->on_cmd = on_cmd;
    p->on_show = on_show;

    int x = 40, y = 30, cw = 560;
    MakeTitle(h, 0, "SuperLauncher 2026", x, y, cw, 40);
    y += 54;

    d->name_lbl = MakeLabel(h, ID_H_NAME, "👤 Гость", x, y, 300, 30);
    y += 48;

    MakeSub(h, 0, "Быстрые действия", x, y, 300, 24);
    y += 40;

    struct QA { int id; const char* txt; };
    static const QA qas[] = {
        { ID_H_ACT_MC, "▶ Minecraft" },
        { ID_H_ACT_SET, "⚙ Настройки" },
        { ID_H_ACT_MODS, "▣ Моды" },
        { ID_H_ACT_SKINS, "♠ Скины" },
        { ID_H_ACT_SERVERS, "⤡ Серверы" },
        { ID_H_ACT_ACCOUNT, "○ Аккаунт" },
    };
    int bw = 170, bh = 80, gx = 16, gy = 16;
    for (int i = 0; i < 6; i++) {
        int cx = x + (i % 3) * (bw + gx);
        int cy = y + (i / 3) * (bh + gy);
        if (qas[i].id == ID_H_ACT_MC)
            MakeButtonAccent(h, qas[i].id, qas[i].txt, cx, cy, bw, bh);
        else
            MakeButton(h, qas[i].id, qas[i].txt, cx, cy, bw, bh);
    }
    return h;
}

} // namespace slui