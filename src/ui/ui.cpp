#include "ui.h"
#include "pages/pages.h"
#include "../core/win.h"
#include "../core/common.h"
#include "../core/paths.h"
#include "../core/config.h"
#include "../core/log.h"
#include "../minecraft/version.h"
#include "../minecraft/install.h"
#include "../minecraft/command.h"
#include "../instances/instances.h"
#include <commctrl.h>
#include <cstdio>
#include <vector>
#include <memory>

#pragma comment(lib, "comctl32.lib")

namespace slui {

// ===== Оригинальная тема SuperLauncher 2.0 (PyQt -> Win32 GDI) =====
// bg_primary #1a1a2e, bg_secondary #16213e, accent #4facfe,
// accent_gradient (#667eea, #764ba2), sidebar (14,14,22)->(10,10,18),
// контент rgba(30,30,40,0.85), границы #313244.

static HBRUSH g_bg, g_edit, g_side;
static HFONT g_font, g_font_big, g_font_title, g_font_header;
static HCURSOR g_cursor = nullptr;

COLORREF page_bg_color() { return RGB(0x1B, 0x1B, 0x21); }   // контент (плоский)
COLORREF page_bg_top()    { return RGB(0x1B, 0x1B, 0x21); }
COLORREF page_bg_bottom() { return RGB(0x1B, 0x1B, 0x21); }
static COLORREF t_side_top()    { return RGB(0x16, 0x16, 0x1B); } // сайдбар (плоский)
static COLORREF t_side_bottom() { return RGB(0x16, 0x16, 0x1B); }
static COLORREF t_text()  { return RGB(0xF2, 0xF2, 0xF4); }       // основной текст
static COLORREF t_edit()  { return RGB(0x14, 0x14, 0x18); }
static COLORREF t_accent()       { return RGB(0x4F, 0xAC, 0xFE); } // единственный акцент
static COLORREF t_accent_deep()  { return RGB(0x3D, 0x8F, 0xD4); } // pressed
static COLORREF t_accent_hot()   { return RGB(0x6D, 0xB8, 0xFF); } // hover
static COLORREF t_grad1()        { return RGB(0x66, 0x7E, 0xEA); }
static COLORREF t_grad2()        { return RGB(0x76, 0x4B, 0xA2); }
static COLORREF t_muted() { return RGB(0x9A, 0x9A, 0xA4); }       // приглушённый
static COLORREF t_faint() { return RGB(0x6E, 0x6E, 0x78); }       // едва заметный
static COLORREF t_border() { return RGB(0x2A, 0x2A, 0x31); }      // разделители

void ui_fill_gradient_v(HDC dc, RECT* r, COLORREF top, COLORREF bottom, int bands) {
    int h = r->bottom - r->top;
    if (h <= 0 || bands <= 0) return;
    if (bands > h) bands = h;
    int c1r = GetRValue(top), c1g = GetGValue(top), c1b = GetBValue(top);
    int c2r = GetRValue(bottom), c2g = GetGValue(bottom), c2b = GetBValue(bottom);
    for (int i = 0; i < bands; i++) {
        float t = bands == 1 ? 0.f : (float)i / (float)(bands - 1);
        RECT row = *r;
        row.top = r->top + (int)((long long)i * h / bands);
        row.bottom = r->top + (int)((long long)(i + 1) * h / bands);
        if (row.bottom <= row.top) continue;
        HBRUSH br = CreateSolidBrush(RGB((int)(c1r + (c2r - c1r) * t),
                                         (int)(c1g + (c2g - c1g) * t),
                                         (int)(c1b + (c2b - c1b) * t)));
        FillRect(dc, &row, br);
        DeleteObject(br);
    }
}

void ui_fill_gradient_h(HDC dc, RECT* r, COLORREF left, COLORREF right, int bands) {
    int w = r->right - r->left;
    if (w <= 0 || bands <= 0) return;
    if (bands > w) bands = w;
    int c1r = GetRValue(left), c1g = GetGValue(left), c1b = GetBValue(left);
    int c2r = GetRValue(right), c2g = GetGValue(right), c2b = GetBValue(right);
    for (int i = 0; i < bands; i++) {
        float t = bands == 1 ? 0.f : (float)i / (float)(bands - 1);
        RECT col = *r;
        col.left = r->left + (int)((long long)i * w / bands);
        col.right = r->left + (int)((long long)(i + 1) * w / bands);
        if (col.right <= col.left) continue;
        HBRUSH br = CreateSolidBrush(RGB((int)(c1r + (c2r - c1r) * t),
                                         (int)(c1g + (c2g - c1g) * t),
                                         (int)(c1b + (c2b - c1b) * t)));
        FillRect(dc, &col, br);
        DeleteObject(br);
    }
}

static void theme_init() {
    if (g_bg) return;
    g_bg = CreateSolidBrush(page_bg_color());
    g_edit = CreateSolidBrush(t_edit());
    g_side = CreateSolidBrush(t_side_top());
    g_font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_font_big = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_font_title = CreateFontW(-22, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_font_header = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    g_cursor = LoadCursor(nullptr, IDC_HAND);
}

static HINSTANCE HInst(HWND w) { return (HINSTANCE)GetWindowLongPtrW(w, GWLP_HINSTANCE); }

HWND MakeLabel(HWND p, int id, const std::string& s, int x, int y, int w, int h) {
    HWND c = CreateWindowExW(0, L"STATIC", sl::s2ws(s).c_str(), WS_CHILD | WS_VISIBLE, x, y, w, h, p, (HMENU)(INT_PTR)id, HInst(p), 0);
    if (g_font) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}
// Заголовок страницы: жирный голубой текст (как "SuperLauncher 2026" в оригинале).
HWND MakeTitle(HWND p, int id, const std::string& s, int x, int y, int w, int h) {
    HWND c = CreateWindowExW(0, L"STATIC", sl::s2ws(s).c_str(), WS_CHILD | WS_VISIBLE, x, y, w, h, p, (HMENU)(INT_PTR)id, HInst(p), 0);
    if (g_font_title) SendMessageW(c, WM_SETFONT, (WPARAM)g_font_title, TRUE);
    SetPropW(c, L"SLTitle", (HANDLE)1);
    return c;
}
// Подзаголовок секции: полужирный приглушённый текст.
HWND MakeSub(HWND p, int id, const std::string& s, int x, int y, int w, int h) {
    HWND c = CreateWindowExW(0, L"STATIC", sl::s2ws(s).c_str(), WS_CHILD | WS_VISIBLE, x, y, w, h, p, (HMENU)(INT_PTR)id, HInst(p), 0);
    if (g_font_header) SendMessageW(c, WM_SETFONT, (WPARAM)g_font_header, TRUE);
    SetPropW(c, L"SLSub", (HANDLE)1);
    return c;
}
HWND MakeEdit(HWND p, int id, const std::string& s, int x, int y, int w, int h) {
    HWND c = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", sl::s2ws(s).c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, x, y, w, h, p, (HMENU)(INT_PTR)id, HInst(p), 0);
    if (g_font) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}
HWND MakeCombo(HWND p, int id, int x, int y, int w, int h) {
    HWND c = CreateWindowExW(0, L"COMBOBOX", 0, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, x, y, w, h, p, (HMENU)(INT_PTR)id, HInst(p), 0);
    if (g_font) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}
HWND MakeProgress(HWND p, int id, int x, int y, int w, int h) {
    HWND c = CreateWindowExW(0, PROGRESS_CLASSW, 0, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, x, y, w, h, p, (HMENU)(INT_PTR)id, HInst(p), 0);
    SendMessageW(c, PBM_SETBKCOLOR, 0, (LPARAM)RGB(0x16, 0x16, 0x20));
    SendMessageW(c, PBM_SETBARCOLOR, 0, (LPARAM)t_accent());
    return c;
}
HWND MakeMemo(HWND p, int id, int x, int y, int w, int h) {
    HWND c = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", 0, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, x, y, w, h, p, (HMENU)(INT_PTR)id, HInst(p), 0);
    if (g_font) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}
HWND MakeList(HWND p, int id, int x, int y, int w, int h) {
    HWND c = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", 0,
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                             x, y, w, h, p, (HMENU)(INT_PTR)id, HInst(p), 0);
    if (g_font) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}

HBRUSH ui_bg_brush() { theme_init(); return g_bg; }
HBRUSH ui_edit_brush() { theme_init(); return g_edit; }

// ---------------- отрисовка скруглённых прямоугольников ----------------
void FillRound(HDC dc, const RECT& rc, int radius, COLORREF color) {
    HBRUSH br = CreateSolidBrush(color);
    HBRUSH ob = (HBRUSH)SelectObject(dc, br);
    HPEN op = (HPEN)SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(br);
}
void StrokeRound(HDC dc, const RECT& rc, int radius, COLORREF color) {
    HPEN pn = CreatePen(PS_SOLID, 1, color);
    HPEN op = (HPEN)SelectObject(dc, pn);
    HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(pn);
}
static void FillRoundGrad(HDC dc, const RECT& rc, int radius, COLORREF top, COLORREF bottom, int bands = 6) {
    int h = rc.bottom - rc.top;
    if (h <= 0 || bands <= 0) return;
    int c1r = GetRValue(top), c1g = GetGValue(top), c1b = GetBValue(top);
    int c2r = GetRValue(bottom), c2g = GetGValue(bottom), c2b = GetBValue(bottom);
    for (int i = 0; i < bands; i++) {
        float t = bands == 1 ? 0.f : (float)i / (float)(bands - 1);
        RECT row = rc;
        row.top = rc.top + (int)((long long)i * h / bands);
        row.bottom = rc.top + (int)((long long)(i + 1) * h / bands);
        if (row.bottom <= row.top) continue;
        FillRound(dc, row, radius, RGB((int)(c1r + (c2r - c1r) * t),
                                       (int)(c1g + (c2g - c1g) * t),
                                       (int)(c1b + (c2b - c1b) * t)));
    }
}
// Цвет фона сайдбара в точке кнопки (вертикальный градиент t_side_top->t_side_bottom).
static COLORREF side_bg_at(HWND h) {
    HWND parent = GetParent(h);
    if (!parent) return t_side_top();
    RECT pr;
    GetClientRect(parent, &pr);
    RECT rc;
    GetClientRect(h, &rc);
    POINT pt = { 0, rc.bottom / 2 };
    ClientToScreen(h, &pt);
    ScreenToClient(parent, &pt);
    int hh = pr.bottom - pr.top;
    float t = hh > 0 ? (float)pt.y / (float)hh : 0.f;
    if (t < 0) t = 0; if (t > 1) t = 1;
    return RGB((int)(GetRValue(t_side_top()) + (GetRValue(t_side_bottom()) - GetRValue(t_side_top())) * t),
               (int)(GetGValue(t_side_top()) + (GetGValue(t_side_bottom()) - GetGValue(t_side_top())) * t),
               (int)(GetBValue(t_side_top()) + (GetBValue(t_side_bottom()) - GetBValue(t_side_top())) * t));
}

// ---------------- SLBtn: кнопка в стиле темы ----------------
// GWLP_USERDATA: bit0 = hover, bit1 = accent (primary)
static LRESULT CALLBACK BtnProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            LONG_PTR st = GetWindowLongPtrW(h, GWLP_USERDATA);
            bool hot = (st & 1) != 0;
            bool accent = (st & 2) != 0;
            bool push = hot && (GetKeyState(VK_LBUTTON) < 0);

