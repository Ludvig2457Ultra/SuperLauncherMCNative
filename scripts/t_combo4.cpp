#include <windows.h>
#include <commctrl.h>
#include <fstream>

static std::ofstream out;
static HWND g_combo;
static WNDPROC g_oldListProc = nullptr;
static int g_wheelCount = 0;

static LRESULT CALLBACK ListSubclass(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_MOUSEWHEEL) {
        g_wheelCount++;
        int top = (int)SendMessageW(h, LB_GETTOPINDEX, 0, 0);
        out << "ListSubclass got WM_MOUSEWHEEL, topindex before=" << top
            << " scroll=" << (short)HIWORD(w) << "\n";
        return 0;
    }
    return CallWindowProcW(g_oldListProc, h, m, w, l);
}

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(h, m, w, l);
}

int main() {
    out.open("build_t/combo_dbg4.txt");
    InitCommonControls();
    HINSTANCE hi = GetModuleHandleW(nullptr);
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = MainProc; wc.hInstance = hi; wc.lpszClassName = L"MW"; RegisterClassW(&wc);
    HWND main = CreateWindowExW(0, L"MW", L"", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, 0, 0, hi, 0);
    ShowWindow(main, SW_SHOW);
    SetForegroundWindow(main);
    g_combo = CreateWindowExW(0, L"COMBOBOX", 0, WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, 10, 10, 260, 200, main, (HMENU)1, hi, 0);
    for (int i = 0; i < 908; i++) {
        wchar_t buf[32]; swprintf(buf, 32, L"version-%d", i);
        SendMessageW(g_combo, CB_ADDSTRING, 0, (LPARAM)buf);
    }
    SetFocus(g_combo);
    SendMessageW(g_combo, CB_SHOWDROPDOWN, TRUE, 0);
    {
        MSG pm; DWORD t = GetTickCount();
        while (GetTickCount() - t < 400) {
            while (PeekMessageW(&pm, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&pm); DispatchMessageW(&pm); }
            Sleep(10);
        }
    }
    COMBOBOXINFO cbi = { sizeof(cbi) };
    SendMessageW(g_combo, CB_GETCOMBOBOXINFO, 0, (LPARAM)&cbi);
    HWND list = cbi.hwndList;
    out << "list=" << (list ? "yes" : "no") << "\n";
    if (list) {
        LONG_PTR style = GetWindowLongPtrW(list, GWL_STYLE);
        out << "WS_VSCROLL=" << ((style & WS_VSCROLL) != 0) << " LBS_NOSCROLL=" << ((style & 0x1000) != 0) << "\n";
        out << "count=" << (int)SendMessageW(list, LB_GETCOUNT, 0, 0)
            << " itemheight=" << (int)SendMessageW(list, LB_GETITEMHEIGHT, 0, 0) << "\n";
        // подкласс для лога колеса
        g_oldListProc = (WNDPROC)SetWindowLongPtrW(list, GWLP_WNDPROC, (LONG_PTR)ListSubclass);
        RECT r; GetWindowRect(list, &r);
        POINT center = { (r.left + r.right) / 2, (r.top + r.bottom) / 2 };
        // реальное колесо
        SetCursorPos(center.x, center.y);
        mouse_event(MOUSEEVENTF_WHEEL, 0, 0, (DWORD)-WHEEL_DELTA * 3, 0);
        Sleep(100);
        out << "wheelCount=" << g_wheelCount << " topindex="
            << (int)SendMessageW(list, LB_GETTOPINDEX, 0, 0) << "\n";
    }
    out.close();
    return 0;
}
