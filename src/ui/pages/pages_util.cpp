#include "pages_priv.h"
#include <commctrl.h>
#include <windowsx.h>
#include <functional>

namespace slui {

static DWORD WINAPI sl_thread_thunk(LPVOID param) {
    std::function<void()>* fn = (std::function<void()>*)param;
    (*fn)();
    delete fn;
    return 0;
}

void sl_fill_combo(HWND combo, const std::vector<std::string>& items, int select) {
    ComboBox_ResetContent(combo);
    for (auto& it : items)
        ComboBox_AddString(combo, sl::s2ws(it).c_str());
    if (select < 0 || select >= (int)items.size()) select = 0;
    ComboBox_SetCurSel(combo, select);
}

std::string sl_combo_sel(HWND combo) {
    int sel = (int)ComboBox_GetCurSel(combo);
    if (sel < 0) return std::string();
    int len = (int)ComboBox_GetLBTextLen(combo, sel);
    std::wstring w((size_t)len, 0);
    ComboBox_GetLBText(combo, sel, &w[0]);
    return sl::w2a(w);
}

void sl_memo_append(HWND memo, const std::string& s) {
    if (!memo) return;
    int len = (int)SendMessageW(memo, WM_GETTEXTLENGTH, 0, 0);
    if (len > 40000) SetCtrl(memo, "");
    std::string cur = GetCtrl(memo);
    if (!cur.empty()) cur += "\r\n";
    cur += s;
    SetCtrl(memo, cur);
    SendMessageW(memo, EM_SETSEL, (WPARAM)-1, -1);
}

void sl_fill_list(HWND list, const std::vector<std::string>& items) {
    ListBox_ResetContent(list);
    for (auto& it : items)
        ListBox_AddString(list, sl::s2ws(it).c_str());
}

void sl_msg(HWND parent, const std::string& title, const std::string& msg, UINT flags) {
    MessageBoxW(parent, sl::s2ws(msg).c_str(), sl::s2ws(title).c_str(),
                MB_OK | flags);
}

bool sl_confirm(HWND parent, const std::string& title, const std::string& msg) {
    return MessageBoxW(parent, sl::s2ws(msg).c_str(), sl::s2ws(title).c_str(),
                       MB_YESNO | MB_ICONQUESTION) == IDYES;
}

void sl_thread(std::function<void()> fn) {
    std::function<void()>* heap = new std::function<void()>(std::move(fn));
    HANDLE h = CreateThread(nullptr, 0, sl_thread_thunk, heap, 0, nullptr);
    if (h) CloseHandle(h);
}

void sl_ai_set_models(HWND combo, std::vector<std::string>* list) {
    if (combo && list) {
        sl_fill_combo(combo, *list);
    }
    delete list;
}

} // namespace slui