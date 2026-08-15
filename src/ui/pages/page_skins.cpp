#include "pages_priv.h"
#include "../../backend/account.h"
#include "../../core/paths.h"
#include <windowsx.h>
#include <commctrl.h>
#include <cstdio>

namespace slui {

enum {
    ID_S_UNLOCK = 2401, ID_S_UPLOAD = 2402, ID_S_STATUS = 2403,
};

struct SkinsData {
    HWND status = 0;
    std::vector<sl::SkinInfo> lib;
};

static SkinsData* S(Page* p) { return (SkinsData*)p->data; }

static bool on_cmd(Page* p, int id, HWND src) {
    SkinsData* d = S(p);
    if (id == ID_S_UPLOAD) {
        sl::User* u = sl::account::current();
        if (!u) { sl_msg(p->hwnd, "Ошибка", "Требуется вход", MB_ICONWARNING); return true; }
        OPENFILENAMEA ofn = {0};
        char file[MAX_PATH] = {0};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = p->hwnd;
        ofn.lpstrFilter = "Images (*.png *.jpg)\0*.png;*.jpg\0\0";
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST;
        if (!GetOpenFileNameA(&ofn)) return true;
        std::string out_id, msg;
        if (sl::skins::upload_custom(file, *u, out_id, msg)) {
            sl_msg(p->hwnd, "Успех", "Скин загружен! ID: " + out_id, MB_ICONINFORMATION);
            SetCtrl(d->status, "Скин загружен!");
        } else {
            sl_msg(p->hwnd, "Ошибка", msg, MB_ICONWARNING);
        }
        return true;
    }
    // разблокировка скина: id кнопки = индекс в библиотеке + база
    if (id >= ID_S_UNLOCK && id < ID_S_UNLOCK + 100) {
        int idx = id - ID_S_UNLOCK;
        if (idx < 0 || (size_t)idx >= d->lib.size()) return true;
        sl::User* u = sl::account::current();
        if (!u) { sl_msg(p->hwnd, "Ошибка", "Требуется вход", MB_ICONWARNING); return true; }
        std::string msg;
        if (sl::skins::unlock(d->lib[idx].id, *u, msg)) {
            sl_msg(p->hwnd, "Успех", msg, MB_ICONINFORMATION);
            SetCtrl(d->status, msg);
        } else {
            sl_msg(p->hwnd, "Ошибка", msg, MB_ICONWARNING);
        }
        return true;
    }
    return false;
}

HWND create_skins_page(HINSTANCE hi, HWND parent, LauncherApp* app) {
    Page* p = new Page;
    HWND h = create_page_common(hi, parent, app, p);
    SkinsData* d = new SkinsData;
    p->data = d;
    p->on_cmd = on_cmd;
    d->lib = sl::skins::library();

    int x = 40, y = 30, cw = 680;
    MakeLabel(h, 0, "Скины", x, y, cw, 30);
    y += 44;

    MakeButton(h, ID_S_UPLOAD, "Загрузить свой скин", x, y, 220, 36);
    y += 52;

    int card_w = 150, card_h = 150, gx = 16, gy = 16;
    for (size_t i = 0; i < d->lib.size(); i++) {
        int cx = x + (i % 4) * (card_w + gx);
        int cy = y + (i / 4) * (card_h + gy);
        const sl::SkinInfo& s = d->lib[i];
        MakeLabel(h, 0, s.icon, cx + 55, cy + 8, 40, 40);
        MakeLabel(h, 0, s.name, cx, cy + 52, card_w, 22);
        sl::User* u = sl::account::current();
        bool unlocked = sl::skins::is_unlocked(s.id, u ? *u : sl::User());
        std::string btn = unlocked ? "Применить" : (std::to_string(s.price) + " XP");
        MakeButton(h, (int)(ID_S_UNLOCK + i), btn, cx + 15, cy + 84, card_w - 30, 34);
    }
    y += 2 * (card_h + gy);
    d->status = MakeLabel(h, ID_S_STATUS, "Выберите скин для разблокировки", x, y, cw, 24);
    return h;
}

} // namespace slui