            COLORREF fill, border;
            if (accent) {
                // primary: сплошной акцент (единственный цвет акцента в UI)
                fill   = push ? t_accent_deep() : hot ? t_accent_hot() : t_accent();
                border = fill;
            } else {
                // secondary: плоский нейтральный
                fill   = push ? RGB(0x20, 0x20, 0x26) : hot ? RGB(0x2E, 0x2E, 0x38) : RGB(0x26, 0x26, 0x2E);
                border = fill;
            }
            FillRound(dc, rc, 8, fill);
            StrokeRound(dc, rc, 8, border);

            SetBkMode(dc, TRANSPARENT);
            HFONT old = (HFONT)SelectObject(dc, g_font);
            SetTextColor(dc, RGB(0xFF, 0xFF, 0xFF));
            wchar_t txt[128];
            GetWindowTextW(h, txt, 128);
            RECT tr = rc;
            tr.left += 6;
            tr.right -= 6;
            DrawTextW(dc, txt, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, old);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEMOVE: {
            LONG_PTR st = GetWindowLongPtrW(h, GWLP_USERDATA);
            if (!(st & 1)) { SetWindowLongPtrW(h, GWLP_USERDATA, st | 1); InvalidateRect(h, 0, FALSE); }
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }
        case WM_MOUSELEAVE: {
            SetWindowLongPtrW(h, GWLP_USERDATA, GetWindowLongPtrW(h, GWLP_USERDATA) & ~1);
            InvalidateRect(h, 0, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            RECT rc;
            GetClientRect(h, &rc);
            POINT pt = { (short)LOWORD(l), (short)HIWORD(l) };
            if (PtInRect(&rc, pt))
                SendMessageW(GetParent(h), WM_COMMAND,
                             MAKEWPARAM((UINT)GetWindowLongPtrW(h, GWLP_ID), BN_CLICKED), (LPARAM)h);
            return 0;
        }
        case WM_SETCURSOR: {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
        case WM_GETDLGCODE: return DLGC_BUTTON | DLGC_WANTARROWS;
    }
    return DefWindowProcW(h, m, w, l);
}

static void reg_btn(HINSTANCE hi) {
    static bool done = false;
    if (done) return;
    done = true;
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = BtnProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SLBtn";
    RegisterClassW(&wc);
}

static HWND make_btn_impl(HWND p, int id, const std::string& s, int x, int y, int w, int h, int flags) {
    HWND c = CreateWindowExW(0, L"SLBtn", sl::s2ws(s).c_str(), WS_CHILD | WS_VISIBLE,
                             x, y, w, h, p, (HMENU)(INT_PTR)id,
                             (HINSTANCE)GetWindowLongPtrW(p, GWLP_HINSTANCE), 0);
    if (g_font) SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    if (flags) SetWindowLongPtrW(c, GWLP_USERDATA, flags);
    return c;
}

HWND MakeButton(HWND p, int id, const std::string& s, int x, int y, int w, int h) {
    return make_btn_impl(p, id, s, x, y, w, h, 0);
}
HWND MakeButtonAccent(HWND p, int id, const std::string& s, int x, int y, int w, int h) {
    return make_btn_impl(p, id, s, x, y, w, h, 2);
}

void SetCtrl(HWND c, const std::string& s) { SetWindowTextW(c, sl::s2ws(s).c_str()); }
std::string GetCtrl(HWND c) {
    int n = GetWindowTextLengthW(c);
    std::wstring w((size_t)n, 0);
    GetWindowTextW(c, &w[0], n + 1);
    return sl::w2a(w);
}

void Win::set_text(const std::string& s) { SetWindowTextW(h, sl::s2ws(s).c_str()); }
std::string Win::get_text() const {
    if (!h) return std::string();
    int n = GetWindowTextLengthW(h);
    std::wstring w((size_t)n, 0);
    GetWindowTextW(h, &w[0], n + 1);
    return sl::w2a(w);
}
void Win::show(bool on) { ShowWindow(h, on ? SW_SHOW : SW_HIDE); }
void Win::move(int x, int y, int w, int h) { MoveWindow(this->h, x, y, w, h, TRUE); }

// ================================================================
//  Запуск Minecraft в отдельном потоке (не блокирует UI)
// ================================================================
namespace {

struct LaunchCtx {
    HWND hwnd;
    std::string version;
    std::string mc_dir;
    sl::LaunchOptions opts;
    bool from_instance = false;
    std::string instance_id;
};

std::vector<sl::ManifestVersion> g_versions;   // кэш манифеста

void post_status(HWND h, const std::string& s) {
    std::string* p = new std::string(s);
    PostMessageW(h, WM_SL_STATUS, 0, (LPARAM)p);
}
void post_progress(HWND h, long long done, long long total) {
    PostMessageW(h, WM_SL_PROGRESS, (WPARAM)(int)done, (LPARAM)(int)total);
}

DWORD WINAPI LaunchWorker(LPVOID param) {
    std::unique_ptr<LaunchCtx> ctx((LaunchCtx*)param);
    HWND h = ctx->hwnd;
    std::string version = ctx->version;
    std::string mc_dir = ctx->mc_dir;
    sl::LaunchOptions opts = ctx->opts;

    sl::InstallProgress prog;
    prog.update = [h](const std::string& status, float d, float t) {
        post_status(h, status);
        post_progress(h, (long long)d, (long long)t ? (long long)t : 1);
    };

    // разрешение псевдонима
    bool alias = (version == "latest_release" || version == "latest" ||
                  version == "snapshot" || version == "latest_snapshot" ||
                  version == "release");
    if (alias) {
        std::vector<sl::ManifestVersion> list;
        if (!g_versions.empty()) list = g_versions;
        else if (sl::fetch_manifest(list)) { g_versions = list; }
        bool ok = false;
        std::string real = sl::resolve_version_alias(list, version, &ok);
        if (!ok) {
            post_status(h, "Не удалось разрешить версию " + version);
            PostMessageW(h, WM_SL_DONE, (WPARAM)-1, 0);
            return 0;
        }
        post_status(h, "Версия " + version + " -> " + real);
        version = real;
    }

    // url version.json
    std::string manifest_url;
    for (auto& v : g_versions)
        if (v.id == version) { manifest_url = v.url; break; }

    post_status(h, "Проверка установки " + version + "...");
    std::string err;
    bool inst_ok = sl::install_minecraft_version(version, mc_dir, manifest_url, &prog, &err, false);
    if (!inst_ok) {
        // повторная попытка установки с проверкой
        post_status(h, "Ошибка установки: " + (err.empty() ? "неизвестная" : err));
        PostMessageW(h, WM_SL_DONE, (WPARAM)-2, 0);
        return 0;
    }

    std::string cmd = sl::build_minecraft_command(version, mc_dir, opts, &err);
    if (cmd.empty()) {
        post_status(h, "Ошибка команды: " + (err.empty() ? "нет команды" : err));
        PostMessageW(h, WM_SL_DONE, (WPARAM)-3, 0);
        return 0;
    }
    post_status(h, "Запуск Minecraft...");
    int code = sl::launch_and_wait(cmd, mc_dir, &err);
    if (code < 0)
        post_status(h, "Не удалось запустить процесс: " + (err.empty() ? "?" : err));
    post_status(h, "Minecraft завершён (код " + std::to_string(code) + ")");
    PostMessageW(h, WM_SL_DONE, (WPARAM)code, 0);
    return 0;
}

void start_launch(LauncherApp* app, const std::string& version, const std::string& mc_dir,
                  const sl::LaunchOptions& opts) {
    if (app->working) return;
    app->working = true;
    LaunchCtx* ctx = new LaunchCtx;
    ctx->hwnd = app->hwnd;
    ctx->version = version;
    ctx->mc_dir = mc_dir;
    ctx->opts = opts;
    CreateThread(nullptr, 0, LaunchWorker, ctx, 0, nullptr);
}

} // namespace

void ui_start_launch(LauncherApp* app, const std::string& version_id,
                     const std::string& mc_dir) {
    sl::LaunchOptions opts;
    sl::Config cfg;
    cfg.load();
    opts.max_ram = cfg.max_ram;
    opts.java_path = cfg.java_path;
    opts.username = cfg.last_username.empty() ? "player" : cfg.last_username;
    if (!cfg.jvm_args.empty())
        opts.extra_jvm.push_back(cfg.jvm_args);
    start_launch(app, version_id, mc_dir, opts);
}

DWORD WINAPI ManifestWorker(LPVOID param) {
    HWND h = (HWND)param;
    std::vector<sl::ManifestVersion> list;
    if (sl::fetch_manifest(list) && !list.empty()) {
        g_versions = list;
        PostMessageW(h, WM_SL_VERSIONS, 0, 0);
    } else {
        std::string* s = new std::string("Не удалось загрузить список версий");
        PostMessageW(h, WM_SL_STATUS, 0, (LPARAM)s);
    }
    return 0;
}

// ================================================================
//  Главное окно
// ================================================================
static LauncherApp* s_app = nullptr;

void LauncherApp::show_page(int idx) {
    for (size_t i = 0; i < pages.size(); i++) {
        if (i == (size_t)idx) {
            ShowWindow(pages[i], SW_SHOW);
        } else {
            ShowWindow(pages[i], SW_HIDE);
        }
    }
    cur_page = idx;
    layout();
    pages_on_show(this, idx);
}

void sl_navigate(LauncherApp* app, int idx) {
    if (!app) return;
    app->show_page(idx);
}

void LauncherApp::layout() {
    if (!hwnd) return;
    RECT rc;
    GetClientRect(hwnd, &rc);
    const int M = 16;           // поля
    const int side = 200;       // ширина сайдбара
    MoveWindow(sidebar, M, M, side, rc.bottom - 2 * M, TRUE);
    // страницы — контентная панель правее сайдбара
    int px = M + side + M;
    int pw = rc.right - px - M;
    int ph = rc.bottom - 2 * M;
    for (size_t i = 0; i < pages.size(); i++) {
        MoveWindow(pages[i], px, M, pw, ph, TRUE);
    }
    // кнопки сайдбара: ниже подписи (40px), шаг 40
    int y = 52;
    for (size_t i = 0; i < nav.size(); i++) {
        HWND b = nav[i];
        MoveWindow(b, 10, y, side - 20, 38, TRUE);
        LONG_PTR sel = (i == (size_t)cur_page) ? 1 : 0;
        if (GetWindowLongPtrW(b, GWLP_USERDATA) != sel) {
            SetWindowLongPtrW(b, GWLP_USERDATA, sel);
            InvalidateRect(b, 0, TRUE);
        }
        y += 40;
    }
}

static LRESULT CALLBACK SideProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        // плоский фон сайдбара
        HBRUSH bg = CreateSolidBrush(t_side_top());
        FillRect(dc, &rc, bg);
        DeleteObject(bg);
        SetBkMode(dc, TRANSPARENT);

