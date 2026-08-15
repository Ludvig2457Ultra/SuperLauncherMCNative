#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif
#include "pages.h"
#include "../ui.h"
#include "../../core/win.h"
#include "../../core/common.h"
#include "../../core/config.h"
#include "../../core/log.h"
#include "../../core/paths.h"
#include "../../minecraft/version.h"
#include "../../minecraft/install.h"
#include "../../minecraft/command.h"
#include "../../backend/mods.h"
#include "../../backend/updates.h"
#include "../../backend/servers.h"
#include <commctrl.h>
#include <windowsx.h>
#include <vector>
#include <map>

namespace slui {

// ---------------- реестр страниц ----------------
static std::vector<Page*> g_pages;

Page* page_of(HWND h) {
    for (auto* p : g_pages)
        if (p->hwnd == h) return p;
    return nullptr;
}

static Page* page_of_control(HWND src) {
    HWND parent = GetParent(src);
    if (!parent) return nullptr;
    return page_of(parent);
}

// ---------------- общая страница (окно) ----------------
static LRESULT CALLBACK PageProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    Page* p = (Page*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)w;
            SetBkColor(dc, page_bg_color());
            HWND c = (HWND)l;
            if (GetPropW(c, L"SLTitle")) SetTextColor(dc, RGB(0xFF, 0xFF, 0xFF));
            else if (GetPropW(c, L"SLSub")) SetTextColor(dc, RGB(0x9A, 0x9A, 0xA4));
            else SetTextColor(dc, RGB(0xF2, 0xF2, 0xF4));
            return (LRESULT)ui_bg_brush();
        }
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORBTN:
            return (LRESULT)ui_bg_brush();
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC dc = (HDC)w;
            SetBkColor(dc, RGB(0x14, 0x14, 0x18));
            SetTextColor(dc, RGB(0xF2, 0xF2, 0xF4));
            return (LRESULT)ui_edit_brush();
        }
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            // контентная панель: плоский однотонный фон
            ui_fill_gradient_v(dc, &rc, page_bg_top(), page_bg_bottom());
            EndPaint(h, &ps);
            return 0;
        }
        case WM_COMMAND:
            SendMessageW(GetParent(h), WM_COMMAND, w, l);
            return 0;
        case WM_VSCROLL:
        case WM_HSCROLL:
            SendMessageW(GetParent(h), WM_COMMAND, w, l);
            return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

HWND create_page_common(HINSTANCE hi, HWND parent, LauncherApp* app, Page* p) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc = PageProc;
        wc.hInstance = hi;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"SLPage";
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }
    p->app = app;
    p->hwnd = CreateWindowExW(0, L"SLPage", nullptr, WS_CHILD, 0, 0, 900, 700,
                              parent, nullptr, hi, nullptr);
    SetWindowLongPtrW(p->hwnd, GWLP_USERDATA, (LONG_PTR)p);
    g_pages.push_back(p);
    return p->hwnd;
}

// ---------------- событийная шина ----------------
void post_event(HWND main, PageEvent* ev) {
    if (!main) { delete ev; return; }
    if (!PostMessageW(main, WM_SL_EVENT, 0, (LPARAM)ev)) delete ev;
}

void pages_on_event(LauncherApp* app, PageEvent* ev) {
    if (!ev) return;
    Page* p = nullptr;
    if (ev->page) p = page_of(ev->page);
    if (p && p->on_event) p->on_event(p, ev);
    delete ev;
}

// ---------------- диспетчер WM_COMMAND ----------------
void pages_on_command(LauncherApp* app, int id, HWND src) {
    Page* p = page_of_control(src);
    if (!p) return;
    if (p->on_cmd && p->on_cmd(p, id, src)) return;
}

void pages_on_show(LauncherApp* app, int idx) {
    if (idx < 0 || idx >= (int)g_pages.size()) return;
    Page* p = g_pages[idx];
    if (p && p->on_show) p->on_show(p);
}

// ---------------- статусы (идут на Minecraft-страницу) ----------------
// Minecraft-страница регистрирует себя здесь для приёма статусов запуска.
static Page* g_launch_page = nullptr;
void sl_register_launch_page(Page* p) { g_launch_page = p; }

void pages_on_status(LauncherApp* app, const std::string& s) {
    Page* p = g_launch_page;
    if (!p || !p->data) return;
    // Проброс: LaunchPageData имеет memo; on_event PE_STATUS
    PageEvent ev;
    ev.kind = PE_STATUS;
    ev.a = s;
    if (p->on_event) p->on_event(p, &ev);
}
void pages_on_progress(LauncherApp* app, long long done, long long total) {
    Page* p = g_launch_page;
    if (!p || !p->data) return;
    PageEvent ev;
    ev.kind = PE_PROGRESS;
    ev.n = total > 0 ? (long long)(done * 100 / total) : 0;
    if (p->on_event) p->on_event(p, &ev);
}
void pages_on_done(LauncherApp* app, int code) {
    Page* p = g_launch_page;
    if (!p || !p->data) return;
    PageEvent ev;
    ev.kind = PE_DONE;
    ev.n = code;
    if (p->on_event) p->on_event(p, &ev);
}
void pages_on_versions(LauncherApp* app, std::vector<sl::ManifestVersion>* list) {
    Page* p = g_launch_page;
    if (!p || !p->data) return;
    PageEvent ev;
    ev.kind = PE_VERSIONS;
    ev.data = list;
    if (p->on_event) p->on_event(p, &ev);
}

// ---------------- формат числа ----------------
std::string fmt_number(long long n) {
    if (n >= 1000000) {
        char b[32];
        double m = (double)n / 1000000.0;
        sprintf_s(b, "%.1fM", m);
        return b;
    }
    if (n >= 1000) {
        char b[32];
        sprintf_s(b, "%.0fK", (double)n / 1000.0);
        return b;
    }
    return std::to_string(n);
}

} // namespace slui