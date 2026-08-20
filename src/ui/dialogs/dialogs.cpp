#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include "../pages/pages_priv.h"
#include "../../core/win.h"
#include "../../core/common.h"
#include "../../core/paths.h"
#include "../../core/config.h"
#include "../../backend/account.h"
#include "../../backend/servers.h"
#include "../../minecraft/version.h"
#include <commctrl.h>
#include <windowsx.h>
#include <string>

namespace slui {

// Общая инфраструктура модальных диалогов: отдельные top-level окна.

// ---------------- входа/регистрации ----------------
struct LoginDialogState {
    HWND login_user = 0, login_pass = 0;
    HWND reg_user = 0, reg_email = 0, reg_pass = 0, reg_pass2 = 0;
    HWND tabs = 0;
};

static LRESULT CALLBACK LoginProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    LoginDialogState* s = (LoginDialogState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
        case WM_CLOSE | WM_DESTROY:
            DestroyWindow(h);
            return 0;
        case WM_COMMAND: {
            int id = LOWORD(w);
            if (id == 1 && s->login_user && s->login_pass) {
                std::string u = GetCtrl(s->login_user);
                std::string pw = GetCtrl(s->login_pass);
                std::string err;
                sl::User out;
                if (!sl::account::login_user(u, pw, out, err)) {
                    sl_msg(h, "Ошибка", err, MB_ICONERROR);
                    return 0;
                }
                EndDialog(h, IDOK);
                return 0;
            }
            if (id == 2) {
                std::string u = GetCtrl(s->reg_user);
                std::string e = GetCtrl(s->reg_email);
                std::string pw = GetCtrl(s->reg_pass);
                std::string pw2 = GetCtrl(s->reg_pass2);
                if (pw != pw2) { sl_msg(h, "Ошибка", "Пароли не совпадают", MB_ICONWARNING); return 0; }
                std::string err;
                sl::User out;
                if (!sl::account::register_user(u, e, pw, out, err)) {
                    sl_msg(h, "Ошибка", err, MB_ICONERROR);
                    return 0;
                }
                EndDialog(h, IDOK);
                return 0;
            }
            if (id == 3) { EndDialog(h, IDCANCEL); return 0; }
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)w;
            SetBkColor(dc, RGB(0x1B, 0x1B, 0x21));
            SetTextColor(dc, RGB(0xF2, 0xF2, 0xF4));
            return (LRESULT)ui_bg_brush();
        }
        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)w;
            SetBkColor(dc, RGB(0x14, 0x14, 0x18));
            SetTextColor(dc, RGB(0xF2, 0xF2, 0xF4));
            return (LRESULT)ui_edit_brush();
        }
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORBTN:
            return (LRESULT)ui_bg_brush();
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            FillRect(dc, &rc, ui_bg_brush());
            EndPaint(h, &ps);
            return 0;
        }
    }
    return DefWindowProcW(h, m, w, l);
}

