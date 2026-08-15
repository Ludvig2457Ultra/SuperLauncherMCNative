#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <fstream>

#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

int main() {
    std::ofstream out("build_t/combo_dbg2.txt");
    InitCommonControls();
    HWND w = CreateWindowExW(0, L"STATIC", L"", WS_OVERLAPPED, 0, 0, 100, 100,
                             nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    HFONT f = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HWND cb = CreateWindowExW(0, L"COMBOBOX", 0, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                              0, 0, 260, 100, w, (HMENU)1, GetModuleHandleW(nullptr), 0);
    SendMessageW(cb, WM_SETFONT, (WPARAM)f, TRUE);

    const wchar_t* items[] = { L"latest_release", L"snapshot", L"26.3-snapshot-8", L"1.20.1", L"26.2" };
    for (auto it : items) {
        int r = (int)SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)it);
        out << "add [" << it << "] -> " << r << "\n";
    }
    int count = (int)SendMessageW(cb, CB_GETCOUNT, 0, 0);
    out << "count=" << count << "\n";
    for (int i = 0; i < count; i++) {
        int len = (int)SendMessageW(cb, CB_GETLBTEXTLEN, i, 0);
        std::wstring buf((size_t)len, 0);
        SendMessageW(cb, CB_GETLBTEXT, i, (LPARAM)&buf[0]);
        out << "item " << i << " len=" << len << " text=W[";
        for (wchar_t c : buf) out << (int)c << ",";
        out << "] lsblen_chars=";
        out << "[";
        for (wchar_t c : buf) out << (char)(c > 0 && c < 128 ? c : '?');
        out << "]\n";
    }
    out.close();
    return 0;
}