        // маленькая нейтральная подпись
        HFONT old = (HFONT)SelectObject(dc, g_font_header);
        SetTextColor(dc, t_text());
        RECT wt = { 12, 12, rc.right - 8, 40 };
        DrawTextW(dc, L"SuperLauncher", -1, &wt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, old);

        // разделитель справа
        RECT line = { rc.right - 1, 0, rc.right, rc.bottom };
        HBRUSH lb = CreateSolidBrush(t_border());
        FillRect(dc, &line, lb);
        DeleteObject(lb);

        // версия внизу
        SelectObject(dc, g_font);
        SetTextColor(dc, t_faint());
        RECT vr = { 12, rc.bottom - 34, rc.right - 8, rc.bottom - 6 };
        DrawTextW(dc, L"v2.0.0", -1, &vr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_ERASEBKGND) return 1;
    return DefWindowProcW(h, m, w, l);
}

// Кнопка сайдбара (стандартный BUTTON переопределённый в owner-draw-стиль)
// GWLP_USERDATA: bit0 = выбранная, bit1 = hover
static LRESULT CALLBACK NavBtnProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            LONG_PTR st = GetWindowLongPtrW(h, GWLP_USERDATA);
            bool sel = (st & 1) != 0;
            bool hover = (st & 2) != 0;
            if (sel) {
                // выбранная: плоская заливка светлее фона
                FillRound(dc, rc, 8, RGB(0x23, 0x23, 0x29));
            } else if (hover) {
                FillRound(dc, rc, 8, RGB(0x1D, 0x1D, 0x23));
            } else {
                // обычная: фон сайдбара под кнопкой
                FillRound(dc, rc, 8, side_bg_at(h));
            }
            SetBkMode(dc, TRANSPARENT);
            HFONT old = (HFONT)SelectObject(dc, g_font);
            SetTextColor(dc, sel ? RGB(0xFF, 0xFF, 0xFF)
                                 : hover ? RGB(0xC8, 0xC8, 0xD0)
                                         : t_muted());
            wchar_t txt[128];
            GetWindowTextW(h, txt, 128);
            RECT tr = rc;
            tr.left += 22;
            tr.right -= 8;
            DrawTextW(dc, txt, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, old);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEMOVE: {
            LONG_PTR st = GetWindowLongPtrW(h, GWLP_USERDATA);
            if (!(st & 2)) { SetWindowLongPtrW(h, GWLP_USERDATA, st | 2); InvalidateRect(h, 0, FALSE); }
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }
        case WM_MOUSELEAVE: {
            SetWindowLongPtrW(h, GWLP_USERDATA, GetWindowLongPtrW(h, GWLP_USERDATA) & ~2);
            InvalidateRect(h, 0, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            SendMessageW(GetAncestor(h, GA_ROOT), WM_COMMAND,
                         MAKEWPARAM((UINT)GetWindowLongPtrW(h, GWLP_ID), BN_CLICKED), (LPARAM)h);
            return 0;
        }
        case WM_SETCURSOR: { SetCursor(g_cursor); return TRUE; }
        case WM_GETDLGCODE: return DLGC_BUTTON | DLGC_WANTARROWS;
    }
    return DefWindowProcW(h, m, w, l);
}