void sl_show_login_dialog(HWND parent) {
    HINSTANCE hi = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = LoginProc;
        wc.hInstance = hi;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"SLLoginDlg";
        RegisterClassW(&wc);
        reg = true;
    }
    LoginDialogState st;
    HWND dlg = CreateWindowExW(0, L"SLLoginDlg", L"Вход / Регистрация",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME,
                               CW_USEDEFAULT, CW_USEDEFAULT, 460, 540, parent, nullptr, hi, nullptr);
    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&st);

    int x = 24, y = 24, cw = 410;
    MakeLabel(dlg, 0, "SuperLauncher Аккаунт", x, y, cw, 26);
    y += 40;

    MakeLabel(dlg, 0, "Вход", x, y, 180, 22);
    y += 30;
    MakeLabel(dlg, 0, "Логин (имя или email):", x, y, 200, 22);
    st.login_user = MakeEdit(dlg, 1, "", x + 200, y - 3, 200, 26);
    y += 34;
    MakeLabel(dlg, 0, "Пароль:", x, y, 200, 22);
    st.login_pass = MakeEdit(dlg, 1, "", x + 200, y - 3, 200, 26);
    SendMessageW(st.login_pass, EM_SETPASSWORDCHAR, '*', 0);
    y += 44;
    MakeButton(dlg, 1, "Войти", x, y, 160, 36);
    y += 56;

    MakeLabel(dlg, 0, "Регистрация", x, y, 180, 22);
    y += 30;
    MakeLabel(dlg, 0, "Имя:", x, y, 200, 22);
    st.reg_user = MakeEdit(dlg, 2, "", x + 200, y - 3, 200, 26);
    y += 34;
    MakeLabel(dlg, 0, "Email:", x, y, 200, 22);
    st.reg_email = MakeEdit(dlg, 2, "", x + 200, y - 3, 200, 26);
    y += 34;
    MakeLabel(dlg, 0, "Пароль:", x, y, 200, 22);
    st.reg_pass = MakeEdit(dlg, 2, "", x + 200, y - 3, 200, 26);
    SendMessageW(st.reg_pass, EM_SETPASSWORDCHAR, '*', 0);
    y += 34;
    MakeLabel(dlg, 0, "Повторите пароль:", x, y, 200, 22);
    st.reg_pass2 = MakeEdit(dlg, 2, "", x + 200, y - 3, 200, 26);
    SendMessageW(st.reg_pass2, EM_SETPASSWORDCHAR, '*', 0);
    y += 44;
    MakeButton(dlg, 2, "Зарегистрироваться", x, y, 200, 36);
    y += 44;
    MakeButton(dlg, 3, "Отмена", x, y, 120, 32);

    EnableWindow(parent, FALSE);
    ShowWindow(dlg, SW_SHOW);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.hwnd == dlg && msg.message == WM_COMMAND &&
            (LOWORD(msg.wParam) == 1 || LOWORD(msg.wParam) == 2 || LOWORD(msg.wParam) == 3)) {
            LoginProc(dlg, WM_COMMAND, msg.wParam, msg.lParam);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(parent, TRUE);
    DestroyWindow(dlg);
}

// ---------------- создание сервера ----------------
struct CreateServerState {
    HWND name_edit = 0, port_edit = 0, version_combo = 0, core_combo = 0;
    HWND ram_track = 0, ram_lbl = 0;
};

static LRESULT CALLBACK CreateServerProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    CreateServerState* s = (CreateServerState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
        case WM_CLOSE: EndDialog(h, IDCANCEL); return 0;
        case WM_COMMAND: {
            int id = LOWORD(w);
            if (id == 1) {
                std::string name = GetCtrl(s->name_edit);
                std::string port = GetCtrl(s->port_edit);
                if (name.empty() || port.empty()) {
                    sl_msg(h, "Ошибка", "Введите корректное имя и порт", MB_ICONWARNING);
                    return 0;
                }
                bool digits = !port.empty();
                for (char c : port) if (!isdigit((unsigned char)c)) { digits = false; break; }
                if (!digits) {
                    sl_msg(h, "Ошибка", "Введите корректное имя и порт", MB_ICONWARNING);
                    return 0;
                }
                int ram = (int)SendMessageW(s->ram_track, TBM_GETPOS, 0, 0);
                std::string ver = sl_combo_sel(s->version_combo);
                std::string core = sl_combo_sel(s->core_combo);
                // создать каталог
                std::string dir = sl::servers::create_server_dir(name);
                // добавить в список
                auto list = sl::servers::load_list();
                sl::ServerInfo si;
                si.name = name;
                si.ip = "localhost:" + port;
                si.managed = true;
                si.ram_gb = ram;
                si.version = ver;
                si.core = core;
                si.dir_path = dir;
                list.push_back(si);
                sl::servers::save_list(list);
                EndDialog(h, IDOK);
                return 0;
            }
            if (id == 2) { EndDialog(h, IDCANCEL); return 0; }
            return 0;
        }
        case WM_HSCROLL: {
            // слайдер RAM
            if (s->ram_lbl) {
                int ram = (int)SendMessageW(s->ram_track, TBM_GETPOS, 0, 0);
                SetCtrl(s->ram_lbl, std::to_string(ram) + " GB");
            }
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)w;
            SetBkColor(dc, RGB(0x1B, 0x1B, 0x21));
            SetTextColor(dc, RGB(0xF2, 0xF2, 0xF4));
            return (LRESULT)ui_bg_brush();
        }
        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)w;
            SetBkColor(dc, RGB(0x14, 0x14, 0x18));
            SetTextColor(dc, RGB(0xF2, 0xF2, 0xF4));
            return (LRESULT)ui_edit_brush();
        }
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORBTN:
            return (LRESULT)ui_bg_brush();
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc; GetClientRect(h, &rc);
            FillRect(dc, &rc, ui_bg_brush());
            EndPaint(h, &ps);
            return 0;
        }
    }
    return DefWindowProcW(h, m, w, l);
}

