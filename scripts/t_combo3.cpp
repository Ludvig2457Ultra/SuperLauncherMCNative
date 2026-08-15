#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <fstream>

#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

int main() {
    std::ofstream out("build_t/combo_dbg3.txt");
    InitCommonControls();
    HWND w = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED, 0, 0, 600, 400,
                             nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(w, SW_SHOW);
    HWND cb = CreateWindowExW(0, L"COMBOBOX", 0, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                              10, 10, 260, 200, w, (HMENU)1, GetModuleHandleW(nullptr), 0);
    const wchar_t* items[] = { L"latest_release", L"snapshot", L"26.3-snapshot-8", L"1.20.1", L"26.2" };
    for (auto it : items) SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)it);

    int dropped = (int)SendMessageW(cb, CB_GETDROPPEDWIDTH, 0, 0);
    out << "CB_GETDROPPEDWIDTH (default)=" << dropped << "\n";

    // Открыть список и измерить фактическую ширину окна списка
    SendMessageW(cb, CB_SHOWDROPDOWN, TRUE, 0);
    UpdateWindow(cb);
    HWND list = (HWND)SendMessageW(cb, CB_GETCOMBOBOXINFO ? 0 : 0, 0, 0);
    COMBOBOXINFO cbi = { sizeof(cbi) };
    SendMessageW(cb, CB_GETCOMBOBOXINFO, 0, (LPARAM)&cbi);
    if (cbi.hwndList) {
        RECT r;
        GetWindowRect(cbi.hwndList, &r);
        out << "list window width=" << (r.right - r.left) << "\n";
    }
    SendMessageW(cb, CB_SHOWDROPDOWN, FALSE, 0);

    // Теперь задать ширину вручную
    SendMessageW(cb, CB_SETDROPPEDWIDTH, 300, 0);
    out << "after CB_SETDROPPEDWIDTH(300) -> " << (int)SendMessageW(cb, CB_GETDROPPEDWIDTH, 0, 0) << "\n";
    out.close();
    return 0;
}