static void reg_navbtn(HINSTANCE hi) {
    static bool done = false;
    if (done) return;
    done = true;
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = NavBtnProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SLNavBtn";
    wc.hbrBackground = nullptr;
    RegisterClassW(&wc);
}

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    LauncherApp* app = (LauncherApp*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
        case WM_CREATE: return 0;
        case WM_SIZE: if (app) app->layout(); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            // фон окна: единый плоский тон
            HBRUSH br = CreateSolidBrush(RGB(0x10, 0x10, 0x14));
            FillRect(dc, &rc, br);
            DeleteObject(br);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(w);
            if (id >= 1000 && id < 1000 + (int)app->nav.size()) {
                app->show_page(id - 1000);
                return 0;
            }
            // команды страниц
            if (app) pages_on_command(app, LOWORD(w), (HWND)l);
            return 0;
        }
        case WM_SL_STATUS: {
            std::string* s = (std::string*)l;
            pages_on_status(app, *s);
            delete s;
            return 0;
        }
        case WM_SL_VERSIONS: {
            pages_on_versions(app, &g_versions);
            return 0;
        }
        case WM_SL_EVENT: {
            pages_on_event(app, (PageEvent*)l);
            return 0;
        }
        case WM_SL_PROGRESS: {
            pages_on_progress(app, (long long)w, (long long)l);
            return 0;
        }
        case WM_SL_DONE: {
            app->working = false;
            pages_on_done(app, (int)w);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// Тёмный тайтлбар (Win10 1809+/Win11). Динамическая загрузка, чтобы не ломать XP-сборку.
static void enable_dark_titlebar(HWND h) {
    HMODULE m = LoadLibraryW(L"dwmapi.dll");
    if (!m) return;
    typedef HRESULT(WINAPI* Fn)(HWND, DWORD, LPCVOID, DWORD);
    Fn fn = (Fn)GetProcAddress(m, "DwmSetWindowAttribute");
    if (fn) {
        BOOL dark = TRUE;
        fn(h, 19, &dark, sizeof(dark)); // DWMWA_USE_IMMERSIVE_DARK_MODE (Win10 1903+)
        fn(h, 20, &dark, sizeof(dark)); // Win11
    }
    FreeLibrary(m);
}

bool LauncherApp::init(HINSTANCE hi, int cmdshow) {
    hinst = hi;
    theme_init();
    reg_navbtn(hi);
    reg_btn(hi);
    INITCOMMONCONTROLSEX icc = { sizeof(INITCOMMONCONTROLSEX), ICC_PROGRESS_CLASS };
    InitCommonControlsEx(&icc);

    const wchar_t cls[] = L"SuperLauncherNativeMain";
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MainProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = cls;
    RegisterClassW(&wc);

    WNDCLASSW wside = {0};
    wside.lpfnWndProc = SideProc;
    wside.hInstance = hi;
    wside.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wside.lpszClassName = L"SLSide";
    wside.hbrBackground = nullptr;
    RegisterClassW(&wside);

    hwnd = CreateWindowExW(0, cls, L"SuperLauncher 2.0",
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           1280, 800, nullptr, nullptr, hi, nullptr);
if (!hwnd) return false;
    enable_dark_titlebar(hwnd);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)this);
    s_app = this;

    sidebar = CreateWindowExW(0, L"SLSide", nullptr, WS_CHILD | WS_VISIBLE,
                              12, 12, 210, 760, hwnd, nullptr, hi, nullptr);