// Версии из манифеста для комбо в диалоге.
static void fill_server_versions(HWND combo) {
    std::vector<sl::ManifestVersion> list;
    if (!sl::fetch_manifest(list)) {
        sl_fill_combo(combo, { "1.20.4", "1.20.1" });
        return;
    }
    std::vector<std::string> items;
    for (auto& v : list) if (v.type == "release") items.insert(items.begin(), v.id);
    sl_fill_combo(combo, items);
}

void sl_show_create_server_dialog(HWND parent) {
    HINSTANCE hi = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = CreateServerProc;
        wc.hInstance = hi;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"SLCreateServer";
        RegisterClassW(&wc);
        reg = true;
    }
    CreateServerState st;
    HWND dlg = CreateWindowExW(0, L"SLCreateServer", L"Создать сервер",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME,
                               CW_USEDEFAULT, CW_USEDEFAULT, 480, 460, parent, nullptr, hi, nullptr);
    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&st);

    int x = 24, y = 24, cw = 430;
    MakeLabel(dlg, 0, "Создание сервера", x, y, cw, 26);
    y += 40;
    MakeLabel(dlg, 0, "Имя:", x, y, 180, 22);
    st.name_edit = MakeEdit(dlg, 1, "", x + 180, y - 3, 240, 26);
    y += 34;
    MakeLabel(dlg, 0, "Порт:", x, y, 180, 22);
    st.port_edit = MakeEdit(dlg, 1, "25565", x + 180, y - 3, 240, 26);
    y += 34;
    MakeLabel(dlg, 0, "Версия:", x, y, 180, 22);
    st.version_combo = MakeCombo(dlg, 1, x + 180, y - 3, 240, 220);
    y += 34;
    MakeLabel(dlg, 0, "Ядро:", x, y, 180, 22);
    st.core_combo = MakeCombo(dlg, 1, x + 180, y - 3, 240, 200);
    y += 40;
    MakeLabel(dlg, 0, "RAM:", x, y, 180, 22);
    st.ram_track = CreateWindowExW(0, TRACKBAR_CLASSW, 0,
                                   WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ,
                                   x + 180, y, 160, 24, dlg, nullptr, hi, 0);
    SendMessageW(st.ram_track, TBM_SETRANGE, TRUE, MAKELPARAM(1, 16));
    SendMessageW(st.ram_track, TBM_SETPOS, TRUE, 4);
    st.ram_lbl = MakeLabel(dlg, 0, "4 GB", x + 350, y, 60, 24);
    y += 52;
    MakeButton(dlg, 1, "Создать", x, y, 160, 36);
    MakeButton(dlg, 2, "Отмена", x + 176, y, 120, 36);

    sl_fill_combo(st.core_combo, { "Paper", "Purpur", "Vanilla", "Fabric", "Quilt" });
    fill_server_versions(st.version_combo);

    EnableWindow(parent, FALSE);
    ShowWindow(dlg, SW_SHOW);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.hwnd == dlg && msg.message == WM_COMMAND &&
            (LOWORD(msg.wParam) == 1 || LOWORD(msg.wParam) == 2)) {
            CreateServerProc(dlg, WM_COMMAND, msg.wParam, msg.lParam);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(parent, TRUE);
    DestroyWindow(dlg);
}

} // namespace slui