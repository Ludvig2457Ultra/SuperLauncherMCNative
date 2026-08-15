#include "pages_priv.h"
#include "../../backend/account.h"
#include <windowsx.h>
#include <string>

namespace slui {

enum {
    ID_A_LOGIN = 2101, ID_A_LOGOUT = 2102, ID_A_ACTIVATE = 2103,
    ID_A_KEY = 2104, ID_A_NAME = 2105, ID_A_STATUS = 2106,
};

struct AccountData {
    HWND name_lbl = 0;
    HWND status_lbl = 0;
    HWND key_edit = 0;
    HWND login_btn = 0;
    HWND logout_btn = 0;
};

static AccountData* A(Page* p) { return (AccountData*)p->data; }

// Диалог входа/регистрации (объявлен в dialogs.cpp).
void sl_show_login_dialog(HWND parent);

void on_show_page_account(Page* p);

static bool on_cmd(Page* p, int id, HWND src) {
    AccountData* a = A(p);
    switch (id) {
        case ID_A_LOGIN: {
            // открыть встроенный диалог входа/регистрации
            extern void sl_show_login_dialog(HWND parent);
            sl_show_login_dialog(p->hwnd);
            on_show_page_account(p);
            return true;
        }
        case ID_A_LOGOUT: {
            sl::account::logout();
            on_show_page_account(p);
            return true;
        }
        case ID_A_ACTIVATE: {
            std::string key = GetCtrl(a->key_edit);
            if (key.empty()) { sl_msg(p->hwnd, "Ошибка", "Введите ключ", MB_ICONWARNING); return true; }
            sl::User* u = sl::account::current();
            if (!u) { sl_msg(p->hwnd, "Ошибка", "Требуется вход", MB_ICONWARNING); return true; }
            std::string msg;
            if (sl::account::activate_license(key, u->user_id, msg)) {
                sl_msg(p->hwnd, "Успех", msg, MB_ICONINFORMATION);
                on_show_page_account(p);
            } else {
                sl_msg(p->hwnd, "Ошибка", msg, MB_ICONERROR);
            }
            return true;
        }
    }
    return false;
}

void on_show_page_account(Page* p) {
    AccountData* a = A(p);
    sl::User* u = sl::account::current();
    if (u) {
        std::string tier = u->license_tier;
        for (auto& c : tier) c = (char)toupper((unsigned char)c);
        SetCtrl(a->name_lbl, "Имя: " + u->username);
        SetCtrl(a->status_lbl, "Статус: " + tier + "   Уровень " + std::to_string(u->level) +
                               "   XP " + std::to_string(u->xp));
        ShowWindow(a->logout_btn, SW_SHOW);
        SetCtrl(a->login_btn, "Сменить аккаунт");
    } else {
        SetCtrl(a->name_lbl, "Имя: Гость");
        SetCtrl(a->status_lbl, "Статус: Бесплатная версия");
        ShowWindow(a->logout_btn, SW_HIDE);
        SetCtrl(a->login_btn, "Войти / Зарегистрироваться");
    }
}

static void on_show(Page* p) { on_show_page_account(p); }

HWND create_account_page(HINSTANCE hi, HWND parent, LauncherApp* app) {
    Page* p = new Page;
    HWND h = create_page_common(hi, parent, app, p);
    AccountData* d = new AccountData;
    p->data = d;
    p->on_cmd = on_cmd;
    p->on_show = on_show;

    int x = 40, y = 30, cw = 520;
    MakeLabel(h, 0, "👤 Мой Аккаунт", x, y, cw, 30);
    y += 48;

    MakeLabel(h, 0, "Информация", x, y, cw, 22);
    y += 30;
    d->name_lbl = MakeLabel(h, ID_A_NAME, "Имя: Гость", x + 8, y, 300, 24);
    y += 30;
    d->status_lbl = MakeLabel(h, ID_A_STATUS, "Статус: Бесплатная версия", x + 8, y, 400, 24);
    y += 52;

    MakeLabel(h, 0, "Лицензия", x, y, cw, 22);
    y += 30;
    d->key_edit = MakeEdit(h, ID_A_KEY, "", x + 8, y - 3, 300, 26);
    MakeButton(h, ID_A_ACTIVATE, "Активировать", x + 320, y - 3, 160, 30);
    y += 52;

    d->login_btn = MakeButtonAccent(h, ID_A_LOGIN, "Войти / Зарегистрироваться", x, y, 240, 40);
    y += 52;
    d->logout_btn = MakeButton(h, ID_A_LOGOUT, "Выйти", x, y, 120, 34);
    ShowWindow(d->logout_btn, SW_HIDE);
    return h;
}

} // namespace slui