static const char* labels[] = { "Главная", "Аккаунт", "Моды", "Сборки", "Скины",
                                    "Новости", "Обновления", "Серверы", "Настройки",
                                    "Minecraft", "AI Агент" };
    for (int i = 0; i < 11; i++) {
        HWND b = CreateWindowExW(0, L"SLNavBtn", sl::s2ws(labels[i]).c_str(),
                                 WS_CHILD | WS_VISIBLE, 10, 24 + i * 44, 190, 42,
                                 sidebar, (HMENU)(INT_PTR)(1000 + i), hi, nullptr);
        nav.push_back(b);
    }

    pages.push_back(create_home_page(hi, hwnd, this));
    pages.push_back(create_account_page(hi, hwnd, this));
    pages.push_back(create_mods_page(hi, hwnd, this));
    pages.push_back(create_builds_page(hi, hwnd, this));
    pages.push_back(create_skins_page(hi, hwnd, this));
    pages.push_back(create_news_page(hi, hwnd, this));
    pages.push_back(create_updates_page(hi, hwnd, this));
    pages.push_back(create_servers_page(hi, hwnd, this));
    pages.push_back(create_settings_page(hi, hwnd, this));
    pages.push_back(create_launch_page(hi, hwnd, this));
    pages.push_back(create_ai_page(hi, hwnd, this));

    ShowWindow(hwnd, cmdshow ? cmdshow : SW_SHOW);
    UpdateWindow(hwnd);
    show_page(0);

    // Фоново загружаем список версий
    CreateThread(nullptr, 0, ManifestWorker, hwnd, 0, nullptr);
    {
        // сразу подставляем уже установленные версии
        sl::Config cfg;
        cfg.load();
    }
    return true;
}

void LauncherApp::run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

int run_app(HINSTANCE hi, int cmdshow) {
    InitCommonControls();
    LauncherApp app;
    if (!app.init(hi, cmdshow)) return 1;
    app.run();
    return 0;
}

} // namespace slui

